#include "parsingjob.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <array>
#include <regex>

void ParsingJob::Execute()
{
    // Locking the parsedErrors vector mutex
    std::lock_guard<std::mutex> lockParsedErrors(m_parsedErrorsMutex);

    // Parse errorOutput and fill errorInfo fields
    std::regex linker_text_error("ld: Undefined symbols:(.*?)(?=clang:|$)");
    std::regex linker_error("clang: error: (.*)");
    std::regex compiler_error("(.*):(\\d+):(\\d+): (?:error|warning): (.*)");

    std::istringstream err(this->GetInput()["output"].get<std::string>());

    std::string line;
    std::string linker_snippet;

    // Flag to determine if linker error snippet is complete
    bool in_linker_error = false;

    while (std::getline(err, line))
    {
        std::smatch match;

        if (std::regex_match(line, match, linker_text_error))
        {
            in_linker_error = true;
        }
        else if (std::regex_match(line, match, linker_error))
        {
            in_linker_error = false;
            linker_snippet.append(match[1]);
        }
        else if (std::regex_match(line, match, compiler_error))
        {
            ErrorInfo errorInfo = {
                .filepath = match[1],
                .lineNumber = std::stoi(match[2]),
                .columnNumber = std::stoi(match[3]),
                .description = match[4],
            };
            m_parsedErrors.push_back(errorInfo);
        }

        if (in_linker_error)
        {
            linker_snippet.append(line + '\n');
        }
    }
    // Pushing linker error to parsedErrors vector
    if (!linker_snippet.empty())
    {
        std::string filePath = "Linker Error";
        ErrorInfo errorInfo = {
            .filepath = filePath,
            .lineNumber = 0,
            .columnNumber = 0,
            .description = linker_snippet,
        };
        m_parsedErrors.push_back(errorInfo);
    }

    nlohmann::json jsonOutput = nlohmann::json::array();

    for (const ErrorInfo &errorInfo : m_parsedErrors)
    {
        nlohmann::json errorJson = {
            {"filepath", errorInfo.filepath},
            {"lineNumber", errorInfo.lineNumber},
            {"columnNumber", errorInfo.columnNumber},
            {"description", errorInfo.description}};

        jsonOutput.push_back(errorJson);
    }

    this->SetOutput(jsonOutput);
}

void ParsingJob::JobCompleteCallback()
{
    std::cout << "Parsing Job " << this->GetUniqueID() << " has been completed, the output is:" << std::endl;
    std::string jsonOuput = this->GetOutput().dump(4);
    std::cout << jsonOuput << std::endl;
}