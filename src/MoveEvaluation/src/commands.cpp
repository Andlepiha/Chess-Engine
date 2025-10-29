#include "../headers/commands.h"
#include "../headers/search.h"

#include "MoveGeneration.h"
#include "MovgenTypes.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

uint16_t _make_moves(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end)
{
	uint16_t move_count = 0;
	auto iterator = begin;

	while(iterator != end)
	{
		bool made_move = false;

		std::string move_string, prom_string;
		//In case of promotion also provide a piece to promote to
		if(iterator->length() > 4)
		{
			move_string = iterator->substr(0, iterator->find(':'));

			prom_string = iterator->substr(iterator->find(':'));
			// Convert to upper case for comparison
			std::transform(prom_string.begin(), prom_string.end(), prom_string.begin(), ::toupper);
		}
		else
			move_string = iterator->substr(0, iterator->find(':'));

		for(auto* move = move_arr; move != arr_end; move++)
		{
			if((std::string(_squares[move->from]) + (std::string(_squares[move->to]))) == move_string)
			{
				if(prom_string != "" && _piece_types[(uint)move->get_promoted()] != prom_string)
					continue;

				arr_end = movgen::make_move(&_saved_pos, *move, move_arr);
				made_move = true;
				break;
			}
		}

		if(!made_move)
		{
			printf("Could not find move \"%s\", position not set\n", iterator->c_str());
			throw std::runtime_error("Specified move not found");
		}
		iterator++;
		move_count++;
	}

	return move_count;
}

void save_position(std::vector<std::string> args)
{
	if(args.empty())
	{
		std::cout << "Please provide position string as an argument" << std::endl;
		return;
	}

	size_t it = 0;
	std::string fen;
	if(args[0] == "startpos")
	{
		fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
		it++;
	}
	else if(args[0] == "fen")
	{ // Fen string contains spaces so we have to concatenate these strings
		for(it = 1; it < args.size() && args[it] != "moves"; it++)
			fen.append(args[it] + ' ');
	}
	else
	{
		std::cout << "Please use keywords 'startpos' or 'fen' to specify position" << std::endl;
		std::cout << "Position was not saved" << std::endl;
		return;
	}

	try
	{
		_saved_pos = movgen::board_from_fen(fen);
	}
	catch(std::runtime_error e)
	{
		std::cout << "Failed to construct position. Aborting\n" << std::endl;
		throw e;
	}
	_saved_pos_is_null = false;

	// If additional moves were specified
	if(args.size() > it && args[it] == "moves")
	{
		try
		{
			_make_moves(args.begin() + it + 1, args.end());
		}
		catch(std::runtime_error e)
		{
			// Do not set the position
			_saved_pos_is_null = true;
			printf("Error: %s\n", e.what());
		}
	}
	else
		_generate_moves();
	//_saved_pos.print();
}

void start_search(std::vector<std::string> args)
{
	if(_saved_pos_is_null)
	{
		printf("Please initialise the position first\n");
		return;
	}

	if(args.size() > 0)
	{
		if(args[0] == "depth")
		{
			if(args.size() < 2)
				throw std::runtime_error("Please provide a depth value");
			auto best_move = minmax_best(&_saved_pos, move_arr, arr_end, static_cast<uint16_t>(atoi(args[1].c_str())));
			printf("%s: %.1f\n", std::string(std::get<1>(best_move)).c_str(), std::get<0>(best_move));
			//minmax_eval(&_saved_pos, _gen_moves, static_cast<uint16_t>(atoi(args[1].c_str())));
		}
		else if(args[0] == "time")
		{
			if(args.size() < 2)
				throw std::runtime_error("Please provide a time value");
			throw std::logic_error("Not implemented");
		}
		else
		{
			printf("Unknown argument \"%s\"\n", args[0].c_str());
			return;
		}
	}
	else
	{
		printf("Starting infinite search, type \"stop\" to stop\n");

		throw std::logic_error("Not implemented");
	}
}

void _generate_moves()
{
	if(_saved_pos_is_null)
		throw std::runtime_error("Please initialise a position\n");

	if(_saved_pos.side_to_move == movgen::Color::WHITE)
		arr_end = movgen::generate_all_moves<movgen::Color::WHITE, movgen::GenType::LEGAL>(_saved_pos, move_arr);
	else
		arr_end = movgen::generate_all_moves<movgen::Color::BLACK, movgen::GenType::LEGAL>(_saved_pos, move_arr);
}

const std::string fen_strings[]{
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", // Initial position
	"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", // Kiwipete by Peter McKenzie
	"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", // pos 3
	"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", // pos 4
	"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", // Mirrored pos 4
	"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", // pos 5
};

const std::string pos_names[]{
	"Initial positon",
	"Kiwipete",
	"Pos 3",
	"Pos 4",
	"Pos 4 Mirrored",
	"Pos 5"
};

const std::vector<uint64_t> positions_reached[]{
	{20, 400, 8902, 197281, 4865609, 119060324, 3195901860, 84998978956},
	{48, 2039, 97862, 4085603, 193690690, 8031647685},
	{14, 191, 2812, 43238, 674624, 11030083, 178633661, 3009794393},
	{6, 264, 9467, 422333, 15833292, 706045033},
	{6, 264, 9467, 422333, 15833292, 706045033},
	{44, 1486, 62379, 2103487, 89941194},
};
const char* squares[]{
	"h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1", "h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
	"h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3", "h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
	"h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5", "h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
	"h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7", "h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8",
};


uint64_t count_moves(movgen::BoardPosition& initial,
					 movgen::Move* move_arr,
					 movgen::Move* arr_end,
					 unsigned int depth,
					 bool toplevel = false)
{
	if(depth == 1)
	{
		if(toplevel)
			for(auto move = move_arr; move != arr_end; move++)
				printf("%s%s: %u\n", squares[move->from], squares[move->to], 1);
		return arr_end - move_arr;
	}

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

	uint64_t move_count = 0;
	for(auto move = move_arr; move != arr_end; move++)
	{
		new_arr_end = movgen::make_move(&initial, *move, &new_arr[0]);

		if(movgen::check_game_state(&initial, new_arr, new_arr_end) == movgen::GameStatus::GAME_CONTINUES)
		{
			auto counted = count_moves(initial, &new_arr[0], new_arr_end, depth - 1);
			move_count += counted;

			if(toplevel)
				printf("%s%s: %lu\n", squares[move->from], squares[move->to], counted);
		}
		movgen::undo_move(&initial, *move);
	}

	return move_count;
}

void check_position(movgen::BoardPosition initial_position, size_t depth, std::string pos_name = "", unsigned long long expected_reached = 0)
{

	printf("===============%s===============\n", pos_name.c_str());

	movgen::Move cur_move_arr[MAX_MOVES];
	movgen::Move* cur_move_arr_end;

	initial_position.side_to_move == movgen::Color::WHITE
		? cur_move_arr_end = movgen::generate_all_moves<movgen::Color::WHITE,movgen::GenType::LEGAL>(initial_position, cur_move_arr)
		: cur_move_arr_end = movgen::generate_all_moves<movgen::Color::BLACK,movgen::GenType::LEGAL>(initial_position, cur_move_arr);

	uint64_t counted = count_moves(initial_position, cur_move_arr, cur_move_arr_end, depth, true);

	if (expected_reached != 0)
	{
		if(counted != expected_reached)
			printf("Test failed, expected: %llu, actual: %llu\n", expected_reached, counted);
		else
			printf("Test passed\n");
		printf("\n");
	}
	else
		printf("Nodes reached: %llu\n", counted);
}

void start_perft(std::vector<std::string> args)
{
	if(args.size() > 0 && args[0] == "pos")
	{
		if(_saved_pos_is_null)
			throw std::runtime_error("Please initialise a position\n");
		check_position(_saved_pos, std::stoi(args[1]));
	}
	else
	{
		for(int test_index = 0; test_index < std::size(fen_strings) - 1; test_index++)
		{
			const std::string* fen_string = &fen_strings[test_index];
			movgen::BoardPosition initial_position = movgen::board_from_fen(*fen_string);

			check_position(initial_position, 5, pos_names[test_index], positions_reached[test_index][4]);
		}
	}
}
