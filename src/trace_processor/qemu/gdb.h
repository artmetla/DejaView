/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_QEMU_GDB_H_
#define SRC_TRACE_PROCESSOR_QEMU_GDB_H_

#include "dejaview/ext/base/subprocess.h"

namespace dejaview {

namespace base {
  class TaskRunner;
}

namespace trace_processor {

class Gdb : public base::Subprocess {
public:
  Gdb(base::TaskRunner *task_runner, std::string elfPath, int port);
  ~Gdb();

  using StdoutFunction =
      std::function<void(const uint8_t* /*data*/, size_t /*size*/)>;
  void SetStdoutFunction(StdoutFunction f) {
    stdout_fn_ = std::move(f);
  }

  void SetStoppedFunction(std::function<void()> f) {
    stopped_fn_ = std::move(f);
  }

  void Stdin(const uint8_t *data, size_t len);
  void Resize(uint16_t rows, uint16_t cols);

private:
  StdoutFunction stdout_fn_;
  std::function<void()> stopped_fn_;
  std::unique_ptr<base::Subprocess> process_;
  base::ScopedPlatformHandle pty_master_fd_;
  base::ScopedPlatformHandle pty_slave_fd_;
  base::TaskRunner* const task_runner_;
};

}  // namespace dejaview::trace_processor

}  // namespace dejaview

#endif  // SRC_TRACE_PROCESSOR_QEMU_GDB_H_
