/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dejaview/ext/base/paged_memory.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#include <Windows.h>
#else  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#include <sys/mman.h>
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/container_annotations.h"
#include "dejaview/ext/base/utils.h"

namespace dejaview {
namespace base {

namespace {

#if TRACK_COMMITTED_SIZE()
constexpr size_t kCommitChunkSize = 4 * 1024 * 1024;  // 4MB
#endif

size_t RoundUpToSysPageSize(size_t req_size) {
  const size_t page_size = GetSysPageSize();
  return (req_size + page_size - 1) & ~(page_size - 1);
}

size_t GuardSize() {
  return GetSysPageSize();
}

}  // namespace

// static
PagedMemory PagedMemory::Allocate(size_t req_size, int flags) {
  size_t rounded_up_size = RoundUpToSysPageSize(req_size);
  DEJAVIEW_CHECK(rounded_up_size >= req_size);
  size_t outer_size = rounded_up_size + GuardSize() * 2;
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  void* ptr = VirtualAlloc(nullptr, outer_size, MEM_RESERVE, PAGE_NOACCESS);
  if (!ptr && (flags & kMayFail))
    return PagedMemory();
  DEJAVIEW_CHECK(ptr);
  char* usable_region = reinterpret_cast<char*>(ptr) + GuardSize();
#else   // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  void* ptr = mmap(nullptr, outer_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED && (flags & kMayFail))
    return PagedMemory();
  DEJAVIEW_CHECK(ptr && ptr != MAP_FAILED);
  char* usable_region = reinterpret_cast<char*>(ptr) + GuardSize();
  int res = mprotect(ptr, GuardSize(), PROT_NONE);
  res |= mprotect(usable_region + rounded_up_size, GuardSize(), PROT_NONE);
  DEJAVIEW_CHECK(res == 0);
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)

  auto memory = PagedMemory(usable_region, req_size);
#if TRACK_COMMITTED_SIZE()
  size_t initial_commit = req_size;
  if (flags & kDontCommit)
    initial_commit = std::min(initial_commit, kCommitChunkSize);
  memory.EnsureCommitted(initial_commit);
#endif  // TRACK_COMMITTED_SIZE()
  return memory;
}

PagedMemory::PagedMemory() {}

// clang-format off
PagedMemory::PagedMemory(char* p, size_t size) : p_(p), size_(size) {
  ANNOTATE_NEW_BUFFER(p_, size_, committed_size_)
}

PagedMemory::PagedMemory(PagedMemory&& other) noexcept {
  *this = other;
  other.p_ = nullptr;
}
// clang-format on

PagedMemory& PagedMemory::operator=(PagedMemory&& other) {
  this->~PagedMemory();
  new (this) PagedMemory(std::move(other));
  return *this;
}

PagedMemory::~PagedMemory() {
  if (!p_)
    return;
  DEJAVIEW_CHECK(size_);
  char* start = p_ - GuardSize();
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  BOOL res = VirtualFree(start, 0, MEM_RELEASE);
  DEJAVIEW_CHECK(res != 0);
#else   // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  const size_t outer_size = RoundUpToSysPageSize(size_) + GuardSize() * 2;
  int res = munmap(start, outer_size);
  DEJAVIEW_CHECK(res == 0);
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  ANNOTATE_DELETE_BUFFER(p_, size_, committed_size_)
}

bool PagedMemory::AdviseDontNeed(void* p, size_t size) {
  DEJAVIEW_DCHECK(p_);
  DEJAVIEW_DCHECK(p >= p_);
  DEJAVIEW_DCHECK(static_cast<char*>(p) + size <= p_ + size_);
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN) || DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_NACL)
  // Discarding pages on Windows has more CPU cost than is justified for the
  // possible memory savings.
  return false;
#else   // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN) ||
        // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_NACL)
  // http://man7.org/linux/man-pages/man2/madvise.2.html
  int res = madvise(p, size, MADV_DONTNEED);
  DEJAVIEW_DCHECK(res == 0);
  return true;
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN) ||
        // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_NACL)
}

#if TRACK_COMMITTED_SIZE()
void PagedMemory::EnsureCommitted(size_t committed_size) {
  DEJAVIEW_DCHECK(committed_size <= size_);
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  if (committed_size_ >= committed_size)
    return;
  // Rounding up.
  size_t delta = committed_size - committed_size_;
  size_t num_additional_chunks =
      (delta + kCommitChunkSize - 1) / kCommitChunkSize;
  DEJAVIEW_DCHECK(num_additional_chunks * kCommitChunkSize >= delta);
  // Don't commit more than the total size.
  size_t commit_size = std::min(num_additional_chunks * kCommitChunkSize,
                                size_ - committed_size_);
  void* res = VirtualAlloc(p_ + committed_size_, commit_size, MEM_COMMIT,
                           PAGE_READWRITE);
  DEJAVIEW_CHECK(res);
  ANNOTATE_CHANGE_SIZE(p_, size_, committed_size_,
                       committed_size_ + commit_size)
  committed_size_ += commit_size;
#else   // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  // mmap commits automatically as needed, so we only track here for ASAN.
  committed_size = std::max(committed_size_, committed_size);
  ANNOTATE_CHANGE_SIZE(p_, size_, committed_size_, committed_size)
  committed_size_ = committed_size;
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
}
#endif  // TRACK_COMMITTED_SIZE()

}  // namespace base
}  // namespace dejaview
