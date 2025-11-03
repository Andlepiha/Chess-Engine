#ifndef TYPES_H
#define TYPES_H

#include "Bitboard.h"
#include <cstdint>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

// Max stored moves for current position
#define MAX_MOVES 256

namespace movgen
{

enum class PieceType
{
	NO_PIECE_TYPE,
	KING,
	QUEEN,
	ROOK,
	BISHOP,
	KNIGHT,
	PAWN,
	ANY,

	PIECE_TYPE_NB,
};

enum class Piece
{
	NO_PIECE,
	B_KING = static_cast<int>(PieceType::KING),
	B_QUEEN,
	B_ROOK,
	B_BISHOP,
	B_KNIGHT,
	B_PAWN,
	W_KING = static_cast<int>(PieceType::KING) + 8,
	W_QUEEN,
	W_ROOK,
	W_BISHOP,
	W_KNIGHT,
	W_PAWN,

	BLACK_PIECES = 7,
	WHITE_PIECES = BLACK_PIECES + 8,
	ALL_PIECES = 0,
	PIECE_NB = 17
};

enum class Color
{
	BLACK,
	WHITE
};

enum class GameStatus
{
	GAME_CONTINUES,
	DRAW,
	WHITE_WINS,
	BLACK_WINS
};

enum class Castling
{
	NO_CASTLING = 0,
	SHORT_CASTLE = 1,
	LONG_CASTLE = 2
};

enum class CastlingRights
{
	NO_CASTLING = 0,
	WHITE_SHORT = 0b0001,
	WHITE_LONG = 0b0010,
	BLACK_SHORT = 0b0100,
	BLACK_LONG = 0b1000,

	SHORT = WHITE_SHORT | BLACK_SHORT,
	LONG = WHITE_LONG | BLACK_LONG,
	WHITE_CASTLE = WHITE_SHORT | WHITE_LONG,
	BLACK_CASTLE = BLACK_SHORT | BLACK_LONG,
	ALL_CASTLE = WHITE_CASTLE | BLACK_CASTLE,

	CASTLING_NB
};

enum class MoveType
{
	REGULAR,
	CAPTURE,
	PROMOTION,
	PROMOTION_CAPTURE,
	EN_PASSANT,
	DOUBLE_MOVE,
	CASTLING
};

enum class GenType
{
	ALL_MOVES,
	LEGAL,
	QUIETS,
	CAPTURES,
	PROMOTIONS,
	CASTLING
};

struct PositionInfo
{
	struct Pin
	{
		bpos pinned;
		bpos pinner;
		// Represents squares a pinned piece can move to
		bitboard mask;
	};
	std::vector<Pin> pins;
	bitboard pin_board = 0;
	// It is only possible to have one en_passant pin
	bool en_passant_pin = 0;

	bitboard checkers = 0;
	// Squares to block check or capture the checker
	bitboard blockers = 0;
	unsigned int checks_num = 0;

	bitboard w_piece_attacks = 0;
	bitboard w_pawn_attacks = 0;
	bitboard w_king_attacks = 0;

	bitboard b_piece_attacks = 0;
	bitboard b_pawn_attacks = 0;
	bitboard b_king_attacks = 0;
};

class BoardPosition;

class BoardHash
{
public:
	BoardHash(BoardPosition& pos);
	BoardHash(BoardHash*& prev);

	// Data for undoing a move
	int castling_rights;
	bpos en_passant;
	uint32_t ply = 0; // Halfmove

	size_t key;
	BoardHash* prev;

	bool operator==(const BoardHash& other) const;
};

/*
    Bit numeration in bitboards:
    63 62 61 60 59 58 57 56
    ...
    7  6  5  4  3  2  1  0
    */
struct BoardPosition
{
	~BoardPosition();

	bitboard pieces[static_cast<uint>(Piece::PIECE_NB)];
	Piece squares[64];

	// Determines current side to move
	// 0 - White, 1 - Black
	Color side_to_move;
	unsigned int fullmove;
	unsigned int repetiton_num; // For use in check_game_state only. Resets to 0 in undo move

	movgen::BoardHash* hash = new movgen::BoardHash(*this);
	PositionInfo* info = nullptr;

	void print();
};

inline const char* fen_regex_string = "\\s*^(((?:[rnbqkpRNBQKP1-8]+\\/){7})[rnbqkpRNBQKP1-8]+)"
									  "\\s*([b|w])\\s*([K|Q|k|q]{1,4}|-)\\s*(-|[a-h][1-8])\\s*(\\d+\\s\\d+){0,1}\\s*$";

BoardPosition board_from_fen(std::string fen);
std::string board_to_fen(BoardPosition& pos);

static constexpr const char* const squares[]{
	"h1", "g1", "f1", "e1", "d1", "c1", "b1", "a1", "h2", "g2", "f2", "e2", "d2", "c2", "b2", "a2",
	"h3", "g3", "f3", "e3", "d3", "c3", "b3", "a3", "h4", "g4", "f4", "e4", "d4", "c4", "b4", "a4",
	"h5", "g5", "f5", "e5", "d5", "c5", "b5", "a5", "h6", "g6", "f6", "e6", "d6", "c6", "b6", "a6",
	"h7", "g7", "f7", "e7", "d7", "c7", "b7", "a7", "h8", "g8", "f8", "e8", "d8", "c8", "b8", "a8",
};
static const std::unordered_map<PieceType, char> piece_str = {
	{PieceType::NO_PIECE_TYPE, 0},
	{PieceType::KING, 'k'},
	{PieceType::QUEEN, 'q'},
	{PieceType::ROOK, 'r'},
	{PieceType::BISHOP, 'b'},
	{PieceType::KNIGHT, 'n'},
	{PieceType::PAWN, 'p'},
};

class Move
{
public:
	Piece piece;
	bpos from;
	bpos to;

	Move();
	Move(Piece piece, bpos from, bpos to);
	// If you have to set move_data
	Move(Piece piece,
		 bpos from,
		 bpos to,
		 Piece capture,
		 Piece promotion = Piece::NO_PIECE,
		 bool double_move = false,
		 bool en_passant = false,
		 Castling castling = Castling::NO_CASTLING);
	Move(const Move& other);

	bool is_quiet() const;
	MoveType get_type() const;
	Piece get_captured() const;
	Piece get_promoted() const;
	Castling get_castling() const;

	operator std::string() const;
	bool operator ==(const movgen::Move& other) const;

	bool is_null_instance = false;
	static Move return_null();

private:
	// Stores addition data about the move: piece captured(if any), promotion(if
	// any), etc... Data stored in order from LSb to MSb: Capture(4 bits):
	//      0 -- No capture
	//      1, 9 -- invalid
	//      2 to 6 in order: B_QUEEN , B_ROOK, B_BISHOP, B_KNIGHT, B_PAWN
	//      8 and 9 -- unused
	//      10 to 14 white pieces in the same order
	//      15 -- unknown piece
	// Promotion(4 bits):
	//      0 -- no promotion
	//      1 -- invalid
	//      2 -- queen
	//      3 -- rook
	//      4 -- bishop
	//      5 -- knight
	//      6 -- any
	// Double pawn move(1 bit) -- used for faster detection to set the en passant
	// En passant (1 bit)
	// square Castling(2 bits) -- none, short, long
	//
	// Note: there is no validity check
	uint16_t move_data;
};

Piece get_piece(BoardPosition& b_pos, bpos pos);

constexpr movgen::PieceType get_piece_type(movgen::Piece piece)
{
	switch(piece)
	{
	case movgen::Piece::B_KING:
	case movgen::Piece::W_KING:
		return movgen::PieceType::KING;
	case movgen::Piece::B_QUEEN:
	case movgen::Piece::W_QUEEN:
		return movgen::PieceType::QUEEN;
	case movgen::Piece::B_ROOK:
	case movgen::Piece::W_ROOK:
		return movgen::PieceType::ROOK;
	case movgen::Piece::B_BISHOP:
	case movgen::Piece::W_BISHOP:
		return movgen::PieceType::BISHOP;
	case movgen::Piece::B_KNIGHT:
	case movgen::Piece::W_KNIGHT:
		return movgen::PieceType::KNIGHT;
	case movgen::Piece::B_PAWN:
	case movgen::Piece::W_PAWN:
		return movgen::PieceType::PAWN;
	default:
		return movgen::PieceType::NO_PIECE_TYPE;
	}
}

PieceType get_piece_type(BoardPosition& b_pos, bpos pos);

movgen::Color get_piece_color(movgen::Piece piece);

constexpr movgen::Piece get_piece_from_type(movgen::PieceType type, movgen::Color c)
{
	switch(type)
	{
	case movgen::PieceType::KING:
		return c == movgen::Color::WHITE ? movgen::Piece::W_KING : movgen::Piece::B_KING;
	case movgen::PieceType::QUEEN:
		return c == movgen::Color::WHITE ? movgen::Piece::W_QUEEN : movgen::Piece::B_QUEEN;
	case movgen::PieceType::ROOK:
		return c == movgen::Color::WHITE ? movgen::Piece::W_ROOK : movgen::Piece::B_ROOK;
	case movgen::PieceType::BISHOP:
		return c == movgen::Color::WHITE ? movgen::Piece::W_BISHOP : movgen::Piece::B_BISHOP;
	case movgen::PieceType::KNIGHT:
		return c == movgen::Color::WHITE ? movgen::Piece::W_KNIGHT : movgen::Piece::B_KNIGHT;
	case movgen::PieceType::PAWN:
		return c == movgen::Color::WHITE ? movgen::Piece::W_PAWN : movgen::Piece::B_PAWN;
	default:
		return movgen::Piece::NO_PIECE;
	}
}
}; // namespace movgen

template <>
struct std::hash<movgen::BoardHash>
{
	size_t operator()(movgen::BoardHash const& p) const noexcept;
};

// Define a hash function for BoardPosition
template <>
struct std::hash<movgen::BoardPosition>
{
	size_t operator()(movgen::BoardPosition const& p) const noexcept;
};

#endif
