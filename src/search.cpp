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

static void score_moves(const Position& pos, MoveList& list) {
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        Square to = move_to(m);
        Piece captured = pos.piece_on(to);

        if (captured != NO_PIECE) {
            /* MVV-LVA: victim_value * 10 - attacker_value */
            Piece moved = pos.piece_on(move_from(m));
            int victim_val = PieceValue[type_of(captured)];
            int attacker_val = PieceValue[type_of(moved)];
            list.scores[i] = victim_val * 10 - attacker_val + 10000;
            /* +10000 ensures ALL captures fundamentally outrank ALL quiet moves */
        } else if (move_type(m) == MT_EN_PASSANT) {
            list.scores[i] = 10500;  /* en passant mechanically acts as pawn capture */
        } else if (move_type(m) == MT_PROMOTION) {
            list.scores[i] = 9000 + PieceValue[promotion_type(m)];
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

    for (int i = 0; i < captures.count; i++) {
        pick_move(captures, i);

        StateInfo si;
        pos.make_move(captures.moves[i], si);
        int score = -quiescence(pos, -beta, -alpha, info);
        pos.unmake_move(captures.moves[i]);

        if (info.stopped) return 0;

        if (score >= beta) return beta;
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
                       SearchInfo& info) {
    /* ─── Termination Overrides ──────────────────────────────── */

    if ((info.nodes & 2047) == 0) check_time(info);
    if (info.stopped) return 0;

    /* Institutional draw states */
    if (pos.is_repetition() || pos.is_fifty_move_draw()) return 0;
    if (pos.is_material_draw()) return 0;

    /* Depth horizon breach -> Escalate to Quiescence fallback */
    if (depth <= 0) return quiescence(pos, alpha, beta, info);

    info.nodes++;

    /* ─── Move Expansion ────────────────────── */
    MoveList moves;
    generate_moves(pos, moves);

    /* Absence of legal execution array -> Checkmate or Stalemate mapping */
    if (moves.count == 0) {
        if (pos.in_check())
            return -VALUE_MATE + info.nodes;  /* Checkmate state — prioritize mathematically shorter mates */
        else
            return 0;  /* Stalemate draw */
    }

    /* Sequencing operations */
    score_moves(pos, moves);

    Move best_move = MOVE_NONE;

    for (int i = 0; i < moves.count; i++) {
        pick_move(moves, i);

        /* Legality filtering */
        StateInfo si;
        pos.make_move(moves.moves[i], si);
        if (pos.is_attacked(pos.king_square(pos.side_to_move() == WHITE ? BLACK : WHITE), pos.side_to_move())) {
            pos.unmake_move(moves.moves[i]);
            continue;
        }

        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, info);
        pos.unmake_move(moves.moves[i]);

        if (info.stopped) return 0;

        if (score > alpha) {
            alpha = score;
            best_move = moves.moves[i];

            if (score >= beta) {
                /* Opponent ceiling breached */
                return beta;
            }
        }
    }

    /* Fallback catch for an all-illegal move iteration resulting in Checkmate/Stalemate masking */
    if (best_move == MOVE_NONE) {
        if (pos.in_check()) return -VALUE_MATE + info.nodes;
        return 0;
    }

    return alpha;
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
    score_moves(pos, moves);

    int alpha = -VALUE_INFINITE;
    int beta  =  VALUE_INFINITE;

    for (int i = 0; i < moves.count; i++) {
        pick_move(moves, i);

        StateInfo si;
        pos.make_move(moves.moves[i], si);
        if (pos.is_attacked(pos.king_square(pos.side_to_move() == WHITE ? BLACK : WHITE), pos.side_to_move())) {
            pos.unmake_move(moves.moves[i]);
            continue;
        }

        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, info);
        pos.unmake_move(moves.moves[i]);

        if (info.stopped) break;

        if (score > alpha) {
            alpha = score;
            result.best_move = moves.moves[i];
            result.score = score;
        }
    }

    /* Fallback in case root checkmate processing yields nil array */
    if (result.best_move == MOVE_NONE && moves.count > 0) {
        for (int i = 0; i < moves.count; i++) {
            StateInfo si;
            pos.make_move(moves.moves[i], si);
            bool ok = !pos.is_attacked(pos.king_square(pos.side_to_move() == WHITE ? BLACK : WHITE), pos.side_to_move());
            pos.unmake_move(moves.moves[i]);
            if (ok) {
                result.best_move = moves.moves[i];
                break;
            }
        }
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

    printf("bestmove %s\n", move_to_string(best_result.best_move).c_str());
    fflush(stdout);

    return best_result;
}
