#include "flowscriptparser.h"
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <regex>

using std::invalid_argument;
using std::regex;

void FlowScriptParseJob::Execute()
{

    std::map<std::string, std::function<Job *()>> jobFactories = {
        {"compileJob", []() -> Job *
         { return new CustomJob(); }},
        {"compileParseJob", []() -> Job *
         { return new ParsingJob(); }},
        {"parseOutputJob", []() -> Job *
         { return new OutputJob(); }}};

    // Tokenize function create vector
    std::vector<Token> tokens;
    try
    {
        tokens = Tokenize(this->GetInput()["flowscript"].get<std::string>());
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Error in tokenizing FlowScript: " << e.what() << std::endl;
        // Handle tokenization errors or return/exit as needed
        return;
    }

    // Get map of instructions (AST)
    std::map<std::string, FlowScriptParseJob::GraphNode> graph;
    try
    {
        graph = parseGraph(tokens);

        nlohmann::json graphJson;

        // Print the graph for verification
        for (const auto &pair : graph)
        {

            const auto &id = pair.first;
            const auto &node = pair.second;

            nlohmann::json nodeJson;
            nodeJson["id"] = node.id;
            nodeJson["type"] = static_cast<int>(node.type);

            for (const auto &dep : node.dependencies)
            {
                nodeJson["dependencies"].push_back(dep);
            }
            // If node has input data
            if (!node.inputData.empty())
            {
                nodeJson["inputData"] = node.inputData;
            }

            graphJson[id] = nodeJson;
        }

        this->SetOutput(graphJson);
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Error in parsing FlowScript: " << e.what() << std::endl;
        return;
    }
}

void FlowScriptParseJob::JobCompleteCallback()
{
    // Output the JSON representation of the graph
    std::cout << "FlowScriptParser Job " << this->GetUniqueID() << " has been completed, the output is:" << std::endl;
    std::string jsonOuput = this->GetOutput().dump(4);
    std::cout << jsonOuput << std::endl;
}

std::string FlowScriptParseJob::processInputData(std::string inputData)
{
    std::string processedJson = inputData;
    // Remove the outermost additional curly braces
    if (processedJson.size() > 4 && processedJson.substr(0, 2) == "{{" && processedJson.substr(processedJson.size() - 2) == "}}")
    {
        processedJson = processedJson.substr(2, processedJson.size() - 4);
    }

    // Replace incorrect JSON format
    size_t pos = processedJson.find(",");
    if (pos != std::string::npos)
    {
        processedJson.replace(pos, 1, ":"); // Replace first comma with a colon
    }

    // Add double quotes around the key if missing
    if (processedJson.front() != '\"')
    {
        processedJson.insert(0, "\"");
        pos = processedJson.find(":"); // Find the position of the newly added colon
        if (pos != std::string::npos)
        {
            processedJson.insert(pos - 1, "\""); // Add a double quote before the colon
        }
    }

    // Enclose the entire string with curly braces
    return "{" + processedJson + "}";
}

std::vector<Token> FlowScriptParseJob::Tokenize(std::string script)
{

    static const std::regex tokenRegex(
        R"((digraph)|(\{)|(\})|(node)|(shape)|(label)|(data)|(=)|(->)|(\[)|(\])|(;)|([a-zA-Z][a-zA-Z0-9_]*)|(\"[^\"]*\")|('[^']*')|(\{\{.*?\}\}))",
        std::regex_constants::ECMAScript);

    std::vector<Token> tokens;
    std::smatch match;

    std::string remainingScript = script;

    while (std::regex_search(remainingScript, match, tokenRegex))
    {
        TokenType type = IDENTIFIER; // Default type

        if (match[1].matched)
            type = DIGRAPH;
        else if (match[2].matched)
            type = LBRACE;
        else if (match[3].matched)
            type = RBRACE;
        else if (match[4].matched)
            type = NODE;
        else if (match[5].matched)
            type = SHAPE;
        else if (match[6].matched)
            type = LABEL;
        else if (match[7].matched)
            type = DATA;
        else if (match[8].matched)
            type = EQUALS;
        else if (match[9].matched)
            type = ARROW;
        else if (match[10].matched)
            type = LBRACKET;
        else if (match[11].matched)
            type = RBRACKET;
        else if (match[12].matched)
            type = SEMICOLON;
        else if (match[13].matched)
            type = IDENTIFIER;
        else if (match[14].matched || match[15].matched)
            type = STRING_LITERAL;
        else if (match[16].matched)
            type = JSON;

        tokens.push_back({type, match.str()});
        remainingScript = match.suffix().str();
    }

    // After tokenization loop
    if (!remainingScript.empty())
    {
        throw std::invalid_argument("Unrecognized token in script: " + remainingScript);
    }

    return tokens;
}

nlohmann::json FlowScriptParseJob::ProcessJsonString(std::string inputStr)
{
    nlohmann::json jsonObj;

    // Find the positions of the key and value in the string
    size_t firstQuote = inputStr.find('\'');
    size_t secondQuote = inputStr.find('\'', firstQuote + 1);
    size_t thirdQuote = inputStr.find('\'', secondQuote + 1);
    size_t fourthQuote = inputStr.find('\'', thirdQuote + 1);

    if (firstQuote != std::string::npos && secondQuote != std::string::npos &&
        thirdQuote != std::string::npos && fourthQuote != std::string::npos)
    {
        // Extract the key and value
        std::string key = inputStr.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        std::string value = inputStr.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);

        // Remove any additional whitespace
        key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());

        // Assign to the JSON object
        jsonObj[key] = value;
    }

    return jsonObj;
}

std::map<std::string, FlowScriptParseJob::GraphNode> FlowScriptParseJob::parseGraph(std::vector<Token> &tokens)
{
    std::map<std::string, FlowScriptParseJob::GraphNode> graph;
    int tokenIndex = 0;
    GraphNode::Type currentType = GraphNode::Type::Undefined;

    if (tokens.empty())
    {
        throw std::invalid_argument("Invalid graph: No tokens provided");
    }

    if (tokens[tokenIndex].type != DIGRAPH)
    {
        throw std::invalid_argument("Invalid graph: Missing 'digraph' keyword");
    }
    tokenIndex++;

    if (tokens[tokenIndex].type != LBRACE)
    {
        throw std::invalid_argument("Invalid graph: Missing opening brace");
    }
    tokenIndex++;

    while (tokenIndex < tokens.size())
    {
        if (tokens[tokenIndex].type == LBRACE)
        {
            tokenIndex++;                             // Skip the opening brace
            currentType = GraphNode::Type::Undefined; // Reset current type for new block

            while (tokenIndex < tokens.size() && tokens[tokenIndex].type != RBRACE)
            {
                if (tokens[tokenIndex].type == NODE)
                {
                    // Update currentType based on shape defined in this block
                    currentType = determineNodeType(tokens, tokenIndex);
                }
                else if (tokens[tokenIndex].type == IDENTIFIER)
                {
                    auto node = parseGraphNode(tokens, tokenIndex, currentType);
                    if (!node.id.empty())
                    {
                        graph[node.id] = std::move(node);
                    }
                }
                else
                {
                    // Skip unrecognized tokens or handle them as needed
                    tokenIndex++;
                }
            }

            if (tokenIndex >= tokens.size() || tokens[tokenIndex].type != RBRACE)
            {
                throw std::invalid_argument("Invalid graph: Missing closing brace in nested block");
            }
            tokenIndex++; // Skip the closing brace
        }
        else if (tokens[tokenIndex].type == IDENTIFIER)
        {
            // Handle potential node declarations or dependencies
            std::string nodeId = tokens[tokenIndex].lexeme;
            tokenIndex++;

            if (tokenIndex < tokens.size() && tokens[tokenIndex].type == ARROW)
            {
                // If the next token is an arrow, parse this as a dependency
                tokenIndex++; // Skip the arrow
                parseDependencies(tokens, tokenIndex, graph[nodeId]);
            }
            // Additional else-if clauses can be added here for other types of statements
            else if (tokens[tokenIndex].type != ARROW)
            {
                throw std::invalid_argument("Invalid dependency operator at index: " + std::to_string(tokenIndex));
            }
        }
        else if (tokens[tokenIndex].type == RBRACE)
        {
            // Closing brace of the entire graph
            tokenIndex++;
            break;
        }
        else
        {
            // Skip unrecognized tokens or handle them as needed
            tokenIndex++;
        }
    }

    for (const auto &nodepair : graph)
    {
        for (const auto &dep : nodepair.second.dependencies)
        {
            if (graph.find(dep) == graph.end())
            {
                throw std::invalid_argument("Undefined node in dependencies: " + dep);
            }
        }
    }

    return graph;
}

FlowScriptParseJob::GraphNode FlowScriptParseJob::parseGraphNode(std::vector<Token> &tokens, int &tokenIndex, GraphNode::Type currentType)
{
    GraphNode node;
    node.type = currentType;

    // Ensure the token is an identifier before assigning it as a node ID
    if (tokens[tokenIndex].type == IDENTIFIER)
    {
        node.id = tokens[tokenIndex].lexeme;
        tokenIndex++; // Move past the node identifier.
    }
    else
    {
        throw std::invalid_argument("Expected node identifier, found: " + tokens[tokenIndex].lexeme);
    }

    // Check for properties or dependencies.
    while (tokenIndex < tokens.size() && tokens[tokenIndex].type != SEMICOLON)
    {
        if (tokens[tokenIndex].type == LBRACKET)
        {
            tokenIndex++; // Enter properties
            parseNodeProperties(tokens, tokenIndex, node);
            if (tokens[tokenIndex].type == RBRACKET)
            {
                tokenIndex++; // Exit properties
            }
        }
        else
        {
            tokenIndex++; // Skip unrecognized tokens
        }
    }

    if (tokens[tokenIndex].type == SEMICOLON)
    {
        tokenIndex++; // Skip semicolon at the end of node definition
    }

    return node;
}

void FlowScriptParseJob::parseNodeProperties(std::vector<Token> &tokens, int &tokenIndex, GraphNode &node)
{
    while (tokenIndex < tokens.size() && tokens[tokenIndex].type != RBRACKET)
    {
        switch (tokens[tokenIndex].type)
        {
        case SHAPE:
            tokenIndex++; // Move past 'shape'
            if (tokenIndex < tokens.size() && tokens[tokenIndex].type == EQUALS)
            {
                tokenIndex++; // Move past '='
                if (tokenIndex < tokens.size() && tokens[tokenIndex].type == STRING_LITERAL)
                {
                    std::string shapeValue = tokens[tokenIndex].lexeme;
                    // Assign shape type based on shape value
                    if (shapeValue == "\"circle\"")
                    {
                        node.type = GraphNode::Type::Data;
                    }
                    else if (shapeValue == "\"box\"")
                    {
                        node.type = GraphNode::Type::Job;
                    }
                    else if (shapeValue == "\"diamond\"")
                    {
                        node.type = GraphNode::Type::Status;
                    }
                    tokenIndex++; // Move past string literal
                }
            }
            break;

        case DATA:
            tokenIndex++; // Move past 'data'
            if (tokenIndex < tokens.size() && tokens[tokenIndex].type == EQUALS)
            {
                tokenIndex++; // Move past '='
                if (tokenIndex < tokens.size() && tokens[tokenIndex].type == STRING_LITERAL)
                {
                    node.inputData = ProcessJsonString(tokens[tokenIndex].lexeme);
                    if (node.type == GraphNode::Type::Data && !node.inputData.is_object())
                    {
                        throw std::invalid_argument("Invalid data format for node: " + node.id);
                    }
                    tokenIndex++; // Move past string literal
                }
            }
            break;

        case LABEL:
            tokenIndex++; // Move past 'label'
            if (tokenIndex < tokens.size() && tokens[tokenIndex].type == EQUALS)
            {
                tokenIndex++; // Move past '='
                if (tokenIndex < tokens.size() && tokens[tokenIndex].type == STRING_LITERAL)
                {
                    node.label = tokens[tokenIndex].lexeme;
                    tokenIndex++; // Move past string literal
                }
            }
            break;

        default:
            tokenIndex++; // Skip unrecognized property tokens
            break;
        }
    }

    if (tokenIndex < tokens.size() && tokens[tokenIndex].type == RBRACKET)
    {
        tokenIndex++; // Move past the right bracket at the end of properties
    }
}

void FlowScriptParseJob::parseDependencies(std::vector<Token> &tokens, int &tokenIndex, GraphNode &node)
{
    while (tokenIndex < tokens.size() && tokens[tokenIndex].type != SEMICOLON)
    {
        if (tokens[tokenIndex].type == IDENTIFIER)
        {

            node.dependencies.push_back(tokens[tokenIndex].lexeme);
            tokenIndex++;
        }
        else if (tokens[tokenIndex].type == LBRACKET)
        {
            // Handle properties of the dependency (like labels)
            tokenIndex++; // Enter label properties
            while (tokenIndex < tokens.size() && tokens[tokenIndex].type != RBRACKET)
            {
                if (tokens[tokenIndex].type == LABEL)
                {
                    tokenIndex++; // Move past 'label'
                    if (tokenIndex < tokens.size() && tokens[tokenIndex].type == EQUALS)
                    {
                        tokenIndex++; // Move past '='
                        if (tokenIndex < tokens.size() && tokens[tokenIndex].type == STRING_LITERAL)
                        {
                            node.label = tokens[tokenIndex].lexeme;
                            tokenIndex++; // Move past string literal
                        }
                    }
                }
                else
                {
                    tokenIndex++; // Skip other properties
                }
            }
            if (tokenIndex < tokens.size() && tokens[tokenIndex].type == RBRACKET)
            {
                tokenIndex++; // Exit label properties
            }
        }
        else
        {
            tokenIndex++; // Skip other tokens
        }
    }
}

FlowScriptParseJob::GraphNode::Type FlowScriptParseJob::determineNodeType(const std::vector<Token> &tokens, int &tokenIndex)
{
    // Skip the 'node' token and proceed to the '['
    tokenIndex += 2; // Assuming 'node' followed by '['

    while (tokens[tokenIndex].type != RBRACKET && tokenIndex < tokens.size())
    {
        if (tokens[tokenIndex].type == SHAPE)
        {
            // Increment to get the value of the shape
            tokenIndex += 2; // Skip '='

            if (tokens[tokenIndex].type == STRING_LITERAL)
            {
                std::string shapeValue = tokens[tokenIndex].lexeme;
                tokenIndex++; // Move past the shape value

                if (shapeValue == "\"circle\"")
                {
                    return GraphNode::Type::Data;
                }
                else if (shapeValue == "\"box\"")
                {
                    return GraphNode::Type::Job;
                }
                else if (shapeValue == "\"diamond\"")
                {
                    return GraphNode::Type::Status;
                }
            }
        }
        tokenIndex += 2;
    }

    // If no shape is found or the shape does not match any known types
    return GraphNode::Type::Undefined;
}
