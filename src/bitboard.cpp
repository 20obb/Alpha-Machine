/*
 * bitboard.cpp — Attack tables and Magic Bitboards implementation
 * =====================================================
 *
 * Contains:
 *   1. File and rank tables
 *   2. Computation of knight, king, and pawn attacks (simple, one-time)
 *   3. Construction of Magic Bitboards for bishops and rooks
 *      - Calculation of blocker masks for each square
 *      - Generation of all possible blocker combinations
 *      - Calculation of actual attacks by ray casting
 *      - Finding a "magic number" that maps each combo to a unique index
 *   4. Print function for diagnostics
 */

#include "bitboard.h"
#include <cstdio>
#include <cstring>

/* ═══════════════════ Global Tables ══════════════════════════ */

Bitboard FileBB[FILE_NB];
Bitboard RankBB[RANK_NB];

Bitboard KnightAttacks[SQUARE_NB];
Bitboard KingAttacks[SQUARE_NB];
Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];

SMagic BishopMagics[SQUARE_NB];
SMagic RookMagics[SQUARE_NB];

/*
 * Tables to store sliding piece attacks.
 * The rook needs 4096 entries per square (max 12 relevant bits).
 * The bishop needs 512 entries per square (max 9 relevant bits).
 * Total: 64 * 4096 + 64 * 512 = 294,912 entries ≈ 2.3 MB.
 */
static Bitboard RookTable[64 * 4096];
static Bitboard BishopTable[64 * 512];

/* ═══════════════════ PRNG (Fixed Seed) ═════════════════════ */
/*
 * Pseudo-random number generator with a fixed seed.
 * Used to find magic numbers.
 * A fixed seed ensures the exact same magics are generated every run.
 */

static U64 rng_state = 1804289383ULL;

static U64 random_u64() {
    /* xorshift64 */
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* A random number with few set bits (sparse, good for magic numbers) */
static U64 random_sparse_u64() {
    return random_u64() & random_u64() & random_u64();
}

/* ═══════════════════ Ray Attack Computation ════════════════ */
/*
 * Calculate attacks for sliding pieces using ray casting.
 * This function is ONLY used during init to build the magic tables.
 * It is not used during the search — search uses the pre-computed tables.
 */

static Bitboard compute_rook_attacks(Square sq, Bitboard blockers) {
    Bitboard attacks = 0;
    int r = rank_of(sq), f = file_of(sq);
    int rr, ff;

    /* North */
    for (rr = r + 1; rr <= 7; rr++) {
        attacks |= (1ULL << (rr * 8 + f));
        if (blockers & (1ULL << (rr * 8 + f))) break;
    }
    /* South */
    for (rr = r - 1; rr >= 0; rr--) {
        attacks |= (1ULL << (rr * 8 + f));
        if (blockers & (1ULL << (rr * 8 + f))) break;
    }
    /* East */
    for (ff = f + 1; ff <= 7; ff++) {
        attacks |= (1ULL << (r * 8 + ff));
        if (blockers & (1ULL << (r * 8 + ff))) break;
    }
    /* West */
    for (ff = f - 1; ff >= 0; ff--) {
        attacks |= (1ULL << (r * 8 + ff));
        if (blockers & (1ULL << (r * 8 + ff))) break;
    }
    return attacks;
}

static Bitboard compute_bishop_attacks(Square sq, Bitboard blockers) {
    Bitboard attacks = 0;
    int r = rank_of(sq), f = file_of(sq);
    int rr, ff;

    /* North-East */
    for (rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++) {
        attacks |= (1ULL << (rr * 8 + ff));
        if (blockers & (1ULL << (rr * 8 + ff))) break;
    }
    /* North-West */
    for (rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--) {
        attacks |= (1ULL << (rr * 8 + ff));
        if (blockers & (1ULL << (rr * 8 + ff))) break;
    }
    /* South-East */
    for (rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++) {
        attacks |= (1ULL << (rr * 8 + ff));
        if (blockers & (1ULL << (rr * 8 + ff))) break;
    }
    /* South-West */
    for (rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--) {
        attacks |= (1ULL << (rr * 8 + ff));
        if (blockers & (1ULL << (rr * 8 + ff))) break;
    }
    return attacks;
}

/* ═══════════════════ Relevant Occupancy Masks ═════════════ */
/*
 * Relevant mask = squares that can potentially block the sliding piece.
 * We exclude the edges of the board because a piece on the edge
 * does not block anything further behind it anyway.
 * This greatly reduces the number of relevant bits and table size.
 */

static Bitboard rook_relevant_mask(Square sq) {
    Bitboard mask = 0;
    int r = rank_of(sq), f = file_of(sq);
    /* North (excluding the edge) */
    for (int rr = r + 1; rr <= 6; rr++) mask |= (1ULL << (rr * 8 + f));
    /* South */
    for (int rr = r - 1; rr >= 1; rr--) mask |= (1ULL << (rr * 8 + f));
    /* East */
    for (int ff = f + 1; ff <= 6; ff++) mask |= (1ULL << (r * 8 + ff));
    /* West */
    for (int ff = f - 1; ff >= 1; ff--) mask |= (1ULL << (r * 8 + ff));
    return mask;
}

static Bitboard bishop_relevant_mask(Square sq) {
    Bitboard mask = 0;
    int r = rank_of(sq), f = file_of(sq);
    for (int rr = r+1, ff = f+1; rr <= 6 && ff <= 6; rr++, ff++) mask |= (1ULL << (rr*8+ff));
    for (int rr = r+1, ff = f-1; rr <= 6 && ff >= 1; rr++, ff--) mask |= (1ULL << (rr*8+ff));
    for (int rr = r-1, ff = f+1; rr >= 1 && ff <= 6; rr--, ff++) mask |= (1ULL << (rr*8+ff));
    for (int rr = r-1, ff = f-1; rr >= 1 && ff >= 1; rr--, ff--) mask |= (1ULL << (rr*8+ff));
    return mask;
}

/* ═══════════════════ Subset Enumeration ════════════════════ */
/*
 * Convert an index (number) into a specific bitboard blocker combination
 * mapped onto the given mask using Carry-Rippler logic.
 *
 * Example: if the mask has 3 bits, we have 2^3 = 8 combinations.
 * index 0 = completely empty, index 7 = all bits blocked.
 */

static Bitboard index_to_occupancy(int index, int bits, Bitboard mask) {
    Bitboard occ = 0;
    for (int i = 0; i < bits; i++) {
        int bit_pos = __builtin_ctzll(mask);  /* Lowest bit in mask */
        mask &= mask - 1;                      /* Clear lowest bit */
        if (index & (1 << i))
            occ |= (1ULL << bit_pos);
    }
    return occ;
}

/* ═══════════════════ Magic Number Finding ═════════════════ */
/*
 * Find a "magic number" for a given square.
 *
 * Idea: we want to convert the occupancy blocker pattern into a unique index.
 *   index = (occupancy * magic) >> (64 - bits)
 *
 * The magic number distributes different patterns into different indices
 * without collisions.
 *
 * Algorithm: try random numbers until we find one with zero collisions.
 * With a fixed seed, this yields the same results every run.
 */

static U64 find_magic(Square sq, int bits, bool is_bishop) {
    Bitboard mask = is_bishop ? bishop_relevant_mask(sq)
                              : rook_relevant_mask(sq);
    int n = popcount(mask);
    int num_occupancies = 1 << n;

    /* Generate all possible blocker combinations and their attacks */
    Bitboard occupancies[4096];
    Bitboard attacks[4096];

    for (int i = 0; i < num_occupancies; i++) {
        occupancies[i] = index_to_occupancy(i, n, mask);
        attacks[i] = is_bishop ? compute_bishop_attacks(sq, occupancies[i])
                               : compute_rook_attacks(sq, occupancies[i]);
    }

    /* Search for the magic number */
    for (int attempt = 0; attempt < 100000000; attempt++) {
        U64 magic = random_sparse_u64();

        /* Quick check: does the number distribute the upper bits enough? */
        if (popcount((mask * magic) & 0xFF00000000000000ULL) < 6)
            continue;

        /* Collision test */
        Bitboard used[4096];
        memset(used, 0, sizeof(Bitboard) * (1 << bits));
        bool fail = false;

        for (int i = 0; i < num_occupancies; i++) {
            int idx = (int)((occupancies[i] * magic) >> (64 - bits));

            if (used[idx] == 0) {
                used[idx] = attacks[i];
            } else if (used[idx] != attacks[i]) {
                fail = true;
                break;
            }
        }

        if (!fail) return magic;
    }

    /* We should never reach here */
    printf("ERROR: Failed to find magic for square %d\n", sq);
    return 0;
}

/* ═══════════════════ Relevant Bits Per Square ═══════════ */

/* Number of relevant occupancy bits in the mask for the rook on each square */
static const int RookBits[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12,
};

/* Bishop's relevant occupancy bits per square */
static const int BishopBits[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6,
};

/* ═══════════════════ Init Functions ════════════════════════ */

static void init_file_rank_masks() {
    for (int f = 0; f < FILE_NB; f++)
        FileBB[f] = FILE_A_BB << f;
    for (int r = 0; r < RANK_NB; r++)
        RankBB[r] = RANK_1_BB << (r * 8);
}

static void init_knight_attacks() {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard bb = square_bb(Square(sq));
        Bitboard attacks = 0;

        /* 8 possible knight jumps */
        if (bb << 17 & ~FILE_A_BB) attacks |= bb << 17;  /* NNE */
        if (bb << 15 & ~FILE_H_BB) attacks |= bb << 15;  /* NNW */
        if (bb << 10 & ~(FILE_A_BB | FILE_B_BB)) attacks |= bb << 10;  /* NEE */
        if (bb <<  6 & ~(FILE_G_BB | FILE_H_BB)) attacks |= bb <<  6;  /* NWW */
        if (bb >> 17 & ~FILE_H_BB) attacks |= bb >> 17;  /* SSW */
        if (bb >> 15 & ~FILE_A_BB) attacks |= bb >> 15;  /* SSE */
        if (bb >> 10 & ~(FILE_G_BB | FILE_H_BB)) attacks |= bb >> 10;  /* SWW */
        if (bb >>  6 & ~(FILE_A_BB | FILE_B_BB)) attacks |= bb >>  6;  /* SEE */

        KnightAttacks[sq] = attacks;
    }
}

static void init_king_attacks() {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard bb = square_bb(Square(sq));
        Bitboard attacks = 0;

        attacks |= shift_north(bb);
        attacks |= shift_south(bb);
        attacks |= shift_east(bb);
        attacks |= shift_west(bb);
        attacks |= shift_north_east(bb);
        attacks |= shift_north_west(bb);
        attacks |= shift_south_east(bb);
        attacks |= shift_south_west(bb);

        KingAttacks[sq] = attacks;
    }
}

static void init_pawn_attacks() {
    for (int sq = 0; sq < 64; sq++) {
        Bitboard bb = square_bb(Square(sq));

        /* White attacks upward (North-East + North-West) */
        PawnAttacks[WHITE][sq] = shift_north_east(bb) | shift_north_west(bb);

        /* Black attacks downward (South-East + South-West) */
        PawnAttacks[BLACK][sq] = shift_south_east(bb) | shift_south_west(bb);
    }
}

static void init_slider_magics(bool is_bishop) {
    SMagic*   magics = is_bishop ? BishopMagics  : RookMagics;
    Bitboard* table  = is_bishop ? BishopTable   : RookTable;
    const int* bits  = is_bishop ? BishopBits    : RookBits;
    int table_size   = is_bishop ? 512           : 4096;

    int offset = 0;

    for (int sq = 0; sq < 64; sq++) {
        SMagic& m = magics[sq];

        m.mask    = is_bishop ? bishop_relevant_mask(Square(sq))
                              : rook_relevant_mask(Square(sq));
        m.shift   = 64 - bits[sq];
        m.attacks = &table[offset];

        /* Find the magic number */
        m.magic = find_magic(Square(sq), bits[sq], is_bishop);

        /* Populate the attack table */
        int n = popcount(m.mask);
        int num_occupancies = 1 << n;

        for (int i = 0; i < num_occupancies; i++) {
            Bitboard occ = index_to_occupancy(i, n, m.mask);
            int idx = (int)((occ * m.magic) >> m.shift);
            m.attacks[idx] = is_bishop ? compute_bishop_attacks(Square(sq), occ)
                                       : compute_rook_attacks(Square(sq), occ);
        }

        offset += table_size;
    }
}

/* ═══════════════════ Public Init ════════════════════════════ */

void init_bitboards() {
    init_file_rank_masks();
    init_knight_attacks();
    init_king_attacks();
    init_pawn_attacks();

    /* Magic Bitboards — takes ~50ms during startup */
    rng_state = 1804289383ULL;   /* Reset seed for reproducibility */
    init_slider_magics(true);    /* Bishop */
    init_slider_magics(false);   /* Rook */
}

/* ═══════════════════ Debug ══════════════════════════════════ */

void print_bitboard(Bitboard bb) {
    printf("\n");
    for (int r = 7; r >= 0; r--) {
        printf("  %d  ", r + 1);
        for (int f = 0; f < 8; f++) {
            int sq = r * 8 + f;
            printf(" %c", bit_is_set(bb, Square(sq)) ? '#' : '.');
        }
        printf("\n");
    }
    printf("\n      a b c d e f g h\n");
    printf("\n  Bitboard: 0x%016llx (%d bits)\n\n",
           (unsigned long long)bb, popcount(bb));
}
