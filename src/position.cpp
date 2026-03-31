/*
 * position.cpp — Position Implementation: FEN, make_move, unmake_move
 * ===================================================================
 *
 * This file contains the most critical functions of the engine:
 *   1. set_fen: Parsing a setup from a FEN string
 *   2. get_fen: Generating a FEN string representing the current state
 *   3. make_move: Executing a move (updating all internal structures)
 *   4. unmake_move: Reverting a move (restoring the previous state)
 *   5. is_attacked: Checking if a square is under attack
 *   6. is_repetition: Draw detection via repetition
 *
 * ⚠️ This file is extremely sensitive to bugs.
 *    Any bug in make_move/unmake_move will cause erratic search behavior.
 *    Modifications must be thoroughly verified using targeted perft tests.
 */

#include "position.h"
#include <cstring>
#include <cstdio>
#include <sstream>

/* ═══════════════════ Constructor ════════════════════════════ */

Position::Position() {
    memset(piece_bb, 0, sizeof(piece_bb));
    memset(occupied_bb, 0, sizeof(occupied_bb));
    all_bb = 0;
    for (int sq = 0; sq < SQUARE_NB; sq++)
        board[sq] = NO_PIECE;
    king_sq[WHITE] = king_sq[BLACK] = NO_SQ;
    side = WHITE;
    castling = NO_CASTLING;
    ep_sq = NO_SQ;
    halfmove = 0;
    fullmove = 1;
    zobrist = 0;
    state_count = 0;
    history_count = 0;
}

/* ═══════════════════ Piece Management ═════════════════════ */
/*
 * These three functions are foundational: every board change goes through them.
 * They guarantee that the Bitboards, the mailbox, and the Zobrist key
 * remain fully consistent at all times.
 */

void Position::put_piece(Piece p, Square sq) {
    Color c = color_of(p);
    PieceType pt = type_of(p);

    board[sq] = p;
    set_bit(piece_bb[c][pt], sq);
    set_bit(occupied_bb[c], sq);
    set_bit(all_bb, sq);

    if (pt == KING) king_sq[c] = sq;

    zobrist ^= Zobrist::PieceKeys[p][sq];
}

void Position::remove_piece(Square sq) {
    Piece p = board[sq];
    if (p == NO_PIECE) return;

    Color c = color_of(p);
    PieceType pt = type_of(p);

    board[sq] = NO_PIECE;
    clear_bit(piece_bb[c][pt], sq);
    clear_bit(occupied_bb[c], sq);
    clear_bit(all_bb, sq);

    zobrist ^= Zobrist::PieceKeys[p][sq];
}

void Position::move_piece(Square from, Square to) {
    Piece p = board[from];
    Color c = color_of(p);
    PieceType pt = type_of(p);

    /* Remove piece from source square */
    board[from] = NO_PIECE;
    clear_bit(piece_bb[c][pt], from);
    clear_bit(occupied_bb[c], from);
    clear_bit(all_bb, from);

    /* Place piece on destination square */
    board[to] = p;
    set_bit(piece_bb[c][pt], to);
    set_bit(occupied_bb[c], to);
    set_bit(all_bb, to);

    if (pt == KING) king_sq[c] = to;

    /* Update Zobrist Key: XOR both the old and new square positions */
    zobrist ^= Zobrist::PieceKeys[p][from] ^ Zobrist::PieceKeys[p][to];
}

/* ═══════════════════ FEN Parsing ═════════════════════════ */
/*
 * FEN (Forsyth-Edwards Notation): A standard text format to describe a chess position.
 * Example: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
 *
 * 6 parts:
 *   1. Piece placement (from Rank 8 to Rank 1, delimited by '/')
 *   2. Active color: 'w' or 'b'
 *   3. Castling availability: 'KQkq' or '-'
 *   4. En passant target square: e.g., 'e3' or '-'
 *   5. Halfmove clock
 *   6. Fullmove number
 */

void Position::set_fen(const std::string& fen) {
    /* Reset all structural fields to defaults */
    memset(piece_bb, 0, sizeof(piece_bb));
    memset(occupied_bb, 0, sizeof(occupied_bb));
    all_bb = 0;
    for (int sq = 0; sq < SQUARE_NB; sq++)
        board[sq] = NO_PIECE;
    king_sq[WHITE] = king_sq[BLACK] = NO_SQ;
    zobrist = 0;
    state_count = 0;
    history_count = 0;

    std::istringstream ss(fen);
    std::string token;

    /* ─── Part 1: Piece Placement ──────────────────────── */
    ss >> token;
    int sq = A8;  /* We start scanning from A8 (Top-Left corner) */

    for (char c : token) {
        if (c == '/') {
            sq -= 16;  /* Drop down one rank: transitioning from Hx (end of rank) to Ax-1 (start of prev rank) involves -16 offset. */
        } else if (c >= '1' && c <= '8') {
            sq += (c - '0');  /* Advance by the count of empty squares */
        } else {
            Piece p = char_to_piece(c);
            if (p != NO_PIECE) {
                put_piece(p, Square(sq));
                sq++;
            }
        }
    }

    /* ─── Part 2: Active Color ─────────────────────────── */
    ss >> token;
    side = (token == "b") ? BLACK : WHITE;
    if (side == BLACK) zobrist ^= Zobrist::SideKey;

    /* ─── Part 3: Castling Rights ──────────────────────── */
    ss >> token;
    castling = NO_CASTLING;
    for (char c : token) {
        switch (c) {
            case 'K': castling |= WHITE_OO;  break;
            case 'Q': castling |= WHITE_OOO; break;
            case 'k': castling |= BLACK_OO;  break;
            case 'q': castling |= BLACK_OOO; break;
            default: break;
        }
    }
    zobrist ^= Zobrist::CastlingKeys[castling];

    /* ─── Part 4: En Passant Square ────────────────────── */
    ss >> token;
    ep_sq = string_to_square(token);
    if (ep_sq != NO_SQ) {
        zobrist ^= Zobrist::EnPassantKeys[file_of(ep_sq)];
    }

    /* ─── Part 5 & 6: Counters ─────────────────────────── */
    ss >> halfmove >> fullmove;
}

/* ═══════════════════ FEN Output ═════════════════════════ */

std::string Position::get_fen() const {
    std::string fen;

    /* Piece Placement */
    for (int r = RANK_8; r >= RANK_1; r--) {
        int empty = 0;
        for (int f = FILE_A; f <= FILE_H; f++) {
            Piece p = board[make_square(File(f), Rank(r))];
            if (p == NO_PIECE) {
                empty++;
            } else {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                fen += piece_to_char(p);
            }
        }
        if (empty > 0) fen += std::to_string(empty);
        if (r > 0) fen += '/';
    }

    /* Active Color */
    fen += (side == WHITE) ? " w " : " b ";

    /* Castling Rights */
    if (castling == NO_CASTLING) {
        fen += '-';
    } else {
        if (castling & WHITE_OO)  fen += 'K';
        if (castling & WHITE_OOO) fen += 'Q';
        if (castling & BLACK_OO)  fen += 'k';
        if (castling & BLACK_OOO) fen += 'q';
    }

    /* En Passant Square */
    fen += ' ';
    fen += (ep_sq != NO_SQ) ? square_to_string(ep_sq) : "-";

    /* Move Counters */
    fen += " ";
    fen += std::to_string(halfmove);
    fen += " ";
    fen += std::to_string(fullmove);

    return fen;
}

/* ═══════════════════ Setup Starting Position ═════════════ */

void Position::set_startpos() {
    set_fen(START_FEN);
}

/* ═══════════════════ Attack Detection ═══════════════════ */
/*
 * is_attacked: Is the target square 'sq' attacked by the specified color 'by'?
 *
 * The logic strategy:
 *   Instead of branching out from 'by' pieces, we perform reverse lookups.
 *   "Reverse Vision" mechanism:
 *   - Assume we place a Knight on 'sq'. Does its attack pattern intersect an enemy Knight? -> sq is attacked by a Knight.
 *   - Assume a Bishop on 'sq'. Do its sliding rays intersect an enemy Bishop or Queen? -> sq is attacked diagonally.
 *   - And so on for all active piece types.
 */

Bitboard Position::attackers_to(Square sq, Bitboard occ) const {
    return (PawnAttacks[WHITE][sq]  & piece_bb[BLACK][PAWN])
         | (PawnAttacks[BLACK][sq]  & piece_bb[WHITE][PAWN])
         | (KnightAttacks[sq]      & pieces(KNIGHT))
         | (KingAttacks[sq]        & pieces(KING))
         | (bishop_attacks(sq, occ) & (pieces(BISHOP) | pieces(QUEEN)))
         | (rook_attacks(sq, occ)   & (pieces(ROOK)   | pieces(QUEEN)));
}

bool Position::is_attacked(Square sq, Color by) const {
    /* Perform fast verifications ordered from computationally cheapest to most expensive */

    if (PawnAttacks[~by][sq] & piece_bb[by][PAWN])  return true;
    if (KnightAttacks[sq]    & piece_bb[by][KNIGHT]) return true;
    if (KingAttacks[sq]      & piece_bb[by][KING])   return true;

    Bitboard occ = all_bb;
    if (bishop_attacks(sq, occ) & (piece_bb[by][BISHOP] | piece_bb[by][QUEEN])) return true;
    if (rook_attacks(sq, occ)   & (piece_bb[by][ROOK]   | piece_bb[by][QUEEN])) return true;

    return false;
}

/* ═══════════════════ Make Move ═══════════════════════════ */
/*
 * Apply a move and update all surrounding internal structural state variables.
 *
 * ⚠️ This function is incredibly delicate.
 *    Any misstep here translates to random, hard-to-trace bugs in node search.
 *
 * Execution flow:
 *   1. Save the active state snapshot (required for subsequent retraction)
 *   2. Process based on specialized move types: castling, en passant, promotion
 *   3. Relocate the piece footprint
 *   4. Update ongoing castling rights
 *   5. Update en passant vulnerability
 *   6. Update game state clocks (Halfmove & Fullmove)
 *   7. Switch turn
 *   8. Append resultant Zobrist key to execution history log (Repetition detection)
 */

void Position::make_move(Move m, StateInfo& si) {
    StateInfo snapshot;

    /* ─── 1. Save The Active State ────────────────────── */
    snapshot.castling       = castling;
    snapshot.en_passant     = ep_sq;
    snapshot.halfmove_clock = halfmove;
    snapshot.fullmove_number = fullmove;
    snapshot.captured       = NO_PIECE;
    snapshot.zobrist_key    = zobrist;
    snapshot.last_move      = m;

    Square from = move_from(m);
    Square to   = move_to(m);
    MoveType mt = move_type(m);
    Piece moved = board[from];
    PieceType pt = type_of(moved);

    /* ─── Prior en passant: Remove from Zobrist hash ──── */
    if (ep_sq != NO_SQ) {
        zobrist ^= Zobrist::EnPassantKeys[file_of(ep_sq)];
        ep_sq = NO_SQ;
    }

    /* ─── 2. Handle move based on its core type ───────── */

    if (mt == MT_CASTLING) {
        /*
         * Castling processing: King shifts, then Rook shifts.
         * Note: While the UCI standard parses moves as "e1g1" (just King moving 2 spots),
         *       structurally both the King and the Rook traverse squares.
         */
        Square rook_from, rook_to;

        if (to > from) {
            /* King-side: King e1→g1, Rook h1→f1 */
            rook_from = Square(from + 3);   /* h1 or h8 */
            rook_to   = Square(from + 1);   /* f1 or f8 */
        } else {
            /* Queen-side: King e1→c1, Rook a1→d1 */
            rook_from = Square(from - 4);   /* a1 or a8 */
            rook_to   = Square(from - 1);   /* d1 or d8 */
        }

        move_piece(from, to);         /* Move the King */
        move_piece(rook_from, rook_to); /* Move the corresponding Rook */

        halfmove++;

    } else if (mt == MT_EN_PASSANT) {
        /*
         * En Passant Capture: The pawn being actively captured is NOT on the target square!
        * Rather, it resides one rank directly behind the target (relative to the moving player).
         */
        Square cap_sq = (side == WHITE) ? Square(to - 8) : Square(to + 8);
        snapshot.captured = board[cap_sq];
        remove_piece(cap_sq);
        move_piece(from, to);
        halfmove = 0;  /* A pawn capture → reset halfmove clock */

    } else if (mt == MT_PROMOTION) {
        /*
         * Promotion: Destroy the originating Pawn and spawn the requested piece.
         */
        Piece promo_piece = make_piece(side, promotion_type(m));

        /* Target square capture verification */
        if (board[to] != NO_PIECE) {
            snapshot.captured = board[to];
            remove_piece(to);
        }

        remove_piece(from);         /* Clear the originating pawn */
        put_piece(promo_piece, to); /* Spawn the promoted piece */
        halfmove = 0;

    } else {
        /* ─── Normal Move Execution ───────────────────────── */

        /* Target square capture logic */
        if (board[to] != NO_PIECE) {
            snapshot.captured = board[to];
            remove_piece(to);
            halfmove = 0;
        } else {
            halfmove++;
        }

        move_piece(from, to);

        /* Pawn advances: Halfmove clock is universally reset */
        if (pt == PAWN) {
            halfmove = 0;

            /* If pawn advances exactly two steps → Define the en-passant square */
            if (  (side == WHITE && rank_of(from) == RANK_2 && rank_of(to) == RANK_4)
               || (side == BLACK && rank_of(from) == RANK_7 && rank_of(to) == RANK_5))
            {
                ep_sq = Square((from + to) / 2);  /* Locate intermediate traversed square */
                zobrist ^= Zobrist::EnPassantKeys[file_of(ep_sq)];
            }
        }
    }

    /* ─── 3. Update active Castling Rights ───────────────── */
    /*
     * Castling rights mutate only when the King or Rook steps off their origin, or a Rook is captured.
     * The mask functionally masks out and nullifies specific corners corresponding to squares modified.
     */
    CastlingRight old_castling = castling;
    castling &= castling_rights_mask(from);
    castling &= castling_rights_mask(to);

    if (castling != old_castling) {
        zobrist ^= Zobrist::CastlingKeys[old_castling];
        zobrist ^= Zobrist::CastlingKeys[castling];
    }

    /* ─── 4. Advance Fullmove Iterator ───────────────────── */
    if (side == BLACK) fullmove++;

    /* ─── 5. Shift Turn Control ──────────────────────────── */
    side = ~side;
    zobrist ^= Zobrist::SideKey;

    /* ─── 6. Catalog Zobrist key into History Database ──── */
    if (history_count < MAX_GAME_MOVES)
        history_keys[history_count++] = zobrist;

    if (state_count < MAX_GAME_MOVES)
        state_stack[state_count++] = snapshot;

    si = snapshot;
}

/* ═══════════════════ Unmake Move ═════════════════════════ */
/*
 * Retract a move. Completely unwinds the active state to the exact snapshot recorded right before instantiation.
 *
 * Golden Rule: After invoking unmake_move sequentially, absolutely every internal data structure
 * MUST mirror the condition it was in right before make_move was enacted.
 */

void Position::unmake_move(Move m) {
    if (state_count <= 0)
        return;

    StateInfo& prev = state_stack[state_count - 1];

    /* Step backwards within the History Log */
    history_count--;

    /* Swap the turn back (inverse of the make_move shift) */
    side = ~side;

    Square from = move_from(m);
    Square to   = move_to(m);
    MoveType mt = move_type(m);

    if (mt == MT_CASTLING) {
        Square rook_from, rook_to;
        if (to > from) {
            rook_from = Square(from + 3);
            rook_to   = Square(from + 1);
        } else {
            rook_from = Square(from - 4);
            rook_to   = Square(from - 1);
        }
        move_piece(to, from);             /* De-relocate the King */
        move_piece(rook_to, rook_from);   /* De-relocate the corresponding Rook */

    } else if (mt == MT_EN_PASSANT) {
        Square cap_sq = (side == WHITE) ? Square(to - 8) : Square(to + 8);
        move_piece(to, from);
        put_piece(prev.captured, cap_sq);

    } else if (mt == MT_PROMOTION) {
        remove_piece(to);
        put_piece(make_piece(side, PAWN), from);
        if (prev.captured != NO_PIECE) {
            put_piece(prev.captured, to);
        }

    } else {
        move_piece(to, from);
        if (prev.captured != NO_PIECE) {
            put_piece(prev.captured, to);
        }
    }

    /* Restore all scalar variables from the pre-captured snapshot */
    castling = prev.castling;
    ep_sq    = prev.en_passant;
    halfmove = prev.halfmove_clock;
    fullmove = prev.fullmove_number;
    zobrist  = prev.zobrist_key;
    state_count--;
}

/* ═══════════════════ Null Move ═══════════════════════════ */
/*
 * Null Move: Forfeiting and delegating the turn without materially moving a single item.
 * Exclusively utilized for the Null Move Pruning algorithm within node searching logic.
 * Intrinsically illegal in raw chess behavior, but devastatingly effective for search pruning.
 */

void Position::make_null_move(StateInfo& si) {
    StateInfo snapshot;
    snapshot.castling       = castling;
    snapshot.en_passant     = ep_sq;
    snapshot.halfmove_clock = halfmove;
    snapshot.fullmove_number = fullmove;
    snapshot.captured       = NO_PIECE;
    snapshot.zobrist_key    = zobrist;
    snapshot.last_move      = MOVE_NULL;

    if (ep_sq != NO_SQ) {
        zobrist ^= Zobrist::EnPassantKeys[file_of(ep_sq)];
        ep_sq = NO_SQ;
    }

    side = ~side;
    zobrist ^= Zobrist::SideKey;

    halfmove++;
    if (side == WHITE) fullmove++;

    if (history_count < MAX_GAME_MOVES)
        history_keys[history_count++] = zobrist;

    if (state_count < MAX_GAME_MOVES)
        state_stack[state_count++] = snapshot;

    si = snapshot;
}

void Position::unmake_null_move() {
    if (state_count <= 0)
        return;

    StateInfo& prev = state_stack[state_count - 1];

    history_count--;
    side      = ~side;
    castling  = prev.castling;
    ep_sq     = prev.en_passant;
    halfmove  = prev.halfmove_clock;
    fullmove  = prev.fullmove_number;
    zobrist   = prev.zobrist_key;
    state_count--;
}

void Position::make_move(Move m) {
    StateInfo si;
    make_move(m, si);
}

bool Position::do_move(Move m) {
    Color us = side;
    StateInfo si;
    make_move(m, si);

    if (is_attacked(king_sq[us], ~us)) {
        unmake_move(m);
        return false;
    }

    return true;
}

/* ═══════════════════ Repetition Detection ═══════════════ */
/*
 * Verifying execution of Threefold Repetition.
 *
 * Algorithm protocol: We aggressively traverse past captured Zobrist hash values.
 * Traversal ONLY operates down to the point of the last halfmove reset iteration.
 * (This is because resolving a capture or pawn advance inherently makes exact reversal mathematically impossible).
 *
 * Search condition: A singular identical instance found forces a repetition confirmation
 * (since attempting to reach the same situation generally equates to a forfeit).
 * Official Play constraint: Exact configuration repetition requires 3 hits to officially claim 'Draw'.
 */

bool Position::is_repetition() const {
    int start = history_count - 1;
    int end = history_count - halfmove;
    if (end < 0) end = 0;

    int count = 0;
    /* Skip by 2 intervals to ensure identical player-to-move mapping */
    for (int i = start - 2; i >= end; i -= 2) {
        if (history_keys[i] == zobrist) {
            count++;
            if (count >= 1)  /* Within Search logic: singular repetition verifies cyclic exhaustion */
                return true;
        }
    }
    return false;
}

/* ═══════════════════ Material Draw ══════════════════════ */

bool Position::is_material_draw() const {
    /* If either side has a pawn remaining, mate remains technically plausible */
    if (pieces(PAWN) || pieces(ROOK) || pieces(QUEEN)) return false;

    int w_knights = popcount(piece_bb[WHITE][KNIGHT]);
    int b_knights = popcount(piece_bb[BLACK][KNIGHT]);
    int w_bishops = popcount(piece_bb[WHITE][BISHOP]);
    int b_bishops = popcount(piece_bb[BLACK][BISHOP]);

    int total = w_knights + b_knights + w_bishops + b_bishops;

    /* King versus bare King */
    if (total == 0) return true;
    /* King supplemented sequentially with minor elements against a single King */
    if (total == 1) return true;
    /* King equipped with dual knights locking onto a bare King (Checkmate cannot be mechanically forced) */
    if (total == 2 && w_knights == 2) return true;
    if (total == 2 && b_knights == 2) return true;

    return false;
}

/* ═══════════════════ Compute Key (Verification) ═════════ */

U64 Position::compute_key() const {
    U64 k = 0;

    for (int sq = 0; sq < 64; sq++) {
        Piece p = board[sq];
        if (p != NO_PIECE)
            k ^= Zobrist::PieceKeys[p][sq];
    }

    if (side == BLACK) k ^= Zobrist::SideKey;
    k ^= Zobrist::CastlingKeys[castling];
    if (ep_sq != NO_SQ) k ^= Zobrist::EnPassantKeys[file_of(ep_sq)];

    return k;
}

/* ═══════════════════ Print ══════════════════════════════ */

void Position::print() const {
    printf("\n +---+---+---+---+---+---+---+---+\n");
    for (int r = RANK_8; r >= RANK_1; r--) {
        printf(" |");
        for (int f = FILE_A; f <= FILE_H; f++) {
            Piece p = board[make_square(File(f), Rank(r))];
            char c = (p == NO_PIECE) ? ' ' : piece_to_char(p);
            printf(" %c |", c);
        }
        printf(" %d\n", r + 1);
        printf(" +---+---+---+---+---+---+---+---+\n");
    }
    printf("   a   b   c   d   e   f   g   h\n\n");

    printf("  FEN:       %s\n", get_fen().c_str());
    printf("  Side:      %s\n", side == WHITE ? "White" : "Black");
    printf("  Castling:  %c%c%c%c\n",
           (castling & WHITE_OO)  ? 'K' : '-',
           (castling & WHITE_OOO) ? 'Q' : '-',
           (castling & BLACK_OO)  ? 'k' : '-',
           (castling & BLACK_OOO) ? 'q' : '-');
    printf("  En pass:   %s\n", ep_sq != NO_SQ ? square_to_string(ep_sq).c_str() : "-");
    printf("  Half move: %d\n", halfmove);
    printf("  Full move: %d\n", fullmove);
    printf("  Zobrist:   0x%016llx\n", (unsigned long long)zobrist);
    printf("  Key check: %s\n", zobrist == compute_key() ? "OK" : "MISMATCH!");
    printf("  In check:  %s\n\n", in_check() ? "Yes" : "No");
}
