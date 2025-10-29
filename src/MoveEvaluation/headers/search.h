#include "MovgenTypes.h"
#include <cstdint>
#include <unordered_map>
#include <cmath>

// Print search tree to file
#define LOG_SEARCH 0
#if LOG_SEARCH == 1
	#define _ROOT_NODE _LogTreeNode _root_node;
	#define _LOG_NODE_ARG_DEF ,_LogTreeNode* _log_node
	#define _LOG_NODE_CHILD_ARG ,_log_node->add_child(std::string(move))
	#define _LOG_NODE_ARG ,_log_node
	#define _APPEND_SCORE _log_node->data += ": " + std::to_string(best_value);

	#define _PRINT_LOG_TO_FILE std::ofstream log_file("search_log.txt", std::ios_base::app); 	\
		_print_log_tree(log_file, "", &_root_node, true); 											\
		log_file << "\n" << std::endl; 															\
		log_file.close();
#else
	#define _ROOT_NODE
	#define _LOG_NODE_ARG_DEF
	#define _LOG_NODE_CHILD_ARG
	#define _LOG_NODE_ARG
	#define _APPEND_SCORE
	#define _PRINT_LOG_TO_FILE
#endif

struct _LogTreeNode
{
	std::string data;
	std::vector<_LogTreeNode*> children;

	_LogTreeNode* add_child(std::string data = "")
	{
		children.push_back(new _LogTreeNode);
		if(data != "")
			children.back()->data = data;

		return children.back();
	}
};

constexpr size_t TRANSPOSITION_TABLE_SIZE_MB = 500;

enum NodeType
{
	EXACT,
	LOWER_BOUND,
	UPPER_BOUND
};

struct _TTRow
{
	uint64_t hash = 0;
	float score = 0;
	uint8_t depth = 0;
	uint8_t node_age = 0;
	NodeType type = EXACT;
	movgen::Move best_move;
};

class TranspositionTable
{
public:
	TranspositionTable() = default;

	void increment_age();
	const _TTRow* search(uint64_t hash);
	void insert(uint64_t hash, float score, uint8_t depth, NodeType type, movgen::Move best_move);

private:
	static constexpr uint64_t num_entries =
		(TRANSPOSITION_TABLE_SIZE_MB * 1024 * 1024) / sizeof(_TTRow);
	const uint32_t hashmask = num_entries - 1;

	_TTRow* table = new _TTRow[this->num_entries];
	uint8_t current_age = 0;
};

// Returns 1 if moves should be swapped and 0 if they shouldn't
bool cmp_moves(movgen::Move lhs, movgen::Move rhs);
// Sort moves according to cmp_moves function
// Places most likely best moves on top
void sort_moves(movgen::Move* move_arr, movgen::Move* arr_end);

// Return only evaluation
float minmax_eval(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth);
// Return best move adn it's eval
std::tuple<float, movgen::Move>
minmax_best(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth);
// Return all moves and their evals
std::vector<std::tuple<float, movgen::Move>>
minmax_all(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, uint16_t depth);

template <movgen::Color col>
static float
_minmax(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end,
		uint16_t depth, float alpha, float beta _LOG_NODE_ARG_DEF);

template <movgen::Color col>
static float _minmax_captures(movgen::BoardPosition* pos, float alpha, float beta _LOG_NODE_ARG_DEF);

// Return eval, if the game ended
constexpr bool eval_if_game_ended(movgen::GameStatus status, float* eval);
bool eval_if_game_ended(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, float* eval);
