#pragma once
#include "./lib/job.h"
#include <iostream>
#include <string>
#include <array>

class CustomJob : public Job
{
public:
    CustomJob() = default;
    CustomJob(nlohmann::json input);
    ~CustomJob(){};

    void Execute() override;

    void JobCompleteCallback() override;

    std::string output;
    int returnCode;

private:
    nlohmann::json m_compileJobInput;
};