#ifndef COMM_H
#define COMM_H

#include "MoveGeneration.h"
#include "MovgenTypes.h"
#include <vector>


// When suppled with an array of moves, corresponding to a valid game,
// this function makes these moves and updates the saved position to match
// the last move in the array
// Returns an error if finds an impossible move
template<typename InputIterator>
uint16_t _make_moves(InputIterator begin, InputIterator end);

// Wrapper function to call appropriate function template depending
// on the current color to move
static void _generate_moves();

void start_search(std::vector<std::string> args);
void start_perft(std::vector<std::string> args);

static const char* const _squares[]
{
    "h1","g1","f1","e1","d1","c1","b1","a1",
    "h2","g2","f2","e2","d2","c2","b2","a2",
    "h3","g3","f3","e3","d3","c3","b3","a3",
    "h4","g4","f4","e4","d4","c4","b4","a4",
    "h5","g5","f5","e5","d5","c5","b5","a5",
    "h6","g6","f6","e6","d6","c6","b6","a6",
    "h7","g7","f7","e7","d7","c7","b7","a7",
    "h8","g8","f8","e8","d8","c8","b8","a8",
};

static const char* _piece_types[]
{
    "",
    "K",
    "Q",
    "R",
    "B",
    "K",
    "P",
};

static bool _saved_pos_is_null = true;
static movgen::BoardPosition _saved_pos;

static movgen::Move move_arr[MAX_MOVES];
static movgen::Move* arr_end = move_arr;

void uci(std::vector<std::string> args);
void debug(std::vector<std::string> args);
void isready(std::vector<std::string> args);
void setoption(std::vector<std::string> args);
void reg(std::vector<std::string> args);
void ucinewgame(std::vector<std::string> args);
void position(std::vector<std::string> args);
void go(std::vector<std::string> args);
void stop(std::vector<std::string> args);

#endif
