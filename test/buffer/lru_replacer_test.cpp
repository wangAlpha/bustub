//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer_test.cpp
//
// Identification: test/buffer/lru_replacer_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <thread>  // NOLINT
#include <vector>

#include "buffer/lru_replacer.h"
#include "gtest/gtest.h"

namespace bustub {

TEST(LRUReplacerTest, SampleTest) {
  LRUReplacer lru_replacer(7);

  // Scenario: unpin six elements, i.e. add them to the replacer.
  lru_replacer.Unpin(1);
  lru_replacer.Unpin(2);
  lru_replacer.Unpin(3);
  lru_replacer.Unpin(4);
  lru_replacer.Unpin(5);
  lru_replacer.Unpin(6);
  lru_replacer.Unpin(1);
  EXPECT_EQ(6, lru_replacer.Size());

  // Scenario: get three victims from the lru.
  int value;
  lru_replacer.Victim(&value);
  EXPECT_EQ(2, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(3, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(4, value);

  // Scenario: pin elements in the replacer.
  // Note that 3 has already been victimized, so pinning 3 should have no effect.
  lru_replacer.Pin(4);
  lru_replacer.Pin(6);
  EXPECT_EQ(2, lru_replacer.Size());

  // Scenario: continue looking for victims. We expect these victims.
  lru_replacer.Victim(&value);
  EXPECT_EQ(5, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(1, value);
}

TEST(LRUReplacerTest, SampleTest1) {
  LRUReplacer lru_replacer(5);

  // Scenario: unpin five elements, i.e. add them to the replacer.
  lru_replacer.Unpin(5);
  lru_replacer.Unpin(3);
  lru_replacer.Unpin(4);
  lru_replacer.Unpin(1);
  lru_replacer.Unpin(2);
  lru_replacer.Unpin(3);
  lru_replacer.Unpin(2);

  EXPECT_EQ(5, lru_replacer.Size());

  // Scenario: get three victims from the clock.
  int value;
  lru_replacer.Victim(&value);
  EXPECT_EQ(5, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(4, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(1, value);

  // Scenario: pin elements in the replacer.
  // Note that 3 has already been victimized, so pinning 3 should have no effect.
  lru_replacer.Pin(2);
  lru_replacer.Pin(5);
  EXPECT_EQ(1, lru_replacer.Size());

  // Scenario: unpin 4. We expect that the reference bit of 4 will be set to 1.
  lru_replacer.Unpin(4);

  // Scenario: continue looking for victims. We expect these victims.
  lru_replacer.Victim(&value);
  EXPECT_EQ(3, value);
  lru_replacer.Victim(&value);
  EXPECT_EQ(4, value);
  EXPECT_EQ(false, lru_replacer.Victim(&value));
  EXPECT_EQ(0, lru_replacer.Size());
}

TEST(LRUReplacerTest, BasicTest) {
  LRUReplacer lru_replacer(100);

  // push element into replacer
  for (int i = 0; i < 100; ++i) {
    lru_replacer.Unpin(i);
  }
  EXPECT_EQ(100, lru_replacer.Size());

  // reverse then insert again
  for (int i = 0; i < 100; ++i) {
    lru_replacer.Unpin(99 - i);
  }

  // erase 50 element from the tail
  for (int i = 0; i < 50; ++i) {
    // EXPECT_EQ(true, lru_replacer.Erase(i));
    lru_replacer.Pin(i);
  }

  // check left
  int value = -1;
  for (int i = 99; i >= 50; --i) {
    lru_replacer.Victim(&value);
    EXPECT_EQ(i, value);
    value = -1;
  }
}

}  // namespace bustub
