#include <gtest/gtest.h>

#include "togyz/dag_v2_search.hpp"

namespace {

TEST(KillerTable, StartsEmpty) {
  dag_v2::KillerTable kt;
  for (int ply = 0; ply < dag_v2::MAX_PLY; ++ply) {
    for (int m = 0; m < 9; ++m) EXPECT_FALSE(kt.is_killer(ply, m));
  }
}

TEST(KillerTable, RecordAndShift) {
  dag_v2::KillerTable kt;
  kt.record(3, 4);
  EXPECT_TRUE(kt.is_killer(3, 4));
  EXPECT_EQ(kt.slot(3, 0), 4);
  kt.record(3, 7);
  EXPECT_EQ(kt.slot(3, 0), 7);
  EXPECT_EQ(kt.slot(3, 1), 4);
  kt.record(3, 2);
  EXPECT_EQ(kt.slot(3, 0), 2);
  EXPECT_EQ(kt.slot(3, 1), 7);
  EXPECT_FALSE(kt.is_killer(3, 4));
}

TEST(KillerTable, DuplicateRecordIsNoOp) {
  dag_v2::KillerTable kt;
  kt.record(5, 1);
  kt.record(5, 8);
  kt.record(5, 8);  // already slot 0 — must not duplicate into slot 1
  EXPECT_EQ(kt.slot(5, 0), 8);
  EXPECT_EQ(kt.slot(5, 1), 1);
}

TEST(KillerTable, PerPlyIsolationAndClear) {
  dag_v2::KillerTable kt;
  kt.record(0, 3);
  EXPECT_FALSE(kt.is_killer(1, 3));
  kt.clear();
  EXPECT_FALSE(kt.is_killer(0, 3));
}

TEST(KillerTable, OutOfRangePlyIsSafe) {
  dag_v2::KillerTable kt;
  kt.record(-1, 3);
  kt.record(dag_v2::MAX_PLY, 3);
  EXPECT_FALSE(kt.is_killer(-1, 3));
  EXPECT_FALSE(kt.is_killer(dag_v2::MAX_PLY, 3));
}

TEST(HistoryTable, RewardAccumulatesDepthSquared) {
  dag_v2::HistoryTable ht;
  EXPECT_EQ(ht.get(0, 4), 0);
  ht.reward(0, 4, 3);
  EXPECT_EQ(ht.get(0, 4), 9);
  ht.reward(0, 4, 5);
  EXPECT_EQ(ht.get(0, 4), 34);
}

TEST(HistoryTable, PerPlayerIsolation) {
  dag_v2::HistoryTable ht;
  ht.reward(0, 2, 4);
  EXPECT_EQ(ht.get(1, 2), 0);
  EXPECT_EQ(ht.get(0, 2), 16);
}

TEST(HistoryTable, OverflowGuardHalvesAll) {
  dag_v2::HistoryTable ht;
  ht.reward(0, 1, 10);  // 100
  while (ht.get(0, 0) <= dag_v2::HistoryTable::HALVE_THRESHOLD - 100) {
    ht.reward(0, 0, 100);  // +10000 each
  }
  int other_before = ht.get(0, 1);
  ht.reward(0, 0, 100);  // crosses threshold, triggers halving
  EXPECT_LE(ht.get(0, 0), dag_v2::HistoryTable::HALVE_THRESHOLD);
  EXPECT_EQ(ht.get(0, 1), other_before / 2);
}

TEST(HistoryTable, Clear) {
  dag_v2::HistoryTable ht;
  ht.reward(1, 3, 7);
  ht.clear();
  EXPECT_EQ(ht.get(1, 3), 0);
}

}  // namespace
