#include "../engine/position_hash.hpp"
#include "../engine/minimax_engine.hpp"
#include "../engine/togyzkumalak_rules.hpp"

#include <array>
#include <exception>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TestFailure : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::ostringstream out;                                             \
            out << __FILE__ << ":" << __LINE__ << ": check failed: "          \
                << #condition;                                                  \
            throw TestFailure(out.str());                                       \
        }                                                                       \
    } while (false)

int total_stones(const ToguzEnv& env) {
    int total = env.kazans[PLAYER_1] + env.kazans[PLAYER_2];
    for (int pit = 0; pit < BOARD_PITS; ++pit) {
        total += env.board.get(pit);
    }
    return total;
}

void require_invariants(const ToguzEnv& env) {
    CHECK(total_stones(env) == TOTAL_STONES);
    for (int player = 0; player < 2; ++player) {
        CHECK(env.kazans[player] >= 0);
        CHECK(env.kazans[player] <= TOTAL_STONES);
        CHECK(env.tuzduks[player] == -1 ||
              (env.tuzduks[player] >= 0 && env.tuzduks[player] < NUM_PITS - 1));
    }
    CHECK(env.tuzduks[PLAYER_1] == -1 || env.tuzduks[PLAYER_2] == -1 ||
          env.tuzduks[PLAYER_1] != env.tuzduks[PLAYER_2]);

    if (!env.is_game_over()) {
        std::array<int, NUM_PITS> moves{};
        int count = ToguzEnv::generate_moves_search(env.board, env.tuzduks,
                                                    env.to_play, moves);
        CHECK(count > 0);
        int opponent = 1 - env.to_play;
        for (int i = 0; i < count; ++i) {
            int move = moves[i];
            CHECK(move >= 0);
            CHECK(move < NUM_PITS);
            CHECK(env.board.get(env.to_play * NUM_PITS + move) > 0);
            CHECK(env.tuzduks[opponent] != move);
        }
    }
}

void assert_same_board(const Bitboard& lhs, const Bitboard& rhs) {
    CHECK(lhs.side1 == rhs.side1);
    CHECK(lhs.side2 == rhs.side2);
}

void assert_step_search_matches_step(const ToguzEnv& env, int move) {
    ToguzEnv expected = env;
    expected.reset_repetition_history();
    expected.step(move);

    Bitboard board = env.board;
    auto kazans = env.kazans;
    auto tuzduks = env.tuzduks;
    int steps = env.steps;
    int winner_code = -2;
    int next_to_play = -1;
    bool terminal = false;
    uint64_t hash = env.position_hash();

    ToguzEnv::step_search(board, kazans, tuzduks, move, env.to_play, steps,
                          env.max_steps, winner_code, next_to_play, terminal,
                          hash);

    assert_same_board(board, expected.board);
    CHECK(kazans == expected.kazans);
    CHECK(tuzduks == expected.tuzduks);
    CHECK(steps == expected.steps);
    CHECK(winner_code == expected.winner_code);
    CHECK(next_to_play == expected.to_play);
    CHECK(terminal == expected.is_game_over());

    uint64_t full_hash =
        Zobrist::get_initial_hash_bitboard(board, kazans, tuzduks, next_to_play);
    CHECK(hash == full_hash);
    CHECK(hash == expected.position_hash());
    require_invariants(expected);
}

void test_landing_pit_formula() {
    const std::array<int, 7> stone_counts = {1, 2, 9, 18, 19, 81, 162};
    for (int start = 0; start < BOARD_PITS; ++start) {
        for (int stones : stone_counts) {
            int expected = (stones == 1) ? (start + 1) % BOARD_PITS
                                         : (start + stones - 1) % BOARD_PITS;
            CHECK(landing_pit(start, stones) == expected);
            if (stones > 1) {
                CHECK(landing_pit(start, stones) !=
                      (start + stones) % BOARD_PITS);
            }
        }
    }
}

void test_minimax_tactical_classification() {
    Bitboard board;
    board.side1 = 0;
    board.side2 = 0;

    board.set(0, 10);
    board.set(9, 1);
    CHECK(minimax_engine::move_order_key(board, PLAYER_1, 0) == 0);

    board.side1 = 0;
    board.side2 = 0;
    board.set(0, 10);
    board.set(9, 2);
    CHECK(minimax_engine::move_order_key(board, PLAYER_1, 0) == 1);

    board.side1 = 0;
    board.side2 = 0;
    board.set(0, 10);
    CHECK(minimax_engine::move_order_key(board, PLAYER_1, 0) == 2);

    board.side1 = 0;
    board.side2 = 0;
    board.set(0, 10);
    board.set(9, 4);
    CHECK(minimax_engine::move_order_key(board, PLAYER_1, 0) == 3);
}

void test_random_playout_invariants() {
    ToguzEnv env;
    std::mt19937_64 rng(20260613);

    for (int game = 0; game < 64; ++game) {
        env.reset();
        env.max_steps = 500;
        require_invariants(env);

        for (int ply = 0; ply < env.max_steps && !env.is_game_over(); ++ply) {
            std::vector<int> moves = env.generate_moves();
            CHECK(!moves.empty());
            std::uniform_int_distribution<int> pick(
                0, static_cast<int>(moves.size()) - 1);
            env.step(moves[pick(rng)]);
            require_invariants(env);
        }
    }
}

void test_step_search_matches_step_on_reachable_positions() {
    ToguzEnv env;
    env.max_steps = 500;
    std::mt19937_64 rng(709791810521833ULL);

    for (int ply = 0; ply < 100 && !env.is_game_over(); ++ply) {
        env.reset_repetition_history();
        std::vector<int> moves = env.generate_moves();
        CHECK(!moves.empty());
        for (int move : moves) {
            assert_step_search_matches_step(env, move);
        }

        std::uniform_int_distribution<int> pick(0, static_cast<int>(moves.size()) - 1);
        env.step(moves[pick(rng)]);
        if (!env.is_game_over()) {
            env.reset_repetition_history();
        }
    }
}

void test_terminal_collection_hash_consistency() {
    ToguzEnv env;
    env.board.side1 = 0;
    env.board.side2 = 0;
    env.board.set(0, 1);
    env.kazans = {80, 81};
    env.tuzduks = {-1, -1};
    env.to_play = PLAYER_1;
    env.steps = 0;
    env.winner_code = -2;
    env.max_steps = 10000;
    env.reset_repetition_history();

    require_invariants(env);
    assert_step_search_matches_step(env, 0);
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*run)();
    };

    const TestCase tests[] = {
        {"landing_pit_formula", test_landing_pit_formula},
        {"minimax_tactical_classification",
         test_minimax_tactical_classification},
        {"random_playout_invariants", test_random_playout_invariants},
        {"step_search_matches_step_on_reachable_positions",
         test_step_search_matches_step_on_reachable_positions},
        {"terminal_collection_hash_consistency",
         test_terminal_collection_hash_consistency},
    };

    int failed = 0;
    for (const TestCase& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const std::exception& ex) {
            failed++;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << "\n";
        }
    }

    if (failed != 0) {
        std::cerr << failed << " test(s) failed.\n";
        return 1;
    }
    return 0;
}
