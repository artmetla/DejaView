#include "disassembler.h"

#include <string.h>

extern "C" {
#include "qemu_api.h"
}

struct target_info {
  const char* name;
  cs_arch arch;
  cs_mode mode;
};

static struct target_info* target;

static target_info all_archs[] = {
  {"aarch64", CS_ARCH_ARM64, cs_mode(CS_MODE_LITTLE_ENDIAN)},
  {"x86_64", CS_ARCH_X86, cs_mode(CS_MODE_64)},
  {NULL, CS_ARCH_ALL, cs_mode(CS_MODE_16)}
};

Disassembler *Disassembler::Initialize(const char *arch) {
  csh *new_handle = nullptr;
  // Initialize the Capstone disassembly engine
  for (int i = 0; all_archs[i].name; i++) {
    if (!strcmp(all_archs[i].name, arch)) {
      target = &all_archs[i];
      new_handle = new csh;
      cs_err err = cs_open(all_archs[i].arch, all_archs[i].mode, new_handle);
      if (err != CS_ERR_OK)
        return nullptr;

      break;
    }
  }
  if (!new_handle)
    return nullptr;

  cs_option(*new_handle, CS_OPT_DETAIL, CS_OPT_ON);

  return new Disassembler(new_handle);
}

void Disassembler::is_call_or_ret(struct qemu_insn *insn, bool &is_call, bool &is_ret) {
  // Extract the content of the instruction
  uint8_t insn_buf[16];
  uint64_t insn_vaddr = qemu_plugin_insn_vaddr(insn);
  size_t insn_size = qemu_plugin_insn_data(insn, insn_buf, sizeof(insn_buf));

  // To disassemble it with Capstone
  cs_insn* cs_insn;
  size_t count =
      cs_disasm(*cs_handle, insn_buf, insn_size, insn_vaddr, 0, &cs_insn);

  // And figure out if it's a call or ret
  if (count > 0) {
    // TODO: Handle CS_GRP_IRET and CS_GRP_INT ?
    is_call = cs_insn_group(*cs_handle, cs_insn, CS_GRP_CALL);
    is_ret = cs_insn_group(*cs_handle, cs_insn, CS_GRP_RET);
    cs_free(cs_insn, count);
  }
}

Disassembler::~Disassembler() {
  // Stop the Capstone engine
  cs_close(cs_handle);
}
