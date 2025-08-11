#include "dejaview/ext/base/string_utils.h"

#include <string>

#include "disassembler.h"
#include "qemu_helpers.h"
#include "tracer.h"

extern "C" {
#include "qemu_api.h"
}

// This indicates to QEMU the version we support.
__attribute__((visibility("default"))) int qemu_plugin_version = 4;

// These are allocated dynamically otherwise the C++ atexit constructors will
// destroy them before plugin_exit() gets called.
static Disassembler *disassembler;
static Tracer *tracer;

// This keeps per-CPU counters to:
// - Count instructions
// - Remember if the previous instruction was a call
typedef struct {
  uint64_t insn_count;
  uint64_t last_insn_is_call;
} CpuScoreboard;
static struct qemu_scoreboard* cpu_sb;
static qemu_plugin_u64 insn_count;
static qemu_plugin_u64 last_insn_is_call;

// When landing somewhere after a call, log the instruction
static void log_call_landing(unsigned int vcpu_id, void* udata) {
  // Reset the last instruction's type
  qemu_plugin_u64_set(last_insn_is_call, vcpu_id, 0);

  // Use the number of executed instructions as "timestamp"
  uint64_t ts = qemu_plugin_u64_get(insn_count, vcpu_id);

  // And fill it with information depending on whether it's a call or ret
  uint64_t addr = *(static_cast<uint64_t *>(udata));
  tracer->LogCall(addr, vcpu_id, ts);
}

// When returning from somewhere, close the slice we opened earlier
static void log_ret(unsigned int vcpu_id, void* /*udata*/) {
  uint64_t ts = qemu_plugin_u64_get(insn_count, vcpu_id);
  tracer->LogRet(vcpu_id, ts);
}

// When TCG translates a new translation block, register callbacks for
// interesting instructions (calls/rets and possible landing pads)
static void vcpu_tb_trans(uint64_t /*id*/, struct qemu_tb* tb) {
  // Only the first instruction of a block could be landed on by a call or ret
  struct qemu_insn* first_insn = qemu_plugin_tb_get_insn(tb, 0);
  // We never free those: :(
  uint64_t *first_insn_vaddr = new uint64_t(qemu_plugin_insn_vaddr(first_insn));

  // If this instruction is executed immediately after a call, log it
  qemu_plugin_register_vcpu_insn_exec_cond_cb(
      first_insn, log_call_landing, QEMU_CB_R_REGS, QEMU_COND_NE,
      last_insn_is_call, 0, first_insn_vaddr);

  // We only need the correct instructions count at basic block boundaries.
  // Call callbacks know they are 1 instruction ahead and manually keep
  // track of this discrepancy for efficiency.
  size_t n_insns = qemu_plugin_tb_n_insns(tb);
  struct qemu_insn* last_insn = qemu_plugin_tb_get_insn(tb, n_insns - 1);
  for (size_t i = 0; i < n_insns; i++) {
    struct qemu_insn* insn = qemu_plugin_tb_get_insn(tb, i);
    qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
        insn, QEMU_INLINE_ADD_U64, insn_count, 1);
  }

  // Only the last instruction of a block could be a call or ret, so figure out
  // if it's either of those
  bool is_call = false, is_ret = false;
  disassembler->is_call_or_ret(last_insn, is_call, is_ret);

  // Set the appropriate "last instruction type" per-cpu flag on those
  if (is_call)
    qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu(
        last_insn, QEMU_INLINE_STORE_U64, last_insn_is_call, 1);
  else if (is_ret)
    qemu_plugin_register_vcpu_insn_exec_cb(
        last_insn, log_ret, QEMU_CB_NO_REGS, nullptr);
}

// Initialize the "scoreboard" of a new online vCPU
static void vcpu_init(uint64_t /*id*/, unsigned int vcpu_id) {
  qemu_plugin_u64_set(last_insn_is_call, vcpu_id, 0);
  qemu_plugin_u64_set(insn_count, vcpu_id, 0);
}

// Plugin exit point
static void plugin_exit(uint64_t /*id*/, void* /*p*/) {
  tracer->WriteToDisk();

  delete tracer;
  delete disassembler;

  qemu_plugin_scoreboard_free(cpu_sb);
}

// Plugin entry point
__attribute__((visibility("default")))
int qemu_plugin_install(uint64_t id, const struct qemu_info* info, int argc,
                        char** argv) {
  // Parse arguments
  std::string kernel_path;
  std::string starting_from;
  std::string dest_path("trace.dvtrace");
  uint64_t min_insns = 0;
  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];
    size_t eq_pos = arg.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = arg.substr(0, eq_pos);
      std::string value = arg.substr(eq_pos + 1);

      if (key == "symbols_from") {
        kernel_path = std::string(value);
      } else if (key == "out") {
        dest_path = std::string(value);
      } else if (key == "starting_from") {
        starting_from = std::string(value);
      } else if (key == "min_insns") {
        std::optional<uint64_t> min = dejaview::base::StringToUInt64(value);
        if (!min.has_value()) {
          QEMU_LOG() << "Bad value for min_insns: " << value << "\n";
          return 1;
        }
        min_insns = min.value();
      } else {
        QEMU_LOG() << "Bad argument: " << arg << "\n";
        return 1;
      }
    } else {
      QEMU_LOG() << "Bad argument format: " << arg << "\n";
      return 1;
    }
  }

  // Initialize objects
  disassembler = Disassembler::Initialize(info->target_name);
  if (!disassembler) {
    QEMU_LOG() << "Disassembler initialization failed \n";
    return 1;
  }

  tracer = new Tracer(dest_path, kernel_path, starting_from, min_insns);

  // QEMU's per-CPU scoreboard keeps track of instruction counts and types
  cpu_sb = qemu_plugin_scoreboard_new(sizeof(CpuScoreboard));
  insn_count.score = cpu_sb;
  insn_count.offset = offsetof(CpuScoreboard, insn_count);
  last_insn_is_call.score = cpu_sb;
  last_insn_is_call.offset = offsetof(CpuScoreboard, last_insn_is_call);

  // Register a callback for each vCPU initialization to populate this
  qemu_plugin_register_vcpu_init_cb(id, vcpu_init);
  // Register a callback for the translation of each new basic block
  qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
  // And register an exit callback to save the trace to a file
  qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

  return 0;
}
