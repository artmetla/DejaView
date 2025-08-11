/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_EXT_BASE_SCOPED_MMAP_H_
#define INCLUDE_DEJAVIEW_EXT_BASE_SCOPED_MMAP_H_

#include <cstddef>

#include "dejaview/base/build_config.h"
#include "dejaview/ext/base/scoped_file.h"

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#define DEJAVIEW_HAS_MMAP() 1
#else
#define DEJAVIEW_HAS_MMAP() 0
#endif

namespace dejaview::base {

// RAII wrapper that holds ownership of an mmap()d area and of a file. Calls
// unmap() and close() on destruction.
class ScopedMmap {
 public:
  // Creates a memory mapping for the first `length` bytes of `file`.
  static ScopedMmap FromHandle(base::ScopedPlatformHandle file, size_t length);

  ScopedMmap() {}
  ~ScopedMmap();
  ScopedMmap(ScopedMmap&& other) noexcept;

  ScopedMmap& operator=(ScopedMmap&& other) noexcept;

  // Returns a pointer to the mapped memory area. Only valid if `IsValid()` is
  // true.
  void* data() const { return ptr_; }

  // Returns true if this object contains a successfully mapped area.
  bool IsValid() const { return ptr_ != nullptr; }

  // Returns the length of the mapped area.
  size_t length() const { return length_; }

  // Unmaps the area and closes the file. Returns false if this held a mmap()d
  // area and unmapping failed. In any case, after this method, `IsValid()` will
  // return false.
  bool reset() noexcept;

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
  // Takes ownership of an mmap()d area that starts at `data`, `size` bytes
  // long. `data` should not be MAP_FAILED.
  static ScopedMmap InheritMmappedRange(void* data, size_t size);
#endif

 private:
  ScopedMmap(const ScopedMmap&) = delete;
  ScopedMmap& operator=(const ScopedMmap&) = delete;

  size_t length_ = 0;
  void* ptr_ = nullptr;
  ScopedPlatformHandle file_;
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  ScopedPlatformHandle map_;
#endif
};

// Tries to open `fname` and maps its first `length` bytes in memory.
ScopedMmap ReadMmapFilePart(const char* fname, size_t length);

// Tries to open `fname` and maps the whole file into memory.
ScopedMmap ReadMmapWholeFile(const char* fname);

}  // namespace dejaview::base

#endif  // INCLUDE_DEJAVIEW_EXT_BASE_SCOPED_MMAP_H_
