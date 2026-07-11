#include <gtest/gtest.h>

#include "togyz/dag_v2_search.hpp"

namespace {

using dag_v2::AspirationWindow;

TEST(AspirationWindow, InitialCentersOnPreviousScore) {
  AspirationWindow w = AspirationWindow::initial(120.0);
  EXPECT_DOUBLE_EQ(w.alpha, 120.0 - AspirationWindow::INITIAL_DELTA);
  EXPECT_DOUBLE_EQ(w.beta, 120.0 + AspirationWindow::INITIAL_DELTA);
  EXPECT_FALSE(w.is_full());
}

TEST(AspirationWindow, FailHighWidensBetaOnly) {
  AspirationWindow w = AspirationWindow::initial(0.0);
  AspirationWindow w2 = w.widen_fail_high();
  EXPECT_DOUBLE_EQ(w2.alpha, w.alpha);
  EXPECT_GT(w2.beta, w.beta);
}

TEST(AspirationWindow, FailLowWidensAlphaOnly) {
  AspirationWindow w = AspirationWindow::initial(0.0);
  AspirationWindow w2 = w.widen_fail_low();
  EXPECT_DOUBLE_EQ(w2.beta, w.beta);
  EXPECT_LT(w2.alpha, w.alpha);
}

TEST(AspirationWindow, RepeatedFailsEscalateToFullWindow) {
  AspirationWindow w = AspirationWindow::initial(0.0);
  for (int i = 0; i < 10 && !w.is_full(); ++i) w = w.widen_fail_high();
  EXPECT_TRUE(w.is_full());
  w = AspirationWindow::initial(0.0);
  for (int i = 0; i < 10 && !w.is_full(); ++i) w = w.widen_fail_low();
  EXPECT_TRUE(w.is_full());
}

TEST(AspirationWindow, FullWindowIsFull) {
  EXPECT_TRUE(AspirationWindow::full().is_full());
}

TEST(AspirationWindow, MateBoundScoresSkipAspiration) {
  // TT clamps stored scores to int16, so anything at/above that band is
  // mate-like and windows around it are useless.
  EXPECT_TRUE(AspirationWindow::is_mate_bound(32000.0));
  EXPECT_TRUE(AspirationWindow::is_mate_bound(-32000.0));
  EXPECT_TRUE(AspirationWindow::is_mate_bound(dag_v2::WIN_SCORE));
  EXPECT_FALSE(AspirationWindow::is_mate_bound(3000.0));
  EXPECT_FALSE(AspirationWindow::is_mate_bound(0.0));
}

}  // namespace
