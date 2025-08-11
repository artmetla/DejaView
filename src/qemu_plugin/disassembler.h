#ifndef SRC_QEMU_PLUGIN_DISASSEMBLER_H_
#define SRC_QEMU_PLUGIN_DISASSEMBLER_H_

#include <stddef.h>

#include <capstone/capstone.h>

struct qemu_insn;

class Disassembler {
public:
  static Disassembler *Initialize(const char *arch);
  void is_call_or_ret(struct qemu_insn *insn, bool &is_call, bool &_is_ret);
  ~Disassembler();

private:
  explicit Disassembler(csh *handle) : cs_handle(handle) {}

  csh *cs_handle;
};

#endif  // SRC_QEMU_PLUGIN_DISASSEMBLER_H_
