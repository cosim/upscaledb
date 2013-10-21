/*
 * Copyright (C) 2005-2013 Christoph Rupp (chris@crupp.de).
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * See files COPYING.* for License information.
 */

#ifndef HAM_BTREE_NODE_PAX_H__
#define HAM_BTREE_NODE_PAX_H__

#include "util.h"
#include "page.h"
#include "btree_node.h"
#include "blob_manager.h"
#include "duplicates.h"
#include "env_local.h"

namespace hamsterdb {

template<typename KeyList, typename RecordList>
class PaxNodeLayout;

/*
 * A helper class to access (flags/key data/record data) values in a
 * BtreeNode with PAX-style layout
 */
template<typename KeyList, typename RecordList>
struct PaxIterator
{
  public:
    // Constructor
    PaxIterator(PaxNodeLayout<KeyList, RecordList> *node, ham_size_t slot)
      : m_node(node), m_slot(slot) {
    }

    // Constructor
    PaxIterator(const PaxNodeLayout<KeyList, RecordList> *node, ham_size_t slot)
      : m_node((PaxNodeLayout<KeyList, RecordList> *)node), m_slot(slot) {
    }

    // Returns true if the record is inline
    bool is_record_inline() const {
      return (m_node->is_record_inline(m_slot));
    }

    // Returns the maximum size of inline records
    ham_size_t get_max_inline_record_size() const {
      return (m_node->get_max_inline_record_size());
    }

    // Removes an inline record
    void remove_record_inline() {
      ham_assert(is_record_inline() == true);
      m_node->remove_record_inline(m_slot);
    }

    // Returns the size of the record, if inline
    ham_size_t get_inline_record_size() const {
      return (m_node->get_inline_record_size(m_slot));
    }

    // Returns a pointer to the record's inline data
    void *get_inline_record_data() {
      ham_assert(is_record_inline() == true);
      return (m_node->get_record_data(m_slot));
    }

    // Returns a pointer to the record's inline data
    const void *get_inline_record_data() const {
      ham_assert(is_record_inline() == true);
      return (m_node->get_record_data(m_slot));
    }

    // Returns the record id
    ham_u64_t get_record_id() const {
      return (ham_db2h_offset(*m_node->get_record_data(m_slot)));
    }

    // Sets the record data
    void set_inline_record_data(const void *ptr, ham_size_t size) {
      m_node->set_record_data(m_slot, ptr, size);
    }

    // Sets the record id
    void set_record_id(ham_u64_t ptr) {
      m_node->set_record_id(m_slot, ham_h2db_offset(ptr));
    }

    // Returns the (persisted) flags of a key
    ham_u8_t get_flags() const {
      return (m_node->get_flags(m_slot));
    }

    // Sets the flags of a key (BtreeKey::kBlobSizeTiny etc)
    void set_flags(ham_u8_t flags) {
      m_node->set_flags(m_slot, flags);
    }

    // Returns the size of a btree key
    ham_u16_t get_key_size() const {
      return (m_node->get_key_size());
    }

    // Sets the size of a btree key
    void set_key_size(ham_u16_t size) {
      ham_assert(size == get_key_size());
    }

    // Returns a pointer to the key data
    ham_u8_t *get_key_data() {
      return (m_node->get_key_data(m_slot));
    }

    // Returns a pointer to the key data
    ham_u8_t *get_key_data() const {
      return (m_node->get_key_data(m_slot));
    }

    // Overwrites the key data
    void set_key_data(const void *ptr, ham_size_t size) {
      ham_assert(size == get_key_size());
      m_node->set_key_data(m_slot, ptr, size);
    }

    // Returns the record address of an extended key overflow area
    ham_u64_t get_extended_rid(LocalDatabase *db) const {
      // nop, but required to compile
      ham_assert(!"shouldn't be here");
      return (0);
    }

    // Sets the record address of an extended key overflow area
    void set_extended_rid(LocalDatabase *db, ham_u64_t rid) {
      // nop, but required to compile
      ham_assert(!"shouldn't be here");
    }

    // Moves this Iterator to the next key
    PaxIterator<KeyList, RecordList> next() {
      return (PaxIterator<KeyList, RecordList>(m_node, m_slot + 1));
    }

    // Moves this Iterator to the next key
    PaxIterator<KeyList, RecordList> next() const {
      return (PaxIterator<KeyList, RecordList>(m_node, m_slot + 1));
    }

    // Allows use of operator-> in the caller
    PaxIterator<KeyList, RecordList> *operator->() {
      return (this);
    }

    // Allows use of operator-> in the caller
    const PaxIterator<KeyList, RecordList> *operator->() const {
      return (this);
    }

  private:
    // The node of this iterator
    PaxNodeLayout<KeyList, RecordList> *m_node;

    // The current slot in the node
    ham_size_t m_slot;
};

//
// A template class managing an array of POD types
//
template<typename T>
class PodKeyList
{
  public:
    PodKeyList(LocalDatabase *db, ham_u8_t *data)
      : m_data((T *)data) {
    }

    ham_size_t get_key_size() const {
      return (sizeof(T));
    }

    ham_u8_t *get_key_data(int slot) {
      return ((ham_u8_t *)&m_data[slot]);
    }

    ham_u8_t *get_key_data(int slot) const {
      return ((ham_u8_t *)&m_data[slot]);
    }

    void set_key_data(int slot, const void *ptr, ham_size_t size) {
      ham_assert(size == get_key_size());
      m_data[slot] = *(T *)ptr;
    }

  private:
    T *m_data;
};

//
// A template class managing an array of fixed length bytes
//
class BinaryKeyList
{
  public:
    BinaryKeyList(LocalDatabase *db, ham_u8_t *data)
      : m_data(data) {
      m_key_size = db->get_key_size();
      ham_assert(m_key_size != 0);
    }

    ham_size_t get_key_size() const {
      return (m_key_size);
    }

    ham_u8_t *get_key_data(int slot) {
      return (&m_data[slot * m_key_size]);
    }

    ham_u8_t *get_key_data(int slot) const {
      return (&m_data[slot * m_key_size]);
    }

    void set_key_data(int slot, const void *ptr, ham_size_t size) {
      ham_assert(size == get_key_size());
      memcpy(&m_data[slot * m_key_size], ptr, size);
    }

  private:
    ham_u8_t *m_data;
    ham_size_t m_key_size;
};


//
// A proxy class handling access to records of non-fixed size//
//
class DefaultRecordList
{
  public:
    DefaultRecordList(LocalDatabase *db)
      : m_data(0) {
    }

    // Returns the maximum size of an inline record
    ham_size_t get_max_inline_record_size() const {
      return (sizeof(ham_u64_t));
    }

    // Returns true if the record is inline
    bool is_record_inline(int slot, ham_u8_t flags) const {
      return ((flags & BtreeKey::kBlobSizeTiny)
              || (flags & BtreeKey::kBlobSizeSmall)
              || (flags & BtreeKey::kBlobSizeEmpty) != 0);
    }

    // Returns the size of an inline record
    ham_size_t get_inline_record_size(int slot, ham_u32_t flags) const {
      ham_assert(is_record_inline(slot, flags));
      if (flags & BtreeKey::kBlobSizeTiny) {
        /* the highest byte of the record id is the size of the blob */
        char *p = (char *)get_record_data(slot);
        return (p[sizeof(ham_u64_t) - 1]);
      }
      else if (flags & BtreeKey::kBlobSizeSmall)
        return (sizeof(ham_u64_t));
      else if (flags & BtreeKey::kBlobSizeEmpty)
        return (0);
      else
        ham_assert(!"shouldn't be here");
      return (0);
    }

    // Sets the data pointer
    void set_data_pointer(void *ptr) {
      m_data = (ham_u64_t *)ptr;
    }

    // Returns the fixed record size
    ham_size_t get_record_size() const {
      return (sizeof(ham_u64_t));
    }

    // Returns data to a specific record
    void *get_record_data(int slot) {
      return (&m_data[slot]);
    }

    // Returns data to a specific record
    const void *get_record_data(int slot) const {
      return (&m_data[slot]);
    }

    // Sets the record id
    void set_record_id(int slot, ham_u64_t ptr) {
      m_data[slot] = ptr;
    }

    // Sets record data; returns the new flags
    ham_u32_t set_record_data(int slot, ham_u32_t flags, const void *ptr,
                    ham_size_t size) {
      flags &= ~(BtreeKey::kBlobSizeSmall
                      | BtreeKey::kBlobSizeTiny
                      | BtreeKey::kBlobSizeEmpty);

      if (size == 0) {
        m_data[slot] = 0;
        return (flags | BtreeKey::kBlobSizeEmpty);
      }
      if (size < 8) {
        /* the highest byte of the record id is the size of the blob */
        char *p = (char *)&m_data[slot];
        p[sizeof(ham_u64_t) - 1] = size;
        memcpy(&m_data[slot], ptr, size);
        return (flags | BtreeKey::kBlobSizeTiny);
      }
      if (size == 8) {
        memcpy(&m_data[slot], ptr, size);
        return (flags | BtreeKey::kBlobSizeSmall);
      }
      ham_assert(!"shouldn't be here");
      return (flags);
    }

    // Clears a record
    void reset(int slot) {
      m_data[slot] = 0;
    }

    // Removes an inline record
    ham_u32_t remove_record_inline(int slot, ham_u32_t flags) {
      m_data[slot] = 0;
      return (flags);
    }

  private:
    ham_u64_t *m_data;
};


//
// A proxy class handling access to inline records of internal nodes
//
class InternalRecordList
{
  public:
    InternalRecordList(LocalDatabase *db)
      : m_data(0) {
    }

    // Returns the maximum size of an inline record
    ham_size_t get_max_inline_record_size() const {
      return (sizeof(ham_u64_t));
    }

    // Returns true if the record is inline
    bool is_record_inline(int slot, ham_u8_t flags) const {
      return (true);
    }

    // Returns the size of an inline record
    ham_size_t get_inline_record_size(int slot, ham_u32_t flags) const {
      return (get_record_size());
    }

    // Sets the data pointer
    void set_data_pointer(void *ptr) {
      m_data = (ham_u64_t *)ptr;
    }

    // Returns the fixed record size
    ham_size_t get_record_size() const {
      return (sizeof(ham_u64_t));
    }

    // Returns data to a specific record
    void *get_record_data(int slot) {
      return (&m_data[slot]);
    }

    // Returns data to a specific record
    const void *get_record_data(int slot) const {
      return (&m_data[slot]);
    }

    // Removes an inline record
    ham_u32_t remove_record_inline(int slot, ham_u32_t flags) {
      flags &= ~(BtreeKey::kBlobSizeSmall
                      | BtreeKey::kBlobSizeTiny
                      | BtreeKey::kBlobSizeEmpty
                      | BtreeKey::kDuplicates);
      m_data[slot] = 0;
      return (flags);
    }

    // Sets the record id
    void set_record_id(int slot, ham_u64_t ptr) {
      m_data[slot] = ptr;
    }

    // Sets record data
    ham_u32_t set_record_data(int slot, ham_u32_t flags, const void *ptr,
                    ham_size_t size) {
      flags &= ~(BtreeKey::kBlobSizeSmall
                      | BtreeKey::kBlobSizeTiny
                      | BtreeKey::kBlobSizeEmpty);
      ham_assert(size == get_record_size());
      m_data[slot] = *(ham_u64_t *)ptr;
      return (flags);
    }

    // Clears a record
    void reset(int slot) {
      m_data[slot] = 0;
    }

  private:
    ham_u64_t *m_data;
};


//
// A proxy class handling access to inline records with fixed length (for btree
// leafs storing the actual record data)
//
class InlineRecordList
{
  public:
    InlineRecordList(LocalDatabase *db)
      : m_data(0), m_record_size(db->get_record_size()) {
      ham_assert(m_record_size != HAM_RECORD_SIZE_UNLIMITED);
    }

    // Returns the maximum size of an inline record
    ham_size_t get_max_inline_record_size() const {
      return (m_record_size);
    }

    // Returns true if the record is inline
    bool is_record_inline(int slot, ham_u8_t flags) const {
      return (true);
    }

    // Returns the size of an inline record
    ham_size_t get_inline_record_size(int slot, ham_u32_t flags) const {
      return (get_record_size());
    }

    // Sets the data pointer
    void set_data_pointer(void *ptr) {
      m_data = (ham_u8_t *)ptr;
    }

    // Returns the fixed record size
    ham_size_t get_record_size() const {
      return (m_record_size);
    }

    // Returns data to a specific record
    void *get_record_data(int slot) {
      return (&m_data[slot * m_record_size]);
    }

    // Returns data to a specific record
    const void *get_record_data(int slot) const {
      return (&m_data[slot * m_record_size]);
    }

    // Sets the record id
    void set_record_id(int slot, ham_u64_t ptr) {
      ham_assert(!"shouldn't be here");
    }

    // Sets record data
    ham_u32_t set_record_data(int slot, ham_u32_t flags, const void *ptr,
                    ham_size_t size) {
      ham_assert(size == get_record_size());
      if (size)
        memcpy(&m_data[m_record_size * slot], ptr, size);
      return (flags);
    }

    // Clears a record
    void reset(int slot) {
      if (m_record_size)
        memset(&m_data[m_record_size * slot], 0, m_record_size);
    }

    // Removes an inline record
    ham_u32_t remove_record_inline(int slot, ham_u32_t flags) {
      if (m_record_size)
        memset(&m_data[m_record_size * slot], 0, m_record_size);
      return (flags);
    }

  private:
    ham_u8_t *m_data;
    ham_size_t m_record_size;
};


//
// A BtreeNodeProxy layout which stores key data, key flags and
// and the record pointers in a PAX style layout.
//
template<typename KeyList, typename RecordList>
class PaxNodeLayout
{
  public:
    typedef PaxIterator<KeyList, RecordList> Iterator;
    typedef const PaxIterator<KeyList, RecordList> ConstIterator;

    PaxNodeLayout(Page *page)
      : m_page(page), m_node(PBtreeNode::from_page(page)),
        m_keys(page->get_db(), m_node->get_data()),
        m_records(page->get_db()) {
      ham_size_t usable_nodesize = page->get_env()->get_pagesize()
                    - PBtreeNode::get_entry_offset()
                    - Page::sizeof_persistent_header;
      ham_size_t keysize = get_system_keysize(m_keys.get_key_size());
      m_max_count = usable_nodesize / (keysize + m_records.get_record_size());

      ham_u8_t *p = m_node->get_data();
      m_flags = &p[m_max_count * get_key_size()];
      m_records.set_data_pointer(&p[m_max_count * (1 + get_key_size())]);
    }

    // Returns the actual key size (including overhead, without record)
    static ham_u16_t get_system_keysize(ham_size_t keysize) {
      return ((ham_u16_t)(keysize + 1));
    }

    Iterator begin() {
      return (at(0));
    }

    Iterator begin() const {
      return (at(0));
    }

    Iterator at(int slot) {
      return (Iterator(this, slot));
    }

    ConstIterator at(int slot) const {
      return (ConstIterator(this, slot));
    }

    Iterator next(Iterator it) {
      return (it->next());
    }

    ConstIterator next(ConstIterator it) const {
      return (it->next());
    }

    void release_key(Iterator it) {
    }

    ham_status_t copy_full_key(ConstIterator it, ByteArray *arena,
                    ham_key_t *dest) const {
      LocalDatabase *db = m_page->get_db();

      if (!(dest->flags & HAM_KEY_USER_ALLOC)) {
        if (!arena->resize(get_key_size()))
          return (HAM_OUT_OF_MEMORY);
        dest->data = arena->get_ptr();
        dest->size = get_key_size();
      }

      ham_assert(get_key_size() == db->get_key_size());
      memcpy(dest->data, it->get_key_data(), get_key_size());
      return (0);
    }

    ham_status_t check_integrity(Iterator it, BlobManager *bm) const {
      return (0);
    }

    template<typename Cmp>
    int compare(const ham_key_t *lhs, Iterator it, Cmp &cmp) {
      return (cmp(lhs->data, lhs->size, it->get_key_data(), get_key_size()));
    }

    void split(PaxNodeLayout *other, int pivot) {
      ham_size_t count = m_node->get_count();

      /*
       * if a leaf page is split then the pivot element must be inserted in
       * the leaf page AND in the internal node. the internal node update
       * is handled by the caller.
       *
       * in internal nodes the pivot element is only propagated to the
       * parent node. the pivot element is skipped.
       */
      if (m_node->is_leaf()) {
        memcpy(other->m_keys.get_key_data(0), m_keys.get_key_data(pivot),
                    get_key_size() * (count - pivot));
        memcpy(&other->m_flags[0], &m_flags[pivot],
                    count - pivot);
        memcpy(other->m_records.get_record_data(0),
                    m_records.get_record_data(pivot),
                    m_records.get_record_size() * (count - pivot));
      }
      else {
        memcpy(other->m_keys.get_key_data(0), m_keys.get_key_data(pivot + 1),
                    get_key_size() * (count - pivot - 1));
        memcpy(&other->m_flags[0], &m_flags[pivot + 1],
                    count - pivot - 1);
        memcpy(other->m_records.get_record_data(0),
                    m_records.get_record_data(pivot + 1),
                    m_records.get_record_size() * (count - pivot - 1));
      }
    }

    Iterator insert(ham_u32_t slot, const ham_key_t *key) {
      ham_assert(key->size == get_key_size());

      ham_size_t count = m_node->get_count();

      // make space for 1 additional element.
      // only store the key data; flags and record IDs are set by the caller
      if (count > slot) {
        memmove(m_keys.get_key_data(slot + 1), m_keys.get_key_data(slot),
                        get_key_size() * (count - slot));
        m_keys.set_key_data(slot, key->data, key->size);
        memmove(&m_flags[slot + 1], &m_flags[slot],
                        count - slot);
        m_flags[slot] = 0;
        memmove(m_records.get_record_data(slot + 1),
                        m_records.get_record_data(slot),
                        m_records.get_record_size() * (count - slot));
        m_records.reset(slot);
      }
      else {
        m_keys.set_key_data(slot, key->data, key->size);
        m_flags[slot] = 0;
        m_records.reset(slot);
      }

      return (at(slot));
    }

    void make_space(ham_u32_t slot) {
      ham_size_t count = m_node->get_count();

      // make space for 1 additional element.
      if (count > slot) {
        memmove(m_keys.get_key_data(slot + 1), m_keys.get_key_data(slot),
                        get_key_size() * (count - slot));
        memmove(&m_flags[slot + 1], &m_flags[slot],
                        count - slot);
        m_flags[slot] = 0;
        memmove(m_records.get_record_data(slot + 1),
                        m_records.get_record_data(slot),
                        m_records.get_record_size() * (count - slot));
        m_records.reset(slot);
      }
    }

    void remove(ham_u32_t slot) {
      ham_size_t count = m_node->get_count();

      if (slot != count - 1) {
        memmove(m_keys.get_key_data(slot), m_keys.get_key_data(slot + 1),
                get_key_size() * (count - slot - 1));
        memmove(&m_flags[slot], &m_flags[slot + 1],
                count - slot - 1);
        memmove(m_records.get_record_data(slot),
                m_records.get_record_data(slot + 1),
                m_records.get_record_size() * (count - slot - 1));
      }
    }

    void merge_from(PaxNodeLayout *other) {
      ham_size_t count = m_node->get_count();

      /* shift items from the sibling to this page */
      memcpy(m_keys.get_key_data(count), other->m_keys.get_key_data(0),
                      get_key_size() * other->m_node->get_count());
      memcpy(&m_flags[count], &other->m_flags[0],
                      other->m_node->get_count());
      memcpy(m_records.get_record_data(count),
                      other->m_records.get_record_data(0),
                      m_records.get_record_size() * other->m_node->get_count());
    }

    void shift_from_right(PaxNodeLayout *other, int count) {
      ham_size_t pos = m_node->get_count();

      // shift |count| elements from |other| to this page
      memcpy(m_keys.get_key_data(pos), other->m_keys.get_key_data(0),
                      get_key_size() * count);
      memcpy(&m_flags[pos], &other->m_flags[0],
                      count);
      memcpy(m_records.get_record_data(pos),
                      other->m_records.get_record_data(0),
                      m_records.get_record_size() * count);

      // and reduce the other page
      memmove(other->m_keys.get_key_data(0), other->m_keys.get_key_data(count),
                      get_key_size() * (other->m_node->get_count() - count));
      memmove(&other->m_flags[0], &other->m_flags[count],
                      (other->m_node->get_count() - count));
      memmove(other->m_records.get_record_data(0),
                      other->m_records.get_record_data(count),
                      m_records.get_record_size()
                            * (other->m_node->get_count() - count));
    }

    void shift_to_right(PaxNodeLayout *other, int slot, int count) {
      // make room in the right sibling
      memmove(other->m_keys.get_key_data(count), other->m_keys.get_key_data(0),
                      get_key_size() * other->m_node->get_count());
      memmove(&other->m_flags[count], &other->m_flags[0],
                      other->m_node->get_count());
      memmove(other->m_records.get_record_data(count),
                      other->m_records.get_record_data(0),
                      m_records.get_record_size() * other->m_node->get_count());

      // shift |count| elements from this page to |other|
      memcpy(other->m_keys.get_key_data(0), m_keys.get_key_data(slot),
                      get_key_size() * count);
      memcpy(&other->m_flags[0], &m_flags[slot],
                      count);
      memcpy(other->m_records.get_record_data(0),
                      m_records.get_record_data(slot),
                      m_records.get_record_size() * count);
    }

  private:
    friend struct PaxIterator<KeyList, RecordList>;

    // Returns the BtreeKey at index |i| in this node
    //
    // note that this function does not check the boundaries (i.e. whether
    // i <= get_count(), because some functions deliberately write to
    // elements "after" get_count()
    Iterator *get_iterator(LocalDatabase *db, int i) {
      return (Iterator(this, i));
    }

    // Same as above, const flavor
    ConstIterator *get_iterator(LocalDatabase *db, int i) const {
      return (ConstIterator(this, i));
    }

    // Returns the key size
    ham_size_t get_key_size() const {
      return (m_keys.get_key_size());
    }

    // Returns the flags of a key
    ham_u8_t get_flags(int slot) const {
      return (m_flags[slot]);
    }

    // Sets the flags of a key
    void set_flags(int slot, ham_u8_t flags) {
      m_flags[slot] = flags;
    }

    // Returns a pointer to the key data
    ham_u8_t *get_key_data(int slot) const {
      return (m_keys.get_key_data(slot));
    }

    // Sets the key data
    void set_key_data(int slot, const void *ptr, ham_size_t size) {
      m_keys.set_key_data(slot, ptr, size);
    }

    // Returns true if the record is inline
    bool is_record_inline(int slot) const {
      return (m_records.is_record_inline(slot, get_flags(slot)));
    }

    // Returns the maximum size of an inline record
    ham_size_t get_max_inline_record_size() const {
      return (m_records.get_max_inline_record_size());
    }

    // Returns the size of an inline record
    ham_size_t get_inline_record_size(int slot) const {
      ham_assert(is_record_inline(slot) == true);
      return (m_records.get_inline_record_size(slot, get_flags(slot)));
    }

    // Removes an inline record
    void remove_record_inline(int slot) {
      m_flags[slot] = m_records.remove_record_inline(slot, m_flags[slot]);
    }

    // Returns a pointer to the record id
    ham_u64_t *get_record_data(int slot) {
      return ((ham_u64_t *)m_records.get_record_data(slot));
    }

    // Returns a pointer to the record id
    const ham_u64_t *get_record_data(int slot) const {
      return ((ham_u64_t *)m_records.get_record_data(slot));
    }

    // Sets the record id
    void set_record_id(int slot, ham_u64_t ptr) {
      m_records.set_record_id(slot, ptr);
    }

    // Sets the record data
    void set_record_data(int slot, const void *ptr, ham_size_t size) {
      m_flags[slot] = m_records.set_record_data(slot, m_flags[slot], ptr, size);
    }

    Page *m_page;
    PBtreeNode *m_node;
    ham_size_t m_max_count;
    ham_u8_t *m_flags;
    KeyList m_keys;
    RecordList m_records;
};

} // namespace hamsterdb

#endif /* HAM_BTREE_NODE_PAX_H__ */
