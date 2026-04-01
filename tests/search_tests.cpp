#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "zobrist.h"

#include <atomic>
#include <cassert>
#include <cstdio>

static bool move_in_list(const MoveList& list, Move move) {
    for (int i = 0; i < list.count; i++) {
        if (list.moves[i] == move)
            return true;
    }
    return false;
}

#ifndef NDEBUG
static Move parse_move(Position& pos, const char* text) {
    Move move = MOVE_NONE;
    assert(parse_legal_move(pos, text, move));
    return move;
}

static int history_entry(const SearchOrderingDebugState& state, Color side, Move move) {
    return state.history[side][move_from(move)][move_to(move)];
}

static void test_quiet_cutoffs_update_killers_and_history() {
    Position pos;
    pos.set_startpos();

    SearchOrderingDebugState state;
    search_debug_clear_ordering_tables(state);

    Move first = parse_move(pos, "e2e4");
    Move second = parse_move(pos, "d2d4");

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 4, first, 4);
    assert(state.killers[4][0] == first);
    assert(state.killers[4][1] == MOVE_NONE);
    assert(history_entry(state, WHITE, first) == 16);

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 4, second, 3);
    assert(state.killers[4][0] == second);
    assert(state.killers[4][1] == first);
    assert(history_entry(state, WHITE, first) == 16);
    assert(history_entry(state, WHITE, second) == 9);

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 4, second, 2);
    assert(state.killers[4][0] == second);
    assert(state.killers[4][1] == first);
    assert(history_entry(state, WHITE, second) == 13);
}

static void test_tactical_cutoffs_do_not_update_ordering_tables() {
    SearchOrderingDebugState state;
    search_debug_clear_ordering_tables(state);

    Position capture_pos;
    capture_pos.set_fen("rnbqkbnr/pppppppp/8/8/3p4/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    Move capture = parse_move(capture_pos, "e3d4");

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, capture_pos, 5, capture, 4);
    assert(state.killers[5][0] == MOVE_NONE);
    assert(state.killers[5][1] == MOVE_NONE);
    assert(history_entry(state, WHITE, capture) == 0);

    Position promotion_pos;
    promotion_pos.set_fen("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    Move promotion = parse_move(promotion_pos, "a7a8q");

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, promotion_pos, 5, promotion, 4);
    assert(state.killers[5][0] == MOVE_NONE);
    assert(state.killers[5][1] == MOVE_NONE);
    assert(history_entry(state, WHITE, promotion) == 0);
}

static void test_hash_move_stays_ahead_of_killers_history_and_captures() {
    Position pos;
    pos.set_fen("rnbqkbnr/pppppppp/8/8/3p4/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 1");

    SearchOrderingDebugState state;
    search_debug_clear_ordering_tables(state);

    Move history_quiet = parse_move(pos, "a2a3");
    Move killer2 = parse_move(pos, "d2d3");
    Move killer1 = parse_move(pos, "b1c3");
    Move hash_move = parse_move(pos, "g1f3");
    Move untouched_quiet = parse_move(pos, "h2h3");
    Move capture = parse_move(pos, "e3d4");

    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 2, history_quiet, 2);
    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 2, killer2, 3);
    search_debug_record_quiet_beta_cutoff(state, ORDERING_STAGE2B, pos, 2, killer1, 4);

    int hash_score = search_debug_score_move(pos, hash_move, hash_move, state, ORDERING_STAGE2B, 2);
    int capture_score = search_debug_score_move(pos, capture, hash_move, state, ORDERING_STAGE2B, 2);
    int killer1_score = search_debug_score_move(pos, killer1, hash_move, state, ORDERING_STAGE2B, 2);
    int killer2_score = search_debug_score_move(pos, killer2, hash_move, state, ORDERING_STAGE2B, 2);
    int history_score = search_debug_score_move(pos, history_quiet, hash_move, state, ORDERING_STAGE2B, 2);
    int untouched_score = search_debug_score_move(pos, untouched_quiet, hash_move, state, ORDERING_STAGE2B, 2);

    assert(hash_score > capture_score);
    assert(capture_score > killer1_score);
    assert(killer1_score > killer2_score);
    assert(killer2_score > history_score);
    assert(history_score > untouched_score);
    assert(untouched_score == 0);
}
#endif

static void test_mate_in_one() {
    Position pos;
    pos.set_fen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4");

    SearchLimits limits;
    limits.max_depth = 3;
    limits.ordering_stage = ORDERING_STAGE1;

    SearchResult result = search(pos, limits);
    assert(result.best_move == make_move(H5, F7));
}

static void test_transposition_table_hits() {
    Position pos;
    pos.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    SearchLimits limits;
    limits.max_depth = 4;
    limits.ordering_stage = ORDERING_STAGE1;

    SearchResult result = search(pos, limits);
    assert(result.best_move != MOVE_NONE);
    assert(result.tt_hits > 0);
    assert(result.tt_cutoffs > 0);
    assert(result.tt_stores > 0);
}

static void test_parse_and_do_move() {
    Position pos;
    pos.set_startpos();

    Move move;
    assert(parse_legal_move(pos, "e2e4", move));
    assert(pos.do_move(move));
    assert(pos.get_fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
}

static void test_position_copy_is_safe_after_persistent_moves() {
    Position pos;
    pos.set_startpos();

    Move move;
    assert(parse_legal_move(pos, "e2e4", move));
    assert(pos.do_move(move));

    Position copy = pos;
    assert(parse_legal_move(copy, "e7e5", move));
    assert(copy.do_move(move));
    assert(copy.get_fen() == "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2");
}

static void test_immediate_stop_returns_legal_fallback() {
    Position pos;
    pos.set_startpos();

    SearchLimits limits;
    limits.max_depth = 6;
    limits.ordering_stage = ORDERING_STAGE2B;

    std::atomic<bool> stop(true);
    SearchResult result = search(pos, limits, &stop);

    MoveList legal_moves;
    generate_legal_moves(pos, legal_moves);

    assert(result.best_move != MOVE_NONE);
    assert(move_in_list(legal_moves, result.best_move));
    assert(result.depth == 0);
    assert(result.hard_stopped);
}

static void test_internal_hard_deadline_returns_legal_move() {
    Position pos;
    pos.set_startpos();

    SearchLimits limits;
    limits.max_depth = MAX_DEPTH;
    limits.ordering_stage = ORDERING_STAGE2B;
    limits.soft_time_ms = 1;
    limits.hard_time_ms = 2;

    SearchResult result = search(pos, limits);

    MoveList legal_moves;
    generate_legal_moves(pos, legal_moves);

    assert(result.best_move != MOVE_NONE);
    assert(move_in_list(legal_moves, result.best_move));
    assert(result.stopped);
}

int main() {
    init_bitboards();
    Zobrist::init();

#ifndef NDEBUG
    test_quiet_cutoffs_update_killers_and_history();
    test_tactical_cutoffs_do_not_update_ordering_tables();
    test_hash_move_stays_ahead_of_killers_history_and_captures();
#endif
    test_mate_in_one();
    test_transposition_table_hits();
    test_parse_and_do_move();
    test_position_copy_is_safe_after_persistent_moves();
    test_immediate_stop_returns_legal_fallback();
    test_internal_hard_deadline_returns_legal_move();

    std::puts("search tests passed");
    return 0;
}
