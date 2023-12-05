#pragma once
#include "./lib/job.h"
#include "flowscript_common.h"
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <regex>
#include "customjob.h"
#include "parsingjob.h"
#include "outputjob.h"

class FlowScriptParseJob : public Job
{

public:
    FlowScriptParseJob() = default;
    ~FlowScriptParseJob(){};

    void Execute() override;

    void JobCompleteCallback() override;

    std::string processInputData(std::string inputData);

    std::string output;
    int returnCode;

    struct GraphNode
    {

        enum class Type
        {
            Data,
            Job,
            Status,
            Undefined
        };
        std::string id;
        Type type = Type::Undefined;
        std::string label;
        std::vector<std::string> dependencies;
        nlohmann::json inputData;
        std::string statusCondition;
        std::string output;
    };

    std::vector<Token> Tokenize(std::string script);

    std::map<std::string, GraphNode> parseGraph(std::vector<Token> &tokens);

    GraphNode parseGraphNode(std::vector<Token> &tokens, int &tokenIndex, GraphNode::Type currentType);

private:
    void parseNodeProperties(std::vector<Token> &tokens, int &tokenIndex, GraphNode &node);
    void parseDependencies(std::vector<Token> &tokens, int &tokenIndex, GraphNode &node);
    nlohmann::json ProcessJsonString(std::string jsonStr);

    GraphNode::Type determineNodeType(const std::vector<Token> &tokens, int &tokenIndex);
    nlohmann::json m_inputScript;
    nlohmann::json m_outputAST;
};