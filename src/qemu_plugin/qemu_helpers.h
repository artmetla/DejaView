#ifndef SRC_QEMU_PLUGIN_QEMU_HELPERS_H_
#define SRC_QEMU_PLUGIN_QEMU_HELPERS_H_

struct qemu_plugin_register;

#include <sstream>
#include <functional>

extern "C" {
#include "qemu_api.h"
}

#define QEMU_LOG() QemuLog().GetStream()

class QemuLog {
public:
  explicit QemuLog() { }
  std::ostringstream& GetStream() { return m_stringStream; }
  ~QemuLog() { qemu_plugin_outs(m_stringStream.str().c_str()); }

private:
  std::ostringstream m_stringStream;
};

qemu_reg *get_gs_base_handle(void);

#endif  // SRC_QEMU_PLUGIN_QEMU_HELPERS_H_
