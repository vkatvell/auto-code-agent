#pragma once
#include <iostream>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include "job.h"
#include "jobsystem.h"
#include <nlohmann/json.hpp>

class JobSystem;

class JobSystemAPI
{
public:
    JobSystemAPI();
    ~JobSystemAPI();

    void Destroy();
    void Start();
    void Stop();

    nlohmann::json JobStatus(std::string &);
    nlohmann::json FinishJob(std::string &);
    nlohmann::json CreateJob(const char *, nlohmann::json &);
    nlohmann::json GetJobTypes();

    void QueueJob(int jobId);

    void RegisterJob(const char *, std::function<Job *()>);

    void SetDependency(const char *dependentJobName, const char *dependencyJobName);

private:
    JobSystem *m_jobSystem;
    bool isDestroyed = false;
};