#include "../include/MovgenTypes.h"
#include "../include/Zobrist.h"
#include <cassert>
#include <iostream>

movgen::BoardHash::BoardHash(BoardPosition& pos)
	: key(0)
	, prev(nullptr)
{ }

movgen::BoardHash::BoardHash(BoardHash*& prev)
{
	this->castling_rights = prev->castling_rights;
	this->en_passant = prev->en_passant;
	this->key = prev->key;
	this->prev = prev;
}

bool movgen::BoardHash::operator==(const movgen::BoardHash& other) const
{
	return this->key == other.key;
}

movgen::BoardPosition::~BoardPosition()
{
	if(this->info == nullptr)
		return;

	this->info->pins.clear();
	this->info->pins.shrink_to_fit();
	delete this->info;
}

movgen::BoardPosition movgen::board_from_fen(std::string fen)
{
	static std::regex fen_regex(movgen::fen_regex_string);
	if(!std::regex_match(fen, fen_regex))
	{
		throw std::runtime_error("Invalid fen string");
	}

	movgen::BoardPosition return_pos = {};

	for(unsigned int i = 0; i < static_cast<uint>(Piece::PIECE_NB); i++)
		return_pos.pieces[i] = 0;
	for(unsigned int i = 0; i < 64; i++)
		return_pos.squares[i] = Piece::NO_PIECE;

	int it = 0;
	for(int i = 0; i < 8; i++)
	{
		for(int j = 0; j < 8; j++)
		{
			if(isdigit(fen[it]))
			{
				j += static_cast<int>(fen[it] - '0') - 1;
				it++;

				if(j > 8)
					throw std::runtime_error("Invalid fen");

				continue;
			}

			switch(fen[it])
			{
			case 'P':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_PAWN)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_PAWN;
				break;
			case 'p':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_PAWN)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_PAWN;
				break;
			case 'N':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_KNIGHT)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_KNIGHT;
				break;
			case 'n':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_KNIGHT)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_KNIGHT;
				break;
			case 'B':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_BISHOP)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_BISHOP;
				break;
			case 'b':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_BISHOP)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_BISHOP;
				break;
			case 'R':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_ROOK)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_ROOK;
				break;
			case 'r':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_ROOK)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_ROOK;
				break;
			case 'Q':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_QUEEN)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_QUEEN;
				break;
			case 'q':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_QUEEN)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_QUEEN;
				break;
			case 'K':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::W_KING)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::W_KING;
				break;
			case 'k':
				bitb::set_bit(&return_pos.pieces[static_cast<uint>(Piece::B_KING)], 63 - (i * 8 + j));
				return_pos.squares[63 - (i * 8 + j)] = Piece::B_KING;
				break;

				// This is forward slash(/) this shid does not work with char def
			case '/':
				if(j != 0)
				{
					throw std::runtime_error("Invalid fen(while parsing piece information)");
				}
				j--;
				break;
			}
			it++;
		}
	}

	return_pos.hash = new movgen::BoardHash(return_pos);

	// Bitboard, representing 1st and last rows
	constexpr bitboard top_and_bottom = bitb::Rank[0] | bitb::Rank[7];
	// Test that no pawns are on the first and last rows
	if(return_pos.pieces[static_cast<uint>(Piece::W_PAWN)] & top_and_bottom ||
	   return_pos.pieces[static_cast<uint>(Piece::B_PAWN)] & top_and_bottom)
	{
		throw std::runtime_error("Impossible position");
	}

	// Assign composite bitboards
	return_pos.pieces[(uint)Piece::BLACK_PIECES] =
		return_pos.pieces[(uint)Piece::B_KING] | return_pos.pieces[(uint)Piece::B_QUEEN] |
		return_pos.pieces[(uint)Piece::B_ROOK] | return_pos.pieces[(uint)Piece::B_BISHOP] |
		return_pos.pieces[(uint)Piece::B_KNIGHT] | return_pos.pieces[(uint)Piece::B_PAWN];
	return_pos.pieces[(uint)Piece::WHITE_PIECES] =
		return_pos.pieces[(uint)Piece::W_KING] | return_pos.pieces[(uint)Piece::W_QUEEN] |
		return_pos.pieces[(uint)Piece::W_ROOK] | return_pos.pieces[(uint)Piece::W_BISHOP] |
		return_pos.pieces[(uint)Piece::W_KNIGHT] | return_pos.pieces[(uint)Piece::W_PAWN];
	return_pos.pieces[(uint)Piece::ALL_PIECES] =
		return_pos.pieces[(uint)Piece::BLACK_PIECES] | return_pos.pieces[(uint)Piece::WHITE_PIECES];

	// Scip spaces
	while(fen[++it] == ' ')
	{
	};
	return_pos.side_to_move = (movgen::Color)(fen[it] == 'w');

	// Skip spaces
	while(fen[++it] == ' ')
	{
	}

	uint castling = static_cast<uint>(CastlingRights::NO_CASTLING);
	while(fen[it] != ' ')
	{
		switch(fen[it++])
		{
		case 'K':
			castling |= static_cast<uint>(CastlingRights::WHITE_SHORT);
			break;
		case 'Q':
			castling |= static_cast<uint>(CastlingRights::WHITE_LONG);
			break;
		case 'k':
			castling |= static_cast<uint>(CastlingRights::BLACK_SHORT);
			break;
		case 'q':
			castling |= static_cast<uint>(CastlingRights::BLACK_LONG);
			break;
		default:
			break;
		}
	}
	return_pos.hash->castling_rights = castling;

EnPassant:
	while(fen[++it] == ' ')
	{
	}

	if(fen[it] == '-')
	{
		// It is a valid position, but considering, that with correct position
		// a pawn cannot take that en passant
		return_pos.hash->en_passant = 0;
	}
	else
	{
		return_pos.hash->en_passant = ('h' - fen[it]) + (fen[it + 1] - '1') * 8;
		it++;
	}
	// Test that en passant square is on 3rd or 6th row
	// TODO: possible bug here(maybe should sub 1 from return_pos.en_passant)
	if(return_pos.hash->en_passant > 0)
		if(!(bitb::sq_bitb(return_pos.hash->en_passant) & (bitb::Rank[2] | bitb::Rank[5])))
			throw std::runtime_error("Invalid position");

	while(it != (fen.size() - 1) && fen[++it] == ' ')
	{
	}
	// Halfmove and fullmove specifier is optional
	if(it != fen.size() - 1)
	{
		return_pos.hash->ply = fen[it] - '0';
		while(fen[++it] == ' ')
		{
		}
		return_pos.fullmove = fen[it] - '0';
	}
	else
	{
		return_pos.hash->ply = 0;
		return_pos.fullmove = 0;
	}

	return_pos.hash->key = zobrist::hash(return_pos, return_pos.hash);

	return return_pos;
}

std::string movgen::board_to_fen(movgen::BoardPosition& pos)
{
	std::string fen;

	int emptyConsec = 0;
	for(int row = 7; row >= 0; row--)
	{
		for(int column = 7; column >= 0; column--)
		{
			movgen::Piece piece = pos.squares[row * 8 + column];
			movgen::PieceType piece_type = movgen::get_piece_type(piece);

			if(piece == movgen::Piece::NO_PIECE)
			{
				emptyConsec++;
				continue;
			}
			movgen::Color piece_color = movgen::get_piece_color(piece);
			if(emptyConsec != 0)
			{
				fen += std::to_string(emptyConsec);
				emptyConsec = 0;
			}

			fen += piece_color == Color::BLACK ? piece_str.at(piece_type)
											   : std::toupper(piece_str.at(piece_type));
		}
		if(emptyConsec != 0)
		{
			fen += std::to_string(emptyConsec);
			emptyConsec = 0;
		}
		fen += '/';
	}
	// Delete last slash
	fen.pop_back();
	fen += ' ';

	fen += pos.side_to_move == Color::WHITE ? 'w' : 'b';
	fen += ' ';

	if(pos.hash->castling_rights & static_cast<uint>(CastlingRights::WHITE_SHORT))
		fen += 'K';
	if(pos.hash->castling_rights & static_cast<uint>(CastlingRights::WHITE_LONG))
		fen += 'Q';
	if(pos.hash->castling_rights & static_cast<uint>(CastlingRights::BLACK_SHORT))
		fen += 'k';
	if(pos.hash->castling_rights & static_cast<uint>(CastlingRights::BLACK_LONG))
		fen += 'q';

	if(pos.hash->castling_rights == static_cast<uint>(CastlingRights::NO_CASTLING))
		fen += '-';
	fen += ' ';

	if(pos.hash->en_passant != 0)
		fen += squares[pos.hash->en_passant];
	else
		fen += '-';
	fen += ' ';

	fen += std::to_string(pos.hash->ply);
	fen += ' ';
	fen += std::to_string(pos.fullmove);

	return fen;
}

movgen::Piece movgen::get_piece(BoardPosition& b_pos, bpos pos)
{
	return b_pos.squares[pos];
}

movgen::PieceType movgen::get_piece_type(BoardPosition& b_pos, bpos pos)
{
	return movgen::get_piece_type(b_pos.squares[pos]);
}

movgen::Color movgen::get_piece_color(movgen::Piece piece)
{
	auto p_uint = static_cast<uint>(piece);

	assert(p_uint != 0);
	assert(p_uint <= 14);
	return static_cast<movgen::Color>(p_uint >> 3);
}

movgen::Move::Move()
	: Move(movgen::Piece::NO_PIECE, 0, 0)
{
	this->is_null_instance = true;
}

movgen::Move::Move(const Move& other)
	: piece(other.piece)
	, from(other.from)
	, to(other.to)
	, move_data(other.move_data)
{ }

movgen::Move::Move(Piece piece, bpos from, bpos to)
	: piece(piece)
	, from(from)
	, to(to)
	, move_data(0)
{ }

movgen::Move::Move(
	Piece piece, bpos from, bpos to, Piece capture, Piece promotion, bool double_move, bool en_passant, Castling castling)
	: piece(piece)
	, from(from)
	, to(to)
	, move_data(0)
{
	this->move_data |= (static_cast<uint>(capture) & 0x0F);
	this->move_data |= (static_cast<uint>(promotion) & 0x0F) << 4;
	this->move_data |= double_move << 8;
	this->move_data |= en_passant << 9;
	this->move_data |= static_cast<uint>(castling) << 10;
}

bool movgen::Move::is_quiet() const
{
	return this->move_data == 0;
}

movgen::MoveType movgen::Move::get_type() const
{
	assert(!is_null_instance);

	if(this->move_data == 0)
		return MoveType::REGULAR;
	else if(this->move_data & 0x200)
		return MoveType::EN_PASSANT;
	else if(this->move_data & 0x0F)
		return this->move_data & 0xF0 ? MoveType::PROMOTION_CAPTURE : MoveType::CAPTURE;
	else if(this->move_data & 0xF0)
		return MoveType::PROMOTION;
	else if(this->move_data & 0x100)
		return MoveType::DOUBLE_MOVE;
	else
		return MoveType::CASTLING;
}

movgen::Piece movgen::Move::get_captured() const
{
	assert(!is_null_instance);

	// En-passant
	if(this->move_data & 0x200)
		return movgen::get_piece_from_type(PieceType::PAWN,
										   static_cast<movgen::Color>(!(bool)movgen::get_piece_color(this->piece)));
	else
		return static_cast<movgen::Piece>(this->move_data & 0x0F);
}

movgen::Piece movgen::Move::get_promoted() const
{
	assert(!is_null_instance);

	return static_cast<movgen::Piece>((this->move_data >> 4) & 0x0F);
}

movgen::Castling movgen::Move::get_castling() const
{
	assert(!is_null_instance);

 	return static_cast<Castling>((this->move_data >> 10) & 0x3);
}

movgen::Move movgen::Move::return_null()
{
	Move instance(movgen::Piece::NO_PIECE, 0, 0);
	instance.is_null_instance = true;

	return instance;
}

movgen::Move::operator std::string() const
{
	if(this->is_null_instance)
		return "none";

	std::string move_str = squares[from];
	move_str += squares[to];

	if(this->get_promoted() != Piece::NO_PIECE)
		move_str += piece_str.at(get_piece_type(this->get_promoted()));

	return move_str;
}

bool movgen::Move::operator ==(const movgen::Move& other) const
{
	return  (this->piece == other.piece) 	&&
			(this->from == other.from) 		&&
			(this->to == other.to) 			&&
			(this->move_data == other.move_data);
}

void movgen::BoardPosition::print()
{
	const char* pieces[]{
		" ", //NO_PIECE
		"k", //B_KING
		"q", //B_QUEEN
		"r", //B_ROOK
		"b", //B_BISHOP
		"n", //B_KNIGHT
		"p", //B_PAWN
		"",
		"",
		"K", //W_KING
		"Q", //W_QUEEN
		"R", //W_ROOK
		"B", //W_BISHOP
		"N", //W_KNIGHT
		"P", //W_PAWN
	};
	std::string ret_string;

	ret_string += "=================================\n";
	for(int r = 7; r >= 0; r--)
	{
		for(int c = 7; c >= 0; c--)
		{
			ret_string += "| ";
			ret_string += pieces[(uint16_t)this->squares[r * 8 + c]];
			ret_string += " ";
		}
		if(r != 0)
			ret_string += "|\n|-------------------------------|\n";
	}
	ret_string += "|\n=================================";

	std::cout << ret_string << std::endl;
}
