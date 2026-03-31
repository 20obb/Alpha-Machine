/*
 * bitboard.h — Bitboard operations and pre-computed attack tables
 * =============================================================
 *
 * Bitboard = 64-bit integer.
 * Each bit represents a square on the board.
 * bit 0 = A1, bit 1 = B1, ..., bit 63 = H8.
 *
 * This file provides:
 *   - Bit manipulation functions (set, clear, pop, lsb, popcount)
 *   - Pre-computed attack tables (knights, kings, pawns)
 *   - Magic Bitboards for sliding pieces (bishop, rook, queen)
 *   - File and rank masks
 */

#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

/* ═══════════════════ File & Rank Masks ═══════════════════════ */

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_B_BB = FILE_A_BB << 1;
constexpr Bitboard FILE_C_BB = FILE_A_BB << 2;
constexpr Bitboard FILE_D_BB = FILE_A_BB << 3;
constexpr Bitboard FILE_E_BB = FILE_A_BB << 4;
constexpr Bitboard FILE_F_BB = FILE_A_BB << 5;
constexpr Bitboard FILE_G_BB = FILE_A_BB << 6;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;

constexpr Bitboard RANK_1_BB = 0x00000000000000FFULL;
constexpr Bitboard RANK_2_BB = RANK_1_BB << 8;
constexpr Bitboard RANK_3_BB = RANK_1_BB << 16;
constexpr Bitboard RANK_4_BB = RANK_1_BB << 24;
constexpr Bitboard RANK_5_BB = RANK_1_BB << 32;
constexpr Bitboard RANK_6_BB = RANK_1_BB << 40;
constexpr Bitboard RANK_7_BB = RANK_1_BB << 48;
constexpr Bitboard RANK_8_BB = RANK_1_BB << 56;

extern Bitboard FileBB[FILE_NB];
extern Bitboard RankBB[RANK_NB];

/* ═══════════════════ Bit Manipulation ════════════════════════ */

/* Return a bitboard with a single bit set at sq */
inline Bitboard square_bb(Square sq) {
    return 1ULL << sq;
}

/* Is the bit set at position sq? */
inline bool bit_is_set(Bitboard bb, Square sq) {
    return (bb >> sq) & 1;
}

/* Set bit */
inline void set_bit(Bitboard& bb, Square sq) {
    bb |= square_bb(sq);
}

/* Clear bit */
inline void clear_bit(Bitboard& bb, Square sq) {
    bb &= ~square_bb(sq);
}

/*
 * pop_lsb: clear and return the least significant set bit.
 * This is the most heavily used function in the engine.
 * __builtin_ctzll = count trailing zeros (GCC/Clang intrinsic)
 * Runs in a single instruction on most modern CPUs.
 */
inline Square lsb(Bitboard bb) {
    return Square(__builtin_ctzll(bb));
}

inline Square pop_lsb(Bitboard& bb) {
    Square sq = lsb(bb);
    bb &= bb - 1;   /* Clear LSB — popular bit twiddling trick */
    return sq;
}

/* Count set bits */
inline int popcount(Bitboard bb) {
    return __builtin_popcountll(bb);
}

/* ═══════════════════ Pre-computed Attack Tables ══════════════ */

/* Tables for knights, kings, and pawns:                         */
/*   KnightAttacks[sq] = bitboard for all squares knight attacks from sq */
/*   KingAttacks[sq]   = bitboard for all squares king attacks from sq  */
/*   PawnAttacks[color][sq] = bitboard for diagonal pawn captures     */

extern Bitboard KnightAttacks[SQUARE_NB];
extern Bitboard KingAttacks[SQUARE_NB];
extern Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];

/* ═══════════════════ Magic Bitboards ════════════════════════ */
/*                                                              */
/* For sliding pieces (bishop, rook, queen):                    */
/* Attacks depend on the blocking pieces (blockers) in the way. */
/*                                                              */
/* Magic Bitboards transform the blocker pattern into an index  */
/* in a lookup table:                                           */
/*   index = ((occupancy & mask) * magic) >> shift              */
/*   attacks = table[index]                                     */
/*                                                              */
/* This is very fast: one multiply + one shift + memory lookup. */

struct SMagic {
    Bitboard  mask;       /* Relevant blocker masks (ignoring edges) */
    Bitboard  magic;      /* Magic number                            */
    Bitboard* attacks;    /* Pointer to the attack table             */
    int       shift;      /* 64 - number of relevant bits            */
};

extern SMagic BishopMagics[SQUARE_NB];
extern SMagic RookMagics[SQUARE_NB];

/* ── Query sliding piece attacks ──────────────────────────── */

inline Bitboard bishop_attacks(Square sq, Bitboard occ) {
    const SMagic& m = BishopMagics[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}

inline Bitboard rook_attacks(Square sq, Bitboard occ) {
    const SMagic& m = RookMagics[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}

inline Bitboard queen_attacks(Square sq, Bitboard occ) {
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
}

/* ═══════════════════ Utility ════════════════════════════════ */

/* Shift a Bitboard to the North/South/East/West */
inline Bitboard shift_north(Bitboard bb)      { return bb << 8; }
inline Bitboard shift_south(Bitboard bb)      { return bb >> 8; }
inline Bitboard shift_east(Bitboard bb)       { return (bb << 1) & ~FILE_A_BB; }
inline Bitboard shift_west(Bitboard bb)       { return (bb >> 1) & ~FILE_H_BB; }
inline Bitboard shift_north_east(Bitboard bb) { return (bb << 9) & ~FILE_A_BB; }
inline Bitboard shift_north_west(Bitboard bb) { return (bb << 7) & ~FILE_H_BB; }
inline Bitboard shift_south_east(Bitboard bb) { return (bb >> 7) & ~FILE_A_BB; }
inline Bitboard shift_south_west(Bitboard bb) { return (bb >> 9) & ~FILE_H_BB; }

/* ═══════════════════ Initialization ═════════════════════════ */

/*
 * init_bitboards()
 * Called once at engine startup.
 * Computes all tables: rank/file masks, knight/king/pawn attacks,
 * and builds Magic Bitboards for bishops and rooks.
 */
void init_bitboards();

/* Print a bitboard for debugging/diagnostics */
void print_bitboard(Bitboard bb);

#endif /* BITBOARD_H */
