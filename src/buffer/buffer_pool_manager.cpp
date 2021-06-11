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
#include "include/common/logger.h"

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // We allocate a consecutive memory space for the buffer pool.

  pages_ = std::make_unique<std::vector<Page>>(pool_size_);
  replacer_ = std::make_unique<LRUReplacer>(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() {}

Page *BufferPoolManager::FetchPageImpl(page_id_t page_id) {
  // TODO: To implement this method
  // Search the page table for the requested page (P).
  // If P exists, pin it and return it immediately.

  if (auto it = page_table_.find(page_id); it != page_table_.end()) {
    auto const frame_id = it->second;
    auto &page = pages_->at(frame_id);
    replacer_->Pin(frame_id);
    LOG_INFO("#FetchPage: %d", page_id);
    return &page;
  }

  Page *fetch_page = nullptr;

  if (!free_list_.empty()) {
    auto const frame_id = free_list_.front();
    free_list_.pop_front();

    fetch_page = &pages_->at(frame_id);
    fetch_page->page_id_ = page_id;
    page_table_[page_id] = frame_id;

    // Update P's metadata, read in the page content from disk, and then return a pointer to P.
    disk_manager_->ReadPage(fetch_page->GetPageId(), fetch_page->GetData());
    LOG_INFO("#FetchPage - Read: %d ", fetch_page->GetPageId());
  } else {
    frame_id_t frame_id;
    if (!replacer_->Victim(&frame_id)) {
      return nullptr;
    }
    // Delete R from the page table and insert P.
    page_table_[page_id] = frame_id;
    fetch_page = &pages_->at(frame_id);

    fetch_page->page_id_ = page_id;

    // If R is dirty, write it back to the disk.
    if (fetch_page->IsDirty()) {
      disk_manager_->WritePage(fetch_page->GetPageId(), fetch_page->GetData());
    }

    LOG_INFO("#FetchPage - Replace: %d ", fetch_page->GetPageId());
  }

  return fetch_page;
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  if (auto it = page_table_.find(page_id); it == page_table_.end()) {
    LOG_WARN("#UnpinPage Fail! Page id:%d", page_id);
    for (auto &[k, v] : page_table_) {
      LOG_WARN("%d -> %d", k, v);
    }
    return false;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);

  page.pin_count_ = 0;
  page.is_dirty_ = is_dirty;
  replacer_->Unpin(frame_id);

  return true;
}

bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  if (page_table_.find(page_id) == page_table_.end()) {
    LOG_WARN("#FlushPage Page id:%d fail", page_id);
    return false;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);
  disk_manager_->WritePage(page_id, page.GetData());
  return true;
}

Page *BufferPoolManager::NewPageImpl(page_id_t *page_id) {
  if (free_list_.empty()) {
    // TODO: all pinned
    auto const all_pinned = std::all_of(pages_->begin(), pages_->end(), [](Page &e) { return e.GetPinCount() > 0; });
    // If all the pages in the buffer pool are pinned, return nullptr.
    if (all_pinned) {
      LOG_WARN("#NewPage fail, free_list is empty");
      return nullptr;
    }

    // Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
    frame_id_t replace_frame_id;
    if (!replacer_->Victim(&replace_frame_id)) {
      return nullptr;
    }

    auto &page = pages_->at(replace_frame_id);
    page.ResetMemory();
    *page_id = page.GetPageId();
    LOG_INFO("#New Page Id: %d", *page_id);
    return &page;
  }

  // Make a new page from DiskManager,
  *page_id = disk_manager_->AllocatePage();
  auto const frame_id = free_list_.front();

  free_list_.pop_front();
  page_table_[*page_id] = frame_id;

  // Update P's metadata, zero out memory and add P to the page table.
  // Set the page ID output parameter. Return a pointer to P.
  auto &page = pages_->at(frame_id);
  page.page_id_ = *page_id;
  page.ResetMemory();

  return &page;
}

bool BufferPoolManager::DeletePageImpl(page_id_t page_id) {
  // 0.   Make sure you call DiskManager::DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  if (auto it = page_table_.find(page_id); it == page_table_.end()) {
    return true;
  }

  auto const frame_id = page_table_[page_id];
  auto &page = pages_->at(frame_id);
  if (page.GetPinCount() != 0) {
    return false;
  }
  page_table_.erase(page_id);
  disk_manager_->DeallocatePage(page_id);

  free_list_.push_front(frame_id);
  page.page_id_ = INVALID_PAGE_ID;
  page.ResetMemory();
  return true;
}

void BufferPoolManager::FlushAllPagesImpl() {
  for (auto &page : *pages_) {
    disk_manager_->WritePage(page.GetPageId(), page.GetData());
  }
}

}  // namespace bustub

/// -`BufferPoolManager`职责是`DiskManager`取得数据库页，并将其存储在内存中。`BufferPoolManager`有明确的指令或者需要换出空间时也会进行交换。

/// -主要交互对象是DiskMaennager

// BufferPoolManager`
// 但是对于系统开发人员来说，了解`Page`对象只是缓冲池中内存的容器，因此并不特定于唯一页面，这一点很重要。
// 也就是说，每个`Page` 对象都包含一块内存，`DiskManager` 将使用该内存块作为一个位置来复制它从磁盘读取的 **物理页面**
// 的内容。 `BufferPoolManager` 将重用相同的 `Page`
//
// 对象来存储数据，因为它来回移动到磁盘。这意味着在系统的整个生命周期中，同一个`Page`对象可能包含不同的物理页面。
// `Page` 对象的标识符 (`page_id`) 跟踪它包含的物理页面；
// 如果“Page”对象不包含物理页面，则其`page_id`必须设置为`INVALID_PAGE_ID`。

// 每个`Page`页维持一个计数器计数具有"pinned"页的线程数。`BufferPoolManager`不允许释放 pinned 的`Page`。
// 对`Page`object也要进行追踪。你的工作是记录页unpinned前是否被修改。
// 你的`BufferPoolManager`必须把脏页写回硬盘，在对象被重用前。

// `BufferPoolManager`使用`LRUReplacer`实现。它将使用`LRUReplacer`来跟踪“Page”对象何时被访问，
// 以便它可以决定在必须释放帧以腾出空间从磁盘复制新物理页面时驱逐哪个对象
