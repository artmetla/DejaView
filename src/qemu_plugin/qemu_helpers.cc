#include "qemu_helpers.h"

#include <string.h>

extern "C" {
#include "qemu_api.h"
}

static qemu_reg *get_reg_handle(const char *reg_name) {
  qemu_reg *ret = nullptr;

  GLibArray *registers = qemu_plugin_get_registers();
  qemu_reg_descriptor *descriptors = reinterpret_cast<qemu_reg_descriptor *>(registers->data);
  for (unsigned int i = 0; i < registers->len; i++) {
    if (strcmp(descriptors[i].name, reg_name) == 0) {
      ret = descriptors[i].handle;
      break;
    }
  }
  free(registers);
  return ret;
}

qemu_reg *get_gs_base_handle(void) {
  static qemu_reg *ret = nullptr;
  if (!ret)
    ret = get_reg_handle("gs_base");
  return ret;
}
