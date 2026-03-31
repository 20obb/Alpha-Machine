/*
 * zobrist.h — Zobrist Hashing Keys
 * ====================================
 *
 * Zobrist Hashing: A technique to convert any chess position into
 * an almost unique 64-bit number.
 *
 * How it works:
 *   1. Assign a fixed 64-bit random number to each (piece x square) combination.
 *   2. Position key = XOR of all pieces + side to move + castling rights + en passant.
 *   3. When a piece is moved: key ^= old_piece_key ^ new_piece_key (O(1) incremental update).
 *
 * Why Zobrist?
 *   - Extremely fast (one XOR operation per change)
 *   - Negligible collision probability (1/2^64)
 *   - Crucial for Transposition Tables and repetition detection
 */

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace Zobrist {

/* Random keys for each (piece x square) combination */
extern U64 PieceKeys[PIECE_NB][SQUARE_NB];

/* Key to toggle side to move (XORed on every move) */
extern U64 SideKey;

/* Keys for castling rights (16 possible combinations) */
extern U64 CastlingKeys[CASTLING_NB];

/* Keys for en passant square (for each possible file) */
extern U64 EnPassantKeys[FILE_NB];

/* Initialize all keys (called once on startup) */
void init();

}  /* namespace Zobrist */

#endif /* ZOBRIST_H */
