#include "togyz/dag_v2_search.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

#include "togyz/position_hash.hpp"

namespace dag_v2 {

namespace {

using Clock = std::chrono::steady_clock;
constexpr double SEARCH_INF = 10000000.0;

constexpr int QUIESCENCE_DEPTH = 6;
constexpr size_t DEFAULT_TT_MB = 256;

struct ScoredMove {
  int move;
  int score;
};

double terminal_value(int winner_code, int perspective_player) {
  if (winner_code == -1) return 0.0;
  return (winner_code == perspective_player) ? WIN_SCORE : -WIN_SCORE;
}

int move_order_key(const Bitboard& board, const std::array<int, 2>& tuzduks, int player, int move) {
  int opponent = 1 - player;
  int start = player * NUM_PITS + move;
  int stones = board.get(start);
  if (stones <= 0) return 100;

  int last = landing_pit(start, stones);
  if (last / NUM_PITS == opponent) {
    int col = last % NUM_PITS;
    int val_after = board.get(last) + 1;
    if (val_after == 3 && tuzduks[player] == -1 && col != 8 && tuzduks[opponent] != col) {
      return 0;
    }
    if (val_after % 2 == 0) return 1;
  }

  if (board.get(last) == 0) return 2;
  return 3;
}

void sort_moves(const Bitboard& board, const std::array<int, 2>& tuzduks, int player,
                std::array<int, 9>& moves, int count) {
  for (int i = 0; i < count - 1; ++i) {
    int best_idx = i;
    int best_key = move_order_key(board, tuzduks, player, moves[i]);
    for (int j = i + 1; j < count; ++j) {
      int key = move_order_key(board, tuzduks, player, moves[j]);
      if (key < best_key) {
        best_key = key;
        best_idx = j;
      }
    }
    std::swap(moves[i], moves[best_idx]);
  }
}

// Ordering bands (highest first): TT move > tactical (capture/tuzdyk) >
// killer slots > quiets (+history). Killers and history apply to quiet moves
// only and never outrank tactical moves — this game is capture-heavy.
void order_moves(std::array<int, 9>& moves, int count, const Bitboard& board,
                 const std::array<int, 2>& tuzduks, int player, uint16_t tt_best_move,
                 const KillerTable* killers = nullptr, int ply = -1,
                 const HistoryTable* history = nullptr) {
  std::array<ScoredMove, 9> scored_moves{};
  for (int i = 0; i < count; ++i) {
    int move = moves[i];
    int score = 0;
    int key = move_order_key(board, tuzduks, player, move);
    if (static_cast<uint16_t>(move) == tt_best_move) {
      score = 100000;
    } else if (key <= 1) {
      score = 1000 - key;
    } else if (killers && killers->slot(ply, 0) == move) {
      score = 500;
    } else if (killers && killers->slot(ply, 1) == move) {
      score = 499;
    } else {
      score = 10 - key;
      if (history) {
        score += std::min(history->get(player, move) / 8, 400);
      }
    }
    scored_moves[static_cast<size_t>(i)] = ScoredMove{move, score};
  }

  std::sort(scored_moves.begin(), scored_moves.begin() + count,
            [](const ScoredMove& a, const ScoredMove& b) {
              if (a.score != b.score) return a.score > b.score;
              return a.move < b.move;
            });

  for (int i = 0; i < count; ++i) {
    moves[static_cast<size_t>(i)] = scored_moves[static_cast<size_t>(i)].move;
  }
}

bool is_tactical_transition(const std::array<int, 2>& before_kazans,
                            const std::array<int, 2>& before_tuzduks,
                            const std::array<int, 2>& after_kazans,
                            const std::array<int, 2>& after_tuzduks, int mover) {
  return after_kazans[mover] > before_kazans[mover] ||
         after_tuzduks[mover] != before_tuzduks[mover];
}

double evaluate_leaf(const Bitboard& board, const std::array<int, 2>& kazans,
                     const std::array<int, 2>& tuzduks, int to_play, int winner_code, bool terminal,
                     const Evaluator& evaluator) {
  return evaluator.evaluate_position(
      BoardState{board, kazans, tuzduks, to_play, winner_code, to_play, terminal});
}

class DagSearcher {
 public:
  explicit DagSearcher(size_t table_size_mb, const std::vector<uint64_t>& root_history,
                       const Evaluator& evaluator_ref, SearchOptions options = SearchOptions{},
                       const Clock::time_point* deadline_ref = nullptr,
                       bool* timed_out_ref = nullptr)
      : opts(options),
        evaluator(evaluator_ref),
        deadline(deadline_ref),
        timed_out(timed_out_ref),
        history_stack(root_history) {
    if (!global_tt) {
      init_tt(table_size_mb);
    }
  }

  double search(Bitboard& board, std::array<int, 2>& kazans, std::array<int, 2>& tuzduks,
                int to_play, int steps, int max_steps, int winner_code, int depth, double alpha,
                double beta, int ply) {
    if (deadline_reached()) {
      return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code, winner_code != -2,
                           evaluator);
    }
    stats.nodes++;

    if (winner_code != -2) {
      stats.terminal_hits++;
      return terminal_value(winner_code, to_play);
    }

    if (depth == 0) {
      return qsearch(board, kazans, tuzduks, to_play, steps, max_steps, winner_code, alpha, beta,
                     QUIESCENCE_DEPTH);
    }

    double alpha_original = alpha;
    bool use_tt = !has_repetition_pressure();
    uint64_t tt_hash = 0;
    TTEntry tt_entry;
    uint16_t tt_best_move = 0;
    if (use_tt) {
      tt_hash = exact_hash(board, kazans, tuzduks, to_play);
      if (global_tt && global_tt->probe(tt_hash, tt_entry) &&
          tt_entry.depth >= static_cast<uint8_t>(std::clamp(depth, 0, 255))) {
        tt_best_move = tt_entry.best_move;
        double stored_eval = static_cast<double>(tt_entry.eval);
        if (tt_entry.flag == 1) {
          stats.tt_hits++;
          return stored_eval;
        }
        if (tt_entry.flag == 2 && stored_eval >= beta) {
          stats.tt_hits++;
          return stored_eval;
        }
        if (tt_entry.flag == 3 && stored_eval <= alpha) {
          stats.tt_hits++;
          return stored_eval;
        }
      }
    }

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play, moves);
    stats.generated_moves += static_cast<uint64_t>(count);

    if (count == 0) {
      stats.leaves++;
      return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code, false, evaluator);
    }

    order_moves(moves, count, board, tuzduks, to_play, tt_best_move,
                opts.use_killers ? &killers : nullptr, ply, opts.use_history ? &history : nullptr);

    Bitboard board_backup = board;
    auto kazans_backup = kazans;
    auto tuzduks_backup = tuzduks;
#ifndef NDEBUG
    size_t history_size_at_entry = history_stack.size();
#endif

    double best = -SEARCH_INF;
    uint16_t node_best_move = tt_best_move;
    for (int i = 0; i < count; ++i) {
      if (deadline_reached()) break;
      int next_winner = -2;
      int next_to_play = 1 - to_play;
      bool terminal = false;
      int next_steps = steps;
      uint64_t ignored_hash = 0;

      ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play, next_steps, max_steps,
                            next_winner, next_to_play, terminal, ignored_hash);

      double child_value;
      if (terminal) {
        stats.terminal_hits++;
        child_value = terminal_value(next_winner, next_to_play);
      } else if (enter_repetition(exact_hash(board, kazans, tuzduks, next_to_play))) {
        child_value = 0.0;
      } else if (opts.use_pvs && i > 0) {
        // Principal Variation Search: probe non-first moves with a null
        // window; re-search with the full window only on a fail-high.
        child_value = search(board, kazans, tuzduks, next_to_play, next_steps, max_steps,
                             next_winner, depth - 1, -(alpha + 1.0), -alpha, ply + 1);
        double probe = -child_value;
        if (probe > alpha && probe < beta && !(timed_out && *timed_out)) {
          stats.pvs_researches++;
          child_value = search(board, kazans, tuzduks, next_to_play, next_steps, max_steps,
                               next_winner, depth - 1, -beta, -alpha, ply + 1);
        }
      } else {
        child_value = search(board, kazans, tuzduks, next_to_play, next_steps, max_steps,
                             next_winner, depth - 1, -beta, -alpha, ply + 1);
      }
      if (timed_out && *timed_out) {
        board = board_backup;
        kazans = kazans_backup;
        tuzduks = tuzduks_backup;
        break;
      }
      if (!terminal) {
        leave_repetition();
      }

      double value = -child_value;

      board = board_backup;
      kazans = kazans_backup;
      tuzduks = tuzduks_backup;
#ifndef NDEBUG
      assert(history_stack.size() == history_size_at_entry);
#endif

      if (value <= -WIN_SCORE) {
        stats.pruned_losing_moves++;
      }

      if (value >= WIN_SCORE) {
        stats.forced_wins++;
        best = value;
        node_best_move = static_cast<uint16_t>(moves[i]);
        alpha = std::max(alpha, value);
        break;
      }

      if (value > best) {
        best = value;
        node_best_move = static_cast<uint16_t>(moves[i]);
      }
      if (value > alpha) alpha = value;

      if (alpha >= beta) {
        stats.beta_cuts++;
        if (move_order_key(board, tuzduks, to_play, moves[i]) >= 2 &&
            static_cast<uint16_t>(moves[i]) != tt_best_move) {
          if (opts.use_killers) killers.record(ply, moves[i]);
          if (opts.use_history) history.reward(to_play, moves[i], depth);
        }
        break;
      }
    }

    uint8_t flag = 1;
    if (best <= alpha_original) {
      flag = 3;
    } else if (best >= beta) {
      flag = 2;
    }
    if (use_tt) {
      global_tt->store(tt_hash, static_cast<int16_t>(std::clamp(best, -32768.0, 32767.0)),
                       static_cast<uint8_t>(std::clamp(depth, 0, 255)), flag, node_best_move);
      stats.tt_stores++;
      stats.table_entries = global_tt->entries();
    }
    return best;
  }

  double qsearch(Bitboard& board, std::array<int, 2>& kazans, std::array<int, 2>& tuzduks,
                 int to_play, int steps, int max_steps, int winner_code, double alpha, double beta,
                 int q_depth) {
    if (deadline_reached()) {
      return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code, winner_code != -2,
                           evaluator);
    }
    stats.nodes++;

    if (winner_code != -2) {
      stats.terminal_hits++;
      return terminal_value(winner_code, to_play);
    }

    double stand_pat =
        evaluate_leaf(board, kazans, tuzduks, to_play, winner_code, false, evaluator);
    if (stand_pat >= beta) {
      stats.beta_cuts++;
      return stand_pat;
    }
    if (stand_pat > alpha) alpha = stand_pat;

    if (q_depth == 0) {
      stats.leaves++;
      return stand_pat;
    }

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play, moves);
    stats.generated_moves += static_cast<uint64_t>(count);
    if (count == 0) {
      stats.leaves++;
      return stand_pat;
    }

    order_moves(moves, count, board, tuzduks, to_play, 0);

    Bitboard board_backup = board;
    auto kazans_backup = kazans;
    auto tuzduks_backup = tuzduks;

    double best = stand_pat;
    for (int i = 0; i < count; ++i) {
      if (deadline_reached()) break;
      int next_winner = -2;
      int next_to_play = 1 - to_play;
      bool terminal = false;
      int next_steps = steps;
      uint64_t ignored_hash = 0;

      ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play, next_steps, max_steps,
                            next_winner, next_to_play, terminal, ignored_hash);

      bool tactical = terminal || is_tactical_transition(kazans_backup, tuzduks_backup, kazans,
                                                         tuzduks, to_play);

      double value = -SEARCH_INF;
      if (tactical) {
        double child_value;
        if (terminal) {
          stats.terminal_hits++;
          child_value = terminal_value(next_winner, next_to_play);
        } else if (enter_repetition(exact_hash(board, kazans, tuzduks, next_to_play))) {
          child_value = 0.0;
        } else {
          child_value = qsearch(board, kazans, tuzduks, next_to_play, next_steps, max_steps,
                                next_winner, -beta, -alpha, q_depth - 1);
        }
        if (timed_out && *timed_out) {
          board = board_backup;
          kazans = kazans_backup;
          tuzduks = tuzduks_backup;
          break;
        }
        if (!terminal) {
          leave_repetition();
        }
        value = -child_value;
      }

      board = board_backup;
      kazans = kazans_backup;
      tuzduks = tuzduks_backup;

      if (!tactical) continue;

      if (value >= WIN_SCORE) {
        stats.forced_wins++;
        return value;
      }
      if (value > best) best = value;
      if (value > alpha) alpha = value;
      if (alpha >= beta) {
        stats.beta_cuts++;
        return best;
      }
    }

    return best;
  }

  size_t table_entries() const { return global_tt ? global_tt->entries() : 0; }

  uint64_t exact_hash(const Bitboard& board, const std::array<int, 2>& kazans,
                      const std::array<int, 2>& tuzduks, int to_play) const {
    return Zobrist::get_initial_hash_bitboard(board, kazans, tuzduks, to_play);
  }

  bool enter_repetition(uint64_t hash) {
    history_stack.push_back(hash);
    int seen = 0;
    for (int i = static_cast<int>(history_stack.size()) - 1; i >= 0; i -= 2) {
      if (history_stack[static_cast<size_t>(i)] == hash) {
        ++seen;
        if (seen >= 3) {
          stats.repetition_draws++;
          return true;
        }
      }
    }
    return false;
  }

  void leave_repetition() {
    if (!history_stack.empty()) history_stack.pop_back();
  }

  void reset_iteration_stats() { stats = SearchStats{}; }

  SearchStats stats;
  SearchOptions opts;
  KillerTable killers;
  HistoryTable history;

 private:
  const Evaluator& evaluator;
  const Clock::time_point* deadline;
  bool* timed_out;
  std::vector<uint64_t> history_stack;

  bool deadline_reached() {
    if (!deadline || !timed_out) return false;
    if (*timed_out) return true;
    if (Clock::now() < *deadline) return false;
    *timed_out = true;
    return true;
  }

  bool has_repetition_pressure() const {
    if (history_stack.empty()) return false;
    uint64_t current = history_stack.back();
    int seen = 0;
    for (int i = static_cast<int>(history_stack.size()) - 1; i >= 0; i -= 2) {
      if (history_stack[static_cast<size_t>(i)] == current) {
        ++seen;
        if (seen > 1) return true;
      }
    }
    return false;
  }
};

thread_local SearchStats last_stats;

// Single-depth root search over [alpha0, beta0]. `root_hint` seeds the move
// ordering (previous iteration's best move; 0 reproduces the v1 quirk of
// boosting pit 0). PVS applies across root moves when enabled.
MoveEval search_root(DagSearcher& searcher, const ToguzEnv& env, int depth,
                     const Evaluator& evaluator, double alpha0, double beta0, uint16_t root_hint,
                     bool* timed_out = nullptr) {
  (void)evaluator;
  MoveEval result;
  if (depth < 1) return result;

  Bitboard board = env.board;
  auto kazans = env.kazans;
  auto tuzduks = env.tuzduks;

  std::array<int, 9> moves;
  int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
  if (count == 0) return result;

  order_moves(moves, count, board, tuzduks, env.to_play, root_hint,
              searcher.opts.use_killers ? &searcher.killers : nullptr, 0,
              searcher.opts.use_history ? &searcher.history : nullptr);

  int best_move = moves[0];
  double best_value = -SEARCH_INF;
  double alpha = alpha0;
  double beta = beta0;

  for (int i = 0; i < count; ++i) {
    if (timed_out && *timed_out) break;

    Bitboard b_tmp = board;
    auto k_tmp = kazans;
    auto t_tmp = tuzduks;
    int next_winner = -2;
    int next_to_play = 1 - env.to_play;
    bool terminal = false;
    int next_steps = env.steps;
    uint64_t ignored_hash = 0;

    ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play, next_steps, env.max_steps,
                          next_winner, next_to_play, terminal, ignored_hash);

    double child_value;
    if (terminal) {
      searcher.stats.terminal_hits++;
      child_value = terminal_value(next_winner, next_to_play);
    } else if (searcher.enter_repetition(searcher.exact_hash(b_tmp, k_tmp, t_tmp, next_to_play))) {
      child_value = 0.0;
    } else if (searcher.opts.use_pvs && i > 0) {
      child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play, next_steps, env.max_steps,
                                    next_winner, depth - 1, -(alpha + 1.0), -alpha, 1);
      double probe = -child_value;
      if (probe > alpha && probe < beta && !(timed_out && *timed_out)) {
        searcher.stats.pvs_researches++;
        child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play, next_steps, env.max_steps,
                                      next_winner, depth - 1, -beta, -alpha, 1);
      }
    } else {
      child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play, next_steps, env.max_steps,
                                    next_winner, depth - 1, -beta, -alpha, 1);
    }
    if (timed_out && *timed_out) break;
    if (!terminal) {
      searcher.leave_repetition();
    }

    double value = -child_value;
    if (value > best_value) {
      best_value = value;
      best_move = moves[i];
    }
    if (value > alpha) alpha = value;
    if (value >= WIN_SCORE) {
      searcher.stats.forced_wins++;
      break;
    }
    if (alpha >= beta) break;
  }

  searcher.stats.table_entries = static_cast<uint64_t>(searcher.table_entries());
  result.move = best_move;
  result.eval = best_value;
  result.immediate_win = best_value >= WIN_SCORE;
  result.forced_loss = best_value <= -WIN_SCORE;
  return result;
}

// Unified iterative-deepening driver used by both the timed and untimed entry
// points (deadline == nullptr means untimed). Aspiration windows narrow the
// root window around the previous iteration's score.
TimedMoveResult search_iterative(const ToguzEnv& env, int max_depth, const Evaluator& evaluator,
                                 SearchOptions opts, const Clock::time_point* deadline) {
  TimedMoveResult best;

  std::array<int, 9> moves;
  auto board = env.board;
  auto tuzduks = env.tuzduks;
  int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
  if (count <= 0 || max_depth < 1) return best;
  sort_moves(board, tuzduks, env.to_play, moves, count);
  best.move = moves[0];

  bool timed_out = false;
  DagSearcher searcher(DEFAULT_TT_MB, env.history_stack, evaluator, opts, deadline,
                       deadline ? &timed_out : nullptr);

  double prev_score = 0.0;
  bool have_prev = false;
  uint16_t root_hint = 0;
  uint64_t accumulated_nodes = 0;

  for (int depth = 1; depth <= max_depth; ++depth) {
    searcher.reset_iteration_stats();

    AspirationWindow window = (opts.use_aspiration && have_prev && depth >= 2 &&
                               !AspirationWindow::is_mate_bound(prev_score))
                                  ? AspirationWindow::initial(prev_score)
                                  : AspirationWindow::full();

    MoveEval current;
    while (true) {
      current = search_root(searcher, env, depth, evaluator, window.alpha, window.beta, root_hint,
                            deadline ? &timed_out : nullptr);
      if (timed_out || current.move < 0) break;
      if (!window.is_full() && current.eval <= window.alpha) {
        searcher.stats.aspiration_researches++;
        window = window.widen_fail_low();
        continue;
      }
      if (!window.is_full() && current.eval >= window.beta) {
        searcher.stats.aspiration_researches++;
        window = window.widen_fail_high();
        continue;
      }
      break;
    }

    accumulated_nodes += searcher.stats.nodes;
    if (timed_out || current.move < 0) {
      best.timed_out = true;
      break;
    }

    best.move = current.move;
    best.eval = current.eval;
    best.completed_depth = depth;
    best.timed_out = false;
    prev_score = current.eval;
    have_prev = true;
    root_hint = static_cast<uint16_t>(current.move);

    if (current.eval >= WIN_SCORE) break;
    if (deadline && Clock::now() >= *deadline) {
      best.timed_out = true;
      break;
    }
  }

  SearchStats final_stats = searcher.stats;
  final_stats.nodes = accumulated_nodes;
  last_stats = final_stats;
  return best;
}

}  // namespace

int get_best_move(const ToguzEnv& env, int depth) {
  static const HeuristicEvaluator heuristic;
  return get_best_move(env, depth, heuristic, SearchOptions{});
}

int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator,
                  SearchOptions options) {
  if (!options.any()) {
    // Baseline mode: exact v1 code path — single fixed-depth root search,
    // no iterative deepening (the equivalence tests pin this).
    bool* no_timeout = nullptr;
    DagSearcher searcher(DEFAULT_TT_MB, env.history_stack, evaluator, options);
    MoveEval result =
        search_root(searcher, env, depth, evaluator, -SEARCH_INF, SEARCH_INF, 0, no_timeout);
    last_stats = searcher.stats;
    return result.move;
  }
  return search_iterative(env, depth, evaluator, options, nullptr).move;
}

double get_root_score(const ToguzEnv& env, int depth, const Evaluator& evaluator,
                      SearchOptions options) {
  if (!options.use_aspiration) {
    DagSearcher searcher(DEFAULT_TT_MB, env.history_stack, evaluator, options);
    MoveEval result =
        search_root(searcher, env, depth, evaluator, -SEARCH_INF, SEARCH_INF, 0, nullptr);
    last_stats = searcher.stats;
    return result.eval;
  }
  return search_iterative(env, depth, evaluator, options, nullptr).eval;
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move, int max_depth) {
  static const HeuristicEvaluator heuristic;
  return get_best_move_timed(env, seconds_per_move, max_depth, heuristic, SearchOptions{});
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move, int max_depth,
                                    const Evaluator& evaluator, SearchOptions options) {
  seconds_per_move = std::clamp(seconds_per_move, 0.001, 30.0);
  auto budget =
      std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds_per_move));
  Clock::time_point deadline = Clock::now() + budget;
  return search_iterative(env, max_depth, evaluator, options, &deadline);
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth) {
  static const HeuristicEvaluator heuristic;
  return get_all_moves_with_evals(env, depth, heuristic);
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth,
                                               const Evaluator& evaluator) {
  if (depth < 1) return {};

  Bitboard board = env.board;
  auto kazans = env.kazans;
  auto tuzduks = env.tuzduks;

  std::array<int, 9> moves;
  int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
  if (count == 0) return {};

  sort_moves(board, tuzduks, env.to_play, moves, count);

  DagSearcher searcher(DEFAULT_TT_MB, env.history_stack, evaluator, SearchOptions::v1_baseline());
  std::vector<MoveEval> results;
  results.reserve(static_cast<size_t>(count));

  double alpha = -SEARCH_INF;
  double beta = SEARCH_INF;
  bool winning_move_found = false;

  for (int i = 0; i < count; ++i) {
    if (winning_move_found) {
      results.push_back(MoveEval{moves[i], 0.0, false, false, true});
      continue;
    }

    Bitboard b_tmp = board;
    auto k_tmp = kazans;
    auto t_tmp = tuzduks;
    int next_winner = -2;
    int next_to_play = 1 - env.to_play;
    bool terminal = false;
    int next_steps = env.steps;
    uint64_t ignored_hash = 0;

    ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play, next_steps, env.max_steps,
                          next_winner, next_to_play, terminal, ignored_hash);

    double child_value;
    if (terminal) {
      searcher.stats.terminal_hits++;
      child_value = terminal_value(next_winner, next_to_play);
    } else if (searcher.enter_repetition(searcher.exact_hash(b_tmp, k_tmp, t_tmp, next_to_play))) {
      child_value = 0.0;
    } else {
      child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play, next_steps, env.max_steps,
                                    next_winner, depth - 1, -beta, -alpha, 1);
    }
    if (!terminal) {
      searcher.leave_repetition();
    }

    double value = -child_value;
    if (value > alpha) alpha = value;

    MoveEval eval;
    eval.move = moves[i];
    eval.eval = value;
    eval.immediate_win = value >= WIN_SCORE;
    eval.forced_loss = value <= -WIN_SCORE;
    results.push_back(eval);

    if (eval.immediate_win) {
      searcher.stats.forced_wins++;
      winning_move_found = true;
    }
  }

  searcher.stats.table_entries = static_cast<uint64_t>(searcher.table_entries());
  last_stats = searcher.stats;
  return results;
}

SearchStats get_last_stats() { return last_stats; }

}  // namespace dag_v2
