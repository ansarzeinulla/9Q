#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "togyz/dag_v1_search.hpp"
#include "togyz/dag_v2_search.hpp"
#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"

// Search benchmark behind the speed numbers in the paper (Table 8, Sec. 9.2).
//
// Each run is an iterative-deepening ladder from the initial position with a
// fresh 256 MB transposition table: depth 1, 2, ..., --max-depth, keeping the
// table between depths exactly as the timed search does. For every depth it
// prints the nodes of that iteration, the cumulative nodes and time, and the
// cumulative nodes per second. "Depth reached in the budget" is the deepest
// iteration whose cumulative time fits inside --budget seconds.
//
// DAGv1 is the engine described in the paper (16-byte TT, TT-guided ordering).
// "DAGv1, no ordering" searches moves in generation order, which reproduces
// the with/without move-ordering comparison. DAGv2 is the newer engine.
//
// Node counts are deterministic; times are the median over --reps runs.
//
// Usage: tg_bench_search [--max-depth 9] [--budget 1.0] [--reps 5]

namespace {

using Clock = std::chrono::steady_clock;

enum class Engine { kDagV1, kDagV1NoOrdering, kDagV2 };

const char* engine_name(Engine engine) {
  switch (engine) {
    case Engine::kDagV1:
      return "DAGv1 (paper engine)";
    case Engine::kDagV1NoOrdering:
      return "DAGv1, no move ordering";
    case Engine::kDagV2:
      return "DAGv2";
  }
  return "?";
}

struct DepthRow {
  int depth = 0;
  uint64_t nodes = 0;      // nodes searched in this iteration
  uint64_t cum_nodes = 0;  // nodes searched in iterations 1..depth
  double cum_ms = 0.0;     // wall time of iterations 1..depth
};

std::vector<DepthRow> run_ladder(Engine engine, int max_depth, const Evaluator& evaluator) {
  ToguzEnv env;  // initial position
  // Fresh, zeroed tables allocated before the clock starts, so the timings
  // measure search only (allocating 256 MB takes ~30 ms on its own).
  dag_v1::init_tt(256);
  dag_v2::init_tt(256);
  dag_v1::g_move_ordering = (engine != Engine::kDagV1NoOrdering);

  std::vector<DepthRow> rows;
  uint64_t cum_nodes = 0;
  double cum_ms = 0.0;
  for (int depth = 1; depth <= max_depth; ++depth) {
    auto start = Clock::now();
    uint64_t nodes = 0;
    if (engine == Engine::kDagV2) {
      dag_v2::get_best_move(env, depth, evaluator);
      nodes = dag_v2::get_last_stats().nodes;
    } else {
      dag_v1::get_best_move(env, depth, evaluator);
      nodes = dag_v1::get_last_stats().nodes;
    }
    double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    cum_nodes += nodes;
    cum_ms += ms;
    rows.push_back(DepthRow{depth, nodes, cum_nodes, cum_ms});
  }
  dag_v1::g_move_ordering = true;
  return rows;
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  size_t n = values.size();
  return (n % 2 == 1) ? values[n / 2] : 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

// Runs the ladder `reps` times and returns one row per depth with median times.
std::vector<DepthRow> bench_engine(Engine engine, int max_depth, int reps,
                                   const Evaluator& evaluator) {
  std::vector<std::vector<DepthRow>> runs;
  for (int r = 0; r < reps; ++r) runs.push_back(run_ladder(engine, max_depth, evaluator));

  std::vector<DepthRow> result = runs[0];
  for (int d = 0; d < max_depth; ++d) {
    std::vector<double> times;
    for (const auto& run : runs) {
      times.push_back(run[static_cast<size_t>(d)].cum_ms);
      if (run[static_cast<size_t>(d)].nodes != result[static_cast<size_t>(d)].nodes) {
        std::printf("warning: %s depth %d node count differs between runs\n", engine_name(engine),
                    d + 1);
      }
    }
    result[static_cast<size_t>(d)].cum_ms = median(times);
  }
  return result;
}

void print_table(Engine engine, const std::vector<DepthRow>& rows, double budget_s) {
  std::printf("\n### %s\n\n", engine_name(engine));
  std::printf(
      "| Depth | Nodes (this depth) | Cumulative nodes | Cumulative time (ms) | Nodes/s |\n");
  std::printf("|---:|---:|---:|---:|---:|\n");
  const DepthRow* reached = nullptr;
  for (const auto& row : rows) {
    double nps = row.cum_ms > 0.0 ? row.cum_nodes / (row.cum_ms / 1000.0) : 0.0;
    std::printf("| %d | %llu | %llu | %.1f | %.0f |\n", row.depth,
                static_cast<unsigned long long>(row.nodes),
                static_cast<unsigned long long>(row.cum_nodes), row.cum_ms, nps);
    if (row.cum_ms <= budget_s * 1000.0) reached = &row;
  }
  if (reached != nullptr) {
    double nps = reached->cum_ms > 0.0 ? reached->cum_nodes / (reached->cum_ms / 1000.0) : 0.0;
    std::printf("\nDepth reached in %.2f s: %d (%llu nodes in %.1f ms, %.0f nodes/s)\n", budget_s,
                reached->depth, static_cast<unsigned long long>(reached->cum_nodes),
                reached->cum_ms, nps);
  } else {
    std::printf("\nDepth 1 already exceeds the %.2f s budget.\n", budget_s);
  }
}

}  // namespace

int main(int argc, char** argv) {
  int max_depth = 9;
  double budget_s = 1.0;
  int reps = 5;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--max-depth" && i + 1 < argc) {
      max_depth = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--budget" && i + 1 < argc) {
      budget_s = std::max(0.001, std::atof(argv[++i]));
    } else if (arg == "--reps" && i + 1 < argc) {
      reps = std::max(1, std::atoi(argv[++i]));
    } else {
      std::printf("Usage: %s [--max-depth 9] [--budget 1.0] [--reps 5]\n", argv[0]);
      return arg == "--help" ? 0 : 1;
    }
  }

  std::printf("# 9Q search benchmark\n\n");
  std::printf("Initial position, single thread, fresh 256 MB TT per run.\n");
  std::printf("Max depth %d, budget %.2f s, median of %d runs.\n", max_depth, budget_s, reps);
#if defined(__VERSION__)
  std::printf("Compiler: %s\n", __VERSION__);
#endif

  const HeuristicEvaluator evaluator;
  std::vector<DepthRow> with_ordering;
  std::vector<DepthRow> without_ordering;
  for (Engine engine : {Engine::kDagV1, Engine::kDagV1NoOrdering, Engine::kDagV2}) {
    std::vector<DepthRow> rows = bench_engine(engine, max_depth, reps, evaluator);
    print_table(engine, rows, budget_s);
    if (engine == Engine::kDagV1) with_ordering = rows;
    if (engine == Engine::kDagV1NoOrdering) without_ordering = rows;
  }

  uint64_t with_nodes = with_ordering.back().cum_nodes;
  uint64_t without_nodes = without_ordering.back().cum_nodes;
  std::printf("\n### Move ordering (DAGv1, depth %d)\n\n", max_depth);
  std::printf("Cumulative nodes without ordering: %llu\n",
              static_cast<unsigned long long>(without_nodes));
  std::printf("Cumulative nodes with ordering:    %llu\n",
              static_cast<unsigned long long>(with_nodes));
  if (with_nodes > 0) {
    std::printf("Reduction: %.2fx\n", static_cast<double>(without_nodes) / with_nodes);
  }
  return 0;
}
