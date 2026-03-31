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

/* ═══════════════════ Search Info ═══════════════════════════ */

struct SearchInfo {
    int      depth;          /* Maximum depth */
    int      nodes;          /* Number of nodes visited */
    bool     stopped;        /* Has the search been ordered to stop? */

    /* Time management context (expanded later) */
    int64_t  start_time;     /* Start time in milliseconds */
    int64_t  stop_time;      /* Targeted stop time in milliseconds */
    bool     time_set;       /* Is there a strict time limit applied? */

    SearchInfo() : depth(MAX_DEPTH), nodes(0), stopped(false),
                   start_time(0), stop_time(0), time_set(false) {}
};

/* ═══════════════════ Search Result ════════════════════════ */

struct SearchResult {
    Move best_move;
    int  score;
    int  depth;
    int  nodes;

    SearchResult() : best_move(MOVE_NONE), score(0), depth(0), nodes(0) {}
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
SearchResult search(Position& pos, SearchInfo& info);

#endif /* SEARCH_H */
