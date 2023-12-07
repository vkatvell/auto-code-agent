#include <iostream>
#include <fstream>
#include <string>
#include "./lib/jobsystemapi.h"
#include "utils.h"
#include "flowscriptparser.h"

int main(void)
{
    // Create an instance of JobSystemAPI
    JobSystemAPI jobSystem;

    // Start the job system
    jobSystem.Start();

    // TODO create a flowscript job that writes a flowscript job from LLM in javascript and writes to file fstest1.dot
    // TODO register that job and queue it and run it. Then queue flowscriptJob

    // Register flowscript parse job type
    std::cout << "Registering custom flowscript parsing job\n"
              << std::endl;
    jobSystem.RegisterJob("flowscriptJob", [&jobSystem]() -> Job *
                          { return new FlowScriptParseJob(&jobSystem); });

    // Read in the file here and create JSON object input
    std::string errorReportPath = "./Data/error_report.json";
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

    // Loop until no compilation errors
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

    return 0;
}
