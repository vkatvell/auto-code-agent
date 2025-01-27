#pragma once
#include "./lib/job.h"
#include <iostream>
#include <string>
#include <array>
#include <vector>

class ParsingJob : public Job
{
public:
    ParsingJob() = default;
    struct ErrorInfo
    {
        std::string description;
        std::string filepath;
        int lineNumber;
        int columnNumber;
    };

    void to_json(nlohmann::json &j, const ErrorInfo &e)
    {
        j = {{"description", e.description}, {"filepath", e.filepath}, {"lineNumber", e.lineNumber}, {"columnNumber", e.columnNumber}};
    }

    ~ParsingJob(){};

    void Execute() override;

    void JobCompleteCallback() override;

private:
    nlohmann::json m_compileJobOutput;
    std::vector<ErrorInfo> m_parsedErrors;
    mutable std::mutex m_parsedErrorsMutex;
};