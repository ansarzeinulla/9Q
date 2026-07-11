#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include <array>
#include <cstdint>
#include <string>

#include "togyz/togyzkumalak_rules.hpp"

struct BoardState {
  const Bitboard& board;
  const std::array<int, 2>& kazans;
  const std::array<int, 2>& tuzduks;
  int to_play = PLAYER_1;
  int winner_code = -2;
  int perspective_player = PLAYER_1;
  bool terminal = false;
};

class Evaluator {
 public:
  virtual ~Evaluator() = default;
  virtual double evaluate_position(const BoardState& state) const = 0;
  virtual uint64_t cache_key() const = 0;
  virtual std::string description() const = 0;
};

// Tunable weights of the classic heuristic. Defaults reproduce the historic
// ToguzEnv::evaluate constants bit-for-bit.
struct EvalParams {
  double win_score = 1000000.0;
  double tuzdyk_base = 650.0;
  double tuzdyk_per_pos = 70.0;
  double kazan_base = 108.0;
  double kazan_per_captured = 0.28;
  double kazan_endgame_bonus = 18.0;
  int endgame_board_threshold = 30;
  double material_endgame = 18.0;
  double material_normal = 4.0;
  int material_threshold = 24;
  double mobility = 18.0;
  double lone_tuzdyk = 170.0;
  double tempo = 10.0;
};

class HeuristicEvaluator : public Evaluator {
 public:
  HeuristicEvaluator() = default;
  explicit HeuristicEvaluator(const EvalParams& params);
  double evaluate_position(const BoardState& state) const override;
  uint64_t cache_key() const override;
  std::string description() const override;
  const EvalParams& params() const { return params_; }

 private:
  EvalParams params_{};
  uint64_t cache_key_ = 0x5354415448455552ULL;
};

#endif
