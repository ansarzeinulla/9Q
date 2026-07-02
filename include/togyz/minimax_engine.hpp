#ifndef MINIMAX_ENGINE_HPP
#define MINIMAX_ENGINE_HPP

#include <vector>

#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"

namespace minimax_engine {

int move_order_key(const Bitboard& b, int player, int move);
void sort_moves_in_place(const Bitboard& b, int player, std::array<int, 9>& moves, int count);

double minimax_raw(Bitboard& board, std::array<int, 2>& kazans, std::array<int, 2>& tuzduks,
                   int to_play_idx, int steps, int max_steps, int winner_code, int depth,
                   double alpha, double beta, bool maximizing_player, int perspective_player,
                   std::vector<Bitboard>& history_board,
                   std::vector<std::array<int, 2>>& history_kazans,
                   std::vector<std::array<int, 2>>& history_tuzduks,
                   std::vector<std::array<int, 9>>& history_moves, uint64_t current_hash,
                   const Evaluator& evaluator);

struct MoveEval {
  int move;
  double eval;
  bool skipped = false;
};

struct TimedMoveResult {
  int move = -1;
  int completed_depth = 0;
  double eval = 0.0;
  bool timed_out = false;
};

int get_best_move(const ToguzEnv& env, int depth = 3);
int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator);
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move, int max_depth,
                                    const Evaluator& evaluator);
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth = 64);
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth = 3);
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth,
                                               const Evaluator& evaluator);
}  // namespace minimax_engine

#endif
