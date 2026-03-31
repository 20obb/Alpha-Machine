/*
 * search.cpp — Core Search Engine: Alpha-Beta + Iterative Deepening + Quiescence
 * =========================================================================
 *
 * Architecture Hierarchy:
 *   1. Negamax: Simplest fundamental recursive structure.
 *   2. Alpha-Beta: Branches pruning mechanism (eliminates ~90% of useless evaluations).
 *   3. Quiescence: Specialized capture search when depth hits 0 (mitigates horizon effect).
 *   4. Iterative Deepening: Incremental depth-based loop (re-searches at Depth 1, 2, 3...)
 *
 * Full Pipeline:
 *   search() -> iterative_deepening -> alpha_beta -> quiescence
 *
 * Negamax Architecture:
 *   "What is best for me is worse for my opponent."
 *   score = -alpha_beta(...)  <- Perspective flips every recursion level.
 *
 * Alpha-Beta:
 *   - alpha = Best guaranteed score for me so far.
 *   - beta = Best score my opponent will concede to me.
 *   - If score >= beta: The opponent will deliberately avoid this branch -> Beta cutoff.
 *   - If score > alpha: We located a better move -> Increase alpha threshold.
 *
 * Quiescence Search:
 *   At Depth 0, blindly evaluating could be disastrous if a piece is mid-capture.
 *   - Compute "Stand Pat" (static evaluation rating).
 *   - Iteratively evaluate ONLY capture moves.
 *   - Extinguishes the "Horizon Effect", ensuring stability.
 */

#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "bitboard.h"
#include "tt.h"

#include <cstdio>
#include <chrono>
#include <algorithm>

/* ═══════════════════ Timing ═══════════════════════════════ */

static int64_t current_time_ms() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

static void check_time(SearchInfo& info) {
    if (info.time_set && current_time_ms() >= info.stop_time)
        info.stopped = true;
}

static inline bool leaves_king_in_check(const Position& pos, Color us) {
    return pos.is_attacked(pos.king_square(us), ~us);
}

static inline int mated_in(int ply) {
    return -VALUE_MATE + ply;
}

/* ═══════════════════ Move Ordering (Simple) ═══════════════ */
/*
 * Move ordering dramatically dictates alpha-beta performance yield.
 * Testing better moves first achieves Beta cutoffs rapidly.
 *
 * V1 Configuration:
 *   - Captures: Sequenced by MVV-LVA (Most Valuable Victim, Least Valuable Attacker)
 *     Pawn taking Queen = Highest possible priority.
 *     Queen taking Pawn = Lowest capture priority.
 *   - Quiet Moves: Baseline priority score 0 (upgraded later via killer/history).
 */

static constexpr int HASH_MOVE_SCORE = 30000000;
static constexpr int CAPTURE_SCORE_BASE = 10000000;
static constexpr int PROMOTION_SCORE_BASE = 9000000;

static void score_moves(const Position& pos, MoveList& list, Move hash_move = MOVE_NONE) {
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        Square to = move_to(m);
        Piece captured = pos.piece_on(to);

        if (m == hash_move) {
            list.scores[i] = HASH_MOVE_SCORE;
        } else if (captured != NO_PIECE) {
            /* MVV-LVA: victim_value * 10 - attacker_value */
            Piece moved = pos.piece_on(move_from(m));
            int victim_val = PieceValue[type_of(captured)];
            int attacker_val = PieceValue[type_of(moved)];
            list.scores[i] = victim_val * 10 - attacker_val + CAPTURE_SCORE_BASE;
            /* Captures outrank quiet moves; hash move outranks everything. */
        } else if (move_type(m) == MT_EN_PASSANT) {
            list.scores[i] = CAPTURE_SCORE_BASE + 500;  /* En passant behaves like pawn capture. */
        } else if (move_type(m) == MT_PROMOTION) {
            list.scores[i] = PROMOTION_SCORE_BASE + PieceValue[promotion_type(m)];
        } else {
            list.scores[i] = 0;
        }
    }
}

/*
 * pick_move: Incrementally extract best remaining move (Selection Sort variation).
 * Vastly superior to sorting the complete array natively, since we usually only
 * extract 1 or 2 moves before triggering a Beta cutoff.
 */
static void pick_move(MoveList& list, int start) {
    int best_idx = start;
    int best_score = list.scores[start];

    for (int i = start + 1; i < list.count; i++) {
        if (list.scores[i] > best_score) {
            best_score = list.scores[i];
            best_idx = i;
        }
    }

    if (best_idx != start) {
        std::swap(list.moves[start], list.moves[best_idx]);
        std::swap(list.scores[start], list.scores[best_idx]);
    }
}

/* ═══════════════════ Quiescence Search ════════════════════ */
/*
 * Recursive search constrained purely to capture moves.
 *
 * Stand Pat:
 *   The positional evaluation score unconditionally provided as a baseline.
 *   If it's already formidable -> Decline further capture calculations.
 *   Maintains algorithmic sanity when threatened with bad exchanges.
 */

static int quiescence(Position& pos, int alpha, int beta, SearchInfo& info) {
    info.nodes++;

    /* Enforce termination clock every 2048 node cycles */
    if ((info.nodes & 2047) == 0) check_time(info);
    if (info.stopped) return 0;

    /* Repetition and draw recognition */
    if (pos.is_repetition() || pos.is_fifty_move_draw()) return 0;

    /* Stand Pat */
    int stand_pat = evaluate(pos);

    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    /* Formulate capture moves strictly */
    MoveList captures;
    generate_captures(pos, captures);
    score_moves(pos, captures);

    Color us = pos.side_to_move();

    for (int i = 0; i < captures.count; i++) {
        pick_move(captures, i);

        StateInfo si;
        pos.make_move(captures.moves[i], si);
        if (leaves_king_in_check(pos, us)) {
            pos.unmake_move(captures.moves[i]);
            continue;
        }

        int score = -quiescence(pos, -beta, -alpha, info);
        pos.unmake_move(captures.moves[i]);

        if (info.stopped) return 0;

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

/* ═══════════════════ Alpha-Beta ═══════════════════════════ */
/*
 * Primary recursive search hub.
 *
 * Parameters:
 *   pos    — Target spatial matrix
 *   depth  — Remaining vertical cascade limitation
 *   alpha  — Ground floor guaranteed evaluation boundary
 *   beta   — Maximum upper boundary opponent will accept
 *   info   — System control data
 *
 * Returns: Complete sub-node evaluation perspective.
 */

static int alpha_beta(Position& pos, int depth, int alpha, int beta,
                      int ply, SearchInfo& info) {
    /* ─── Termination Overrides ──────────────────────────────── */

    if ((info.nodes & 2047) == 0) check_time(info);
    if (info.stopped) return 0;

    /* Institutional draw states */
    if (pos.is_repetition() || pos.is_fifty_move_draw()) return 0;
    if (pos.is_material_draw()) return 0;

    /* Depth horizon breach -> Escalate to Quiescence fallback */
    if (depth <= 0) return quiescence(pos, alpha, beta, info);

    info.nodes++;

    int alpha_orig = alpha;
    Move tt_move = MOVE_NONE;
    TTEntry tt_entry;

    info.tt_probes++;
    if (g_tt.probe(pos.key(), tt_entry)) {
        info.tt_hits++;
        tt_move = tt_entry.best_move;

        if (tt_entry.depth >= depth) {
            int tt_score = TranspositionTable::score_from_tt(tt_entry.score, ply);

            if (tt_entry.bound == TT_BOUND_EXACT) {
                info.tt_cutoffs++;
                return tt_score;
            }
            if (tt_entry.bound == TT_BOUND_LOWER && tt_score >= beta) {
                info.tt_cutoffs++;
                return tt_score;
            }
            if (tt_entry.bound == TT_BOUND_UPPER && tt_score <= alpha) {
                info.tt_cutoffs++;
                return tt_score;
            }
        }
    }

    /* ─── Move Expansion ────────────────────── */
    MoveList moves;
    generate_moves(pos, moves);

    /* Sequencing operations */
    score_moves(pos, moves, tt_move);

    Color us = pos.side_to_move();
    Move best_move = MOVE_NONE;
    int best_score = -VALUE_INFINITE;
    bool found_legal = false;

    for (int i = 0; i < moves.count; i++) {
        pick_move(moves, i);

        /* Legality filtering */
        StateInfo si;
        pos.make_move(moves.moves[i], si);
        if (leaves_king_in_check(pos, us)) {
            pos.unmake_move(moves.moves[i]);
            continue;
        }

        found_legal = true;

        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, info);
        pos.unmake_move(moves.moves[i]);

        if (info.stopped) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = moves.moves[i];
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta)
            break;
    }

    if (!found_legal) {
        int terminal = pos.in_check() ? mated_in(ply) : 0;
        g_tt.store(pos.key(), depth, terminal, MOVE_NONE, TT_BOUND_EXACT, ply);
        info.tt_stores++;
        return terminal;
    }

    TTBound bound = TT_BOUND_EXACT;
    if (best_score <= alpha_orig) bound = TT_BOUND_UPPER;
    else if (best_score >= beta) bound = TT_BOUND_LOWER;

    g_tt.store(pos.key(), depth, best_score, best_move, bound, ply);
    info.tt_stores++;

    return best_score;
}

/* ═══════════════════ Root Alpha-Beta ═════════════════════ */
/*
 * Iteration Zero search vector. Tracks absolute 'best' root move structurally.
 */

static SearchResult root_search(Position& pos, int depth, SearchInfo& info) {
    SearchResult result;
    result.depth = depth;

    MoveList moves;
    generate_moves(pos, moves);
    score_moves(pos, moves, g_tt.probe_move(pos.key()));

    int alpha = -VALUE_INFINITE;
    int beta  =  VALUE_INFINITE;
    Color us = pos.side_to_move();
    bool found_legal = false;

    for (int i = 0; i < moves.count; i++) {
        pick_move(moves, i);

        StateInfo si;
        pos.make_move(moves.moves[i], si);
        if (leaves_king_in_check(pos, us)) {
            pos.unmake_move(moves.moves[i]);
            continue;
        }

        found_legal = true;
        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, 1, info);
        pos.unmake_move(moves.moves[i]);

        if (info.stopped) break;

        if (score > alpha) {
            alpha = score;
            result.best_move = moves.moves[i];
            result.score = score;
        }
    }

    if (!found_legal) {
        result.score = pos.in_check() ? mated_in(0) : 0;
    }

    if (!info.stopped) {
        g_tt.store(pos.key(), depth, result.score, result.best_move, TT_BOUND_EXACT, 0);
        info.tt_stores++;
    }

    result.nodes = info.nodes;
    return result;
}

/* ═══════════════════ Iterative Deepening ═════════════════ */
/*
 * Layer-cake depth parsing sequence (Depth 1, 2, 3...).
 *
 * Method rationale:
 *   1. Guarantee instantaneous move response availability against clock termination.
 *   2. Previous depth computations drastically streamline subsequent deeper move orders (TT prep).
 *   3. Insignificant total overhead latency (~10%) due to exponential node scaling at max depth.
 */

SearchResult search(Position& pos, SearchInfo& info) {
    SearchResult best_result;
    info.nodes = 0;
    info.stopped = false;
    info.tt_probes = 0;
    info.tt_hits = 0;
    info.tt_cutoffs = 0;
    info.tt_stores = 0;
    g_tt.new_search();

    int64_t search_start = current_time_ms();

    for (int depth = 1; depth <= info.depth; depth++) {
        SearchResult result = root_search(pos, depth, info);

        if (info.stopped) break;

        best_result = result;

        /* ─── Standard UCI Log Outputs ────────────────────── */
        int64_t elapsed = current_time_ms() - search_start;
        if (elapsed == 0) elapsed = 1;
        int64_t nps = (int64_t)info.nodes * 1000 / elapsed;

        /* Standardize mate detection thresholds */
        if (result.score > VALUE_MATE - MAX_PLY) {
            int mate_in = (VALUE_MATE - result.score + 1) / 2;
            printf("info depth %d score mate %d nodes %d time %lld nps %lld pv %s\n",
                   depth, mate_in, info.nodes,
                   (long long)elapsed, (long long)nps,
                   move_to_string(result.best_move).c_str());
        } else if (result.score < -VALUE_MATE + MAX_PLY) {
            int mate_in = -(VALUE_MATE + result.score) / 2;
            printf("info depth %d score mate %d nodes %d time %lld nps %lld pv %s\n",
                   depth, mate_in, info.nodes,
                   (long long)elapsed, (long long)nps,
                   move_to_string(result.best_move).c_str());
        } else {
            printf("info depth %d score cp %d nodes %d time %lld nps %lld pv %s\n",
                   depth, result.score, info.nodes,
                   (long long)elapsed, (long long)nps,
                   move_to_string(result.best_move).c_str());
        }
        fflush(stdout);

        /* Terminate Deepening if terminal horizon confirmed */
        if (result.score > VALUE_MATE - MAX_PLY ||
            result.score < -VALUE_MATE + MAX_PLY)
            break;
    }

    best_result.nodes = info.nodes;
    best_result.tt_probes = info.tt_probes;
    best_result.tt_hits = info.tt_hits;
    best_result.tt_cutoffs = info.tt_cutoffs;
    best_result.tt_stores = info.tt_stores;

    printf("bestmove %s\n", move_to_string(best_result.best_move).c_str());
    fflush(stdout);

    return best_result;
}
