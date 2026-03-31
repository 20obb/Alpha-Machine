/*
 * main.cpp — Alpha-Machine Engine Entry Point
 * ====================================
 *
 * Global setup, test bench suites, and initialization sequences.
 */

#include "types.h"
#include "bitboard.h"
#include "zobrist.h"
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "evaluate.h"

#include <cstdio>
#include <cstring>
#include <chrono>

/* ═══════════════════ Perft Test Suite ══════════════════════ */

struct PerftTest {
    const char* fen;
    int         depth;
    U64         expected;
    const char* name;
};

/*
 * Standard global chess validation situations.
 * Achieving 100% equivalence verifies complete absence of logic faults in move generation.
 */
static const PerftTest PERFT_TESTS[] = {
    /* Baseline Starting Format */
    { START_FEN, 1, 20, "Start Pos depth 1" },
    { START_FEN, 2, 400, "Start Pos depth 2" },
    { START_FEN, 3, 8902, "Start Pos depth 3" },
    { START_FEN, 4, 197281, "Start Pos depth 4" },
    { START_FEN, 5, 4865609, "Start Pos depth 5" },

    /* Kiwipete: Dense extreme tactical layout */
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      1, 48, "Kiwipete depth 1" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      2, 2039, "Kiwipete depth 2" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      3, 97862, "Kiwipete depth 3" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      4, 4085603, "Kiwipete depth 4" },

    /* Position 3: Pawn-centric advancement dynamics */
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      1, 14, "Pos3 depth 1" },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      2, 191, "Pos3 depth 2" },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      3, 2812, "Pos3 depth 3" },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      4, 43238, "Pos3 depth 4" },
    { "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
      5, 674624, "Pos3 depth 5" },

    /* Position 4: Symmetrical En Passant heavy sequence */
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      1, 6, "Pos4 depth 1" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      2, 264, "Pos4 depth 2" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      3, 9467, "Pos4 depth 3" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      4, 422333, "Pos4 depth 4" },

    /* Position 5: Obfuscated castling rules test */
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      1, 44, "Pos5 depth 1" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      2, 1486, "Pos5 depth 2" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      3, 62379, "Pos5 depth 3" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      4, 2103487, "Pos5 depth 4" },
};

static const int NUM_PERFT_TESTS = sizeof(PERFT_TESTS) / sizeof(PERFT_TESTS[0]);

/* ═══════════════════ Run Tests ════════════════════════════ */

static int run_perft_tests() {
    int passed = 0;
    int failed = 0;

    printf("════════════════════════════════════════════════════════════\n");
    printf("  Alpha-Machine Chess Engine — Perft Test Suite\n");
    printf("════════════════════════════════════════════════════════════\n\n");

    for (int t = 0; t < NUM_PERFT_TESTS; t++) {
        const PerftTest& test = PERFT_TESTS[t];
        Position pos;
        pos.set_fen(test.fen);

        auto start = std::chrono::high_resolution_clock::now();
        U64 result = perft(pos, test.depth);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        bool ok = (result == test.expected);
        if (ok) passed++; else failed++;

        printf("  %s %s — got %llu, expected %llu (%.1f ms)\n",
               ok ? "✅" : "❌",
               test.name,
               (unsigned long long)result,
               (unsigned long long)test.expected,
               ms);

        /* On failure trace, trigger divide sequence for manual mapping */
        if (!ok && test.depth <= 2) {
            printf("  ─── Divide ───\n");
            perft_divide(pos, test.depth);
        }
    }

    printf("\n════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed, %d total\n",
           passed, failed, passed + failed);
    printf("════════════════════════════════════════════════════════════\n\n");

    return failed;
}

/* ─── Mate-in-N Tests ────────────────────────────────────── */

struct MateTest {
    const char* fen;
    int         depth;
    const char* name;
};

static const MateTest MATE_TESTS[] = {
    /* Mate in 1 */
    { "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",
      3, "Scholar's Mate in 1 (Qxf7#)" },
    /* Mate in 2 */
    { "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1",
      5, "Mate in 2" },
};

static void run_mate_tests() {
    printf("\n════════════════════════════════════════════════════════════\n");
    printf("  Mate-in-N Tests\n");
    printf("════════════════════════════════════════════════════════════\n\n");

    int n = sizeof(MATE_TESTS) / sizeof(MATE_TESTS[0]);
    for (int t = 0; t < n; t++) {
        const MateTest& test = MATE_TESTS[t];
        Position pos;
        pos.set_fen(test.fen);
        pos.print();

        SearchInfo info;
        info.depth = test.depth;

        printf("  Searching: %s (depth %d)\n", test.name, test.depth);
        SearchResult result = search(pos, info);
        printf("  Result: best=%s score=%d nodes=%d tt_hits=%llu tt_cutoffs=%llu\n\n",
               move_to_string(result.best_move).c_str(),
               result.score, result.nodes,
               (unsigned long long)result.tt_hits,
               (unsigned long long)result.tt_cutoffs);
    }
}

/* ═══════════════════ Main ═════════════════════════════════ */

int main(int argc, char* argv[]) {
    /* ─── Initialize Core Operating Architecture ──────────── */
    printf("⚡ Initializing Alpha-Machine Chess Engine...\n");

    auto init_start = std::chrono::high_resolution_clock::now();

    init_bitboards();
    Zobrist::init();

    auto init_end = std::chrono::high_resolution_clock::now();
    double init_ms = std::chrono::duration<double, std::milli>(init_end - init_start).count();
    printf("  ✅ Initialized in %.1f ms\n\n", init_ms);

    /* ─── Command CLI Parsing ──────────────────────────────── */
    if (argc > 1 && strcmp(argv[1], "perft") == 0) {
        return run_perft_tests();
    }

    if (argc > 1 && strcmp(argv[1], "search") == 0) {
        /* Initiate search sequence directly from base formation */
        Position pos;
        pos.set_startpos();
        pos.print();

        int depth = (argc > 2) ? atoi(argv[2]) : 6;
        printf("  Searching to depth %d...\n\n", depth);

        SearchInfo info;
        info.depth = depth;

        SearchResult result = search(pos, info);
        printf("\n  Best move: %s, Score: %d cp, Nodes: %d, TT hits: %llu, TT cutoffs: %llu\n\n",
               move_to_string(result.best_move).c_str(),
               result.score, result.nodes,
               (unsigned long long)result.tt_hits,
               (unsigned long long)result.tt_cutoffs);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "mate") == 0) {
        run_mate_tests();
        return 0;
    }

    /* ─── Default Invocation Action Sequence ────────────── */
    Position pos;
    pos.set_startpos();
    pos.print();

    /* Identify legal root moves */
    MoveList moves;
    generate_moves(pos, moves);
    printf("  Legal moves (pseudo): %d\n\n", moves.count);

    /* Perform Rapid Validation Perft */
    printf("  Running quick Perft...\n");
    for (int d = 1; d <= 5; d++) {
        auto start = std::chrono::high_resolution_clock::now();
        U64 nodes = perft(pos, d);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double nps = (ms > 0) ? nodes / ms * 1000.0 : 0;

        printf("  Perft(%d) = %12llu  (%.1f ms, %.0f nps)\n",
               d, (unsigned long long)nodes, ms, nps);
    }
    printf("\n");

    /* Formulate Fast Base Search */
    printf("  Searching startpos to depth 6...\n\n");
    SearchInfo info;
    info.depth = 6;
    SearchResult result = search(pos, info);
    printf("\n  Best move: %s, Score: %d cp, Nodes: %d, TT hits: %llu, TT cutoffs: %llu\n\n",
           move_to_string(result.best_move).c_str(),
           result.score, result.nodes,
           (unsigned long long)result.tt_hits,
           (unsigned long long)result.tt_cutoffs);

    /* Run baseline tactical validation */
    run_mate_tests();

    return 0;
}
