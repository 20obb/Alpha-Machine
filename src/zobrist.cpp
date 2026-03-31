/*
 * zobrist.cpp — Initialization of Zobrist keys
 */

#include "zobrist.h"
#include <cstdint>

namespace Zobrist {

U64 PieceKeys[PIECE_NB][SQUARE_NB];
U64 SideKey;
U64 CastlingKeys[CASTLING_NB];
U64 EnPassantKeys[FILE_NB];

/*
 * PRNG with a fixed seed, distinct from the PRNG used for magics.
 * This guarantees consistent Zobrist keys across different runs.
 */
static U64 zobrist_rng = 7364928103247ULL;

static U64 zobrist_random() {
    /* xorshift64* — high quality */
    zobrist_rng ^= zobrist_rng >> 12;
    zobrist_rng ^= zobrist_rng << 25;
    zobrist_rng ^= zobrist_rng >> 27;
    return zobrist_rng * 0x2545F4914F6CDD1DULL;
}

void init() {
    zobrist_rng = 7364928103247ULL;  /* Reset seed for reproducibility */

    /* Piece keys */
    for (int p = 0; p < PIECE_NB; p++)
        for (int sq = 0; sq < SQUARE_NB; sq++)
            PieceKeys[p][sq] = zobrist_random();

    /* Side to move key */
    SideKey = zobrist_random();

    /* Castling keys */
    for (int i = 0; i < CASTLING_NB; i++)
        CastlingKeys[i] = zobrist_random();

    /* En passant keys (per file) */
    for (int f = 0; f < FILE_NB; f++)
        EnPassantKeys[f] = zobrist_random();
}

}  /* namespace Zobrist */
