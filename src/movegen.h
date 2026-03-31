/*
 * movegen.h — Move Generation 
 * ====================================
 *
 * Provides a function to generate all pseudo-legal moves for a position.
 * Then we verify them using make_move() to rule out moving into check.
 */
#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "position.h"
#include "move.h"

/* Generate all pseudo-legal moves and add them to the provided list */
void generate_moves(const Position& pos, MoveList& list);

/* Generate ONLY capturing moves (used in Quiescence Search) */
void generate_captures(const Position& pos, MoveList& list);

/* Run Perft (Performance Test) for a specific depth and return the number of valid leaf nodes */
U64 perft(Position& pos, int depth);

/* Run Perft and print the number of nodes per initial move (useful for debugging) */
void perft_divide(Position& pos, int depth);

#endif /* MOVEGEN_H */
