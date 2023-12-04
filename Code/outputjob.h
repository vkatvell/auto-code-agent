#pragma once
#include "./lib/job.h"
#include <vector>
#include <nlohmann/json.hpp>

class OutputJob : public Job
{
public:
    OutputJob() = default;
    ~OutputJob(){};

    void Execute() override;
    void JobCompleteCallback() override;

    nlohmann::ordered_json errorJson;

    struct ErrorInfo
    {
        std::string description;
        std::string filepath;
        int lineNumber;
        int columnNumber;
    };

private:
    nlohmann::json m_parseJobOutput;
    mutable std::mutex m_errorInfoVectorMutex;
    mutable std::mutex m_errorJsonMutex;
    mutable std::mutex m_jsonFileMutex;
};