#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "togyz/dag_search.hpp"
#include "togyz/evaluation.hpp"
#include "togyz/minimax_engine.hpp"
#include "togyz/position_hash.hpp"
#include "togyz/togyzkumalak_rules.hpp"

using namespace std;

// Publishable CLI for the Togyzkumalak research engine. The `aiN` bot is the
// hand-written minimax baseline; `dagN` is kept for comparison experiments.
#ifndef AI_DEPTH
#define AI_DEPTH 4
#endif

struct Args {
  string mode = "human-ai3";
  string position = "default";
  int numgames = 1;
  int depth = 4;
  int positions = 100;
  string benchmark_bot = "both";
  int threads = 1;
  int max_steps = 10000;
  double move_time_seconds = 0.01;
  int max_search_depth = 64;
  uint64_t seed = 1337;
  int max_reduction_per_pit = 3;
  int max_total_reduction = 18;
  string dataset_file = "";
  string p1_evaluator = "heuristic";
  string p2_evaluator = "heuristic";
  bool noterminal = false;
  bool switch_color = false;
  bool uci = false;
  bool compare = false;
  bool benchmark = false;
};

Args parse_args(int argc, char* argv[]) {
  Args args;
  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];
    if (arg == "--mode" && i + 1 < argc) {
      args.mode = argv[++i];
    } else if (arg == "--position" && i + 1 < argc) {
      args.position = argv[++i];
    } else if (arg == "--numgames" && i + 1 < argc) {
      args.numgames = stoi(argv[++i]);
    } else if (arg == "--depth" && i + 1 < argc) {
      args.depth = stoi(argv[++i]);
    } else if ((arg == "--positions" || arg == "--numpositions") && i + 1 < argc) {
      args.positions = stoi(argv[++i]);
    } else if ((arg == "--bot" || arg == "--benchmark-bot") && i + 1 < argc) {
      args.benchmark_bot = argv[++i];
    } else if (arg == "--threads" && i + 1 < argc) {
      args.threads = stoi(argv[++i]);
    } else if (arg == "--seed" && i + 1 < argc) {
      args.seed = stoull(argv[++i]);
    } else if (arg == "--max-reduction-per-pit" && i + 1 < argc) {
      args.max_reduction_per_pit = stoi(argv[++i]);
    } else if (arg == "--max-total-reduction" && i + 1 < argc) {
      args.max_total_reduction = stoi(argv[++i]);
    } else if (arg == "--max-steps" && i + 1 < argc) {
      args.max_steps = stoi(argv[++i]);
    } else if ((arg == "--move-time" || arg == "--time-per-move" || arg == "--seconds-per-move") &&
               i + 1 < argc) {
      args.move_time_seconds = stod(argv[++i]);
    } else if (arg == "--movetime-ms" && i + 1 < argc) {
      args.move_time_seconds = stod(argv[++i]) / 1000.0;
    } else if (arg == "--max-search-depth" && i + 1 < argc) {
      args.max_search_depth = stoi(argv[++i]);
    } else if ((arg == "--dataset" || arg == "--dataset-file" || arg == "--training-data") &&
               i + 1 < argc) {
      args.dataset_file = argv[++i];
    } else if (arg == "--p1" && i + 1 < argc) {
      args.p1_evaluator = argv[++i];
    } else if (arg == "--p2" && i + 1 < argc) {
      args.p2_evaluator = argv[++i];
    } else if (arg == "--noterminal") {
      args.noterminal = true;
    } else if (arg == "--switch-color" || arg == "--switch") {
      args.switch_color = true;
    } else if (arg == "--uci") {
      args.uci = true;
    } else if (arg == "--compare") {
      args.compare = true;
    } else if (arg == "--benchmark" || arg == "--bench" || arg == "--time-bots") {
      args.benchmark = true;
    }
  }
  return args;
}

using EvaluatorMap = array<shared_ptr<Evaluator>, 2>;

shared_ptr<Evaluator> create_evaluator(const string& spec) {
  if (spec == "heuristic") {
    return make_shared<HeuristicEvaluator>();
  }

  throw runtime_error("Unsupported evaluator '" + spec + "'. Use heuristic.");
}

EvaluatorMap build_evaluators(const Args& args) {
  return {create_evaluator(args.p1_evaluator), create_evaluator(args.p2_evaluator)};
}

constexpr int TRAINING_STATE_SIZE = 23;

struct TrainingExample {
  array<int, TRAINING_STATE_SIZE> state;
};

array<int, TRAINING_STATE_SIZE> encode_training_state(const ToguzEnv& env) {
  array<int, TRAINING_STATE_SIZE> state{};
  for (int pit = 0; pit < NUM_PITS * 2; ++pit) {
    state[pit] = env.board.get(pit);
  }
  state[18] = env.kazans[PLAYER_1];
  state[19] = env.kazans[PLAYER_2];
  state[20] = env.tuzduks[PLAYER_1];
  state[21] = env.tuzduks[PLAYER_2];
  state[22] = env.to_play;
  return state;
}

TrainingExample capture_training_example(const ToguzEnv& env) {
  return TrainingExample{encode_training_state(env)};
}

double training_label_for_result(const ToguzEnv& env) {
  if (env.winner_code == PLAYER_1) return 1.0;
  if (env.winner_code == PLAYER_2) return 0.0;
  return 0.5;
}

class DatasetWriter {
 public:
  explicit DatasetWriter(string output_path) : path(std::move(output_path)) {}

  bool enabled() const { return !path.empty(); }

  void append_game(const vector<TrainingExample>& examples, double label) {
    if (!enabled() || examples.empty()) return;

    lock_guard<mutex> lock(write_mutex);
    ofstream out(path, ios::app);
    if (!out) {
      throw runtime_error("Could not open dataset file for append: " + path);
    }

    for (const auto& example : examples) {
      out << "{\"state\":[";
      for (size_t i = 0; i < example.state.size(); ++i) {
        if (i > 0) out << ",";
        out << example.state[i];
      }
      out << "],\"label\":" << fixed << setprecision(1) << label << "}\n";
    }
  }

  const string& output_path() const { return path; }

 private:
  string path;
  mutex write_mutex;
};

int human_move(ToguzEnv& env, bool quiet) {
  auto legal_moves = env.generate_moves();
  if (!quiet) {
    cout << "Legal moves: ";
    for (size_t i = 0; i < legal_moves.size(); ++i) {
      cout << (legal_moves[i] + 1) << (i == legal_moves.size() - 1 ? "" : ", ");
    }
    cout << "\n";
  }

  while (true) {
    cout << "Choose pit (1-9), or type q to quit: ";
    string raw;
    if (!(cin >> raw)) return -1;

    if (raw == "q" || raw == "quit" || raw == "exit") return -1;

    bool is_num = true;
    for (char c : raw)
      if (!isdigit(c)) is_num = false;

    if (!is_num) {
      cout << "Please enter a number from 1 to 9.\n";
      continue;
    }

    int move = stoi(raw) - 1;
    if (find(legal_moves.begin(), legal_moves.end(), move) == legal_moves.end()) {
      cout << "Illegal move for this position. Try again.\n";
      continue;
    }
    return move;
  }
}

struct TimedBotResult {
  int move = -1;
  int completed_depth = 0;
  double eval = 0.0;
  uint64_t nodes = 0;
};

TimedBotResult timed_bot_move(ToguzEnv& env, const string& kind, double move_time_seconds,
                              int max_depth, const Evaluator& evaluator) {
  TimedBotResult result;
  if (kind == "randombot") {
    result.move = env.get_random_move();
    return result;
  } else if (kind == "ai") {
    auto search = minimax_engine::get_best_move_timed(env, move_time_seconds, max_depth, evaluator);
    result.move = search.move;
    result.completed_depth = search.completed_depth;
    result.eval = search.eval;
    return result;
  } else if (kind == "dag") {
    auto search = dag_search::get_best_move_timed(env, move_time_seconds, max_depth, evaluator);
    result.move = search.move;
    result.completed_depth = search.completed_depth;
    result.eval = search.eval;
    result.nodes = dag_search::get_last_stats().nodes;
    return result;
  } else {
    throw runtime_error("Unsupported bot kind: " + kind);
  }
}

int bot_move(ToguzEnv& env, double move_time_seconds, int max_depth, const string& kind, bool quiet,
             const Evaluator& evaluator) {
  auto start = chrono::high_resolution_clock::now();
  TimedBotResult result = timed_bot_move(env, kind, move_time_seconds, max_depth, evaluator);
  auto end = chrono::high_resolution_clock::now();
  chrono::duration<double> elapsed = end - start;

  if (result.move == -1) {
    throw runtime_error(kind + " has no legal move.");
  }

  if (!quiet) {
    if (kind == "randombot") {
      cout << "RandomBot chooses pit " << (result.move + 1) << " (" << fixed << setprecision(3)
           << elapsed.count() << "s)\n";
    } else {
      cout << (kind == "dag" ? "DAG search" : "Minimax") << " chooses pit " << (result.move + 1)
           << " (time=" << fixed << setprecision(4) << move_time_seconds
           << "s, completed depth=" << result.completed_depth << ", elapsed=" << setprecision(4)
           << elapsed.count() << "s)\n";
    }
  }
  return result.move;
}

struct PlayerConfig {
  string kind;
  int depth = -1;
};

PlayerConfig parse_controller(const string& str) {
  PlayerConfig pc;
  if (str.length() >= 2 && str.substr(0, 2) == "ai") {
    pc.kind = "ai";
    if (str.length() > 2) {
      pc.depth = stoi(str.substr(2));
    } else {
      pc.depth = 3;  // default depth
    }
  } else if (str.length() >= 5 && str.substr(0, 5) == "dagv2") {
    pc.kind = "dagv2";
    if (str.length() > 5) {
      pc.depth = stoi(str.substr(5));
    } else {
      pc.depth = 3;
    }
  } else if (str.length() >= 5 && str.substr(0, 5) == "dagv1") {
    pc.kind = "dag";
    if (str.length() > 5) {
      pc.depth = stoi(str.substr(5));
    } else {
      pc.depth = 3;
    }
  } else if (str.length() >= 3 && str.substr(0, 3) == "dag") {
    pc.kind = "dag";
    if (str.length() > 3) {
      pc.depth = stoi(str.substr(3));
    } else {
      pc.depth = 3;
    }
  } else if (str.length() >= 6 && str.substr(0, 6) == "filter") {
    pc.kind = "dag";
    if (str.length() > 6) {
      pc.depth = stoi(str.substr(6));
    } else {
      pc.depth = 3;
    }
  } else {
    pc.kind = str;
    pc.depth = -1;
  }
  return pc;
}

map<int, PlayerConfig> build_controllers(const string& mode, bool p1_is_black) {
  size_t dash = mode.find('-');
  string p1_kind_str = mode.substr(0, dash);
  string p2_kind_str = mode.substr(dash + 1);

  PlayerConfig p1_kind = parse_controller(p1_kind_str);
  PlayerConfig p2_kind = parse_controller(p2_kind_str);

  map<int, PlayerConfig> controllers;
  if (p1_is_black) {
    controllers[0] = p1_kind;  // PLAYER_1 is 0
    controllers[1] = p2_kind;  // PLAYER_2 is 1
  } else {
    controllers[0] = p2_kind;
    controllers[1] = p1_kind;
  }
  return controllers;
}

int prepare_start_position(ToguzEnv& env, const string& position) {
  if (position == "random") {
    return env.setup_random_position();
  }
  if (position == "balanced" || position == "reduced") {
    static thread_local mt19937_64 rng(random_device{}());
    return env.setup_balanced_reduced_position(rng);
  }
  env.reset();
  return 0;
}

bool play_single_terminal_game(ToguzEnv& env, const map<int, PlayerConfig>& controllers,
                               const EvaluatorMap& evaluators, double move_time_seconds,
                               int max_search_depth, bool quiet,
                               DatasetWriter* dataset_writer = nullptr) {
  auto get_name = [](const PlayerConfig& pc) {
    if (pc.kind == "ai") return string("minimax-ai-timed");
    if (pc.kind == "dag") return string("dag-ai-timed");
    return pc.kind;
  };
  string p1_name = get_name(controllers.at(PLAYER_1));
  string p2_name = get_name(controllers.at(PLAYER_2));

  vector<TrainingExample> training_examples;
  if (dataset_writer != nullptr && dataset_writer->enabled()) {
    training_examples.reserve(static_cast<size_t>(min(env.max_steps, 10000)) + 1);
    training_examples.push_back(capture_training_example(env));
  }

  while (!env.is_game_over()) {
    if (!quiet) {
      env.render("terminal", p1_name, p2_name);
    }
    int player_id = env.to_play;
    string current_name = (player_id == PLAYER_1) ? p1_name : p2_name;
    PlayerConfig pc = controllers.at(player_id);
    string kind = pc.kind;

    if (!quiet) {
      cout << current_name << " to move.\n";
    }

    int move = -1;
    if (kind == "human") {
      move = human_move(env, quiet);
      if (move == -1) {
        if (!quiet) cout << "Game interrupted by user.\n";
        return false;
      }
    } else {
      move =
          bot_move(env, move_time_seconds, max_search_depth, kind, quiet, *evaluators[player_id]);
    }

    env.step(move);
    if (dataset_writer != nullptr && dataset_writer->enabled()) {
      training_examples.push_back(capture_training_example(env));
    }
    if (!quiet) cout << "\n";
  }

  if (!quiet) {
    env.render("terminal", p1_name, p2_name);
    cout << "Final result: " << env.get_result_string() << "\n";
    cout << "Kazans: " << p1_name << "=" << env.kazans[0] << ", " << p2_name << "=" << env.kazans[1]
         << "\n";
  }
  if (dataset_writer != nullptr && dataset_writer->enabled()) {
    dataset_writer->append_game(training_examples, training_label_for_result(env));
  }
  return true;
}

string logical_winner(const ToguzEnv& env, bool p1_is_black) {
  if (env.winner_code == PLAYER_1) {
    return p1_is_black ? "P1" : "P2";
  }
  if (env.winner_code == PLAYER_2) {
    return p1_is_black ? "P2" : "P1";
  }
  return "DRAW";
}

struct Stat {
  int games = 0;
  int p1_win = 0;
  int p2_win = 0;
  int draw = 0;
};

void update_stats(map<string, Stat>& stats, bool p1_is_black, const string& winner) {
  stats["total"].games++;
  if (winner == "P1")
    stats["total"].p1_win++;
  else if (winner == "P2")
    stats["total"].p2_win++;
  else
    stats["total"].draw++;

  string key = p1_is_black ? "p1_black" : "p1_white";
  stats[key].games++;
  if (winner == "P1")
    stats[key].p1_win++;
  else if (winner == "P2")
    stats[key].p2_win++;
  else
    stats[key].draw++;
}

double safe_pct(int n, int d) {
  if (d == 0) return 0.0;
  return 100.0 * (double)n / (double)d;
}

void print_summary(const map<string, Stat>& stats) {
  auto b = stats.at("p1_white");
  int d = b.games;
  double p1_white_win = safe_pct(b.p1_win, d);
  double p1_white_draw = safe_pct(b.draw, d);
  double p1_white_lose = safe_pct(b.p2_win, d);

  auto c = stats.at("p1_black");
  int e = c.games;
  double p2_white_win = safe_pct(c.p2_win, e);
  double p2_white_draw = safe_pct(c.draw, e);
  double p2_white_lose = safe_pct(c.p1_win, e);

  auto t = stats.at("total");
  int g = t.games;
  double total_win = safe_pct(t.p1_win, g);
  double total_draw = safe_pct(t.draw, g);
  double total_lose = safe_pct(t.p2_win, g);

  cout << "\n=== Statistics ===\n";
  cout << "P1 white vs P2 black\n";
  cout << "win% " << fixed << setprecision(1) << p1_white_win << "  draw% " << p1_white_draw
       << "  lose% " << p1_white_lose << "\n";
  cout << "P2 white vs P1 black\n";
  cout << "win% " << p2_white_win << "  draw% " << p2_white_draw << "  lose% " << p2_white_lose
       << "\n";
  cout << "Total\n";
  cout << "P1 vs P2\n";
  cout << "win% " << total_win << "  draw% " << total_draw << "  lose% " << total_lose << "\n";
}

struct CompareStats {
  int positions = 0;
  int games = 0;
  int minimax_wins = 0;
  int dag_wins = 0;
  int draws = 0;
  int first_move_agreements = 0;
  int first_move_disagreements = 0;
  long long minimax_margin_sum = 0;
  long long dag_margin_sum = 0;
  long long total_reduction_sum = 0;
};

int fixed_depth_move(const ToguzEnv& env, const string& kind, int depth,
                     const Evaluator& evaluator) {
  if (kind == "ai") return minimax_engine::get_best_move(env, depth, evaluator);
  if (kind == "dag") return dag_search::get_best_move(env, depth, evaluator);
  throw runtime_error("Unsupported comparison bot: " + kind);
}

string play_comparison_game(const ToguzEnv& start, const string& p1_kind, const string& p2_kind,
                            int depth, const EvaluatorMap& evaluators, CompareStats& stats) {
  ToguzEnv env = start;
  array<string, 2> kinds = {p1_kind, p2_kind};

  while (!env.is_game_over()) {
    string kind = kinds[env.to_play];
    int move = fixed_depth_move(env, kind, depth, *evaluators[env.to_play]);
    if (move == -1) {
      break;
    }
    env.step(move);
  }

  stats.games++;
  if (env.winner_code == -1) {
    stats.draws++;
    return "draw";
  }

  string winner_kind = kinds[env.winner_code];
  int loser = 1 - env.winner_code;
  int margin = env.kazans[env.winner_code] - env.kazans[loser];
  if (winner_kind == "dag") {
    stats.dag_wins++;
    stats.dag_margin_sum += margin;
    stats.minimax_margin_sum -= margin;
    return "dag";
  }

  stats.minimax_wins++;
  stats.minimax_margin_sum += margin;
  stats.dag_margin_sum -= margin;
  return "minimax";
}

void run_fixed_depth_comparison(const Args& args) {
  if (args.depth < 1) {
    throw runtime_error("--depth must be >= 1 for comparison.");
  }
  if (args.positions < 1) {
    throw runtime_error("--positions must be >= 1 for comparison.");
  }
  if (args.max_steps < 1) {
    throw runtime_error("--max-steps must be >= 1.");
  }

  mt19937_64 rng(args.seed);
  CompareStats stats;
  EvaluatorMap evaluators = build_evaluators(args);

  cout << "=== Fixed-depth bot comparison ===\n";
  cout << "Minimax bot: ai" << args.depth << "  DAG bot: dag" << args.depth << "\n";
  cout << "Positions: " << args.positions << " balanced reduced starts, seed=" << args.seed << "\n";
  cout << "Evaluators: P1=" << evaluators[PLAYER_1]->description()
       << "  P2=" << evaluators[PLAYER_2]->description() << "\n";
  cout << "Randomization: each side subtracts 1.." << args.max_reduction_per_pit
       << " per selected pit with equal total removed per side.\n";

  for (int i = 0; i < args.positions; ++i) {
    ToguzEnv start;
    start.max_steps = args.max_steps;
    int reduction = start.setup_balanced_reduced_position(rng, args.max_reduction_per_pit,
                                                          args.max_total_reduction);
    stats.positions++;
    stats.total_reduction_sum += reduction;

    int minimax_first =
        minimax_engine::get_best_move(start, args.depth, *evaluators[start.to_play]);
    int dag_first = dag_search::get_best_move(start, args.depth, *evaluators[start.to_play]);
    if (minimax_first == dag_first)
      stats.first_move_agreements++;
    else
      stats.first_move_disagreements++;

    play_comparison_game(start, "ai", "dag", args.depth, evaluators, stats);
    play_comparison_game(start, "dag", "ai", args.depth, evaluators, stats);

    if (!args.noterminal && ((i + 1) % 10 == 0 || i + 1 == args.positions)) {
      cout << "Processed " << (i + 1) << "/" << args.positions << " positions\r" << flush;
    }
  }

  if (!args.noterminal) cout << "\n";

  cout << "\n=== Comparison Results ===\n";
  cout << "Games: " << stats.games << " (" << stats.positions << " positions x 2 color orders)\n";
  cout << "DAG wins: " << stats.dag_wins << "  minimax wins: " << stats.minimax_wins
       << "  draws: " << stats.draws << "\n";
  cout << fixed << setprecision(1);
  cout << "DAG win%: " << safe_pct(stats.dag_wins, stats.games)
       << "  minimax win%: " << safe_pct(stats.minimax_wins, stats.games)
       << "  draw%: " << safe_pct(stats.draws, stats.games) << "\n";
  cout << "First-move agreement: " << stats.first_move_agreements << "/" << stats.positions << " ("
       << safe_pct(stats.first_move_agreements, stats.positions) << "%)\n";
  cout << "Average equal reduction per side: "
       << (double)stats.total_reduction_sum / (double)stats.positions << "\n";
  cout << "Average DAG margin per game: " << (double)stats.dag_margin_sum / (double)stats.games
       << "\n";
}

struct StartPosition {
  Bitboard board;
  array<int, 2> kazans;
  array<int, 2> tuzduks;
  int to_play = PLAYER_1;
  int steps = 0;
  int winner_code = -2;
  int max_steps = 10000;
  int reduction = 0;
  vector<uint64_t> history_stack;
};

StartPosition capture_start_position(const ToguzEnv& env, int reduction) {
  StartPosition start;
  start.board = env.board;
  start.kazans = env.kazans;
  start.tuzduks = env.tuzduks;
  start.to_play = env.to_play;
  start.steps = env.steps;
  start.winner_code = env.winner_code;
  start.max_steps = env.max_steps;
  start.reduction = reduction;
  start.history_stack = env.history_stack;
  return start;
}

void apply_start_position(ToguzEnv& env, const StartPosition& start) {
  env.board = start.board;
  env.kazans = start.kazans;
  env.tuzduks = start.tuzduks;
  env.to_play = start.to_play;
  env.steps = start.steps;
  env.winner_code = start.winner_code;
  env.max_steps = start.max_steps;
  env.history_stack = start.history_stack;
}

vector<StartPosition> make_balanced_start_positions(const Args& args) {
  mt19937_64 rng(args.seed);
  ToguzEnv env;
  env.max_steps = args.max_steps;
  vector<StartPosition> starts;
  starts.reserve(args.positions);
  for (int i = 0; i < args.positions; ++i) {
    int reduction = env.setup_balanced_reduced_position(rng, args.max_reduction_per_pit,
                                                        args.max_total_reduction);
    starts.push_back(capture_start_position(env, reduction));
  }
  return starts;
}

string board_snapshot(const ToguzEnv& env, int ply, int mover, int move) {
  stringstream out;
  out << "ply " << ply;
  if (move >= 0) {
    out << " mover=P" << (mover + 1) << " move=" << (move + 1);
  } else {
    out << " initial";
  }
  out << " next=P" << (env.to_play + 1) << " steps=" << env.steps << " winner=" << env.winner_code
      << " hash=" << env.position_hash() << "\n";
  out << "kazans P1=" << env.kazans[0] << " P2=" << env.kazans[1]
      << " tuzduks P1=" << (env.tuzduks[0] + 1) << " P2=" << (env.tuzduks[1] + 1) << "\n";

  out << "P2:";
  for (int i = 8; i >= 0; --i) {
    out << " " << env.board.get(i + NUM_PITS);
  }
  out << "\nP1:";
  for (int i = 0; i < NUM_PITS; ++i) {
    out << " " << env.board.get(i);
  }
  out << "\n\n";
  return out.str();
}

struct LimitTraceContext {
  atomic<bool> stop_requested{false};
  mutex write_mutex;
  string path = "limit.txt";
};

void initialize_limit_file(const string& path) {
  ofstream out(path, ios::trunc);
  out << "0\n";
}

void write_limit_trace_once(LimitTraceContext* ctx, const string& kind, int depth,
                            size_t position_idx, const StartPosition& start,
                            const vector<string>& history, const ToguzEnv& env) {
  if (ctx == nullptr) return;

  bool expected = false;
  if (!ctx->stop_requested.compare_exchange_strong(expected, true)) {
    return;
  }

  lock_guard<mutex> lock(ctx->write_mutex);
  ofstream out(ctx->path, ios::trunc);
  out << "1\n";
  out << "limit_exceeded=1\n";
  out << "reason=max_steps_reached\n";
  out << "bot=" << kind << depth << "\n";
  out << "position_index=" << position_idx << "\n";
  out << "start_equal_reduction_per_side=" << start.reduction << "\n";
  out << "max_steps=" << env.max_steps << "\n";
  out << "final_steps=" << env.steps << "\n";
  out << "final_winner=" << env.winner_code << "\n";
  out << "final_kazans P1=" << env.kazans[0] << " P2=" << env.kazans[1] << "\n";
  out << "\n";
  for (const string& snapshot : history) {
    out << snapshot;
  }
}

struct TimingStats {
  string bot;
  int depth = 0;
  double move_time_seconds = 0.0;
  int threads = 1;
  int games = 0;
  int p1_wins = 0;
  int p2_wins = 0;
  int draws = 0;
  int max_step_games = 0;
  int repetition_draws = 0;
  long long moves = 0;
  long long total_reduction = 0;
  unsigned long long nodes = 0;
  int min_moves = 0;
  int max_moves = 0;
  double seconds = 0.0;
};

TimingStats run_selfplay_timing_range(const vector<StartPosition>& starts, const string& kind,
                                      int depth, double move_time_seconds, size_t begin_idx,
                                      size_t end_idx, LimitTraceContext* limit_ctx,
                                      DatasetWriter* dataset_writer,
                                      const EvaluatorMap& evaluators) {
  TimingStats stats;
  stats.bot = kind;
  stats.depth = depth;
  stats.move_time_seconds = move_time_seconds;

  ToguzEnv env;
  for (size_t idx = begin_idx; idx < end_idx; ++idx) {
    if (limit_ctx != nullptr && limit_ctx->stop_requested.load()) break;

    const auto& start = starts[idx];
    apply_start_position(env, start);
    int game_moves = 0;
    vector<string> history;
    history.reserve(static_cast<size_t>(min(env.max_steps, 10000)) + 1);
    history.push_back(board_snapshot(env, 0, -1, -1));
    vector<TrainingExample> training_examples;
    if (dataset_writer != nullptr && dataset_writer->enabled()) {
      training_examples.reserve(static_cast<size_t>(min(env.max_steps, 10000)) + 1);
      training_examples.push_back(capture_training_example(env));
    }

    while (!env.is_game_over()) {
      if (limit_ctx != nullptr && limit_ctx->stop_requested.load()) break;

      int mover = env.to_play;
      TimedBotResult timed =
          timed_bot_move(env, kind, move_time_seconds, depth, *evaluators[env.to_play]);
      stats.nodes += timed.nodes;
      int move = timed.move;
      if (move == -1) break;
      env.step(move);
      game_moves++;
      history.push_back(board_snapshot(env, game_moves, mover, move));
      if (dataset_writer != nullptr && dataset_writer->enabled()) {
        training_examples.push_back(capture_training_example(env));
      }
    }

    if (limit_ctx != nullptr && limit_ctx->stop_requested.load() && !env.is_game_over()) {
      break;
    }

    stats.games++;
    stats.moves += game_moves;
    stats.total_reduction += start.reduction;
    if (env.steps >= env.max_steps) {
      stats.max_step_games++;
      write_limit_trace_once(limit_ctx, kind, depth, idx, start, history, env);
    }
    if (stats.games == 1 || game_moves < stats.min_moves) stats.min_moves = game_moves;
    if (game_moves > stats.max_moves) stats.max_moves = game_moves;

    if (env.winner_code == PLAYER_1)
      stats.p1_wins++;
    else if (env.winner_code == PLAYER_2)
      stats.p2_wins++;
    else {
      stats.draws++;
      if (env.current_repetition_count() >= 3) stats.repetition_draws++;
    }

    if (dataset_writer != nullptr && dataset_writer->enabled()) {
      dataset_writer->append_game(training_examples, training_label_for_result(env));
    }
  }
  return stats;
}

void merge_timing_stats(TimingStats& total, const TimingStats& part) {
  if (total.move_time_seconds == 0.0) total.move_time_seconds = part.move_time_seconds;
  total.games += part.games;
  total.p1_wins += part.p1_wins;
  total.p2_wins += part.p2_wins;
  total.draws += part.draws;
  total.max_step_games += part.max_step_games;
  total.repetition_draws += part.repetition_draws;
  total.moves += part.moves;
  total.total_reduction += part.total_reduction;
  if (part.games > 0 && (total.min_moves == 0 || part.min_moves < total.min_moves))
    total.min_moves = part.min_moves;
  if (part.max_moves > total.max_moves) total.max_moves = part.max_moves;
}

TimingStats run_selfplay_timing(const vector<StartPosition>& starts, const string& kind, int depth,
                                int threads, double move_time_seconds,
                                DatasetWriter* dataset_writer, const EvaluatorMap& evaluators) {
  TimingStats stats;
  stats.bot = kind;
  stats.depth = depth;
  stats.move_time_seconds = move_time_seconds;
  stats.threads = threads;

  auto end = chrono::high_resolution_clock::now();
  auto begin = end;

  if (threads <= 1 || starts.size() <= 1) {
    begin = chrono::high_resolution_clock::now();
    LimitTraceContext limit_ctx;
    initialize_limit_file(limit_ctx.path);
    stats = run_selfplay_timing_range(starts, kind, depth, move_time_seconds, 0, starts.size(),
                                      &limit_ctx, dataset_writer, evaluators);
    end = chrono::high_resolution_clock::now();
    stats.threads = 1;
    stats.seconds = chrono::duration<double>(end - begin).count();
    return stats;
  }

  threads = min<int>(threads, static_cast<int>(starts.size()));
  stats.threads = threads;
  vector<future<TimingStats>> futures;
  futures.reserve(static_cast<size_t>(threads));
  LimitTraceContext limit_ctx;
  initialize_limit_file(limit_ctx.path);

  begin = chrono::high_resolution_clock::now();
  size_t chunk = (starts.size() + static_cast<size_t>(threads) - 1) / static_cast<size_t>(threads);
  for (int i = 0; i < threads; ++i) {
    size_t start = static_cast<size_t>(i) * chunk;
    size_t finish = min(starts.size(), start + chunk);
    if (start >= finish) break;
    futures.push_back(async(launch::async, [&starts, kind, depth, move_time_seconds, start, finish,
                                            &limit_ctx, dataset_writer, &evaluators]() {
      return run_selfplay_timing_range(starts, kind, depth, move_time_seconds, start, finish,
                                       &limit_ctx, dataset_writer, evaluators);
    }));
  }

  for (auto& future : futures) {
    merge_timing_stats(stats, future.get());
  }
  end = chrono::high_resolution_clock::now();
  stats.seconds = chrono::duration<double>(end - begin).count();
  return stats;
}

void print_timing_stats(const TimingStats& stats) {
  double sec_per_game = stats.games == 0 ? 0.0 : stats.seconds / stats.games;
  double ms_per_move = stats.moves == 0 ? 0.0 : 1000.0 * stats.seconds / (double)stats.moves;
  double avg_moves = stats.games == 0 ? 0.0 : (double)stats.moves / (double)stats.games;
  double avg_reduction = stats.games == 0 ? 0.0 : (double)stats.total_reduction / stats.games;

  cout << "\n"
       << (stats.bot == "dag" ? "dag" : "minimax") << " timed self-play"
       << "\n";
  cout << "time/half-move: " << fixed << setprecision(4) << stats.move_time_seconds
       << "s  max depth: " << stats.depth << "\n";
  cout << "games: " << stats.games << "  moves: " << stats.moves << "  threads: " << stats.threads
       << "  wall time: " << fixed << setprecision(3) << stats.seconds << "s\n";
  cout << "sec/game: " << fixed << setprecision(4) << sec_per_game << "  ms/move: " << fixed
       << setprecision(3) << ms_per_move << "  avg moves/game: " << fixed << setprecision(1)
       << avg_moves << "\n";
  double nps = stats.seconds > 0.0 ? (double)stats.nodes / stats.seconds : 0.0;
  cout << "nodes: " << stats.nodes << "  nps: " << fixed << setprecision(0) << nps << "\n";
  cout << "P1 wins: " << stats.p1_wins << "  P2 wins: " << stats.p2_wins
       << "  draws: " << stats.draws << "  repetition draws: " << stats.repetition_draws
       << "  max-step games: " << stats.max_step_games << "\n";
  cout << "move range: " << stats.min_moves << ".." << stats.max_moves
       << "  avg equal reduction per side: " << fixed << setprecision(1) << avg_reduction << "\n";
}

void run_selfplay_benchmark(const Args& args) {
  if (args.positions < 1) {
    throw runtime_error("--positions must be >= 1 for timing.");
  }
  if (args.max_steps < 1) {
    throw runtime_error("--max-steps must be >= 1.");
  }

  vector<string> bots;
  if (args.benchmark_bot == "both") {
    bots = {"dag", "ai"};
  } else if (args.benchmark_bot == "dag" || args.benchmark_bot == "filter" ||
             args.benchmark_bot == "new") {
    bots = {"dag"};
  } else if (args.benchmark_bot == "ai" || args.benchmark_bot == "minimax") {
    bots = {"ai"};
  } else {
    throw runtime_error("--bot must be one of: both, dag, ai, minimax.");
  }

  int threads = args.threads;
  if (threads <= 0) {
    threads = static_cast<int>(thread::hardware_concurrency());
    if (threads <= 0) threads = 1;
  }
  threads = max(1, threads);

  cout << "=== Self-play timing benchmark ===\n";
  cout << "Time/half-move: " << fixed << setprecision(4) << args.move_time_seconds
       << "s  Max search depth: " << args.max_search_depth << "\n";
  cout << "Positions/Games: " << args.positions << " balanced reduced starts, seed=" << args.seed
       << "\n";
  cout << "Max steps per game: " << args.max_steps << "\n";
  cout << "Threads: " << threads << " (use --threads 1 for single-core timing, "
       << "--threads 0 for auto)\n";
  EvaluatorMap evaluators = build_evaluators(args);
  cout << "Evaluators: P1=" << evaluators[PLAYER_1]->description()
       << "  P2=" << evaluators[PLAYER_2]->description() << "\n";
  if (!args.dataset_file.empty()) {
    cout << "Dataset output: " << args.dataset_file << " (JSONL append)\n";
  }
  cout << "Randomization: each side subtracts 1.." << args.max_reduction_per_pit
       << " per selected pit with equal total removed per side.\n";

  DatasetWriter dataset_writer(args.dataset_file);
  DatasetWriter* dataset_writer_ptr = dataset_writer.enabled() ? &dataset_writer : nullptr;

  auto starts = make_balanced_start_positions(args);
  for (const string& bot : bots) {
    TimingStats stats = run_selfplay_timing(starts, bot, args.max_search_depth, threads,
                                            args.move_time_seconds, dataset_writer_ptr, evaluators);
    print_timing_stats(stats);
    if (stats.max_step_games > 0) {
      cout << "Limit trace written to limit.txt. Stopping benchmark early.\n";
      break;
    }
  }
}

struct WorkerResult {
  bool p1_is_black;
  string winner;
};

WorkerResult play_game_worker(string mode, string position, bool p1_is_black,
                              DatasetWriter* dataset_writer, EvaluatorMap evaluators,
                              double move_time_seconds, int max_search_depth) {
  ToguzEnv env;
  auto controllers = build_controllers(mode, p1_is_black);
  prepare_start_position(env, position);

  vector<TrainingExample> training_examples;
  if (dataset_writer != nullptr && dataset_writer->enabled()) {
    training_examples.reserve(static_cast<size_t>(min(env.max_steps, 10000)) + 1);
    training_examples.push_back(capture_training_example(env));
  }

  while (!env.is_game_over()) {
    int player_id = env.to_play;
    PlayerConfig pc = controllers.at(player_id);
    if (pc.kind != "randombot" && pc.kind != "ai" && pc.kind != "dag")
      throw runtime_error("Unsupported mode for worker: " + pc.kind);
    TimedBotResult timed =
        timed_bot_move(env, pc.kind, move_time_seconds, max_search_depth, *evaluators[player_id]);
    int move = timed.move;
    if (move == -1) throw runtime_error(pc.kind + " has no legal move.");
    env.step(move);
    if (dataset_writer != nullptr && dataset_writer->enabled()) {
      training_examples.push_back(capture_training_example(env));
    }
  }

  if (dataset_writer != nullptr && dataset_writer->enabled()) {
    dataset_writer->append_game(training_examples, training_label_for_result(env));
  }

  WorkerResult res;
  res.p1_is_black = p1_is_black;
  res.winner = logical_winner(env, p1_is_black);
  return res;
}

void run_uci_mode(const EvaluatorMap& evaluators, double default_move_time, int default_max_depth) {
  ToguzEnv env;
  string line;
  bool newgame_received = false;

  while (getline(cin, line)) {
    if (line == "uci") {
      cout << "id name TogyzkumalakMinimaxEngine\n";
      cout << "id author Ansar Zeinulla\n";
      cout << "uciok" << endl;
    } else if (line == "ucinewgame") {
      env.reset();
      newgame_received = true;
    } else if (line == "isready") {
      // Wait for ucinewgame then isready, respond readyok
      if (newgame_received) {
        cout << "readyok" << endl;
      }
    } else if (line.substr(0, 8) == "position") {
      env.reset();
      // Start parsing after "position "
      string payload = line.substr(8);
      // Handle optional "moves" keyword if present, otherwise assume direct
      // list
      size_t moves_pos = payload.find("moves");
      if (moves_pos != string::npos) {
        payload = payload.substr(moves_pos + 5);
      }
      stringstream ss(payload);
      int move;
      while (ss >> move) {
        env.step(move - 1);
      }
    } else if (line.substr(0, 2) == "go") {
      double move_time = default_move_time;
      int max_depth = default_max_depth;
      size_t d_pos = line.find("depth");
      if (d_pos != string::npos) {
        stringstream ss(line.substr(d_pos + 6));
        ss >> max_depth;
      }
      size_t mt_pos = line.find("movetime");
      if (mt_pos != string::npos) {
        stringstream ss(line.substr(mt_pos + 8));
        double movetime_ms = 0.0;
        ss >> movetime_ms;
        if (movetime_ms > 0.0) {
          move_time = movetime_ms / 1000.0;
        }
      }
      auto result =
          minimax_engine::get_best_move_timed(env, move_time, max_depth, *evaluators[env.to_play]);
      cout << "bestmove " << (result.move + 1) << endl;
    } else if (line == "render") {
      env.render("terminal");
    } else if (line == "legal-moves") {
      auto moves = env.generate_moves();
      cout << "legal-moves";
      for (int m : moves) cout << " " << (m + 1);
      cout << endl;
    } else if (line.substr(0, 8) == "is-legal") {
      int move = stoi(line.substr(9)) - 1;
      auto moves = env.generate_moves();
      if (find(moves.begin(), moves.end(), move) != moves.end()) {
        cout << "yes" << endl;
      } else {
        cout << "no" << endl;
      }
    } else if (line == "quit") {
      break;
    }
  }
}

int main(int argc, char* argv[]) {
  Zobrist::init();
  Args args = parse_args(argc, argv);

  if (args.move_time_seconds <= 0.0) {
    cerr << "--move-time must be positive.\n";
    return 1;
  }
  if (args.max_search_depth < 1) {
    cerr << "--max-search-depth must be >= 1.\n";
    return 1;
  }

  if (args.uci) {
    EvaluatorMap evaluators = build_evaluators(args);
    run_uci_mode(evaluators, args.move_time_seconds, args.max_search_depth);
    return 0;
  }

  if (args.benchmark) {
    run_selfplay_benchmark(args);
    return 0;
  }

  if (args.compare) {
    run_fixed_depth_comparison(args);
    return 0;
  }

  if (args.numgames < 1) {
    cerr << "--numgames must be >= 1.\n";
    return 1;
  }
  if (args.noterminal && args.mode.find("human") != string::npos) {
    cerr << "--noterminal cannot be used with human players.\n";
    return 1;
  }

  map<string, Stat> stats;
  stats["p1_white"] = Stat();
  stats["p1_black"] = Stat();
  stats["total"] = Stat();

  EvaluatorMap evaluators = build_evaluators(args);
  if (!args.noterminal) {
    cout << "Evaluators: P1=" << evaluators[PLAYER_1]->description()
         << "  P2=" << evaluators[PLAYER_2]->description() << "\n";
    cout << "Time control: " << fixed << setprecision(4) << args.move_time_seconds
         << "s per half-move"
         << "  max search depth: " << args.max_search_depth << "\n";
  }

  DatasetWriter dataset_writer(args.dataset_file);
  DatasetWriter* dataset_writer_ptr = dataset_writer.enabled() ? &dataset_writer : nullptr;

  if (dataset_writer_ptr != nullptr && !args.noterminal) {
    cout << "Dataset output: " << dataset_writer.output_path() << " (JSONL append)\n";
  }

  bool use_parallel =
      (args.noterminal && args.mode.find("human") == string::npos && args.numgames > 1);

  if (use_parallel) {
    vector<future<WorkerResult>> futures;
    int max_cores = thread::hardware_concurrency();
    if (max_cores == 0) max_cores = 1;
    int launched = 0;

    while (launched < args.numgames || !futures.empty()) {
      while (futures.size() < (size_t)max_cores && launched < args.numgames) {
        bool p1_black = (!args.switch_color) || ((launched) % 2 == 0);
        futures.push_back(async(launch::async, play_game_worker, args.mode, args.position, p1_black,
                                dataset_writer_ptr, evaluators, args.move_time_seconds,
                                args.max_search_depth));
        launched++;
      }

      // wait for at least one to finish
      if (!futures.empty()) {
        bool removed = false;
        for (auto it = futures.begin(); it != futures.end();) {
          if (it->wait_for(chrono::milliseconds(10)) == future_status::ready) {
            WorkerResult res = it->get();
            update_stats(stats, res.p1_is_black, res.winner);
            it = futures.erase(it);
            removed = true;
          } else {
            ++it;
          }
        }
        if (!removed) {
          this_thread::sleep_for(chrono::milliseconds(10));
        }
      }
    }
  } else {
    ToguzEnv env;
    for (int game_idx = 1; game_idx <= args.numgames; ++game_idx) {
      bool p1_is_black = (!args.switch_color) || ((game_idx - 1) % 2 == 0);
      auto controllers = build_controllers(args.mode, p1_is_black);
      int random_plies = prepare_start_position(env, args.position);

      if (!args.noterminal) {
        cout << "=== Game " << game_idx << "/" << args.numgames << " ===\n";
        cout << "Starting mode: " << args.mode << "\n";
        if (args.position == "random") {
          cout << "Start position: random (" << random_plies << " random plies from initial).\n";
        } else if (args.position == "balanced" || args.position == "reduced") {
          cout << "Start position: balanced reduced (" << random_plies
               << " stones removed from each side).\n";
        } else {
          cout << "Start position: default initial position.\n";
        }
        cout << "Player 1 goes first. Pit numbers are 1..9.\n\n";
      }

      bool finished =
          play_single_terminal_game(env, controllers, evaluators, args.move_time_seconds,
                                    args.max_search_depth, args.noterminal, dataset_writer_ptr);
      if (!finished) return 0;

      string winner = logical_winner(env, p1_is_black);
      update_stats(stats, p1_is_black, winner);
    }
  }

  print_summary(stats);
  return 0;
}
