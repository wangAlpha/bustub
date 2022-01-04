/**
 * b_plus_tree_test.cpp
 */

#include <algorithm>
#include <chrono>  // NOLINT
#include <cstdio>
#include <functional>
#include <random>
#include <thread>                   // NOLINT
#include "b_plus_tree_test_util.h"  // NOLINT
#include "buffer/buffer_pool_manager.h"
#include "gtest/gtest.h"
#include "storage/index/b_plus_tree.h"

namespace bustub {
// helper function to launch multiple threads
template <typename... Args>
void LaunchParallelTest(uint64_t num_threads, Args &&...args) {
  std::vector<std::thread> thread_group;

  // Launch a group of threads
  for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
    thread_group.push_back(std::thread(args..., thread_itr));
  }

  // Join the threads with the main thread
  for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
    thread_group[thread_itr].join();
  }
}

// helper function to insert
void InsertHelper(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree, const std::vector<int64_t> &keys,
                  __attribute__((unused)) uint64_t thread_itr = 0) {
  GenericKey<8> index_key;
  RID rid;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (auto key : keys) {
    int64_t value = key & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(key >> 32), value);
    index_key.SetFromInteger(key);
    tree->Insert(index_key, rid, transaction);
  }
  delete transaction;
}

void InsertAndGetHelper(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree, const std::vector<int64_t> &keys,
                        __attribute__((unused)) uint64_t thread_itr = 0) {
  GenericKey<8> index_key;
  RID rid;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (const auto &key : keys) {
    int64_t value = key & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(key >> 32), value);
    index_key.SetFromInteger(key);
    tree->Insert(index_key, rid, transaction);
  }

  for (const auto &key : keys) {
    std::vector<RID> rids;
    int64_t value = key & 0xFFFFFFFF;
    index_key.SetFromInteger(key);
    const bool ok = tree->GetValue(index_key, &rids, transaction);
    EXPECT_TRUE(ok);
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }
  delete transaction;
}

// helper function to seperate insert
void InsertHelperSplit(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree, const std::vector<int64_t> &keys,
                       int total_threads, __attribute__((unused)) uint64_t thread_itr) {
  GenericKey<8> index_key;
  RID rid;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (auto key : keys) {
    if (static_cast<uint64_t>(key) % total_threads == thread_itr) {
      int64_t value = key & 0xFFFFFFFF;
      rid.Set(static_cast<int32_t>(key >> 32), value);
      index_key.SetFromInteger(key);
      tree->Insert(index_key, rid, transaction);
    }
  }
  delete transaction;
}

// helper function to delete
void DeleteHelper(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree, const std::vector<int64_t> &remove_keys,
                  __attribute__((unused)) uint64_t thread_itr = 0) {
  GenericKey<8> index_key;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (auto key : remove_keys) {
    index_key.SetFromInteger(key);
    tree->Remove(index_key, transaction);
  }
  delete transaction;
}

void DeleteAndGetsHelper(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree,
                         const std::vector<int64_t> &remove_keys, __attribute__((unused)) uint64_t thread_itr = 0) {
  GenericKey<8> index_key;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (const auto &key : remove_keys) {
    index_key.SetFromInteger(key);
    tree->Remove(index_key, transaction);
  }
  for (const auto &key : remove_keys) {
    std::vector<RID> rids;
    index_key.SetFromInteger(key);
    const bool ok = tree->GetValue(index_key, &rids, transaction);
    EXPECT_EQ(ok, false);
  }

  delete transaction;
}

// helper function to seperate delete
void DeleteHelperSplit(BPlusTree<GenericKey<8>, RID, GenericComparator<8>> *tree,
                       const std::vector<int64_t> &remove_keys, int total_threads,
                       __attribute__((unused)) uint64_t thread_itr) {
  GenericKey<8> index_key;
  // create transaction
  Transaction *transaction = new Transaction(0);
  for (auto key : remove_keys) {
    if (static_cast<uint64_t>(key) % total_threads == thread_itr) {
      index_key.SetFromInteger(key);
      tree->Remove(index_key, transaction);
    }
  }
  delete transaction;
}

TEST(BPlusTreeConcurrentTest, InsertTest1) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  // keys to Insert
  std::vector<int64_t> keys;
  int64_t scale_factor = 100;
  for (int64_t key = 1; key < scale_factor; key++) {
    keys.push_back(key);
  }
  LaunchParallelTest(2, InsertHelper, &tree, keys);

  std::vector<RID> rids;
  GenericKey<8> index_key;
  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  int64_t start_key = 1;
  int64_t current_key = start_key;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    auto location = (*iterator).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key = current_key + 1;
  }

  EXPECT_EQ(current_key, keys.size() + 1);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, InsertTest2) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);
  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  // keys to Insert
  std::vector<int64_t> keys;
  int64_t scale_factor = 100;
  for (int64_t key = 1; key < scale_factor; key++) {
    keys.push_back(key);
  }
  LaunchParallelTest(2, InsertHelperSplit, &tree, keys, 2);

  std::vector<RID> rids;
  GenericKey<8> index_key;
  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  int64_t start_key = 1;
  int64_t current_key = start_key;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    auto location = (*iterator).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key = current_key + 1;
  }

  EXPECT_EQ(current_key, keys.size() + 1);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, InsertAndGetTest) {
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // Create a new b++ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);

  // Create and fetch header page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  std::vector<int64_t> keys;
  int64_t scale_factor = 1000;
  for (int64_t key = 1; key < scale_factor; ++key) {
    keys.push_back(key);
  }
  LaunchParallelTest(4, InsertAndGetHelper, &tree, keys);

  std::vector<RID> rids;
  GenericKey<8> index_key;
  for (const auto &key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  int64_t start_key = 1;
  int64_t current_key = start_key;
  index_key.SetFromInteger(start_key);
  for (auto it = tree.Begin(index_key); it != tree.end(); ++it) {
    auto location = (*it).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
  }

  EXPECT_EQ(current_key, keys.size() + 1);
  bpm->UnpinPage(HEADER_PAGE_ID, true);

  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteTest1) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  // sequential insert
  std::vector<int64_t> keys = {1, 2, 3, 4, 5};
  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys = {1, 5, 3, 4};
  LaunchParallelTest(2, DeleteHelper, &tree, remove_keys);

  int64_t start_key = 2;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    auto location = (*iterator).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key = current_key + 1;
    size += 1;
  }

  EXPECT_EQ(size, 1);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteAndGetTest) {
  Schema *key_schema = ParseCreateStatement("a bigint");
  constexpr int kKeySize = 8;
  GenericComparator<kKeySize> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);

  BPlusTree<GenericKey<kKeySize>, RID, GenericComparator<kKeySize>> tree("foo_pk", bpm, comparator);

  GenericKey<kKeySize> index_key;

  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  std::vector<int64_t> keys = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys = {1, 5, 3, 4, 6, 7, 8, 9, 10};
  LaunchParallelTest(2, DeleteAndGetsHelper, &tree, remove_keys);

  int64_t start_key = 2;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto it = tree.Begin(index_key); !it.isEnd(); ++it) {
    auto location = (*it).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
    size += 1;
  }

  EXPECT_EQ(size, 1);

  bpm->UnpinPage(HEADER_PAGE_ID, true);

  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteTest2) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  // sequential insert
  std::vector<int64_t> keys = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys = {1, 4, 3, 2, 5, 6};
  LaunchParallelTest(2, DeleteHelperSplit, &tree, remove_keys, 2);

  int64_t start_key = 7;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    auto location = (*iterator).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key = current_key + 1;
    size = size + 1;
  }

  EXPECT_EQ(size, 4);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteTest3) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  // sequential insert
  std::vector<int64_t> keys;
  int64_t scale_factor = 100;
  for (int64_t i = 1; i <= 100; ++i) {
    keys.emplace_back(i);
  }
  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys;
  for (int64_t i = 1; i <= (scale_factor - 20); ++i) {
    remove_keys.emplace_back(i);
  }
  LaunchParallelTest(2, DeleteHelperSplit, &tree, remove_keys, 2);

  int64_t start_key = scale_factor - 20 + 1;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    auto location = (*iterator).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
    size += 1;
  }

  EXPECT_EQ(size, 20);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteTest4) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  // sequential insert
  std::vector<int64_t> keys;
  int64_t scale_factor = 1000;
  for (int64_t i = 1; i <= scale_factor; ++i) {
    keys.emplace_back(i);
  }
  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys;
  for (int64_t i = 1; i <= (scale_factor - 20); ++i) {
    remove_keys.emplace_back(i);
  }
  LaunchParallelTest(3, DeleteHelperSplit, &tree, remove_keys, 3);

  int64_t start_key = scale_factor - 20 + 1;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto it = tree.Begin(index_key); it != tree.end(); ++it) {
    auto location = (*it).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
    size += 1;
  }

  EXPECT_EQ(size, 20);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, DeleteTest5) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;
  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  std::random_device rd;
  std::mt19937 mt(rd());

  // sequential insert
  std::vector<int64_t> keys;
  int64_t scale_factor = 1000;
  for (int64_t i = 1; i <= scale_factor; ++i) {
    keys.emplace_back(i);
  }
  std::shuffle(keys.begin(), keys.end(), mt);

  InsertHelper(&tree, keys);

  std::vector<int64_t> remove_keys;
  for (int64_t i = 1; i <= (scale_factor - 20); ++i) {
    remove_keys.emplace_back(i);
  }
  LaunchParallelTest(4, DeleteHelperSplit, &tree, remove_keys, 4);

  int64_t start_key = scale_factor - 20 + 1;
  int64_t current_key = start_key;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto it = tree.Begin(index_key); it != tree.end(); ++it) {
    auto location = (*it).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
    size += 1;
  }

  EXPECT_EQ(size, 20);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, MixTest) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;

  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  // first, populate index
  std::vector<int64_t> keys = {1, 2, 3, 4, 5};
  InsertHelper(&tree, keys);

  // concurrent insert
  keys.clear();
  for (int i = 6; i <= 10; i++) {
    keys.push_back(i);
  }
  LaunchParallelTest(1, InsertHelper, &tree, keys);
  // concurrent delete
  std::vector<int64_t> remove_keys = {1, 4, 3, 5, 6};
  LaunchParallelTest(1, DeleteHelper, &tree, remove_keys);

  int64_t start_key = 2;
  int64_t size = 0;
  index_key.SetFromInteger(start_key);
  for (auto iterator = tree.Begin(index_key); iterator != tree.end(); ++iterator) {
    size = size + 1;
  }

  EXPECT_EQ(size, 5);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, MixTest2) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;

  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;
  // first, populate index
  std::vector<int64_t> keys, removed_keys;
  for (int64_t i = 1; i <= 1000; i++) {
    removed_keys.push_back(i);
    keys.push_back(i + 1000);
  }
  // keys1: 0, 2, 4
  // keys2: 1, 3, 5
  // keys: 100 - 200

  // concurrent insert
  LaunchParallelTest(4, InsertHelperSplit, &tree, removed_keys, 4);
  // concurrent insert and delete
  std::thread t0(InsertHelper, &tree, keys, 0);
  LaunchParallelTest(4, DeleteHelperSplit, &tree, removed_keys, 4);
  t0.join();

  for (const auto &key : removed_keys) {
    std::vector<RID> rids;
    index_key.SetFromInteger(key);
    const bool ok = tree.GetValue(index_key, &rids);
    EXPECT_EQ(false, ok);
  }

  int64_t current_key = 1001;
  int64_t size = 0;
  index_key.SetFromInteger(current_key);
  for (auto it = tree.Begin(index_key); it != tree.end(); ++it) {
    auto location = (*it).second;
    EXPECT_EQ(location.GetPageId(), 0);
    EXPECT_EQ(location.GetSlotNum(), current_key);
    current_key += 1;
    size += 1;
  }

  EXPECT_EQ(size, 1000);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}

TEST(BPlusTreeConcurrentTest, MixTest3) {
  // create KeyComparator and index schema
  Schema *key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema);

  DiskManager *disk_manager = new DiskManager("test.db");
  BufferPoolManager *bpm = new BufferPoolManager(50, disk_manager);
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", bpm, comparator);
  GenericKey<8> index_key;

  // create and fetch header_page
  page_id_t page_id;
  auto header_page = bpm->NewPage(&page_id);
  (void)header_page;

  std::random_device rd;
  std::mt19937 mt(rd());

  // first, populate index
  std::vector<int64_t> keys, keys2, removed_keys1, removed_keys2;
  int64_t scale = 10000;
  for (int64_t i = 1; i <= scale; i++) {
    keys.push_back(i);
  }
  std::shuffle(keys.begin(), keys.end(), mt);
  for (int64_t i = 1; i <= scale / 4; ++i) {
    removed_keys1.push_back(keys.back());
    keys.pop_back();
  }
  for (int64_t i = 1; i <= scale / 4; ++i) {
    removed_keys2.push_back(keys.back());
    keys.pop_back();
  }
  for (int64_t i = 1; i <= scale / 4; ++i) {
    keys2.push_back(keys.back());
    keys.pop_back();
  }
  // concurrent insert
  LaunchParallelTest(4, InsertHelperSplit, &tree, removed_keys1, 4);
  LaunchParallelTest(4, InsertHelperSplit, &tree, removed_keys2, 4);
  // concurrent delete
  std::thread t0(InsertAndGetHelper, &tree, keys, 0);
  std::thread t1(InsertAndGetHelper, &tree, keys2, 0);
  std::thread t2(DeleteAndGetsHelper, &tree, removed_keys1, 0);
  std::thread t3(DeleteAndGetsHelper, &tree, removed_keys2, 0);

  t0.join();
  t1.join();
  t2.join();
  t3.join();

  std::vector<RID> rids;
  for (const auto &key : removed_keys1) {
    rids.clear();
    index_key.SetFromInteger(key);
    const bool ok = tree.GetValue(index_key, &rids);
    EXPECT_EQ(false, ok);
  }

  for (const auto &key : removed_keys2) {
    rids.clear();
    index_key.SetFromInteger(key);
    const bool ok = tree.GetValue(index_key, &rids);
    EXPECT_EQ(false, ok);
  }

  int64_t current_key = 0;
  int64_t size = 0;
  index_key.SetFromInteger(current_key);
  for (auto it = tree.Begin(index_key); it != tree.end(); ++it) {
    size += 1;
  }
  EXPECT_EQ(size, scale / 2);

  bpm->UnpinPage(HEADER_PAGE_ID, true);
  delete key_schema;
  delete disk_manager;
  delete bpm;
  remove("test.db");
  remove("test.log");
}
}  // namespace bustub
