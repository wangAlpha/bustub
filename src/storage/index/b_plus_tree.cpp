//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/index/b_plus_tree.cpp
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <string>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {

INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to decide whether current b+tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::IsEmpty() const { return root_page_id_ == INVALID_PAGE_ID; }

INDEX_TEMPLATE_ARGUMENTS
BPlusTreePage *BPLUSTREE_TYPE::FetchPage(page_id_t page_id) {
  const auto page = buffer_pool_manager_->FetchPage(page_id);
  assert(page != nullptr);
  return reinterpret_cast<BPlusTreePage *>(page->GetData());
}
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) {
  // 1. find page
  auto leaf_page = FindLeafPage(key);
  if (!leaf_page) {
    return false;
  }
  // 2. find value
  result->resize(1);
  const bool exist = leaf_page->Lookup(key, &result->at(0), comparator_);
  // 3. unpin buffer page
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return exist;
}
// std::ofstream out("out.txt", std::ios::out | std::ios::trunc);
/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) {
  // if current tree is empty, start new tree, update root page id and insert entry

  bool inserted = true;
  if (IsEmpty()) {
    StartNewTree(key, value);
  } else {
    inserted = InsertIntoLeaf(key, value, transaction);
  }
  return inserted;
}
/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  // 1. Allocate a new page from bufer pool manager
  // page_id_t root_page_id;
  Page *root_page = buffer_pool_manager_->NewPage(&root_page_id_);
  assert(root_page != nullptr);
  LOG_DEBUG("root page id: %d", root_page_id_);
  // B_PLUS_TREE_LEAF_PAGE
  auto root = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(root_page->GetData());
  // 2. Update B+ tree root page id.
  root->Init(root_page_id_, INVALID_PAGE_ID);
  // 3. Insert a new entry into leaf page.
  root->Insert(key, value, comparator_);
  UpdateRootPageId(true);
  buffer_pool_manager_->UnpinPage(root->GetPageId(), true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immdiately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction) {
  auto leaf_page = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(FindLeafPage(key));
  ValueType _v;  // unused
  const bool exist = leaf_page->Lookup(key, &_v, comparator_);
  if (exist) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;
  }
  leaf_page->Insert(key, value, comparator_);
  LOG_DEBUG("Inserted page id: %d, size: %d max size: %d %d", leaf_page->GetPageId(), leaf_page->GetSize(),
            leaf_page->GetMaxSize(), leaf_max_size_);
  if (leaf_page->GetSize() > leaf_page->GetMaxSize()) {
    auto new_leaf_page = Split(leaf_page);
    InsertIntoParent(leaf_page, new_leaf_page->KeyAt(0), new_leaf_page, transaction);
  }
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
  return true;
}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
INDEX_TEMPLATE_ARGUMENTS template <typename N>
N *BPLUSTREE_TYPE::Split(N *node) {
  page_id_t page_id;
  Page *new_page = buffer_pool_manager_->NewPage(&page_id);
  assert(new_page != nullptr);
  auto new_node = reinterpret_cast<N *>(new_page->GetData());
  new_node->Init(page_id, node->GetParentPageId());
  new_node->MoveHalfTo(node, buffer_pool_manager_);
  return node;
}
/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
                                      Transaction *transaction) {
  if (old_node->IsRootPage()) {
    page_id_t new_page_id;
    Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
    assert(new_page != nullptr);
    assert(new_page->GetPinCount() == 1);

    InternalPage *node = reinterpret_cast<InternalPage *>(new_page->GetData());
    node->Init(root_page_id_, INVALID_PAGE_ID);
    node->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    old_node->SetParentPageId(root_page_id_);
    new_node->SetParentPageId(root_page_id_);
    UpdateRootPageId(false);
    // Fetch page and new page need to unpin page
    buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(old_node->GetPageId(), true);
    return;
  }
  const page_id_t parent_id = old_node->GetParentPageId();
  auto parent_page = FetchPage(parent_id);
  assert(parent_page != nullptr);
  auto parent_node = reinterpret_cast<InternalPage *>(parent_page);
  new_node->SetParentPageId(parent_id);
  buffer_pool_manager_->UnpinPage(new_node->GetPageId(), true);

  parent_node->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
  if (parent_node->GetSize() > parent_node->GetMaxSize()) {
    InternalPage *new_leaf_page = Split(parent_node);
    InsertIntoParent(parent_node, new_leaf_page->KeyAt(0), new_leaf_page, transaction);
  }
  buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immdiately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  if (IsEmpty()) {
    return;
  }
  auto leaf_page = FindLeafPage(key);
  const int old_size = leaf_page->GetSize();
  const int curr_size = leaf_page->RemoveAndDeleteRecord(key, comparator_);
  const bool removed = old_size > curr_size;
  ToString(leaf_page, buffer_pool_manager_);
  const bool delete_page =
      removed && curr_size < leaf_page->GetMinSize() && CoalesceOrRedistribute(leaf_page, transaction);
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), removed);
  if (delete_page) {
    LOG_DEBUG("Start to delete Page");
    buffer_pool_manager_->DeletePage(leaf_page->GetPageId());
  }
}

/*
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) {
  if (node->IsRootPage()) {
    return AdjustRoot(node);
  }
  Page *parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());

  auto parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  const int node_index = parent_node->ValueIndex(node->GetPageId());

  const int sibling_index = node_index == 0 ? 1 : node_index - 1;
  page_id_t sibling_page_id = parent_node->ValueAt(sibling_index);
  Page *sibling_page = buffer_pool_manager_->FetchPage(sibling_page_id);
  auto sibling_node = reinterpret_cast<N *>(sibling_page);
  const bool coalesce = sibling_node->GetSize() + node->GetSize() < node->GetMaxSize();

  bool removed_parent = false;
  if (coalesce) {
    removed_parent = Coalesce(sibling_node, node, parent_node, node_index, transaction);
    buffer_pool_manager_->UnpinPage(node->GetPageId(), true);
    if (node_index == 0) {
      buffer_pool_manager_->DeletePage(sibling_node->GetPageId());
    }
  } else {
    Redistribute(sibling_node, node, node_index);
    buffer_pool_manager_->UnpinPage(sibling_node->GetPageId(), true);
  }

  buffer_pool_manager_->UnpinPage(parent_node->GetPageId(), true);
  if (removed_parent) {
    buffer_pool_manager_->DeletePage(parent_node->GetPageId());
  }

  return coalesce && node_index != 0;
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
bool BPLUSTREE_TYPE::Coalesce(N *&neighbor_node, N *&node,
                              BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *&parent, int index,
                              Transaction *transaction) {
  if (index == 0) {
    std::swap(node, neighbor_node);
    index = 1;
  }
  node->MoveAllTo(neighbor_node, neighbor_node->KeyAt(index), buffer_pool_manager_);

  parent->Remove(index);
  if (parent->GetSize() <= parent->GetMinSize()) {
    return CoalesceOrRedistribute(parent, transaction);
  }

  return false;
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node, int index) {
  if (index == 0) {
    neighbor_node->MoveFirstToEndOf(node, node->KeyAt(0), buffer_pool_manager_);
  } else {
    neighbor_node->MoveLastToFrontOf(node, node->KeyAt(index), buffer_pool_manager_);
  }
}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happend
 */
INDEX_TEMPLATE_ARGUMENTS
bool BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) {
  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    root_page_id_ = INVALID_PAGE_ID;
    UpdateRootPageId(false);
    return true;
  }
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    auto internal_page = reinterpret_cast<InternalPage *>(old_root_node);
    auto new_root_id = internal_page->RemoveAndReturnOnlyChild();
    auto new_root_page = buffer_pool_manager_->FetchPage(new_root_id);
    auto new_root_node = reinterpret_cast<BPlusTreePage *>(new_root_page);
    new_root_node->SetParentPageId(new_root_id);
    root_page_id_ = new_root_id;
    buffer_pool_manager_->UnpinPage(new_root_node->GetPageId(), true);
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leaftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::begin() {
  KeyType _key;  // holdplacer
  auto first_page = FindLeafPage(_key, true);
  return INDEXITERATOR_TYPE(first_page, 0, buffer_pool_manager_);
}

/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::Begin(const KeyType &key) {
  B_PLUS_TREE_LEAF_PAGE_TYPE *leaf_node = FindLeafPage(key, true);
  if (leaf_node == nullptr) {
    LOG_DEBUG("Tree find first key is NULL");
    return INDEXITERATOR_TYPE(nullptr, 0, buffer_pool_manager_);
  }
  return INDEXITERATOR_TYPE(leaf_node, leaf_node->KeyIndex(key, comparator_), buffer_pool_manager_);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE BPLUSTREE_TYPE::end() {
  auto leaf_node = FetchPage(root_page_id_);
  int index = leaf_node->GetSize();
  return INDEXITERATOR_TYPE(nullptr, index, buffer_pool_manager_);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page
 */
INDEX_TEMPLATE_ARGUMENTS
B_PLUS_TREE_LEAF_PAGE_TYPE *BPLUSTREE_TYPE::FindLeafPage(const KeyType &key, bool leftMost) {
  // NOTE: Leafmost is used for Interator location at beginning
  // Find leaf page by next page_id in internel page node
  if (IsEmpty()) {
    return nullptr;
  }
  auto page = FetchPage(root_page_id_);
  //  LOG_DEBUG("page size: %d", page->GetSize());
  page_id_t next;
  for (auto cur = root_page_id_; !page->IsLeafPage(); cur = next, page = FetchPage(cur)) {
    InternalPage *internal_page = static_cast<InternalPage *>(page);
    if (leftMost) {
      next = internal_page->ValueAt(0);
    } else {
      next = internal_page->Lookup(key, comparator_);
    }
    buffer_pool_manager_->UnpinPage(cur, false);
  }
  return static_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page);
}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record defualt value is false. When set to true,
 * insert a record <index_name, root_page_id> into header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  HeaderPage *header_page = reinterpret_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't  need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto leaf = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    InternalPage *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto leaf = reinterpret_cast<B_PLUS_TREE_LEAF_PAGE_TYPE *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    InternalPage *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
