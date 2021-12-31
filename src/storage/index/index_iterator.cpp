/**
 * index_iterator.cpp
 */
#include <cassert>

#include "common/config.h"
#include "common/logger.h"
#include "storage/index/index_iterator.h"

namespace bustub {

/*
 * NOTE: you can change the destructor/constructor method here
 * set your own input parameters
 */
// INDEX_TEMPLATE_ARGUMENTS
// INDEXITERATOR_TYPE::IndexIterator() = default;

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() {
  if (current_leaf_node_ != nullptr) {
    buffer_pool_manager_->UnpinPage(current_leaf_node_->GetPageId(), false);
  }
}

INDEX_TEMPLATE_ARGUMENTS
bool INDEXITERATOR_TYPE::isEnd() { return current_leaf_node_ == nullptr || index_ >= current_leaf_node_->GetSize(); }

INDEX_TEMPLATE_ARGUMENTS
const MappingType &INDEXITERATOR_TYPE::operator*() { return current_leaf_node_->GetItem(index_); }

INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE &INDEXITERATOR_TYPE::operator++() {
  index_ += 1;
  LOG_DEBUG("index_ == %d", index_);
  if (index_ >= current_leaf_node_->GetMaxSize()) {
    const page_id_t next = current_leaf_node_->GetNextPageId();
    if (next == INVALID_PAGE_ID) {
      return *this;
    } else {
      Page *page = buffer_pool_manager_->FetchPage(next);
      current_leaf_node_ = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page->GetData());
      buffer_pool_manager_->UnpinPage(next, false);
      index_ = 0;
    }
  }
  return *this;
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;
template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;
template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
