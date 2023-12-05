#pragma once
#include <mutex>
#include <map>
#include <deque>
#include <vector>
#include <thread>
#include <nlohmann/json.hpp>

class JobSystem;
class Job
{
    friend class JobSystem;
    friend class JobWorkerThread;

public:
    Job(unsigned long jobChannels = 0xFFFFFFF, int jobType = -1) : m_jobChannels(jobChannels), m_jobType(jobType)
    {
        static int s_nextJobID = 0;
        m_jobID = s_nextJobID++;
    }

    virtual ~Job() {}

    // Must inherit Execute() function because it has no body
    virtual void Execute() = 0;
    // virtual nlohmann::json Execute(const nlohmann::json &input) = 0;

    const std::string &GetJobName() const
    {
        return m_jobName;
    }

    // Set job Name
    void SetJobName(const std::string &jobName)
    {
        std::lock_guard<std::mutex> lockName(m_jobNameMutex);
        m_jobName = jobName;
    }

    // Set input for the job
    void SetInput(const nlohmann::json &input)
    {
        std::lock_guard<std::mutex> lockInput(m_inputMutex);
        m_input = input;
    }

    // Get input for a job
    const nlohmann::json &GetInput() const
    {
        std::lock_guard<std::mutex> lockInput(m_inputMutex);
        return m_input;
    }

    // Set output for the job
    void SetOutput(const nlohmann::json &output)
    {
        std::lock_guard<std::mutex> lockOutput(m_outputMutex);
        m_output = output;
    }

    // Get output for a job
    const nlohmann::json &GetOutput() const
    {
        std::lock_guard<std::mutex> lockOutput(m_outputMutex);
        return m_output;
    }

    // Do not have to implement JobCompleteCallback() because it has a body
    virtual void JobCompleteCallback(){};
    // Forcing the function, job type will be returned as a const
    int GetUniqueID() const { return m_jobID; }
    // Queuing dependent jobs, if a job has one
    virtual void EnqueueNextJob(JobSystem *js){};

private:
    int m_jobID = -1;
    int m_jobType = -1;

    std::string m_jobName;
    mutable std::mutex m_jobNameMutex;

    nlohmann::json m_input;
    mutable std::mutex m_inputMutex;
    nlohmann::json m_output;
    mutable std::mutex m_outputMutex;

    unsigned long m_jobChannels = 0xFFFFFFFF;
};