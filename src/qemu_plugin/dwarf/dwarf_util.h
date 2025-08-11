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

#ifndef SRC_QEMU_PLUGIN_DWARF_DWARF_UTIL_H_
#define SRC_QEMU_PLUGIN_DWARF_DWARF_UTIL_H_

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "util.h"
#include "../qemu_helpers.h"

namespace dwarf {

uint64_t ReadLEB128Internal(bool is_signed, std::string_view* data);

// Reads a DWARF LEB128 varint, where high bits indicate continuation.
template <typename T>
T ReadLEB128(std::string_view* data) {
  typedef typename std::conditional<std::is_signed<T>::value, int64_t,
                                    uint64_t>::type Int64Type;
  Int64Type val = static_cast<Int64Type>(ReadLEB128Internal(std::is_signed<T>::value, data));
  if (val > std::numeric_limits<T>::max() ||
      val < std::numeric_limits<T>::min()) {
    QEMU_LOG() << "DWARF data contained larger LEB128 than we were expecting\n";
    exit(1);
  }
  return static_cast<T>(val);
}

void SkipLEB128(std::string_view* data);

bool IsValidDwarfAddress(uint64_t addr, uint8_t address_size);

inline int DivRoundUp(int n, int d) {
  return (n + (d - 1)) / d;
}

std::string_view ReadDebugStrEntry(std::string_view section, size_t ofs);

}  // namepsace dwarf

#endif  // SRC_QEMU_PLUGIN_DWARF_DWARF_UTIL_H_
