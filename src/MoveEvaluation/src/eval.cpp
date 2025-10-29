#include "../headers/eval.h"
#include "MovgenTypes.h"

constexpr bitboard center = 0x1818000000;

float eval(movgen::BoardPosition& pos)
{
	// (pos.side_to_move * 2 - 1) -- map side to move to [-1;+1]
	// This is needed for negamax framework
	return (_eval_side<movgen::Color::WHITE>(pos)
			- _eval_side<movgen::Color::BLACK>(pos))
			* (static_cast<uint>(pos.side_to_move) * 2 - 1);
}

template<movgen::Color col>
float _eval_side(const movgen::BoardPosition& pos)
{
	constexpr int8_t them = col == movgen::Color::WHITE ? 0 : 8;
	constexpr int8_t us = col == movgen::Color::WHITE ? 8 : 0;

	const bitboard& piece_attacks = col == movgen::Color::WHITE ?
		pos.info->w_piece_attacks : pos.info->b_piece_attacks;
	const bitboard& pawn_attacks = col == movgen::Color::WHITE ?
		pos.info->w_pawn_attacks : pos.info->b_pawn_attacks;
	const bitboard& king_attacks = col == movgen::Color::WHITE ?
		pos.info->w_king_attacks : pos.info->b_king_attacks;

	float piece_eval = 0.0f;
	constexpr uint16_t piece_from = static_cast<uint>(movgen::Piece::B_KING) + us;
	constexpr uint16_t piece_to = static_cast<uint>(movgen::Piece::B_PAWN) + us;
	for(uint16_t piece = piece_from; piece <= piece_to; piece++)
		piece_eval += static_cast<float>(bitb::bit_count(pos.pieces[piece])) *
					  piece_val(movgen::get_piece_type(static_cast<movgen::Piece>(piece)));

	float piece_mobility = 0.0f;
	piece_mobility += static_cast<float>(
		bitb::bit_count(pos.info->w_piece_attacks & ~(pos.pieces[static_cast<uint>(movgen::Piece::BLACK_PIECES) + us]))) * 0.025f;
	// Penalize king mobility
	piece_mobility -= static_cast<float>
		(bitb::bit_count(pos.info->w_king_attacks & ~(pos.pieces[static_cast<uint>(movgen::Piece::BLACK_PIECES) + us]))) * 0.1f;

	float center_control = static_cast<float>(bitb::bit_count(
				center & (pos.pieces[static_cast<uint>(movgen::Piece::BLACK_PIECES) + us]))) * 0.1f;
	center_control += static_cast<float>(bitb::bit_count(center & piece_attacks)) * 0.05f;

	//And finnaly add piece square tables
	float pst = 0.0f;
	constexpr uint16_t piece_type_from = static_cast<uint>(movgen::PieceType::KING) + us;
	constexpr uint16_t piece_type_to = static_cast<uint>(movgen::PieceType::PAWN) + us;
	for(uint16_t piece_type = piece_type_from; piece_type < piece_type_to; piece_type++)
		for(bpos square : bitb::BitscanIterator(pos.pieces[piece_type + us]))
			pst += col == movgen::Color::WHITE ?
				piece_square_tables[piece_type - 1][63 - square] :
				piece_square_tables[piece_type - 1][square];
	pst *= 0.01;

	if(col == pos.side_to_move)
		return piece_eval + piece_mobility + center_control + pst + 0.25; //Tempo score
	else
		return piece_eval + piece_mobility + center_control + pst;
}
