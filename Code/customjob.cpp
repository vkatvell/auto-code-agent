#include "customjob.h"
#include <iostream>
#include <string>
#include <array>

CustomJob::CustomJob(nlohmann::json input) : m_compileJobInput(input)
{
    this->SetInput(input);
}

void CustomJob::Execute()
{
    std::array<char, 128> buffer;

    if (!GetInput().contains("command"))
    {
        std::cout << "Custom Job: Missing 'command' in input JSON" << std::endl;
        return;
    }
    else if (GetInput().contains("error"))
    {
        std::cout << "Custom Job: Error in input JSON, 'bad input'" << std::endl;
        return;
    }

    std::string command = GetInput()["command"];

    // Redirect cerr to cout | capture errors and send to cout:
    command.append(" 2>&1");

    // Open pipe and run command:
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
    {
        std::cout << "popen Failed: Failed to open pipe" << std::endl;
        return;
    }

    // Read till end of process
    while (fgets(buffer.data(), 128, pipe) != nullptr)
    {
        this->output.append(buffer.data());
    }

    nlohmann::json jsonOutput;
    jsonOutput["status"] = "completed";
    jsonOutput["output"] = output;

    // Set output JSON
    this->SetOutput(jsonOutput);

    // Close pipe and get return code
    this->returnCode = pclose(pipe);
}

void CustomJob::JobCompleteCallback()
{
    std::cout << "Custom Job " << this->GetUniqueID() << " has been completed, the output is:" << std::endl;
    std::string jsonOuput = this->GetOutput().dump(4);
    std::cout << jsonOuput << std::endl;
}