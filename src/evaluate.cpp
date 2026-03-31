/*
 * evaluate.cpp — Position Evaluation Function
 * ====================================
 *
 * Evaluation = Material + Piece-Square Tables + Additional Bonuses
 *
 * All values are in centipawns (100 = value of one pawn).
 *
 * Design:
 *   - calculate score as "score_white - score_black"
 *   - return multiplying by side: positive = good for current player
 *
 * PST Tables:
 *   Defined from White's perspective (A1 in bottom-left corner).
 *   For Black: we mirror the ranks vertically.
 *   This means PST index for White = sq ^ 0 (no change)
 *   And PST index for Black = sq ^ 56 (mirror ranks).
 *
 *   Why sq ^ 56?
 *   - XOR with 56 (= 0b111000) flips bits 3-5 (the rank)
 *   - A1(0) <-> A8(56), B1(1) <-> B8(57), etc.
 */

#include "evaluate.h"
#include "bitboard.h"

/* ═══════════════════ Piece-Square Tables ═══════════════════ */
/*
 * Each table = 64 values (A1=index 0, H8=index 63)
 * From White's perspective.
 *
 * Values are taken from classic engines and tweaked for balance.
 * Can be heavily optimized later using Texel Tuning.
 */

/* Pawn: Center and advancement are important */
static const int PAWN_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,   /* Rank 1 — no pawns here */
     5, 10, 10,-20,-20, 10, 10,  5,   /* Rank 2 — start */
     5, -5,-10,  0,  0,-10, -5,  5,   /* Rank 3 */
     0,  0,  0, 20, 20,  0,  0,  0,   /* Rank 4 — center */
     5,  5, 10, 25, 25, 10,  5,  5,   /* Rank 5 */
    10, 10, 20, 30, 30, 20, 10, 10,   /* Rank 6 */
    50, 50, 50, 50, 50, 50, 50, 50,   /* Rank 7 — close to promo */
     0,  0,  0,  0,  0,  0,  0,  0,   /* Rank 8 — no pawns here */
};

/* Knight: Center is excellent, edges are bad */
static const int KNIGHT_PST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

/* Bishop: Long diagonals are best */
static const int BISHOP_PST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

/* Rook: 7th rank (2nd for black) is highly prized */
static const int ROOK_PST[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      5, 10, 10, 10, 10, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0,
};

/* Queen: Avoid edges early */
static const int QUEEN_PST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};

/* King — Middlegame: stay tucked in corner */
static const int KING_MG_PST[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};

/* King — Endgame: center is better */
static const int KING_EG_PST[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50,
};

/* PST Dictionary */
static const int* PST[PIECE_TYPE_NB] = {
    nullptr,      /* NO_PIECE_TYPE */
    PAWN_PST,     /* PAWN   */
    KNIGHT_PST,   /* KNIGHT */
    BISHOP_PST,   /* BISHOP */
    ROOK_PST,     /* ROOK   */
    QUEEN_PST,    /* QUEEN  */
    KING_MG_PST,  /* KING (middlegame default) */
};

/* ═══════════════════ Mirror for Black ═════════════════════ */
/*
 * For black: flip the rank vertically.
 * sq ^ 56 flips bits 3-5 (the rank).
 * Example: A1(0) -> A8(56), E4(28) -> E5(36)
 */
static inline int mirror_sq(int sq) {
    return sq ^ 56;
}

/* ═══════════════════ Evaluation Function ═══════════════════ */

int evaluate(const Position& pos) {
    int score = 0;

    /* ─── 1. Material + PST ──────────────────────────── */
    /*
     * For each piece: add its dynamic value + positional bonus.
     * White adds score, Black subtracts score.
     */

    for (int pt = PAWN; pt <= KING; pt++) {
        PieceType piece_type = PieceType(pt);

        /* White pieces */
        Bitboard white_pieces = pos.pieces(WHITE, piece_type);
        while (white_pieces) {
            Square sq = pop_lsb(white_pieces);
            score += PieceValue[pt];
            if (PST[pt])
                score += PST[pt][sq];
        }

        /* Black pieces */
        Bitboard black_pieces = pos.pieces(BLACK, piece_type);
        while (black_pieces) {
            Square sq = pop_lsb(black_pieces);
            score -= PieceValue[pt];
            if (PST[pt])
                score -= PST[pt][mirror_sq(sq)];
        }
    }

    /* ─── 2. Bishop Pair Bonus ──────────────── */
    /*
     * Having two bishops is generally stronger than having just one
     * because they cover both color complexes entirely.
     * Bonus of 30 centipawns.
     */
    if (popcount(pos.pieces(WHITE, BISHOP)) >= 2) score += 30;
    if (popcount(pos.pieces(BLACK, BISHOP)) >= 2) score -= 30;

    /* ─── 3. Return from active player's perspective ────────────── */
    return (pos.side_to_move() == WHITE) ? score : -score;
}
