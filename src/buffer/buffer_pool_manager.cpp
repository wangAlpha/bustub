//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"
#include <iterator>
#include <ostream>
#include "include/common/logger.h"

namespace bustub {

template <typename T, typename InputIterator>
void PrintContainer(std::ostream &ostr, InputIterator begin, InputIterator end, const std::string &delimiter) {
  std::copy(begin, end, std::ostream_iterator<T>(ostr, delimiter.c_str()));
}

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // We allocate a consecutive memory space for the buffer pool.

  pages_ = std::make_unique<std::vector<Page>>(pool_size);
  replacer_ = std::make_unique<LRUReplacer>(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() {}

Page *BufferPoolManager::FetchPageImpl(page_id_t page_id) {
  if (auto it = page_table_.find(page_id); it != page_table_.end()) {
    auto const &frame_id = it->second;
    auto &page = pages_->at(frame_id);
    if (page.GetPinCount() == 0) {
      page.pin_count_ = 1;
    }
    replacer_->Pin(frame_id);
    LOG_DEBUG("Fetch Page id:%d, frame id:%d", page_id, frame_id);
    return &page;
  }

  frame_id_t frame_id;
  if (AllPinned()) {
    LOG_WARN("Fail to search a free frame, page id:%d", page_id);
    return nullptr;
  }
  LOG_DEBUG("Free list size:%zu, Replacer size: %zu, page id:%d", free_list_.size(), replacer_->Size(), page_id);
  SearchFreeFrame(&frame_id);
  auto &page = pages_->at(frame_id);
  ResetPage(frame_id, page_id);
  disk_manager_->ReadPage(page.GetPageId(), page.GetData());
  replacer_->Pin(frame_id);
  page_table_.emplace(page_id, frame_id);

  LOG_DEBUG("Fetch Page id:%d, frame id:%d", page_id, frame_id);
  return &page;
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  if (auto it = page_table_.find(page_id); it == page_table_.end()) {
    //    LOG_WARN("#UnpinPage Fail! Page id:%d", page_id);
    //    for (auto &[k, v] : page_table_) {
    //      LOG_WARN("%d -> %d", k, v);
    //    }
    return false;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);
  page.pin_count_--;
  if (page.GetPinCount() <= 0) {
    replacer_->Unpin(frame_id);
    //    free_list_.push_back(frame_id);
    page_table_.erase(page_id);
  }
  page.is_dirty_ |= is_dirty;

  return true;
}

bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  if (page_table_.find(page_id) == page_table_.end()) {
    LOG_WARN("#FlushPage Page id:%d failure", page_id);
    return false;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);
  disk_manager_->WritePage(page_id, page.GetData());
  page.is_dirty_ = false;
  return true;
}

Page *BufferPoolManager::NewPageImpl(page_id_t *page_id) {
  if (AllPinned()) {
    return nullptr;
  }

  *page_id = disk_manager_->AllocatePage();
  frame_id_t frame_id;
  if (SearchFreeFrame(&frame_id)) {
    auto &page = pages_->at(frame_id);
    ResetPage(frame_id, *page_id);
    page_table_[*page_id] = frame_id;
    replacer_->Pin(frame_id);
    return &page;
  }
  LOG_WARN("Can't create a new page, without free list");
  return nullptr;
}

bool BufferPoolManager::DeletePageImpl(page_id_t page_id) {
  if (auto it = page_table_.find(page_id); it == page_table_.end()) {
    return true;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);
  if (page.GetPinCount() > 0) {
    return false;
  }
  page_table_.erase(page_id);
  if (page.IsDirty()) {
    disk_manager_->WritePage(page_id, page.GetData());
  }
  page_table_.erase(page_id);
  disk_manager_->DeallocatePage(page_id);

  free_list_.push_front(frame_id);
  page.page_id_ = INVALID_PAGE_ID;
  return true;
}

void BufferPoolManager::FlushAllPagesImpl() {
  for (auto &page : *pages_) {
    FlushPageImpl(page.GetPageId());
  }
}

void BufferPoolManager::ResetPage(frame_id_t frame_id, page_id_t page_id) {
  auto &page = pages_->at(frame_id);
  page.page_id_ = page_id;
  page.pin_count_ = 1;
  page.ResetMemory();
}

bool BufferPoolManager::AllPinned() { return free_list_.empty() && (replacer_->Size() == 0); }

bool BufferPoolManager::SearchFreeFrame(frame_id_t *frame_id) {
  if (!free_list_.empty()) {
    *frame_id = free_list_.front();
    free_list_.pop_front();
    return true;
  }

  if (!replacer_->Victim(frame_id)) {
    LOG_ERROR("Find replacer free frame failure");
    return false;
  }
  auto &page = pages_->at(page_table_[*frame_id]);
  auto const &page_id = page.GetPageId();
  page_table_.erase(page_id);
  if (page.IsDirty()) {
    disk_manager_->WritePage(page_id, page.GetData());
  }
  return true;
}
}  // namespace bustub
