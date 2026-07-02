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

class HeuristicEvaluator : public Evaluator {
 public:
  double evaluate_position(const BoardState& state) const override;
  uint64_t cache_key() const override;
  std::string description() const override;
};

#endif
