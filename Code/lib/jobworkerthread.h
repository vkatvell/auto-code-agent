#pragma once
#include <mutex>
#include <map>
#include <deque>
#include <vector>
#include <thread>

#include "job.h"

class JobSystem;

class JobWorkerThread
{
    friend class JobSystem;

    // Only job system should be affecting this stuff
private:
    JobWorkerThread(const char *uniqueName, unsigned long workerJobChannels, JobSystem *jobSystem);
    ~JobWorkerThread();

    void StartUp();  // Kick off actual thread, which will call Work()
    void Work();     // Called in private thread, blocks forever until StopWorking() is called
    void ShutDown(); // Signal that work should stop at next opportunity

    bool IsStopping() const;
    void SetWorkerJobChannels(unsigned long workerJobChannels);
    static void WorkerThreadMain(void *workerThreadObject);

private:
    const char *m_uniqueName;
    unsigned long m_workerJobChannels = 0xffffffff;
    bool m_isStopping = false;
    JobSystem *m_jobSystem = nullptr;
    std::thread *m_thread = nullptr;
    mutable std::mutex m_workerStatusMutex;
};
