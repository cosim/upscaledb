/*
 * Copyright (C) 2005-2014 Christoph Rupp (chris@crupp.de).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Base class for btree node implementations
 *
 * @exception_safe: unknown
 * @thread_safe: unknown
 */

#ifndef HAM_BTREE_IMPL_BASE_H
#define HAM_BTREE_IMPL_BASE_H

#include "0root/root.h"

#include <sstream>
#include <iostream>

// Always verify that a file of level N does not include headers > N!
#include "1globals/globals.h"
#include "1base/byte_array.h"
#include "2page/page.h"
#include "3btree/btree_node.h"

#ifndef HAM_ROOT_H
#  error "root.h was not included"
#endif

namespace hamsterdb {

template<typename KeyList, typename RecordList>
struct BaseNodeImpl
{
  // Constructor
  BaseNodeImpl(Page *page)
    : m_page(page), m_node(PBtreeNode::from_page(page)),
      m_estimated_capacity(0), m_keys(page->get_db()),
      m_records(page->get_db(), m_node) {
  }

  // Returns the estimated page's capacity
  size_t estimate_capacity() const {
    return (m_estimated_capacity);
  }

  // Checks this node's integrity
  virtual void check_integrity() const {
  }

  // Returns a copy of a key and stores it in |dest|
  void get_key(int slot, ByteArray *arena, ham_key_t *dest) {
    // copy (or assign) the key data
    m_keys.get_key(slot, arena, dest, true);
  }

  // Returns the record size of a key or one of its duplicates
  uint64_t get_record_size(int slot, int duplicate_index) {
    return (m_records.get_record_size(slot, duplicate_index));
  }

  // Returns the record counter of a key
  int get_record_count(int slot) {
    return (m_records.get_record_count(slot));
  }

  // Returns the full record and stores it in |dest|
  void get_record(int slot, ByteArray *arena, ham_record_t *record,
                  uint32_t flags, int duplicate_index) {
    // copy the record data
    m_records.get_record(slot, arena, record, flags, duplicate_index);
  }

  // Updates the record of a key
  void set_record(int slot, ham_record_t *record,
                  int duplicate_index, uint32_t flags,
                  uint32_t *new_duplicate_index) {
    // automatically overwrite an existing key unless this is a
    // duplicate operation
    if ((flags & (HAM_DUPLICATE
                  | HAM_DUPLICATE
                  | HAM_DUPLICATE_INSERT_BEFORE
                  | HAM_DUPLICATE_INSERT_AFTER
                  | HAM_DUPLICATE_INSERT_FIRST
                  | HAM_DUPLICATE_INSERT_LAST)) == 0)
      flags |= HAM_OVERWRITE;

    m_records.set_record(slot, duplicate_index, record, flags,
            new_duplicate_index);
  }

  // Erases the extended part of a key
  void erase_extended_key(int slot) {
    m_keys.erase_extended_key(slot);
  }

  // Erases the record
  void erase_record(int slot, int duplicate_index,
                  bool all_duplicates) {
    m_records.erase_record(slot, duplicate_index, all_duplicates);
  }

  // Erases a key
  void erase(int slot) {
    size_t node_count = m_node->get_count();

    m_keys.erase(node_count, slot);
    m_records.erase(node_count, slot);
  }

  // Inserts a new key
  void insert(int slot, const ham_key_t *key) {
    size_t node_count = m_node->get_count();

    // make space for 1 additional element.
    // only store the key data; flags and record IDs are set by the caller
    m_keys.insert(node_count, slot, key);
    m_records.insert(node_count, slot);
  }

  // Compares two keys using the supplied comparator
  template<typename Cmp>
  int compare(const ham_key_t *lhs, uint32_t rhs, Cmp &cmp) {
    if (KeyList::kHasSequentialData) {
      return (cmp(lhs->data, lhs->size, m_keys.get_key_data(rhs),
                              m_keys.get_key_size(rhs)));
    }
    else {
      ham_key_t tmp = {0};
      m_keys.get_key(rhs, &m_arena, &tmp, false);
      return (cmp(lhs->data, lhs->size, tmp.data, tmp.size));
    }
  }

  // Searches the node for the key and returns the slot of this key
  template<typename Cmp>
  int find_child(ham_key_t *key, Cmp &comparator, uint64_t *precord_id,
                  int *pcmp) {
    int slot = find_impl(key, comparator, pcmp);
    if (precord_id) {
      if (slot == -1)
        *precord_id = m_node->get_ptr_down();
      else
        *precord_id = m_records.get_record_id(slot);
    }
    return (slot);
  }

  // Searches the node for the key and returns the slot of this key
  // - only for exact matches!
  template<typename Cmp>
  int find_exact(ham_key_t *key, Cmp &comparator) {
    int cmp;
    int r = find_impl(key, comparator, &cmp);
    return (cmp ? -1 : r);
  }

  // Implementation of the find method; uses a linear search if possible
  template<typename Cmp>
  int find_impl(ham_key_t *key, Cmp &comparator, int *pcmp) {
    size_t node_count = m_node->get_count();
    ham_assert(node_count > 0);

    int i, l = 0, r = (int)node_count;
    int last = node_count + 1;
    int cmp = -1;

    // Run a binary search, but fall back to linear search as soon as
    // the remaining range is too small. Sets threshold to 0 if linear
    // search is disabled for this KeyList.
    int threshold = m_keys.get_linear_search_threshold();

    /* repeat till we found the key or the remaining range is so small that
     * we rather perform a linear search (which is faster for small ranges) */
    while (r - l > threshold) {
      /* get the median item; if it's identical with the "last" item,
       * we've found the slot */
      i = (l + r) / 2;

      if (i == last) {
        ham_assert(i >= 0);
        ham_assert(i < (int)node_count);
        *pcmp = 1;
        return (i);
      }

      /* compare it against the key */
      cmp = compare(key, i, comparator);

      /* found it? */
      if (cmp == 0) {
        *pcmp = cmp;
        return (i);
      }
      /* if the key is bigger than the item: search "to the left" */
      else if (cmp < 0) {
        if (r == 0) {
          ham_assert(i == 0);
          *pcmp = cmp;
          return (-1);
        }
        r = i;
      }
      /* otherwise search "to the right" */
      else {
        last = i;
        l = i;
      }
    }

    // still here? then perform a linear search for the remaining range
    ham_assert(r - l <= threshold);
    return (m_keys.linear_search(l, r - l, key, comparator, pcmp));
  }

  // Splits a node and moves parts of the current node into |other|, starting
  // at the |pivot| slot
  void split(BaseNodeImpl<KeyList, RecordList> *other, int pivot) {
    size_t node_count = m_node->get_count();
    size_t other_node_count = other->m_node->get_count();

    //
    // if a leaf page is split then the pivot element must be inserted in
    // the leaf page AND in the internal node. the internal node update
    // is handled by the caller.
    //
    // in internal nodes the pivot element is only propagated to the
    // parent node. the pivot element is skipped.
    //
    if (m_node->is_leaf()) {
      m_keys.copy_to(pivot, node_count, other->m_keys,
                      other_node_count, 0);
      m_records.copy_to(pivot, node_count, other->m_records,
                      other_node_count, 0);
    }
    else {
      m_keys.copy_to(pivot + 1, node_count, other->m_keys,
                      other_node_count, 0);
      m_records.copy_to(pivot + 1, node_count, other->m_records,
                      other_node_count, 0);
    }
  }

  // Returns true if the node requires a merge or a shift
  bool requires_merge() const {
    return (m_node->get_count() <= 3);
  }

  // Merges this node with the |other| node
  void merge_from(BaseNodeImpl<KeyList, RecordList> *other) {
    size_t node_count = m_node->get_count();
    size_t other_node_count = other->m_node->get_count();

    // shift items from the sibling to this page
    if (other_node_count > 0) {
      other->m_keys.copy_to(0, other_node_count, m_keys,
                      node_count, node_count);
      other->m_records.copy_to(0, other_node_count, m_records,
                      node_count, node_count);
    }
  }

  // Prints a slot to stdout (for debugging)
  void print(int slot) {
    std::stringstream ss;
    ss << "   ";
    m_keys.print(slot, ss);
    ss << " -> ";
    m_records.print(slot, ss);
    std::cout << ss.str() << std::endl;
  }

  // Returns the record id
  uint64_t get_record_id(int slot) const {
    return (m_records.get_record_id(slot));
  }

  // Sets the record id
  void set_record_id(int slot, uint64_t ptr) {
    m_records.set_record_id(slot, ptr);
  }

  // The page we're operating on
  Page *m_page;

  // The node we're operating on
  PBtreeNode *m_node;

  // Capacity of this node (maximum number of key/record pairs that
  // can be stored)
  size_t m_estimated_capacity;

  // for accessing the keys
  KeyList m_keys;

  // for accessing the records
  RecordList m_records;

  // A memory arena for various tasks
  ByteArray m_arena;
};

} // namespace hamsterdb

#endif /* HAM_BTREE_IMPL_BASE_H */
