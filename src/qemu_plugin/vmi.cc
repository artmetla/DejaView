#include "vmi.h"

#include <string>
#include <cstring>
#include <iostream>

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "dwarf/debug_info.h"
#include "dwarf/elf.h"

#include "qemu_helpers.h"

extern "C" {
#include "qemu_api.h"
}

using namespace dwarf2reader;

// Helper function to find member offsets within task_struct
static bool findMemberOffset(dwarf::CU &cu, dwarf::DIEReader &die_reader,
                             const dwarf::AbbrevTable::Abbrev* abbrev,
                             long* tgidOffset, long* pidOffset, long* commOffset) {
  if (abbrev->tag == DW_TAG_member) {
    std::string name = "";
    long val = -1;
    die_reader.ReadAttributes(
        cu, abbrev, [&cu, &name, &val](uint16_t tag, dwarf::AttrValue value) {
          if (tag == DW_AT_name && value.IsString()) {
            name = value.GetString(cu);
          } else if (tag == DW_AT_data_member_location) {
            std::optional<uint64_t> uintValue = value.ToUint(cu);
            if (uintValue.has_value()) {
              val = static_cast<long>(uintValue.value());
            }
          }
        });
    if (val == -1) {
      return false;
    }
    if (name == "tgid") {
      *tgidOffset = val;
      return true;
    }
    if (name == "pid") {
      *pidOffset = val;
      return true;
    }
    if (name == "comm") {
      *commOffset = val;
      return true;
    }
  }
  return false;
}

std::string_view* dwarf::File::GetFieldByName(std::string_view name) {
  if (name == "aranges") {
    return &debug_aranges;
  } else if (name == "addr") {
    return &debug_addr;
  } else if (name == "str") {
    return &debug_str;
  } else if (name == "str_offsets") {
    return &debug_str_offsets;
  } else if (name == "line_str") {
    return &debug_line_str;
  } else if (name == "info") {
    return &debug_info;
  } else if (name == "types") {
    return &debug_types;
  } else if (name == "abbrev") {
    return &debug_abbrev;
  } else if (name == "line") {
    return &debug_line;
  } else if (name == "loc") {
    return &debug_loc;
  } else if (name == "pubnames") {
    return &debug_pubnames;
  } else if (name == "pubtypes") {
    return &debug_pubtypes;
  } else if (name == "ranges") {
    return &debug_ranges;
  } else if (name == "rnglists") {
    return &debug_rnglists;
  } else {
    return nullptr;
  }
}

struct ChdrMunger {
  template <class From, class Func>
  void operator()(const From& from, Elf64_Chdr* to, Func func) {
    to->ch_type = func(from.ch_type);
    to->ch_size = func(from.ch_size);
    to->ch_addralign   = func(from.ch_addralign);
  }
};

// Function to get task_struct offsets
static bool getTaskStructOffsets(ElfFile &elf,
                                 long* tgidOffset, long* pidOffset,
                                 long* commOffset) {
  *tgidOffset = -1;
  *pidOffset = -1;
  *commOffset = -1;
  bool struct_found = false;

  dwarf::File dwarf;
  for (Elf64_Xword i = 1; i < elf.section_count(); i++) {
    ElfFile::Section section;
    elf.ReadSection(i, &section);
    string_view name = section.GetName();
    string_view contents = section.contents();
    uint64_t uncompressed_size = 0;

    if (section.header().sh_flags & SHF_COMPRESSED) {
      // Standard ELF section compression, produced when you link with
      //   --compress-debug-sections=zlib-gabi
      Elf64_Chdr chdr;
      std::string_view range;
      elf.ReadStruct<Elf32_Chdr>(contents, 0, ChdrMunger(), &range, &chdr);
      if (chdr.ch_type != ELFCOMPRESS_ZLIB) {
        // Unknown compression format.
        continue;
      }
      uncompressed_size = chdr.ch_size;
      contents.remove_prefix(range.size());
    }

    if (name.find(".debug_") == 0) {
      name.remove_prefix(string_view(".debug_").size());
    } else if (name.find(".zdebug_") == 0) {
      // GNU format compressed debug info, produced when you link with
      //   --compress-debug-sections=zlib-gnu
      name.remove_prefix(string_view(".zdebug_").size());
      if (ReadBytes(4, &contents) != "ZLIB") {
        continue;  // Bad compression header.
      }
      uncompressed_size = ReadBigEndian<uint64_t>(&contents);
    }

    static constexpr string_view dwo_str(".dwo");
    if (name.size() >= dwo_str.size() &&
        name.rfind(".dwo") == name.size() - dwo_str.size()) {
      name.remove_suffix(dwo_str.size());
    }

    if (string_view* member = dwarf.GetFieldByName(name)) {
      if (uncompressed_size) {
        // TODO: We could probably wire up zlib here...
	      QEMU_LOG() << "Unhandled zlib compressed debugging info" << std::endl;
        exit(1);
      } else {
        *member = section.contents();
      }
    }
  }

  dwarf::InfoReader reader(dwarf, /*skeleton=*/nullptr);
  dwarf::CUIter iter = reader.GetCUIter(dwarf::InfoReader::Section::kDebugInfo);
  dwarf::CU cu;

  // Iterate over compilation units
  while (iter.NextCU(reader, &cu)) {
    dwarf::DIEReader die_reader = cu.GetDIEReader();
    // Iterate over DIEs
    while (auto abbrev = die_reader.ReadCode(cu)) {
      // Only consider structure types
      if (abbrev->tag == DW_TAG_structure_type) {
        // Only consider those with the name tag "task_struct"
        bool is_task_struct = false;
        die_reader.ReadAttributes(
            cu, abbrev, [&cu, &is_task_struct](uint16_t tag, dwarf::AttrValue value) {
              if (tag == DW_AT_name && value.IsString() && value.GetString(cu) == "task_struct") {
                is_task_struct = true;
              }
            });
        if (is_task_struct) {
          long found_count = 0;
          die_reader.ReadChildren(cu, abbrev, [&cu, &die_reader, &found_count, &tgidOffset, &pidOffset, &commOffset](const dwarf::AbbrevTable::Abbrev* child) {
            bool found = findMemberOffset(cu, die_reader, child, tgidOffset, pidOffset, commOffset);
            if (found)
              found_count++;
          });

          if (found_count == 3)
            return true;  // Found all three members

          QEMU_LOG() << "Error: Could not find tgid, pid, and comm offsets in task_struct" << std::endl;
          return false;
        }
      } else {
        die_reader.ReadAttributes(
            cu, abbrev, [](uint16_t, dwarf::AttrValue) { });
      }
    }
  }

  if (!struct_found) {
      QEMU_LOG() << "Error: Structure task_struct not found" << std::endl;
      return false;
  }
  QEMU_LOG() << "Error: Could not find tgid, pid and comm offsets in task_struct" << std::endl;
  return false;
}

// Assume these are provided externally
static uint64_t TASK_STRUCT_COMM_LEN = 16;

int VMI::Init(ElfFile &elf, Symbolizer *symbolizer) {
  if (!getTaskStructOffsets(elf,
                           &m_tgidOffset, &m_pidOffset, &m_commOffset))
    return -1;

  m_pcpuHotOffset = symbolizer->lookupSymbol("pcpu_hot");
  if (!m_pcpuHotOffset)
    return -1;

  if (!m_pcpuHotOffset) {
    m_perCpuOffset = symbolizer->lookupSymbol("__per_cpu_offset");
    if (!m_perCpuOffset)
      return -1;

    m_currentTaskOffset = symbolizer->lookupSymbol("current_task");
    if (!m_currentTaskOffset)
      return -1;
  }

  m_switchToAddr = symbolizer->lookupSymbol("__switch_to_asm");
  if (!m_switchToAddr)
    return -1;

  return 0;
}

uint64_t VMI::GetCurrentTaskStruct() {
  uint64_t ret = 0;
  uint64_t gs_base = 0;
  GLibArray *reg = g_byte_array_new();

  if (qemu_plugin_read_register(get_gs_base_handle(), reg) < 0)
    goto exit;

  memcpy(&gs_base, reg->data, sizeof(gs_base));
  
  if (m_pcpuHotOffset) {
    if (!qemu_plugin_read_memory_vaddr(gs_base + m_pcpuHotOffset, reg, sizeof(ret)))
      goto exit;
  } else {
    if (!qemu_plugin_read_memory_vaddr(m_perCpuOffset, reg, sizeof(ret)))
      goto exit;

    memcpy(&ret, reg->data, sizeof(ret));

    if (!qemu_plugin_read_memory_vaddr(ret + m_currentTaskOffset, reg, sizeof(ret)))
      goto exit;
  }

  memcpy(&ret, reg->data, sizeof(ret));

exit:
  g_byte_array_free(reg, true);
  return ret;
}

// Function to extract process information
int VMI::GetProcessInfo(uint32_t &tgid, uint32_t &pid, std::string &comm) {
  int ret = -1;
  GLibArray *data = g_byte_array_new();

  uint64_t current_task_addr = GetCurrentTaskStruct();
  if (!current_task_addr)
    goto exit;

  if (!qemu_plugin_read_memory_vaddr(current_task_addr + static_cast<uint64_t>(m_tgidOffset), data, sizeof(tgid)))
    goto exit;
  memcpy(&tgid, data->data, sizeof(uint32_t));

  if (!qemu_plugin_read_memory_vaddr(current_task_addr + static_cast<uint64_t>(m_pidOffset), data, sizeof(pid)))
    goto exit;
  memcpy(&pid, data->data, sizeof(pid));

  if (!qemu_plugin_read_memory_vaddr(current_task_addr + static_cast<uint64_t>(m_commOffset), data, TASK_STRUCT_COMM_LEN))
    goto exit;
  comm = std::string(reinterpret_cast<char *>(data->data), TASK_STRUCT_COMM_LEN);
  comm = comm.substr(0, comm.find('\0'));

  m_processInvalidated = false;
  ret = 0;
exit:
  g_byte_array_free(data, true);
  return ret;
}
