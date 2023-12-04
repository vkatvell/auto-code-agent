#include "jobsystemapi.h"

JobSystemAPI::JobSystemAPI()
{
    m_jobSystem = JobSystem::CreateOrGet();

    for (unsigned int n = 1; n < std::thread::hardware_concurrency(); ++n)
    {
        std::string threadName = "Thread " + std::to_string(n);
        m_jobSystem->CreateWorkerThread(threadName.c_str(), 0xFFFFFFFF);
    }
}

JobSystemAPI::~JobSystemAPI()
{
    Destroy();
}

void JobSystemAPI::Start()
{
    if (m_jobSystem == nullptr)
    {
        m_jobSystem = JobSystem::CreateOrGet();
    }
}

void JobSystemAPI::Stop()
{
    if (m_jobSystem != nullptr)
    {

        m_jobSystem->FinishCompletedJobs();
    }
}

void JobSystemAPI::Destroy()
{
    if (isDestroyed)
        return; // Prevent double destruction
    m_jobSystem->FinishCompletedJobs();
    m_jobSystem->Destroy();
    m_jobSystem = nullptr;
    isDestroyed = true;
}

nlohmann::json JobSystemAPI::CreateJob(const char *jobName, nlohmann::json &input)
{
    return m_jobSystem->CreateJob(std::string(jobName), input);
}

nlohmann::json JobSystemAPI::JobStatus(std::string &jobID)
{
    int jobInt = stoi(jobID);
    int status = m_jobSystem->GetJobStatus(jobInt);

    nlohmann::json jsonResponse;
    jsonResponse["jobID"] = jobID;

    switch (status)
    {
    case JOB_STATUS_NEVER_SEEN:
        jsonResponse["status"] = "never seen";
        break;
    case JOB_STATUS_QUEUED:
        jsonResponse["status"] = "queued";
        break;
    case JOB_STATUS_RUNNING:
        jsonResponse["status"] = "running";
        break;
    case JOB_STATUS_COMPLETED:
        jsonResponse["status"] = "completed";
        break;
    case JOB_STATUS_RETIRED:
        jsonResponse["status"] = "retired";
        break;
    default:
        jsonResponse["status"] = "unknown";
        break;
    }

    return jsonResponse;
}

nlohmann::json JobSystemAPI::FinishJob(std::string &jobID)
{
    int jobInt = stoi(jobID);
    return m_jobSystem->FinishJob(jobInt);
}

nlohmann::json JobSystemAPI::GetJobTypes()
{
    std::vector<std::string> jobTypes = m_jobSystem->GetAvailableJobTypes();
    nlohmann::json jsonResponse;
    jsonResponse["availableJobTypes"] = jobTypes;
    return jsonResponse;
}

void JobSystemAPI::QueueJob(int jobId)
{
    m_jobSystem->QueueJob(jobId);
}

void JobSystemAPI::RegisterJob(const char *jobName, std::function<Job *()> jobFactory)
{
    m_jobSystem->RegisterJobType(std::string(jobName), jobFactory);
}

void JobSystemAPI::SetDependency(const char *dependentJobName, const char *dependencyJobName)
{
    m_jobSystem->SetDependency(std::string(dependentJobName), std::string(dependencyJobName));
}
