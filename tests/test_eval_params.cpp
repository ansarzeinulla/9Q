#include <gtest/gtest.h>

#include <random>

#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"

namespace {

// Default-params HeuristicEvaluator must reproduce ToguzEnv::evaluate exactly
// (bit-identical doubles) across a broad corpus of positions.
TEST(EvalParams, DefaultParamsMatchLegacyEvaluate) {
  std::mt19937_64 rng(1337);
  int checked = 0;
  HeuristicEvaluator eval;
  for (int p = 0; p < 250; ++p) {
    ToguzEnv env;
    if (p % 2 == 0) {
      env.setup_balanced_reduced_position(rng);
    } else {
      env.setup_random_position(20);
    }
    for (int perspective = 0; perspective < 2; ++perspective) {
      for (int to_play = 0; to_play < 2; ++to_play) {
        double legacy = ToguzEnv::evaluate(env.board, env.kazans, env.tuzduks, perspective, to_play,
                                           env.winner_code, env.is_game_over());
        BoardState st{env.board,       env.kazans,  env.tuzduks,       to_play,
                      env.winner_code, perspective, env.is_game_over()};
        EXPECT_DOUBLE_EQ(legacy, eval.evaluate_position(st));
        ++checked;
      }
    }
  }
  EXPECT_EQ(checked, 1000);
}

TEST(EvalParams, TerminalPositionsMatch) {
  HeuristicEvaluator eval;
  ToguzEnv env;
  env.reset();
  for (int wc : {-1, 0, 1}) {
    for (int perspective = 0; perspective < 2; ++perspective) {
      double legacy =
          ToguzEnv::evaluate(env.board, env.kazans, env.tuzduks, perspective, 0, wc, true);
      BoardState st{env.board, env.kazans, env.tuzduks, 0, wc, perspective, true};
      EXPECT_DOUBLE_EQ(legacy, eval.evaluate_position(st));
    }
  }
}

TEST(EvalParams, NonDefaultParamsChangeScore) {
  ToguzEnv env;
  env.reset();
  EvalParams p;
  p.tempo = 0.0;
  HeuristicEvaluator custom(p);
  HeuristicEvaluator standard;
  BoardState st{env.board, env.kazans, env.tuzduks, 0, -2, 0, false};
  EXPECT_NE(standard.evaluate_position(st), custom.evaluate_position(st));
  // Tempo is the only asymmetry in the initial position: zeroing it gives 0.
  EXPECT_DOUBLE_EQ(custom.evaluate_position(st), 0.0);
}

TEST(EvalParams, CacheKeyStableForDefaultsDistinctForCustom) {
  HeuristicEvaluator a, b;
  EXPECT_EQ(a.cache_key(), 0x5354415448455552ULL);
  EXPECT_EQ(a.cache_key(), b.cache_key());
  EvalParams p;
  p.mobility = 20.0;
  HeuristicEvaluator custom(p);
  EXPECT_NE(custom.cache_key(), a.cache_key());
}

}  // namespace
