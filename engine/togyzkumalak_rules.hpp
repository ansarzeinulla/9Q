#ifndef TOGYZKUMALAK_RULES_HPP
#define TOGYZKUMALAK_RULES_HPP

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <random>
#include <cstdint>
#include <unordered_map>

typedef __int128_t uint128;

constexpr int NUM_PITS = 9;
constexpr int BOARD_PITS = NUM_PITS * 2;
constexpr int INITIAL_STONES = 9;
constexpr int TOTAL_STONES = 162;
constexpr int WIN_THRESHOLD = 82;

constexpr int PLAYER_1 = 0;
constexpr int PLAYER_2 = 1;

constexpr int landing_pit(int start, int stones) {
    return (stones == 1) ? (start + 1) % BOARD_PITS
                         : (start + stones - 1) % BOARD_PITS;
}

struct Bitboard {
    uint128 side1 = 0; // P1 pits (0-8), 8 bits each
    uint128 side2 = 0; // P2 pits (0-8), 8 bits each

    inline int get(int pit) const {
        if (pit < 9) return (int)((side1 >> (pit * 8)) & 0xFFU);
        else return (int)((side2 >> ((pit - 9) * 8)) & 0xFFU);
    }

    inline void set(int pit, int stones) {
        if (pit < 9) {
            side1 &= ~(uint128(0xFF) << (pit * 8));
            side1 |= (uint128(stones & 0xFF) << (pit * 8));
        } else {
            side2 &= ~(uint128(0xFF) << ((pit - 9) * 8));
            side2 |= (uint128(stones & 0xFF) << ((pit - 9) * 8));
        }
    }

    inline void add(int pit, int stones) {
        if (pit < 9) side1 += (uint128(stones) << (pit * 8));
        else side2 += (uint128(stones) << ((pit - 9) * 8));
    }
};

class ToguzEnv {
public:
    Bitboard board;
    std::array<int, 2> kazans;
    std::array<int, 2> tuzduks;
    
    int to_play;
    int steps;
    int winner_code;
    int max_steps;
    std::unordered_map<uint64_t, int> repetition_counts;

    std::vector<int> random_buffer;
    int random_idx;
    
    ToguzEnv();
    void reset();
    void step(int action);
    void update_legal_actions(std::array<int, 9>& actions);
    std::vector<int> generate_moves();
    int get_random_move();
    int setup_random_position(int plies = 15);
    int setup_balanced_reduced_position(std::mt19937_64& rng, int max_reduction_per_pit = 3,
                                        int max_total_reduction = 18);
    bool is_game_over() const;
    void determine_winner_by_kazans();
    std::string get_result_string() const;
    void render(const std::string& mode, const std::string& p1_name = "P1", const std::string& p2_name = "P2") const;
    uint64_t position_hash() const;
    void reset_repetition_history();
    bool record_repetition_and_check_draw();
    int current_repetition_count() const;
    
    static double evaluate(const Bitboard& b, const std::array<int, 2>& k, const std::array<int, 2>& t,
                          int perspective, int to_play_idx, int w_code, bool terminal);
                           
    static void step_search(Bitboard& b, std::array<int, 2>& k, std::array<int, 2>& t,
                            int action, int to_play_idx, int& steps_val, int max_s,
                            int& out_winner_code, int& out_next_to_play, bool& out_is_terminal,
                            uint64_t& current_hash);
    
    static int generate_moves_search(const Bitboard& b, const std::array<int, 2>& t, int player, std::array<int, 9>& out_moves);
};

#endif
