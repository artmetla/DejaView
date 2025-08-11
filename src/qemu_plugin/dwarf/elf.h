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

#ifndef SRC_QEMU_PLUGIN_DWARF_ELF_H_
#define SRC_QEMU_PLUGIN_DWARF_ELF_H_

#include <string_view>

#include <elf.h>

#include "util.h"

using std::string_view;

struct ByteSwapFunc {
  template <class T>
  T operator()(T val) {
    return ByteSwap(val);
  }
};

struct NullFunc {
  template <class T>
  T operator()(T val) { return val; }
};

class ElfFile {
 public:
  ElfFile(string_view data);

  bool IsOpen();

  // Regions of the file where different headers live.
  string_view entire_file() const;
  string_view header_region() const;
  string_view section_headers() const;
  string_view segment_headers() const;

  const Elf64_Ehdr& header() const;
  Elf64_Xword section_count() const;
  Elf64_Xword section_string_index() const;

  // Represents an ELF segment (data used by the loader / dynamic linker).
  class Segment {
   public:
    const Elf64_Phdr& header() const;
    string_view contents() const;
    string_view range() const;

   private:
    friend class ElfFile;
    Elf64_Phdr header_;
    string_view contents_;
    string_view range_;
  };

  // Represents an ELF section (.text, .data, .bss, etc.)
  class Section {
   public:
    const Elf64_Shdr& header() const;
    string_view contents() const;
    string_view range() const;

    // For SHN_UNDEF (undefined name), returns [nullptr, 0].
    string_view GetName() const;

    // Requires: this is a section with fixed-width entries (symbol table,
    // relocation table, etc).
    Elf64_Word GetEntryCount() const;

    // Requires: header().sh_type == SHT_STRTAB.
    string_view ReadString(Elf64_Word index) const;

    // Requires: header().sh_type == SHT_SYMTAB || header().sh_type ==
    // SHT_DYNSYM
    void ReadSymbol(Elf64_Word index, Elf64_Sym* sym,
                    string_view* file_range) const;

    // Requires: header().sh_type == SHT_REL
    void ReadRelocation(Elf64_Word index, Elf64_Rel* rel,
                        string_view* file_range) const;

    // Requires: header().sh_type == SHT_RELA
    void ReadRelocationWithAddend(Elf64_Word index, Elf64_Rela* rel,
                                  string_view* file_range) const;

    const ElfFile& elf() const;

   private:
    friend class ElfFile;
    const ElfFile* elf_;
    Elf64_Shdr header_;
    string_view contents_;
    string_view range_;
  };

  class NoteIter {
   public:
    NoteIter(const Section& section);

    bool IsDone() const;
    uint32_t type() const;
    string_view name() const;
    string_view descriptor() const;

    void Next();

   public:
    const ElfFile* elf_;
    string_view name_;
    string_view descriptor_;
    string_view remaining_;
    uint32_t type_;
    bool done_ = false;
  };

  void ReadSegment(Elf64_Xword index, Segment* segment) const;
  void ReadSection(Elf64_Xword index, Section* section) const;

  bool FindSectionByName(std::string_view name, Section* section) const;

  bool is_64bit() const;
  bool is_native_endian() const;

  template <class T32, class T64, class Munger>
  void ReadStruct(std::string_view contents, uint64_t offset, Munger munger,
                  std::string_view* range, T64* out) const {
    StructReader(*this, contents).Read<T32>(offset, munger, range, out);
  }

 private:
  friend class Section;

  bool Initialize();

  string_view GetRegion(uint64_t start, uint64_t n) const;

  // Shared code for reading various ELF structures.  Handles endianness
  // conversion and 32->64 bit conversion, when necessary.
  class StructReader {
   public:
    StructReader(const ElfFile& elf, string_view data);

    template <class T32, class T64, class Munger>
    void Read(uint64_t offset, Munger /*munger*/, std::string_view* range,
              T64* out) const {
      if (elf_.is_64bit() && elf_.is_native_endian()) {
        return Memcpy(offset, range, out);
      } else {
        return ReadFallback<T32, T64, Munger>(offset, range, out);
      }
    }

   private:
    const ElfFile& elf_;
    string_view data_;

    template <class T32, class T64, class Munger>
    void ReadFallback(uint64_t offset,
                      std::string_view* range,
                      T64* out) const {
      // Fallback for either 32-bit ELF file or non-native endian.
      if (elf_.is_64bit()) {
        assert(!elf_.is_native_endian());
        Memcpy(offset, range, out);
        Munger()(*out, out, ByteSwapFunc());
      } else {
        T32 data32;
        Memcpy(offset, range, &data32);
        if (elf_.is_native_endian()) {
          Munger()(data32, out, NullFunc());
        } else {
          Munger()(data32, out, ByteSwapFunc());
        }
      }
    }

    template <class T>
    void Memcpy(uint64_t offset, std::string_view* out_range, T* out) const {
      std::string_view range = StrictSubstr(data_, offset, sizeof(*out));
      if (out_range) {
        *out_range = range;
      }
      memcpy(out, data_.data() + offset, sizeof(*out));
    }
  };

  bool ok_;
  bool is_64bit_;
  bool is_native_endian_;
  string_view data_;
  Elf64_Ehdr header_;
  Elf64_Xword section_count_;
  Elf64_Xword section_string_index_;
  string_view header_region_;
  string_view section_headers_;
  string_view segment_headers_;
  Section section_name_table_;
};

#endif  // SRC_QEMU_PLUGIN_DWARF_ELF_H_
