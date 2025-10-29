#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "MovgenTypes.h"

namespace zobrist
{
    // Marsaglia's xorshf generator
    inline uint64_t xorshift64star(void)
    {
        /* initial seed must be nonzero, don't use a static variable for the state if multithreaded */
        static uint64_t x = 1;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        return x * 0x2545F4914F6CDD1DULL;
    }

    extern uint64_t table[static_cast<uint>(movgen::Piece::PIECE_NB)][64];
    extern uint64_t en_passant[8];
    extern uint64_t castling[static_cast<uint>(movgen::CastlingRights::CASTLING_NB)];
    extern uint64_t side;

    void init();
    uint64_t hash(const movgen::BoardPosition &pos, const movgen::BoardHash &hash);
}

#endif // !ZOBRIST_H
