#ifndef TT_H
#define TT_H

#include "types.h"
#include "move.h"

#include <cstddef>
#include <cstdint>
#include <vector>

enum TTBound : uint8_t {
    TT_BOUND_NONE = 0,
    TT_BOUND_EXACT,
    TT_BOUND_LOWER,
    TT_BOUND_UPPER
};

struct TTEntry {
    U64      key;
    int      score;
    Move     best_move;
    int16_t  depth;
    uint8_t  bound;
    uint8_t  generation;

    TTEntry()
        : key(0), score(0), best_move(MOVE_NONE),
          depth(-1), bound(TT_BOUND_NONE), generation(0) {}
};

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes = 16);

    void resize(std::size_t megabytes);
    void clear();
    void new_search();

    bool probe(U64 key, TTEntry& entry) const;
    Move probe_move(U64 key) const;

    void store(U64 key, int depth, int score, Move best_move,
               TTBound bound, int ply);

    static int score_to_tt(int score, int ply);
    static int score_from_tt(int score, int ply);

private:
    std::vector<TTEntry> table;
    std::size_t mask;
    uint8_t generation;
};

extern TranspositionTable g_tt;

#endif /* TT_H */
