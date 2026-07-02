#include "togyz/position_hash.hpp"

uint64_t Zobrist::board_p1[9][256];
uint64_t Zobrist::board_p2[9][256];
uint64_t Zobrist::kazan[2][TOTAL_STONES + 1];
uint64_t Zobrist::tuzduk[2][10];
uint64_t Zobrist::to_play[2];
