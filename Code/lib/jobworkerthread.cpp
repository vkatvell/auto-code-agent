#include "jobworkerthread.h"
#include "jobsystem.h"
#include <iostream>

JobWorkerThread::JobWorkerThread(const char *uniqueName, unsigned long workerJobChannels, JobSystem *jobSystem) : m_uniqueName(uniqueName),
                                                                                                                  m_workerJobChannels(workerJobChannels),
                                                                                                                  m_jobSystem(jobSystem)
{
}

JobWorkerThread::~JobWorkerThread()
{
    // If we haven't already, signal thread that we should exit as soon as it can (after its current job, if any)
    ShutDown();

    // Block on the thread's main until it has a chance to finish its current job and exit
    m_thread->join();
    delete m_thread;
    m_thread = nullptr;
}

void JobWorkerThread::StartUp()
{
    m_thread = new std::thread(WorkerThreadMain, this);
}

void JobWorkerThread::Work()
{
    // While the thread is not signaled to stop, keep working
    while (!IsStopping())
    {
        // Protect the worker status
        std::lock_guard<std::mutex> lock(m_workerStatusMutex);
        unsigned long workerJobChannels = m_workerJobChannels;

        // Claim a job from the queue
        Job *job = m_jobSystem->ClaimAJob(m_workerJobChannels);
        if (job)
        {
            // Execute only if dependencies are met
            if (m_jobSystem->AreDependenciesResolved(job->GetUniqueID()))
            {
                // Call the execute function of the job
                job->Execute();
                // Signal the jobsystem that the job is done and ready to be cleaned up
                m_jobSystem->OnJobCompleted(job);
            }
            else
            {
                // If dependencies are not resolved, re-queue the job
                m_jobSystem->QueueJob(job->GetUniqueID());
                // Wait for a short duration before attempting to claim a job again
                // This prevents the thread from spinning too quickly and re-claiming the same job
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // If no job was claimed, wait for a short duration before trying again
        // This reduces the chance of busy-waiting if the job queue is empty
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void JobWorkerThread::ShutDown()
{
    std::lock_guard<std::mutex> lock(m_workerStatusMutex);
    m_isStopping = true;
}

bool JobWorkerThread::IsStopping() const
{
    std::lock_guard<std::mutex> lock(m_workerStatusMutex);
    bool shouldClose = m_isStopping;
    return shouldClose;
}

void JobWorkerThread::SetWorkerJobChannels(unsigned long workerJobChannels)
{
    std::lock_guard<std::mutex> lock(m_workerStatusMutex);
    m_workerJobChannels = workerJobChannels;
}

void JobWorkerThread::WorkerThreadMain(void *workerThreadObject)
{
    JobWorkerThread *thisWorker = (JobWorkerThread *)workerThreadObject;
    thisWorker->Work();
}