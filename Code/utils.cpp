#include "utils.h"
#include <fstream>
#include <string>
#include <thread>
#include <set>
#include <chrono>
#include <filesystem>
#include "./lib/jobsystemapi.h"
#include "customjob.h"
#include "compilejob.h"
#include "parsingjob.h"
#include "outputjob.h"
#include "flowscriptparser.h"

void registerAndQueueJobs(JobSystemAPI *jobSystem, nlohmann::json &flowscriptJobOutput)
{
    std::map<std::string, std::function<Job *()>> jobFactories = {
        {"compileJob", []() -> Job *
         { return new CompileJob(); }},
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
    std::cout << "Opening Error Report JSON file: " << errorReportPath << "\n"
              << std::endl;

    std::ifstream jsonFile(errorReportPath);
    if (!jsonFile.is_open())
    {
        std::cerr << "ERROR: Failed to open the Error JSON file for reading!" << std::endl;
        return true; // Indicates an error if the file can't be opened
    }

    // Check if the file is empty or contains "null"
    std::string fileContent((std::istreambuf_iterator<char>(jsonFile)), std::istreambuf_iterator<char>());
    jsonFile.close();

    // Trim leading and trailing whitespace from file content
    fileContent.erase(0, fileContent.find_first_not_of(" \t\n\r\f\v"));
    fileContent.erase(fileContent.find_last_not_of(" \t\n\r\f\v") + 1);

    if (fileContent.empty() || fileContent == "null")
    {
        std::cout << "No compilation errors (file is empty or contains 'null').\n"
                  << std::endl;
        return false; // No errors if file is empty or contains 'null'
    }
    else
    {
        std::cout << "Compilation errors present.\n"
                  << std::endl;
        return true; // Errors are present
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
    std::cout << "Creating FlowScript Parse Job: " << flowscriptJobCreation.dump(4) << std::endl;

    jobSystem.QueueJob(flowscriptJobCreation["jobId"]);
    std::cout << "Queuing FlowScript Parse Job with ID: " << flowscriptJobCreation["jobId"] << std::endl;

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
        std::cout << "\nEnqueuing Jobs from FlowScript Graph! \n"
                  << std::endl;
        registerAndQueueJobs(&jobSystem, flowscriptJobOutput);

        // Waiting for compile flow to complete and for error_report.json to be closed
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check for compilation errors using hasCompilationErrors function
        if (!hasCompilationErrors("./Data/error_report.json"))
        {
            std::cout << "No more compilation errors. Skipping gptCallJob.\n"
                      << std::endl;
        }
        else
        {
            /*
        LLM Call node.js script
        */
            std::cout << "Registering gptCall Job\n"
                      << std::endl;

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

            // Short delay before starting the polling
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Timeout duration is 10 seconds
            const int timeoutDuration = 10;

            // Start time for timeout calculation
            auto startTime = std::chrono::high_resolution_clock::now();

            // Polling for gptCall job completion
            std::string correctedCodePath = "./Data/corrected_code.json";
            std::string correctionHistoryPath = "./Data/correction_history.json";
            std::string codeChangeDescriptionPath = "./Data/code_change_descriptions.txt";

            std::time_t lastModifiedTimeCorrectedCode = std::filesystem::exists(correctedCodePath) ? std::filesystem::last_write_time(correctedCodePath).time_since_epoch().count() : 0;
            std::time_t lastModifiedTimeCorrectionHistory = std::filesystem::exists(correctionHistoryPath) ? std::filesystem::last_write_time(correctionHistoryPath).time_since_epoch().count() : 0;
            std::time_t lastModifiedTimeCodeChangeDescription = std::filesystem::exists(codeChangeDescriptionPath) ? std::filesystem::last_write_time(codeChangeDescriptionPath).time_since_epoch().count() : 0;

            // Polling loop for gptCall job
            while (true)
            {
                std::cout << "Polling for gptCall job completion..."
                          << std::endl;

                // Check if either file is updated
                if (isFileUpdated(correctedCodePath, lastModifiedTimeCorrectedCode) && isFileUpdated(correctionHistoryPath, lastModifiedTimeCorrectedCode))
                {
                    std::cout << "File has been modified, breaking out of loop: \n"
                              << std::endl;
                    // Exit loop if files are updated
                    break;
                }

                // Check for timeout
                auto currentTime = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
                if (elapsed >= timeoutDuration)
                {
                    std::cerr << "Timeout reached while waiting for gptCallJob.\n";
                    break;
                }

                // Sleep for a short duration before next check
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // Sleep for a short duration before queuing codeCorrection job
            std::this_thread::sleep_for(std::chrono::seconds(5));

            std::cout << "Queuing codeCorrection Job: \n"
                      << std::endl;
            jobSystem.QueueJob(codeCorrectionJobCreation["jobId"]);

            // Reset start time for the next job's timeout
            startTime = std::chrono::high_resolution_clock::now();

            // Polling loop for codeCorrection job
            while (true)
            {
                std::cout << "Polling for codeCorrection job completion..."
                          << std::endl;
                if (isFileUpdated(codeChangeDescriptionPath, lastModifiedTimeCodeChangeDescription))
                {
                    std::cout << "code_change_descriptions.txt file has been modified, breaking out of loop: \n"
                              << std::endl;
                    break; // Exit loop if file is updated
                }

                // Check for timeout
                auto currentTime = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
                if (elapsed >= timeoutDuration)
                {
                    std::cerr << "Timeout reached while waiting for codeCorrectionJob.\n";
                    break;
                }

                // Sleep for a short duration before next check
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    else
    {
        std::cerr << "No output found for Job " << jobID << std::endl;
    }
}
