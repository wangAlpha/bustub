//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// clock_replacer.cpp
//
// Identification: src/buffer/clock_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/clock_replacer.h"

namespace bustub {

ClockReplacer::ClockReplacer(size_t num_pages) : num_pages_(num_pages) {}

ClockReplacer::~ClockReplacer() = default;

bool ClockReplacer::Victim(frame_id_t *frame_id) {
  if (page_table_.empty()) {
    return false;
  }
  for (auto &it : pages_) {
    if (it.second == true) {
      it.second = false;
    } else {
      const auto id = it.first;
      *frame_id = id;
      pages_.remove_if([id](auto &e) { return id == e.first; });
      page_table_.erase(id);
      return true;
    }
  }
  return false;
}

void ClockReplacer::Pin(frame_id_t frame_id) {
  if (page_table_.empty() || page_table_.find(frame_id) == page_table_.end()) {
    return;
  }
  page_table_.erase(frame_id);
  pages_.remove_if([id = frame_id](auto &e) { return e.first == id; });
}

void ClockReplacer::Unpin(frame_id_t frame_id) {
  if (page_table_.find(frame_id) == page_table_.end() && page_table_.size() < num_pages_) {
  }
}

size_t ClockReplacer::Size() { return page_table_.size(); }

}  // namespace bustub
