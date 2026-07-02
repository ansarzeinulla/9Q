#include "togyz/togyzkumalak_rules.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

uint64_t perft(int depth, ToguzEnv& env) {
    if (depth == 0) return 1;

    std::vector<int> moves = env.generate_moves();
    if (depth == 1) return static_cast<uint64_t>(moves.size());

    uint64_t nodes = 0;
    for (int move : moves) {
        ToguzEnv child = env;
        child.step(move);
        nodes += perft(depth - 1, child);
    }
    return nodes;
}

void run_test_suite() {
    ToguzEnv env;
    std::cout << "Running Perft Correctness Suite...\n";

    // Baseline counts for the current starting position.
    const uint64_t expected[] = {0, 9, 73, 613, 5199};

    for (int depth = 1; depth <= 4; ++depth) {
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t result = perft(depth, env);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "Depth " << depth << ": Nodes = " << result
                  << " (Expected: " << expected[depth] << ") | Time: "
                  << ms << " ms\n";

        if (result != expected[depth]) {
            std::cerr << "CRITICAL ERROR: Move generation mismatch at depth "
                      << depth << "\n";
            std::exit(1);
        }
    }

    std::cout << "All Perft checks passed successfully.\n";
}

} // namespace

int main() {
    run_test_suite();
    return 0;
}
