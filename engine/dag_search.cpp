#include "dag_search.hpp"
#include "position_hash.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace dag_search {

namespace {

using Clock = std::chrono::steady_clock;
constexpr double SEARCH_INF = 10000000.0;

#ifndef DAG_Q_DEPTH
#define DAG_Q_DEPTH 6
#endif

constexpr int QUIESCENCE_DEPTH = DAG_Q_DEPTH;

enum class Bound : uint8_t {
    Exact,
    Lower,
    Upper,
};

struct NormalizedKey {
    uint64_t side1_lo = 0;
    uint64_t side1_hi = 0;
    uint64_t side2_lo = 0;
    uint64_t side2_hi = 0;
    uint16_t kazan_me = 0;
    uint16_t kazan_opp = 0;
    int8_t tuzduk_me = -1;
    int8_t tuzduk_opp = -1;

    bool operator==(const NormalizedKey& other) const {
        return side1_lo == other.side1_lo &&
               side1_hi == other.side1_hi &&
               side2_lo == other.side2_lo &&
               side2_hi == other.side2_hi &&
               kazan_me == other.kazan_me &&
               kazan_opp == other.kazan_opp &&
               tuzduk_me == other.tuzduk_me &&
               tuzduk_opp == other.tuzduk_opp;
    }
};

static uint64_t mix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct KeyHash {
    size_t operator()(const NormalizedKey& k) const {
        uint64_t h = mix64(k.side1_lo);
        h ^= mix64(k.side1_hi + 0x9e3779b97f4a7c15ULL);
        h ^= mix64(k.side2_lo + 0xbf58476d1ce4e5b9ULL);
        h ^= mix64(k.side2_hi + 0x94d049bb133111ebULL);
        h ^= mix64((uint64_t(k.kazan_me) << 32) | k.kazan_opp);
        h ^= mix64((uint64_t(uint8_t(k.tuzduk_me + 1)) << 8) |
                   uint8_t(k.tuzduk_opp + 1));
        return static_cast<size_t>(h);
    }
};

struct TTEntry {
    double value = 0.0;
    int depth = -1;
    Bound bound = Bound::Exact;
};

NormalizedKey normalize_key(const Bitboard& board,
                            const std::array<int, 2>& kazans,
                            const std::array<int, 2>& tuzduks,
                            int to_play) {
    int opponent = 1 - to_play;
    uint128 mover_side = (to_play == PLAYER_1) ? board.side1 : board.side2;
    uint128 opponent_side = (to_play == PLAYER_1) ? board.side2 : board.side1;

    NormalizedKey key;
    key.side1_lo = static_cast<uint64_t>(mover_side);
    key.side1_hi = static_cast<uint64_t>(mover_side >> 64);
    key.side2_lo = static_cast<uint64_t>(opponent_side);
    key.side2_hi = static_cast<uint64_t>(opponent_side >> 64);
    key.kazan_me = static_cast<uint16_t>(kazans[to_play]);
    key.kazan_opp = static_cast<uint16_t>(kazans[opponent]);
    key.tuzduk_me = static_cast<int8_t>(tuzduks[to_play]);
    key.tuzduk_opp = static_cast<int8_t>(tuzduks[opponent]);
    return key;
}

double terminal_value(int winner_code, int perspective_player) {
    if (winner_code == -1) return 0.0;
    return (winner_code == perspective_player) ? WIN_SCORE : -WIN_SCORE;
}

int move_order_key(const Bitboard& board, const std::array<int, 2>& tuzduks,
                   int player, int move) {
    int opponent = 1 - player;
    int start = player * NUM_PITS + move;
    int stones = board.get(start);
    if (stones <= 0) return 100;

    int last = (stones == 1) ? ((start + 1) % 18) : ((start + stones - 1) % 18);
    if (last / NUM_PITS == opponent) {
        int col = last % NUM_PITS;
        int val_after = board.get(last) + 1;
        if (val_after == 3 && tuzduks[player] == -1 && col != 8 &&
            tuzduks[opponent] != col) {
            return 0;
        }
        if (val_after % 2 == 0) return 1;
    }

    if (board.get(last) == 0) return 2;
    return 3;
}

void sort_moves(const Bitboard& board, const std::array<int, 2>& tuzduks,
                int player, std::array<int, 9>& moves, int count) {
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

bool is_tactical_transition(const std::array<int, 2>& before_kazans,
                            const std::array<int, 2>& before_tuzduks,
                            const std::array<int, 2>& after_kazans,
                            const std::array<int, 2>& after_tuzduks,
                            int mover) {
    return after_kazans[mover] > before_kazans[mover] ||
           after_tuzduks[mover] != before_tuzduks[mover];
}

double evaluate_leaf(const Bitboard& board, const std::array<int, 2>& kazans,
                     const std::array<int, 2>& tuzduks, int to_play,
                     int winner_code, bool terminal,
                     const Evaluator& evaluator) {
    return evaluator.evaluate_position(BoardState{board, kazans, tuzduks,
                                                  to_play, winner_code,
                                                  to_play, terminal});
}

class DagSearcher {
public:
    explicit DagSearcher(size_t reserve_hint,
                         const std::unordered_map<uint64_t, int>& root_repetitions,
                         const Evaluator& evaluator_ref,
                         const Clock::time_point* deadline_ref = nullptr,
                         bool* timed_out_ref = nullptr)
        : evaluator(evaluator_ref), deadline(deadline_ref),
          timed_out(timed_out_ref), repetitions(root_repetitions) {
        table.reserve(reserve_hint);
    }

    double search(Bitboard& board, std::array<int, 2>& kazans,
                  std::array<int, 2>& tuzduks, int to_play, int steps,
                  int max_steps, int winner_code, int depth,
                  double alpha, double beta) {
        if (deadline_reached()) {
            return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code,
                                 winner_code != -2, evaluator);
        }
        stats.nodes++;

        if (winner_code != -2) {
            stats.terminal_hits++;
            return terminal_value(winner_code, to_play);
        }

        if (depth == 0) {
            return qsearch(board, kazans, tuzduks, to_play, steps, max_steps,
                           winner_code, alpha, beta, QUIESCENCE_DEPTH);
        }

        double alpha_original = alpha;
        bool use_tt = !has_repetition_pressure();
        NormalizedKey key;
        if (use_tt) {
            key = normalize_key(board, kazans, tuzduks, to_play);
            auto found = table.find(key);
            if (found != table.end() && found->second.depth >= depth) {
                const TTEntry& entry = found->second;
                if (entry.bound == Bound::Exact) {
                    stats.tt_hits++;
                    return entry.value;
                }
                if (entry.bound == Bound::Lower && entry.value >= beta) {
                    stats.tt_hits++;
                    return entry.value;
                }
                if (entry.bound == Bound::Upper && entry.value <= alpha) {
                    stats.tt_hits++;
                    return entry.value;
                }
            }
        }

        std::array<int, 9> moves;
        int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play, moves);
        stats.generated_moves += static_cast<uint64_t>(count);

        if (count == 0) {
            stats.leaves++;
            return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code,
                                 false, evaluator);
        }

        sort_moves(board, tuzduks, to_play, moves, count);

        Bitboard board_backup = board;
        auto kazans_backup = kazans;
        auto tuzduks_backup = tuzduks;

        double best = -SEARCH_INF;
        for (int i = 0; i < count; ++i) {
            if (deadline_reached()) break;
            int next_winner = -2;
            int next_to_play = 1 - to_play;
            bool terminal = false;
            int next_steps = steps;
            uint64_t ignored_hash = 0;

            ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play,
                                  next_steps, max_steps, next_winner,
                                  next_to_play, terminal, ignored_hash);

            double child_value;
            if (terminal) {
                stats.terminal_hits++;
                child_value = terminal_value(next_winner, next_to_play);
            } else if (enter_repetition(exact_hash(board, kazans, tuzduks,
                                                   next_to_play))) {
                child_value = 0.0;
            } else {
                child_value = search(board, kazans, tuzduks, next_to_play,
                                     next_steps, max_steps, next_winner,
                                     depth - 1, -beta, -alpha);
            }
            if (timed_out && *timed_out) {
                board = board_backup;
                kazans = kazans_backup;
                tuzduks = tuzduks_backup;
                break;
            }
            if (!terminal) {
                leave_repetition(exact_hash(board, kazans, tuzduks, next_to_play));
            }

            double value = -child_value;

            board = board_backup;
            kazans = kazans_backup;
            tuzduks = tuzduks_backup;

            if (value <= -WIN_SCORE) {
                stats.pruned_losing_moves++;
            }

            if (value >= WIN_SCORE) {
                stats.forced_wins++;
                best = value;
                alpha = std::max(alpha, value);
                break;
            }

            if (value > best) best = value;
            if (value > alpha) alpha = value;

            if (alpha >= beta) {
                stats.beta_cuts++;
                break;
            }
        }

        Bound bound = Bound::Exact;
        if (best <= alpha_original) {
            bound = Bound::Upper;
        } else if (best >= beta) {
            bound = Bound::Lower;
        }
        if (use_tt) {
            table[key] = TTEntry{best, depth, bound};
            stats.tt_stores++;
            stats.table_entries = static_cast<uint64_t>(table.size());
        }
        return best;
    }

    double qsearch(Bitboard& board, std::array<int, 2>& kazans,
                   std::array<int, 2>& tuzduks, int to_play, int steps,
                   int max_steps, int winner_code, double alpha, double beta,
                   int q_depth) {
        if (deadline_reached()) {
            return evaluate_leaf(board, kazans, tuzduks, to_play, winner_code,
                                 winner_code != -2, evaluator);
        }
        stats.nodes++;

        if (winner_code != -2) {
            stats.terminal_hits++;
            return terminal_value(winner_code, to_play);
        }

        double stand_pat = evaluate_leaf(board, kazans, tuzduks, to_play,
                                         winner_code, false, evaluator);
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

        sort_moves(board, tuzduks, to_play, moves, count);

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

            ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play,
                                  next_steps, max_steps, next_winner,
                                  next_to_play, terminal, ignored_hash);

            bool tactical = terminal ||
                is_tactical_transition(kazans_backup, tuzduks_backup,
                                       kazans, tuzduks, to_play);

            double value = -SEARCH_INF;
            if (tactical) {
                double child_value;
                if (terminal) {
                    stats.terminal_hits++;
                    child_value = terminal_value(next_winner, next_to_play);
                } else if (enter_repetition(exact_hash(board, kazans, tuzduks,
                                                       next_to_play))) {
                    child_value = 0.0;
                } else {
                    child_value = qsearch(board, kazans, tuzduks, next_to_play,
                                          next_steps, max_steps, next_winner,
                                          -beta, -alpha, q_depth - 1);
                }
                if (timed_out && *timed_out) {
                    board = board_backup;
                    kazans = kazans_backup;
                    tuzduks = tuzduks_backup;
                    break;
                }
                if (!terminal) {
                    leave_repetition(exact_hash(board, kazans, tuzduks,
                                                next_to_play));
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

    std::unordered_map<NormalizedKey, TTEntry, KeyHash> table;
    const Evaluator& evaluator;
    const Clock::time_point* deadline;
    bool* timed_out;
    std::unordered_map<uint64_t, int> repetitions;
    SearchStats stats;

    bool deadline_reached() {
        if (!deadline || !timed_out) return false;
        if (*timed_out) return true;
        if (Clock::now() < *deadline) return false;
        *timed_out = true;
        return true;
    }

    uint64_t exact_hash(const Bitboard& board, const std::array<int, 2>& kazans,
                        const std::array<int, 2>& tuzduks, int to_play) const {
        return Zobrist::get_initial_hash_bitboard(board, kazans, tuzduks, to_play);
    }

    bool enter_repetition(uint64_t hash) {
        int& count = repetitions[hash];
        count++;
        if (count >= 3) {
            stats.repetition_draws++;
            return true;
        }
        return false;
    }

    void leave_repetition(uint64_t hash) {
        auto found = repetitions.find(hash);
        if (found == repetitions.end()) return;
        found->second--;
        if (found->second <= 0) repetitions.erase(found);
    }

    bool has_repetition_pressure() const {
        for (const auto& item : repetitions) {
            if (item.second > 1) return true;
        }
        return false;
    }
};

SearchStats last_stats;

size_t reserve_hint_for_depth(int depth) {
    size_t value = 256;
    for (int i = 0; i < depth && value < (1u << 20); ++i) {
        value *= 4;
    }
    return value;
}

MoveEval search_root(const ToguzEnv& env, int depth, const Evaluator& evaluator,
                     const Clock::time_point* deadline = nullptr,
                     bool* timed_out = nullptr) {
    MoveEval result;
    if (depth < 1) return result;

    Bitboard board = env.board;
    auto kazans = env.kazans;
    auto tuzduks = env.tuzduks;

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
    if (count == 0) return result;

    sort_moves(board, tuzduks, env.to_play, moves, count);

    DagSearcher searcher(reserve_hint_for_depth(depth), env.repetition_counts,
                         evaluator, deadline, timed_out);
    int best_move = moves[0];
    double best_value = -SEARCH_INF;
    double alpha = -SEARCH_INF;
    double beta = SEARCH_INF;

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

        ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play,
                              next_steps, env.max_steps, next_winner,
                              next_to_play, terminal, ignored_hash);

        double child_value;
        if (terminal) {
            searcher.stats.terminal_hits++;
            child_value = terminal_value(next_winner, next_to_play);
        } else if (searcher.enter_repetition(searcher.exact_hash(
                       b_tmp, k_tmp, t_tmp, next_to_play))) {
            child_value = 0.0;
        } else {
            child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play,
                                          next_steps, env.max_steps,
                                          next_winner, depth - 1,
                                          -beta, -alpha);
        }
        if (timed_out && *timed_out) break;
        if (!terminal) {
            searcher.leave_repetition(searcher.exact_hash(
                b_tmp, k_tmp, t_tmp, next_to_play));
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
    }

    searcher.stats.table_entries = static_cast<uint64_t>(searcher.table.size());
    last_stats = searcher.stats;
    result.move = best_move;
    result.eval = best_value;
    result.immediate_win = best_value >= WIN_SCORE;
    result.forced_loss = best_value <= -WIN_SCORE;
    return result;
}

} // namespace

int get_best_move(const ToguzEnv& env, int depth) {
    static const HeuristicEvaluator heuristic;
    return get_best_move(env, depth, heuristic);
}

int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator) {
    return search_root(env, depth, evaluator).move;
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth) {
    static const HeuristicEvaluator heuristic;
    return get_best_move_timed(env, seconds_per_move, max_depth, heuristic);
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth, const Evaluator& evaluator) {
    TimedMoveResult best;
    std::array<int, 9> moves;
    auto board = env.board;
    auto tuzduks = env.tuzduks;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
    if (count <= 0 || max_depth < 1) return best;
    sort_moves(board, tuzduks, env.to_play, moves, count);
    best.move = moves[0];

    seconds_per_move = std::clamp(seconds_per_move, 0.001, 30.0);
    auto budget = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(seconds_per_move));
    Clock::time_point deadline = Clock::now() + budget;

    for (int depth = 1; depth <= max_depth; ++depth) {
        bool timed_out = false;
        MoveEval current = search_root(env, depth, evaluator, &deadline, &timed_out);
        if (timed_out || current.move < 0) {
            best.timed_out = true;
            break;
        }
        best.move = current.move;
        best.eval = current.eval;
        best.completed_depth = depth;
        best.timed_out = false;
        if (Clock::now() >= deadline) {
            best.timed_out = true;
            break;
        }
    }
    return best;
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth) {
    static const HeuristicEvaluator heuristic;
    return get_all_moves_with_evals(env, depth, heuristic);
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth, const Evaluator& evaluator) {
    if (depth < 1) return {};

    Bitboard board = env.board;
    auto kazans = env.kazans;
    auto tuzduks = env.tuzduks;

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, env.to_play, moves);
    if (count == 0) return {};

    sort_moves(board, tuzduks, env.to_play, moves, count);

    DagSearcher searcher(reserve_hint_for_depth(depth), env.repetition_counts,
                         evaluator);
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

        ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play,
                              next_steps, env.max_steps, next_winner,
                              next_to_play, terminal, ignored_hash);

        double child_value;
        if (terminal) {
            searcher.stats.terminal_hits++;
            child_value = terminal_value(next_winner, next_to_play);
        } else if (searcher.enter_repetition(searcher.exact_hash(
                       b_tmp, k_tmp, t_tmp, next_to_play))) {
            child_value = 0.0;
        } else {
            child_value = searcher.search(b_tmp, k_tmp, t_tmp, next_to_play,
                                          next_steps, env.max_steps,
                                          next_winner, depth - 1,
                                          -beta, -alpha);
        }
        if (!terminal) {
            searcher.leave_repetition(searcher.exact_hash(
                b_tmp, k_tmp, t_tmp, next_to_play));
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

    searcher.stats.table_entries = static_cast<uint64_t>(searcher.table.size());
    last_stats = searcher.stats;
    return results;
}

SearchStats get_last_stats() {
    return last_stats;
}

} // namespace dag_search
