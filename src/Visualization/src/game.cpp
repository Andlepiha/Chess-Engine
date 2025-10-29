#include "../headers/game.h"
#include <thread>
#include <format>
#include <filesystem>
#include "spdlog/spdlog.h"

#include "MoveGeneration.h"
#include "MagicNumbers.h"
#include <iostream>

#include <thread>


Chess::Chess(sf::Vector2u window_size, GameMode mode)
	: Chess(window_size, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", mode)
{ }

Chess::Chess(sf::Vector2u window_size, std::string fen, GameMode mode)
	: fen_string(fen.c_str()),
	window(sf::VideoMode(window_size.x, window_size.y), "Chess"),
	mode(mode)
{
    window.setFramerateLimit(60.0f);

	#ifndef NDEBUG
		std::cout << "Current execution dir: " << std::filesystem::current_path() << std::endl;
	#endif

	if(std::filesystem::exists("Data"))
		this->data_dir = "Data";
	else if(std::filesystem::exists("../Data"))
		this->data_dir = "../Data";
	else
		throw std::runtime_error("Could not find data directory");

	this->board = new Board(window_size, this->data_dir);

    game_icon.loadFromFile(std::format("{}/icon.png", data_dir));
    window.setIcon(game_icon.getSize().x, game_icon.getSize().y, game_icon.getPixelsPtr());
	position = movgen::board_from_fen(fen_string);

	auto move_generation = [this](movgen::BoardPosition position) {
		// Wait for all modules to initialize
		while(!movgen::initialized || !bitb::initialized || !movgen::initialized_magics)
		{
			std::this_thread::sleep_for(5ms);
		}

		position.side_to_move == movgen::Color::WHITE
			? this->arr_end = movgen::generate_all_moves<movgen::Color::WHITE, movgen::GenType::LEGAL>(position, move_arr)
			: this->arr_end = movgen::generate_all_moves<movgen::Color::BLACK, movgen::GenType::LEGAL>(position, move_arr);
	};
	std::thread movegen_thread(move_generation, std::ref(this->position));

	if(mode == GameMode::PlayerVEngine)
	{
		try
		{
			this->engine = new EngineChildProcess;
		}
		catch(std::exception& ex)
		{
			std::cerr << "Failed to initialize the engine, defaulting to PvP mode" << std::endl;
			mode = GameMode::PlayerVPlayer;
			goto noPVE;
		}

		//Flip a coin to determine player side
		std::srand(std::time(NULL));
		// Player is white
		if((std::rand() % 2 + 1) == 1)
			players_turn = true;
		else
		{
			this->board->flip_board();
		}

		if(mode == GameMode::PlayerVEngine && !players_turn)
		{
			std::thread th(std::bind(&Chess::handle_engine_move, this));
			th.detach();
		}
	}

noPVE:
	display();
	movegen_thread.detach();
}

void Chess::loop()
{
	while(window.isOpen())
	{
		sf::Event event;
		while(window.pollEvent(event))
		{
			handle_event(event);
		}

		display();
	}
}

void Chess::handle_engine_move()
{
	std::string engine_move;
	while((engine_move = engine->engine_search(movgen::board_to_fen(position))) == "")
		std::this_thread::sleep_for(2ms);

	//Construct move from string
	bpos from, to;
	unsigned char capture = 0, promotion = 0;
	from = (engine_move[1] - '1') * 8 + (7 - (engine_move[0] - 'a'));

	if (engine_move[2] == 'x')
		to = (engine_move[4] - '1') * 8 + (7 - (engine_move[3] - 'a'));
	else
		to = (engine_move[3] - '1') * 8 + (7 - (engine_move[2] - 'a'));

	auto piece = movgen::get_piece(position, from);

	for(auto move = move_arr; move != arr_end; move++)
	{
		if(move->from == from && move->to == to)
		{
			move_piece(*move);
		}
	}

}

void Chess::handle_event(sf::Event ev)
{
	switch(ev.type)
	{
	case sf::Event::Closed:
		engine->~EngineChildProcess();
		window.close();
		break;

    case sf::Event::KeyReleased:
        switch (ev.key.code)
        {
        case sf::Keyboard::F:
            board->flip_board();
            break;
        case sf::Keyboard::Left:
            undo_move();
            break;
        default:
            break;
        }
        break;

	case sf::Event::MouseButtonPressed:
		switch(ev.mouseButton.button)
		{
		case sf::Mouse::Left:
			this->handle_left_button_press();
			break;
		case sf::Mouse::Right:
			board->deselect_square();
			selected_piece_moves.clear();
			break;

		default:
			break;
		}
		break;

	case sf::Event::Resized: {
		this->handle_resized_event(ev.size);
		break;
	}

	default:
		break;
	}
}

void Chess::undo_move()
{
	if(!prev_moves.empty())
	{
		movgen::undo_move(&position, prev_moves.top());
		position.side_to_move == movgen::Color::WHITE
			? movgen::generate_all_moves<movgen::Color::WHITE, movgen::GenType::ALL_MOVES>(position, move_arr)
			: movgen::generate_all_moves<movgen::Color::BLACK, movgen::GenType::ALL_MOVES>(position, move_arr);
		arr_end = movgen::get_legal_moves(position, move_arr, arr_end);

		prev_moves.pop();
	}
}

void Chess::handle_left_button_press()
{
	sf::Vector2i mouse_pos = sf::Mouse::getPosition(window);

	if(!board->within_bounds(mouse_pos.x, mouse_pos.y))
		return;
	if(this->mode == GameMode::PlayerVEngine && !players_turn)
		return;
	if(!movgen::initialized || !bitb::initialized || !movgen::initialized_magics)
		return;

	if(board->get_selected_square() != -1 && !selected_piece_moves.empty())
	{
		board->select_square(mouse_pos);
		for(auto move : selected_piece_moves)
			if(move.to == board->get_selected_square())
				move_piece(move);
	}
	else
		board->select_square(mouse_pos);

	update_piece_moves_highlight();
}

void Chess::move_piece(movgen::Move move)
{
	// Empty the move array
	arr_end = move_arr;

	movgen::make_move<movgen::GenType::ALL_MOVES>(&position, move, move_arr);
	auto game_status = movgen::check_game_state(&position, move_arr, arr_end);

	players_turn ^= 1;
	prev_moves.push(move);
	board->deselect_square();

	switch(game_status)
	{
		case movgen::GameStatus::GAME_CONTINUES:
			if(mode == GameMode::PlayerVEngine && !players_turn)
			{
				std::thread th(std::bind(&Chess::handle_engine_move, this));
				th.detach();
			}
			break;
		case movgen::GameStatus::DRAW:
			printf("Draw\n");
			break;
		case movgen::GameStatus::BLACK_WINS:
			printf("Black wins\n");
			break;
		case movgen::GameStatus::WHITE_WINS:
			printf("White wins\n");
			break;
	}
}

void Chess::handle_resized_event(sf::Event::SizeEvent size)
{
	window.setView(sf::View(sf::FloatRect(0, 0, size.width, size.height)));

	int selected_square = board->get_selected_square();
	bool is_flipped = board->is_flipped();

	delete board;
	board = new Board(window.getSize(), data_dir, is_flipped);

	if(selected_square != -1)
		board->select_square(selected_square % 8, selected_square / 8);
}

void Chess::update_piece_moves_highlight()
{
	int selected = board->get_selected_square();
	selected_piece_moves.clear();
	if(selected != -1)
	{
		// Check if there is a piece on that square
		if(position.pieces[static_cast<uint>(movgen::Piece::ALL_PIECES)] & (1ull << selected))
		{
			for(auto move = move_arr; move != arr_end; move++)
			{
				if(move->from == selected)
				{
					selected_piece_moves.push_back(*move);
				}
			}
		}
	}
}

void Chess::display()
{
	window.clear(sf::Color(255, 255, 240));

	board->draw_board(&window, &position);

    if (!selected_piece_moves.empty())
        board->draw_piece_moves(&window, selected_piece_moves);
    if (position.info != nullptr && position.info->checks_num > 0)
		board->draw_check(&window, bitb::pop_lsb(
			position.pieces[
				static_cast<uint>(
					movgen::get_piece_from_type(
						movgen::PieceType::KING,
						position.side_to_move
					)
				)
			]
		));
	if(!selected_piece_moves.empty())
		board->draw_piece_moves(&window, selected_piece_moves);

	window.display();
}

EngineChildProcess::EngineChildProcess()
	: engine_ready(false)
{
	try
	{
		engine_process = bp::child(
				engine_exe_path,
				bp::std_in < engine_in,
				bp::std_out > engine_out);
		engine_process.detach();
	}
	catch(std::exception& ex)
	{
		std::cerr << "Error while creating a child process:\n"
			<<  ex.what() << std::endl;
		throw std::runtime_error("");
	}

	auto check_ready = [this]() {
		std::string ready_message;

		while(true)
		{
			std::getline(engine_out, ready_message);
			if (ready_message == "Initializing...\r")
				continue;
			if (ready_message == "Engine is ready, input a command\r")
			{
				engine_ready = true;
				return;
			}

			//Anything else is an error
			std::cerr << ready_message << std::endl;
			throw std::runtime_error(ready_message);
		}
	};

	std::thread check_ready_th(check_ready);
	check_ready_th.detach();
}

EngineChildProcess::~EngineChildProcess()
{
	engine_process.terminate();
}

std::string EngineChildProcess::engine_search(std::string fen)
{
	if(!this->engine_ready)
		return "";

	std::string engine_output;
	engine_in << "position fen " << fen << std::endl;
	engine_in << "search depth 7" << std::endl;

	spdlog::info("Searching position " + fen);

	std::getline(engine_out, engine_output);
	spdlog::info("Engine output: " + engine_output);

	engine_output = engine_output.substr(0, engine_output.find(':'));

	return engine_output;
}

