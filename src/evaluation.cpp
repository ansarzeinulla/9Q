#include "togyz/evaluation.hpp"

using namespace std;

double HeuristicEvaluator::evaluate_position(const BoardState &state) const {
  return ToguzEnv::evaluate(state.board, state.kazans, state.tuzduks,
                            state.perspective_player, state.to_play,
                            state.winner_code, state.terminal);
}

uint64_t HeuristicEvaluator::cache_key() const {
  return 0x5354415448455552ULL;
}

string HeuristicEvaluator::description() const {
  return "heuristic";
}
