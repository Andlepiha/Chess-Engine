#ifndef BITBOARD_H
#define BITBOARD_H

#include <atomic>
#include <iterator>
#include <vector>

/*
    Bit numeration:
    63 62 61 60 59 58 57 56
    ...
    7  6  5  4  3  2  1  0
*/
typedef unsigned long long bitboard;
typedef unsigned char bpos;

namespace bitb
{
// Initializes bitboard global variables
void init();
extern std::atomic_bool initialized;

constexpr void set_bit(bitboard* b, bpos pos)
{
	*b |= 1ull << pos;
}
constexpr void clear_bit(bitboard* b, bpos pos)
{
	*b &= ~(1ull << pos);
}
constexpr void flip_bit(bitboard* b, bpos pos)
{
	*b ^= 1ull << pos;
}
constexpr bool read_bit(bitboard& b, bpos pos)
{
	return b & (1ull << pos);
}

inline bitboard flip_vertical(bitboard b)
{
#if defined(__llvm__)
    return __builtin_bswap64( b );
#elif defined(_MSC_VER)
    return _byteswap_uint64( value );
#endif
}

constexpr bitboard flip_horizontal(bitboard b)
{
	constexpr bitboard k1 = 0x5555555555555555;
	constexpr bitboard k2 = 0x3333333333333333;
	constexpr bitboard k4 = 0x0f0f0f0f0f0f0f0f;
	b = ((b >> 1) & k1) + 2 * (b & k1);
	b = ((b >> 2) & k2) + 4 * (b & k2);
	b = ((b >> 4) & k4) + 16 * (b & k4);
	return b;
}

enum Direction
{
	LEFT = 1,
	RIGHT = -1,
	UP = 8,
	DOWN = -8
};

constexpr bitboard File[8]{
	0x8080808080808080,
	0x8080808080808080 >> 1,
	0x8080808080808080 >> 2,
	0x8080808080808080 >> 3,
	0x8080808080808080 >> 4,
	0x8080808080808080 >> 5,
	0x8080808080808080 >> 6,
	0x8080808080808080 >> 7,
};
constexpr bitboard Rank[8]{
	0xFFull,
	0xFFull << (8 * 1),
	0xFFull << (8 * 2),
	0xFFull << (8 * 3),
	0xFFull << (8 * 4),
	0xFFull << (8 * 5),
	0xFFull << (8 * 6),
	0xFFull << (8 * 7),
};

// Converts a square position to bitboard with 1 in that position
inline const bitboard sq_bitb(bpos pos) { return 1ull << pos; }

/// @brief converts a bitboard into a sequense of values between 0 and 63, that
/// correspond to positions of set bits in the bitboard
std::vector<bpos> bitscan(bitboard board);
bpos pop_lsb(bitboard board);
unsigned char bit_count(bitboard board);

// Lazy implementation of the bitscan method
class BitscanIterator
{
	bitboard board;
public:
	explicit BitscanIterator(bitboard initial) : board(initial) {}

	class Iterator
	{
		bitboard board;
	public:
		using iterator_category = std::input_iterator_tag;
        using value_type = bpos;
        using difference_type = std::ptrdiff_t;
        using pointer = const bpos*;
        using reference = const bpos&;

		explicit Iterator(bitboard b) : board(b) {}

		bpos operator*() const { return pop_lsb(board); }
		bool operator!=(const Iterator& other) const {
			return board != other.board;
		}
		Iterator& operator++() {
			board &= board - 1;
			return *this;
		}
	};

	Iterator begin() const { return Iterator(board); };
	Iterator end() const { return Iterator(0); };
};

// Common line(from edge to edge) between two squares (if exists)
extern bitboard Line[64][64];
// A line connecting two squares(includes these squares) (if exists)
extern bitboard Between[64][64];
// The same as between, but not including edge squares
extern bitboard Between_in[64][64];

// Get a rank or a file of a given square
constexpr bitboard sq_rank(bpos sq)
{
	return Rank[sq >> 3];
}
constexpr bitboard sq_file(bpos sq)
{
	return File[7 - sq & 0b111];
}

template <bitb::Direction D>
constexpr bitboard shift(bitboard bitb)
{
	return D == bitb::UP				   ? bitb << 8
		   : D == bitb::UP + bitb::UP	   ? bitb << 16
		   : D == bitb::UP + bitb::LEFT	   ? (bitb & ~File[0]) << 9
		   : D == bitb::UP + bitb::RIGHT   ? (bitb & ~File[7]) << 7
		   : D == bitb::DOWN			   ? bitb >> 8
		   : D == bitb::DOWN + bitb::DOWN  ? bitb >> 16
		   : D == bitb::DOWN + bitb::LEFT  ? (bitb & ~File[0]) >> 7
		   : D == bitb::DOWN + bitb::RIGHT ? (bitb & ~File[7]) >> 9
		   : D == bitb::LEFT			   ? (bitb & ~File[0]) << 1
		   : D == bitb::RIGHT			   ? (bitb & ~File[7]) >> 1
										   : 0;
};
}; // namespace bitb

#endif // BITBOARD_H
