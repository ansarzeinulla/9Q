#include "minimax_engine.hpp"
#include "position_hash.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <random>
#include <vector>

namespace minimax_engine {

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------
// Transposition Table (Optimized Fixed-Size Vector)
// ---------------------------------------------------------
#ifndef MINIMAX_TT_BITS
#ifdef __EMSCRIPTEN__
#define MINIMAX_TT_BITS 20
#else
#define MINIMAX_TT_BITS 24
#endif
#endif

const int TT_SIZE = 1 << MINIMAX_TT_BITS; // Native default: 16.7M entries.
struct TTEntry {
    uint64_t hash = 0;
    uint64_t evaluator_key = 0;
    double value = 0.0;
    int depth = -1;
    int perspective = -1;
};

static std::vector<TTEntry>& transposition_table() {
    static std::vector<TTEntry> table(TT_SIZE);
    return table;
}

bool deadline_reached(const Clock::time_point& deadline) {
    return Clock::now() >= deadline;
}

double evaluate_leaf(const Bitboard& board, const std::array<int, 2>& kazans,
                     const std::array<int, 2>& tuzduks, int to_play_idx,
                     int winner_code, bool terminal, int perspective_player,
                     const Evaluator& evaluator) {
    return evaluator.evaluate_position(BoardState{board, kazans, tuzduks,
                                                  to_play_idx, winner_code,
                                                  perspective_player, terminal});
}

// ---------------------------------------------------------
// Move Ordering (Tactical Heuristics)
// ---------------------------------------------------------
int move_order_key(const Bitboard& b, int player, int move) {
    int opponent = 1 - player;
    int start = player * NUM_PITS + move;
    int stones = b.get(start);
    if (stones <= 0) return 100;
    int target = landing_pit(start, stones);
    
    // (1) Moves that capture stones (Highest Priority)
    if (target / 9 == opponent) {
        int next_val = b.get(target) + 1;
        if (next_val % 2 == 0) return 0; // Capture
        if (next_val == 3) return 1;     // Tuzdyk
    }

    // (3) Moves that move into empty pits
    // If the target pit currently has 0 stones
    if (b.get(target) == 0) return 2;

    return 3; // Quiet move
}

void sort_moves_in_place(const Bitboard& b, int player, std::array<int, 9>& moves, int count) {
    // Selection sort for small array (9 elements)
    for (int i = 0; i < count - 1; ++i) {
        int best_idx = i;
        int best_val = move_order_key(b, player, moves[i]);
        for (int j = i + 1; j < count; ++j) {
            int val = move_order_key(b, player, moves[j]);
            if (val < best_val) {
                best_val = val;
                best_idx = j;
            }
        }
        std::swap(moves[i], moves[best_idx]);
    }
}

// ---------------------------------------------------------
// Quiescence Search
// ---------------------------------------------------------
double qsearch(Bitboard& board, std::array<int, 2>& kazans, std::array<int, 2>& tuzduks,
               int to_play_idx, int steps, int max_steps, int winner_code,
               double alpha, double beta, bool maximizing_player,
               int perspective_player, int q_depth, const Evaluator& evaluator) {
    if (winner_code != -2) {
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             true, perspective_player, evaluator);
    }

    double stand_pat = evaluate_leaf(board, kazans, tuzduks, to_play_idx,
                                     winner_code, false, perspective_player,
                                     evaluator);
    if (maximizing_player) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    } else {
        if (stand_pat <= alpha) return alpha;
        if (stand_pat < beta) beta = stand_pat;
    }

    if (q_depth == 0) return stand_pat;

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play_idx, moves);
    
    std::array<int, 9> tactical;
    int tac_count = 0;
    for (int i=0; i<count; ++i) {
        if (move_order_key(board, to_play_idx, moves[i]) <= 1) tactical[tac_count++] = moves[i];
    }
    
    if (tac_count == 0) return stand_pat;
    sort_moves_in_place(board, to_play_idx, tactical, tac_count);

    Bitboard b_bak = board;
    auto k_bak = kazans; auto t_bak = tuzduks;
    int ot = to_play_idx, os = steps, ow = winner_code;

    if (maximizing_player) {
        double value = -10000000.0;
        for (int i=0; i<tac_count; ++i) {
            int nw, np; bool term; int ns = steps; uint64_t dummy = 0;
            ToguzEnv::step_search(board, kazans, tuzduks, tactical[i], to_play_idx, ns, max_steps, nw, np, term, dummy);
            double res = qsearch(board, kazans, tuzduks, np, ns, max_steps, nw, alpha, beta, false, perspective_player, q_depth - 1, evaluator);
            board = b_bak; kazans = k_bak; tuzduks = t_bak; to_play_idx = ot; steps = os; winner_code = ow;
            if (res > value) value = res;
            if (value > alpha) alpha = value;
            if (beta <= alpha) break;
        }
        return value;
    } else {
        double value = 10000000.0;
        for (int i=0; i<tac_count; ++i) {
            int nw, np; bool term; int ns = steps; uint64_t dummy = 0;
            ToguzEnv::step_search(board, kazans, tuzduks, tactical[i], to_play_idx, ns, max_steps, nw, np, term, dummy);
            double res = qsearch(board, kazans, tuzduks, np, ns, max_steps, nw, alpha, beta, true, perspective_player, q_depth - 1, evaluator);
            board = b_bak; kazans = k_bak; tuzduks = t_bak; to_play_idx = ot; steps = os; winner_code = ow;
            if (res < value) value = res;
            if (value < beta) beta = value;
            if (beta <= alpha) break;
        }
        return value;
    }
}

// ---------------------------------------------------------
// Main Minimax Engine
// ---------------------------------------------------------
double minimax_raw(Bitboard& board, std::array<int, 2>& kazans, std::array<int, 2>& tuzduks,
                   int to_play_idx, int steps, int max_steps, int winner_code,
                   int depth, double alpha, double beta, bool maximizing_player, int perspective_player,
                   std::vector<Bitboard>& history_board,
                   std::vector<std::array<int, 2>>& history_kazans,
                   std::vector<std::array<int, 2>>& history_tuzduks,
                   std::vector<std::array<int, 9>>& history_moves,
                   uint64_t current_hash,
                   const Evaluator& evaluator) {
    if (winner_code != -2) {
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             true, perspective_player, evaluator);
    }

    // TT Probe
    uint64_t evaluator_key = evaluator.cache_key();
    int tt_idx = current_hash & (TT_SIZE - 1);
    auto& tt = transposition_table();
    if (tt[tt_idx].hash == current_hash &&
        tt[tt_idx].evaluator_key == evaluator_key &&
        tt[tt_idx].perspective == perspective_player &&
        tt[tt_idx].depth >= depth) {
        return tt[tt_idx].value;
    }

    if (depth == 0) {
        return qsearch(board, kazans, tuzduks, to_play_idx, steps, max_steps, winner_code, alpha, beta, maximizing_player, perspective_player, 6, evaluator);
    }

    auto& moves = history_moves[depth];
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play_idx, moves);
    if (count == 0) return evaluate_leaf(board, kazans, tuzduks, to_play_idx,
                                         winner_code, false, perspective_player,
                                         evaluator);
    
    // Sort moves to improve Alpha-Beta pruning
    if (depth >= 1) sort_moves_in_place(board, to_play_idx, moves, count);

    history_board[depth] = board;
    history_kazans[depth] = kazans;
    history_tuzduks[depth] = tuzduks;
    int ot = to_play_idx, os = steps, ow = winner_code;

    if (maximizing_player) {
        double value = -10000000.0;
        for (int i=0; i<count; ++i) {
            int nw, np; bool term; int ns = steps; uint64_t next_h = current_hash;
            ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play_idx, ns, max_steps, nw, np, term, next_h);
            double res = minimax_raw(board, kazans, tuzduks, np, ns, max_steps, nw, depth - 1, alpha, beta, false, perspective_player, history_board, history_kazans, history_tuzduks, history_moves, next_h, evaluator);
            board = history_board[depth]; kazans = history_kazans[depth]; tuzduks = history_tuzduks[depth]; to_play_idx = ot; steps = os; winner_code = ow;
            if (res > value) value = res;
            if (value > alpha) alpha = value;
            if (beta <= alpha) break;
        }
        tt[tt_idx] = {current_hash, evaluator_key, value, depth, perspective_player};
        return value;
    } else {
        double value = 10000000.0;
        for (int i=0; i<count; ++i) {
            int nw, np; bool term; int ns = steps; uint64_t next_h = current_hash;
            ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play_idx, ns, max_steps, nw, np, term, next_h);
            double res = minimax_raw(board, kazans, tuzduks, np, ns, max_steps, nw, depth - 1, alpha, beta, true, perspective_player, history_board, history_kazans, history_tuzduks, history_moves, next_h, evaluator);
            board = history_board[depth]; kazans = history_kazans[depth]; tuzduks = history_tuzduks[depth]; to_play_idx = ot; steps = os; winner_code = ow;
            if (res < value) value = res;
            if (value < beta) beta = value;
            if (beta <= alpha) break;
        }
        tt[tt_idx] = {current_hash, evaluator_key, value, depth, perspective_player};
        return value;
    }
}

double qsearch_timed(Bitboard& board, std::array<int, 2>& kazans,
                     std::array<int, 2>& tuzduks, int to_play_idx, int steps,
                     int max_steps, int winner_code, double alpha, double beta,
                     bool maximizing_player, int perspective_player,
                     int q_depth, const Evaluator& evaluator,
                     const Clock::time_point& deadline, bool& timed_out) {
    if (timed_out || deadline_reached(deadline)) {
        timed_out = true;
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             winner_code != -2, perspective_player, evaluator);
    }

    if (winner_code != -2) {
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             true, perspective_player, evaluator);
    }

    double stand_pat = evaluate_leaf(board, kazans, tuzduks, to_play_idx,
                                     winner_code, false, perspective_player,
                                     evaluator);
    if (maximizing_player) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    } else {
        if (stand_pat <= alpha) return alpha;
        if (stand_pat < beta) beta = stand_pat;
    }

    if (q_depth == 0) return stand_pat;

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play_idx, moves);

    std::array<int, 9> tactical;
    int tac_count = 0;
    for (int i = 0; i < count; ++i) {
        if (move_order_key(board, to_play_idx, moves[i]) <= 1)
            tactical[tac_count++] = moves[i];
    }

    if (tac_count == 0) return stand_pat;
    sort_moves_in_place(board, to_play_idx, tactical, tac_count);

    Bitboard b_bak = board;
    auto k_bak = kazans;
    auto t_bak = tuzduks;
    int ot = to_play_idx, os = steps, ow = winner_code;

    if (maximizing_player) {
        double value = -10000000.0;
        for (int i = 0; i < tac_count; ++i) {
            if (deadline_reached(deadline)) {
                timed_out = true;
                break;
            }
            int nw, np;
            bool term;
            int ns = steps;
            uint64_t dummy = 0;
            ToguzEnv::step_search(board, kazans, tuzduks, tactical[i],
                                  to_play_idx, ns, max_steps, nw, np, term,
                                  dummy);
            double res = qsearch_timed(board, kazans, tuzduks, np, ns,
                                       max_steps, nw, alpha, beta, false,
                                       perspective_player, q_depth - 1,
                                       evaluator, deadline, timed_out);
            board = b_bak;
            kazans = k_bak;
            tuzduks = t_bak;
            to_play_idx = ot;
            steps = os;
            winner_code = ow;
            if (timed_out) break;
            if (res > value) value = res;
            if (value > alpha) alpha = value;
            if (beta <= alpha) break;
        }
        return value == -10000000.0 ? stand_pat : value;
    }

    double value = 10000000.0;
    for (int i = 0; i < tac_count; ++i) {
        if (deadline_reached(deadline)) {
            timed_out = true;
            break;
        }
        int nw, np;
        bool term;
        int ns = steps;
        uint64_t dummy = 0;
        ToguzEnv::step_search(board, kazans, tuzduks, tactical[i],
                              to_play_idx, ns, max_steps, nw, np, term,
                              dummy);
        double res = qsearch_timed(board, kazans, tuzduks, np, ns, max_steps,
                                   nw, alpha, beta, true, perspective_player,
                                   q_depth - 1, evaluator, deadline,
                                   timed_out);
        board = b_bak;
        kazans = k_bak;
        tuzduks = t_bak;
        to_play_idx = ot;
        steps = os;
        winner_code = ow;
        if (timed_out) break;
        if (res < value) value = res;
        if (value < beta) beta = value;
        if (beta <= alpha) break;
    }
    return value == 10000000.0 ? stand_pat : value;
}

double minimax_raw_timed(Bitboard& board, std::array<int, 2>& kazans,
                         std::array<int, 2>& tuzduks, int to_play_idx,
                         int steps, int max_steps, int winner_code, int depth,
                         double alpha, double beta, bool maximizing_player,
                         int perspective_player,
                         std::vector<Bitboard>& history_board,
                         std::vector<std::array<int, 2>>& history_kazans,
                         std::vector<std::array<int, 2>>& history_tuzduks,
                         std::vector<std::array<int, 9>>& history_moves,
                         uint64_t current_hash, const Evaluator& evaluator,
                         const Clock::time_point& deadline, bool& timed_out) {
    if (timed_out || deadline_reached(deadline)) {
        timed_out = true;
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             winner_code != -2, perspective_player, evaluator);
    }

    if (winner_code != -2) {
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             true, perspective_player, evaluator);
    }

    uint64_t evaluator_key = evaluator.cache_key();
    int tt_idx = current_hash & (TT_SIZE - 1);
    auto& tt = transposition_table();
    if (tt[tt_idx].hash == current_hash &&
        tt[tt_idx].evaluator_key == evaluator_key &&
        tt[tt_idx].perspective == perspective_player &&
        tt[tt_idx].depth >= depth) {
        return tt[tt_idx].value;
    }

    if (depth == 0) {
        return qsearch_timed(board, kazans, tuzduks, to_play_idx, steps,
                             max_steps, winner_code, alpha, beta,
                             maximizing_player, perspective_player, 6,
                             evaluator, deadline, timed_out);
    }

    auto& moves = history_moves[depth];
    int count = ToguzEnv::generate_moves_search(board, tuzduks, to_play_idx, moves);
    if (count == 0) {
        return evaluate_leaf(board, kazans, tuzduks, to_play_idx, winner_code,
                             false, perspective_player, evaluator);
    }

    sort_moves_in_place(board, to_play_idx, moves, count);

    history_board[depth] = board;
    history_kazans[depth] = kazans;
    history_tuzduks[depth] = tuzduks;
    int ot = to_play_idx, os = steps, ow = winner_code;

    if (maximizing_player) {
        double value = -10000000.0;
        for (int i = 0; i < count; ++i) {
            if (deadline_reached(deadline)) {
                timed_out = true;
                break;
            }
            int nw, np;
            bool term;
            int ns = steps;
            uint64_t next_h = current_hash;
            ToguzEnv::step_search(board, kazans, tuzduks, moves[i],
                                  to_play_idx, ns, max_steps, nw, np, term,
                                  next_h);
            double res = minimax_raw_timed(
                board, kazans, tuzduks, np, ns, max_steps, nw, depth - 1,
                alpha, beta, false, perspective_player, history_board,
                history_kazans, history_tuzduks, history_moves, next_h,
                evaluator, deadline, timed_out);
            board = history_board[depth];
            kazans = history_kazans[depth];
            tuzduks = history_tuzduks[depth];
            to_play_idx = ot;
            steps = os;
            winner_code = ow;
            if (timed_out) break;
            if (res > value) value = res;
            if (value > alpha) alpha = value;
            if (beta <= alpha) break;
        }
        if (!timed_out) {
            tt[tt_idx] = {current_hash, evaluator_key, value, depth,
                          perspective_player};
        }
        return value;
    }

    double value = 10000000.0;
    for (int i = 0; i < count; ++i) {
        if (deadline_reached(deadline)) {
            timed_out = true;
            break;
        }
        int nw, np;
        bool term;
        int ns = steps;
        uint64_t next_h = current_hash;
        ToguzEnv::step_search(board, kazans, tuzduks, moves[i], to_play_idx,
                              ns, max_steps, nw, np, term, next_h);
        double res = minimax_raw_timed(
            board, kazans, tuzduks, np, ns, max_steps, nw, depth - 1, alpha,
            beta, true, perspective_player, history_board, history_kazans,
            history_tuzduks, history_moves, next_h, evaluator, deadline,
            timed_out);
        board = history_board[depth];
        kazans = history_kazans[depth];
        tuzduks = history_tuzduks[depth];
        to_play_idx = ot;
        steps = os;
        winner_code = ow;
        if (timed_out) break;
        if (res < value) value = res;
        if (value < beta) beta = value;
        if (beta <= alpha) break;
    }
    if (!timed_out) {
        tt[tt_idx] = {current_hash, evaluator_key, value, depth,
                      perspective_player};
    }
    return value;
}

TimedMoveResult search_depth_timed(const ToguzEnv& env, int depth,
                                   const Evaluator& evaluator,
                                   const Clock::time_point& deadline) {
    TimedMoveResult result;
    result.completed_depth = depth;

    Bitboard b = env.board;
    auto k = env.kazans;
    auto t = env.tuzduks;
    int perspective = env.to_play;
    uint64_t hash = Zobrist::get_initial_hash_bitboard(b, k, t, env.to_play);

    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(b, t, env.to_play, moves);
    if (count == 0) return result;
    sort_moves_in_place(b, env.to_play, moves, count);

    result.move = moves[0];
    result.eval = -10000000.0;

    std::vector<Bitboard> h_b(depth + 1);
    std::vector<std::array<int, 2>> h_k(depth + 1);
    std::vector<std::array<int, 2>> h_t(depth + 1);
    std::vector<std::array<int, 9>> h_m(depth + 1);

    double alpha = -10000000.0;
    double beta = 10000000.0;
    bool timed_out = false;

    for (int i = 0; i < count; ++i) {
        if (deadline_reached(deadline)) {
            timed_out = true;
            break;
        }
        Bitboard b_tmp = b;
        auto k_tmp = k;
        auto t_tmp = t;
        int nw, np;
        bool term;
        int ns = env.steps;
        uint64_t next_h = hash;
        ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play, ns,
                              env.max_steps, nw, np, term, next_h);
        double res = minimax_raw_timed(
            b_tmp, k_tmp, t_tmp, np, ns, env.max_steps, nw, depth - 1, alpha,
            beta, false, perspective, h_b, h_k, h_t, h_m, next_h, evaluator,
            deadline, timed_out);
        if (timed_out) break;
        if (res > result.eval) {
            result.eval = res;
            result.move = moves[i];
        }
        if (result.eval > alpha) alpha = result.eval;
        if (result.eval >= 1000000.0) break;
    }

    result.timed_out = timed_out;
    return result;
}

int get_best_move(const ToguzEnv& env, int depth) {
    static const HeuristicEvaluator heuristic;
    return get_best_move(env, depth, heuristic);
}

int get_best_move(const ToguzEnv& env, int depth, const Evaluator& evaluator) {
    if (depth < 1) return -1;
    Bitboard b = env.board;
    auto k = env.kazans;
    auto t = env.tuzduks;
    int perspective = env.to_play;

    uint64_t hash = Zobrist::get_initial_hash_bitboard(b, k, t, env.to_play);
    
    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(b, t, env.to_play, moves);
    if (count == 0) return -1;
    sort_moves_in_place(b, env.to_play, moves, count);

    int best_move = moves[0];
    double best_val = -10000000.0;
    double alpha = -10000000.0;
    double beta = 10000000.0;

    std::vector<Bitboard> h_b(depth + 1);
    std::vector<std::array<int, 2>> h_k(depth + 1);
    std::vector<std::array<int, 2>> h_t(depth + 1);
    std::vector<std::array<int, 9>> h_m(depth + 1);

    for (int i=0; i<count; ++i) {
        Bitboard b_tmp = b; auto k_tmp = k; auto t_tmp = t;
        int nw, np; bool term; int ns = env.steps; uint64_t next_h = hash;
        ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play, ns, env.max_steps, nw, np, term, next_h);
        double res = minimax_raw(b_tmp, k_tmp, t_tmp, np, ns, env.max_steps, nw, depth - 1, alpha, beta, false, perspective, h_b, h_k, h_t, h_m, next_h, evaluator);
        if (res > best_val) { best_val = res; best_move = moves[i]; }
        if (best_val > alpha) alpha = best_val;
        if (best_val >= 1000000.0) break; // Stop searching root moves if a win is guaranteed
    }
    return best_move;
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth) {
    static const HeuristicEvaluator heuristic;
    return get_best_move_timed(env, seconds_per_move, max_depth, heuristic);
}

TimedMoveResult get_best_move_timed(const ToguzEnv& env, double seconds_per_move,
                                    int max_depth,
                                    const Evaluator& evaluator) {
    TimedMoveResult best;
    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(env.board, env.tuzduks,
                                                env.to_play, moves);
    if (count == 0) return best;

    sort_moves_in_place(env.board, env.to_play, moves, count);
    best.move = moves[0];
    best.timed_out = true;

    if (max_depth < 1) max_depth = 1;
    if (seconds_per_move <= 0.0) seconds_per_move = 0.000001;

    auto budget = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(seconds_per_move));
    Clock::time_point deadline = Clock::now() + budget;

    for (int depth = 1; depth <= max_depth; ++depth) {
        if (deadline_reached(deadline)) {
            best.timed_out = true;
            break;
        }

        TimedMoveResult current =
            search_depth_timed(env, depth, evaluator, deadline);
        if (current.timed_out || current.move == -1) {
            best.timed_out = true;
            break;
        }

        best = current;
        best.timed_out = false;
        if (current.eval >= 1000000.0) break;
    }

    return best;
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth) {
    static const HeuristicEvaluator heuristic;
    return get_all_moves_with_evals(env, depth, heuristic);
}

std::vector<MoveEval> get_all_moves_with_evals(const ToguzEnv& env, int depth, const Evaluator& evaluator) {
    if (depth < 1) return {};
    Bitboard b = env.board;
    auto k = env.kazans;
    auto t = env.tuzduks;
    int perspective = env.to_play;

    uint64_t hash = Zobrist::get_initial_hash_bitboard(b, k, t, env.to_play);
    
    std::array<int, 9> moves;
    int count = ToguzEnv::generate_moves_search(b, t, env.to_play, moves);
    if (count == 0) return {};
    sort_moves_in_place(b, env.to_play, moves, count);

    std::vector<MoveEval> results;
    double alpha = -10000000.0;
    double beta = 10000000.0;

    std::vector<Bitboard> h_b(depth + 1);
    std::vector<std::array<int, 2>> h_k(depth + 1);
    std::vector<std::array<int, 2>> h_t(depth + 1);
    std::vector<std::array<int, 9>> h_m(depth + 1);

    bool win_found = false;
    for (int i=0; i<count; ++i) {
        if (win_found) {
            results.push_back({moves[i], 0.0, true});
            continue;
        }
        Bitboard b_tmp = b; auto k_tmp = k; auto t_tmp = t;
        int nw, np; bool term; int ns = env.steps; uint64_t next_h = hash;
        ToguzEnv::step_search(b_tmp, k_tmp, t_tmp, moves[i], env.to_play, ns, env.max_steps, nw, np, term, next_h);
        double res = minimax_raw(b_tmp, k_tmp, t_tmp, np, ns, env.max_steps, nw, depth - 1, alpha, beta, false, perspective, h_b, h_k, h_t, h_m, next_h, evaluator);
        results.push_back({moves[i], res, false});
        if (res >= 1000000.0) win_found = true;
    }
    return results;
}

}
