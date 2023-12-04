#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "./lib/jobsystemapi.h"
#include "customjob.h"

int main(void)
{
    // Create an instance of JobSystemAPI
    JobSystemAPI jobSystem;

    // Vector of job ids
    std::vector<int> jobIds;

    // Start the job system
    jobSystem.Start();

    // Register custom job type
    std::cout << "Registering custom job\n"
              << std::endl;

    jobSystem.RegisterJob("nodeJob", []() -> Job *
                          { return new CustomJob(); });

    // Create and enqueue a node job
    nlohmann::json nodeJobInput = {{"command", "node ./Code/index.js -file ./Data/error_report.json --ip http://localhost:4891/v1"}};
    nlohmann::json nodeJobCreation = jobSystem.CreateJob("nodeJob", nodeJobInput);
    std::cout << "Creating Node Job: " << nodeJobCreation.dump(4) << std::endl;
    jobIds.push_back(nodeJobCreation["jobId"]);

    for (auto id : jobIds)
    {
        jobSystem.QueueJob(id);
    }
    std::cout << "Queuing Jobs\n"
              << std::endl;

    int running = 1;

    while (running)
    {
        std::string command;
        std::cout << "Enter status or jobtypes: \n";
        std::cin >> command;

        if (command == "status")
        {
            std::cout << "Enter job ID to get the status of that job: \n";
            std::string jobID;
            std::cin >> jobID;

            nlohmann::json jobStatus = jobSystem.JobStatus(jobID);
            std::cout << "Job " << jobID << " Status: " << jobStatus.dump(4) << std::endl;
        }
        else if (command == "jobtypes")
        {
            std::cout << jobSystem.GetJobTypes().dump(4) << std::endl;
        }
    }

    return 0;
}
