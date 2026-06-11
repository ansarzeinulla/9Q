#ifndef DAG_SEARCH_HPP
#define DAG_SEARCH_HPP

#include "evaluation.hpp"
#include "togyzkumalak_rules.hpp"
#include <cstdint>
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
