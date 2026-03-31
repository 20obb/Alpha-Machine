/*
 * move.h — 16-bit Move Encoding
 * ====================================
 *
 * Each move is encoded in a single uint16_t (16 bits):
 *
 *   bits 0-5:   Source square (from_sq)      — 64 possibilities
 *   bits 6-11:  Target square (to_sq)        — 64 possibilities
 *   bits 12-13: Promoted piece type          — 0=Knight, 1=Bishop, 2=Rook, 3=Queen
 *   bits 14-15: MoveType                     — Normal, Promotion, En Passant, Castling
 *
 * Why 16 bits?
 *   - Extremely memory-efficient (crucial for Transposition Tables and move lists)
 *   - Fast copying and comparison
 *   - Sufficient to encode all types of chess moves
 *
 * Note: Captured pieces are NOT encoded in the move itself.
 *   We extract the captured piece from the board during make_move.
 *   This saves bits and simplifies encoding.
 */

#ifndef MOVE_H
#define MOVE_H

#include "types.h"
#include <string>

/* ═══════════════════ Move Type ══════════════════════════════ */

typedef uint16_t Move;

constexpr Move MOVE_NONE = 0;
constexpr Move MOVE_NULL = 65;  /* Null move — structurally distinct from MOVE_NONE */

/* ═══════════════════ Move Construction ═════════════════════ */

/*
 * make_move: Construct a regular move (including normal captures)
 * make_move<MT>: Construct a special move (Promotions, En Passant, Castling)
 */

inline Move make_move(Square from, Square to) {
    return Move(from | (to << 6));
}

template<MoveType MT>
inline Move make_move(Square from, Square to, PieceType promo = KNIGHT) {
    return Move(from | (to << 6) | MT | ((promo - KNIGHT) << 12));
}

/* ═══════════════════ Move Extraction ══════════════════════ */

inline Square move_from(Move m) {
    return Square(m & 0x3F);
}

inline Square move_to(Move m) {
    return Square((m >> 6) & 0x3F);
}

inline MoveType move_type(Move m) {
    return MoveType(m & (3 << 14));
}

inline PieceType promotion_type(Move m) {
    return PieceType(((m >> 12) & 3) + KNIGHT);
}

/* ═══════════════════ Move Validation ═════════════════════ */

inline bool move_is_ok(Move m) {
    return m != MOVE_NONE && m != MOVE_NULL;
}

/* ═══════════════════ Move ↔ String ══════════════════════ */
/*
 * UCI format: "e2e4", "e7e8q" (promotion), "e1g1" (castling)
 */

inline std::string move_to_string(Move m) {
    if (m == MOVE_NONE) return "0000";
    if (m == MOVE_NULL) return "null";

    std::string str = square_to_string(move_from(m))
                    + square_to_string(move_to(m));

    if (move_type(m) == MT_PROMOTION) {
        constexpr const char promo_chars[] = "nbrq";
        str += promo_chars[promotion_type(m) - KNIGHT];
    }

    return str;
}

/* ═══════════════════ Move List ═══════════════════════════ */
/*
 * Fixed-size move list (max 256 moves in any position).
 * We avoid std::vector to prevent dynamic allocations during search.
 */

struct MoveList {
    Move moves[MAX_MOVES];
    int  scores[MAX_MOVES];  /* Used for move ordering later */
    int  count;

    MoveList() : count(0) {}

    void add(Move m) {
        moves[count] = m;
        scores[count] = 0;
        count++;
    }

    void add(Move m, int score) {
        moves[count] = m;
        scores[count] = score;
        count++;
    }

    void clear() { count = 0; }
};

#endif /* MOVE_H */
