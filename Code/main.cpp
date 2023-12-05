// #include <iostream>
// #include <fstream>
// #include <string>
// #include <thread>
// #include <vector>
// #include "./lib/jobsystemapi.h"
// #include "customjob.h"

// int main(void)
// {
//     // Create an instance of JobSystemAPI
//     JobSystemAPI jobSystem;

//     // Vector of job ids
//     std::vector<int> jobIds;

//     // Start the job system
//     jobSystem.Start();

//     // Register custom job type
//     std::cout << "Registering custom job\n"
//               << std::endl;

//     jobSystem.RegisterJob("nodeJob", []() -> Job *
//                           { return new CustomJob(); });

//     // Create and enqueue a node job
//     nlohmann::json nodeJobInput = {{"command", "node ./Code/index.js -file ./Data/error_report.json --ip http://localhost:4891/v1"}};
//     nlohmann::json nodeJobCreation = jobSystem.CreateJob("nodeJob", nodeJobInput);
//     std::cout << "Creating Node Job: " << nodeJobCreation.dump(4) << std::endl;
//     jobIds.push_back(nodeJobCreation["jobId"]);

//     for (auto id : jobIds)
//     {
//         jobSystem.QueueJob(id);
//     }
//     std::cout << "Queuing Jobs\n"
//               << std::endl;

//     int running = 1;

//     while (running)
//     {
//         std::string command;
//         std::cout << "Enter status or jobtypes: \n";
//         std::cin >> command;

//         if (command == "status")
//         {
//             std::cout << "Enter job ID to get the status of that job: \n";
//             std::string jobID;
//             std::cin >> jobID;

//             nlohmann::json jobStatus = jobSystem.JobStatus(jobID);
//             std::cout << "Job " << jobID << " Status: " << jobStatus.dump(4) << std::endl;
//         }
//         else if (command == "jobtypes")
//         {
//             std::cout << jobSystem.GetJobTypes().dump(4) << std::endl;
//         }
//     }

//     return 0;
// }

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "./lib/jobsystemapi.h"
#include "customjob.h"
#include "parsingjob.h"
#include "outputjob.h"
#include "flowscriptparser.h"

int main(void)
{
    // Create an instance of JobSystemAPI
    JobSystemAPI jobSystem;

    // Vector of job ids
    std::vector<int> jobIds;

    // Start the job system
    jobSystem.Start();

    // Register custom job type
    std::cout << "Registering custom flowscript parsing job\n"
              << std::endl;

    jobSystem.RegisterJob("flowscriptJob", []() -> Job *
                          { return new FlowScriptParseJob(); });

    // Read in the file here and create JSON object input
    std::ifstream dotFile1("./Data/fstest1.dot");

    if (!dotFile1.is_open())
    {
        throw std::runtime_error("Failed to open Flowscript file");
    }

    std::string flowscriptText;
    std::string line;
    while (std::getline(dotFile1, line))
    {
        flowscriptText += line;
    }

    dotFile1.close();

    // Create and enqueue a flowscript parse job
    nlohmann::json flowscriptJobInput = {{"flowscript", flowscriptText}};
    nlohmann::json flowscriptJobCreation = jobSystem.CreateJob("flowscriptJob", flowscriptJobInput);
    std::cout << "Creating FlowScript Job 1: " << flowscriptJobCreation.dump(4) << std::endl;
    jobIds.push_back(flowscriptJobCreation["jobId"]);

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
        std::cout << "Enter stop, start, finish, status, jobtypes, or destroy: \n";
        std::cin >> command;

        if (command == "stop")
        {
            jobSystem.Stop();
            std::cout << "Stopping Job System" << std::endl;
        }
        else if (command == "destroy")
        {
            jobSystem.Destroy();
            running = 0;
            std::cout << "Destroying Job System" << std::endl;
        }
        else if (command == "finish")
        {
            std::cout << "Enter job ID to finish a specific job: \n";
            std::string jobID;
            std::cin >> jobID;

            nlohmann::json jobResult = jobSystem.FinishJob(jobID);
            std::cout << "Finishing Job " << jobID << " with result: " << jobResult.dump(4) << std::endl;
        }
        else if (command == "status")
        {
            std::cout << "Enter job ID to get the status of that job: \n";
            std::string jobID;
            std::cin >> jobID;

            nlohmann::json jobStatus = jobSystem.JobStatus(jobID);
            std::cout << "Job " << jobID << " Status: " << jobStatus.dump(4) << std::endl;
        }
        else if (command == "start")
        {
            jobSystem.Start();
            std::cout << "Starting the Job System back up" << std::endl;
        }
        else if (command == "jobtypes")
        {
            std::cout << jobSystem.GetJobTypes().dump(4) << std::endl;
        }
    }

    return 0;
}