#include "utils.h"
#include <fstream>
#include <string>
#include <thread>
#include <set>
#include <chrono>
#include <filesystem>
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

bool hasCompilationErrors(const std::string &errorReportPath)
{
    std::cout << "Opening Error Report JSON file: " << errorReportPath << std::endl;

    std::ifstream jsonFile(errorReportPath);
    if (!jsonFile.is_open())
    {
        std::cerr << "ERROR: Failed to open the Error JSON file for reading!" << std::endl;
        return true; // Indicates an error if file can't be opened
    }

    std::string fileContent((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
    jsonFile.close();

    if (fileContent == "null")
    {
        std::cout << "No compilation errors (file contains 'null')." << std::endl;
        return false; // No errors if file contains 'null'
    }

    std::cout << "File Content: \n"
              << fileContent << std::endl;

    try
    {
        nlohmann::json errorReport = nlohmann::json::parse(fileContent);
        std::cout << "Error Report JSON parsed successfully." << std::endl;
        return !errorReport.is_null(); // Check if parsed JSON is not null
    }
    catch (const nlohmann::json::parse_error &e)
    {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return true; // Treat parsing error as presence of errors
    }
}

bool isFileUpdated(const std::string &filePath, const std::time_t &lastModifiedTime)
{
    std::filesystem::path path(filePath);
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    auto currentModifiedTime = std::filesystem::last_write_time(path).time_since_epoch().count();
    return currentModifiedTime > lastModifiedTime;
}

void runFlowScript(JobSystemAPI &jobSystem, const std::string &flowscriptText)
{
    // Truncate the error report file at the start of the program
    std::ofstream jsonFile("./Data/error_report.json", std::ios::out | std::ios::trunc);
    if (!jsonFile.is_open())
    {
        std::cerr << "ERROR: Failed to open the JSON file for writing" << std::endl;
    }
    jsonFile.close();

    // Create and enqueue a flowscript parse job
    nlohmann::json flowscriptJobInput = {{"flowscript", flowscriptText}};
    nlohmann::json flowscriptJobCreation = jobSystem.CreateJob("flowscriptJob", flowscriptJobInput);
    std::cout << "Creating FlowScript Job 1: " << flowscriptJobCreation.dump(4) << std::endl;

    jobSystem.QueueJob(flowscriptJobCreation["jobId"]);
    std::cout << "Queuing FlowScript Job with ID: " << flowscriptJobCreation["jobId"] << std::endl;

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

        // Create gptCallJob job
        nlohmann::json gptCallJobInput = {{"command", "node ./Code/gptCall.js -file ./Data/error_report.json"}};
        nlohmann::json gptCallJobCreation = jobSystem.CreateJob("gptCallJob", gptCallJobInput);
        std::cout << "Creating Node Job: " << gptCallJobCreation.dump(4) << std::endl;

        /*
         Code Correction node.js script
        */
        std::cout << "Registering codeCorrection Job" << std::endl;

        jobSystem.RegisterJob("codeCorrectionJob", []() -> Job *
                              { return new CustomJob(); });

        // Create codeCorrection job
        nlohmann::json codeCorrectionJobInput = {{"command", "node ./Code/codeCorrection.js"}};
        nlohmann::json codeCorrectionJobCreation = jobSystem.CreateJob("codeCorrectionJob", codeCorrectionJobInput);
        std::cout << "Creating Node Job: " << codeCorrectionJobCreation.dump(4) << std::endl;

        std::cout << "Queuing gptCall Job: \n"
                  << std::endl;
        jobSystem.QueueJob(gptCallJobCreation["jobId"]);

        // Introduce a short delay before starting the polling
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Polling for gptCall job completion
        std::string correctedCodePath = "./Data/corrected_code.json";
        std::string correctionHistoryPath = "./Data/correction_history.json";
        std::time_t lastModifiedTime = 0;
        // Polling loop for gptCall job
        while (true)
        {
            std::cout << "In polling loop: \n"
                      << std::endl;
            // Check if either file is updated
            if (isFileUpdated(correctedCodePath, lastModifiedTime) || isFileUpdated(correctionHistoryPath, lastModifiedTime))
            {
                std::cout << "File has been modified, breaking out of loop: \n"
                          << std::endl;
                // Exit loop if files are updated
                break;
            }

            // Sleep for a short duration before next check
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        std::cout << "Queuing codeCorrection Job: \n"
                  << std::endl;
        jobSystem.QueueJob(codeCorrectionJobCreation["jobId"]);

        // Polling for codeCorrection job completion
        std::string codeChangeDescriptionPath = "./Data/code_change_descriptions.txt";
        std::time_t lastModifiedTimeCodeChange = 0;

        // Polling loop for codeCorrection job
        while (true)
        {
            std::cout << "Polling for codeCorrection job completion: \n"
                      << std::endl;
            if (isFileUpdated(codeChangeDescriptionPath, lastModifiedTimeCodeChange))
            {
                std::cout << "code_change_descriptions.txt file has been modified, breaking out of loop: \n"
                          << std::endl;
                break; // Exit loop if file is updated
            }

            // Sleep for a short duration before next check
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else
    {
        std::cerr << "No output found for Job " << jobID << std::endl;
    }
}
