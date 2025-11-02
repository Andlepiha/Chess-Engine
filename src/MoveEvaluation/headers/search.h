#include "MovgenTypes.h"
#include <cstdint>
#include <stop_token>
#include <chrono>

namespace ch = std::chrono;

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

extern std::atomic<size_t> node_count;
extern std::atomic<bool> stop_search_request;
extern std::atomic<bool> search_running;
extern ch::time_point<ch::steady_clock> start_time;

enum class StopCond
{
	DEPTH,
	NODES,
	TIME,
	INFINITE,
	MATE
};

// Return best move adn it's eval
std::tuple<float, movgen::Move> minmax_best(
		movgen::BoardPosition* pos, movgen::Move* move_arr,
		movgen::Move* arr_end, StopCond cond, size_t cond_arg);

// Return all moves and their evals
std::vector<std::tuple<float, movgen::Move>> minmax_all(
		movgen::BoardPosition* pos, movgen::Move* move_arr,
		movgen::Move* arr_end, uint16_t depth);

template <movgen::Color col>
static std::tuple<float, movgen::Move> _minmax(
		std::stop_token stoken,
		movgen::BoardPosition* pos, uint16_t depth,
		float alpha, float beta);

template <movgen::Color col>
static float _minmax_captures(
		std::stop_token stoken,
		movgen::BoardPosition* pos, float alpha,
		float beta);

// Return eval, if the game ended
constexpr bool eval_if_game_ended(movgen::GameStatus status, float* eval);
bool eval_if_game_ended(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end, float* eval);
