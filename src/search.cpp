/*
 * search.cpp — Core Search Engine: Alpha-Beta + Iterative Deepening + Quiescence
 * =========================================================================
 */

#include "search.h"
#include "evaluate.h"
#include "movegen.h"
#include "tt.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {

constexpr int HASH_MOVE_SCORE = 30000000;
constexpr int CAPTURE_SCORE_BASE = 20000000;
constexpr int PROMOTION_SCORE_BASE = 19000000;
constexpr int KILLER_1_SCORE = 15000000;
constexpr int KILLER_2_SCORE = 14900000;
constexpr int HISTORY_SCORE_BASE = 1000;
constexpr uint64_t TIME_CHECK_INTERVAL = 64;
constexpr uint64_t TIME_CHECK_MASK = TIME_CHECK_INTERVAL - 1;

struct SearchState {
    explicit SearchState(const SearchLimits& requested_limits,
                         const std::atomic<bool>* stop_flag)
        : limits(requested_limits),
          external_stop(stop_flag),
          nodes(0),
          hard_stopped(false),
          soft_stopped(false),
          start_time_ms(current_time_ms()),
          soft_stop_time_ms(0),
          hard_stop_time_ms(0),
          tt_probes(0),
          tt_hits(0),
          tt_cutoffs(0),
          tt_stores(0) {
        std::memset(killers, 0, sizeof(killers));
        std::memset(history, 0, sizeof(history));

        if (limits.max_depth <= 0)
            limits.max_depth = MAX_DEPTH;

        if (limits.movetime_ms > 0) {
            int64_t movetime_safety_margin_ms =
                std::clamp<int64_t>(limits.movetime_ms / 50, 5, 50);

            if (limits.hard_time_ms <= 0)
                limits.hard_time_ms = std::max<int64_t>(limits.movetime_ms - movetime_safety_margin_ms, 1);

            if (limits.soft_time_ms <= 0) {
                limits.soft_time_ms = (limits.movetime_ms > 20)
                    ? (limits.movetime_ms * 9) / 10
                    : limits.hard_time_ms;
            }

            if (limits.soft_time_ms > limits.hard_time_ms)
                limits.soft_time_ms = limits.hard_time_ms;
        }

        if (limits.soft_time_ms > 0)
            soft_stop_time_ms = start_time_ms + limits.soft_time_ms;
        if (limits.hard_time_ms > 0)
            hard_stop_time_ms = start_time_ms + limits.hard_time_ms;
    }

    SearchLimits limits;
    const std::atomic<bool>* external_stop;
    uint64_t nodes;
    bool hard_stopped;
    bool soft_stopped;
    int64_t start_time_ms;
    int64_t soft_stop_time_ms;
    int64_t hard_stop_time_ms;
    uint64_t tt_probes;
    uint64_t tt_hits;
    uint64_t tt_cutoffs;
    uint64_t tt_stores;
    Move killers[MAX_PLY][2];
    int history[COLOR_NB][SQUARE_NB][SQUARE_NB];

    static int64_t current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
};

struct RootSearchResult {
    SearchResult result;
    bool completed;

    RootSearchResult() : completed(false) {}
};

static int64_t current_time_ms() {
    return SearchState::current_time_ms();
}

static void check_hard_stop(SearchState& state) {
    if (state.hard_stopped)
        return;

    if (state.external_stop && state.external_stop->load(std::memory_order_relaxed)) {
        state.hard_stopped = true;
        return;
    }

    if (state.hard_stop_time_ms > 0 && current_time_ms() >= state.hard_stop_time_ms)
        state.hard_stopped = true;
}

static inline void maybe_check_hard_stop(SearchState& state) {
    if ((state.nodes & TIME_CHECK_MASK) == 0)
        check_hard_stop(state);
}

static bool soft_stop_reached(SearchState& state) {
    if (state.soft_stop_time_ms <= 0)
        return false;

    if (current_time_ms() >= state.soft_stop_time_ms) {
        state.soft_stopped = true;
        return true;
    }

    return false;
}

static bool should_start_next_iteration(SearchState& state, int64_t last_iteration_ms) {
    if (soft_stop_reached(state))
        return false;

    if (state.hard_stop_time_ms <= 0 || last_iteration_ms <= 0)
        return true;

    int64_t remaining_ms = state.hard_stop_time_ms - current_time_ms();
    if (remaining_ms <= 1) {
        state.soft_stopped = true;
        return false;
    }

    int64_t predicted_next_iteration_ms = std::max<int64_t>(last_iteration_ms * 2, 1);
    if (remaining_ms <= predicted_next_iteration_ms) {
        state.soft_stopped = true;
        return false;
    }

    return true;
}

static inline bool leaves_king_in_check(const Position& pos, Color us) {
    return pos.is_attacked(pos.king_square(us), ~us);
}

static inline int mated_in(int ply) {
    return -VALUE_MATE + ply;
}

static bool is_capture_move(const Position& pos, Move m) {
    return move_type(m) == MT_EN_PASSANT || pos.piece_on(move_to(m)) != NO_PIECE;
}

static bool is_tactical_move(const Position& pos, Move m) {
    return move_type(m) == MT_PROMOTION || is_capture_move(pos, m);
}

static bool is_quiet_move(const Position& pos, Move m) {
    return !is_tactical_move(pos, m);
}

static void update_killers(SearchState& state, int ply, Move move) {
    if (state.limits.ordering_stage < ORDERING_STAGE2A || ply >= MAX_PLY)
        return;

    if (state.killers[ply][0] == move)
        return;

    state.killers[ply][1] = state.killers[ply][0];
    state.killers[ply][0] = move;
}

static void update_history(SearchState& state, Color us, Move move, int depth) {
    if (state.limits.ordering_stage < ORDERING_STAGE2B)
        return;

    int& entry = state.history[us][move_from(move)][move_to(move)];
    int bonus = depth * depth;

    if (entry > std::numeric_limits<int>::max() - bonus)
        entry = std::numeric_limits<int>::max();
    else
        entry += bonus;
}

static int score_move(const Position& pos, Move move, Move hash_move,
                      const SearchState& state, int ply) {
    if (move == hash_move)
        return HASH_MOVE_SCORE;

    if (move_type(move) == MT_PROMOTION)
        return PROMOTION_SCORE_BASE + PieceValue[promotion_type(move)];

    if (move_type(move) == MT_EN_PASSANT)
        return CAPTURE_SCORE_BASE + 500;

    Piece captured = pos.piece_on(move_to(move));
    if (captured != NO_PIECE) {
        Piece moved = pos.piece_on(move_from(move));
        int victim_val = PieceValue[type_of(captured)];
        int attacker_val = PieceValue[type_of(moved)];
        return CAPTURE_SCORE_BASE + victim_val * 10 - attacker_val;
    }

    if (state.limits.ordering_stage >= ORDERING_STAGE2A && ply < MAX_PLY) {
        if (move == state.killers[ply][0])
            return KILLER_1_SCORE;
        if (move == state.killers[ply][1])
            return KILLER_2_SCORE;
    }

    if (state.limits.ordering_stage >= ORDERING_STAGE2B) {
        int history_score = state.history[pos.side_to_move()][move_from(move)][move_to(move)];
        if (history_score > 0)
            return HISTORY_SCORE_BASE + history_score;
    }

    return 0;
}

static void score_moves(const Position& pos, MoveList& list, const SearchState& state,
                        int ply, Move hash_move = MOVE_NONE) {
    for (int i = 0; i < list.count; i++)
        list.scores[i] = score_move(pos, list.moves[i], hash_move, state, ply);
}

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

static void fill_result_totals(SearchResult& result, const SearchState& state) {
    result.nodes = state.nodes;
    result.tt_probes = state.tt_probes;
    result.tt_hits = state.tt_hits;
    result.tt_cutoffs = state.tt_cutoffs;
    result.tt_stores = state.tt_stores;
    result.time_ms = current_time_ms() - state.start_time_ms;
    result.stopped = state.soft_stopped || state.hard_stopped;
    result.hard_stopped = state.hard_stopped;
}

static void print_iteration_info(const SearchResult& result) {
    int64_t elapsed = result.time_ms > 0 ? result.time_ms : 1;
    int64_t nps = static_cast<int64_t>(result.nodes * 1000 / elapsed);

    if (result.score > VALUE_MATE - MAX_PLY) {
        int mate_in = (VALUE_MATE - result.score + 1) / 2;
        std::printf("info depth %d score mate %d nodes %llu time %lld nps %lld pv %s\n",
                    result.depth, mate_in,
                    static_cast<unsigned long long>(result.nodes),
                    static_cast<long long>(elapsed),
                    static_cast<long long>(nps),
                    move_to_string(result.best_move).c_str());
    } else if (result.score < -VALUE_MATE + MAX_PLY) {
        int mate_in = -(VALUE_MATE + result.score) / 2;
        std::printf("info depth %d score mate %d nodes %llu time %lld nps %lld pv %s\n",
                    result.depth, mate_in,
                    static_cast<unsigned long long>(result.nodes),
                    static_cast<long long>(elapsed),
                    static_cast<long long>(nps),
                    move_to_string(result.best_move).c_str());
    } else {
        std::printf("info depth %d score cp %d nodes %llu time %lld nps %lld pv %s\n",
                    result.depth, result.score,
                    static_cast<unsigned long long>(result.nodes),
                    static_cast<long long>(elapsed),
                    static_cast<long long>(nps),
                    move_to_string(result.best_move).c_str());
    }

    std::fflush(stdout);
}

static int quiescence(Position& pos, int alpha, int beta, SearchState& state) {
    state.nodes++;

    maybe_check_hard_stop(state);
    if (state.hard_stopped)
        return 0;

    if (pos.is_repetition() || pos.is_fifty_move_draw())
        return 0;

    int stand_pat = evaluate(pos);

    if (stand_pat >= beta)
        return beta;
    if (stand_pat > alpha)
        alpha = stand_pat;

    MoveList captures;
    generate_captures(pos, captures);
    score_moves(pos, captures, state, MAX_PLY);

    Color us = pos.side_to_move();

    for (int i = 0; i < captures.count; i++) {
        if ((i & 7) == 0)
            check_hard_stop(state);
        if (state.hard_stopped)
            return 0;

        pick_move(captures, i);

        StateInfo si;
        pos.make_move(captures.moves[i], si);
        if (leaves_king_in_check(pos, us)) {
            pos.unmake_move(captures.moves[i]);
            continue;
        }

        int score = -quiescence(pos, -beta, -alpha, state);
        pos.unmake_move(captures.moves[i]);

        if (state.hard_stopped)
            return 0;

        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

static int alpha_beta(Position& pos, int depth, int alpha, int beta,
                      int ply, SearchState& state) {
    maybe_check_hard_stop(state);
    if (state.hard_stopped)
        return 0;

    if (pos.is_repetition() || pos.is_fifty_move_draw() || pos.is_material_draw())
        return 0;

    if (depth <= 0)
        return quiescence(pos, alpha, beta, state);

    state.nodes++;

    int alpha_orig = alpha;
    Move tt_move = MOVE_NONE;
    TTEntry tt_entry;

    state.tt_probes++;
    if (g_tt.probe(pos.key(), tt_entry)) {
        state.tt_hits++;
        tt_move = tt_entry.best_move;

        if (tt_entry.depth >= depth) {
            int tt_score = TranspositionTable::score_from_tt(tt_entry.score, ply);

            if (tt_entry.bound == TT_BOUND_EXACT) {
                state.tt_cutoffs++;
                return tt_score;
            }
            if (tt_entry.bound == TT_BOUND_LOWER && tt_score >= beta) {
                state.tt_cutoffs++;
                return tt_score;
            }
            if (tt_entry.bound == TT_BOUND_UPPER && tt_score <= alpha) {
                state.tt_cutoffs++;
                return tt_score;
            }
        }
    }

    MoveList moves;
    generate_moves(pos, moves);
    score_moves(pos, moves, state, ply, tt_move);

    Color us = pos.side_to_move();
    Move best_move = MOVE_NONE;
    int best_score = -VALUE_INFINITE;
    bool found_legal = false;

    for (int i = 0; i < moves.count; i++) {
        if ((i & 7) == 0)
            check_hard_stop(state);
        if (state.hard_stopped)
            return 0;

        pick_move(moves, i);

        StateInfo si;
        pos.make_move(moves.moves[i], si);
        if (leaves_king_in_check(pos, us)) {
            pos.unmake_move(moves.moves[i]);
            continue;
        }

        found_legal = true;

        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, ply + 1, state);
        pos.unmake_move(moves.moves[i]);

        if (state.hard_stopped)
            return 0;

        if (score > best_score) {
            best_score = score;
            best_move = moves.moves[i];
        }

        if (score > alpha)
            alpha = score;

        if (alpha >= beta) {
            if (is_quiet_move(pos, moves.moves[i])) {
                update_killers(state, ply, moves.moves[i]);
                update_history(state, us, moves.moves[i], depth);
            }
            break;
        }
    }

    if (!found_legal) {
        int terminal = pos.in_check() ? mated_in(ply) : 0;
        g_tt.store(pos.key(), depth, terminal, MOVE_NONE, TT_BOUND_EXACT, ply);
        state.tt_stores++;
        return terminal;
    }

    TTBound bound = TT_BOUND_EXACT;
    if (best_score <= alpha_orig)
        bound = TT_BOUND_UPPER;
    else if (best_score >= beta)
        bound = TT_BOUND_LOWER;

    g_tt.store(pos.key(), depth, best_score, best_move, bound, ply);
    state.tt_stores++;

    return best_score;
}

static RootSearchResult root_search(Position& pos, const MoveList& legal_moves,
                                    int depth, SearchState& state) {
    RootSearchResult out;
    out.result.depth = depth;

    if (legal_moves.count == 0) {
        out.result.best_move = MOVE_NONE;
        out.result.score = pos.in_check() ? mated_in(0) : 0;
        fill_result_totals(out.result, state);
        out.completed = true;
        return out;
    }

    MoveList moves = legal_moves;
    score_moves(pos, moves, state, 0, g_tt.probe_move(pos.key()));

    int alpha = -VALUE_INFINITE;
    int beta = VALUE_INFINITE;

    for (int i = 0; i < moves.count; i++) {
        check_hard_stop(state);
        if (state.hard_stopped)
            return out;

        pick_move(moves, i);

        StateInfo si;
        pos.make_move(moves.moves[i], si);
        int score = -alpha_beta(pos, depth - 1, -beta, -alpha, 1, state);
        pos.unmake_move(moves.moves[i]);

        if (state.hard_stopped)
            return out;

        if (i == 0 || score > alpha) {
            alpha = score;
            out.result.best_move = moves.moves[i];
            out.result.score = score;
        }
    }

    g_tt.store(pos.key(), depth, out.result.score, out.result.best_move, TT_BOUND_EXACT, 0);
    state.tt_stores++;

    fill_result_totals(out.result, state);
    out.completed = true;
    return out;
}

}  // namespace

const char* ordering_stage_name(OrderingStage stage) {
    switch (stage) {
        case ORDERING_STAGE1: return "stage1";
        case ORDERING_STAGE2A: return "stage2a";
        case ORDERING_STAGE2B: return "stage2b";
        default: return "unknown";
    }
}

SearchResult search(Position& pos, const SearchLimits& limits,
                    const std::atomic<bool>* external_stop) {
    SearchState state(limits, external_stop);
    SearchResult fallback_result;
    SearchResult last_completed_result;
    bool have_completed_iteration = false;

    g_tt.new_search();

    MoveList legal_moves;
    generate_legal_moves(pos, legal_moves);

    if (legal_moves.count == 0) {
        fallback_result.best_move = MOVE_NONE;
        fallback_result.score = pos.in_check() ? mated_in(0) : 0;
        fill_result_totals(fallback_result, state);
        return fallback_result;
    }

    fallback_result.best_move = legal_moves.moves[0];
    fallback_result.score = 0;
    fill_result_totals(fallback_result, state);

    int64_t previous_completed_time_ms = 0;
    int64_t last_iteration_time_ms = 0;

    for (int depth = 1; depth <= state.limits.max_depth; depth++) {
        if (have_completed_iteration && !should_start_next_iteration(state, last_iteration_time_ms))
            break;

        RootSearchResult iteration = root_search(pos, legal_moves, depth, state);
        if (state.hard_stopped || !iteration.completed)
            break;

        last_completed_result = iteration.result;
        have_completed_iteration = true;
        last_iteration_time_ms = last_completed_result.time_ms - previous_completed_time_ms;
        previous_completed_time_ms = last_completed_result.time_ms;

        if (state.limits.enable_info_output)
            print_iteration_info(last_completed_result);

        if (last_completed_result.score > VALUE_MATE - MAX_PLY ||
            last_completed_result.score < -VALUE_MATE + MAX_PLY)
            break;
    }

    SearchResult result = have_completed_iteration ? last_completed_result : fallback_result;
    fill_result_totals(result, state);
    return result;
}
