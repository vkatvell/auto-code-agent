#ifndef UTILS_H
#define UTILS_H

#include <string>
#include "./lib/jobsystemapi.h"
#include "nlohmann/json.hpp"

void registerAndQueueJobs(JobSystemAPI *jobSystem, nlohmann::json &flowscriptJobOutput);
bool hasCompilationErrors(const std::string &errorReportPath);
void runFlowScript(JobSystemAPI &jobSystem, const std::string &flowscriptText);
bool isFileUpdated(const std::string &filePath, const std::time_t &lastModifiedTime);

void cleanupDataFiles(const std::vector<std::string> &fileNames);

#endif // UTILS_H