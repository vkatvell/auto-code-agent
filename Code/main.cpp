#include <iostream>
#include <fstream>
#include <string>
#include "./lib/jobsystemapi.h"
#include "utils.h"
#include "flowscriptparser.h"

int main(int argc, char *argv[])
{
    // Create an instance of JobSystemAPI
    JobSystemAPI jobSystem;

    // Start the job system
    jobSystem.Start();

    // Parse command line arguments
    std::string filePathArg;
    if (argc > 1)
    {
        // First argument is the file path
        filePathArg = argv[1];
    }
    else
    {
        throw std::runtime_error("No file path argument provided");
    }

    // Construct the command for flowscriptGenJobInput
    std::string command = "node ./Code/flowScriptGen.js -files " + filePathArg;

    std::cout << "Registering FlowScript Generation job\n"
              << std::endl;
    jobSystem.RegisterJob("flowscriptGenJob", []() -> Job *
                          { return new CustomJob(); });

    // Create flowscriptGen job
    nlohmann::json flowscriptGenJobInput = {{"command", command}};
    nlohmann::json flowscriptGenJobCreation = jobSystem.CreateJob("flowscriptGenJob", flowscriptGenJobInput);
    std::cout << "Creating FlowScript Generation Job: " << flowscriptGenJobCreation.dump(4) << std::endl;

    std::cout << "Queuing flowscriptGenJob: \n"
              << std::endl;
    jobSystem.QueueJob(flowscriptGenJobCreation["jobId"]);

    // Introducing delay to wait for flowscript.dot file to be created
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // Register flowscript parse job type
    std::cout << "Registering custom flowscript parsing job\n"
              << std::endl;
    jobSystem.RegisterJob("flowscriptJob", [&jobSystem]() -> Job *
                          { return new FlowScriptParseJob(&jobSystem); });

    // Read in the file here and create JSON object input
    std::string errorReportPath = "./Data/error_report.json";

    std::ifstream dotFile("./Data/flowscript.dot");

    if (!dotFile.is_open())
    {
        throw std::runtime_error("Failed to open Flowscript file");
    }

    std::string flowscriptText;
    std::string line;
    while (std::getline(dotFile, line))
    {
        flowscriptText += line;
    }

    dotFile.close();

    // Begin execution of FlowScript and loop until no compilation errors
    while (true)
    {
        runFlowScript(jobSystem, flowscriptText);

        if (!hasCompilationErrors(errorReportPath))
        {
            // Exit the loop if no errors
            break;
        }
    }

    std::cout << "Execution complete!\nDestroying the Job System.\n"
              << std::endl;

    jobSystem.Destroy();

    std::cout << "Job System Destroyed!\nCleaning up files.\n"
              << std::endl;
    // List of files to clean up
    std::vector<std::string> filesToCleanup = {
        "corrected_code.json",
        "correction_history.json",
    };

    // Call cleanup function
    cleanupDataFiles(filesToCleanup);

    return 0;
}
