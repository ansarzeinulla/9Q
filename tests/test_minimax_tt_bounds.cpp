// The transposition table must not change what the search returns.
//
// Alpha-beta produces an exact value only inside the search window; outside it the
// value is a bound. Storing a bound and later reusing it as exact can change the
// chosen move, so every entry records which kind of value it holds.
#include <gtest/gtest.h>

#include <random>

#include "togyz/evaluation.hpp"
#include "togyz/minimax_engine.hpp"
#include "togyz/togyzkumalak_rules.hpp"

namespace {

int best_move(const ToguzEnv& env, int depth, const Evaluator& ev, bool use_tt) {
  minimax_engine::clear_transposition_table();
  minimax_engine::set_tt_enabled(use_tt);
  return minimax_engine::get_best_move(env, depth, ev);
}

}  // namespace

TEST(MinimaxTTBounds, TableDoesNotChangeTheChosenMove) {
  HeuristicEvaluator ev;
  std::mt19937_64 rng(2026);
  int checked = 0;

  for (int i = 0; i < 40; ++i) {
    ToguzEnv env;
    env.reset();
    const int plies = 4 + static_cast<int>(rng() % 24);
    for (int p = 0; p < plies && !env.is_game_over(); ++p) {
      const std::vector<int> moves = env.generate_moves();
      if (moves.empty()) break;
      env.step(moves[rng() % moves.size()]);
    }
    if (env.is_game_over()) continue;
    ++checked;

    const int without_tt = best_move(env, 5, ev, false);
    const int with_tt = best_move(env, 5, ev, true);
    EXPECT_EQ(without_tt, with_tt) << "position " << i << " differs with and without the table";
  }

  minimax_engine::set_tt_enabled(true);
  EXPECT_GT(checked, 20);
}
