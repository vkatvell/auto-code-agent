#include "outputjob.h"
#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;

void OutputJob::Execute()
{
    // locking errorInfoVector to prevent multiple threads from accessing it at same time
    std::lock_guard<std::mutex> lockVector(m_errorInfoVectorMutex);

    // locking the jsonFile to prevent threads from writing to the file at the same time
    std::lock_guard<std::mutex> lockJson(m_jsonFileMutex);
    // Opening the file in append mode to append to existing content
    std::ofstream jsonFile("./Data/error_report.json", std::ios::out | std::ios::app);

    // Checking if file was successfully opened
    if (!jsonFile.is_open())
    {
        std::cout << "ERROR: Failed to open the JSON file for appending" << std::endl;
        return;
    }

    nlohmann::json inputJson = this->GetInput();

    // converting parsed error information to JSON format and populating errorJson
    for (const auto &errorInfo : inputJson)
    {
        // Extract error info from the JSON object
        int lineNumber = errorInfo["lineNumber"];
        int columnNumber = errorInfo["columnNumber"];
        std::string description = errorInfo["description"];
        std::string filepath = errorInfo["filepath"];

        ordered_json errorEntry;
        errorEntry["lineNumber"] = lineNumber;
        errorEntry["columnNumber"] = columnNumber;
        errorEntry["errorDescription"] = description;

        // locking errorJson to prevent threads from writing to it at same time
        std::lock_guard<std::mutex> lockError(m_errorJsonMutex);

        // Adding code snippets before and after error line
        // std::string codeSnippet;
        ordered_json codeSnippetArray;
        int errorLineNumber = lineNumber;
        const int linesBeforeError = 2;
        const int linesAfterError = 2;

        // Opening source file
        if (filepath != "Linker Error")
        {
            std::ifstream sourceFile(filepath);
            if (sourceFile.is_open())
            {
                int currentLineNumber = 0;
                std::string line;

                // Reading lines from the source file
                while (std::getline(sourceFile, line))
                {
                    currentLineNumber++;

                    // Edge cases
                    if ((currentLineNumber > errorLineNumber + linesAfterError) || errorLineNumber == 0)
                    {
                        break;
                    }

                    // Checking current line is in desired range around error line
                    if (currentLineNumber >= (errorLineNumber - linesBeforeError) &&
                        currentLineNumber <= (errorLineNumber + linesAfterError))
                    {
                        codeSnippetArray.push_back(line);
                    }
                }

                sourceFile.close();
            }
            // Adding the code snippet to the JSON entry
            errorEntry["codeSnippet"] = codeSnippetArray;

            errorJson[filepath].push_back(errorEntry);
        }
        else
        {
            errorJson[filepath].push_back(errorEntry);
        }
    }

    this->SetOutput(errorJson);

    // writing the JSON to a file
    jsonFile << std::setw(4) << errorJson << std::endl;

    // Closing the JSON file
    jsonFile.close();
}

void OutputJob::JobCompleteCallback()
{
    // Dumping JSON to console to verify JSON output
    std::cout << "JSON Output Job " << this->GetUniqueID() << " completed, the output is:" << std::endl;
    std::cout << this->GetOutput().dump(4) << std::endl;
}