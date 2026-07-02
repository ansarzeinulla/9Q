#include <gtest/gtest.h>

#include "togyz/dag_search.hpp"

TEST(TTEvictionTest, BasicStoreAndProbe) {
  dag_search::TranspositionTable tt(4);  // 4 MB table
  uint64_t hash = 0x123456789ABCDEFULL;
  int16_t eval = 42;
  uint8_t depth = 5;
  uint8_t flag = 1;
  uint16_t best_move = 3;

  tt.store(hash, eval, depth, flag, best_move);

  dag_search::TTEntry entry;
  bool found = tt.probe(hash, entry);

  EXPECT_TRUE(found);
  EXPECT_EQ(entry.lock, hash);
  EXPECT_EQ(entry.eval, eval);
  EXPECT_EQ(entry.depth, depth);
  EXPECT_EQ(entry.flag, flag);
  EXPECT_EQ(entry.best_move, best_move);
}

TEST(TTEvictionTest, DepthPreferredEviction) {
  dag_search::TranspositionTable tt(4);  // 4 MB table

  // Two hashes that collide at the same table index:
  // hash2 = hash1 + table_size  =>  (hash1 & mask) == (hash2 & mask)
  size_t table_size = tt.entries();
  uint64_t hash1 = 100;
  uint64_t hash2 = 100 + table_size;

  // Store deeper search first.
  tt.store(hash1, 100, 10, 1, 2);

  // Try to overwrite with a shallower search (depth 5 < 10 => should NOT replace).
  tt.store(hash2, 50, 5, 1, 3);

  dag_search::TTEntry entry;
  bool found1 = tt.probe(hash1, entry);
  EXPECT_TRUE(found1);
  EXPECT_EQ(entry.eval, 100);
  EXPECT_EQ(entry.depth, 10);

  // Overwrite with equal depth (depth 10 >= 10 => should replace).
  tt.store(hash2, 60, 10, 1, 4);
  bool found2 = tt.probe(hash2, entry);
  EXPECT_TRUE(found2);
  EXPECT_EQ(entry.eval, 60);
  EXPECT_EQ(entry.depth, 10);

  // Overwrite with higher depth (15 >= 10 => should replace).
  tt.store(hash1, 200, 15, 1, 5);
  found1 = tt.probe(hash1, entry);
  EXPECT_TRUE(found1);
  EXPECT_EQ(entry.eval, 200);
  EXPECT_EQ(entry.depth, 15);
}

TEST(TTEvictionTest, TableClear) {
  dag_search::TranspositionTable tt(4);
  uint64_t hash = 999;
  tt.store(hash, 10, 5, 0, 1);

  dag_search::TTEntry entry;
  EXPECT_TRUE(tt.probe(hash, entry));

  tt.clear();
  EXPECT_FALSE(tt.probe(hash, entry));
}
