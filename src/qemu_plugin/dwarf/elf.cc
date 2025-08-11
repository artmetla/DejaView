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

#include <iostream>

#include "elf.h"
#include "dwarf_constants.h"
#include "../qemu_helpers.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

using namespace dwarf2reader;
using std::string_view;

template <class T>
void AdvancePastStruct(string_view* data) {
  *data = data->substr(sizeof(T));
}

// ElfFile /////////////////////////////////////////////////////////////////////

// For parsing the pieces we need out of an ELF file (.o, .so, and binaries).

ElfFile::ElfFile(string_view data) : data_(data) {
  ok_ = Initialize();
}

bool ElfFile::IsOpen() { return ok_; }

string_view ElfFile::entire_file() const { return data_; }
string_view ElfFile::header_region() const { return header_region_; }
string_view ElfFile::section_headers() const { return section_headers_; }
string_view ElfFile::segment_headers() const { return segment_headers_; }

const Elf64_Ehdr& ElfFile::header() const { return header_; }
Elf64_Xword ElfFile::section_count() const { return section_count_; }
Elf64_Xword ElfFile::section_string_index() const { return section_string_index_; }

const Elf64_Phdr& ElfFile::Segment::header() const { return header_; }
string_view ElfFile::Segment::contents() const { return contents_; }
string_view ElfFile::Segment::range() const { return range_; }

const Elf64_Shdr& ElfFile::Section::header() const { return header_; }
string_view ElfFile::Section::contents() const { return contents_; }
string_view ElfFile::Section::range() const { return range_; }

const ElfFile& ElfFile::Section::elf() const { return *elf_; }

ElfFile::NoteIter::NoteIter(const Section& section)
    : elf_(&section.elf()), remaining_(section.contents()) {
  Next();
}

bool ElfFile::NoteIter::IsDone() const { return done_; }
uint32_t ElfFile::NoteIter::type() const { return type_; }
string_view ElfFile::NoteIter::name() const { return name_; }
string_view ElfFile::NoteIter::descriptor() const { return descriptor_; }

bool ElfFile::is_64bit() const { return is_64bit_; }
bool ElfFile::is_native_endian() const { return is_native_endian_; }

string_view ElfFile::GetRegion(uint64_t start, uint64_t n) const {
  return StrictSubstr(data_, start, n);
}

ElfFile::StructReader::StructReader(const ElfFile& elf, string_view data)
    : elf_(elf), data_(data) {}

// ELF uses different structure definitions for 32/64 bit files.  The sizes of
// members are different, and members are even in a different order!
//
// These mungers can convert 32 bit structures to 64-bit ones.  They can also
// handle converting endianness.  We use templates so a single template function
// can handle all three patterns:
//
//   32 native  -> 64 native
//   32 swapped -> 64 native
//   64 swapped -> 64 native

struct EhdrMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Ehdr* to, Func func) {
    memmove(&to->e_ident[0], &from.e_ident[0], EI_NIDENT);
    to->e_type       = func(from.e_type);
    to->e_machine    = func(from.e_machine);
    to->e_version    = func(from.e_version);
    to->e_entry      = func(from.e_entry);
    to->e_phoff      = func(from.e_phoff);
    to->e_shoff      = func(from.e_shoff);
    to->e_flags      = func(from.e_flags);
    to->e_ehsize     = func(from.e_ehsize);
    to->e_phentsize  = func(from.e_phentsize);
    to->e_phnum      = func(from.e_phnum);
    to->e_shentsize  = func(from.e_shentsize);
    to->e_shnum      = func(from.e_shnum);
    to->e_shstrndx   = func(from.e_shstrndx);
  }
};

struct ShdrMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Shdr* to, Func func) {
    to->sh_name       = func(from.sh_name);
    to->sh_type       = func(from.sh_type);
    to->sh_flags      = func(from.sh_flags);
    to->sh_addr       = func(from.sh_addr);
    to->sh_offset     = func(from.sh_offset);
    to->sh_size       = func(from.sh_size);
    to->sh_link       = func(from.sh_link);
    to->sh_info       = func(from.sh_info);
    to->sh_addralign  = func(from.sh_addralign);
    to->sh_entsize    = func(from.sh_entsize);
  }
};

struct PhdrMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Phdr* to, Func func) {
    to->p_type   = func(from.p_type);
    to->p_flags  = func(from.p_flags);
    to->p_offset = func(from.p_offset);
    to->p_vaddr  = func(from.p_vaddr);
    to->p_paddr  = func(from.p_paddr);
    to->p_filesz = func(from.p_filesz);
    to->p_memsz  = func(from.p_memsz);
    to->p_align  = func(from.p_align);
  }
};

struct SymMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Sym* to, Func func) {
    to->st_name   = func(from.st_name);
    to->st_info   = func(from.st_info);
    to->st_other  = func(from.st_other);
    to->st_shndx  = func(from.st_shndx);
    to->st_value  = func(from.st_value);
    to->st_size   = func(from.st_size);
  }
};

struct RelMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Rel* to, Func func) {
    to->r_offset = func(from.r_offset);
    to->r_info   = func(from.r_info);
  }
};

struct RelaMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Rela* to, Func func) {
    to->r_offset = func(from.r_offset);
    to->r_info   = func(from.r_info);
    to->r_addend = func(from.r_addend);
  }
};

struct NoteMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Nhdr* to, Func func) {
    to->n_namesz = func(from.n_namesz);
    to->n_descsz = func(from.n_descsz);
    to->n_type   = func(from.n_type);
  }
};

string_view ElfFile::Section::GetName() const {
  if (header_.sh_name == SHN_UNDEF) {
    return string_view(nullptr, 0);
  }
  return elf_->section_name_table_.ReadString(header_.sh_name);
}

string_view ElfFile::Section::ReadString(Elf64_Word index) const {
  assert(header().sh_type == SHT_STRTAB);

  if (index == SHN_UNDEF || index >= contents_.size()) {
    QEMU_LOG() << "can't read index " << index << " from strtab, total size is " << contents_.size() << "\n";
    exit(1);
  }

  string_view ret = StrictSubstr(contents_, index);

  const char* null_pos =
      static_cast<const char*>(memchr(ret.data(), '\0', ret.size()));

  if (null_pos == NULL) {
    QEMU_LOG() << "no NULL terminator found\n";
    exit(1);
  }

  size_t len = static_cast<size_t>(null_pos - ret.data());
  ret = ret.substr(0, len);
  return ret;
}

Elf64_Word ElfFile::Section::GetEntryCount() const {
  if (header_.sh_entsize == 0) {
    QEMU_LOG() << "sh_entsize is zero\n";
    exit(1);
  }
  return static_cast<Elf64_Word>(contents_.size() / header_.sh_entsize);
}

void ElfFile::Section::ReadSymbol(Elf64_Word index, Elf64_Sym* sym,
                                  string_view* file_range) const {
  assert(header().sh_type == SHT_SYMTAB || header().sh_type == SHT_DYNSYM);
  size_t offset = header_.sh_entsize * index;
  elf_->ReadStruct<Elf32_Sym>(contents(), offset, SymMunger(), file_range, sym);
}

void ElfFile::Section::ReadRelocation(Elf64_Word index, Elf64_Rel* rel,
                                      string_view* file_range) const {
  assert(header().sh_type == SHT_REL);
  size_t offset = header_.sh_entsize * index;
  elf_->ReadStruct<Elf32_Rel>(contents(), offset, RelMunger(), file_range, rel);
}

void ElfFile::Section::ReadRelocationWithAddend(Elf64_Word index,
                                                Elf64_Rela* rela,
                                                string_view* file_range) const {
  assert(header().sh_type == SHT_RELA);
  size_t offset = header_.sh_entsize * index;
  elf_->ReadStruct<Elf32_Rela>(contents(), offset, RelaMunger(), file_range,
                               rela);
}

void ElfFile::NoteIter::Next() {
  if (remaining_.empty()) {
    done_ = true;
    return;
  }

  Elf_Note note;
  elf_->ReadStruct<Elf_Note>(remaining_, 0, NoteMunger(), nullptr, &note);

  // 32-bit and 64-bit note are the same size, so we don't have to treat
  // them separately when advancing.
  AdvancePastStruct<Elf_Note>(&remaining_);

  type_ = note.n_type;
  name_ = StrictSubstr(remaining_, 0, note.n_namesz);

  // Size might include NULL terminator.
  if (name_[name_.size() - 1] == 0) {
    name_ = name_.substr(0, name_.size() - 1);
  }

  remaining_ = StrictSubstr(remaining_, AlignUp(note.n_namesz, 4));
  descriptor_ = StrictSubstr(remaining_, 0, note.n_descsz);
  remaining_ = StrictSubstr(remaining_, AlignUp(note.n_descsz, 4));
}

bool ElfFile::Initialize() {
  if (data_.size() < EI_NIDENT) {
    return false;
  }

  unsigned char ident[EI_NIDENT];
  memcpy(ident, data_.data(), EI_NIDENT);

  if (memcmp(ident, "\177ELF", 4) != 0) {
    // Not an ELF file.
    return false;
  }

  switch (ident[EI_CLASS]) {
    case ELFCLASS32:
      is_64bit_ = false;
      break;
    case ELFCLASS64:
      is_64bit_ = true;
      break;
    default:
      QEMU_LOG() << "unexpected ELF class: " << ident[EI_CLASS] << "\n";
      exit(1);
  }

  switch (ident[EI_DATA]) {
    case ELFDATA2LSB:
      is_native_endian_ = GetMachineEndian() == Endian::kLittle;
      break;
    case ELFDATA2MSB:
      is_native_endian_ = GetMachineEndian() == Endian::kBig;
      break;
    default:
      QEMU_LOG() << "unexpected ELF data: " << ident[EI_DATA] << "\n";
      exit(1);
  }

  std::string_view range;
  ReadStruct<Elf32_Ehdr>(entire_file(), 0, EhdrMunger(), &range, &header_);

  Section section0;
  bool has_section0 = 0;

  // ELF extensions: if certain fields overflow, we have to find their true data
  // from elsewhere.  For more info see:
  // https://docs.oracle.com/cd/E19683-01/817-3677/chapter6-94076/index.html
  if (header_.e_shoff > 0 &&
      data_.size() > (header_.e_shoff + header_.e_shentsize)) {
    section_count_ = 1;
    ReadSection(0, &section0);
    has_section0 = true;
  }

  section_count_ = header_.e_shnum;
  section_string_index_ = header_.e_shstrndx;

  if (section_count_ == 0 && has_section0) {
    section_count_ = section0.header().sh_size;
  }

  if (section_string_index_ == SHN_XINDEX && has_section0) {
    section_string_index_ = section0.header().sh_link;
  }

  header_region_ = GetRegion(0, header_.e_ehsize);
  section_headers_ = GetRegion(header_.e_shoff,
                               CheckedMul(header_.e_shentsize, section_count_));
  segment_headers_ = GetRegion(
      header_.e_phoff, CheckedMul(header_.e_phentsize, header_.e_phnum));

  if (section_count_ > 0) {
    ReadSection(section_string_index_, &section_name_table_);
    if (section_name_table_.header().sh_type != SHT_STRTAB) {
      QEMU_LOG() << "section string index pointed to non-strtab\n";
      exit(1);
    }
  }

  return true;
}

void ElfFile::ReadSegment(Elf64_Xword index, Segment* segment) const {
  if (index >= header_.e_phnum) {
    QEMU_LOG() << "segment " << index << " doesn't exist, only " << header_.e_phnum << "segments\n";
    exit(1);
  }

  Elf64_Phdr* header = &segment->header_;
  ReadStruct<Elf32_Phdr>(
      entire_file(),
      CheckedAdd(header_.e_phoff, CheckedMul(header_.e_phentsize, index)),
      PhdrMunger(), &segment->range_, header);
  if (header->p_filesz > 0) {
    segment->contents_ = GetRegion(header->p_offset, header->p_filesz);
  }
}

void ElfFile::ReadSection(Elf64_Xword index, Section* section) const {
  if (index >= section_count_) {
    QEMU_LOG() << "tried to read section " << index << " but there are only " << section_count_ << "\n";
    exit(1);
  }

  Elf64_Shdr* header = &section->header_;
  ReadStruct<Elf32_Shdr>(
      entire_file(),
      CheckedAdd(header_.e_shoff, CheckedMul(header_.e_shentsize, index)),
      ShdrMunger(), &section->range_, header);

  if (header->sh_type == SHT_NOBITS) {
    section->contents_ = string_view();
  } else {
    section->contents_ = GetRegion(header->sh_offset, header->sh_size);
  }

  section->elf_ = this;
}

bool ElfFile::FindSectionByName(std::string_view name, Section* section) const {
  for (Elf64_Word i = 0; i < section_count_; i++) {
    ReadSection(i, section);
    if (section->GetName() == name) {
      return true;
    }
  }
  return false;
}
