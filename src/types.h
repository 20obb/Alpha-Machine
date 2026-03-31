/*
 * types.h — Core types and constants for the chess engine
 * =======================================================
 *
 * This file contains all fundamental definitions relied upon
 * by the rest of the files: piece types, squares, directions, castling, etc.
 *
 * Piece Design:
 *   Piece = (Color << 3) | PieceType
 *   W_PAWN=1, W_KNIGHT=2, ..., W_KING=6
 *   B_PAWN=9, B_KNIGHT=10, ..., B_KING=14
 *   color_of(p) = p >> 3     (very fast)
 *   type_of(p)  = p & 7      (very fast)
 *
 * Square Ordering: Little-Endian Rank-File (LERF)
 *   A1=0, B1=1, ..., H1=7
 *   A2=8, B2=9, ..., H2=15
 *   ...
 *   A8=56, B8=57, ..., H8=63
 */

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <string>

/* ───────────────────────── Core type ───────────────────────── */

typedef uint64_t U64;
typedef U64 Bitboard;

/* ───────────────────────── Squares ──────────────────────────── */

enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQ = 64,
    SQUARE_NB = 64
};

/* ───────────────────────── Colors ──────────────────────────── */

enum Color : int {
    WHITE, BLACK,
    COLOR_NB = 2
};

/* ───────────────────────── Piece types ─────────────────────── */

enum PieceType : int {
    NO_PIECE_TYPE = 0,
    PAWN = 1, KNIGHT = 2, BISHOP = 3,
    ROOK = 4, QUEEN = 5, KING = 6,
    PIECE_TYPE_NB = 7
};

/* ───────────────────────── Pieces ──────────────────────────── */
/*  Encoding: Piece = (Color << 3) | PieceType                   */
/*  The gap at index 7,8 is intentional for fast bit extraction   */

enum Piece : int {
    NO_PIECE = 0,
    W_PAWN = 1, W_KNIGHT = 2, W_BISHOP = 3,
    W_ROOK = 4, W_QUEEN  = 5, W_KING   = 6,
    /* 7, 8 unused */
    B_PAWN = 9, B_KNIGHT = 10, B_BISHOP = 11,
    B_ROOK = 12, B_QUEEN = 13, B_KING   = 14,
    PIECE_NB = 16
};

/* ───────────────────────── Files & Ranks ───────────────────── */

enum File : int {
    FILE_A, FILE_B, FILE_C, FILE_D,
    FILE_E, FILE_F, FILE_G, FILE_H,
    FILE_NB
};

enum Rank : int {
    RANK_1, RANK_2, RANK_3, RANK_4,
    RANK_5, RANK_6, RANK_7, RANK_8,
    RANK_NB
};

/* ───────────────────────── Directions ─────────────────────── */

enum Direction : int {
    NORTH =  8,  SOUTH = -8,
    EAST  =  1,  WEST  = -1,
    NORTH_EAST =  9, NORTH_WEST =  7,
    SOUTH_EAST = -7, SOUTH_WEST = -9,
    NORTH_NORTH = 16, SOUTH_SOUTH = -16
};

/* ───────────────────────── Castling rights ─────────────────── */
/*  4 bits: WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO          */

enum CastlingRight : int {
    NO_CASTLING    = 0,
    WHITE_OO       = 1,   /* White king-side  O-O   */
    WHITE_OOO      = 2,   /* White queen-side O-O-O */
    BLACK_OO       = 4,   /* Black king-side  O-O   */
    BLACK_OOO      = 8,   /* Black queen-side O-O-O */
    WHITE_CASTLING = WHITE_OO  | WHITE_OOO,   /* 3  */
    BLACK_CASTLING = BLACK_OO  | BLACK_OOO,   /* 12 */
    ALL_CASTLING   = WHITE_CASTLING | BLACK_CASTLING, /* 15 */
    CASTLING_NB    = 16
};

/* ─────────────────── Move type flags (bits 14-15) ─────────── */
/*  A move is encoded in 16 bits:                                 */
/*    bits 0-5:   from square                                     */
/*    bits 6-11:  to square                                       */
/*    bits 12-13: promotion piece type (0=knight,1=bishop,...)    */
/*    bits 14-15: move type (normal, promotion, en_passant, castle) */

enum MoveType : int {
    MT_NORMAL     = 0,
    MT_PROMOTION  = 1 << 14,
    MT_EN_PASSANT = 2 << 14,
    MT_CASTLING   = 3 << 14
};

/* ───────────────────── Search constants ───────────────────── */

constexpr int VALUE_INFINITE = 50000;
constexpr int VALUE_MATE     = 49000;
constexpr int VALUE_NONE     = 50001;
constexpr int MAX_DEPTH      = 64;
constexpr int MAX_MOVES      = 256;
constexpr int MAX_PLY         = 128;
constexpr int MAX_GAME_MOVES = 1024;

/* ───────────────────── Piece values (centipawns) ──────────── */

constexpr int PieceValue[PIECE_TYPE_NB] = {
    0,      /* NO_PIECE_TYPE */
    100,    /* PAWN   */
    320,    /* KNIGHT */
    330,    /* BISHOP */
    500,    /* ROOK   */
    900,    /* QUEEN  */
    20000   /* KING   */
};

/* ───────────────────── Starting FEN ───────────────────────── */

constexpr const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

/* ═══════════════════ Inline utility functions ═════════════════ */

inline Square   make_square(File f, Rank r)    { return Square(r * 8 + f); }
inline File     file_of(Square s)              { return File(s & 7); }
inline Rank     rank_of(Square s)              { return Rank(s >> 3); }

inline Piece    make_piece(Color c, PieceType pt) { return Piece((c << 3) | pt); }
inline Color    color_of(Piece p)              { return Color(p >> 3); }
inline PieceType type_of(Piece p)              { return PieceType(p & 7); }

inline Color    operator~(Color c)             { return Color(c ^ 1); }

inline Square   operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
inline Square   operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square&  operator+=(Square& s, Direction d) { return s = s + d; }
inline Square&  operator-=(Square& s, Direction d) { return s = s - d; }
inline Square&  operator++(Square& s)          { return s = Square(int(s) + 1); }

inline Direction operator*(int i, Direction d)  { return Direction(i * int(d)); }
inline Direction operator-(Direction d)         { return Direction(-int(d)); }

inline CastlingRight operator|(CastlingRight a, CastlingRight b) { return CastlingRight(int(a) | int(b)); }
inline CastlingRight operator&(CastlingRight a, CastlingRight b) { return CastlingRight(int(a) & int(b)); }
inline CastlingRight operator~(CastlingRight c) { return CastlingRight(~int(c) & 15); }
inline CastlingRight& operator|=(CastlingRight& a, CastlingRight b) { return a = a | b; }
inline CastlingRight& operator&=(CastlingRight& a, CastlingRight b) { return a = a & b; }

/* ─────────────────── Square ↔ String ──────────────────────── */

inline std::string square_to_string(Square s) {
    if (s == NO_SQ) return "-";
    return std::string(1, 'a' + file_of(s)) + std::string(1, '1' + rank_of(s));
}

inline Square string_to_square(const std::string& str) {
    if (str == "-" || str.length() < 2) return NO_SQ;
    int f = str[0] - 'a';
    int r = str[1] - '1';
    if (f < 0 || f > 7 || r < 0 || r > 7) return NO_SQ;
    return make_square(File(f), Rank(r));
}

/* ─────────────────── Piece ↔ Char (FEN) ──────────────────── */

inline char piece_to_char(Piece p) {
    /*  Index: 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 */
    /*  Char:  .  P  N  B  R  Q  K  .  .  p  n  b  r  q  k */
    constexpr const char table[] = ".PNBRQK..pnbrqk";
    return (p >= 0 && p <= 14) ? table[p] : '?';
}

inline Piece char_to_piece(char c) {
    constexpr const char table[] = "PNBRQKpnbrqk";
    constexpr Piece      map[]   = {
        W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
        B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING
    };
    for (int i = 0; i < 12; i++)
        if (table[i] == c) return map[i];
    return NO_PIECE;
}

/* ─────────────────── Castling ↔ Square helpers ───────────── */
/*  When a piece moves FROM or TO these squares, clear the     */
/*  corresponding castling rights.                              */

inline CastlingRight castling_rights_mask(Square sq) {
    /* For each square: which castling rights remain after a   */
    /* piece moves from/to that square.                         */
    /* Default: no rights are lost (mask = ALL_CASTLING).       */
    /* Only rook corners and king squares cause a loss.         */
    switch (sq) {
        case E1: return CastlingRight(ALL_CASTLING & ~WHITE_CASTLING);
        case A1: return CastlingRight(ALL_CASTLING & ~WHITE_OOO);
        case H1: return CastlingRight(ALL_CASTLING & ~WHITE_OO);
        case E8: return CastlingRight(ALL_CASTLING & ~BLACK_CASTLING);
        case A8: return CastlingRight(ALL_CASTLING & ~BLACK_OOO);
        case H8: return CastlingRight(ALL_CASTLING & ~BLACK_OO);
        default: return ALL_CASTLING;
    }
}

#endif /* TYPES_H */
