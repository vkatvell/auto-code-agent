#pragma once
#include <mutex>
#include <map>
#include <deque>
#include <vector>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>

constexpr int JOB_TYPE_ANY = -1;

class JobWorkerThread;

enum JobStatus
{
    JOB_STATUS_NEVER_SEEN,
    JOB_STATUS_QUEUED,
    JOB_STATUS_RUNNING,
    JOB_STATUS_COMPLETED,
    JOB_STATUS_RETIRED,
    NUM_JOB_STATUSES
};

struct JobHistoryEntry
{
    JobHistoryEntry(int jobType, JobStatus JobStatus) : m_jobType(jobType), m_jobStatus(JobStatus) {}

    int m_jobType = -1;
    int m_jobStatus = JOB_STATUS_NEVER_SEEN;
};

class Job;

class JobSystem
{
    friend class JobWorkerThread;

public:
    JobSystem();
    ~JobSystem();

    static JobSystem *CreateOrGet();
    static void Destroy();

    void CreateWorkerThread(const char *uniqueName, unsigned long workerJobChannels = 0xFFFFFFFF);
    void DestroyWorkerThread(const char *uniqueName);
    void QueueJob(int jobID);

    // Registering custom job
    void RegisterJobType(const std::string &jobType, std::function<Job *()> jobFactory);

    nlohmann::json CreateJob(const std::string &jobType, nlohmann::json &input);
    nlohmann::json GetAJobStatus(const std::string &jobType);

    std::vector<std::string> GetAvailableJobTypes();

    // Job dependency functions
    void SetDependency(const std::string &dependentJobName, const std::string &dependencyJobName);

    // Status Queries
    JobStatus GetJobStatus(int jobID) const;
    bool IsJobComplete(int jobID) const;

    void FinishCompletedJobs();
    nlohmann::json FinishJob(int jobID);

private:
    Job *ClaimAJob(unsigned long workerJobChannels);
    void OnJobCompleted(Job *jobJustExecuted);
    bool AreDependenciesResolved(int jobID);

    static JobSystem *s_jobSystem;

    std::map<std::string, std::function<Job *()>> m_jobFactories;
    mutable std::mutex m_jobFactoriesMutex;
    std::map<int, std::vector<int>> m_jobDependencies;
    mutable std::mutex m_jobDependenciesMutex;

    // Mapping job namse to their unique IDs
    std::unordered_map<std::string, int> m_jobNameToID;
    mutable std::mutex m_jobNameToIDMutex;
    std::unordered_map<int, Job *> m_jobs;
    mutable std::mutex m_jobsMutex;

    std::vector<JobWorkerThread *> m_workerThreads;
    mutable std::mutex m_workerThreadMutex;
    std::deque<Job *> m_jobsQueued;
    std::deque<Job *> m_jobsRunning;
    std::deque<Job *> m_jobsCompleted;
    mutable std::mutex m_jobsQueuedMutex;
    mutable std::mutex m_jobsRunningMutex;
    mutable std::mutex m_jobsCompletedMutex;

    std::vector<JobHistoryEntry> m_jobHistory;
    mutable int m_jobHistoryLowestActiveIndex = 0;
    mutable std::mutex m_jobHistoryMutex;

    std::vector<std::string> m_availableJobTypes;
    mutable std::mutex m_availableJobTypeMutex;
};