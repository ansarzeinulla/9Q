#include "togyz/position_hash.hpp"
#include "togyz/togyzkumalak_rules.hpp"
#include <gtest/gtest.h>
#include <array>

TEST(ZobristHashTest, DeterminismAndInitialization) {
    // Initialize Zobrist hashing keys
    Zobrist::init();

    Bitboard b1;
    for (int i = 0; i < 18; ++i) {
        b1.set(i, 9);
    }
    std::array<int, 2> k1 = {0, 0};
    std::array<int, 2> t1 = {-1, -1};
    int turn1 = PLAYER_1;

    uint64_t hash1 = Zobrist::get_initial_hash_bitboard(b1, k1, t1, turn1);
    uint64_t hash2 = Zobrist::get_initial_hash_bitboard(b1, k1, t1, turn1);

    // Identical positions must produce identical hashes
    EXPECT_EQ(hash1, hash2);

    // Modifying turn must change the hash
    uint64_t hash_turn2 = Zobrist::get_initial_hash_bitboard(b1, k1, t1, PLAYER_2);
    EXPECT_NE(hash1, hash_turn2);

    // Modifying kazan must change the hash
    std::array<int, 2> k2 = {1, 0};
    uint64_t hash_kazan = Zobrist::get_initial_hash_bitboard(b1, k2, t1, turn1);
    EXPECT_NE(hash1, hash_kazan);

    // Modifying board stones must change the hash
    Bitboard b2 = b1;
    b2.set(0, 8);
    uint64_t hash_board = Zobrist::get_initial_hash_bitboard(b2, k1, t1, turn1);
    EXPECT_NE(hash1, hash_board);
}
