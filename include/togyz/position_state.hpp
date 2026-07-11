#ifndef POSITION_STATE_HPP
#define POSITION_STATE_HPP

#include <array>

#include "togyz/togyzkumalak_rules.hpp"

// Lightweight, copyable snapshot of the game position — the state a search
// actually needs, decoupled from the full ToguzEnv (history, rendering, RNG).
struct PositionState {
  Bitboard board;
  std::array<int, 2> kazans{0, 0};
  std::array<int, 2> tuzduks{-1, -1};
  int to_play = PLAYER_1;
};

inline PositionState capture_position(const ToguzEnv& env) {
  return PositionState{env.board, env.kazans, env.tuzduks, env.to_play};
}

#endif
