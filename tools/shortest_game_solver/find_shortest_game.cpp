#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

// Standalone shortest-game verifier for the rule set used in the paper.
//
// This file deliberately does not include the main engine. The shortest-game
// proof is easier to audit when the complete state representation, move logic,
// and breadth search live in one small translation unit.
constexpr int kPits = 9;
constexpr int kTotalPits = 18;
constexpr int kWinThreshold = 82;
constexpr int kOngoing = -2;

struct State {
  std::array<uint8_t, kTotalPits> board{};
  std::array<uint8_t, 2> kazans{};
  std::array<int8_t, 2> tuzdyks{};
  uint8_t to_play = 0;
  uint8_t steps = 0;
  int8_t winner = kOngoing;
};

struct MoveResult {
  State state;
  std::string notation;
  int mover_gain = 0;
};

struct DepthStats {
  int halfmove = 0;
  uint64_t frontier_positions = 0;
  uint64_t legal_edges = 0;
  uint64_t nonterminal_edges = 0;
  uint64_t terminal_edges = 0;
  uint64_t p1_terminal_edges = 0;
  uint64_t p2_terminal_edges = 0;
  uint64_t draw_terminal_edges = 0;
  uint64_t unique_nonterminal_after = 0;
  bool unique_after_available = false;
  int min_legal_moves = kPits;
  int max_legal_moves = 0;
  std::array<uint64_t, kPits> legal_move_counts{};
  std::array<uint64_t, 4> tuzdyk_state_counts{};
  int max_p1_kazan_after = 0;
  int max_p2_kazan_after = 0;
  int max_mover_gain = 0;
  int min_p1_side_after = kWinThreshold * 2;
  int min_p2_side_after = kWinThreshold * 2;
  int max_p1_side_after = 0;
  int max_p2_side_after = 0;
  double elapsed_seconds = 0.0;
};

State initial_state() {
  State state;
  state.board.fill(9);
  state.kazans = {0, 0};
  state.tuzdyks = {-1, -1};
  return state;
}

int collect_legal_moves(const State& state, std::array<int, kPits>& moves) {
  int count = 0;
  const int blocked = state.tuzdyks[1 - state.to_play];
  const int base = state.to_play * kPits;

  for (int pit = 0; pit < kPits; ++pit) {
    if (pit != blocked && state.board[base + pit] > 0) {
      moves[count++] = pit;
    }
  }

  return count;
}

bool has_legal_move(const State& state) {
  std::array<int, kPits> moves{};
  return collect_legal_moves(state, moves) > 0;
}

void determine_winner(State& state) {
  if (state.kazans[0] > state.kazans[1]) {
    state.winner = 0;
  } else if (state.kazans[1] > state.kazans[0]) {
    state.winner = 1;
  } else {
    state.winner = -1;
  }
}

MoveResult play_move(const State& before, int action) {
  State state = before;
  const int player = state.to_play;
  const int opponent = 1 - player;
  const int start = player * kPits + action;
  const int stones = state.board[start];
  const int kazan_before = state.kazans[player];
  int last = start;
  bool created_tuzdyk = false;

  // Sow first, then collect existing Tuzdyks, then apply the landing-cell
  // capture/Tuzdyk rule. Keeping this order explicit is important because the
  // 11-halfmove terminal line depends on the same rule order as the engine.
  if (stones == 1) {
    state.board[start] = 0;
    last = (start + 1) % kTotalPits;
    state.board[last]++;
  } else {
    state.board[start] = 1;
    const int sow_count = stones - 1;
    const int cycles = sow_count / kTotalPits;
    const int remainder = sow_count % kTotalPits;
    last = (start + sow_count) % kTotalPits;

    if (cycles > 0) {
      for (uint8_t& pit : state.board) {
        pit = static_cast<uint8_t>(pit + cycles);
      }
    }

    for (int index = 0; index < remainder; ++index) {
      state.board[(start + 1 + index) % kTotalPits]++;
    }
  }

  for (int owner = 0; owner < 2; ++owner) {
    const int tuzdyk = state.tuzdyks[owner];
    if (tuzdyk != -1) {
      const int pit = (1 - owner) * kPits + tuzdyk;
      const int captured = state.board[pit];
      if (captured > 0) {
        state.kazans[owner] = static_cast<uint8_t>(state.kazans[owner] + captured);
        state.board[pit] = 0;
      }
    }
  }

  if (last / kPits == opponent) {
    const int column = last % kPits;
    const int value = state.board[last];
    if (value == 3 && state.tuzdyks[player] == -1 && column != 8 &&
        state.tuzdyks[opponent] != column) {
      state.tuzdyks[player] = static_cast<int8_t>(column);
      state.kazans[player] = static_cast<uint8_t>(state.kazans[player] + 3);
      state.board[last] = 0;
      created_tuzdyk = true;
    } else if (value % 2 == 0) {
      state.kazans[player] = static_cast<uint8_t>(state.kazans[player] + value);
      state.board[last] = 0;
    }
  }

  state.steps++;
  state.winner = kOngoing;
  if (state.kazans[player] >= kWinThreshold) {
    state.winner = static_cast<int8_t>(player);
  } else if (state.kazans[opponent] >= kWinThreshold) {
    state.winner = static_cast<int8_t>(opponent);
  }

  state.to_play = static_cast<uint8_t>(opponent);
  if (state.winner == kOngoing && !has_legal_move(state)) {
    for (int pit = 0; pit < kPits; ++pit) {
      state.kazans[0] = static_cast<uint8_t>(state.kazans[0] + state.board[pit]);
      state.kazans[1] = static_cast<uint8_t>(state.kazans[1] + state.board[pit + kPits]);
      state.board[pit] = 0;
      state.board[pit + kPits] = 0;
    }
    determine_winner(state);
  }

  MoveResult result;
  result.state = state;
  result.notation = std::to_string(action + 1) + std::to_string((last % kPits) + 1);
  if (created_tuzdyk) {
    result.notation += "x";
  }
  result.mover_gain = state.kazans[player] - kazan_before;
  return result;
}

int best_immediate_gain_for(const State& state, int player) {
  if (state.winner != kOngoing || state.to_play != player) {
    return 0;
  }

  std::array<int, kPits> moves{};
  const int count = collect_legal_moves(state, moves);
  int best = 0;
  for (int index = 0; index < count; ++index) {
    const State next = play_move(state, moves[index]).state;
    best = std::max(best,
                    static_cast<int>(next.kazans[player]) - static_cast<int>(state.kazans[player]));
  }
  return best;
}

bool state_less(const State& left, const State& right) {
  if (left.board != right.board) {
    return left.board < right.board;
  }
  if (left.kazans != right.kazans) {
    return left.kazans < right.kazans;
  }
  if (left.tuzdyks != right.tuzdyks) {
    return left.tuzdyks < right.tuzdyks;
  }
  return left.to_play < right.to_play;
}

bool state_equal(const State& left, const State& right) {
  return left.board == right.board && left.kazans == right.kazans &&
         left.tuzdyks == right.tuzdyks && left.to_play == right.to_play;
}

int side_stones(const State& state, int player) {
  int total = 0;
  const int base = player * kPits;
  for (int pit = 0; pit < kPits; ++pit) {
    total += state.board[base + pit];
  }
  return total;
}

int tuzdyk_state_index(const State& state) {
  const bool p1_has = state.tuzdyks[0] != -1;
  const bool p2_has = state.tuzdyks[1] != -1;
  if (!p1_has && !p2_has) {
    return 0;
  }
  if (p1_has && !p2_has) {
    return 1;
  }
  if (!p1_has && p2_has) {
    return 2;
  }
  return 3;
}

void add_child_stats(DepthStats& stats, const MoveResult& result) {
  const State& child = result.state;
  if (child.winner == kOngoing) {
    stats.nonterminal_edges++;
  } else {
    stats.terminal_edges++;
    if (child.winner == 0) {
      stats.p1_terminal_edges++;
    } else if (child.winner == 1) {
      stats.p2_terminal_edges++;
    } else {
      stats.draw_terminal_edges++;
    }
  }

  stats.max_p1_kazan_after = std::max(stats.max_p1_kazan_after, static_cast<int>(child.kazans[0]));
  stats.max_p2_kazan_after = std::max(stats.max_p2_kazan_after, static_cast<int>(child.kazans[1]));
  stats.max_mover_gain = std::max(stats.max_mover_gain, result.mover_gain);

  const int p1_side = side_stones(child, 0);
  const int p2_side = side_stones(child, 1);
  stats.min_p1_side_after = std::min(stats.min_p1_side_after, p1_side);
  stats.min_p2_side_after = std::min(stats.min_p2_side_after, p2_side);
  stats.max_p1_side_after = std::max(stats.max_p1_side_after, p1_side);
  stats.max_p2_side_after = std::max(stats.max_p2_side_after, p2_side);
  stats.tuzdyk_state_counts[tuzdyk_state_index(child)]++;
}

void write_depth_stats(const std::vector<DepthStats>& stats, const std::string& path,
                       int terminal_depth) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("failed to open " + path);
  }

  out << "# Depth-by-depth breadth search statistics.\n";
  out << "# frontier_positions are exact unique nonterminal positions before "
         "that halfmove.\n";
  out << "# unique_nonterminal_after is exact when available; at the last "
         "proof layer it is intentionally skipped to avoid storing the huge "
         "next frontier.\n";
  out << "halfmove\tfrontier_positions\tlegal_edges\tavg_legal_moves"
      << "\tmin_legal_moves\tmax_legal_moves\tnonterminal_edges"
      << "\tterminal_edges\tp1_terminal_edges\tp2_terminal_edges"
      << "\tdraw_terminal_edges\tunique_nonterminal_after"
      << "\tmax_p1_kazan_after\tmax_p2_kazan_after\tmax_mover_gain"
      << "\tmin_p1_side_after\tmax_p1_side_after\tmin_p2_side_after"
      << "\tmax_p2_side_after\ttuzdyk_none\ttuzdyk_only_p1"
      << "\ttuzdyk_only_p2\ttuzdyk_both";
  for (int pit = 0; pit < kPits; ++pit) {
    out << "\tlegal_move_" << (pit + 1);
  }
  out << "\telapsed_seconds\n";

  out << std::fixed << std::setprecision(6);
  for (const DepthStats& row : stats) {
    const double avg_legal =
        row.frontier_positions == 0
            ? 0.0
            : static_cast<double>(row.legal_edges) / static_cast<double>(row.frontier_positions);

    out << row.halfmove << "\t" << row.frontier_positions << "\t" << row.legal_edges << "\t"
        << avg_legal << "\t" << (row.frontier_positions == 0 ? 0 : row.min_legal_moves) << "\t"
        << row.max_legal_moves << "\t" << row.nonterminal_edges << "\t" << row.terminal_edges
        << "\t" << row.p1_terminal_edges << "\t" << row.p2_terminal_edges << "\t"
        << row.draw_terminal_edges << "\t";

    if (row.unique_after_available) {
      out << row.unique_nonterminal_after;
    } else {
      out << "not_materialized";
    }

    out << "\t" << row.max_p1_kazan_after << "\t" << row.max_p2_kazan_after << "\t"
        << row.max_mover_gain << "\t" << row.min_p1_side_after << "\t" << row.max_p1_side_after
        << "\t" << row.min_p2_side_after << "\t" << row.max_p2_side_after;

    for (uint64_t count : row.tuzdyk_state_counts) {
      out << "\t" << count;
    }
    for (uint64_t count : row.legal_move_counts) {
      out << "\t" << count;
    }
    out << "\t" << row.elapsed_seconds << "\n";
  }

  out << "# proven_terminal_edges_zero_through_halfmove\t" << (terminal_depth - 1) << "\n";
  out << "# saved_shortest_candidate_halfmove\t" << terminal_depth << "\n";
  out << "# see\tshortest_terminal_game.txt\tshortest_candidate_replay.tsv\n";
}

bool prove_no_terminal_before(int terminal_depth) {
  std::vector<State> frontier{initial_state()};
  std::vector<State> next;
  std::vector<DepthStats> rows;
  const auto started = std::chrono::steady_clock::now();

  // The proof only needs to show that every edge through halfmove 10 is
  // nonterminal. At the last proof layer we count children but intentionally
  // avoid materializing the enormous next frontier.
  for (int depth = 1; depth < terminal_depth; ++depth) {
    next.clear();
    DepthStats row;
    row.halfmove = depth;
    row.frontier_positions = frontier.size();
    const bool need_next_frontier = depth + 1 < terminal_depth;

    if (need_next_frontier) {
      next.reserve(frontier.size() * kPits);
    }

    for (const State& state : frontier) {
      std::array<int, kPits> moves{};
      const int count = collect_legal_moves(state, moves);
      row.legal_edges += static_cast<uint64_t>(count);
      row.min_legal_moves = std::min(row.min_legal_moves, count);
      row.max_legal_moves = std::max(row.max_legal_moves, count);

      for (int index = 0; index < count; ++index) {
        row.legal_move_counts[moves[index]]++;
        MoveResult result = play_move(state, moves[index]);
        add_child_stats(row, result);
        if (result.state.winner == kOngoing && need_next_frontier) {
          next.push_back(result.state);
        }
      }
    }

    std::cerr << "depth " << depth << ": frontier=" << frontier.size()
              << " edges=" << row.legal_edges << " terminals=" << row.terminal_edges;

    if (row.terminal_edges > 0) {
      std::cerr << "\n";
      return false;
    }

    if (need_next_frontier) {
      std::sort(next.begin(), next.end(), state_less);
      next.erase(std::unique(next.begin(), next.end(), state_equal), next.end());
      row.unique_nonterminal_after = next.size();
      row.unique_after_available = true;
      std::cerr << " next=" << next.size();
      frontier.swap(next);
    }

    const auto now = std::chrono::steady_clock::now();
    row.elapsed_seconds = std::chrono::duration<double>(now - started).count();
    rows.push_back(row);
    std::cerr << " elapsed=" << row.elapsed_seconds << "s\n";
  }

  write_depth_stats(rows, "shortest_depth_proof.tsv", terminal_depth);
  return true;
}

std::vector<std::string> beam_find_terminal(int max_depth, int beam_width) {
  struct BeamNode {
    State state;
    std::vector<std::string> line;
    int score = 0;
  };

  std::vector<BeamNode> beam{{initial_state(), {}, 0}};

  for (int depth = 1; depth <= max_depth; ++depth) {
    std::vector<BeamNode> candidates;

    for (const BeamNode& node : beam) {
      if (node.state.winner != kOngoing) {
        continue;
      }

      std::array<int, kPits> moves{};
      const int count = collect_legal_moves(node.state, moves);
      for (int index = 0; index < count; ++index) {
        MoveResult result = play_move(node.state, moves[index]);
        std::vector<std::string> line = node.line;
        line.push_back(result.notation);

        if (result.state.winner != kOngoing) {
          return line;
        }

        const int target = 0;
        const int opponent = 1;
        const int target_gain = result.state.kazans[target] - node.state.kazans[target];
        const int opponent_gain = result.state.kazans[opponent] - node.state.kazans[opponent];
        const int target_next_gain = best_immediate_gain_for(result.state, target);
        const int side0 =
            std::accumulate(result.state.board.begin(), result.state.board.begin() + kPits, 0);
        const int side1 =
            std::accumulate(result.state.board.begin() + kPits, result.state.board.end(), 0);

        BeamNode child;
        child.state = result.state;
        child.line = std::move(line);
        child.score = result.state.kazans[target] * 10000 - result.state.kazans[opponent] * 1000 +
                      target_next_gain * 750 + target_gain * 100 - opponent_gain * 100 -
                      std::abs(side0 - side1);
        candidates.push_back(std::move(child));
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const BeamNode& left, const BeamNode& right) { return left.score > right.score; });

    if (static_cast<int>(candidates.size()) > beam_width) {
      candidates.resize(beam_width);
    }
    beam = std::move(candidates);
  }

  return {};
}

State replay_line(const std::vector<int>& actions, std::vector<std::string>& notation) {
  State state = initial_state();
  notation.clear();

  for (int action : actions) {
    if (state.winner != kOngoing) {
      throw std::runtime_error("line continues after terminal position");
    }

    std::array<int, kPits> moves{};
    const int count = collect_legal_moves(state, moves);
    if (std::find(moves.begin(), moves.begin() + count, action) == moves.begin() + count) {
      throw std::runtime_error("illegal action in candidate line");
    }

    MoveResult result = play_move(state, action);
    state = result.state;
    notation.push_back(result.notation);
  }

  return state;
}

void write_game(const std::vector<std::string>& line, const std::string& path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("failed to open " + path);
  }

  for (size_t index = 0; index < line.size(); index += 2) {
    out << (index / 2 + 1) << ". " << line[index];
    if (index + 1 < line.size()) {
      out << " " << line[index + 1];
    }
    out << "\n";
  }
}

void write_candidate_stats(const std::vector<int>& actions, const std::string& path) {
  std::ofstream out(path);
  if (!out) {
    throw std::runtime_error("failed to open " + path);
  }

  State state = initial_state();
  out << "shortest_candidate_halfmoves\t" << actions.size() << "\n";
  out << "initial_position\tp1_side=81\tp2_side=81\tp1_kazan=0\tp2_kazan=0\n";
  out << "halfmove\tplayer\tmove\tp1_kazan\tp2_kazan\tp1_side\tp2_side"
      << "\tp1_tuzdyk\tp2_tuzdyk\twinner\n";

  for (size_t index = 0; index < actions.size(); ++index) {
    const int player = state.to_play;
    MoveResult result = play_move(state, actions[index]);
    state = result.state;

    out << (index + 1) << "\tP" << (player + 1) << "\t" << result.notation << "\t"
        << static_cast<int>(state.kazans[0]) << "\t" << static_cast<int>(state.kazans[1]) << "\t"
        << side_stones(state, 0) << "\t" << side_stones(state, 1) << "\t"
        << static_cast<int>(state.tuzdyks[0]) << "\t" << static_cast<int>(state.tuzdyks[1]) << "\t";

    if (state.winner == kOngoing) {
      out << "ongoing";
    } else if (state.winner == -1) {
      out << "draw";
    } else {
      out << "P" << (static_cast<int>(state.winner) + 1);
    }
    out << "\n";
  }
}

int main(int argc, char** argv) {
  int max_depth = 11;
  int beam_width = 1000;
  bool prove = true;
  bool verify_only = false;

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--no-proof") {
      prove = false;
    } else if (arg == "--verify-only") {
      verify_only = true;
      prove = false;
    } else if (arg == "--max-depth" && index + 1 < argc) {
      max_depth = std::stoi(argv[++index]);
    } else if (arg == "--beam" && index + 1 < argc) {
      beam_width = std::stoi(argv[++index]);
    }
  }

  const std::vector<int> known_actions = {8, 1, 7, 0, 6, 1, 5, 0, 8, 1, 4};
  std::vector<std::string> notation;
  State final_state = replay_line(known_actions, notation);
  if (final_state.winner == kOngoing) {
    throw std::runtime_error("candidate line is not terminal");
  }

  if (!verify_only) {
    const std::vector<std::string> discovered = beam_find_terminal(max_depth, beam_width);
    if (discovered.empty()) {
      std::cerr << "beam search did not find a terminal line within " << max_depth
                << " halfmoves; verifying saved candidate\n";
    }

    if (prove) {
      if (!prove_no_terminal_before(static_cast<int>(notation.size()))) {
        throw std::runtime_error("found a shorter terminal line");
      }
    }
  }

  write_game(notation, "shortest_terminal_game.txt");
  write_candidate_stats(known_actions, "shortest_candidate_replay.tsv");

  std::cout << "Saved shortest candidate with " << notation.size()
            << " halfmoves to shortest_terminal_game.txt\n";
  std::cout << "Final kazans: " << static_cast<int>(final_state.kazans[0]) << "-"
            << static_cast<int>(final_state.kazans[1]) << "\n";
  std::cout << "Winner: P" << (static_cast<int>(final_state.winner) + 1) << "\n";
}
