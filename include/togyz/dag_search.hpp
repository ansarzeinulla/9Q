#ifndef DAG_SEARCH_HPP
#define DAG_SEARCH_HPP

#include "togyz/evaluation.hpp"
#include "togyz/togyzkumalak_rules.hpp"
#include <cstdint>
#include <algorithm>
#include <memory>
#include <vector>

namespace dag_search {

constexpr double WIN_SCORE = 1000000.0;

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
inline std::unique_ptr<TranspositionTable> global_tt;

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

    void clear() {
        std::fill(table.begin(), table.end(), TTEntry{});
    }

    size_t entries() const {
        return table.size();
    }
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
};

struct TimedMoveResult {
    int move = -1;
    int completed_depth = 0;
    double eval = 0.0;
    bool timed_out = false;
};

int get_best_move(const ToguzEnv& env, int depth = 3);
int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator);
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth = 4);
TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth, const Evaluator& evaluator);
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth = 3);
std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth, const Evaluator& evaluator);
SearchStats get_last_stats();

} // namespace dag_search

#endif
