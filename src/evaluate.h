/*
 * evaluate.h — Evaluation Function Interface
 * ==================================
 *
 * Position evaluation returns a score in centipawns:
 *   - Positive = good for White
 *   - Negative = good for Black
 *
 * The current version relies on:
 *   1. Material balance (difference in piece values)
 *   2. Piece-Square Tables (positional bonuses based on piece location)
 */

#ifndef EVALUATE_H
#define EVALUATE_H

#include "position.h"

/*
 * evaluate(pos)
 * Evaluates the position from the perspective of the side to move.
 * Positive = good for the current player.
 */
int evaluate(const Position& pos);

#endif /* EVALUATE_H */
