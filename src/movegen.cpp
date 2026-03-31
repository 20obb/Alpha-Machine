/*
 * movegen.cpp — Pseudo-legal Move Generation
 * =============================================================
 *
 * This file generates all pseudo-legal moves.
 * "Pseudo-legal" means the move follows the piece's move rules,
 * but might leave the King in check. We filter out illegal moves
 * during the search using make_move() which tests legality.
 *
 * It is much faster to generate all pseudo-legal moves and discard
 * the illegal ones later, than to perfectly generate only legal ones.
 */

#include "movegen.h"
#include <cstdio>

/* ═══════════════════ Helper Functions ═════════════════════════ */

/* Add a standard quiet move or capture */
static inline void add_move(MoveList& list, Square from, Square to) {
    list.add(make_move(from, to));
}

/* Add pawn promotions (generates 4 moves: Q, R, B, N) */
static inline void add_promotions(MoveList& list, Square from, Square to) {
    list.add(make_move<MT_PROMOTION>(from, to, QUEEN));
    list.add(make_move<MT_PROMOTION>(from, to, ROOK));
    list.add(make_move<MT_PROMOTION>(from, to, BISHOP));
    list.add(make_move<MT_PROMOTION>(from, to, KNIGHT));
}

/* ═══════════════════ Pawn Moves ══════════════════════════════ */

template<Color Us>
static void generate_pawn_moves(const Position& pos, MoveList& list, Bitboard target) {
    constexpr Color Them = (Us == WHITE) ? BLACK : WHITE;
    constexpr Direction Up = (Us == WHITE) ? NORTH : SOUTH;
    constexpr Direction UpRight = (Us == WHITE) ? NORTH_EAST : SOUTH_EAST;
    constexpr Direction UpLeft = (Us == WHITE) ? NORTH_WEST : SOUTH_WEST;
    constexpr Rank PromotionRank = (Us == WHITE) ? RANK_8 : RANK_1;

    Bitboard pawns = pos.pieces(Us, PAWN);
    Bitboard empty = ~pos.all_pieces();
    Bitboard enemies = pos.pieces(Them);

    /* ── Single Push ── */
    Bitboard b1 = (Us == WHITE) ? shift_north(pawns) & empty
                                : shift_south(pawns) & empty;
    Bitboard single_pushes = b1 & target;
    while (single_pushes) {
        Square to = pop_lsb(single_pushes);
        Square from = to - Up;
        if (rank_of(to) == PromotionRank) {
            add_promotions(list, from, to);
        } else {
            add_move(list, from, to);
        }
    }

    /* ── Double Push ── */
    /* Must have been able to single push to Rank 3, and Rank 4 must be empty */
    Bitboard double_pushes = (Us == WHITE) ? shift_north(b1 & RankBB[RANK_3]) & empty
                                           : shift_south(b1 & RankBB[RANK_6]) & empty;
    double_pushes &= target;
    while (double_pushes) {
        Square to = pop_lsb(double_pushes);
        Square from = to - Up - Up;
        add_move(list, from, to);
    }

    /* ── Captures (Right/East) ── */
    Bitboard cap_right = (Us == WHITE) ? shift_north_east(pawns) & enemies
                                       : shift_south_east(pawns) & enemies;
    cap_right &= target;
    while (cap_right) {
        Square to = pop_lsb(cap_right);
        Square from = to - UpRight;
        if (rank_of(to) == PromotionRank) {
            add_promotions(list, from, to);
        } else {
            add_move(list, from, to);
        }
    }

    /* ── Captures (Left/West) ── */
    Bitboard cap_left = (Us == WHITE) ? shift_north_west(pawns) & enemies
                                      : shift_south_west(pawns) & enemies;
    cap_left &= target;
    while (cap_left) {
        Square to = pop_lsb(cap_left);
        Square from = to - UpLeft;
        if (rank_of(to) == PromotionRank) {
            add_promotions(list, from, to);
        } else {
            add_move(list, from, to);
        }
    }

    /* ── En Passant ── */
    Square ep_sq = pos.ep_square();
    if (ep_sq != NO_SQ && (square_bb(ep_sq) & target)) {
        Bitboard ep_pawns = PawnAttacks[Them][ep_sq] & pawns;
        while (ep_pawns) {
            Square from = pop_lsb(ep_pawns);
            list.add(make_move<MT_EN_PASSANT>(from, ep_sq));
        }
    }
}

/* ═══════════════════ Piece Moves ════════════════════════════ */

template<PieceType PT>
static void generate_piece_moves(const Position& pos, MoveList& list, Color us, Bitboard target) {
    Bitboard pieces = pos.pieces(us, PT);
    Bitboard occ = pos.all_pieces();

    while (pieces) {
        Square from = pop_lsb(pieces);
        Bitboard attacks = 0;

        if constexpr (PT == KNIGHT) attacks = KnightAttacks[from];
        else if constexpr (PT == BISHOP) attacks = bishop_attacks(from, occ);
        else if constexpr (PT == ROOK)   attacks = rook_attacks(from, occ);
        else if constexpr (PT == QUEEN)  attacks = queen_attacks(from, occ);
        else if constexpr (PT == KING)   attacks = KingAttacks[from];

        attacks &= target;

        while (attacks) {
            Square to = pop_lsb(attacks);
            add_move(list, from, to);
        }
    }
}

/* ═══════════════════ Castling ═══════════════════════════════ */

template<Color Us>
static void generate_castling(const Position& pos, MoveList& list) {
    if (pos.in_check()) return;

    CastlingRight cr = pos.castling_rights();
    Bitboard occ = pos.all_pieces();

    if constexpr (Us == WHITE) {
        if (cr & WHITE_OO) {
            /* f1 and g1 must be empty, and not attacked */
            if (!(occ & 0x60ULL) && !pos.is_attacked(F1, BLACK) && !pos.is_attacked(G1, BLACK)) {
                list.add(make_move<MT_CASTLING>(E1, G1));
            }
        }
        if (cr & WHITE_OOO) {
            /* b1, c1, d1 empty; c1, d1 not attacked */
            if (!(occ & 0xEULL) && !pos.is_attacked(D1, BLACK) && !pos.is_attacked(C1, BLACK)) {
                list.add(make_move<MT_CASTLING>(E1, C1));
            }
        }
    } else {
        if (cr & BLACK_OO) {
            /* f8 and g8 must be empty, and not attacked */
            if (!(occ & 0x6000000000000000ULL) && !pos.is_attacked(F8, WHITE) && !pos.is_attacked(G8, WHITE)) {
                list.add(make_move<MT_CASTLING>(E8, G8));
            }
        }
        if (cr & BLACK_OOO) {
            /* b8, c8, d8 empty; c8, d8 not attacked */
            if (!(occ & 0x0E00000000000000ULL) && !pos.is_attacked(D8, WHITE) && !pos.is_attacked(C8, WHITE)) {
                list.add(make_move<MT_CASTLING>(E8, C8));
            }
        }
    }
}

/* ═══════════════════ Main Generators ════════════════════════ */

void generate_moves(const Position& pos, MoveList& list) {
    list.clear();
    Color us = pos.side_to_move();

    /* Target: any square NOT occupied by our own pieces */
    Bitboard target = ~pos.pieces(us);

    if (us == WHITE) {
        generate_pawn_moves<WHITE>(pos, list, target);
        generate_castling<WHITE>(pos, list);
    } else {
        generate_pawn_moves<BLACK>(pos, list, target);
        generate_castling<BLACK>(pos, list);
    }

    generate_piece_moves<KNIGHT>(pos, list, us, target);
    generate_piece_moves<BISHOP>(pos, list, us, target);
    generate_piece_moves<ROOK>  (pos, list, us, target);
    generate_piece_moves<QUEEN> (pos, list, us, target);
    generate_piece_moves<KING>  (pos, list, us, target);
}

void generate_legal_moves(Position& pos, MoveList& list) {
    MoveList pseudo;
    generate_moves(pos, pseudo);

    list.clear();
    Color us = pos.side_to_move();

    for (int i = 0; i < pseudo.count; i++) {
        Move m = pseudo.moves[i];
        StateInfo si;
        pos.make_move(m, si);

        if (!pos.is_attacked(pos.king_square(us), ~us))
            list.add(m);

        pos.unmake_move(m);
    }
}

void generate_captures(const Position& pos, MoveList& list) {
    list.clear();
    Color us = pos.side_to_move();
    Color them = ~us;

    /* Target: ONLY squares occupied by enemy pieces */
    Bitboard target = pos.pieces(them);

    /* En passant square counts as enemy target for pawns */
    Bitboard pawn_target = target;
    if (pos.ep_square() != NO_SQ) pawn_target |= square_bb(pos.ep_square());

    if (us == WHITE) {
        generate_pawn_moves<WHITE>(pos, list, pawn_target);
    } else {
        generate_pawn_moves<BLACK>(pos, list, pawn_target);
    }

    /* Filter pawn pushes that got into the capture generation list */
    int j = 0;
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        if (pos.piece_on(move_to(m)) != NO_PIECE || move_type(m) == MT_EN_PASSANT || move_type(m) == MT_PROMOTION) {
            list.moves[j++] = m;
        }
    }
    list.count = j;

    generate_piece_moves<KNIGHT>(pos, list, us, target);
    generate_piece_moves<BISHOP>(pos, list, us, target);
    generate_piece_moves<ROOK>  (pos, list, us, target);
    generate_piece_moves<QUEEN> (pos, list, us, target);
    generate_piece_moves<KING>  (pos, list, us, target);
}

bool parse_legal_move(Position& pos, const std::string& text, Move& move_out) {
    MoveList legal_moves;
    generate_legal_moves(pos, legal_moves);

    for (int i = 0; i < legal_moves.count; i++) {
        if (move_to_string(legal_moves.moves[i]) == text) {
            move_out = legal_moves.moves[i];
            return true;
        }
    }

    move_out = MOVE_NONE;
    return false;
}

/* ═══════════════════ Perft ══════════════════════════════════ */
/*
 * perft (Performance Test): counts the number of valid leaf nodes up to 'depth'.
 * It strictly tests move generation and make_move precision.
 */

U64 perft(Position& pos, int depth) {
    if (depth == 0) return 1ULL;

    MoveList list;
    generate_moves(pos, list);

    U64 nodes = 0;
    Color us = pos.side_to_move();

    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        StateInfo si;
        pos.make_move(m, si);

        /* Is the move legal? (Did we leave our king in check?) */
        if (!pos.is_attacked(pos.king_square(us), ~us)) {
            nodes += perft(pos, depth - 1);
        }

        pos.unmake_move(m);
    }

    return nodes;
}

void perft_divide(Position& pos, int depth) {
    MoveList list;
    generate_moves(pos, list);

    U64 total_nodes = 0;
    Color us = pos.side_to_move();

    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        StateInfo si;
        pos.make_move(m, si);

        if (!pos.is_attacked(pos.king_square(us), ~us)) {
            U64 nodes = perft(pos, depth - 1);
            printf("%s: %llu\n", move_to_string(m).c_str(), (unsigned long long)nodes);
            total_nodes += nodes;
        }

        pos.unmake_move(m);
    }

    printf("\nTotal Nodes: %llu\n", (unsigned long long)total_nodes);
}
