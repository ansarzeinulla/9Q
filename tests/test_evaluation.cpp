#include <gtest/gtest.h>

#include <array>

#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"

TEST(EvaluationTest, BasicHeuristicEvaluation) {
  HeuristicEvaluator eval;

  EXPECT_FALSE(eval.description().empty());
  EXPECT_NE(eval.cache_key(), 0);

  Bitboard board;
  for (int i = 0; i < 18; ++i) {
    board.set(i, 9);  // Initial setup
  }

  std::array<int, 2> kazans = {0, 0};
  std::array<int, 2> tuzduks = {-1, -1};

  BoardState state{board, kazans, tuzduks, PLAYER_1, -2, PLAYER_1, false};

  double initial_score = eval.evaluate_position(state);
  // At balanced start with P1's turn + P1's perspective: +10.0 tempo bonus.
  EXPECT_NEAR(initial_score, 10.0, 1e-4);

  // If P1's turn but perspective is P2, score should be -10.0.
  BoardState state_p2_initial{board, kazans, tuzduks, PLAYER_1, -2, PLAYER_2, false};
  double initial_score_p2 = eval.evaluate_position(state_p2_initial);
  EXPECT_NEAR(initial_score_p2, -10.0, 1e-4);

  // If P1 has stones in Kazan, evaluation from P1's perspective should be > initial.
  std::array<int, 2> kazans_p1_ahead = {5, 0};
  BoardState state_p1_ahead{board, kazans_p1_ahead, tuzduks, PLAYER_1, -2, PLAYER_1, false};
  double score_p1_ahead = eval.evaluate_position(state_p1_ahead);
  EXPECT_GT(score_p1_ahead, 10.0);

  // From P2's perspective, the score should be negative when P1 is ahead.
  BoardState state_p2_perspective{board, kazans_p1_ahead, tuzduks, PLAYER_1, -2, PLAYER_2, false};
  double score_p2_perspective = eval.evaluate_position(state_p2_perspective);
  EXPECT_LT(score_p2_perspective, -10.0);
}
