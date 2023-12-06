#include <fstream>
#include <string>

void readFile(const std::string &filename)
{
    std::ifstream file(filename); if (!file.is_open()) return;
    // Process file...
}