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
 * The Cache Manager
 *
 * @exception_safe: nothrow
 * @thread_safe: no
 */

#ifndef HAM_CACHE_STATE_H
#define HAM_CACHE_STATE_H

#include "0root/root.h"

#include <vector>

#include "ham/hamsterdb_int.h"

// Always verify that a file of level N does not include headers > N!
#include "1base/spinlock.h"
#include "2page/page_collection.h"
#include "2config/env_config.h"
#include "3changeset/changeset.h"

#ifndef HAM_ROOT_H
#  error "root.h was not included"
#endif

namespace hamsterdb {

struct CacheState
{
  enum {
    // The number of buckets should be a prime number or similar, as it
    // is used in a MODULO hash scheme
    kBucketSize = 10317,

    // Lower bound limit for purging pages
    kPurgeAtLeast = 20
  };

  // The default constructor
  CacheState(const EnvironmentConfiguration &config)
    : capacity_bytes(config.flags & HAM_CACHE_UNLIMITED
                            ? 0xffffffffffffffffull
                            : config.cache_size_bytes),
      page_size_bytes(config.page_size_bytes),
      alloc_elements(0), totallist(Page::kListCache),
      buckets(kBucketSize, PageCollection(Page::kListBucket)),
      cache_hits(0), cache_misses(0) {
    ham_assert(capacity_bytes > 0);
  }

  // A fast spinlock
  Spinlock mutex;

  // The Changeset
  Changeset *changeset;

  // the capacity (in bytes)
  uint64_t capacity_bytes;

  // the current page size (in bytes)
  uint64_t page_size_bytes;

  // the current number of cached elements that were allocated (and not
  // mapped)
  size_t alloc_elements;

  // linked list of ALL cached pages
  PageCollection totallist;

  // The hash table buckets - each is a linked list of Page pointers
  std::vector<PageCollection> buckets;

  // counts the cache hits
  uint64_t cache_hits;

  // counts the cache misses
  uint64_t cache_misses;
};

} // namespace hamsterdb

#endif /* HAM_CACHE_STATE_H */
