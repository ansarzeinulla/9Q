#ifndef POSITION_HASH_HPP
#define POSITION_HASH_HPP

#include <cstdint>
#include <array>
#include <random>
#include "togyzkumalak_rules.hpp"

// Zobrist hashing gives each position a stable 64-bit key for repetition
// detection and transposition-table lookup.
class Zobrist {
public:
    static uint64_t board_p1[9][256];
    static uint64_t board_p2[9][256];
    static uint64_t kazan[2][TOTAL_STONES + 1];
    static uint64_t tuzduk[2][10];
    static uint64_t to_play[2];

    static void init() {
        static bool initialized = false;
        if (initialized) return;
        std::mt19937_64 rng(1337);
        for (int i = 0; i < 9; ++i) {
            for (int s = 0; s < 256; ++s) {
                board_p1[i][s] = rng();
                board_p2[i][s] = rng();
            }
        }
        for (int p = 0; p < 2; ++p) {
            for (int s = 0; s <= TOTAL_STONES; ++s) kazan[p][s] = rng();
            for (int s = 0; s < 10; ++s) tuzduk[p][s] = rng();
            to_play[p] = rng();
        }
        initialized = true;
    }

    static uint64_t get_initial_hash_bitboard(const Bitboard& b, const std::array<int, 2>& k, const std::array<int, 2>& t, int turn) {
        uint64_t h = 0;
        for (int i = 0; i < 9; ++i) {
            h ^= board_p1[i][b.get(i)];
            h ^= board_p2[i][b.get(i + 9)];
        }
        for (int p = 0; p < 2; ++p) {
            h ^= kazan[p][k[p]];
            h ^= tuzduk[p][t[p] + 1];
        }
        h ^= to_play[turn];
        return h;
    }
};

#endif
