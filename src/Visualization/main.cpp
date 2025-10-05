#include <SFML/Graphics.hpp>

#include "Bitboard.h"
#include "MoveGeneration.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "headers/game.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <string>

/// @brief Catch and process all thrown exceptions, that bubble past main
void terminate_func(std::exception_ptr eptr);

int main()
{
    std::thread init_thread1(movgen::init);
    std::thread init_thread2(bitb::init);

    init_thread1.detach();
    init_thread2.detach();

#ifndef NDEBUG
	auto new_logger = spdlog::basic_logger_mt("new_default_logger", "logs/runtime-engine-log.txt", true);
	spdlog::set_default_logger(new_logger);
#endif

    std::exception_ptr eptr;
    try
    {
        Chess chess({ 1000, 1000 }, GameMode::PlayerVEngine);
        chess.loop();
    }
    catch (...)
    {
        eptr = std::current_exception();
    }
    terminate_func(eptr);

    return 0;
}

void terminate_func(std::exception_ptr eptr)
{
    std::cerr << "Program was terminated\n";
    try
    {
        std::rethrow_exception(eptr);
    }
	catch (const std::runtime_error &ex)
	{
		std::cerr << "Execution failed due to the following error: \n";
		std::cerr << ex.what() << std::endl;
	}
    catch (const std::exception &ex)
    {
        std::cerr << "The following exception was not caught: " << typeid(ex).name() << std::endl;
        std::cerr << "Exception details: " << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << typeid(std::current_exception()).name() << std::endl;
        std::cerr << " ...something, not an exception, dunno what." << std::endl;
    }

 #ifdef _WIN32
    MessageBox(NULL, "Failed due to an exception", "Error", MB_OK | MB_ICONERROR);
#else
#endif

	std::cerr << "errno: " << errno << ": " << std::strerror(errno) << std::endl;
    std::abort();
}
