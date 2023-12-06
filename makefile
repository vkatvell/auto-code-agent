compile:
	clang++ -shared -o ./Code/lib/libjob.so -fPIC ./Code/lib/*.cpp -std=c++17 -I/usr/include/nlohmann
	clang++ -g -o app -std=c++17 ./Code/main.cpp ./Code/utils.cpp ./Code/compilejob.cpp ./Code/flowscriptparser.cpp ./Code/customjob.cpp ./Code/parsingjob.cpp ./Code/outputjob.cpp -L./Code/lib -ljob -I/usr/include/nlohmann

libLinux:
	clear
	clang++ -shared -o ./Code/lib/libjob.so -fPIC ./Code/lib/*.cpp -std=c++17 -I/usr/local/Cellar/nlohmann-json/3.11.2/include/nlohmann/json.hpp

buildLinux:
	clear
	clang++ -o app -std=c++17 ./Code/main.cpp ./Code/utils.cpp ./Code/compilejob.cpp ./Code/flowscriptparser.cpp ./Code/customjob.cpp ./Code/parsingjob.cpp ./Code/outputjob.cpp -L./Code/lib -ljob -I/usr/local/Cellar/nlohmann-json/3.11.2/include/nlohmann/json.hpp

### Running on WSL and Checking Memory Leaks
### export LD_LIBRARY_PATH=./Code/lib:$LD_LIBRARY_PATH
### LD_LIBRARY_PATH=./Code/lib valgrind --leak-check=yes ./app