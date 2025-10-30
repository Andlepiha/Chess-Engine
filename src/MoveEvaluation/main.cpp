#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <thread>

#include "headers/commands.h"
#include "MoveGeneration.h"

constexpr uint16_t TABLE_SIZE = 9;

void print_help(std::vector<std::string> args);

// Struct that is used to call functions from cmd
struct CommandStruct
{
    const char* command;
    void (*commandHandler)(std::vector<std::string> args);
	bool execute_async = false;
} commandTable[TABLE_SIZE] = {
	{ "uci", uci },
	{ "debug", debug },
	{ "isready", isready },
	{ "setoption", setoption },
	{ "register", reg },
	{ "ucinewgame", ucinewgame },
    { "position", position },
	{ "go", go, true },
	{ "stop", stop },
};

std::vector<std::string> split_string(std::string str, std::string delim)
{
    std::vector<std::string> ret_vec;

    size_t pos = 0;
    while((pos = str.find(delim)) != std::string::npos)
    {
        ret_vec.push_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }
    ret_vec.push_back(str);

    return ret_vec;
}

int main() {
    std::thread init_thread1(movgen::init);
	std::thread init_thread2(bitb::init);

	init_thread1.detach();
	init_thread2.detach();
	srand(time(NULL));

    while(true)
    {
        static std::string line;

        //printf(">");
		if (!std::getline(std::cin, line)) {
			// EOF or error occurred, exit the loop
			break;
		}
        std::vector<std::string> split_line = split_string(line, " ");

        // Firs argument is command name, the rest are cmd arguments for a function
        std::string command = split_line[0];
        auto args = std::vector<std::string>(split_line.begin() + 1, split_line.end());

        // Iterate through command aliases and call the corresponding function
        for(int i = 0; i < TABLE_SIZE; i++)
        {
            if(command == commandTable[i].command)
            {
                try {
                    commandTable[i].commandHandler(args);
                } catch (std::exception e) {
                    std::cout << e.what() << std::endl;
                }
                goto LoopEnd;
            }
        }
        // Command was not found (did not break from the loop)
        printf("Command \"%s\" was not found\n", command.c_str());
LoopEnd:
    }

    return 0;
}

void print_help(std::vector<std::string> args)
{
    if (args.size() == 0)
    {
        printf("Get a grip! You are a man! You don't need any help!\n");
        return;
    }
    printf("Get a grip! You are a man! You don't need any help!\n");
}
