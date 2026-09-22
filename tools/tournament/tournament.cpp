#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "togyz/dag_v1_search.hpp"
#include "togyz/evaluation.hpp"
#include "togyz/minimax_engine.hpp"
#include "togyz/togyzkumalak_rules.hpp"

// Fixed-depth round-robin behind the playing-strength table of the paper
// (Table 6, Sec. 9.3), following the protocol stated there:
//
//   * --positions start positions, each made by playing --opening-plies
//     uniformly random legal moves from the initial board (terminal positions
//     are rejected and redrawn);
//   * every start is played twice with colours swapped;
//   * a position occurring three times is scored as a draw (official laws).
//
// Players: "random", "minimax:D" (minimax_engine at fixed depth D) and
// "dagv1:D" (the TT-guided DAGv1 engine at fixed depth D).
//
// Runs single-threaded on purpose: minimax_engine keeps one process-wide
// transposition table, so parallel games would race on it, and a fixed game
// order makes every run with the same --seed identical.
//
// Usage:
//   tg_tournament --paper                     # the four matches of Table 6
//   tg_tournament --a minimax:3 --b minimax:1 [--positions 500]
//                 [--opening-plies 6] [--seed 2026]

namespace {

struct Player {
  std::string kind;  // "random", "minimax", "dagv1"
  int depth = 0;
  std::string label;
};

Player parse_player(const std::string& spec) {
  Player p;
  size_t colon = spec.find(':');
  p.kind = spec.substr(0, colon);
  if (p.kind == "random") {
    p.label = "Random";
    return p;
  }
  if ((p.kind != "minimax" && p.kind != "dagv1") || colon == std::string::npos) {
    throw std::runtime_error("player must be random, minimax:D or dagv1:D, got " + spec);
  }
  p.depth = std::atoi(spec.c_str() + colon + 1);
  if (p.depth < 1) throw std::runtime_error("depth must be >= 1 in " + spec);
  p.label = (p.kind == "minimax" ? "Minimax (D=" : "DAGv1 (D=") + std::to_string(p.depth) + ")";
  return p;
}

// Plays `plies` uniformly random legal moves from the initial board and
// returns the position, redrawing whenever the game ends inside the opening.
ToguzEnv make_start(std::mt19937_64& rng, int plies) {
  while (true) {
    ToguzEnv env;
    bool ok = true;
    for (int i = 0; i < plies; ++i) {
      std::vector<int> moves = env.generate_moves();
      if (moves.empty() || env.is_game_over()) {
        ok = false;
        break;
      }
      std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
      env.step(moves[pick(rng)]);
      if (env.is_game_over()) {
        ok = false;
        break;
      }
    }
    if (ok) {
      env.reset_repetition_history();
      return env;
    }
  }
}

int choose_move(const Player& player, ToguzEnv& env, std::mt19937_64& rng,
                const Evaluator& evaluator) {
  if (player.kind == "random") {
    std::vector<int> moves = env.generate_moves();
    std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
    return moves[pick(rng)];
  }
  if (player.kind == "minimax") return minimax_engine::get_best_move(env, player.depth, evaluator);
  return dag_v1::get_best_move(env, player.depth, evaluator);
}

// Returns +1 if `first` (moving first as P1) wins, -1 if `second` wins, 0 for a draw.
int play_game(ToguzEnv env, const Player& first, const Player& second, std::mt19937_64& rng,
              const Evaluator& evaluator) {
  dag_v1::init_tt(64);  // fresh DAGv1 table per game
  while (!env.is_game_over()) {
    const Player& mover = (env.to_play == 0) ? first : second;
    int move = choose_move(mover, env, rng, evaluator);
    if (move < 0) break;
    env.step(move);
    if (env.record_repetition_and_check_draw()) break;  // threefold repetition -> draw
  }
  if (env.winner_code == 0) return 1;
  if (env.winner_code == 1) return -1;
  return 0;
}

struct MatchResult {
  int a_wins = 0;
  int b_wins = 0;
  int draws = 0;
};

MatchResult play_match(const Player& a, const Player& b, int positions, int opening_plies,
                       uint64_t seed) {
  const HeuristicEvaluator evaluator;
  std::mt19937_64 start_rng(seed);  // same starts for every match with this seed
  MatchResult r;
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < positions; ++i) {
    ToguzEnv start = make_start(start_rng, opening_plies);
    for (int swap = 0; swap < 2; ++swap) {
      std::mt19937_64 game_rng(seed * 1000003ULL + static_cast<uint64_t>(2 * i + swap));
      int outcome = (swap == 0) ? play_game(start, a, b, game_rng, evaluator)
                                : -play_game(start, b, a, game_rng, evaluator);
      if (outcome > 0) {
        r.a_wins++;
      } else if (outcome < 0) {
        r.b_wins++;
      } else {
        r.draws++;
      }
    }
  }
  double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  int games = r.a_wins + r.b_wins + r.draws;
  double score = (r.a_wins + 0.5 * r.draws) / games;
  std::printf("| %s | %s | %.1f%% | %.1f%% | %.1f%% | %.1f%% |", a.label.c_str(), b.label.c_str(),
              100.0 * r.a_wins / games, 100.0 * r.b_wins / games, 100.0 * r.draws / games,
              100.0 * score);
  if (score > 0.0 && score < 1.0) {
    std::printf(" %+.0f |", -400.0 * std::log10(1.0 / score - 1.0));
  } else {
    std::printf(" n/a |");
  }
  std::printf(" %d games, %.0f s\n", games, secs);
  std::fflush(stdout);
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  std::string a_spec;
  std::string b_spec;
  bool paper = false;
  int positions = 500;
  int opening_plies = 6;
  uint64_t seed = 2026;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--paper") {
      paper = true;
    } else if (arg == "--a" && i + 1 < argc) {
      a_spec = argv[++i];
    } else if (arg == "--b" && i + 1 < argc) {
      b_spec = argv[++i];
    } else if (arg == "--positions" && i + 1 < argc) {
      positions = std::max(1, std::atoi(argv[++i]));
    } else if (arg == "--opening-plies" && i + 1 < argc) {
      opening_plies = std::max(0, std::atoi(argv[++i]));
    } else if (arg == "--seed" && i + 1 < argc) {
      seed = std::strtoull(argv[++i], nullptr, 10);
    } else {
      std::printf(
          "Usage: %s --paper | --a SPEC --b SPEC [--positions 500] [--opening-plies 6] "
          "[--seed 2026]\nSPEC: random | minimax:D | dagv1:D\n",
          argv[0]);
      return arg == "--help" ? 0 : 1;
    }
  }
  if (!paper && (a_spec.empty() || b_spec.empty())) {
    std::printf("Give --paper, or both --a and --b. See --help.\n");
    return 1;
  }

  std::printf("# 9Q fixed-depth tournament\n\n");
  std::printf("%d start positions x 2 colour orders, %d random opening plies, seed %llu, ",
              positions, opening_plies, static_cast<unsigned long long>(seed));
  std::printf("threefold repetition = draw.\n\n");
  std::printf("| Player A | Player B | A wins | B wins | Draws | A score | Elo diff | |\n");
  std::printf("|---|---|---:|---:|---:|---:|---:|---|\n");

  try {
    if (paper) {
      play_match(parse_player("minimax:1"), parse_player("random"), positions, opening_plies, seed);
      play_match(parse_player("minimax:3"), parse_player("random"), positions, opening_plies, seed);
      play_match(parse_player("minimax:3"), parse_player("minimax:1"), positions, opening_plies,
                 seed);
      play_match(parse_player("minimax:5"), parse_player("minimax:3"), positions, opening_plies,
                 seed);
    } else {
      play_match(parse_player(a_spec), parse_player(b_spec), positions, opening_plies, seed);
    }
  } catch (const std::exception& e) {
    std::printf("error: %s\n", e.what());
    return 1;
  }
  return 0;
}
