#ifndef SRC_QEMU_PLUGIN_VMI_H_
#define SRC_QEMU_PLUGIN_VMI_H_

#include <cinttypes>

#include <string>

#include "symbolizer.h"
#include "qemu_helpers.h"

class ElfFile;

class VMI {
public:
  explicit VMI() : m_processInvalidated(true) {}
  int Init(ElfFile &elf, Symbolizer *symbolizer);
  int GetProcessInfo(uint32_t &tgid, uint32_t &pid, std::string &comm);
  inline bool LogCall(uint64_t addr) {
    if (addr == m_switchToAddr) {
      m_processInvalidated = true;
      return true;
    }
    return false;
  }
  bool IsProcessInvalidated() { return m_processInvalidated; }

private:
  uint64_t GetCurrentTaskStruct();
  long m_tgidOffset, m_pidOffset, m_commOffset;
  uint64_t m_pcpuHotOffset, m_switchToAddr;
  bool m_processInvalidated;
};

#endif  // SRC_QEMU_PLUGIN_VMI_H_
