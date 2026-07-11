#include "togyz/evaluation.hpp"

#include <cstring>

using namespace std;

namespace {

uint64_t mix_double(uint64_t h, double v) {
  uint64_t bits = 0;
  static_assert(sizeof(bits) == sizeof(v), "double must be 64-bit");
  memcpy(&bits, &v, sizeof(bits));
  h ^= bits + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
  return h;
}

uint64_t params_cache_key(const EvalParams& p) {
  uint64_t h = 0x5354415448455552ULL;
  h = mix_double(h, p.win_score);
  h = mix_double(h, p.tuzdyk_base);
  h = mix_double(h, p.tuzdyk_per_pos);
  h = mix_double(h, p.kazan_base);
  h = mix_double(h, p.kazan_per_captured);
  h = mix_double(h, p.kazan_endgame_bonus);
  h = mix_double(h, static_cast<double>(p.endgame_board_threshold));
  h = mix_double(h, p.material_endgame);
  h = mix_double(h, p.material_normal);
  h = mix_double(h, static_cast<double>(p.material_threshold));
  h = mix_double(h, p.mobility);
  h = mix_double(h, p.lone_tuzdyk);
  h = mix_double(h, p.tempo);
  return h;
}

bool is_default(const EvalParams& p) {
  const EvalParams d{};
  return p.win_score == d.win_score && p.tuzdyk_base == d.tuzdyk_base &&
         p.tuzdyk_per_pos == d.tuzdyk_per_pos && p.kazan_base == d.kazan_base &&
         p.kazan_per_captured == d.kazan_per_captured &&
         p.kazan_endgame_bonus == d.kazan_endgame_bonus &&
         p.endgame_board_threshold == d.endgame_board_threshold &&
         p.material_endgame == d.material_endgame && p.material_normal == d.material_normal &&
         p.material_threshold == d.material_threshold && p.mobility == d.mobility &&
         p.lone_tuzdyk == d.lone_tuzdyk && p.tempo == d.tempo;
}

double terminal_eval(const EvalParams& p, int winner_code, int perspective) {
  if (winner_code == -1) return 0.0;
  return winner_code == perspective ? p.win_score : -p.win_score;
}

double simple_tuzdyk_value(const EvalParams& p, int pos) {
  if (pos < 0 || pos >= 8) return 0.0;
  return p.tuzdyk_base + p.tuzdyk_per_pos * pos;
}

}  // namespace

HeuristicEvaluator::HeuristicEvaluator(const EvalParams& params) : params_(params) {
  if (!is_default(params_)) cache_key_ = params_cache_key(params_);
}

double HeuristicEvaluator::evaluate_position(const BoardState& state) const {
  const EvalParams& p = params_;
  const Bitboard& b = state.board;
  const std::array<int, 2>& k = state.kazans;
  const std::array<int, 2>& t = state.tuzduks;
  const int perspective = state.perspective_player;

  if (state.terminal) return terminal_eval(p, state.winner_code, perspective);

  int opponent = 1 - perspective;
  int my_stones = 0;
  int opponent_stones = 0;
  int my_legal_cells = 0;
  int opponent_legal_cells = 0;
  for (int i = 0; i < NUM_PITS; ++i) {
    int my_cell = b.get(perspective * NUM_PITS + i);
    int opponent_cell = b.get(opponent * NUM_PITS + i);
    my_stones += my_cell;
    opponent_stones += opponent_cell;
    if (my_cell > 0 && t[opponent] != i) my_legal_cells++;
    if (opponent_cell > 0 && t[perspective] != i) opponent_legal_cells++;
  }

  int total_on_board = my_stones + opponent_stones;
  int total_captured = k[PLAYER_1] + k[PLAYER_2];

  double kazan_weight = p.kazan_base + p.kazan_per_captured * total_captured;
  if (total_on_board < p.endgame_board_threshold) kazan_weight += p.kazan_endgame_bonus;

  double score = (k[perspective] - k[opponent]) * kazan_weight;
  score += simple_tuzdyk_value(p, t[perspective]) - simple_tuzdyk_value(p, t[opponent]);
  score += (my_stones - opponent_stones) *
           (total_on_board < p.material_threshold ? p.material_endgame : p.material_normal);
  score += (my_legal_cells - opponent_legal_cells) * p.mobility;

  if (t[perspective] != -1 && t[opponent] == -1) {
    score += p.lone_tuzdyk;
  } else if (t[perspective] == -1 && t[opponent] != -1) {
    score -= p.lone_tuzdyk;
  }
  score += (state.to_play == perspective) ? p.tempo : -p.tempo;
  return score;
}

uint64_t HeuristicEvaluator::cache_key() const { return cache_key_; }

string HeuristicEvaluator::description() const { return "heuristic"; }
