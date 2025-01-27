#pragma once
#include "./lib/job.h"
#include <iostream>
#include <string>
#include <array>

class CompileJob : public Job
{
public:
    CompileJob() = default;
    CompileJob(nlohmann::json input);
    ~CompileJob(){};

    void Execute() override;

    void JobCompleteCallback() override;

    std::string output;
    int returnCode;

private:
    nlohmann::json m_compileJobInput;
};