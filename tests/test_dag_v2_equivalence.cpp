#include <gtest/gtest.h>

#include <random>
#include <vector>

#include "togyz/dag_v1_search.hpp"
#include "togyz/dag_v2_search.hpp"

namespace {

std::vector<ToguzEnv> make_corpus() {
  std::vector<ToguzEnv> corpus;
  for (uint64_t seed : {1337ULL, 42ULL, 7ULL}) {
    std::mt19937_64 rng(seed);
    for (int i = 0; i < 30; ++i) {
      ToguzEnv env;
      if (i % 3 == 2) {
        env.setup_random_position(12);
      } else {
        env.setup_balanced_reduced_position(rng);
      }
      if (!env.is_game_over()) corpus.push_back(env);
    }
  }
  return corpus;
}

// v2 in baseline mode (all features off) must be move-, score-, and
// node-count-identical to v1 at every fixed depth.
TEST(DagV2Equivalence, BaselineMatchesV1Exactly) {
  const HeuristicEvaluator heuristic;
  auto corpus = make_corpus();
  ASSERT_GT(corpus.size(), 50u);
  const auto baseline = dag_v2::SearchOptions::v1_baseline();
  for (int depth = 1; depth <= 5; ++depth) {
    for (const auto& env : corpus) {
      dag_v1::init_tt(16);
      int v1_move = dag_v1::get_best_move(env, depth, heuristic);
      auto v1_stats = dag_v1::get_last_stats();

      dag_v2::init_tt(16);
      int v2_move = dag_v2::get_best_move(env, depth, heuristic, baseline);
      auto v2_stats = dag_v2::get_last_stats();

      ASSERT_EQ(v1_move, v2_move) << "depth=" << depth;
      ASSERT_EQ(v1_stats.nodes, v2_stats.nodes) << "depth=" << depth;
    }
  }
}

TEST(DagV2Equivalence, BaselineRootEvalsMatchV1) {
  const HeuristicEvaluator heuristic;
  auto corpus = make_corpus();
  for (int depth = 2; depth <= 4; ++depth) {
    for (size_t i = 0; i < corpus.size(); i += 5) {
      const auto& env = corpus[i];
      dag_v1::init_tt(16);
      auto v1_evals = dag_v1::get_all_moves_with_evals(env, depth, heuristic);
      dag_v2::init_tt(16);
      auto v2_evals = dag_v2::get_all_moves_with_evals(env, depth, heuristic);
      ASSERT_EQ(v1_evals.size(), v2_evals.size());
      for (size_t m = 0; m < v1_evals.size(); ++m) {
        ASSERT_EQ(v1_evals[m].move, v2_evals[m].move);
        ASSERT_DOUBLE_EQ(v1_evals[m].eval, v2_evals[m].eval);
        ASSERT_EQ(v1_evals[m].skipped, v2_evals[m].skipped);
      }
    }
  }
}

// Move-ordering features (killers, history) and PVS must never change the
// root minimax value at a fixed depth — only how fast it is found.
TEST(DagV2Equivalence, FeaturesPreserveRootScore) {
  const HeuristicEvaluator heuristic;
  auto corpus = make_corpus();
  std::vector<dag_v2::SearchOptions> configs;
  {
    dag_v2::SearchOptions o = dag_v2::SearchOptions::v1_baseline();
    o.use_killers = true;
    configs.push_back(o);
  }
  {
    dag_v2::SearchOptions o = dag_v2::SearchOptions::v1_baseline();
    o.use_history = true;
    configs.push_back(o);
  }
  {
    dag_v2::SearchOptions o = dag_v2::SearchOptions::v1_baseline();
    o.use_pvs = true;
    configs.push_back(o);
  }

  const auto baseline = dag_v2::SearchOptions::v1_baseline();
  for (int depth = 3; depth <= 5; ++depth) {
    for (size_t i = 0; i < corpus.size(); i += 4) {
      const auto& env = corpus[i];
      dag_v2::init_tt(16);
      double base_score = dag_v2::get_root_score(env, depth, heuristic, baseline);
      for (const auto& cfg : configs) {
        dag_v2::init_tt(16);
        double score = dag_v2::get_root_score(env, depth, heuristic, cfg);
        ASSERT_DOUBLE_EQ(base_score, score) << "depth=" << depth << " pos=" << i;
      }
    }
  }
}

// Full feature set (including aspiration + iterative deepening) must still
// return the same root score at a fixed target depth.
TEST(DagV2Equivalence, FullFeatureSetPreservesRootScore) {
  const HeuristicEvaluator heuristic;
  auto corpus = make_corpus();
  const auto baseline = dag_v2::SearchOptions::v1_baseline();
  const dag_v2::SearchOptions full;
  for (int depth = 3; depth <= 5; ++depth) {
    for (size_t i = 0; i < corpus.size(); i += 4) {
      const auto& env = corpus[i];
      dag_v2::init_tt(16);
      double base_score = dag_v2::get_root_score(env, depth, heuristic, baseline);
      dag_v2::init_tt(16);
      double score = dag_v2::get_root_score(env, depth, heuristic, full);
      ASSERT_DOUBLE_EQ(base_score, score) << "depth=" << depth << " pos=" << i;
    }
  }
}

}  // namespace
