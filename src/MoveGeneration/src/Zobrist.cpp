#include "../include/Zobrist.h"

uint64_t zobrist::table[static_cast<uint>(movgen::Piece::PIECE_NB)][64];
uint64_t zobrist::castling[static_cast<uint>(movgen::CastlingRights::CASTLING_NB)];
uint64_t zobrist::en_passant[8];
uint64_t zobrist::side;

void zobrist::init()
{
    /// Indexes:
    // 0 -- Black to move
    // 1-17 -- Castling rights
    // 18-26 -- en-passant file
    // 27-780 -- pieces
    for (uint16_t i = 0; i < 8; i++)
        zobrist::en_passant[i] = zobrist::xorshift64star();
    for (uint16_t i = 0; i < static_cast<uint>(movgen::CastlingRights::CASTLING_NB); i++)
        zobrist::castling[i] = zobrist::xorshift64star();
    zobrist::side = zobrist::xorshift64star();

    for (uint16_t i = 0; i < static_cast<uint>(movgen::Piece::PIECE_NB); i++)
        for (uint16_t j = 0; j < 64; j++)
            zobrist::table[i][j] = zobrist::xorshift64star();
}

uint64_t zobrist::hash(const movgen::BoardPosition &pos, const movgen::BoardHash &hash)
{
    const uint8_t us = pos.side_to_move == movgen::Color::BLACK ? 0 : 8;

    uint64_t key = 0;

    key ^= zobrist::side * static_cast<uint>(pos.side_to_move);
    key ^= zobrist::castling[hash.castling_rights];
    key ^= zobrist::en_passant[hash.en_passant % 8]; // En passant file

    for (uint piece = static_cast<uint>(movgen::PieceType::KING); piece <= static_cast<uint>(movgen::PieceType::PAWN); piece++)
        for (auto pos : bitb::bitscan(pos.pieces[piece]))
            key ^= zobrist::table[piece][pos];

    return key;
}
