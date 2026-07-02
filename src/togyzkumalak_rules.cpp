#include "togyz/togyzkumalak_rules.hpp"

#include <algorithm>
#include <iomanip>

#include "togyz/position_hash.hpp"

// SOW_MASKS[start_pit][side][count]
static uint128 SOW_MASKS[18][2][163];

static void init_sow_masks() {
  static const bool initialized = [] {
    for (int start = 0; start < 18; ++start) {
      for (int count = 0; count < 163; ++count) {
        uint128 m1 = 0, m2 = 0;
        for (int i = 0; i < count; ++i) {
          int pit = (start + i) % 18;
          if (pit < 9)
            m1 += (uint128(1) << (pit * 8));
          else
            m2 += (uint128(1) << ((pit - 9) * 8));
        }
        SOW_MASKS[start][0][count] = m1;
        SOW_MASKS[start][1][count] = m2;
      }
    }
    return true;
  }();
  (void)initialized;
}

ToguzEnv::ToguzEnv() {
  init_sow_masks();
  random_buffer.resize(100000);
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> dist(0, 8);
  for (int i = 0; i < 100000; ++i) random_buffer[i] = dist(rng);
  random_idx = 0;
  max_steps = 10000;
  reset();
}

void ToguzEnv::reset() {
  board.side1 = board.side2 = 0;
  for (int i = 0; i < 18; ++i) board.set(i, INITIAL_STONES);
  kazans = {0, 0};
  tuzduks = {-1, -1};
  to_play = PLAYER_1;
  steps = 0;
  winner_code = -2;
  reset_repetition_history();
}

void ToguzEnv::step(int action) {
  if (is_game_over()) return;
  int p = to_play, opp = 1 - p;
  int start = p * 9 + action;
  int stones = board.get(start);
  if (stones == 0) return;

  if (stones == 1) {
    board.set(start, 0);
    int target = landing_pit(start, stones);
    board.set(target, board.get(target) + 1);
    for (int i = 0; i < 2; ++i)
      if (tuzduks[i] != -1) {
        int tp = (1 - i) * 9 + tuzduks[i];
        int s = board.get(tp);
        if (s > 0) {
          kazans[i] += s;
          board.set(tp, 0);
        }
      }
    int r = target / 9, c = target % 9;
    if (r == opp) {
      int cnt = board.get(target);
      if (cnt == 3 && tuzduks[p] == -1 && c != 8 && tuzduks[opp] != c) {
        tuzduks[p] = c;
        kazans[p] += 3;
        board.set(target, 0);
      } else if (cnt % 2 == 0) {
        kazans[p] += cnt;
        board.set(target, 0);
      }
    }
  } else {
    board.set(start, 1);
    int sc = stones - 1;
    int next = (start + 1) % 18;
    board.side1 += SOW_MASKS[next][0][sc];
    board.side2 += SOW_MASKS[next][1][sc];

    for (int i = 0; i < 2; ++i)
      if (tuzduks[i] != -1) {
        int tp = (1 - i) * 9 + tuzduks[i];
        int s = board.get(tp);
        if (s > 0) {
          kazans[i] += s;
          board.set(tp, 0);
        }
      }

    int last = landing_pit(start, stones);
    int r = last / 9, c = last % 9;
    if (r == opp) {
      int val = board.get(last);
      if (val == 3 && tuzduks[p] == -1 && c != 8 && tuzduks[opp] != c) {
        tuzduks[p] = c;
        kazans[p] += 3;
        board.set(last, 0);
      } else if (val % 2 == 0) {
        kazans[p] += val;
        board.set(last, 0);
      }
    }
  }
  steps++;
  if (kazans[p] >= WIN_THRESHOLD)
    winner_code = p;
  else if (kazans[opp] >= WIN_THRESHOLD)
    winner_code = opp;
  else if (steps >= max_steps)
    determine_winner_by_kazans();
  to_play = opp;
  if (winner_code != -2) return;
  std::array<int, 9> acts;
  update_legal_actions(acts);
  bool any = false;
  for (int i = 0; i < 9; ++i)
    if (acts[i]) any = true;
  if (!any) {
    for (int i = 0; i < 9; ++i) {
      kazans[0] += board.get(i);
      kazans[1] += board.get(i + 9);
      board.set(i, 0);
      board.set(i + 9, 0);
    }
    determine_winner_by_kazans();
  }
  if (winner_code == -2) record_repetition_and_check_draw();
}

void ToguzEnv::update_legal_actions(std::array<int, 9>& actions) {
  int opp = 1 - to_play;
  for (int i = 0; i < 9; ++i) {
    int pit = to_play * 9 + i;
    actions[i] = (board.get(pit) > 0 && tuzduks[opp] != i) ? 1 : 0;
  }
}

std::vector<int> ToguzEnv::generate_moves() {
  std::array<int, 9> acts;
  update_legal_actions(acts);
  std::vector<int> res;
  for (int i = 0; i < 9; ++i)
    if (acts[i]) res.push_back(i);
  return res;
}

int ToguzEnv::get_random_move() {
  std::array<int, 9> acts;
  update_legal_actions(acts);
  bool ok = false;
  for (int i = 0; i < 9; ++i)
    if (acts[i]) ok = true;
  if (!ok) return -1;
  while (true) {
    int m = random_buffer[random_idx];
    random_idx = (random_idx + 1) % 100000;
    if (acts[m]) return m;
  }
}

bool ToguzEnv::is_game_over() const { return winner_code != -2 || steps >= max_steps; }
void ToguzEnv::determine_winner_by_kazans() {
  if (kazans[0] > kazans[1])
    winner_code = 0;
  else if (kazans[1] > kazans[0])
    winner_code = 1;
  else
    winner_code = -1;
}
std::string ToguzEnv::get_result_string() const {
  if (!is_game_over()) return "";
  return (winner_code == 0) ? "P1 wins" : (winner_code == 1 ? "P2 wins" : "Draw");
}

uint64_t ToguzEnv::position_hash() const {
  Zobrist::init();
  return Zobrist::get_initial_hash_bitboard(board, kazans, tuzduks, to_play);
}

void ToguzEnv::reset_repetition_history() {
  history_stack.clear();
  history_stack.reserve(256);
  history_stack.push_back(position_hash());
}

bool ToguzEnv::record_repetition_and_check_draw() {
  uint64_t hash = position_hash();
  history_stack.push_back(hash);
  int seen = 0;
  for (int i = static_cast<int>(history_stack.size()) - 1; i >= 0; i -= 2) {
    if (history_stack[static_cast<size_t>(i)] == hash) {
      ++seen;
      if (seen >= 3) {
        winner_code = -1;
        return true;
      }
    }
  }
  return false;
}

int ToguzEnv::current_repetition_count() const {
  uint64_t hash = position_hash();
  int seen = 0;
  for (int i = static_cast<int>(history_stack.size()) - 1; i >= 0; i -= 2) {
    if (history_stack[static_cast<size_t>(i)] == hash) {
      ++seen;
    }
  }
  return seen;
}

void ToguzEnv::step_search(Bitboard& b, std::array<int, 2>& k, std::array<int, 2>& t, int action,
                           int turn, int& s_val, int max_s, int& wc, int& n_play, bool& term,
                           uint64_t& hash) {
  hash ^= Zobrist::to_play[turn];
  hash ^= Zobrist::kazan[0][k[0]];
  hash ^= Zobrist::kazan[1][k[1]];
  hash ^= Zobrist::tuzduk[0][t[0] + 1];
  hash ^= Zobrist::tuzduk[1][t[1] + 1];

  int p = turn, opp = 1 - p;
  int start = p * 9 + action;
  int stones = b.get(start);

  if (stones == 1) {
    hash ^= (p == 0 ? Zobrist::board_p1[action][1] : Zobrist::board_p2[action][1]);
    b.set(start, 0);
    hash ^= (p == 0 ? Zobrist::board_p1[action][0] : Zobrist::board_p2[action][0]);

    int tgt = landing_pit(start, stones);
    int old = b.get(tgt);
    hash ^= (tgt < 9 ? Zobrist::board_p1[tgt][old] : Zobrist::board_p2[tgt - 9][old]);
    b.set(tgt, old + 1);
    hash ^= (tgt < 9 ? Zobrist::board_p1[tgt][old + 1] : Zobrist::board_p2[tgt - 9][old + 1]);

    for (int i = 0; i < 2; ++i)
      if (t[i] != -1) {
        int tp = (1 - i) * 9 + t[i];
        int s = b.get(tp);
        if (s > 0) {
          k[i] += s;
          hash ^= (tp < 9 ? Zobrist::board_p1[tp][s] : Zobrist::board_p2[tp - 9][s]);
          b.set(tp, 0);
          hash ^= (tp < 9 ? Zobrist::board_p1[tp][0] : Zobrist::board_p2[tp - 9][0]);
        }
      }

    if (tgt / 9 == opp) {
      int cnt = b.get(tgt);
      int col = tgt % 9;
      if (cnt == 3 && t[p] == -1 && col != 8 && t[opp] != col) {
        t[p] = col;
        k[p] += 3;
        hash ^= (opp == 0 ? Zobrist::board_p1[col][3] : Zobrist::board_p2[col][3]);
        b.set(tgt, 0);
        hash ^= (opp == 0 ? Zobrist::board_p1[col][0] : Zobrist::board_p2[col][0]);
      } else if (cnt % 2 == 0) {
        k[p] += cnt;
        hash ^= (opp == 0 ? Zobrist::board_p1[col][cnt] : Zobrist::board_p2[col][cnt]);
        b.set(tgt, 0);
        hash ^= (opp == 0 ? Zobrist::board_p1[col][0] : Zobrist::board_p2[col][0]);
      }
    }
  } else {
    hash ^= (p == 0 ? Zobrist::board_p1[action][stones] : Zobrist::board_p2[action][stones]);
    b.set(start, 1);
    hash ^= (p == 0 ? Zobrist::board_p1[action][1] : Zobrist::board_p2[action][1]);

    int sc = stones - 1;
    int next = (start + 1) % 18;
    Bitboard b_old = b;
    b.side1 += SOW_MASKS[next][0][sc];
    b.side2 += SOW_MASKS[next][1][sc];

    for (int i = 0; i < sc; ++i) {
      int cur = (next + i) % 18;
      int v_old = b_old.get(cur);
      if (cur < 9) {
        hash ^= Zobrist::board_p1[cur][v_old];
        hash ^= Zobrist::board_p1[cur][v_old + 1];
      } else {
        hash ^= Zobrist::board_p2[cur - 9][v_old];
        hash ^= Zobrist::board_p2[cur - 9][v_old + 1];
      }
      b_old.set(cur, v_old + 1);
    }

    for (int i = 0; i < 2; ++i)
      if (t[i] != -1) {
        int tp = (1 - i) * 9 + t[i];
        int s = b.get(tp);
        if (s > 0) {
          k[i] += s;
          hash ^= (tp < 9 ? Zobrist::board_p1[tp][s] : Zobrist::board_p2[tp - 9][s]);
          b.set(tp, 0);
          hash ^= (tp < 9 ? Zobrist::board_p1[tp][0] : Zobrist::board_p2[tp - 9][0]);
        }
      }

    int last = landing_pit(start, stones);
    if (last / 9 == opp) {
      int val = b.get(last);
      int col = last % 9;
      if (val == 3 && t[p] == -1 && col != 8 && t[opp] != col) {
        t[p] = col;
        k[p] += 3;
        hash ^= (opp == 0 ? Zobrist::board_p1[col][3] : Zobrist::board_p2[col][3]);
        b.set(last, 0);
        hash ^= (opp == 0 ? Zobrist::board_p1[col][0] : Zobrist::board_p2[col][0]);
      } else if (val % 2 == 0) {
        k[p] += val;
        hash ^= (opp == 0 ? Zobrist::board_p1[col][val] : Zobrist::board_p2[col][val]);
        b.set(last, 0);
        hash ^= (opp == 0 ? Zobrist::board_p1[col][0] : Zobrist::board_p2[col][0]);
      }
    }
  }

  s_val++;
  wc = -2;
  if (k[p] >= WIN_THRESHOLD)
    wc = p;
  else if (k[opp] >= WIN_THRESHOLD)
    wc = opp;
  else if (s_val >= max_s) {
    if (k[0] > k[1])
      wc = 0;
    else if (k[1] > k[0])
      wc = 1;
    else
      wc = -1;
  }

  n_play = opp;
  term = (wc != -2);
  if (!term) {
    bool ok = false;
    int no = 1 - n_play;
    for (int i = 0; i < 9; ++i)
      if (b.get(n_play * 9 + i) > 0 && t[no] != i) {
        ok = true;
        break;
      }
    if (!ok) {
      for (int i = 0; i < 9; ++i) {
        int p1_stones = b.get(i);
        if (p1_stones > 0) {
          k[0] += p1_stones;
          hash ^= Zobrist::board_p1[i][p1_stones];
          b.set(i, 0);
          hash ^= Zobrist::board_p1[i][0];
        }
        int p2_stones = b.get(i + 9);
        if (p2_stones > 0) {
          k[1] += p2_stones;
          hash ^= Zobrist::board_p2[i][p2_stones];
          b.set(i + 9, 0);
          hash ^= Zobrist::board_p2[i][0];
        }
      }
      if (k[0] > k[1])
        wc = 0;
      else if (k[1] > k[0])
        wc = 1;
      else
        wc = -1;
      term = true;
    }
  }
  hash ^= Zobrist::to_play[n_play];
  hash ^= Zobrist::kazan[0][k[0]];
  hash ^= Zobrist::kazan[1][k[1]];
  hash ^= Zobrist::tuzduk[0][t[0] + 1];
  hash ^= Zobrist::tuzduk[1][t[1] + 1];
}

int ToguzEnv::generate_moves_search(const Bitboard& b, const std::array<int, 2>& t, int player,
                                    std::array<int, 9>& out) {
  int count = 0, no = 1 - player;
  for (int i = 0; i < 9; ++i)
    if (b.get(player * 9 + i) > 0 && t[no] != i) out[count++] = i;
  return count;
}

namespace {

double terminal_eval(int winner_code, int perspective) {
  if (winner_code == -1) return 0.0;
  return winner_code == perspective ? 1000000.0 : -1000000.0;
}

double simple_tuzdyk_value(int pos) {
  if (pos < 0 || pos >= 8) return 0.0;
  return 650.0 + 70.0 * pos;
}

}  // namespace

double ToguzEnv::evaluate(const Bitboard& b, const std::array<int, 2>& k,
                          const std::array<int, 2>& t, int perspective, int to_play_idx, int wc,
                          bool term) {
  if (term) return terminal_eval(wc, perspective);

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

  double kazan_weight = 108.0 + 0.28 * total_captured;
  if (total_on_board < 30) kazan_weight += 18.0;

  double score = (k[perspective] - k[opponent]) * kazan_weight;
  score += simple_tuzdyk_value(t[perspective]) - simple_tuzdyk_value(t[opponent]);
  score += (my_stones - opponent_stones) * (total_on_board < 24 ? 18.0 : 4.0);
  score += (my_legal_cells - opponent_legal_cells) * 18.0;

  if (t[perspective] != -1 && t[opponent] == -1) {
    score += 170.0;
  } else if (t[perspective] == -1 && t[opponent] != -1) {
    score -= 170.0;
  }
  score += (to_play_idx == perspective) ? 10.0 : -10.0;
  return score;
}

void ToguzEnv::render(const std::string& mode, const std::string& p1_name,
                      const std::string& p2_name) const {
  if (mode != "terminal") return;
  std::cout << "Kazan: " << p1_name << "=" << kazans[0] << " " << p2_name << "=" << kazans[1]
            << "\n";

  // P2 pits: 17...9 (side2 indices 8...0)
  for (int i = 8; i >= 0; --i) {
    if (tuzduks[0] == i)
      std::cout << "[ X]";
    else
      std::cout << "[" << std::setw(2) << board.get(i + 9) << "]";
  }
  std::cout << "\n";

  // P1 pits: 0...8 (side1 indices 0...8)
  for (int i = 0; i < 9; ++i) {
    if (tuzduks[1] == i)
      std::cout << "[ X]";
    else
      std::cout << "[" << std::setw(2) << board.get(i) << "]";
  }
  std::cout << "\n";
}

int ToguzEnv::setup_random_position(int plies) {
  reset();
  for (int i = 0; i < plies; ++i) {
    if (is_game_over()) break;
    int m = get_random_move();
    if (m == -1) break;
    step(m);
  }
  return steps;
}

int ToguzEnv::setup_balanced_reduced_position(std::mt19937_64& rng, int max_reduction_per_pit,
                                              int max_total_reduction) {
  reset();

  max_reduction_per_pit = std::clamp(max_reduction_per_pit, 1, INITIAL_STONES - 1);
  int absolute_max = max_reduction_per_pit * NUM_PITS;
  max_total_reduction = std::clamp(max_total_reduction, 1, absolute_max);

  std::uniform_int_distribution<int> total_dist(1, max_total_reduction);
  int target_reduction = total_dist(rng);

  auto make_reductions = [&](int total) {
    std::array<int, NUM_PITS> reductions{};
    while (total > 0) {
      std::array<int, NUM_PITS> candidates{};
      int count = 0;
      for (int i = 0; i < NUM_PITS; ++i) {
        if (reductions[i] < max_reduction_per_pit) {
          candidates[count++] = i;
        }
      }
      std::uniform_int_distribution<int> pick(0, count - 1);
      reductions[candidates[pick(rng)]]++;
      total--;
    }
    return reductions;
  };

  auto side1_reductions = make_reductions(target_reduction);
  auto side2_reductions = make_reductions(target_reduction);

  for (int i = 0; i < NUM_PITS; ++i) {
    board.set(i, INITIAL_STONES - side1_reductions[i]);
    board.set(i + NUM_PITS, INITIAL_STONES - side2_reductions[i]);
  }

  kazans = {0, 0};
  tuzduks = {-1, -1};
  to_play = PLAYER_1;
  steps = 0;
  winner_code = -2;
  reset_repetition_history();
  return target_reduction;
}
