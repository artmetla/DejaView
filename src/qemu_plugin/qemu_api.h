#ifndef SRC_QEMU_PLUGIN_QEMU_API_H_
#define SRC_QEMU_PLUGIN_QEMU_API_H_

#include <cinttypes>
#include <stdbool.h>
#include <stddef.h>

// Symbols required to be recognized as a plugin by QEMU
extern int qemu_plugin_version;

struct qemu_info {
    const char *target_name;
    int min;
    int cur;
    bool emulation;
    int smp_vcpus;
    int max_vcpus;
};
extern __attribute__((visibility("default")))
int qemu_plugin_install(uint64_t id, const struct qemu_info *info,
                        int argc, char **argv);



// QEMU logging facilities
extern void qemu_plugin_outs(const char *str);

// Glib2 functions required to read registers
struct GLibArray {
  char *data;
  unsigned int len;
};
extern GLibArray *g_byte_array_new(void);
extern void *g_byte_array_free(GLibArray *array, bool free);

// Register reading
struct qemu_reg;

typedef struct {
    struct qemu_reg *handle;
    const char *name;
    const char *feature;
} qemu_reg_descriptor;

extern GLibArray *qemu_plugin_get_registers(void);
extern int qemu_plugin_read_register(struct qemu_reg *handle, GLibArray *buf);

// Memory reading
extern bool qemu_plugin_read_memory_vaddr(uint64_t addr, void *data, size_t len);

// Per-VCPU data
struct qemu_scoreboard;

extern struct qemu_scoreboard *qemu_plugin_scoreboard_new(size_t element_size);
extern void qemu_plugin_scoreboard_free(struct qemu_scoreboard *score);

typedef struct {
    struct qemu_scoreboard *score;
    size_t offset;
} qemu_plugin_u64;

extern uint64_t qemu_plugin_u64_get(qemu_plugin_u64 entry,
                                    unsigned int vcpu_id);
extern void qemu_plugin_u64_set(qemu_plugin_u64 entry, unsigned int vcpu_id,
                                uint64_t val);

// VCPU initialization callback
extern void qemu_plugin_register_vcpu_init_cb
    (uint64_t id, void (*cb)(uint64_t id, unsigned int vcpu_id));

// Instruction translation callback
struct qemu_tb;

extern void qemu_plugin_register_vcpu_tb_trans_cb
    (uint64_t id, void (*cb)(uint64_t id, struct qemu_tb *tb));

// Instruction execution callback
enum qemu_cb_flags {
    QEMU_CB_NO_REGS, QEMU_CB_R_REGS, QEMU_CB_RW_REGS,
};

extern void qemu_plugin_register_vcpu_insn_exec_cb
    (struct qemu_insn *insn, void (*cb)(unsigned int vcpu_id, void *userdata),
     enum qemu_cb_flags flags, void *userdata);

enum qemu_plugin_cond {
    QEMU_COND_NEVER, QEMU_COND_ALWAYS, QEMU_COND_EQ, QEMU_COND_NE,
    QEMU_COND_LT, QEMU_COND_LE, QEMU_COND_GT, QEMU_COND_GE,
};

extern void qemu_plugin_register_vcpu_insn_exec_cond_cb
    (struct qemu_insn *insn, void (*cb)(unsigned int vcpu_id, void *userdata),
     enum qemu_cb_flags flags, enum qemu_plugin_cond cond,
     qemu_plugin_u64 entry, uint64_t imm, void *userdata);

enum qemu_plugin_op {
    QEMU_INLINE_ADD_U64, QEMU_INLINE_STORE_U64,
};

extern void qemu_plugin_register_vcpu_insn_exec_inline_per_vcpu
    (struct qemu_insn *insn, enum qemu_plugin_op op,
     qemu_plugin_u64 entry, uint64_t imm);

// Plugin exit callbacks
extern void qemu_plugin_register_atexit_cb
    (uint64_t id, void (*cb)(uint64_t id, void *userdata), void *userdata);

// Instruction metadata
struct qemu_insn;

extern size_t qemu_plugin_tb_n_insns(const struct qemu_tb *tb);
extern struct qemu_insn *qemu_plugin_tb_get_insn(
    const struct qemu_tb *tb, size_t idx);
extern size_t qemu_plugin_insn_data
    (const struct qemu_insn *insn, void *dst, size_t len);
extern uint64_t qemu_plugin_insn_vaddr(const struct qemu_insn *insn);

#endif  // SRC_QEMU_PLUGIN_QEMU_API_H_
