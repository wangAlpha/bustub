//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.cpp
//
// Identification: src/buffer/lru_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_replacer.h"
#include "include/common/logger.h"
namespace bustub {

LRUReplacer::LRUReplacer(const size_t num_pages) : num_pages_(num_pages) {}

LRUReplacer::~LRUReplacer() = default;

// Victim(T*) = Swap(*T): Remove the object that was accessed the least recently
// compared to all the elements being tracked by the Replacer, store its contents
// in the output parameter.
bool LRUReplacer::Victim(frame_id_t *frame_id) {
  if (page_table_.empty()) {
    return false;
  }

  const auto least_frame = pages_.front();
  pages_.pop_front();
  *frame_id = least_frame;
  page_table_.erase(least_frame);
  return true;
}

// Pin/Reference(T) : This method should be called after a page is pinned to a frame in the BufferPoolManager.
// It should remove the frame containing the pinned page from the LRUReplacer.
void LRUReplacer::Pin(frame_id_t frame_id) {
  if (page_table_.find(frame_id) != page_table_.end()) {
    auto it = page_table_[frame_id];
    pages_.erase(it);
    page_table_.erase(frame_id);
  }
}

// Unpin(T) : This method should be called when the pin_count of a page becomes 0.
// This method should add the frame containing the unpinned page to the LRUReplacer.
void LRUReplacer::Unpin(frame_id_t frame_id) {
  if (page_table_.find(frame_id) == page_table_.end()) {
    if (page_table_.size() >= num_pages_) {
      auto least_frame = pages_.front();
      pages_.pop_front();
      page_table_.erase(least_frame);
    }

    const auto it = pages_.insert(pages_.end(), frame_id);
    page_table_[frame_id] = it;
  }
}

// Size() : This method returns the number of frames that are currently.
size_t LRUReplacer::Size() { return pages_.size(); }

}  // namespace bustub
