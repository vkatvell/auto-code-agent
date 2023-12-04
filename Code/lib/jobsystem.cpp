#include <iostream>
#include <algorithm>
#include <functional>
#include <nlohmann/json.hpp>
#include "jobsystem.h"
#include "jobworkerthread.h"
#include "job.h"

JobSystem *JobSystem::s_jobSystem = nullptr;

typedef void (*JobCallback)(Job *completedJob);

JobSystem::JobSystem()
{
    m_jobHistory.reserve(256 * 1024);
}

JobSystem::~JobSystem()
{
    std::lock_guard<std::mutex> lock(m_workerThreadMutex);
    int numWorkerThreads = (int)m_workerThreads.size();

    // First, tell each worker thread to stop picking up jobs
    for (int i = 0; i < numWorkerThreads; ++i)
    {
        m_workerThreads[i]->ShutDown();
    }

    // Deleting the job from the queue
    while (!m_workerThreads.empty())
    {
        delete m_workerThreads.back();
        m_workerThreads.pop_back();
    }
}

JobSystem *JobSystem::CreateOrGet()
{
    if (!s_jobSystem)
    {
        s_jobSystem = new JobSystem();
    }
    return s_jobSystem;
}

void JobSystem::Destroy()
{
    if (s_jobSystem)
    {
        delete s_jobSystem;
        s_jobSystem = nullptr;
    }
}

void JobSystem::CreateWorkerThread(const char *uniqueName, unsigned long workerJobChannels)
{
    JobWorkerThread *newWorker = new JobWorkerThread(uniqueName, workerJobChannels, this);
    std::lock_guard<std::mutex> lock(m_workerThreadMutex);
    m_workerThreads.push_back(newWorker);

    m_workerThreads.back()->StartUp();
}

void JobSystem::DestroyWorkerThread(const char *uniqueName)
{
    std::lock_guard<std::mutex> lock(m_workerThreadMutex);

    auto it = std::find_if(m_workerThreads.begin(), m_workerThreads.end(), [uniqueName](JobWorkerThread *worker)
                           { return std::string(worker->m_uniqueName) == uniqueName; });

    if (it != m_workerThreads.end())
    {
        JobWorkerThread *doomedWorker = *it;
        m_workerThreads.erase(it);
        doomedWorker->ShutDown();
        delete doomedWorker;
    }
}

void JobSystem::QueueJob(int jobID)
{
    std::lock_guard<std::mutex> lockQueued(m_jobsQueuedMutex);

    std::lock_guard<std::mutex> lockMap(m_jobsMutex);
    Job *job = m_jobs[jobID];

    // Job history entry
    std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);

    m_jobHistory.emplace_back(JobHistoryEntry(job->m_jobType, JOB_STATUS_QUEUED));

    m_jobsQueued.push_back(job);
}

nlohmann::json JobSystem::GetAJobStatus(const std::string &jobName)
{
    // Get the status of a job based on its ID
    // Return the job status as JSON

    std::lock_guard<std::mutex> lockRunning(m_jobsRunningMutex);

    // Find the job by name and get ID
    auto it = std::find_if(m_jobsRunning.begin(), m_jobsRunning.end(), [&jobName](const Job *job)
                           { return job->GetJobName() == jobName; });

    if (it == m_jobsRunning.end())
    {
        return nlohmann::json{{"status", "error"}, {"message", "Job not found"}};
    }

    int jobID = (*it)->GetUniqueID();
    JobStatus jobStatus = GetJobStatus(jobID);

    // Create a response based on the status
    std::string statusString;
    switch (jobStatus)
    {
    case JOB_STATUS_COMPLETED:
        statusString = "completed";
        break;
    case JOB_STATUS_RUNNING:
        statusString = "in progress";
        break;
    case JOB_STATUS_QUEUED:
        statusString = "queued";
        break;
    case JOB_STATUS_RETIRED:
        statusString = "retired";
        break;
    case JOB_STATUS_NEVER_SEEN:
        statusString = "never seen";
        break;
    default:
        statusString = "unknown";
        break;
    }

    return nlohmann::json{{"status", statusString}};
}

std::vector<std::string> JobSystem::GetAvailableJobTypes()
{
    return m_availableJobTypes;
}

void JobSystem::RegisterJobType(const std::string &jobType, std::function<Job *()> jobFactory)
{
    std::lock_guard<std::mutex> lockJobFactory(m_jobFactoriesMutex);
    m_jobFactories[jobType] = jobFactory;

    std::lock_guard<std::mutex> lockJobTypes(m_availableJobTypeMutex);
    m_availableJobTypes.push_back(jobType);
}

nlohmann::json JobSystem::CreateJob(const std::string &jobType, nlohmann::json &input)
{
    // Create a job of the specified type and provide the input data
    // return the job ID or status
    std::lock_guard<std::mutex> lockFactory(m_jobFactoriesMutex);
    std::lock_guard<std::mutex> lockDependencies(m_jobDependenciesMutex);

    auto it = m_jobFactories.find(jobType);

    if (it == m_jobFactories.end())
    {
        // Factory not found, return an error json
        return nlohmann::json{{"error", "Job type not registered"}};
    }

    // Create a new instance of the job type
    Job *job = it->second();
    if (job == nullptr)
    {
        // Failed to create job, return an error json
        return nlohmann::json{{"error", "Failed to create job instance"}};
    }

    // Initialize the job with input
    job->SetInput(input);

    // Mapping job names to their unique ID value to use in the SetDependency function
    std::lock_guard<std::mutex> lockJobNameID(m_jobNameToIDMutex);
    m_jobNameToID[jobType] = job->GetUniqueID();

    std::lock_guard<std::mutex> lockJobMap(m_jobsMutex);
    m_jobs[job->GetUniqueID()] = job;

    // /*
    //     TODO This code queues the job in the system, but maybe we should create
    //     another function that calls for dependencies to be set before queuing
    //     the job:

    // // Store the job in the system
    // std::lock_guard<std::mutex> lockQueued(m_jobsQueuedMutex);

    // // Job history entry
    // std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);
    // m_jobHistory.emplace_back(JobHistoryEntry(job->m_jobType, JOB_STATUS_QUEUED));

    // m_jobsQueued.push_back(job);
    // */

    // Return a JSON object with the job ID and other details
    nlohmann::json response = {
        {"jobId", job->GetUniqueID()},
        {"status", "Job created"},
        {"dependencies", m_jobDependencies[job->GetUniqueID()]}};

    return response;
}

void JobSystem::SetDependency(const std::string &dependentJobName, const std::string &dependencyJobName)
{

    // Go through job factories and find matching job name
    std::lock_guard<std::mutex> lockFactory(m_jobFactoriesMutex);

    // Check if dependent job is registered
    auto dependentJobIter = m_jobFactories.find(dependentJobName);
    if (dependentJobIter == m_jobFactories.end())
    {
        // Dependent job factory not found, handle error...
        std::cerr << "Dependent job factory not found for job: " << dependentJobName << std::endl;
        return;
    }

    // Check if dependency job is registered
    auto dependencyJobIter = m_jobFactories.find(dependencyJobName);
    if (dependencyJobIter == m_jobFactories.end())
    {
        // Factory not found, return an error json
        std::cerr << "Dependency job factory not found for job: " << dependencyJobName << std::endl;
        return;
    }

    // Checking map for id values of job names
    std::lock_guard<std::mutex> lockJobIDMap(m_jobNameToIDMutex);
    int dependentJobId = m_jobNameToID[dependentJobName];
    int dependencyJobId = m_jobNameToID[dependencyJobName];

    // Assigning dependency map with id values of dependent jobs
    std::lock_guard<std::mutex> lock(m_jobDependenciesMutex);
    // std::cout << "\nSetting dependency. Dependent Job ID: " << dependentJobId
    //           << " (" << dependentJobName << "), Dependency Job ID: " << dependencyJobId
    //           << " (" << dependencyJobName << ")" << std::endl;
    m_jobDependencies[dependentJobId].push_back(dependencyJobId);
}

// Checking job dependencies for a specific job
bool JobSystem::AreDependenciesResolved(int jobID)
{
    std::lock_guard<std::mutex> lockDependencies(m_jobDependenciesMutex);

    // Check if the job has dependencies based on its unique ID
    auto dependenciesIt = m_jobDependencies.find(jobID);
    if (dependenciesIt == m_jobDependencies.end())
    {
        // No dependencies for this job
        return true;
    }

    // Iterate over all dependencies and check their status
    for (int dependencyID : dependenciesIt->second)
    {
        // Check status for a job given id from its pair
        if (!IsJobComplete(dependencyID))
        {
            // If any dependency is not completed, returns false
            return false;
        }
    }

    // All dependencies are completed
    return true;
}

JobStatus JobSystem::GetJobStatus(int jobID) const
{
    std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);

    JobStatus jobStatus = JOB_STATUS_NEVER_SEEN;
    if (jobID < (int)m_jobHistory.size())
    {
        jobStatus = (JobStatus)(m_jobHistory[jobID].m_jobStatus);
    }

    return jobStatus;
}

bool JobSystem::IsJobComplete(int jobID) const
{
    return (GetJobStatus(jobID)) == (JOB_STATUS_COMPLETED);
}

void JobSystem::FinishCompletedJobs()
{
    // Creating a double ended queue for holding completed jobs
    std::deque<Job *> jobsCompleted;

    std::lock_guard<std::mutex> lockCompleted(m_jobsCompletedMutex);
    jobsCompleted.swap(m_jobsCompleted);

    // Iterating through jobs in jobsCompleted
    // and calling each job's individual callback functions
    for (Job *job : jobsCompleted)
    {
        job->JobCompleteCallback();

        // Changing the status of the job in the jobHistory vector
        std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);
        m_jobHistory[job->m_jobID].m_jobStatus = JOB_STATUS_RETIRED;
        delete job;
    }
}

nlohmann::json JobSystem::FinishJob(int jobID)
{
    nlohmann::json response;

    while (!IsJobComplete(jobID))
    {
        JobStatus jobStatus = GetJobStatus(jobID);

        // Checking job status before finishing the job
        if ((jobStatus == JOB_STATUS_NEVER_SEEN) || (jobStatus == JOB_STATUS_RETIRED))
        {
            response["status"] = "error";
            response["message"] = "Waiting for Job(#:" + std::to_string(jobID) + ") - no such job in JobSystem!";
            return response;
        }

        // Accessing the jobsCompleted deque
        std::lock_guard<std::mutex> lockCompleted(m_jobsCompletedMutex);
        Job *thisCompletedJob = nullptr;
        for (auto jqIter = m_jobsCompleted.begin(); jqIter != m_jobsCompleted.end(); ++jqIter)
        {
            Job *someCompletedJob = *jqIter;
            if (someCompletedJob->m_jobID == jobID)
            {
                // For matching jobID, delete the job from the jobsCompleted deque
                thisCompletedJob = someCompletedJob;
                m_jobsCompleted.erase(jqIter);
                break;
            }
        }

        if (thisCompletedJob == nullptr)
        {
            response["status"] = "error";
            response["message"] = "Job #" + std::to_string(jobID) + " was status complete but not found in Completed List of jobs";
            return response;
        }

        // Call the job's callback function
        thisCompletedJob->JobCompleteCallback();

        // Change the status of the job in the jobHistory vector
        std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);
        m_jobHistory[thisCompletedJob->m_jobID].m_jobStatus = JOB_STATUS_RETIRED;

        // Handle the memory for the job
        delete thisCompletedJob;

        // If we reach this point, it means the job has been successfully finished
        response["status"] = "success";
        response["message"] = "Job #" + std::to_string(jobID) + " finished successfully";
        return response;
    }

    // If job is already complete by the time we check
    response["status"] = "success";
    response["message"] = "Job #" + std::to_string(jobID) + " was already completed";
    return response;
}

void JobSystem::OnJobCompleted(Job *jobJustExecuted)
{
    // Protect the jobCompleted and jobRunning deques
    std::lock_guard<std::mutex> lockCompleted(m_jobsCompletedMutex);
    std::lock_guard<std::mutex> lockRunning(m_jobsRunningMutex);

    std::deque<Job *>::iterator runningJobItr = m_jobsRunning.begin();
    for (; runningJobItr != m_jobsRunning.end(); ++runningJobItr)
    {
        if (jobJustExecuted == *runningJobItr)
        {
            // Protect the jobHistory vector
            std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);
            // Remove the job from the running deque
            m_jobsRunning.erase(runningJobItr);
            // Add the job to the jobs completed deque
            m_jobsCompleted.push_back(jobJustExecuted);
            // Changed the status of the job in the job history vector
            m_jobHistory[jobJustExecuted->m_jobID].m_jobStatus = JOB_STATUS_COMPLETED;
            break;
        }
        if (runningJobItr == m_jobsRunning.end())
        {
            std::cout << "Job ID: " << jobJustExecuted->GetUniqueID() << " not found in the running jobs."
                      << std::endl;
        }
    }

    // Getting output from previous job to set as input for next
    nlohmann::json output = jobJustExecuted->GetOutput();

    for (auto iter = m_jobDependencies.begin(); iter != m_jobDependencies.end(); ++iter)
    {
        int depJobId = iter->first;
        std::vector<int> &dependencies = iter->second;
        auto findIter = std::find(dependencies.begin(), dependencies.end(), jobJustExecuted->GetUniqueID());
        if (findIter != dependencies.end())
        {
            Job *dependentJob = m_jobs[depJobId];
            dependentJob->SetInput(output);

            // Check if all dependencies of the dependent job are now resolved
            if (AreDependenciesResolved(depJobId))
            {
                // Queue the dependent job for execution
                QueueJob(depJobId);
            }

            // Since we found and processed the dependency, we can remove it from the list.
            dependencies.erase(findIter);

            // If after removing the resolved dependency the list is empty, we can remove the entry from the map.
            if (dependencies.empty())
            {
                m_jobDependencies.erase(iter);
                // Need to break because we've modified the iterator.
                break;
            }
        }
    }
}

Job *JobSystem::ClaimAJob(unsigned long workerJobChannels)
{
    // Protect the queued jobs and jobs running deques
    std::lock_guard<std::mutex> lockQueued(m_jobsQueuedMutex);
    std::lock_guard<std::mutex> lockRunning(m_jobsRunningMutex);

    Job *claimedJob = nullptr;

    std::deque<Job *>::iterator queuedJobItr = m_jobsQueued.begin();
    for (; queuedJobItr != m_jobsQueued.end(); ++queuedJobItr)
    {
        Job *queuedJob = *queuedJobItr;

        if ((queuedJob->m_jobChannels & workerJobChannels) != 0)
        {
            claimedJob = queuedJob;

            // Protect the job history vector
            std::lock_guard<std::mutex> lockHistory(m_jobHistoryMutex);
            // Remove the job from the job queue
            m_jobsQueued.erase(queuedJobItr);
            // Add the job to the running jobs deque
            m_jobsRunning.push_back(queuedJob);
            // Change the job status of the job in the job history vector
            m_jobHistory[claimedJob->m_jobID].m_jobStatus = JOB_STATUS_RUNNING;

            break;
        }
    }

    return claimedJob;
}