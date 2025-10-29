#include "../headers/search.h"
#include "../headers/eval.h"
#include "MoveGeneration.h"
#include "MovgenTypes.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
#include <ostream>
#include <fstream>
#include <algorithm>

#define fuzzy_equal(val1, val2) std::abs(val1 - val2) < 0.01

static TranspositionTable _transpostion_table;

void _print_log_tree(std::ostream& out, const std::string& prefix, const _LogTreeNode* node, bool is_last)
{
	out << prefix << (is_last ? "└──" : "├──");
	out << node->data << "\n";

	if(node->children.size() > 0)
	{
		auto current_child = node->children.begin();
		auto last_child = std::prev(node->children.end());

		while(current_child != last_child)
			_print_log_tree(out, prefix + (is_last ? "    " : "│   "), *(current_child++), false);

		_print_log_tree(out, prefix + (is_last ? "    " : "│   "), *last_child, true);
	}
}

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
			//Promotions with two captures are quite rare, so no checks for value of the peices
			return false;
		}
		//Value queen promotions very highly
		return true;
	}
	if (rhs_type == movgen::MoveType::CAPTURE)
	{
		if (lhs_type == movgen::MoveType::CAPTURE)
		{
			// Cast it to int so I can test for equality
			uint16_t lhs_captured_val = (uint16_t)piece_val(movgen::get_piece_type(lhs.get_captured()));
			uint16_t rhs_captured_val = (uint16_t)piece_val(movgen::get_piece_type(rhs.get_captured()));

			if(rhs_captured_val > lhs_captured_val)
				return true;
			if (rhs_captured_val == lhs_captured_val)
				//Pieces are sorted at move generation by value, so I need to test only pawn captures
				if(rhs.piece == movgen::Piece::W_PAWN || rhs.piece == movgen::Piece::B_PAWN)
					return true;

			return false;
		}

		//Value captures quite highly
		return true;
	}

	//No reason to swap
	return false;
}

void sort_moves(movgen::Move* move_arr, movgen::Move* arr_end)
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
}

float minmax_eval(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth = 5)
{

	if(depth == 0)
	{
		if(pos->side_to_move == movgen::Color::WHITE)
			return eval(*pos);
		else
			return -eval(*pos);
	}

	_ROOT_NODE
#if LOG_SEARCH == 1
	_LogTreeNode* _log_node = _root_node.add_child();
#endif

	float best_value;
	if(pos->side_to_move == movgen::Color::WHITE)
		best_value = _minmax<movgen::Color::WHITE>(pos, move_arr, arr_end, depth, -INFINITY, INFINITY _LOG_NODE_ARG);
	else
		best_value = _minmax<movgen::Color::BLACK>(pos, move_arr, arr_end, depth, -INFINITY, INFINITY _LOG_NODE_ARG);

	_PRINT_LOG_TO_FILE

	return best_value;
}

std::tuple<float, movgen::Move> minmax_best(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth)
{
	assert(depth > 0);
	_transpostion_table.increment_age();

	const movgen::Color col = pos->side_to_move;
	_ROOT_NODE

	float alpha = -INFINITY, beta = INFINITY;
	float score;
	float best_value = -INFINITY;
	uint16_t it = 0;

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

// If not debug, choose randomly among the moves with the best eval, else choose the first move
#ifdef DEBUG
	std::vector<uint16_t> best_move_index;
#else
	uint16_t best_move_index;
#endif
#if LOG_SEARCH == 1
	_LogTreeNode* _log_node = &_root_node;
#endif

	sort_moves(move_arr, arr_end);
	for(auto move = move_arr; move != arr_end; move++)
	{
#if LOG_SEARCH
		std::string move_str = std::string(move);
#endif
		new_arr_end = movgen::make_move(pos, *move, &new_arr[0]);

		// Search remaining captures and only then return the score
		if(depth == 1)
			if(pos->side_to_move == movgen::Color::WHITE)
				score = -_minmax_captures<movgen::Color::WHITE>(pos, alpha, beta _LOG_NODE_CHILD_ARG);
			else
				score = -_minmax_captures<movgen::Color::BLACK>(pos, alpha, beta _LOG_NODE_CHILD_ARG);
		else
		{
			if(pos->side_to_move == movgen::Color::WHITE)
				score = -_minmax<movgen::Color::WHITE>(pos, &new_arr[0], new_arr_end, depth - 1, -beta, -alpha _LOG_NODE_CHILD_ARG);
			else
				score = -_minmax<movgen::Color::BLACK>(pos, &new_arr[0], new_arr_end, depth - 1, -beta, -alpha _LOG_NODE_CHILD_ARG);
		}
		movgen::undo_move(pos, *move);

		if(score > best_value)
		{
			best_value = score;

#if LOG_SEARCH == 1
			_root_node.data = std::format("_: {}", score);
#endif
#ifdef DEBUG
			best_move_index.clear();
			best_move_index.push_back(it);
#else
			best_move_index = it;
#endif
			if(score > alpha)
				alpha = score;
		}
#ifdef DEBUG
		else if(fuzzy_equal(score, best_value))
			best_move_index.push_back(it);
#endif
		if(score >= beta)
			break;

		it++;
	}

	_PRINT_LOG_TO_FILE

#ifdef DEBUG
	uint16_t final_index = uint16_t(rand() % best_move_index.size());
	return std::make_tuple(best_value, gen_moves[best_move_index[final_index]]);
#else
	return std::make_tuple(best_value, new_arr[best_move_index]);
#endif
}

std::vector<std::tuple<float, movgen::Move>>
minmax_all(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth)
{
	assert(depth > 0);
	_transpostion_table.increment_age();

	std::vector<std::tuple<float, movgen::Move>> move_eval;
	move_eval.reserve(arr_end - move_arr);

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

	// Trigger the search
	for(auto move = move_arr; move != arr_end; move++)
	{
		float score;

		movgen::make_move(pos, *move, &new_arr[0]);
		if(!eval_if_game_ended(pos, new_arr, new_arr_end, &score))
			score = minmax_eval(pos, &new_arr[0], new_arr_end, depth - 1);
		movgen::undo_move(pos, *move);

		move_eval.push_back(std::make_tuple(score, *move));
	}

	return move_eval;
}

template <movgen::Color col>
float _minmax(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end,
		uint16_t depth, float alpha, float beta _LOG_NODE_ARG_DEF)
{
	assert(col == pos->side_to_move);
	// Opposite color
	constexpr movgen::Color op_col = (col == movgen::Color::WHITE) ? movgen::Color::BLACK : movgen::Color::WHITE;

	if(const _TTRow* tt_entry = _transpostion_table.search(pos->hash->key))
	{
		if(tt_entry->depth >= depth)
		{
			switch(tt_entry->type)
			{
			case EXACT:
				return tt_entry->score;
			case LOWER_BOUND:
				alpha = std::max(alpha, tt_entry->score);
				break;
			case UPPER_BOUND:
				beta = std::min(beta, tt_entry->score);
				break;
			}
			if(alpha >= beta) return tt_entry->score;
		}
	}

	float score;
	if(eval_if_game_ended(pos, move_arr, arr_end, &score))
		return score;
	float best_value = -INFINITY;

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

	movgen::Move best_move;
	sort_moves(move_arr, arr_end);
	for(auto move = move_arr; move != arr_end; move++)
	{
#if LOG_SEARCH
		std::string move_str = std::string(move);
#endif
		if(depth > 1)
		{
			new_arr_end = movgen::make_move(pos, *move, &new_arr[0]);
			score = -_minmax<op_col>(pos, new_arr, arr_end, depth - 1, -beta, -alpha _LOG_NODE_CHILD_ARG);
		}
		else
		{
			movgen::make_move(pos, *move, nullptr);
			// Search remaining captures and only then return the score
			score = -_minmax_captures<op_col>(pos, alpha, beta _LOG_NODE_CHILD_ARG);
		}
		movgen::undo_move(pos, *move);

		if (score > best_value)
		{
			best_move = *move;
			best_value = score;
			if(score > alpha)
				alpha = score;
		}
		if(score >= beta)
			break;
	}

	NodeType nodeType = EXACT;
    if (best_value <= alpha) nodeType = UPPER_BOUND;
    else if (best_value >= beta) nodeType = LOWER_BOUND;
	_transpostion_table.insert(pos->hash->key, best_value, (uint8_t)depth, nodeType, best_move);

	_APPEND_SCORE
	return best_value;
}

// Implement Quiescence Search
// Search only captures, for indefinite depth
template <movgen::Color col>
float _minmax_captures(movgen::BoardPosition* pos, float alpha, float beta _LOG_NODE_ARG_DEF)
{
	assert(col == pos->side_to_move);

	// Opposite color
	constexpr movgen::Color op_col = (col == movgen::Color::WHITE) ? movgen::Color::BLACK : movgen::Color::WHITE;

	movgen::Move new_arr[MAX_MOVES];
	movgen::Move* new_arr_end;

	float score, best_value;

	//If the king is in check, we need to generate all of the moves and find out if the game ended
	if (pos->info->checks_num > 0)
	{
		new_arr_end = movgen::generate_all_moves<col, movgen::GenType::LEGAL>(*pos, &new_arr[0]);

		if(eval_if_game_ended(pos, new_arr, new_arr_end, &score))
			return score;
	}
	//Else, continue as normal

	best_value = score = eval(*pos);
	_APPEND_SCORE

    if(score >= beta)
		return score;
	if(score > alpha)
		alpha = score;

	new_arr_end = movgen::generate_all_moves<col, movgen::GenType::CAPTURES>(*pos, &new_arr[0]);
	new_arr_end = movgen::get_legal_moves(*pos, &new_arr[0], new_arr_end);

	sort_moves(&new_arr[0], new_arr_end);
	for(auto move = new_arr; move != new_arr; move++)
	{
#if LOG_SEARCH
		std::string move_str = std::string(move);
#endif
		movgen::make_move(pos, *move, nullptr);
		score = -_minmax_captures<op_col>(pos, -beta, -alpha _LOG_NODE_CHILD_ARG);
		movgen::undo_move(pos, *move);

        if(score >= beta)
			return score;
		if(score > best_value)
			best_value = score;
		if(score > alpha)
			alpha = score;
	}

	return best_value;
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
