// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_QEMU_PLUGIN_DWARF_UTIL_H_
#define SRC_QEMU_PLUGIN_DWARF_UTIL_H_

#include <stdexcept>
#include <string_view>
#include <assert.h>

#include "../qemu_helpers.h"

inline uint64_t CheckedAdd(uint64_t a, uint64_t b) {
    uint64_t c = a + b;
    if (c < a) {
        QEMU_LOG() << "integer overflow in addition\n";
        exit(1);
    }
    return c;
}

inline uint64_t CheckedMul(uint64_t a, uint64_t b) {
  if (a == 0 || b == 0) {
    return 0;
  }

  uint64_t c = a * b;
  if (c / a != b) {
    QEMU_LOG() << "integer overflow in multiply\n";
    exit(1);
  }

  return c;
}

inline std::string_view StrictSubstr(std::string_view data, size_t off,
                                      size_t n) {
  uint64_t end = CheckedAdd(off, n);
  if (end > data.size()) {
    QEMU_LOG() << "region out-of-bounds\n";
    exit(1);
  }
  return data.substr(off, n);
}

inline std::string_view StrictSubstr(std::string_view data, size_t off) {
  if (off > data.size()) {
    QEMU_LOG() << "region out-of-bounds\n";
    exit(1);
  }
  return data.substr(off);
}

inline size_t AlignUp(size_t offset, size_t granularity) {
  // Granularity must be a power of two.
  assert((granularity & (granularity - 1)) == 0);
  return (offset + granularity - 1) & ~(granularity - 1);
}

// Endianness utilities ////////////////////////////////////////////////////////

enum class Endian { kBig, kLittle };

inline Endian GetMachineEndian() {
  int x = 1;
  return *reinterpret_cast<const char*>(&x) == 1 ? Endian::kLittle : Endian::kBig;
}

// Generic algorithm for byte-swapping an integer of arbitrary size.
//
// With modern GCC/Clang this optimizes to a "bswap" instruction.
template <size_t N, class T> constexpr T _BS(T val) {
  if constexpr (N == 1) {
    return val & 0xff;
  } else {
    size_t bits = 8 * (N / 2);
    return static_cast<T>(_BS<N / 2>(val) << bits) | _BS<N / 2>(static_cast<T>(val >> bits));
  }
};

// Byte swaps the given integer, and returns the byte-swapped value.
template <class T> constexpr T ByteSwap(T val) {
    return _BS<sizeof(T)>(val);
}

template <class T, size_t N = sizeof(T)> T ReadFixed(std::string_view *data) {
  static_assert(N <= sizeof(T), "N too big for this data type");
  T val = 0;
  if (data->size() < N) {
    QEMU_LOG() << "premature EOF reading fixed-length data\n";
    exit(1);
  }
  memcpy(&val, data->data(), N);
  data->remove_prefix(N);
  return val;
}

template <class T> T ReadEndian(std::string_view *data, Endian endian) {
  T val = ReadFixed<T>(data);
  return endian == GetMachineEndian() ? val : ByteSwap(val);
}

template <class T> T ReadLittleEndian(std::string_view *data) {
  return ReadEndian<T>(data, Endian::kLittle);
}

template <class T> T ReadBigEndian(std::string_view *data) {
  return ReadEndian<T>(data, Endian::kBig);
}

// General data reading  ///////////////////////////////////////////////////////

std::string_view ReadUntil(std::string_view* data, char c);

std::string_view ReadUntilConsuming(std::string_view* data, char c);

inline std::string_view ReadNullTerminated(std::string_view* data) {
  return ReadUntilConsuming(data, '\0');
}

inline std::string_view ReadBytes(size_t bytes, std::string_view* data) {
  if (data->size() < bytes) {
    QEMU_LOG() << "premature EOF reading variable-length DWARF data\n";
    exit(1);
  }
  std::string_view ret = data->substr(0, bytes);
  data->remove_prefix(bytes);
  return ret;
}

inline void SkipBytes(size_t bytes, std::string_view* data) {
  ReadBytes(bytes, data);  // Discard result.
}

#endif  // SRC_QEMU_PLUGIN_DWARF_UTIL_H_
