/*
 * search.h — Search Engine Interface
 * ==============================
 *
 * Provides:
 *   - SearchInfo: Search control variables (depth, time limit, node count)
 *   - SearchResult: The outcome of the search (best move + score)
 *   - search(): Main routine to find the best move
 */

#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "move.h"

#include <atomic>
#include <cstdint>

enum OrderingStage : uint8_t {
    ORDERING_STAGE1 = 0,
    ORDERING_STAGE2A,
    ORDERING_STAGE2B
};

struct SearchLimits {
    int            max_depth;         /* Maximum completed depth target */
    int64_t        movetime_ms;       /* Fixed movetime budget */
    int64_t        soft_time_ms;      /* Optional soft stop */
    int64_t        hard_time_ms;      /* Optional hard stop */
    OrderingStage  ordering_stage;    /* Stage 1 / 2A / 2B benchmark selection */
    bool           enable_info_output;

    SearchLimits()
        : max_depth(MAX_DEPTH), movetime_ms(0), soft_time_ms(0), hard_time_ms(0),
          ordering_stage(ORDERING_STAGE2B), enable_info_output(false) {}
};

/* ═══════════════════ Search Result ════════════════════════ */

struct SearchResult {
    Move best_move;
    int  score;
    int  depth;
    uint64_t nodes;
    uint64_t tt_probes;
    uint64_t tt_hits;
    uint64_t tt_cutoffs;
    uint64_t tt_stores;
    int64_t time_ms;
    bool stopped;
    bool hard_stopped;

    SearchResult()
        : best_move(MOVE_NONE), score(0), depth(0), nodes(0),
          tt_probes(0), tt_hits(0), tt_cutoffs(0), tt_stores(0),
          time_ms(0), stopped(false), hard_stopped(false) {}
};

/* ═══════════════════ Search Function ═════════════════════ */

/*
 * search(pos, info)
 * Locates the optimal move utilizing:
 *   1. Iterative Deepening
 *   2. Alpha-Beta pruning with Negamax formulation
 *   3. Quiescence Search on tactical horizons
 *
 * Returns: SearchResult pairing the determined best move and its objective evaluation.
 */
SearchResult search(Position& pos, const SearchLimits& limits,
                    const std::atomic<bool>* external_stop = nullptr);

const char* ordering_stage_name(OrderingStage stage);

#endif /* SEARCH_H */
