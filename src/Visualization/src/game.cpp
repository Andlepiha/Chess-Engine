#include "../headers/game.h"
#include <thread>
#include <format>
#include <filesystem>

#include "MoveGeneration.h"
#include "MagicNumbers.h"

#include <thread>


Chess::Chess(sf::Vector2u window_size, GameMode mode)
	: Chess(window_size, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", mode)
{ }

Chess::Chess(sf::Vector2u window_size, std::string fen, GameMode mode)
	: fen_string(fen.c_str()),
	window(sf::VideoMode(window_size.x, window_size.y), "Chess"),
	cur_moves(new std::vector<movgen::Move>),
	mode(mode)
{
    window.setFramerateLimit(60.0f);

	if(std::filesystem::exists("../data"))
		this->data_dir = "../data";
	else
		this->data_dir = "data";

	this->board = new Board(window_size, this->data_dir);

    game_icon.loadFromFile(std::format("{}/icon.png", data_dir));
    window.setIcon(game_icon.getSize().x, game_icon.getSize().y, game_icon.getPixelsPtr());
	position = movgen::board_from_fen(fen_string);

	std::vector<movgen::Move>* all_moves = new std::vector<movgen::Move>;
	std::vector<movgen::Move>** legal_moves = &this->cur_moves;

	auto move_generation = [all_moves, legal_moves](movgen::BoardPosition position) {
		// Wait for all modules to initialize
		while(!movgen::initialized || !bitb::initialized || !movgen::initialized_magics)
		{
			std::this_thread::sleep_for(5ms);
		}

		position.side_to_move == movgen::WHITE
			? movgen::generate_all_moves<movgen::WHITE, movgen::GenType::ALL_MOVES>(position, all_moves)
			: movgen::generate_all_moves<movgen::BLACK, movgen::GenType::ALL_MOVES>(position, all_moves);
		**legal_moves = movgen::get_legal_moves(position, *all_moves);
	};
	std::thread movegen_thread(move_generation, std::ref(this->position));

	if(mode == GameMode::PlayerVEngine)
	{
		this->engine = new EngineChildProcess();

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
	{
		to = (engine_move[4] - '1') * 8 + (7 - (engine_move[3] - 'a'));
		capture = movgen::get_piece(position, to);
	}
	else
		to = (engine_move[3] - '1') * 8 + (7 - (engine_move[2] - 'a'));

	auto piece = movgen::get_piece(position, from);

	//Last character is promotion specifier
	if(!std::isdigit(engine_move.back()))
		promotion = std::distance(
				std::begin(movgen::Move::piece_str),
				std::find(
					std::begin(movgen::Move::piece_str),
					std::end(movgen::Move::piece_str),
					engine_move.back()
			)
		);

	unsigned char castling = 0;
	if (movgen::get_piece_type(piece) == movgen::KING)
	{
		if(to - from == 2)
			castling = 1; // Short castle
		else if(from - to == 3)
			castling = 2; // Long castle
	}

	movgen::Move final_move(
			piece,
			from,
			to,
			capture,
			promotion
	);
	move_piece(final_move);
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
            reset_move();
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

void Chess::reset_move()
{
	if(!prev_moves.empty())
	{
		movgen::undo_move(&position, prev_moves.top());
		position.side_to_move == movgen::WHITE
			? movgen::generate_all_moves<movgen::WHITE, movgen::GenType::ALL_MOVES>(position, cur_moves)
			: movgen::generate_all_moves<movgen::BLACK, movgen::GenType::ALL_MOVES>(position, cur_moves);
		*cur_moves = movgen::get_legal_moves(position, *cur_moves);

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
	movgen::make_move<movgen::GenType::ALL_MOVES>(&position, move, cur_moves);
	auto game_status = movgen::check_game_state(&position, *cur_moves);

	players_turn ^= 1;
	prev_moves.push(move);
	board->deselect_square();

	switch(game_status)
	{
		case movgen::GAME_CONTINUES:
			if(mode == GameMode::PlayerVEngine && !players_turn)
			{
				std::thread th(std::bind(&Chess::handle_engine_move, this));
				th.detach();
			}
			break;
		case movgen::DRAW:
			printf("Draw\n");
			break;
		case movgen::BLACK_WINS:
			printf("Black wins\n");
			break;
		case movgen::WHITE_WINS:
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
		if(position.pieces[movgen::ALL_PIECES] & (1ull << selected))
		{
			for(auto& move : *cur_moves)
			{
				if(move.from == selected)
				{
					selected_piece_moves.push_back(move);
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
        board->draw_check(&window, bitb::pop_lsb(position.pieces[movgen::get_piece_from_type(movgen::KING, position.side_to_move)]));
	if(!selected_piece_moves.empty())
		board->draw_piece_moves(&window, selected_piece_moves);

	window.display();
}

EngineChildProcess::EngineChildProcess()
	: engine_ready(false)
{
	engine_process = bp::child(
		engine_exe_path,
		bp::std_in < engine_in,
		bp::std_out > engine_out,
		::boost::process::windows::show);
	engine_process.detach();

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
	engine_in << "search depth 6" << std::endl;

	std::getline(engine_out, engine_output);
	engine_output = engine_output.substr(0, engine_output.find(':'));

	return engine_output;
}

