#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <set>
#include <vector>
#include "./lib/jobsystemapi.h"
#include "customjob.h"
#include "parsingjob.h"
#include "outputjob.h"
#include "flowscriptparser.h"

void registerAndQueueJobs(JobSystemAPI *jobSystem, nlohmann::json &flowscriptJobOutput)
{
    std::map<std::string, std::function<Job *()>> jobFactories = {
        {"compileJob", []() -> Job *
         { return new CustomJob(); }},
        {"compileParseJob", []() -> Job *
         { return new ParsingJob(); }},
        {"parseOutputJob", []() -> Job *
         { return new OutputJob(); }}};

    std::set<std::string> registeredJobs;
    std::map<std::string, int> jobIds;
    std::map<std::string, nlohmann::json> dataNodes;

    // First pass: Handle data nodes and register jobs
    for (const auto &el : flowscriptJobOutput.items())
    {
        const std::string &jobName = el.key();
        const nlohmann::json &jobInfo = el.value();

        if (jobInfo["type"] == 0)
        { // Data nodes
            dataNodes[jobName] = jobInfo["inputData"];
        }
        else if (jobInfo["type"] == 1)
        { // Executable jobs
            if (registeredJobs.find(jobName) == registeredJobs.end())
            {
                auto it = jobFactories.find(jobName);
                if (it != jobFactories.end())
                {
                    jobSystem->RegisterJob(jobName.c_str(), it->second);
                    registeredJobs.insert(jobName);
                    std::cout << "Registered job: " << jobName << std::endl;
                }
                else
                {
                    std::cerr << "Job factory not found for job: " << jobName << std::endl;
                }
            }
        }
    }

    // Second pass: Create jobs and set dependencies
    for (const auto &el : flowscriptJobOutput.items())
    {
        const std::string &jobName = el.key();
        const nlohmann::json &jobInfo = el.value();

        if (jobInfo["type"] == 1)
        { // Executable jobs
            // Set job input, considering data from data nodes
            nlohmann::json jobInput = jobInfo.value("inputData", nlohmann::json{});
            for (const auto &dep : jobInfo["dependencies"])
            {
                if (dataNodes.find(dep) != dataNodes.end())
                {
                    jobInput.merge_patch(dataNodes[dep]);
                }
            }

            auto creationResult = jobSystem->CreateJob(jobName.c_str(), jobInput);
            int createdJobId = creationResult["jobId"];
            jobIds[jobName] = createdJobId;
            std::cout << "Created job: " << jobName << " with ID: " << createdJobId << std::endl;

            for (const auto &dep : jobInfo["dependencies"])
            {
                std::string depKey = dep.get<std::string>(); // Convert to string explicitly

                if (flowscriptJobOutput.find(depKey) != flowscriptJobOutput.end()) // Check if key exists
                {
                    std::string actualDependency = depKey;

                    // Check if the dependency is a status node
                    if (flowscriptJobOutput[depKey]["type"] == 2)
                    {
                        if (!flowscriptJobOutput[depKey]["dependencies"].empty())
                        {
                            actualDependency = flowscriptJobOutput[depKey]["dependencies"][0];
                        }
                    }

                    if (!actualDependency.empty() && actualDependency != jobName)
                    {
                        jobSystem->SetDependency(jobName.c_str(), actualDependency.c_str());
                        std::cout << "Set dependency for " << jobName << " on " << actualDependency << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Dependency key not found: " << depKey << std::endl;
                }
            }

            bool shouldQueue = true;
            for (const auto &dep : jobInfo["dependencies"])
            {
                std::string depKey = dep.get<std::string>();
                if (flowscriptJobOutput.contains(depKey) && flowscriptJobOutput[depKey]["type"] != 0)
                {
                    shouldQueue = false;
                    break;
                }
            }

            if (shouldQueue)
            {
                jobSystem->QueueJob(jobIds[jobName]);
                std::cout << "Queued job: " << jobName << std::endl;
            }
        }
    }
}

int main(void)
{
    // Truncate the error report file at the start of the program
    std::ofstream jsonFile("./Data/error_report.json", std::ios::out | std::ios::trunc);
    if (!jsonFile.is_open())
    {
        std::cerr << "ERROR: Failed to open the JSON file for writing" << std::endl;
        return 1; // or handle the error as appropriate
    }
    jsonFile.close();

    // Create an instance of JobSystemAPI
    JobSystemAPI jobSystem;

    // Vector of job ids
    std::vector<int> jobIds;

    // Start the job system
    jobSystem.Start();

    // TODO create a flowscript job that writes a flowscript job from LLM in javascript
    // TODO register that job andd queue it and run it. Then queue flowscriptJob

    // Register custom job type
    std::cout << "Registering custom flowscript parsing job\n"
              << std::endl;

    jobSystem.RegisterJob("flowscriptJob", [&jobSystem]() -> Job *
                          { return new FlowScriptParseJob(&jobSystem); });

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

    std::cout << "Enter job ID to finish a specific job: \n";
    std::string jobID;
    std::cin >> jobID;

    nlohmann::json flowscriptFinish = jobSystem.FinishJob(jobID);
    std::cout << "Finishing Job " << jobID << " with result: " << flowscriptFinish.dump(4) << std::endl;

    // Now retrieve the output of the finished job
    nlohmann::json flowscriptJobOutput = jobSystem.GetJobOutput(stoi(jobID));
    if (!flowscriptJobOutput.empty())
    {
        // Process the output, e.g., for job queuing and dependency setting
        std::cout << "Enqueuing Jobs from FlowScript Graph! " << std::endl;
        registerAndQueueJobs(&jobSystem, flowscriptJobOutput);

        /*
            LLM Call node.js script
            */

        std::cout << "Registering gptCall Job" << std::endl;

        jobSystem.RegisterJob("gptCallJob", []() -> Job *
                              { return new CustomJob(); });

        // Create and enqueue gptCallJob job
        nlohmann::json gptCallJobInput = {{"command", "node ./Code/gptCall.js -file ./Data/error_report.json"}};
        nlohmann::json gptCallJobCreation = jobSystem.CreateJob("gptCallJob", gptCallJobInput);
        std::cout << "Creating Node Job: " << gptCallJobCreation.dump(4) << std::endl;

        /*
         Code Correction node.js script
        */
        std::cout << "Registering codeCorrection Job" << std::endl;

        jobSystem.RegisterJob("codeCorrectionJob", []() -> Job *
                              { return new CustomJob(); });

        // Create and enqueue codeCorrection job
        nlohmann::json codeCorrectionJobInput = {{"command", "node ./Code/codeCorrection.js"}};
        nlohmann::json codeCorrectionJobCreation = jobSystem.CreateJob("codeCorrectionJob", codeCorrectionJobInput);
        std::cout << "Creating Node Job: " << codeCorrectionJobCreation.dump(4) << std::endl;

        std::cout << "Queuing Jobs\n"
                  << std::endl;
        jobSystem.QueueJob(gptCallJobCreation["jobId"]);

        std::cout << "Queuing Jobs\n"
                  << std::endl;
        jobSystem.QueueJob(codeCorrectionJobCreation["jobId"]);

        jobSystem.Destroy();
    }
    else
    {
        std::cerr << "No output found for Job " << jobID << std::endl;
    }

    // std::string errorJsonFilePath = "./Data/error_report.json";
    // bool errorsResolved = false;

    // while (!errorsResolved)
    // {
    // Execute the job graph
    // TODO registerAndQueueJobs(jobSystem, flowscriptJobOutput);

    // Wait for parseOutputJob to complete and produce error JSON file
    // Polling mechanism
    // TODO waitForJobCompletion(parseOutputJobId);

    // Execute the JavaScript job to interact with LLM and modify source files
    // TODO executeJavaScriptJob(errorJsonFilePath);

    // Read the error JSON file to check if there are still errors
    // TODO nlohmann::json errorJson = readErrorJsonFile(errorJsonFilePath);

    // TODO  Check if error JSON file indicates no more errors
    // if (errorJson.is_null() || errorJson.empty() || allErrorsResolved(errorJson))
    // {
    //     errorsResolved = true;
    // }
    // else
    // {
    //     // Optionally, update the flowscriptJobOutput or other parameters based on the new errorJson
    //     // to provide updated information to the LLM in the next iteration
    // }
    // }

    return 0;
}