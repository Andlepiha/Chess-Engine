#include "../include/MagicNumbers.h"
#include "../include/MoveGeneration.h"

movgen::GeneratedMagics movgen::generated_magics;

bitboard movgen::get_bishop_attacks(bpos square, bitboard blocker) {
  uint64_t index = blocker & generated_magics.bishop_magics[square].mask;
  index *= generated_magics.bishop_magics[square].magic;
  index >>= 64 - 9;
  return generated_magics.bishop_attacks[square][index];
}

bitboard movgen::get_rook_attacks(bpos square, bitboard blocker) {
  uint64_t index = blocker & generated_magics.rook_magics[square].mask;
  index *= generated_magics.rook_magics[square].magic;
  index >>= 64 - 12;
  return generated_magics.rook_attacks[square][index];
}

uint64_t movgen::get_magic_index_bishop(bitboard blocker, Magic& magic) {
  uint64_t index = blocker & magic.mask;
  index *= magic.magic;
  index >>= 64 - 9;
  return index;
}

uint64_t movgen::get_magic_index_rook(bitboard blocker, Magic& magic) {
  uint64_t index = blocker & magic.mask;
  index *= magic.magic;
  index >>= 64 - 12;
  return index;
}

uint64_t movgen::_random_uint64() {
  uint64_t u1, u2, u3, u4;
  u1 = (uint64_t)(rand()) & 0xFFFF;
  u2 = (uint64_t)(rand()) & 0xFFFF;
  u3 = (uint64_t)(rand()) & 0xFFFF;
  u4 = (uint64_t)(rand()) & 0xFFFF;
  return u1 | (u2 << 16) | (u3 << 32) | (u4 << 48);
}

inline bitboard movgen::_random_uint64_fewbits() {
  return _random_uint64() & _random_uint64() & _random_uint64();
}

inline bitboard movgen::_get_next_blocker(bitboard current,
  bitboard blocker_mask) {
  return (current - blocker_mask) & blocker_mask;
}


bitboard movgen::_generate_bishop_mask(bpos sq) {
  bitboard attacks = 0;

  for (int y = sq / 8 - 1, x = sq % 8 - 1; (x > 0) && (y > 0); --x, --y)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8 - 1, x = sq % 8 + 1; (x < 7) && (y > 0); ++x, --y)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8 + 1, x = sq % 8 - 1; (x > 0) && (y < 7); --x, ++y)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8 + 1, x = sq % 8 + 1; (x < 7) && (y < 7); ++x, ++y)
    bitb::set_bit(&attacks, y * 8 + x);

  return attacks;
}

bitboard movgen::_generate_bishop_moves(bpos sq, bitboard blocker) {
  bitboard attacks = 0;

  for (int y = sq / 8 - 1, x = sq % 8 - 1; (x >= 0) && (y >= 0); --x, --y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8 - 1, x = sq % 8 + 1; (x < 8) && (y >= 0); ++x, --y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8 + 1, x = sq % 8 - 1; (x >= 0) && (y < 8); --x, ++y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8 + 1, x = sq % 8 + 1; (x < 8) && (y < 8); ++x, ++y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }

  return attacks;
}

bitboard movgen::_generate_rook_mask(bpos sq) {
  bitboard attacks = 0;

  for (int y = sq / 8, x = sq % 8 - 1; x > 0; --x)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8 - 1, x = sq % 8; y > 0; --y)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8 + 1, x = sq % 8; y < 7; ++y)
    bitb::set_bit(&attacks, y * 8 + x);
  for (int y = sq / 8, x = sq % 8 + 1; x < 7; ++x)
    bitb::set_bit(&attacks, y * 8 + x);

  return attacks;
}

bitboard movgen::_generate_rook_moves(bpos sq, bitboard blocker) {
  bitboard attacks = 0;

  for (int y = sq / 8, x = sq % 8 - 1; x >= 0; --x) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8 - 1, x = sq % 8; y >= 0; --y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8 + 1, x = sq % 8; y < 8; ++y) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }
  for (int y = sq / 8, x = sq % 8 + 1; x < 8; ++x) {
    bitb::set_bit(&attacks, y * 8 + x);
    if (bitb::read_bit(blocker, y * 8 + x))
      break;
  }

  return attacks;
}

void movgen::init_magics() {
  for (bpos pos = 0; pos < 64; pos++) {
    Magic rook_magic, bishop_magic;

    // Exclude border bits from mask, because they are always blocking
    rook_magic.mask = _generate_rook_mask(pos);
    bishop_magic.mask = _generate_bishop_mask(pos);

    // Keep generating new magics until there is no collisions
    while (true) {
      bishop_magic.magic = _random_uint64_fewbits();
      if (bitb::bit_count((bishop_magic.mask * bishop_magic.magic) &
        0xFF00000000000000ull) < 6)
        continue;
      if (try_fill_table_bishop(pos, bishop_magic)) {
        generated_magics.bishop_magics[pos] = bishop_magic;
        break;
      }
    }
    while (true) {
      rook_magic.magic = _random_uint64_fewbits();
      if (bitb::bit_count((rook_magic.mask * rook_magic.magic) &
        0xFF00000000000000ull) < 6)
        continue;
      if (try_fill_table_rook(pos, rook_magic)) {
        generated_magics.rook_magics[pos] = rook_magic;
        break;
      }
    }
  }
  initialized_magics = true;
}

bool movgen::try_fill_table_bishop(bpos sq, Magic& magic) {
  bitboard blocker = 0;
  for (int i = 0; i < 512; i++)
    generated_magics.bishop_attacks[sq][i] = 0;
  do {
    bitboard attacks = movgen::_generate_bishop_moves(sq, blocker);
    uint64_t index = movgen::get_magic_index_bishop(blocker, magic);

    if (generated_magics.bishop_attacks[sq][index] == 0) {
      generated_magics.bishop_attacks[sq][index] = attacks;
    }
    else if (generated_magics.bishop_attacks[sq][index] != attacks) {
      return 0;
    }

    blocker = movgen::_get_next_blocker(blocker, magic.mask);
  } while (blocker != 0);

  return 1;
}

bool movgen::try_fill_table_rook(bpos sq, Magic& magic) {
  bitboard blocker = 0;
  for (int i = 0; i < 4096; i++)
    generated_magics.rook_attacks[sq][i] = 0;
  do {
    bitboard attacks = movgen::_generate_rook_moves(sq, blocker);
    uint64_t index = movgen::get_magic_index_rook(blocker, magic);

    if (generated_magics.rook_attacks[sq][index] == 0) {
      generated_magics.rook_attacks[sq][index] = attacks;
    }
    else if (generated_magics.rook_attacks[sq][index] != attacks) {
      return 0;
    }
    blocker = movgen::_get_next_blocker(blocker, magic.mask);
  } while (blocker != 0);

  return 1;
}
