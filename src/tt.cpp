#include "tt.h"

#include <algorithm>

namespace {

std::size_t highest_power_of_two(std::size_t value) {
    std::size_t power = 1;
    while ((power << 1) <= value)
        power <<= 1;
    return power;
}

}  // namespace

TranspositionTable g_tt;

TranspositionTable::TranspositionTable(std::size_t megabytes)
    : mask(0), generation(0) {
    resize(megabytes);
}

void TranspositionTable::resize(std::size_t megabytes) {
    std::size_t bytes = std::max<std::size_t>(megabytes, 1) * 1024 * 1024;
    std::size_t entries = std::max<std::size_t>(bytes / sizeof(TTEntry), 1);
    entries = highest_power_of_two(entries);

    table.assign(entries, TTEntry());
    mask = entries - 1;
    generation = 0;
}

void TranspositionTable::clear() {
    std::fill(table.begin(), table.end(), TTEntry());
    generation = 0;
}

void TranspositionTable::new_search() {
    generation++;
    if (generation == 0)
        generation = 1;
}

bool TranspositionTable::probe(U64 key, TTEntry& entry) const {
    if (table.empty())
        return false;

    const TTEntry& slot = table[key & mask];
    if (slot.depth < 0 || slot.key != key)
        return false;

    entry = slot;
    return true;
}

Move TranspositionTable::probe_move(U64 key) const {
    TTEntry entry;
    return probe(key, entry) ? entry.best_move : MOVE_NONE;
}

void TranspositionTable::store(U64 key, int depth, int score, Move best_move,
                               TTBound bound, int ply) {
    if (table.empty())
        return;

    TTEntry& slot = table[key & mask];
    int stored_score = score_to_tt(score, ply);

    if (slot.key == key && slot.depth >= 0) {
        if (best_move != MOVE_NONE)
            slot.best_move = best_move;

        if (depth >= slot.depth || bound == TT_BOUND_EXACT) {
            slot.score = stored_score;
            slot.depth = static_cast<int16_t>(depth);
            slot.bound = static_cast<uint8_t>(bound);
        }

        slot.generation = generation;
        return;
    }

    bool replace = slot.depth < 0
                || slot.generation != generation
                || depth >= slot.depth;

    if (!replace)
        return;

    slot.key = key;
    slot.score = stored_score;
    slot.best_move = best_move;
    slot.depth = static_cast<int16_t>(depth);
    slot.bound = static_cast<uint8_t>(bound);
    slot.generation = generation;
}

int TranspositionTable::score_to_tt(int score, int ply) {
    if (score > VALUE_MATE - MAX_PLY)
        return score + ply;
    if (score < -VALUE_MATE + MAX_PLY)
        return score - ply;
    return score;
}

int TranspositionTable::score_from_tt(int score, int ply) {
    if (score > VALUE_MATE - MAX_PLY)
        return score - ply;
    if (score < -VALUE_MATE + MAX_PLY)
        return score + ply;
    return score;
}
