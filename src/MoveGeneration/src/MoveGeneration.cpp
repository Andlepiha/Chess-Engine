#include "../include/MoveGeneration.h"
#include "../include/MagicNumbers.h"
#include "../include/Zobrist.h"
#include <cassert>

#define PIECE_MOVE_INSTANCE(piece, type)                                                                                    \
	template movgen::Move* movgen::generate_piece_moves<piece, type>(                                                       \
		bpos piece_pos, BoardPosition & pos, movgen::Color c, movgen::Move* move_arr)

#define PAWN_MOVE_INSTANCE(piece, type)                                                                                     \
	template movgen::Move* movgen::generate_pawn_moves<piece, type>(BoardPosition & pos, movgen::Move* move_arr)

#define ALL_MOVE_INSTANCE(piece, type)                                                                                      \
	template movgen::Move* movgen::generate_all_moves<piece, type>(BoardPosition & pos, movgen::Move* move_arr)

#define MAKE_MOVE_INSTANCE(type)                                                                                            \
	template movgen::Move* movgen::make_move<type>(                                                                         \
		movgen::BoardPosition * pos, movgen::Move & move, movgen::Move* new_moves)

bitboard movgen::knight_attacks[64];
bitboard movgen::king_attacks[64];
std::atomic_bool movgen::initialized = false;

void movgen::init()
{
	zobrist::init();

	const std::tuple<int, int> king_moves[]{
		{-1, -1},
		{0, -1},
		{1, -1},
		{-1, 0},
		{1, 0},
		{-1, 1},
		{0, 1},
		{1, 1},
	};

	const std::tuple<int, int> knight_moves[]{
		{-1, -2},
		{1, -2},
		{-2, -1},
		{2, -1},
		{-1, 2},
		{1, 2},
		{-2, 1},
		{2, 1},
	};

	for(uint16_t y = 0; y < 8; y++)
	{
		for(uint16_t x = 0; x < 8; x++)
		{
			bitboard king_board = 0;
			bitboard knight_board = 0;

			for(auto offset : king_moves)
			{
				int ky = y + std::get<0>(offset);
				int kx = x + std::get<1>(offset);

				if(ky >= 0 && ky < 8 && kx >= 0 && kx < 8)
					king_board |= 1ull << (ky * 8 + kx);
			}
			for(auto offset : knight_moves)
			{
				int ky = y + std::get<0>(offset);
				int kx = x + std::get<1>(offset);

				if(ky >= 0 && ky < 8 && kx >= 0 && kx < 8)
					knight_board |= 1ull << (ky * 8 + kx);
			}

			king_attacks[y * 8 + x] = king_board;
			knight_attacks[y * 8 + x] = knight_board;
		}
	}

	initialized = true;
}

template <movgen::PieceType type, movgen::GenType gen_type>
movgen::Move* movgen::generate_piece_moves(bpos piece_pos, BoardPosition& pos, movgen::Color c, movgen::Move* move_arr)
{
	const unsigned int us = 8 * (uint)c;
	const unsigned int them = 8 - us;

	if(type == movgen::PieceType::KING)
	{
		if(gen_type == movgen::GenType::CASTLING || gen_type == movgen::GenType::ALL_MOVES)
		{
			const auto short_castle = c == movgen::Color::WHITE ?
				movgen::CastlingRights::WHITE_SHORT : movgen::CastlingRights::BLACK_SHORT;
			const auto long_castle = c == movgen::Color::WHITE ?
				movgen::CastlingRights::WHITE_LONG : movgen::CastlingRights::BLACK_LONG;

			if(pos.hash->castling_rights & static_cast<uint>(short_castle))
				// Check if no pieces are blocking the castling
				if(!(bitb::Between_in[piece_pos][piece_pos - 3] & pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]))
					// Check if there is a rook
					if(pos.pieces[static_cast<uint>(Piece::B_ROOK) + us] & (bitb::sq_rank(piece_pos) & bitb::File[7]))
						*move_arr++ = Move(
								get_piece_from_type(PieceType::KING, c),
								piece_pos, piece_pos - 2,
								Piece::NO_PIECE, Piece::NO_PIECE, false, false, Castling::SHORT_CASTLE);

			if(pos.hash->castling_rights & static_cast<uint>(long_castle))
				// Check if no pieces are blocking the castling
				if(!(bitb::Between_in[piece_pos][piece_pos + 4] & pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]))
					// Check if there is a rook
					if(pos.pieces[static_cast<uint>(Piece::B_ROOK) + us] & (bitb::sq_rank(piece_pos) & bitb::File[0]))
						*move_arr++ = Move(
								get_piece_from_type(PieceType::KING, c),
								piece_pos, piece_pos + 2,
								Piece::NO_PIECE, Piece::NO_PIECE, false, false, Castling::LONG_CASTLE);
		}
		if(gen_type != movgen::GenType::CASTLING)
		{
			bitboard moves = get_pseudo_attacks<PieceType::KING>(piece_pos,
						pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]) &
						~pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + us];

			if(gen_type != movgen::GenType::QUIETS)
			{
				bitboard captures = moves & pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];

				for(auto move_to : bitb::BitscanIterator(captures))
					*move_arr++ = Move(
							get_piece_from_type(PieceType::KING, c),
							piece_pos, move_to,
							get_piece(pos, move_to));
			}
			if(gen_type != movgen::GenType::CAPTURES)
			{
				bitboard quiet = moves & ~pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];

				for(auto move_to : bitb::BitscanIterator(quiet))
					*move_arr++ = Move(
							get_piece_from_type(PieceType::KING, c),
							piece_pos, move_to);
			}
		}
	}
	else
	{
		bitboard moves = get_pseudo_attacks<type>(piece_pos,
				pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]) &
				~pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + us];

		if(gen_type != movgen::GenType::QUIETS)
		{
			bitboard captures = moves & pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];
			for(auto move_to : bitb::BitscanIterator(captures))
				*move_arr++ = Move(
						get_piece_from_type(type, c),
						piece_pos, move_to,
						get_piece(pos, move_to));
		}
		if(gen_type != movgen::GenType::CAPTURES)
		{
			bitboard quiet = moves & ~pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];
			for(auto move_to : bitb::BitscanIterator(quiet))
				*move_arr++ = Move(
						get_piece_from_type(type, c),
						piece_pos, move_to);
		}
	}

	return move_arr;
}

PIECE_MOVE_INSTANCE(movgen::PieceType::QUEEN, movgen::GenType::ALL_MOVES);
PIECE_MOVE_INSTANCE(movgen::PieceType::QUEEN, movgen::GenType::QUIETS);
PIECE_MOVE_INSTANCE(movgen::PieceType::QUEEN, movgen::GenType::CAPTURES);
PIECE_MOVE_INSTANCE(movgen::PieceType::ROOK, movgen::GenType::ALL_MOVES);
PIECE_MOVE_INSTANCE(movgen::PieceType::ROOK, movgen::GenType::QUIETS);
PIECE_MOVE_INSTANCE(movgen::PieceType::ROOK, movgen::GenType::CAPTURES);
PIECE_MOVE_INSTANCE(movgen::PieceType::BISHOP, movgen::GenType::ALL_MOVES);
PIECE_MOVE_INSTANCE(movgen::PieceType::BISHOP, movgen::GenType::QUIETS);
PIECE_MOVE_INSTANCE(movgen::PieceType::BISHOP, movgen::GenType::CAPTURES);
PIECE_MOVE_INSTANCE(movgen::PieceType::KNIGHT, movgen::GenType::ALL_MOVES);
PIECE_MOVE_INSTANCE(movgen::PieceType::KNIGHT, movgen::GenType::QUIETS);
PIECE_MOVE_INSTANCE(movgen::PieceType::KNIGHT, movgen::GenType::CAPTURES);
PIECE_MOVE_INSTANCE(movgen::PieceType::KING, movgen::GenType::ALL_MOVES);
PIECE_MOVE_INSTANCE(movgen::PieceType::KING, movgen::GenType::QUIETS);
PIECE_MOVE_INSTANCE(movgen::PieceType::KING, movgen::GenType::CAPTURES);
PIECE_MOVE_INSTANCE(movgen::PieceType::KING, movgen::GenType::CASTLING);

template <movgen::Color color, movgen::GenType gen_type>
movgen::Move* movgen::generate_pawn_moves(BoardPosition& pos, movgen::Move* move_arr)
{
	constexpr unsigned int us = 8 * static_cast<uint>(color);
	constexpr unsigned int them = 8 - us;
	constexpr bitboard rank3 = (color == Color::WHITE ? bitb::Rank[2] : bitb::Rank[5]);
	constexpr bitboard rank7 = (color == Color::WHITE ? bitb::Rank[6] : bitb::Rank[1]);
	constexpr Piece piece = (color == Color::WHITE ? Piece::W_PAWN : Piece::B_PAWN);

	constexpr bitb::Direction forward = (color == Color::WHITE ? bitb::UP : bitb::DOWN);
	constexpr bitb::Direction back = (color == Color::WHITE ? bitb::DOWN : bitb::UP);
	constexpr bitb::Direction forward_left = static_cast<bitb::Direction>(forward + bitb::LEFT);
	constexpr bitb::Direction forward_right = static_cast<bitb::Direction>(forward + bitb::RIGHT);

	const bitboard prom_pawns = pos.pieces[static_cast<uint>(Piece::B_PAWN) + us] & rank7;
	const bitboard not_prom_pawns = pos.pieces[static_cast<uint>(Piece::B_PAWN) + us] & ~rank7;

	if(gen_type == movgen::GenType::PROMOTIONS || gen_type == movgen::GenType::ALL_MOVES)
	{

		if(prom_pawns)
		{
			bitboard left_capture_prom = bitb::shift<forward_left>(prom_pawns) &
				pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];
			bitboard right_capture_prom = bitb::shift<forward_right>(prom_pawns) &
				pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];
			bitboard push_prom = bitb::shift<forward>(prom_pawns) &
				~pos.pieces[static_cast<uint>(Piece::ALL_PIECES)];

			for(auto move_to : bitb::BitscanIterator(left_capture_prom))
				_make_promotions<color, forward_left>(move_arr, move_to, get_piece(pos, move_to));
			for(auto move_to : bitb::BitscanIterator(right_capture_prom))
				_make_promotions<color, forward_right>(move_arr, move_to, get_piece(pos, move_to));
			for(auto move_to : bitb::BitscanIterator(push_prom))
				_make_promotions<color, forward>(move_arr, move_to, Piece::NO_PIECE);
		}
	}

	if(gen_type == movgen::GenType::CAPTURES || gen_type == movgen::GenType::ALL_MOVES)
	{
		// En passant
		if(pos.hash->en_passant != 0)
		{
			bitboard candidate_pawns = (bitb::shift<bitb::LEFT>(1ull << (pos.hash->en_passant + back)) |
										bitb::shift<bitb::RIGHT>(1ull << (pos.hash->en_passant + back))) &
									   pos.pieces[static_cast<uint>(Piece::B_PAWN) + us];

			for(auto move_from : bitb::BitscanIterator(candidate_pawns))
				*move_arr++ = Move(
					piece, move_from,
					pos.hash->en_passant,
					get_piece_from_type(PieceType::PAWN, color),
					Piece::NO_PIECE, false, true
				);
		}

		// Captures
		bitboard left_capture = bitb::shift<forward_left>(not_prom_pawns) &
			pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];
		bitboard right_capture = bitb::shift<forward_right>(not_prom_pawns) &
			pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];

		for(auto move_to : bitb::BitscanIterator(left_capture))
			*move_arr++ = Move(piece, move_to - forward - bitb::LEFT, move_to, get_piece(pos, move_to));
		for(auto move_to : bitb::BitscanIterator(right_capture))
			*move_arr++ = Move(piece, move_to - forward - bitb::RIGHT, move_to, get_piece(pos, move_to));
	}

	if(gen_type == movgen::GenType::QUIETS || gen_type == movgen::GenType::ALL_MOVES)
	{
		// Single and double moves
		bitboard s = bitb::shift<forward>(not_prom_pawns) & ~pos.pieces[static_cast<uint>(Piece::ALL_PIECES)];
		bitboard d = bitb::shift<forward>(s & rank3) & ~pos.pieces[static_cast<uint>(Piece::ALL_PIECES)];

		for(auto move_to : bitb::BitscanIterator(d))
			*move_arr++ = Move(piece, move_to - forward * 2, move_to, Piece::NO_PIECE, Piece::NO_PIECE, true);
		for(auto move_to : bitb::BitscanIterator(s))
			*move_arr++ = Move(piece, move_to - forward, move_to);
	}

	return move_arr;
}

PAWN_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::ALL_MOVES);
PAWN_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::QUIETS);
PAWN_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::CAPTURES);
PAWN_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::PROMOTIONS);
PAWN_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::ALL_MOVES);
PAWN_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::QUIETS);
PAWN_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::CAPTURES);
PAWN_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::PROMOTIONS);

template <>
bitboard movgen::get_pseudo_attacks<movgen::PieceType::KING>(bpos piece_pos, bitboard blocker)
{
	return king_attacks[piece_pos];
}

template <>
bitboard movgen::get_pseudo_attacks<movgen::PieceType::QUEEN>(bpos piece_pos, bitboard blocker)
{
	return movgen::get_rook_attacks(piece_pos, blocker) | movgen::get_bishop_attacks(piece_pos, blocker);
}

template <>
bitboard movgen::get_pseudo_attacks<movgen::PieceType::ROOK>(bpos piece_pos, bitboard blocker)
{
	return movgen::get_rook_attacks(piece_pos, blocker);
}

template <>
bitboard movgen::get_pseudo_attacks<movgen::PieceType::BISHOP>(bpos piece_pos, bitboard blocker)
{
	return movgen::get_bishop_attacks(piece_pos, blocker);
}

template <>
bitboard movgen::get_pseudo_attacks<movgen::PieceType::KNIGHT>(bpos piece_pos, bitboard blocker)
{
	return knight_attacks[piece_pos];
}

template <movgen::Color them_c>
void movgen::get_checkers(BoardPosition& pos, PositionInfo* info)
{
	constexpr unsigned int them = 8 * static_cast<uint>(them_c);
	constexpr unsigned int us = 8 - them;

	const bpos king_pos = bitb::pop_lsb(pos.pieces[static_cast<uint>(Piece::B_KING) + us]);
	bitboard& piece_attacks = pos.side_to_move == Color::WHITE ? info->b_piece_attacks : info->w_piece_attacks;

	constexpr bitb::Direction back = us == static_cast<uint>(Color::WHITE) ? bitb::DOWN : bitb::UP;

	bitboard king_sliding = get_pseudo_attacks<PieceType::BISHOP>(king_pos, pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]);
	bitboard bishop_checkers = king_sliding & (
			pos.pieces[static_cast<uint>(Piece::B_BISHOP) + them] |
			pos.pieces[static_cast<uint>(Piece::B_QUEEN)  + them]);

	// Prevent the king from going alongside the attacking ray
	for(auto attacker : bitb::BitscanIterator(bishop_checkers))
		piece_attacks |= get_pseudo_attacks<PieceType::BISHOP>(attacker,
				pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^
				pos.pieces[static_cast<uint>(Piece::B_KING) + us]);

	king_sliding = get_pseudo_attacks<PieceType::ROOK>(king_pos,
			pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]);
	bitboard rook_checkers = king_sliding & (
			pos.pieces[static_cast<uint>(Piece::B_ROOK) + them] |
			pos.pieces[static_cast<uint>(Piece::B_QUEEN) + them]);

	// Prevent the king from going alongside the attacking ray
	for(auto attacker : bitb::BitscanIterator(rook_checkers))
		piece_attacks |= get_pseudo_attacks<PieceType::ROOK>(attacker,
				pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^
				pos.pieces[static_cast<uint>(Piece::B_KING) + us]);

	// Check for knights
	bitboard king_jump = get_pseudo_attacks<PieceType::KNIGHT>(king_pos,
			pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]);
	bitboard knight_checkers = king_jump & pos.pieces[static_cast<uint>(Piece::B_KNIGHT) + them];

	info->blockers |= knight_checkers;

	// Check for pawns
	constexpr bitb::Direction forward_left =
		static_cast<bitb::Direction>((them_c == Color::WHITE ? bitb::DOWN : bitb::UP) + bitb::LEFT);
	constexpr bitb::Direction forward_right =
		static_cast<bitb::Direction>((them_c == Color::WHITE ? bitb::DOWN : bitb::UP) + bitb::RIGHT);

	bitboard king_move =
		bitb::shift<forward_left>(pos.pieces[static_cast<uint>(Piece::B_KING) + us]) |
		bitb::shift<forward_right>(pos.pieces[static_cast<uint>(Piece::B_KING) + us]);

	bitboard pawn_checkers = king_move & pos.pieces[static_cast<uint>(Piece::B_PAWN) + them];

	// If it is possible to capture the checking pawn with en_passant, add en passant square to blockers board
	if(pos.hash->en_passant > 0 && (bitb::shift<back>(pawn_checkers) & bitb::sq_bitb(pos.hash->en_passant)))
		bitb::set_bit(&info->blockers, pos.hash->en_passant);

	info->checkers |= bishop_checkers;
	info->checkers |= rook_checkers;
	info->checkers |= knight_checkers;
	info->checkers |= pawn_checkers;

	info->checks_num = bitb::bit_count(info->checkers);

	if(info->checkers > 0)
	{ // Using pop_lsb here because this bitboard should not matter in case of more than 1 checker
		info->blockers |= bitb::Between[king_pos][bitb::pop_lsb(info->checkers)];
		info->blockers |= knight_checkers;
		info->blockers |= pawn_checkers;
	}
}

template <movgen::Color them_c>
void movgen::get_pinners(BoardPosition& pos, PositionInfo* info)
{
	constexpr unsigned int them = 8 * static_cast<uint>(them_c);
	constexpr unsigned int us = 8 - them;
	// Direction in which a pawn advances
	constexpr bitb::Direction forward = them_c == Color::BLACK ? bitb::UP : bitb::DOWN;
	constexpr bitb::Direction back = them_c == Color::BLACK ? bitb::DOWN : bitb::UP;
	constexpr bitb::Direction back_left = static_cast<bitb::Direction>(back + bitb::LEFT);
	constexpr bitb::Direction back_right = static_cast<bitb::Direction>(back + bitb::RIGHT);

	bpos king_pos = bitb::pop_lsb(pos.pieces[static_cast<uint>(Piece::B_KING) + us]);

	bitboard king_sliding = get_pseudo_attacks<PieceType::QUEEN>(king_pos,
			pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]);
	bitboard candidates = king_sliding &
		pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + us];

	king_sliding = get_pseudo_attacks<PieceType::BISHOP>(king_pos,
			pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^ candidates);
	bitboard pinners = king_sliding & pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];

	for(bpos pin : bitb::BitscanIterator(pinners))
	{
		if(get_piece(pos, pin) == get_piece_from_type(PieceType::BISHOP, them_c) ||
		   get_piece(pos, pin) == get_piece_from_type(PieceType::QUEEN, them_c))
		{
			bitboard candidate = pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + us] &
				bitb::Between_in[king_pos][pin];

			if(candidate == 0)
				continue;

			PositionInfo::Pin new_pin;
			new_pin.pinned = bitb::pop_lsb(candidate);
			new_pin.pinner = pin;
			new_pin.mask = bitb::Between[king_pos][new_pin.pinner];

			info->pins.push_back(new_pin);
			info->pin_board |= 1ull << new_pin.pinned;
		}
	}

	king_sliding = get_pseudo_attacks<PieceType::ROOK>(king_pos,
			pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^ candidates);
	pinners = king_sliding & pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + them];

	for(bpos pin : bitb::BitscanIterator(pinners))
	{
		if(get_piece(pos, pin) == get_piece_from_type(PieceType::ROOK, them_c) ||
		   get_piece(pos, pin) == get_piece_from_type(PieceType::QUEEN, them_c))
		{
			bitboard candidate = pos.pieces[static_cast<uint>(Piece::BLACK_PIECES) + us] &
				bitb::Between_in[king_pos][pin];

			if(candidate == 0)
				continue;

			PositionInfo::Pin new_pin;

			new_pin.pinned = bitb::pop_lsb(candidate);
			new_pin.pinner = pin;
			new_pin.mask = bitb::Between[king_pos][pin];

			info->pins.push_back(new_pin);
			info->pin_board |= 1ull << new_pin.pinned;
		}
	}

	// It is possible that this code detects a check instead of a pin, but in
	// that case we would not be able to do en passant anyway Check for en
	// passant pin
	if(pos.hash->en_passant != 0)
	{
		bitboard enp_board = bitb::sq_bitb(pos.hash->en_passant);
		// If the king is not on the same rank as the pawns, en passant pin is impossible
		if(bitb::sq_rank(king_pos) & bitb::shift<back>(enp_board))
		{
			bitboard en_passant_pawn = bitb::shift<back>(enp_board);

			bitboard left_pawn = bitb::shift<back_left>(enp_board);
			if(left_pawn & pos.pieces[static_cast<uint>(Piece::B_PAWN) + us])
			{
				bitboard wo_left = pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^ left_pawn ^ en_passant_pawn;
				bitboard pinner_mask =
					movgen::get_pseudo_attacks<movgen::PieceType::ROOK>(king_pos, wo_left) & bitb::sq_rank(king_pos);

				if(pinner_mask & (pos.pieces[static_cast<uint>(Piece::B_QUEEN) + them] | pos.pieces[static_cast<uint>(Piece::B_ROOK) + them]))
					info->en_passant_pin = 1;
			}

			bitboard right_pawn = bitb::shift<back_right>(enp_board);
			if(&pos.pieces[static_cast<uint>(Piece::B_PAWN) + us])
			{
				bitboard wo_right = pos.pieces[static_cast<uint>(Piece::ALL_PIECES)] ^ right_pawn ^ en_passant_pawn;
				bitboard pinner_mask =
					movgen::get_pseudo_attacks<movgen::PieceType::ROOK>(king_pos, wo_right) & bitb::sq_rank(king_pos);

				if(pinner_mask & (pos.pieces[static_cast<uint>(Piece::B_QUEEN) + them] |
							pos.pieces[static_cast<uint>(Piece::B_ROOK) + them]))
					info->en_passant_pin = 1;
			}
		}
	}
}

template <movgen::Color color>
void movgen::get_attacked(BoardPosition& pos, PositionInfo* info)
{
	constexpr unsigned int us = 8 * static_cast<uint>(color);
	constexpr bitb::Direction forward = color == Color::WHITE ? bitb::UP : bitb::DOWN;
	constexpr bitb::Direction forward_left = static_cast<bitb::Direction>(forward + bitb::LEFT);
	constexpr bitb::Direction forward_right = static_cast<bitb::Direction>(forward + bitb::RIGHT);

	bitboard& piece_attacks = color == Color::WHITE ? info->w_piece_attacks : info->b_piece_attacks;
	bitboard& pawn_attacks = color == Color::WHITE ? info->w_pawn_attacks : info->b_pawn_attacks;
	bitboard& king_attacks = color == Color::WHITE ? info->w_king_attacks : info->b_king_attacks;

#define get_attacks(attacks, piece)                                                                      \
	for(bpos p_pos : bitb::BitscanIterator(pos.pieces[static_cast<uint>(piece) + us]))                   \
		attacks |= get_pseudo_attacks<piece>(p_pos, pos.pieces[static_cast<uint>(Piece::ALL_PIECES)]);

	get_attacks(king_attacks,  PieceType::KING);
	get_attacks(piece_attacks, PieceType::QUEEN);
	get_attacks(piece_attacks, PieceType::ROOK);
	get_attacks(piece_attacks, PieceType::BISHOP);
	get_attacks(piece_attacks, PieceType::KNIGHT);
	pawn_attacks |= bitb::shift<forward_left>(pos.pieces[static_cast<uint>(Piece::B_PAWN) + us]) |
		bitb::shift<forward_right>(pos.pieces[static_cast<uint>(Piece::B_PAWN) + us]);
}

template <movgen::Color color, movgen::GenType gen_type>
movgen::Move* movgen::generate_all_moves(BoardPosition& pos, movgen::Move* move_arr)
{
	constexpr movgen::Color col_them = movgen::Color(!static_cast<uint>(color));

	movgen::Move* arr_end = move_arr;

	switch(gen_type)
	{
	case movgen::GenType::ALL_MOVES:
	case movgen::GenType::QUIETS:
	case movgen::GenType::CAPTURES:
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::KING, color)]))
			arr_end = generate_piece_moves<PieceType::KING, gen_type>(piece_pos, pos, color, arr_end);
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::KNIGHT, color)]))
			arr_end = generate_piece_moves<PieceType::KNIGHT, gen_type>(piece_pos, pos, color, arr_end);
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::BISHOP, color)]))
			arr_end = generate_piece_moves<PieceType::BISHOP, gen_type>(piece_pos, pos, color, arr_end);
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::ROOK, color)]))
			arr_end = generate_piece_moves<PieceType::ROOK, gen_type>(piece_pos, pos, color, arr_end);
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::QUEEN, color)]))
			arr_end = generate_piece_moves<PieceType::QUEEN, gen_type>(piece_pos, pos, color, arr_end);
		arr_end = movgen::generate_pawn_moves<color, gen_type>(pos, arr_end);
		break;
	case movgen::GenType::PROMOTIONS:
		arr_end = movgen::generate_pawn_moves<color, gen_type>(pos, arr_end);
		break;
	case movgen::GenType::CASTLING:
		for(auto piece_pos : bitb::BitscanIterator(pos.pieces[(uint)get_piece_from_type(PieceType::KING, color)]))
			arr_end = generate_piece_moves<PieceType::KING, gen_type>(piece_pos, pos, color, arr_end);
		break;
	}

	if(pos.info != nullptr)
		delete pos.info;
	pos.info = new PositionInfo;

	get_checkers<col_them>(pos, pos.info);
	get_pinners<col_them>(pos, pos.info);

	get_attacked<Color::WHITE>(pos, pos.info);
	get_attacked<Color::BLACK>(pos, pos.info);

	return arr_end;
}

ALL_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::ALL_MOVES);
ALL_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::QUIETS);
ALL_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::CAPTURES);
ALL_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::PROMOTIONS);
ALL_MOVE_INSTANCE(movgen::Color::WHITE, movgen::GenType::CASTLING);
ALL_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::ALL_MOVES);
ALL_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::QUIETS);
ALL_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::CAPTURES);
ALL_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::PROMOTIONS);
ALL_MOVE_INSTANCE(movgen::Color::BLACK, movgen::GenType::CASTLING);

// Convinient wrappers
template<>
movgen::Move* movgen::generate_all_moves<movgen::Color::WHITE, movgen::GenType::LEGAL>(BoardPosition& pos, movgen::Move* move_arr)
{
	auto* arr_end = generate_all_moves<Color::WHITE, GenType::ALL_MOVES>(pos, move_arr);
	return get_legal_moves(pos, move_arr, arr_end);
}
template<>
movgen::Move* movgen::generate_all_moves<movgen::Color::BLACK, movgen::GenType::LEGAL>(BoardPosition& pos, movgen::Move* move_arr)
{
	auto* arr_end = generate_all_moves<Color::BLACK, GenType::ALL_MOVES>(pos, move_arr);
	return get_legal_moves(pos, move_arr, arr_end);
}

movgen::Move* movgen::get_legal_moves(BoardPosition& pos, movgen::Move* move_arr, movgen::Move* arr_end)
{
	const Color us_c = pos.side_to_move;

	const unsigned int us = 8 * static_cast<uint>(us_c);
	const bitboard pinned = pos.info->pin_board;
	const bpos ksq = bitb::pop_lsb(pos.pieces[static_cast<uint>(Piece::B_KING) + us]);

	const bitboard attacked = (pos.side_to_move == Color::WHITE)
								  ? (pos.info->b_piece_attacks | pos.info->b_pawn_attacks | pos.info->b_king_attacks)
								  : (pos.info->w_piece_attacks | pos.info->w_pawn_attacks | pos.info->w_king_attacks);
	movgen::Move* cur_move = move_arr;

	// Only king moves are possible
	if(pos.info->checks_num >= 2)
	{
		while(cur_move != arr_end)
			if(cur_move->from == ksq && cur_move->get_type() != MoveType::CASTLING &&
				!(attacked & bitb::sq_bitb(cur_move->to)))
				cur_move++;
			else
				*cur_move = *(--arr_end);
		return arr_end;
	}
	// Only allow king moves and blockers
	if(pos.info->checks_num == 1)
	{
		while(cur_move != arr_end)
		{
			if(bitb::sq_bitb(cur_move->from) & pos.info->pin_board)
				*cur_move = *(--arr_end);
			else if((cur_move->from == ksq && cur_move->get_type() != MoveType::CASTLING && _is_legal(pos, *cur_move)) ||
			   (cur_move->from != ksq && bitb::sq_bitb(cur_move->to) & pos.info->blockers))
				cur_move++;
			else
				*cur_move = *(--arr_end);
		}
		return arr_end;
	}

	while(cur_move != arr_end)
	{
		// Check for legality only if one these 4 requirements are met
		if (!(bitb::sq_bitb(cur_move->from) & pinned ||
					cur_move->from == ksq ||
					cur_move->get_type() == MoveType::EN_PASSANT) ||
				_is_legal(pos, *cur_move)
		   )
			cur_move++;
		else
			*cur_move = *(--arr_end);
	}
	return arr_end;
}

template <movgen::GenType gen_type>
movgen::Move* movgen::make_move(movgen::BoardPosition* pos, movgen::Move& move, movgen::Move* move_arr)
{
	const bitb::Direction down = pos->side_to_move == Color::WHITE ? bitb::DOWN : bitb::UP;
	const movgen::CastlingRights castling = pos->side_to_move == Color::WHITE ?
		movgen::CastlingRights::WHITE_CASTLE : movgen::CastlingRights::BLACK_CASTLE;

	const movgen::Color cur_color = pos->side_to_move;
	const uint16_t us = cur_color == Color::BLACK ? 8 : 0;
	const uint16_t them = cur_color == Color::BLACK ? 0 : 8;

    const movgen::Piece captured = move.get_type() == MoveType::EN_PASSANT ?
		static_cast<movgen::Piece>(static_cast<uint>(Piece::B_PAWN) + them) : movgen::get_piece(*pos, move.to);

	pos->hash = new BoardHash(pos->hash);
	pos->hash->key ^= zobrist::side;

	// Flip the color
	pos->side_to_move = static_cast<Color>(!(bool)pos->side_to_move);
	if(pos->side_to_move == Color::WHITE)
		pos->fullmove++;
	pos->hash->ply++; // Reset later, if necessary
	pos->hash->en_passant = 0;

	switch(move.get_type())
	{
		case movgen::MoveType::CAPTURE:
		pos->pieces[static_cast<uint>(captured)] &= ~(1ull << move.to);
		pos->squares[move.to] = Piece::NO_PIECE;
		pos->hash->ply = 0;

		pos->hash->key ^= zobrist::table[static_cast<uint>(captured)][move.to];
	// Fallthrough
	case movgen::MoveType::REGULAR:
		_move_piece(pos, move.piece, move.from, move.to);
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.from];
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.to];
		break;
	case movgen::MoveType::PROMOTION_CAPTURE:
		pos->pieces[static_cast<uint>(pos->squares[move.to])] &= ~(1ull << move.to);
		pos->hash->key ^= zobrist::table[static_cast<uint>(captured)][move.to];
	// Fallthrough
	case movgen::MoveType::PROMOTION: {
		movgen::Piece prom = move.get_promoted();
		assert((2 <= (uint)prom && (uint)prom < 6) || (10 <= (uint)prom && (uint)prom < 14));

		pos->pieces[static_cast<uint>(move.piece)] &= ~(1ull << move.from);
		pos->pieces[static_cast<uint>(prom)] |= (1ull << move.to);

		pos->squares[move.from] = Piece::NO_PIECE;
		pos->squares[move.to] = prom;

		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.from];
		pos->hash->key ^= zobrist::table[static_cast<uint>(prom)][move.to];
		break;
	}
	case movgen::MoveType::EN_PASSANT:
		pos->pieces[static_cast<uint>(pos->squares[move.to + down])] ^= 1ull << (move.to + down);
		pos->squares[move.to + down] = Piece::NO_PIECE;
		_move_piece(pos, move.piece, move.from, move.to);

		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.from];
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.to];
		pos->hash->key ^= zobrist::table[static_cast<uint>(captured)][move.to];
		break;
	case movgen::MoveType::DOUBLE_MOVE:
		pos->hash->en_passant = move.to + down;
		_move_piece(pos, move.piece, move.from, move.to);

		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.from];
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.to];
		break;
	case movgen::MoveType::CASTLING: {
		const bitboard king_rank = bitb::sq_rank(move.from);
		const movgen::Piece rook_piece = get_piece_from_type(PieceType::ROOK, cur_color);

		if(move.get_castling() == Castling::SHORT_CASTLE)
		{
			_move_piece(pos, move.piece, move.from, move.to);
			_move_piece(pos, rook_piece, move.from - 3, move.from - 1);

			pos->hash->key ^= zobrist::table[static_cast<uint>(rook_piece)][move.from - 3];
			pos->hash->key ^= zobrist::table[static_cast<uint>(rook_piece)][move.from - 1];
		}
		else
		{
			_move_piece(pos, move.piece, move.from, move.to);
			_move_piece(pos, rook_piece, move.from + 4, move.from + 1);

			pos->hash->key ^= zobrist::table[static_cast<uint>(rook_piece)][move.from + 4];
			pos->hash->key ^= zobrist::table[static_cast<uint>(rook_piece)][move.from + 1];
		}

		pos->hash->castling_rights &= ~(uint)(cur_color == Color::WHITE ?
				CastlingRights::WHITE_CASTLE :
				CastlingRights::BLACK_CASTLE);

		pos->hash->ply = 0;
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.from];
		pos->hash->key ^= zobrist::table[static_cast<uint>(move.piece)][move.to];
		break;
	}
	}

	if(movgen::get_piece_type(move.piece) == PieceType::PAWN)
		pos->hash->ply = 0;

	// Assign composite bitboards
	pos->pieces[(uint)Piece::BLACK_PIECES] = pos->pieces[(uint)Piece::B_KING] | pos->pieces[(uint)Piece::B_QUEEN] |
		pos->pieces[(uint)Piece::B_ROOK] | pos->pieces[(uint)Piece::B_BISHOP] |pos->pieces[(uint)Piece::B_KNIGHT] |
		pos->pieces[(uint)Piece::B_PAWN];

	pos->pieces[(uint)Piece::WHITE_PIECES] = pos->pieces[(uint)Piece::W_KING] | pos->pieces[(uint)Piece::W_QUEEN] |
		pos->pieces[(uint)Piece::W_ROOK] | pos->pieces[(uint)Piece::W_BISHOP] |pos->pieces[(uint)Piece::W_KNIGHT] |
		pos->pieces[(uint)Piece::W_PAWN];

	pos->pieces[(uint)Piece::ALL_PIECES] = pos->pieces[(uint)Piece::BLACK_PIECES] | pos->pieces[(uint)Piece::WHITE_PIECES];

	if(pos->hash->castling_rights & static_cast<uint>(castling))
	{
		if(movgen::get_piece_type(move.piece) == PieceType::KING)
		{
			pos->hash->ply = 0;
			pos->hash->castling_rights &= ~static_cast<uint>(castling);
			pos->hash->key ^= zobrist::castling[pos->hash->castling_rights & static_cast<uint>(castling)];
		}
		else if(movgen::get_piece_type(move.piece) == PieceType::ROOK)
		{
			movgen::CastlingRights castling_change = movgen::CastlingRights::NO_CASTLING;
			// h rank rook
			if(bitb::sq_bitb(move.from) & bitb::File[7])
				castling_change = static_cast<movgen::CastlingRights>(static_cast<uint>(castling) & static_cast<uint>(CastlingRights::SHORT));
			// a rank rook
			else if(bitb::sq_bitb(move.from) & bitb::File[0])
				castling_change = static_cast<movgen::CastlingRights>(static_cast<uint>(castling) & static_cast<uint>(CastlingRights::LONG));

			pos->hash->ply = 0;
			pos->hash->key ^= zobrist::castling[static_cast<uint>(pos->hash->castling_rights) & static_cast<uint>(castling_change)];
			pos->hash->castling_rights &= ~static_cast<uint>(castling_change);
		}
	}

	// Check for 3 fold repetition rule
	if(pos->hash->ply > 2)
	{
		uint16_t reps = 0;
		movgen::BoardHash* hash_it = pos->hash;
		do
		{
			hash_it = hash_it->prev;
			reps += hash_it->key == pos->hash->key;
		} while(hash_it->ply > 0 && hash_it->prev != nullptr);

		assert(reps <= 3); // Should not be more than 3 in a normal game
		if(reps >= 3)
		{
			pos->repetiton_num = reps;
			return move_arr; // Empty array
		}
	}

	if (move_arr == nullptr)
		return nullptr;

	movgen::Move* arr_end;

	if(pos->side_to_move == Color::WHITE)
		arr_end = movgen::generate_all_moves<Color::WHITE, gen_type>(*pos, move_arr);
	else
		arr_end = movgen::generate_all_moves<Color::BLACK, gen_type>(*pos, move_arr);

	return arr_end;
}

MAKE_MOVE_INSTANCE(movgen::GenType::ALL_MOVES);
MAKE_MOVE_INSTANCE(movgen::GenType::LEGAL);
MAKE_MOVE_INSTANCE(movgen::GenType::QUIETS);
MAKE_MOVE_INSTANCE(movgen::GenType::CAPTURES);
MAKE_MOVE_INSTANCE(movgen::GenType::PROMOTIONS);
MAKE_MOVE_INSTANCE(movgen::GenType::CASTLING);

movgen::GameStatus movgen::check_game_state(movgen::BoardPosition* pos, movgen::Move* move_arr, movgen::Move* arr_end)
{
	if(pos->repetiton_num >= 3)
		return GameStatus::DRAW;

	if((arr_end - move_arr) == 0)
	{
		if(pos->info->checks_num > 0)
			return pos->side_to_move == Color::WHITE ? GameStatus::BLACK_WINS : GameStatus::WHITE_WINS;
		else
			return GameStatus::DRAW;
	}
	if(pos->hash->ply == 50)
		return GameStatus::DRAW;

	return GameStatus::GAME_CONTINUES;
}

void movgen::undo_move(movgen::BoardPosition* pos, movgen::Move& move)
{
	pos->side_to_move = static_cast<movgen::Color>(!(uint)pos->side_to_move);
	if(pos->side_to_move == Color::BLACK)
		pos->fullmove--;

	const bitb::Direction down = pos->side_to_move == Color::WHITE ? bitb::DOWN : bitb::UP;
	_move_piece(pos, move.piece, move.to, move.from);

	switch(move.get_type())
	{
		case movgen::MoveType::PROMOTION:
		pos->pieces[static_cast<uint>(move.get_promoted())] &= ~bitb::sq_bitb(move.to);
		break;
	case movgen::MoveType::PROMOTION_CAPTURE:
		pos->pieces[static_cast<uint>(move.get_promoted())] &= ~bitb::sq_bitb(move.to);
		[[fallthrough]];
	case movgen::MoveType::CAPTURE:
		pos->pieces[static_cast<uint>(move.get_captured())] |= bitb::sq_bitb(move.to);
		pos->squares[move.to] = move.get_captured();
		break;
	case movgen::MoveType::EN_PASSANT:
		pos->pieces[static_cast<uint>(move.get_captured())] |= bitb::sq_bitb(move.to + down);
		pos->squares[move.to + down] = move.get_captured();
		break;
	case movgen::MoveType::CASTLING: {
		movgen::Piece rook = get_piece_from_type(PieceType::ROOK, pos->side_to_move);
		if(move.get_castling() == Castling::SHORT_CASTLE)
			_move_piece(pos, rook, move.from - 1, move.to - 1);
		else
			_move_piece(pos, rook, move.from + 1, move.to + 2);
		break;
	}
	default:
		break;
	}

	movgen::BoardHash* cur = pos->hash;
	pos->hash = pos->hash->prev;
	pos->repetiton_num = 0;

	delete cur;

	// Assign composite bitboards
	pos->pieces[static_cast<uint>(Piece::BLACK_PIECES)] = pos->pieces[static_cast<uint>(Piece::B_KING)]   |
		pos->pieces[static_cast<uint>(Piece::B_QUEEN)]  | pos->pieces[static_cast<uint>(Piece::B_ROOK)]   |
		pos->pieces[static_cast<uint>(Piece::B_BISHOP)] | pos->pieces[static_cast<uint>(Piece::B_KNIGHT)] |
		pos->pieces[static_cast<uint>(Piece::B_PAWN)];

	pos->pieces[static_cast<uint>(Piece::WHITE_PIECES)] = pos->pieces[static_cast<uint>(Piece::W_KING)]   |
		pos->pieces[static_cast<uint>(Piece::W_QUEEN)]  | pos->pieces[static_cast<uint>(Piece::W_ROOK)]   |
		pos->pieces[static_cast<uint>(Piece::W_BISHOP)] | pos->pieces[static_cast<uint>(Piece::W_KNIGHT)] |
		pos->pieces[static_cast<uint>(Piece::W_PAWN)];

	pos->pieces[static_cast<uint>(Piece::ALL_PIECES)] = pos->pieces[static_cast<uint>(Piece::BLACK_PIECES)] |
		pos->pieces[static_cast<uint>(Piece::WHITE_PIECES)];
}

bool _is_legal(movgen::BoardPosition& pos, movgen::Move& move)
{
	const movgen::Color c = pos.side_to_move;
	const bpos from = move.from;
	const bpos to = move.to;

	const bitboard attacked = (pos.side_to_move == movgen::Color::WHITE)
								  ? (pos.info->b_piece_attacks | pos.info->b_pawn_attacks | pos.info->b_king_attacks)
								  : (pos.info->w_piece_attacks | pos.info->w_pawn_attacks | pos.info->w_king_attacks);

	if(move.get_type() == movgen::MoveType::EN_PASSANT)
	{
		if(pos.info->pin_board & bitb::sq_bitb(move.from))
			return false;
		return !(pos.info->en_passant_pin);
	}
	else if(move.get_type() == movgen::MoveType::CASTLING)
	{
		// Check that no square inbetween are under attack
		bitboard end_sq = from > to ? from - 2 : from + 2;
		return !(attacked & bitb::Between[from][end_sq]);
	}
	else if(get_piece_type(movgen::get_piece(pos, move.from)) == movgen::PieceType::KING)
	{
		return !(attacked & bitb::sq_bitb(to));
	}

	// A pinned piece can move only to specified squares
	for(auto pin : pos.info->pins)
		if(pin.pinned == move.from)
			return bitb::sq_bitb(move.to) & pin.mask;
	return true;
}

void bitb_movearray(movgen::Piece piece,
					bpos starting_pos,
					bitboard move_board,
					bitboard them,
					movgen::BoardPosition& pos,
					movgen::Move* move_arr)
{
	bitboard captures = move_board & them;
	bitboard quiet = move_board ^ captures;

	for(auto pos_to : bitb::BitscanIterator(quiet))
		*move_arr++ = movgen::Move(piece, starting_pos, pos_to);
	for(auto pos_to : bitb::BitscanIterator(captures))
		*move_arr++ = movgen::Move(piece, starting_pos, pos_to, movgen::get_piece(pos, pos_to));
}

template <movgen::Color c, bitb::Direction d>
inline void _make_promotions(movgen::Move* &move_arr, bpos to, movgen::Piece capture)
{
	constexpr movgen::Piece piece = (c == movgen::Color::WHITE ? movgen::Piece::W_PAWN : movgen::Piece::B_PAWN);

	*move_arr++ = movgen::Move(piece, to - d, to, capture, movgen::get_piece_from_type(movgen::PieceType::QUEEN,  c));
	*move_arr++ = movgen::Move(piece, to - d, to, capture, movgen::get_piece_from_type(movgen::PieceType::ROOK,   c));
	*move_arr++ = movgen::Move(piece, to - d, to, capture, movgen::get_piece_from_type(movgen::PieceType::BISHOP, c));
	*move_arr++ = movgen::Move(piece, to - d, to, capture, movgen::get_piece_from_type(movgen::PieceType::KNIGHT, c));
}

void _move_piece(movgen::BoardPosition* pos, movgen::Piece piece, bpos from, bpos to)
{
	pos->pieces[static_cast<uint>(piece)] &= ~bitb::sq_bitb(from);
	pos->pieces[static_cast<uint>(piece)] |= bitb::sq_bitb(to);

	pos->squares[from] = movgen::Piece::NO_PIECE;
	pos->squares[to] = piece;
}
