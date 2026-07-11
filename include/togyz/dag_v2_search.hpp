#ifndef DAG_V2_SEARCH_HPP
#define DAG_V2_SEARCH_HPP

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"

namespace dag_v2 {

constexpr double WIN_SCORE = 1000000.0;
constexpr int MAX_PLY = 128;

struct MoveEval {
  int move = -1;
  double eval = 0.0;
  bool immediate_win = false;
  bool forced_loss = false;
  bool skipped = false;
};

struct TTEntry {
  uint64_t lock = 0;
  int16_t eval = 0;
  uint8_t depth = 0;
  uint8_t flag = 0;
  uint8_t best_move = 0;
  uint8_t padding[3] = {0, 0, 0};
};

static_assert(sizeof(TTEntry) == 16, "TTEntry must be strictly 16 bytes for cache locality");

class TranspositionTable;
inline thread_local std::unique_ptr<TranspositionTable> global_tt;

inline void init_tt(size_t size_in_mb) {
  if (size_in_mb < 4) size_in_mb = 4;
  global_tt = std::make_unique<TranspositionTable>(size_in_mb);
}

class TranspositionTable {
 private:
  std::vector<TTEntry> table;
  size_t mask = 0;

  static size_t floor_power_of_two(size_t value) {
    size_t power_of_two = 1;
    while ((power_of_two << 1) <= value) {
      power_of_two <<= 1;
    }
    return power_of_two;
  }

 public:
  explicit TranspositionTable(size_t size_in_mb) {
    size_t num_entries = (size_in_mb * 1024ULL * 1024ULL) / sizeof(TTEntry);
    size_t table_size = floor_power_of_two(std::max<size_t>(1, num_entries));
    table.resize(table_size);
    mask = table_size - 1;
  }

  inline void store(uint64_t hash, int16_t eval, uint8_t depth, uint8_t flag, uint16_t best_move) {
    size_t idx = static_cast<size_t>(hash) & mask;
    TTEntry& slot = table[idx];
    if (slot.lock == 0 || depth >= slot.depth) {
      slot = TTEntry{hash, eval, depth, flag, static_cast<uint8_t>(best_move), {0, 0, 0}};
    }
  }

  inline bool probe(uint64_t hash, TTEntry& out_entry) const {
    size_t idx = static_cast<size_t>(hash) & mask;
    const TTEntry& slot = table[idx];
    if (slot.lock == hash) {
      out_entry = slot;
      return true;
    }
    return false;
  }

  void clear() { std::fill(table.begin(), table.end(), TTEntry{}); }

  size_t entries() const { return table.size(); }
};

struct SearchStats {
  uint64_t nodes = 0;
  uint64_t leaves = 0;
  uint64_t terminal_hits = 0;
  uint64_t tt_hits = 0;
  uint64_t tt_stores = 0;
  uint64_t beta_cuts = 0;
  uint64_t forced_wins = 0;
  uint64_t repetition_draws = 0;
  uint64_t generated_moves = 0;
  uint64_t pruned_losing_moves = 0;
  uint64_t table_entries = 0;
  uint64_t aspiration_researches = 0;
  uint64_t pvs_researches = 0;
};

struct TimedMoveResult {
  int move = -1;
  int completed_depth = 0;
  double eval = 0.0;
  bool timed_out = false;
};

// Feature toggles. Defaults enable every DAGv2 improvement; v1_baseline()
// reproduces DAGv1 behavior exactly (used by the equivalence tests).
struct SearchOptions {
  bool use_killers = true;
  bool use_history = true;
  bool use_aspiration = true;
  bool use_pvs = true;

  static SearchOptions v1_baseline() { return SearchOptions{false, false, false, false}; }
  bool any() const { return use_killers || use_history || use_aspiration || use_pvs; }
};

// Two killer-move slots per ply. Quiet moves that caused a beta cutoff.
struct KillerTable {
  std::array<std::array<int, 2>, MAX_PLY> slots{};

  KillerTable() { clear(); }

  void clear() {
    for (auto& s : slots) s = {-1, -1};
  }

  void record(int ply, int move) {
    if (ply < 0 || ply >= MAX_PLY) return;
    auto& s = slots[static_cast<size_t>(ply)];
    if (s[0] == move) return;
    s[1] = s[0];
    s[0] = move;
  }

  int slot(int ply, int idx) const {
    if (ply < 0 || ply >= MAX_PLY) return -1;
    return slots[static_cast<size_t>(ply)][static_cast<size_t>(idx)];
  }

  bool is_killer(int ply, int move) const {
    if (ply < 0 || ply >= MAX_PLY) return false;
    const auto& s = slots[static_cast<size_t>(ply)];
    return s[0] == move || s[1] == move;
  }
};

// History heuristic: cutoff counts per (player, pit), weighted depth^2.
struct HistoryTable {
  static constexpr int32_t HALVE_THRESHOLD = 1 << 20;

  std::array<std::array<int32_t, NUM_PITS>, 2> score{};

  void clear() {
    for (auto& row : score) row.fill(0);
  }

  void reward(int player, int move, int depth) {
    int32_t& s = score[static_cast<size_t>(player)][static_cast<size_t>(move)];
    s += depth * depth;
    if (s > HALVE_THRESHOLD) {
      for (auto& row : score) {
        for (auto& v : row) v /= 2;
      }
    }
  }

  int get(int player, int move) const {
    return score[static_cast<size_t>(player)][static_cast<size_t>(move)];
  }
};

// Aspiration window for iterative deepening: a narrow (alpha, beta) around
// the previous iteration's score, widened exponentially on fail-high/low.
struct AspirationWindow {
  static constexpr double INITIAL_DELTA = 50.0;
  static constexpr double FULL_BOUND = 10000000.0;  // matches SEARCH_INF
  static constexpr double MATE_BOUND = 32000.0;     // TT int16 clamp band

  double alpha = -FULL_BOUND;
  double beta = FULL_BOUND;
  double delta = INITIAL_DELTA;

  static AspirationWindow initial(double prev_score, double d = INITIAL_DELTA) {
    return AspirationWindow{prev_score - d, prev_score + d, d};
  }

  static AspirationWindow full() { return AspirationWindow{}; }

  bool is_full() const { return alpha <= -FULL_BOUND && beta >= FULL_BOUND; }

  AspirationWindow widen_fail_high() const {
    AspirationWindow w = *this;
    w.delta = delta * 4.0;
    if (w.delta > MATE_BOUND) return full();
    w.beta = beta + w.delta;
    return w;
  }

  AspirationWindow widen_fail_low() const {
    AspirationWindow w = *this;
    w.delta = delta * 4.0;
    if (w.delta > MATE_BOUND) return full();
    w.alpha = alpha - w.delta;
    return w;
  }

  static bool is_mate_bound(double score) { return score >= MATE_BOUND || score <= -MATE_BOUND; }
};

int get_best_move(const ToguzEnv& env, int depth = 3);
int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator,
                  SearchOptions options = SearchOptions{});
double get_root_score(const ToguzEnv& env, int depth, const Evaluator& evaluator,
                      SearchOptions options = SearchOptions{});
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth = 4);
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move, int max_depth,
                                    const Evaluator& evaluator,
                                    SearchOptions options = SearchOptions{});
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth = 3);
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth,
                                               const Evaluator& evaluator);
SearchStats get_last_stats();

}  // namespace dag_v2

#endif
