//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/index_iterator.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
/**
 * index_iterator.h
 * For range scan of b+ tree
 */
#pragma once
#include "buffer/buffer_pool_manager.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

#define INDEXITERATOR_TYPE IndexIterator<KeyType, ValueType, KeyComparator>

INDEX_TEMPLATE_ARGUMENTS
class IndexIterator {
 public:
  // you may define your own constructor based on your member variables
  IndexIterator(B_PLUS_TREE_LEAF_PAGE_TYPE *leaf, const int index, BufferPoolManager *bfm)
      : current_leaf_node_(leaf), index_(index), buffer_pool_manager_(bfm) {}
  ~IndexIterator();

  bool isEnd();

  const MappingType &operator*();

  IndexIterator &operator++();

  bool operator==(const IndexIterator &iter) const { return iter.index_ == index_; }

  bool operator!=(const IndexIterator &iter) const { return iter.index_ != index_; }

 private:
  B_PLUS_TREE_LEAF_PAGE_TYPE *current_leaf_node_;
  int index_;
  BufferPoolManager *buffer_pool_manager_;
};

}  // namespace bustub
