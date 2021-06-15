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
  if (pages_.empty()) {
    LOG_DEBUG("------Victim------");
    for (auto const &[page, frame] : pages_) {
      LOG_DEBUG("page: %d-> frame: %d", page, frame);
    }
    return false;
  }
  // LOG_DEBUG("----------Victim:----------");
  // for (const auto &kv : pages_) {
  //   LOG_DEBUG("# %d -> %d", kv.first, kv.second);
  // }
  const auto least_frame =
      std::min_element(pages_.begin(), pages_.end(), [](auto &a, auto &b) { return a.second > b.second; });
  *frame_id = least_frame->first;
  pages_.erase(least_frame);
  return true;
}

// Pin/Reference(T) : This method should be called after a page is pinned to a frame in the BufferPoolManager.
// It should remove the frame containing the pinned page from the LRUReplacer.
void LRUReplacer::Pin(frame_id_t frame_id) {
  if (pages_.find(frame_id) != pages_.end()) {
    pages_.erase(frame_id);
  }
}

// Unpin(T) : This method should be called when the pin_count of a page becomes 0.
// This method should add the frame containing the unpinned page to the LRUReplacer.
void LRUReplacer::Unpin(frame_id_t frame_id) {
  if (pages_.find(frame_id) == pages_.end()) {
    for (auto &[k, v] : pages_) {
      v++;
    }
    pages_[frame_id] = 0;
    if (pages_.size() >= num_pages_) {
      const auto least_frame =
          std::max_element(pages_.begin(), pages_.end(), [](auto &a, auto &b) { return a.second > b.second; });
      pages_.erase(least_frame);
    }
    pages_[frame_id] = 0;
  }
}

// Size() : This method returns the number of frames that are currently.
size_t LRUReplacer::Size() { return pages_.size(); }

bool LRUReplacer::Empty() { return pages_.empty(); }
}  // namespace bustub
