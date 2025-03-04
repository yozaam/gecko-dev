/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozmemory.h"
#include "nsCOMPtr.h"
#include "nsICrashReporter.h"
#include "nsServiceManagerUtils.h"
#include "Utils.h"

#include "gtest/gtest.h"


#if defined(DEBUG) && !defined(XP_WIN) && !defined(ANDROID)
#define HAS_GDB_SLEEP_DURATION 1
extern unsigned int _gdb_sleep_duration;
#endif

// Death tests are too slow on OSX because of the system crash reporter.
#ifndef XP_DARWIN
static void DisableCrashReporter()
{
  nsCOMPtr<nsICrashReporter> crashreporter =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashreporter) {
    crashreporter->SetEnabled(false);
  }
}

// Wrap ASSERT_DEATH_IF_SUPPORTED to disable the crash reporter
// when entering the subprocess, so that the expected crashes don't
// create a minidump that the gtest harness will interpret as an error.
#define ASSERT_DEATH_WRAP(a, b) \
  ASSERT_DEATH_IF_SUPPORTED({ DisableCrashReporter(); a; }, b)
#else
#define ASSERT_DEATH_WRAP(a, b)
#endif


using namespace mozilla;

static inline void
TestOne(size_t size)
{
  size_t req = size;
  size_t adv = malloc_good_size(req);
  char* p = (char*)malloc(req);
  size_t usable = moz_malloc_usable_size(p);
  // NB: Using EXPECT here so that we still free the memory on failure.
  EXPECT_EQ(adv, usable) <<
         "malloc_good_size(" << req << ") --> " << adv << "; "
         "malloc_usable_size(" << req << ") --> " << usable;
  free(p);
}

static inline void
TestThree(size_t size)
{
  ASSERT_NO_FATAL_FAILURE(TestOne(size - 1));
  ASSERT_NO_FATAL_FAILURE(TestOne(size));
  ASSERT_NO_FATAL_FAILURE(TestOne(size + 1));
}

TEST(Jemalloc, UsableSizeInAdvance)
{
  /*
   * Test every size up to a certain point, then (N-1, N, N+1) triplets for a
   * various sizes beyond that.
   */

  for (size_t n = 0; n < 16_KiB; n++)
    ASSERT_NO_FATAL_FAILURE(TestOne(n));

  for (size_t n = 16_KiB; n < 1_MiB; n += 4_KiB)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));

  for (size_t n = 1_MiB; n < 8_MiB; n += 128_KiB)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));
}

static int gStaticVar;

bool InfoEq(jemalloc_ptr_info_t& aInfo, PtrInfoTag aTag, void* aAddr,
            size_t aSize)
{
  return aInfo.tag == aTag && aInfo.addr == aAddr && aInfo.size == aSize;
}

bool InfoEqFreedPage(jemalloc_ptr_info_t& aInfo, void* aAddr, size_t aPageSize)
{
  size_t pageSizeMask = aPageSize - 1;

  return jemalloc_ptr_is_freed_page(&aInfo) &&
         aInfo.addr == (void*)(uintptr_t(aAddr) & ~pageSizeMask) &&
         aInfo.size == aPageSize;
}

TEST(Jemalloc, PtrInfo)
{
  // Some things might be running in other threads, so ensure our assumptions
  // (e.g. about isFreedSmall and isFreedPage ratios below) are not altered by
  // other threads.
  jemalloc_thread_local_arena(true);

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  jemalloc_ptr_info_t info;
  Vector<char*> small, large, huge;

  // For small (<= 2KiB) allocations, test every position within many possible
  // sizes.
  size_t small_max = stats.page_size / 2;
  for (size_t n = 0; n <= small_max; n += 8) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(small.append(p));
    for (size_t j = 0; j < usable; j++) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveSmall, p, usable));
    }
  }

  // Similar for large (2KiB + 1 KiB .. 1MiB - 8KiB) allocations.
  for (size_t n = small_max + 1_KiB; n <= stats.large_max; n += 1_KiB) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(large.append(p));
    for (size_t j = 0; j < usable; j += 347) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveLarge, p, usable));
    }
  }

  // Similar for huge (> 1MiB - 8KiB) allocations.
  for (size_t n = stats.chunksize; n <= 10_MiB; n += 512_KiB) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(huge.append(p));
    for (size_t j = 0; j < usable; j += 567) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveHuge, p, usable));
    }
  }

  // The following loops check freed allocations. We step through the vectors
  // using prime-sized steps, which gives full coverage of the arrays while
  // avoiding deallocating in the same order we allocated.
  size_t len;

  // Free the small allocations and recheck them.
  int isFreedSmall = 0, isFreedPage = 0;
  len = small.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 19) % len) {
    char* p = small[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k++) {
      jemalloc_ptr_info(&p[k], &info);
      // There are two valid outcomes here.
      if (InfoEq(info, TagFreedSmall, p, usable)) {
        isFreedSmall++;
      } else if (InfoEqFreedPage(info, &p[k], stats.page_size)) {
        isFreedPage++;
      } else {
        ASSERT_TRUE(false);
      }
    }
  }
  // There should be both FreedSmall and FreedPage results, but a lot more of
  // the former.
  ASSERT_TRUE(isFreedSmall != 0);
  ASSERT_TRUE(isFreedPage != 0);
  ASSERT_TRUE(isFreedSmall / isFreedPage > 10);

  // Free the large allocations and recheck them.
  len = large.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 31) % len) {
    char* p = large[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k += 357) {
      jemalloc_ptr_info(&p[k], &info);
      ASSERT_TRUE(InfoEqFreedPage(info, &p[k], stats.page_size));
    }
  }

  // Free the huge allocations and recheck them.
  len = huge.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 7) % len) {
    char* p = huge[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k += 587) {
      jemalloc_ptr_info(&p[k], &info);
      ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
    }
  }

  // Null ptr.
  jemalloc_ptr_info(nullptr, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Near-null ptr.
  jemalloc_ptr_info((void*)0x123, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Maximum address.
  jemalloc_ptr_info((void*)uintptr_t(-1), &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Stack memory.
  int stackVar;
  jemalloc_ptr_info(&stackVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Code memory.
  jemalloc_ptr_info((const void*)&jemalloc_ptr_info, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Static memory.
  jemalloc_ptr_info(&gStaticVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Chunk header.
  UniquePtr<int> p = MakeUnique<int>();
  size_t chunksizeMask = stats.chunksize - 1;
  char* chunk = (char*)(uintptr_t(p.get()) & ~chunksizeMask);
  size_t chunkHeaderSize = stats.chunksize - stats.large_max;
  for (size_t i = 0; i < chunkHeaderSize; i += 64) {
    jemalloc_ptr_info(&chunk[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
  }

  // Run header.
  size_t page_sizeMask = stats.page_size - 1;
  char* run = (char*)(uintptr_t(p.get()) & ~page_sizeMask);
  for (size_t i = 0; i < 4 * sizeof(void*); i++) {
    jemalloc_ptr_info(&run[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
  }

  // Entire chunk. It's impossible to check what is put into |info| for all of
  // these addresses; this is more about checking that we don't crash.
  for (size_t i = 0; i < stats.chunksize; i += 256) {
    jemalloc_ptr_info(&chunk[i], &info);
  }

  jemalloc_thread_local_arena(false);
}

size_t sSizes[] = { 1,      42,      79,      918,     1.5_KiB,
                    73_KiB, 129_KiB, 1.1_MiB, 2.6_MiB, 5.1_MiB };

TEST(Jemalloc, Arenas)
{
  arena_id_t arena = moz_create_arena();
  ASSERT_TRUE(arena != 0);
  void* ptr = moz_arena_malloc(arena, 42);
  ASSERT_TRUE(ptr != nullptr);
  ptr = moz_arena_realloc(arena, ptr, 64);
  ASSERT_TRUE(ptr != nullptr);
  moz_arena_free(arena, ptr);
  ptr = moz_arena_calloc(arena, 24, 2);
  // For convenience, free can be used to free arena pointers.
  free(ptr);
  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(arena);

#ifdef HAS_GDB_SLEEP_DURATION
  // Avoid death tests adding some unnecessary (long) delays.
  unsigned int old_gdb_sleep_duration = _gdb_sleep_duration;
  _gdb_sleep_duration = 0;
#endif

  // Can't use an arena after it's disposed.
  // ASSERT_DEATH_WRAP(moz_arena_malloc(arena, 80), "");

  // Arena id 0 can't be used to somehow get to the main arena.
  ASSERT_DEATH_WRAP(moz_arena_malloc(0, 80), "");

  arena = moz_create_arena();
  arena_id_t arena2 = moz_create_arena();
  // Ensure arena2 is used to prevent OSX errors:
  (void)arena2;

  // For convenience, realloc can also be used to reallocate arena pointers.
  // The result should be in the same arena. Test various size class transitions.
  for (size_t from_size : sSizes) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
      ptr = moz_arena_malloc(arena, from_size);
      ptr = realloc(ptr, to_size);
      // Freeing with the wrong arena should crash.
      ASSERT_DEATH_WRAP(moz_arena_free(arena2, ptr), "");
      // Likewise for moz_arena_realloc.
      ASSERT_DEATH_WRAP(moz_arena_realloc(arena2, ptr, from_size), "");
      // The following will crash if it's not in the right arena.
      moz_arena_free(arena, ptr);
    }
  }

  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(arena2);
  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(arena);

#ifdef HAS_GDB_SLEEP_DURATION
  _gdb_sleep_duration = old_gdb_sleep_duration;
#endif
}

// Check that a buffer aPtr is entirely filled with a given character from
// aOffset to aSize. For faster comparison, the caller is required to fill a
// reference buffer with the wanted character, and give the size of that
// reference buffer.
static void
bulk_compare(char* aPtr,
             size_t aOffset,
             size_t aSize,
             char* aReference,
             size_t aReferenceSize)
{
  for (size_t i = aOffset; i < aSize; i += aReferenceSize) {
    size_t length = std::min(aSize - i, aReferenceSize);
    if (memcmp(aPtr + i, aReference, length)) {
      // We got a mismatch, we now want to report more precisely where.
      for (size_t j = i; j < i + length; j++) {
        ASSERT_EQ(aPtr[j], *aReference);
      }
    }
  }
}

// A range iterator for size classes between two given values.
class SizeClassesBetween
{
public:
  SizeClassesBetween(size_t aStart, size_t aEnd)
    : mStart(aStart)
    , mEnd(aEnd)
  {
  }

  class Iterator
  {
  public:
    explicit Iterator(size_t aValue)
      : mValue(malloc_good_size(aValue))
    {
    }

    operator size_t() const { return mValue; }
    size_t operator*() const { return mValue; }
    Iterator& operator++()
    {
      mValue = malloc_good_size(mValue + 1);
      return *this;
    }

  private:
    size_t mValue;
  };

  Iterator begin() { return Iterator(mStart); }
  Iterator end() { return Iterator(mEnd); }

private:
  size_t mStart, mEnd;
};

#define ALIGNMENT_CEILING(s, alignment)                                        \
  (((s) + (alignment - 1)) & (~(alignment - 1)))

static bool
IsSameRoundedHugeClass(size_t aSize1, size_t aSize2, jemalloc_stats_t& aStats)
{
  return (aSize1 > aStats.large_max && aSize2 > aStats.large_max &&
          ALIGNMENT_CEILING(aSize1, aStats.chunksize) ==
            ALIGNMENT_CEILING(aSize2, aStats.chunksize));
}

static bool
CanReallocInPlace(size_t aFromSize, size_t aToSize, jemalloc_stats_t& aStats)
{
  if (aFromSize == malloc_good_size(aToSize)) {
    // Same size class: in-place.
    return true;
  }
  if (aFromSize >= aStats.page_size && aFromSize <= aStats.large_max &&
      aToSize >= aStats.page_size && aToSize <= aStats.large_max) {
    // Any large class to any large class: in-place when there is space to.
    return true;
  }
  if (IsSameRoundedHugeClass(aFromSize, aToSize, aStats)) {
    // Huge sizes that round up to the same multiple of the chunk size:
    // in-place.
    return true;
  }
  return false;
}

TEST(Jemalloc, InPlace)
{
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Using a separate arena, which is always emptied after an iteration, ensures
  // that in-place reallocation happens in all cases it can happen. This test is
  // intended for developers to notice they may have to adapt other tests if
  // they change the conditions for in-place reallocation.
  arena_id_t arena = moz_create_arena();

  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
      char* ptr = (char*)moz_arena_malloc(arena, from_size);
      char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
      if (CanReallocInPlace(from_size, to_size, stats)) {
        EXPECT_EQ(ptr, ptr2);
      } else {
        EXPECT_NE(ptr, ptr2);
      }
      moz_arena_free(arena, ptr2);
    }
  }

  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(arena);
}

// Bug 1474254: disable this test for windows ccov builds because it leads to timeout.
#if !defined(XP_WIN) || !defined(MOZ_CODE_COVERAGE)
TEST(Jemalloc, JunkPoison)
{
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Create buffers in a separate arena, for faster comparisons with
  // bulk_compare.
  arena_id_t buf_arena = moz_create_arena();
  char* junk_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  // Depending on its configuration, the allocator will either fill the
  // requested allocation with the junk byte (0xe4) or with zeroes, or do
  // nothing, in which case, since we're allocating in a fresh arena,
  // we'll be getting zeroes.
  char junk = stats.opt_junk ? '\xe4' : '\0';
  for (size_t i = 0; i < stats.page_size; i++) {
    ASSERT_EQ(junk_buf[i], junk);
  }

  char* poison_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  memset(poison_buf, 0xe5, stats.page_size);

  static const char fill = 0x42;
  char* fill_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  memset(fill_buf, fill, stats.page_size);

  arena_params_t params;
  // Allow as many dirty pages in the arena as possible, so that purge never
  // happens in it. Purge breaks some of the tests below randomly depending on
  // what other things happen on other threads.
  params.mMaxDirty = size_t(-1);
  arena_id_t arena = moz_create_arena_with_params(&params);

  // Allocating should junk the buffer, and freeing should poison the buffer.
  for (size_t size : sSizes) {
    if (size <= stats.large_max) {
      SCOPED_TRACE(testing::Message() << "size = " << size);
      char* buf = (char*)moz_arena_malloc(arena, size);
      size_t allocated = moz_malloc_usable_size(buf);
      if (stats.opt_junk || stats.opt_zero) {
        ASSERT_NO_FATAL_FAILURE(
          bulk_compare(buf, 0, allocated, junk_buf, stats.page_size));
      }
      moz_arena_free(arena, buf);
      // We purposefully do a use-after-free here, to check that the data was
      // poisoned.
      ASSERT_NO_FATAL_FAILURE(
        bulk_compare(buf, 0, allocated, poison_buf, stats.page_size));
    }
  }

  // Shrinking in the same size class should be in place and poison between the
  // new allocation size and the old one.
  size_t prev = 0;
  for (size_t size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "size = " << size);
    SCOPED_TRACE(testing::Message() << "prev = " << prev);
    char* ptr = (char*)moz_arena_malloc(arena, size);
    memset(ptr, fill, moz_malloc_usable_size(ptr));
    char* ptr2 = (char*)moz_arena_realloc(arena, ptr, prev + 1);
    ASSERT_EQ(ptr, ptr2);
    ASSERT_NO_FATAL_FAILURE(
      bulk_compare(ptr, 0, prev + 1, fill_buf, stats.page_size));
    ASSERT_NO_FATAL_FAILURE(
      bulk_compare(ptr, prev + 1, size, poison_buf, stats.page_size));
    moz_arena_free(arena, ptr);
    prev = size;
  }

  // In-place realloc should junk the new bytes when growing and poison the old
  // bytes when shrinking.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
      if (CanReallocInPlace(from_size, to_size, stats)) {
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, moz_malloc_usable_size(ptr));
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_EQ(ptr, ptr2);
        if (from_size >= to_size) {
          ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr, 0, to_size, fill_buf, stats.page_size));
          // On Windows (MALLOC_DECOMMIT), in-place realloc of huge allocations
          // decommits extra pages, writing to them becomes an error.
#ifdef XP_WIN
          if (to_size > stats.large_max) {
            size_t page_limit = ALIGNMENT_CEILING(to_size, stats.page_size);
            ASSERT_NO_FATAL_FAILURE(bulk_compare(
              ptr, to_size, page_limit, poison_buf, stats.page_size));
            ASSERT_DEATH_WRAP(ptr[page_limit] = 0, "");
          } else
#endif
          {
            ASSERT_NO_FATAL_FAILURE(bulk_compare(
              ptr, to_size, from_size, poison_buf, stats.page_size));
          }
        } else {
          ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr, 0, from_size, fill_buf, stats.page_size));
          if (stats.opt_junk || stats.opt_zero) {
            ASSERT_NO_FATAL_FAILURE(
              bulk_compare(ptr, from_size, to_size, junk_buf, stats.page_size));
          }
        }
        moz_arena_free(arena, ptr2);
      }
    }
  }

  // Growing to a different size class should poison the old allocation,
  // preserve the original bytes, and junk the new bytes in the new allocation.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      if (from_size < to_size && malloc_good_size(to_size) != from_size &&
          !IsSameRoundedHugeClass(from_size, to_size, stats)) {
        SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, moz_malloc_usable_size(ptr));
        // Avoid in-place realloc by allocating a buffer, expecting it to be
        // right after the buffer we just received. Buffers smaller than the
        // page size and exactly or larger than the size of the largest large
        // size class can't be reallocated in-place.
        char* avoid_inplace = nullptr;
        if (from_size >= stats.page_size && from_size < stats.large_max) {
          avoid_inplace = (char*)moz_arena_malloc(arena, stats.page_size);
          ASSERT_EQ(ptr + from_size, avoid_inplace);
        }
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_NE(ptr, ptr2);
        if (from_size <= stats.large_max) {
          ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr, 0, from_size, poison_buf, stats.page_size));
        }
        ASSERT_NO_FATAL_FAILURE(
          bulk_compare(ptr2, 0, from_size, fill_buf, stats.page_size));
        if (stats.opt_junk || stats.opt_zero) {
          size_t rounded_to_size = malloc_good_size(to_size);
          ASSERT_NE(to_size, rounded_to_size);
          ASSERT_NO_FATAL_FAILURE(bulk_compare(
            ptr2, from_size, rounded_to_size, junk_buf, stats.page_size));
        }
        moz_arena_free(arena, ptr2);
        moz_arena_free(arena, avoid_inplace);
      }
    }
  }

  // Shrinking to a different size class should poison the old allocation,
  // preserve the original bytes, and junk the extra bytes in the new
  // allocation.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      if (from_size > to_size &&
          !CanReallocInPlace(from_size, to_size, stats)) {
        SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, from_size);
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_NE(ptr, ptr2);
        if (from_size <= stats.large_max) {
          ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr, 0, from_size, poison_buf, stats.page_size));
        }
        ASSERT_NO_FATAL_FAILURE(
          bulk_compare(ptr2, 0, to_size, fill_buf, stats.page_size));
        if (stats.opt_junk || stats.opt_zero) {
          size_t rounded_to_size = malloc_good_size(to_size);
          ASSERT_NE(to_size, rounded_to_size);
          ASSERT_NO_FATAL_FAILURE(bulk_compare(
            ptr2, from_size, rounded_to_size, junk_buf, stats.page_size));
        }
        moz_arena_free(arena, ptr2);
      }
    }
  }

  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(arena);

  moz_arena_free(buf_arena, poison_buf);
  moz_arena_free(buf_arena, junk_buf);
  // Until Bug 1364359 is fixed it is unsafe to call moz_dispose_arena.
  // moz_dispose_arena(buf_arena);
}
#endif
