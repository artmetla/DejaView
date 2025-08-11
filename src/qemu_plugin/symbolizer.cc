#include "symbolizer.h"

#include <cstdio>

#include "dwarf/elf.h"

#include "qemu_helpers.h"

bool Symbolizer::Init(ElfFile &elf) {
  if (!readSymbols(elf))
    return false;

  return true;
}

bool Symbolizer::lookupAddress(uint64_t address, std::string& name, std::string& filename, int& line_number) {
  name = m_address_to_symbol[address];

  // TODO: extract those from DWARF
  filename = "";
  line_number = 0;

  return !name.empty();
}

uint64_t Symbolizer::lookupSymbol(const std::string& symbol_name) {
  auto it = m_symbol_to_address.find(symbol_name);
  if (it != m_symbol_to_address.end())
    return it->second;
  return 0;
}

bool Symbolizer::readSymbols(ElfFile &elf) {
  for (Elf64_Xword i = 1; i < elf.section_count(); i++) {
    ElfFile::Section section;
    elf.ReadSection(i, &section);

    if (section.header().sh_type != SHT_SYMTAB) {
      continue;
    }

    Elf64_Word symbol_count = section.GetEntryCount();

    ElfFile::Section strtab_section;
    elf.ReadSection(section.header().sh_link, &strtab_section);
    if (strtab_section.header().sh_type != SHT_STRTAB) {
      QEMU_LOG() << "symtab section pointed to non-strtab section\n";
      exit(1);
    }

    for (Elf64_Word j = 1; j < symbol_count; j++) {
      Elf64_Sym sym;

      section.ReadSymbol(j, &sym, nullptr);

      if (sym.st_shndx == STN_UNDEF) {
        continue;
      }

      if (sym.st_size == 0) {
        continue;
      }

      std::string name(strtab_section.ReadString(sym.st_name));
      uint64_t address = static_cast<uint64_t>(sym.st_value);
      m_symbol_to_address[name] = address;
      m_address_to_symbol[address] = name;
    }
  }

  return true;
}
