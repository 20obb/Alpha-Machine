/*
 * main.cpp — Alpha-Machine Engine Entry Point
 * ==========================================
 */

#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "types.h"
#include "zobrist.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct PerftTest {
    const char* fen;
    int         depth;
    U64         expected;
    const char* name;
};

static const PerftTest PERFT_TESTS[] = {
    { START_FEN, 1, 20, "Start Pos depth 1" },
    { START_FEN, 2, 400, "Start Pos depth 2" },
    { START_FEN, 3, 8902, "Start Pos depth 3" },
    { START_FEN, 4, 197281, "Start Pos depth 4" },
    { START_FEN, 5, 4865609, "Start Pos depth 5" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      1, 48, "Kiwipete depth 1" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      2, 2039, "Kiwipete depth 2" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      3, 97862, "Kiwipete depth 3" },
    { "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
      4, 4085603, "Kiwipete depth 4" },
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
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      1, 6, "Pos4 depth 1" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      2, 264, "Pos4 depth 2" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      3, 9467, "Pos4 depth 3" },
    { "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
      4, 422333, "Pos4 depth 4" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      1, 44, "Pos5 depth 1" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      2, 1486, "Pos5 depth 2" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      3, 62379, "Pos5 depth 3" },
    { "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
      4, 2103487, "Pos5 depth 4" },
};

struct MateTest {
    const char* fen;
    int         depth;
    const char* name;
};

static const MateTest MATE_TESTS[] = {
    { "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",
      3, "Scholar's Mate in 1 (Qxf7#)" },
    { "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1",
      5, "Mate in 2" },
};

struct BenchmarkPosition {
    const char* id;
    const char* fen;
    int         depth;
};

static const BenchmarkPosition BENCHMARK_POSITIONS[] = {
    { "startpos", START_FEN, 6 },
    { "kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5 },
    { "pos3_endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 6 },
    { "pos5_tactical", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5 },
    { "mate2", "kbK5/pp6/1P6/8/8/8/8/R7 w - - 0 1", 5 },
};

struct BenchmarkRecord {
    std::string stage_label;
    std::string position_id;
    std::string mode;
    int run_index;
    int depth;
    int tt_mb;
    std::string bestmove;
    int score;
    uint64_t nodes;
    uint64_t tt_probes;
    uint64_t tt_hits;
    double tt_hit_rate;
    uint64_t tt_cutoffs;
    double tt_cutoff_rate;
    uint64_t tt_stores;
    int64_t time_ms;
};

namespace {

constexpr int BENCH_TT_MB = 16;
constexpr int BENCH_RUNS_PER_MODE = 10;

struct GoCommand {
    int max_depth;
    int64_t movetime_ms;
    int64_t soft_time_ms;
    int64_t hard_time_ms;
    int64_t wtime_ms;
    int64_t btime_ms;
    int64_t winc_ms;
    int64_t binc_ms;
    int movestogo;
    bool infinite;

    GoCommand()
        : max_depth(MAX_DEPTH), movetime_ms(0), soft_time_ms(0), hard_time_ms(0),
          wtime_ms(-1), btime_ms(-1), winc_ms(0), binc_ms(0), movestogo(0),
          infinite(false) {}
};

struct Stats {
    uint64_t min_value;
    uint64_t max_value;
    long double sum;
    int count;

    Stats() : min_value(0), max_value(0), sum(0.0L), count(0) {}

    void add(uint64_t value) {
        if (count == 0) {
            min_value = value;
            max_value = value;
        } else {
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
        }

        sum += value;
        count++;
    }

    double avg() const {
        return count ? static_cast<double>(sum / count) : 0.0;
    }
};

struct UciSession {
    Position position;
    std::thread worker;
    std::mutex mutex;
    std::atomic<bool> stop_flag;

    UciSession() : stop_flag(false) {
        position.set_startpos();
    }

    ~UciSession() {
        stop_and_join();
    }

    void stop_and_join() {
        stop_flag.store(true, std::memory_order_relaxed);
        if (worker.joinable())
            worker.join();
        stop_flag.store(false, std::memory_order_relaxed);
    }

    void cmd_uci() {
        std::printf("id name Alpha-Machine\n");
        std::printf("id author OpenAI Codex\n");
        std::printf("uciok\n");
        std::fflush(stdout);
    }

    void cmd_isready() {
        std::printf("readyok\n");
        std::fflush(stdout);
    }

    void cmd_ucinewgame() {
        stop_and_join();
        std::lock_guard<std::mutex> lock(mutex);
        position.set_startpos();
        g_tt.clear();
    }

    bool set_position(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2)
            return false;

        stop_and_join();

        Position next_position;
        std::size_t index = 1;

        if (tokens[index] == "startpos") {
            next_position.set_startpos();
            index++;
        } else if (tokens[index] == "fen") {
            index++;
            std::ostringstream fen_stream;
            bool first = true;

            while (index < tokens.size() && tokens[index] != "moves") {
                if (!first)
                    fen_stream << ' ';
                fen_stream << tokens[index++];
                first = false;
            }

            if (first)
                return false;

            next_position.set_fen(fen_stream.str());
        } else {
            return false;
        }

        if (index < tokens.size() && tokens[index] == "moves")
            index++;

        for (; index < tokens.size(); index++) {
            Move move;
            if (!parse_legal_move(next_position, tokens[index], move) || !next_position.do_move(move)) {
                std::printf("info string illegal move %s\n", tokens[index].c_str());
                std::fflush(stdout);
                return false;
            }
        }

        std::lock_guard<std::mutex> lock(mutex);
        position = next_position;
        return true;
    }

    static GoCommand parse_go_command(const std::vector<std::string>& tokens) {
        GoCommand command;

        for (std::size_t i = 1; i < tokens.size(); i++) {
            if (tokens[i] == "depth" && i + 1 < tokens.size()) {
                command.max_depth = std::max(1, std::atoi(tokens[++i].c_str()));
            } else if (tokens[i] == "movetime" && i + 1 < tokens.size()) {
                command.movetime_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "softtime" && i + 1 < tokens.size()) {
                command.soft_time_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "hardtime" && i + 1 < tokens.size()) {
                command.hard_time_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "wtime" && i + 1 < tokens.size()) {
                command.wtime_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "btime" && i + 1 < tokens.size()) {
                command.btime_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "winc" && i + 1 < tokens.size()) {
                command.winc_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "binc" && i + 1 < tokens.size()) {
                command.binc_ms = std::max<int64_t>(0, std::atoll(tokens[++i].c_str()));
            } else if (tokens[i] == "movestogo" && i + 1 < tokens.size()) {
                command.movestogo = std::max(0, std::atoi(tokens[++i].c_str()));
            } else if (tokens[i] == "infinite") {
                command.infinite = true;
            }
        }

        return command;
    }

    static void apply_clock_limits(const Position& pos, const GoCommand& command,
                                   SearchLimits& limits) {
        if (command.infinite)
            return;

        if (command.movetime_ms > 0) {
            limits.movetime_ms = command.movetime_ms;
            return;
        }

        int64_t side_time_ms = (pos.side_to_move() == WHITE) ? command.wtime_ms : command.btime_ms;
        int64_t side_inc_ms = (pos.side_to_move() == WHITE) ? command.winc_ms : command.binc_ms;

        if (side_time_ms < 0)
            return;

        if (side_time_ms <= 1) {
            limits.soft_time_ms = 1;
            limits.hard_time_ms = 1;
            return;
        }

        int moves_to_go = (command.movestogo > 0) ? command.movestogo : 30;
        int64_t reserve_ms = std::clamp<int64_t>(side_time_ms / 20, 5, 200);
        int64_t usable_ms = std::max<int64_t>(side_time_ms - reserve_ms, 1);
        int64_t base_ms = usable_ms / std::max(moves_to_go, 1);
        int64_t increment_share_ms = side_inc_ms / 2;

        int64_t soft_ms = std::max<int64_t>(base_ms + increment_share_ms, 1);
        soft_ms = std::min<int64_t>(soft_ms, usable_ms);

        int64_t hard_cap_ms = std::max<int64_t>(usable_ms - 1, 1);
        int64_t hard_ms = std::max<int64_t>(soft_ms + std::max<int64_t>(soft_ms / 2, 5), soft_ms);
        hard_ms = std::min<int64_t>(hard_ms, hard_cap_ms);

        limits.soft_time_ms = soft_ms;
        limits.hard_time_ms = hard_ms;
    }

    void cmd_go(const std::vector<std::string>& tokens) {
        stop_and_join();

        Position position_copy;
        {
            std::lock_guard<std::mutex> lock(mutex);
            position_copy = position;
        }

        SearchLimits limits;
        limits.enable_info_output = true;
        limits.ordering_stage = ORDERING_STAGE2B;
        GoCommand command = parse_go_command(tokens);
        limits.max_depth = command.max_depth;
        limits.soft_time_ms = command.soft_time_ms;
        limits.hard_time_ms = command.hard_time_ms;
        apply_clock_limits(position_copy, command, limits);

        stop_flag.store(false, std::memory_order_relaxed);
        worker = std::thread([this, position_copy, limits]() mutable {
            SearchResult result = search(position_copy, limits, &stop_flag);
            std::printf("bestmove %s\n", move_to_string(result.best_move).c_str());
            std::fflush(stdout);
        });
    }
};

void initialize_engine() {
    static bool initialized = false;
    if (initialized)
        return;

    init_bitboards();
    Zobrist::init();
    initialized = true;
}

bool parse_stage(const std::string& text, OrderingStage& stage) {
    if (text == "stage1") {
        stage = ORDERING_STAGE1;
        return true;
    }
    if (text == "stage2a") {
        stage = ORDERING_STAGE2A;
        return true;
    }
    if (text == "stage2b") {
        stage = ORDERING_STAGE2B;
        return true;
    }
    return false;
}

std::vector<std::string> split_tokens(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;

    while (input >> token)
        tokens.push_back(token);

    return tokens;
}

int run_perft_tests() {
    int passed = 0;
    int failed = 0;

    std::printf("=============================================\n");
    std::printf("  Alpha-Machine Chess Engine — Perft Tests\n");
    std::printf("=============================================\n\n");

    for (const PerftTest& test : PERFT_TESTS) {
        Position pos;
        pos.set_fen(test.fen);

        U64 result = perft(pos, test.depth);
        bool ok = (result == test.expected);

        if (ok)
            passed++;
        else
            failed++;

        std::printf("  %s %s — got %llu, expected %llu\n",
                    ok ? "PASS" : "FAIL",
                    test.name,
                    static_cast<unsigned long long>(result),
                    static_cast<unsigned long long>(test.expected));

        if (!ok && test.depth <= 2) {
            std::printf("  Divide:\n");
            perft_divide(pos, test.depth);
        }
    }

    std::printf("\nResults: %d passed, %d failed, %d total\n",
                passed, failed, passed + failed);
    return failed;
}

void run_mate_tests() {
    std::printf("=============================================\n");
    std::printf("  Mate-in-N Tests\n");
    std::printf("=============================================\n\n");

    for (const MateTest& test : MATE_TESTS) {
        Position pos;
        pos.set_fen(test.fen);
        pos.print();

        SearchLimits limits;
        limits.max_depth = test.depth;
        limits.enable_info_output = true;
        SearchResult result = search(pos, limits);

        std::printf("  Result: best=%s score=%d nodes=%llu tt_hits=%llu tt_cutoffs=%llu\n\n",
                    move_to_string(result.best_move).c_str(),
                    result.score,
                    static_cast<unsigned long long>(result.nodes),
                    static_cast<unsigned long long>(result.tt_hits),
                    static_cast<unsigned long long>(result.tt_cutoffs));
    }
}

void write_benchmark_runs_csv(const std::string& path,
                              const std::vector<BenchmarkRecord>& records) {
    std::ofstream out(path);
    out << "stage_label,position_id,mode,run_index,depth,tt_mb,bestmove,score,nodes,"
           "tt_probes,tt_hits,tt_hit_rate,tt_cutoffs,tt_cutoff_rate,tt_stores,time_ms\n";
    out << std::fixed << std::setprecision(6);

    for (const BenchmarkRecord& record : records) {
        out << record.stage_label << ','
            << record.position_id << ','
            << record.mode << ','
            << record.run_index << ','
            << record.depth << ','
            << record.tt_mb << ','
            << record.bestmove << ','
            << record.score << ','
            << record.nodes << ','
            << record.tt_probes << ','
            << record.tt_hits << ','
            << record.tt_hit_rate << ','
            << record.tt_cutoffs << ','
            << record.tt_cutoff_rate << ','
            << record.tt_stores << ','
            << record.time_ms << '\n';
    }
}

void write_benchmark_summary_csv(const std::string& path,
                                 const std::vector<BenchmarkRecord>& records,
                                 const std::string& stage_label) {
    std::ofstream out(path);
    out << "stage_label,position_id,mode,depth,tt_mb,canonical_bestmove,distinct_best_moves,"
           "stable_move_ratio,nodes_min,nodes_avg,nodes_max,tt_probes_min,tt_probes_avg,tt_probes_max,"
           "tt_hits_min,tt_hits_avg,tt_hits_max,tt_cutoffs_min,tt_cutoffs_avg,tt_cutoffs_max,"
           "tt_stores_min,tt_stores_avg,tt_stores_max,time_ms_min,time_ms_avg,time_ms_max\n";
    out << std::fixed << std::setprecision(6);

    for (const BenchmarkPosition& position : BENCHMARK_POSITIONS) {
        for (const char* mode : { "cold", "warm" }) {
            std::vector<const BenchmarkRecord*> group;
            for (const BenchmarkRecord& record : records) {
                if (record.position_id == position.id && record.mode == mode)
                    group.push_back(&record);
            }

            if (group.empty())
                continue;

            std::map<std::string, int> move_counts;
            Stats nodes_stats;
            Stats probes_stats;
            Stats hits_stats;
            Stats cutoffs_stats;
            Stats stores_stats;
            Stats time_stats;

            for (const BenchmarkRecord* record : group) {
                move_counts[record->bestmove]++;
                nodes_stats.add(record->nodes);
                probes_stats.add(record->tt_probes);
                hits_stats.add(record->tt_hits);
                cutoffs_stats.add(record->tt_cutoffs);
                stores_stats.add(record->tt_stores);
                time_stats.add(static_cast<uint64_t>(record->time_ms));
            }

            std::string canonical_bestmove = group.front()->bestmove;
            int canonical_count = 0;
            for (const auto& entry : move_counts) {
                if (entry.second > canonical_count) {
                    canonical_bestmove = entry.first;
                    canonical_count = entry.second;
                }
            }

            double stable_ratio = group.empty()
                ? 0.0
                : static_cast<double>(canonical_count) / static_cast<double>(group.size());

            out << stage_label << ','
                << position.id << ','
                << mode << ','
                << position.depth << ','
                << BENCH_TT_MB << ','
                << canonical_bestmove << ','
                << move_counts.size() << ','
                << stable_ratio << ','
                << nodes_stats.min_value << ','
                << nodes_stats.avg() << ','
                << nodes_stats.max_value << ','
                << probes_stats.min_value << ','
                << probes_stats.avg() << ','
                << probes_stats.max_value << ','
                << hits_stats.min_value << ','
                << hits_stats.avg() << ','
                << hits_stats.max_value << ','
                << cutoffs_stats.min_value << ','
                << cutoffs_stats.avg() << ','
                << cutoffs_stats.max_value << ','
                << stores_stats.min_value << ','
                << stores_stats.avg() << ','
                << stores_stats.max_value << ','
                << time_stats.min_value << ','
                << time_stats.avg() << ','
                << time_stats.max_value << '\n';
        }
    }
}

int run_benchmark(OrderingStage stage) {
    std::vector<BenchmarkRecord> records;
    std::string stage_label = ordering_stage_name(stage);

    g_tt.resize(BENCH_TT_MB);

    for (const BenchmarkPosition& position : BENCHMARK_POSITIONS) {
        for (const char* mode : { "cold", "warm" }) {
            bool warm_mode = std::string(mode) == "warm";
            g_tt.clear();

            for (int run_index = 1; run_index <= BENCH_RUNS_PER_MODE; run_index++) {
                if (!warm_mode)
                    g_tt.clear();

                Position pos;
                pos.set_fen(position.fen);

                SearchLimits limits;
                limits.max_depth = position.depth;
                limits.ordering_stage = stage;

                SearchResult result = search(pos, limits);

                BenchmarkRecord record;
                record.stage_label = stage_label;
                record.position_id = position.id;
                record.mode = mode;
                record.run_index = run_index;
                record.depth = position.depth;
                record.tt_mb = BENCH_TT_MB;
                record.bestmove = move_to_string(result.best_move);
                record.score = result.score;
                record.nodes = result.nodes;
                record.tt_probes = result.tt_probes;
                record.tt_hits = result.tt_hits;
                record.tt_hit_rate = result.tt_probes
                    ? static_cast<double>(result.tt_hits) / static_cast<double>(result.tt_probes)
                    : 0.0;
                record.tt_cutoffs = result.tt_cutoffs;
                record.tt_cutoff_rate = result.tt_probes
                    ? static_cast<double>(result.tt_cutoffs) / static_cast<double>(result.tt_probes)
                    : 0.0;
                record.tt_stores = result.tt_stores;
                record.time_ms = result.time_ms;
                records.push_back(record);

                std::printf("bench %s %s run %d/%d best=%s nodes=%llu probes=%llu hits=%llu cutoffs=%llu\n",
                            stage_label.c_str(),
                            position.id,
                            run_index,
                            BENCH_RUNS_PER_MODE,
                            record.bestmove.c_str(),
                            static_cast<unsigned long long>(record.nodes),
                            static_cast<unsigned long long>(record.tt_probes),
                            static_cast<unsigned long long>(record.tt_hits),
                            static_cast<unsigned long long>(record.tt_cutoffs));
            }
        }
    }

    std::string runs_path = "bench_" + stage_label + "_runs.csv";
    std::string summary_path = "bench_" + stage_label + "_summary.csv";

    write_benchmark_runs_csv(runs_path, records);
    write_benchmark_summary_csv(summary_path, records, stage_label);

    std::printf("wrote %s\n", runs_path.c_str());
    std::printf("wrote %s\n", summary_path.c_str());
    return 0;
}

void run_search_command(int depth, OrderingStage stage) {
    Position pos;
    pos.set_startpos();
    pos.print();

    SearchLimits limits;
    limits.max_depth = depth;
    limits.ordering_stage = stage;
    limits.enable_info_output = true;

    SearchResult result = search(pos, limits);
    std::printf("\nBest move: %s, Score: %d cp, Depth: %d, Nodes: %llu, TT hits: %llu, TT cutoffs: %llu\n",
                move_to_string(result.best_move).c_str(),
                result.score,
                result.depth,
                static_cast<unsigned long long>(result.nodes),
                static_cast<unsigned long long>(result.tt_hits),
                static_cast<unsigned long long>(result.tt_cutoffs));
}

void run_uci_loop() {
    UciSession session;
    std::string line;

    while (std::getline(std::cin, line)) {
        std::vector<std::string> tokens = split_tokens(line);
        if (tokens.empty())
            continue;

        if (tokens[0] == "uci") {
            session.cmd_uci();
        } else if (tokens[0] == "isready") {
            session.cmd_isready();
        } else if (tokens[0] == "ucinewgame") {
            session.cmd_ucinewgame();
        } else if (tokens[0] == "position") {
            session.set_position(tokens);
        } else if (tokens[0] == "go") {
            session.cmd_go(tokens);
        } else if (tokens[0] == "stop") {
            session.stop_and_join();
        } else if (tokens[0] == "quit") {
            session.stop_and_join();
            break;
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    initialize_engine();

    if (argc > 1 && std::string(argv[1]) == "perft")
        return run_perft_tests();

    if (argc > 1 && std::string(argv[1]) == "search") {
        int depth = (argc > 2) ? std::atoi(argv[2]) : 6;
        OrderingStage stage = ORDERING_STAGE2B;
        if (argc > 3)
            parse_stage(argv[3], stage);
        run_search_command(depth, stage);
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "mate") {
        run_mate_tests();
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "bench") {
        OrderingStage stage = ORDERING_STAGE1;
        if (argc > 2)
            parse_stage(argv[2], stage);
        return run_benchmark(stage);
    }

    run_uci_loop();
    return 0;
}
