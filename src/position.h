/*
 * position.h — Complete Position State (Board + Variables)
 * =============================================================
 *
 * This class represents a full chess position:
 *   - 12 Bitboards (one for each piece type / color combination)
 *   - 3 Occupancy bitboards (White, Black, All)
 *   - Mailbox[64] array for fast direct access: "What piece is on square X?"
 *   - Game state: side to move, castling rights, en passant, move counters
 *   - Zobrist hashing key
 *   - History stack for move retraction (unmake_move)
 *
 * Design:
 *   We use both Bitboards (for fast search) and a mailbox (for direct access).
 *   Both are updated simultaneously in make_move/unmake_move.
 */

#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include "bitboard.h"
#include "move.h"
#include "zobrist.h"

#include <string>

/* ═══════════════════ State Snapshot ═════════════════════════ */
/*
 * State snapshot saved before executing a move.
 * Stored in a history stack to revert the position using unmake_move.
 *
 * Why is this needed?
 *   - Some information cannot be deduced from just reversing the move:
 *     castling rights, en passant square, halfmove clock.
 *   - Saving them here is faster than recalculating.
 */
struct StateInfo {
    /* Information that cannot be inferred by unmaking the move */
    CastlingRight castling;
    Square        en_passant;
    int           halfmove_clock;    /* 50-move rule counter */
    int           fullmove_number;

    /* The captured piece (to restore it during unmake) */
    Piece         captured;

    /* Zobrist key before the move */
    U64           zobrist_key;

    /* The move that led to this state */
    Move          last_move;

    /* Pointer to the previous state (linked list) */
    StateInfo*    previous;
};

/* ═══════════════════ Position Class ════════════════════════ */

class Position {
public:
    /* ── Construction and Setup ──────────────────────────── */

    Position();

    /* Setup position from FEN string */
    void set_fen(const std::string& fen);

    /* Get current FEN string */
    std::string get_fen() const;

    /* Setup starting position */
    void set_startpos();

    /* ── Data Access ──────────────────────────────────────── */

    /* Bitboard for a specific piece type (both colors) */
    Bitboard pieces(PieceType pt) const;

    /* Bitboard for a specific color and piece type */
    Bitboard pieces(Color c, PieceType pt) const;

    /* Bitboard for all pieces of a specific color */
    Bitboard pieces(Color c) const;

    /* All pieces on the board */
    Bitboard all_pieces() const;

    /* Get piece on a given square */
    Piece piece_on(Square sq) const;

    /* King square for a specific color */
    Square king_square(Color c) const;

    /* Whose turn is it? */
    Color side_to_move() const;

    /* Current castling rights */
    CastlingRight castling_rights() const;

    /* Current en passant square */
    Square ep_square() const;

    /* Current Zobrist key */
    U64 key() const;

    /* Move counters */
    int halfmove_clock() const;
    int fullmove_number() const;

    /* ── Attacks ─────────────────────────────────────────── */

    /* Is the given square attacked by the specified color? */
    bool is_attacked(Square sq, Color by) const;

    /* Is the current side to move in check? */
    bool in_check() const;

    /* Bitboard of all attacks directed to a specific square */
    Bitboard attackers_to(Square sq, Bitboard occ) const;

    /* ── Make / Unmake Moves ─────────────────────────────── */

    /* Execute a move — saves state to si */
    void make_move(Move m, StateInfo& si);

    /* Revert the last move executed */
    void unmake_move(Move m);

    /* Null move (passing the turn — used in search heuristics) */
    void make_null_move(StateInfo& si);
    void unmake_null_move();

    /* ── Repetition and Draws ────────────────────────────── */

    /* Has the position occurred before? */
    bool is_repetition() const;

    /* Have we reached the 50-move draw limit? */
    bool is_fifty_move_draw() const;

    /* Is checkmate impossible due to insufficient material? */
    bool is_material_draw() const;

    /* ── Display ─────────────────────────────────────────── */

    void print() const;

    /* ── Recalculate Zobrist key entirely (for testing) ─── */
    U64 compute_key() const;

private:
    /* ── Internal Data ────────────────────────────────────── */

    /* Bitboards: one for each (color x piece type) */
    Bitboard piece_bb[COLOR_NB][PIECE_TYPE_NB];

    /* Occupancy bitboards */
    Bitboard occupied_bb[COLOR_NB];  /* Pieces of each color */
    Bitboard all_bb;                  /* All active pieces */

    /* Mailbox array for O(1) piece lookup on a square */
    Piece board[SQUARE_NB];

    /* King squares (cached for performance) */
    Square king_sq[COLOR_NB];

    /* Side to move */
    Color side;

    /* Castling rights */
    CastlingRight castling;

    /* En passant square */
    Square ep_sq;

    /* Turn counters */
    int halfmove;
    int fullmove;

    /* Current Zobrist hash key */
    U64 zobrist;

    /* Pointer to the current state (acts as history stack) */
    StateInfo* state;

    /* Array of past Zobrist keys to detect repetition */
    U64 history_keys[MAX_GAME_MOVES];
    int history_count;

    /* ── Internal helper functions ───────────────────────── */

    /* Add / remove pieces (updates all internal representations simultaneously) */
    void put_piece(Piece p, Square sq);
    void remove_piece(Square sq);
    void move_piece(Square from, Square to);
};

/* ═══════════════════ Inline Implementations ════════════════ */

inline Bitboard Position::pieces(PieceType pt) const {
    return piece_bb[WHITE][pt] | piece_bb[BLACK][pt];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const {
    return piece_bb[c][pt];
}

inline Bitboard Position::pieces(Color c) const {
    return occupied_bb[c];
}

inline Bitboard Position::all_pieces() const {
    return all_bb;
}

inline Piece Position::piece_on(Square sq) const {
    return board[sq];
}

inline Square Position::king_square(Color c) const {
    return king_sq[c];
}

inline Color Position::side_to_move() const {
    return side;
}

inline CastlingRight Position::castling_rights() const {
    return castling;
}

inline Square Position::ep_square() const {
    return ep_sq;
}

inline U64 Position::key() const {
    return zobrist;
}

inline int Position::halfmove_clock() const {
    return halfmove;
}

inline int Position::fullmove_number() const {
    return fullmove;
}

inline bool Position::in_check() const {
    return is_attacked(king_sq[side], ~side);
}

inline bool Position::is_fifty_move_draw() const {
    return halfmove >= 100;
}

#endif /* POSITION_H */
