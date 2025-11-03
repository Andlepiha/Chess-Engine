#include "../headers/search.h"
#include "../headers/eval.h"
#include "MoveGeneration.h"
#include "MovgenTypes.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <future>
#include <stop_token>
#include <tuple>
#include <vector>
#include <algorithm>
#include <thread>
#include <math.h>

#define fuzzy_equal(val1, val2) std::abs(val1 - val2) < 0.01

std::atomic<size_t> node_count = 0;
std::atomic<bool> stop_search_request = false;
std::atomic<bool> search_running = false;
ch::time_point<ch::steady_clock> start_time;

std::atomic<size_t> first_move_cutoffs;

static TranspositionTable _transpostion_table;

// When adding a new move to the table, I shift previous moves to the right
// thus size+1 is a padding to ensure that the write does not get out of bounds
static movgen::Move (*killer_table)[KILLER_MOVE_TABLE_SIZE + 1] =
	new movgen::Move[KILLER_MOVE_TABLE_MAX_PLY][KILLER_MOVE_TABLE_SIZE + 1];

void TranspositionTable::increment_age()
{
	this->current_age = std::min((uint8_t)254, this->current_age); // Cap at 254
}

const _TTRow* TranspositionTable::search(uint64_t hash)
{
	const _TTRow* entry = &this->table[hash % this->hashmask];
	return entry->hash == hash ? entry : nullptr;
}

void TranspositionTable::insert(uint64_t hash, float score, uint8_t depth, NodeType type, movgen::Move best_move)
{
	_TTRow& entry = this->table[hash % hashmask];

	// Replace the entry if
	// A: table at this postion is empty
	// B: entry is from an older search
	// C: Current depth is higher
	// D: New score is exact and the old one is not
	if(entry.hash == 0 ||
		entry.node_age < current_age ||
		depth >= entry.depth ||
		(type == EXACT && entry.type != EXACT))
	{
		entry.hash = hash;
		entry.score = score;
		entry.depth = depth;
		entry.node_age = current_age;
		entry.type = type;
		entry.best_move = best_move;
	}
}

bool cmp_moves(movgen::Move lhs, movgen::Move rhs)
{
	auto lhs_type = lhs.get_type();
	auto rhs_type = rhs.get_type();

	if (rhs_type == movgen::MoveType::PROMOTION || rhs_type == movgen::MoveType::PROMOTION_CAPTURE)
	{
		//Put underpromotions at the bottom as they are very rare
		if(movgen::get_piece_type(rhs.get_promoted()) != movgen::PieceType::QUEEN)
			return false;

		if (lhs_type == movgen::MoveType::PROMOTION || lhs_type == movgen::MoveType::PROMOTION_CAPTURE)
		{
			//Put underpromotions at the bottom as they are very rare
			if(movgen::get_piece_type(lhs.get_promoted()) != movgen::PieceType::QUEEN)
				return true;
			// Both moves are promotions, value captures more
			if(lhs_type != movgen::MoveType::PROMOTION_CAPTURE && rhs_type == movgen::MoveType::PROMOTION_CAPTURE)
				return true;

			//Promotions with two possible captures are quite rare, so no checks for value of the peices
			return false;
		}

		//Value queen promotions very highly
		return true;
	}
	if (rhs_type == movgen::MoveType::CAPTURE)
	{
		uint16_t rhs_captured_val = (int16_t)piece_val(movgen::get_piece_type(rhs.get_captured()));
		uint16_t rhs_piece_val = (int16_t)piece_val(movgen::get_piece_type(rhs.piece));

		if (lhs_type == movgen::MoveType::CAPTURE)
		{
			uint16_t lhs_captured_val = (int16_t)piece_val(movgen::get_piece_type(lhs.get_captured()));
			uint16_t lhs_piece_val = (int16_t)piece_val(movgen::get_piece_type(lhs.piece));

			if((rhs_captured_val - rhs_piece_val) > (lhs_captured_val - lhs_piece_val))
				return true;

			return false;
		}

		// Only winning captures are placed on top
		return (rhs_captured_val - rhs_piece_val) > 0;
	}

	//No reason to swap
	return false;
}

void sort_moves(movgen::Move* move_arr, movgen::Move* arr_end, movgen::Move* killer_moves)
{
	//Inserion sort
	for(size_t i = 1; i < arr_end - move_arr; ++i)
	{
		auto key = std::move(move_arr[i]);
		size_t j = i;

		// Shift elements right to make space
		while(j > 0 && cmp_moves(move_arr[j - 1], key))
		{
			move_arr[j] = std::move(move_arr[j - 1]);
			j--;
		}
		move_arr[j] = std::move(key);
	}

	// Insert killer moves after the captures/promotions
	uint insert_pos;
	for(insert_pos = 0; !move_arr[insert_pos].is_quiet(); insert_pos++);

	for (size_t p = 0; p < KILLER_MOVE_TABLE_SIZE; ++p)
    {
        for (size_t i = insert_pos; i < arr_end - move_arr; ++i)
        {
            if (move_arr[i] == killer_moves[p])
            {
                // Rotate range [insert_pos, i] right by 1
                movgen::Move temp = std::move(move_arr[i]);
                for (size_t j = i; j > insert_pos; --j)
                    move_arr[j] = std::move(move_arr[j - 1]);
                move_arr[insert_pos] = std::move(temp);
                ++insert_pos;
                break;
            }
        }
    }
}

std::tuple<float, movgen::Move> minmax_best(
		movgen::BoardPosition* pos, movgen::Move* move_arr,
		movgen::Move* arr_end, StopCond cond, size_t cond_arg)
{
	_transpostion_table.increment_age();

	start_time = ch::steady_clock::now();
	node_count = 0;
	first_move_cutoffs = 0;

	const movgen::Color col = pos->side_to_move;

	uint depth = 1;

	std::tuple<float, movgen::Move> best_move(0.0f,  movgen::Move::return_null());
	search_running = true;

	while(true)
	{
		std::promise<std::tuple<float, movgen::Move>> search_promise;
		auto search_future = search_promise.get_future();

		std::jthread search_thread([&, pos](std::stop_token stoken) mutable {
			if(pos->side_to_move == movgen::Color::WHITE)
			{
				auto result = _minmax<movgen::Color::WHITE>(stoken, pos, depth, -INFINITY, INFINITY);
				search_promise.set_value(result);
			}
			else
			{
				auto result = _minmax<movgen::Color::BLACK>(stoken, pos, depth, -INFINITY, INFINITY);
				search_promise.set_value(result);
			}
		});

		bool search_done = false;
		while(!search_done)
		{
			// Check for conditions every 10ms
			auto status = search_future.wait_for(std::chrono::milliseconds(10));
			if (status == std::future_status::ready) {
				best_move = search_future.get();

				printf("INFO depth %u pv %s, score cp %d nodes %lu fmc %lu\n",
						depth,
						std::string(std::get<movgen::Move>(best_move)).c_str(),
						(int32_t)(std::get<float>(best_move) * 100),
						node_count.load(),
						first_move_cutoffs.load()
					  );

				depth++;
				search_done = true;
			}

			bool cond_satisfied;
			switch(cond)
			{
				case StopCond::DEPTH:
					cond_satisfied = depth > cond_arg;
					break;
				case StopCond::NODES:
					cond_satisfied = node_count > cond_arg;
					break;
				case StopCond::TIME:
					{
						auto search_time = ch::duration_cast<ch::milliseconds>(start_time - ch::steady_clock::now());
						cond_satisfied = search_time.count() > cond_arg;
						break;
					}
				case StopCond::INFINITE:
					// Do not return unless stopped
					cond_satisfied = false;
					break;
				case StopCond::MATE:
					cond_satisfied = std::get<float>(best_move) == INFINITY;
					break;
			}

			if(cond_satisfied || stop_search_request)
			{
				if (search_thread.get_stop_token().stop_possible())
					search_thread.request_stop();
				return best_move;
			}
		}
	}
}

std::vector<std::tuple<float, movgen::Move>> minmax_all(
		movgen::BoardPosition* pos, movgen::Move* move_arr,
		movgen::Move* arr_end, StopCond cond, size_t cond_arg)
{
	_transpostion_table.increment_age();

	start_time = ch::steady_clock::now();
	node_count = 0;

	std::vector<std::tuple<float, movgen::Move>> move_eval;
	move_eval.reserve(arr_end - move_arr);

	uint depth = 5;
	// Trigger the search
	for(auto move = move_arr; move != arr_end; move++)
	{
		float score;

		movgen::make_move(pos, *move);
		if(pos->side_to_move == movgen::Color::WHITE)
			score = std::get<float>(_minmax<movgen::Color::WHITE>(std::stop_token(), pos, depth, -INFINITY, INFINITY));
		else
			score = std::get<float>(_minmax<movgen::Color::BLACK>(std::stop_token(), pos, depth, -INFINITY, INFINITY));
		movgen::undo_move(pos, *move);

		move_eval.push_back(std::make_tuple(score, *move));
	}

	return move_eval;
}

template <movgen::Color col>
std::tuple<float, movgen::Move> _minmax(
		std::stop_token  stoken,
		movgen::BoardPosition* pos, uint16_t depth,
		float alpha, float beta)
{
	assert(col == pos->side_to_move);

	// Opposite color
	constexpr movgen::Color op_col = (col == movgen::Color::WHITE) ? movgen::Color::BLACK : movgen::Color::WHITE;
	float score;

	if(const _TTRow* tt_entry = _transpostion_table.search(pos->hash->key))
	{
		if(tt_entry->depth >= depth)
		{
			switch(tt_entry->type)
			{
			case EXACT:
				return std::make_tuple(tt_entry->score, tt_entry->best_move);
			case LOWER_BOUND:
				alpha = std::max(alpha, tt_entry->score);
				break;
			case UPPER_BOUND:
				beta = std::min(beta, tt_entry->score);
				break;
			}

			if(alpha >= beta)
				return std::make_tuple(tt_entry->score, tt_entry->best_move);
		}

		// Run the search only on the best move
		if (movgen::is_legal(*pos, tt_entry->best_move))
		{
			movgen::make_move(pos,  tt_entry->best_move);

			if (depth > 1)
				score = -std::get<float>(_minmax<op_col>(stoken, pos, depth - 1, -beta, -alpha));
			else
				score = -_minmax_captures<op_col>(stoken, pos, alpha, beta);

			movgen::undo_move(pos, tt_entry->best_move);

			// We can skip move generation
			if(score >= beta)
			{
				first_move_cutoffs++;
				return std::make_tuple(score, tt_entry->best_move);
			}
		}
	}

	float best_score = -INFINITY;

	movgen::Move move_arr[MAX_MOVES];
	movgen::Move* arr_end;

	// This will tecnically seach the best move twise, but I assume this will not impact performance thah much
	arr_end = movgen::generate_all_moves<col, movgen::GenType::LEGAL>(*pos, move_arr);
	sort_moves(move_arr, arr_end, killer_table[pos->hash->ply]);

	movgen::Move best_move;
	for(auto move = move_arr; move != arr_end; move++)
	{
		if(stoken.stop_requested())
			return std::make_tuple(-INFINITY, movgen::Move::return_null());

		node_count++;
		movgen::make_move(pos, *move, nullptr);

		if(depth > 1)
			score = -std::get<float>(_minmax<op_col>(stoken, pos, depth - 1, -beta, -alpha));
		else
			// Search remaining captures and only then return the score
			score = -_minmax_captures<op_col>(stoken, pos, alpha, beta);

		movgen::undo_move(pos, *move);

		if (score > best_score)
		{
			best_move = *move;
			best_score = score;
			if(score > alpha)
				alpha = score;
		}
		if(score >= beta)
		{
			// Add this to the killer moves if this is a quet move
			if(move->is_quiet())
			{
				for(auto i = 0; i < KILLER_MOVE_TABLE_SIZE; i++)
					killer_table[pos->hash->ply][i + 1] = killer_table[pos->hash->ply][i];
				killer_table[pos->hash->ply][0] = *move;
			}
			break;
		}
	}

	// There are no legal moves, the game ended in checkmate or stalemate
	if(best_score == -INFINITY)
	{
		if(pos->info->checks_num > 0)
			return std::make_tuple(-INFINITY, movgen::Move::return_null());
		else
			return std::make_tuple(0.0, movgen::Move::return_null());
	}

	NodeType nodeType = EXACT;
    if (best_score <= alpha) nodeType = UPPER_BOUND;
    else if (best_score >= beta) nodeType = LOWER_BOUND;
	_transpostion_table.insert(pos->hash->key, best_score, (uint8_t)depth, nodeType, best_move);

	return std::make_tuple(best_score, best_move);
}

// Implement Quiescence Search
// Search only captures, for indefinite depth
template <movgen::Color col>
float _minmax_captures(
		std::stop_token stoken,
		movgen::BoardPosition* pos, float alpha,
		float beta)
{
	assert(col == pos->side_to_move);

	// Opposite color
	constexpr movgen::Color op_col = (col == movgen::Color::WHITE) ? movgen::Color::BLACK : movgen::Color::WHITE;

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

	float score, best_score;

	//If the king is in check, we need to generate all of the moves and find out if the game ended
	if (pos->info->checks_num > 0)
	{
		new_arr_end = movgen::generate_all_moves<col, movgen::GenType::LEGAL>(*pos, &new_arr[0]);

		if(eval_if_game_ended(pos, new_arr, new_arr_end, &score))
			return score;
	}
	//Else, continue as normal

	best_score = score = eval(*pos);

    if(score >= beta)
		return score;
	if(score > alpha)
		alpha = score;

	new_arr_end = movgen::generate_all_moves<col, movgen::GenType::LEGAL>(*pos, &new_arr[0]);
	sort_moves(&new_arr[0], new_arr_end, killer_table[pos->hash->ply]);

	for(auto move = new_arr; move != new_arr; move++)
	{
		if(stoken.stop_requested())
			return best_score;

		movgen::make_move(pos, *move, nullptr);
		score = -_minmax_captures<op_col>(stoken, pos, -beta, -alpha);

		node_count++;
		movgen::undo_move(pos, *move);

        if(score >= beta)
			return score;
		if(score > best_score)
			best_score = score;
		if(score > alpha)
			alpha = score;
	}

	return best_score;
}

constexpr bool eval_if_game_ended(movgen::GameStatus status, float* eval)
{
	switch(status)
	{
	case movgen::GameStatus::GAME_CONTINUES:
		return false;
	case movgen::GameStatus::DRAW:
		*eval = 0.0;
		return true;
	case movgen::GameStatus::WHITE_WINS:
		*eval = INFINITY;
		return true;
	case movgen::GameStatus::BLACK_WINS:
		*eval = -INFINITY;
		return true;
	default:
		throw std::logic_error("Unexpected control flow!");
	}
}

bool eval_if_game_ended(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, float* eval)
{
	return eval_if_game_ended(movgen::check_game_state(pos, move_arr, arr_end), eval);
}
