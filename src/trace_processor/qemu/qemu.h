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

#ifndef SRC_TRACE_PROCESSOR_QEMU_QEMU_H_
#define SRC_TRACE_PROCESSOR_QEMU_QEMU_H_

#include "src/trace_processor/qemu/gdb.h"
#include "src/trace_processor/qemu/qmp_server.h"

#include "dejaview/ext/base/subprocess.h"

namespace dejaview {

namespace base {
  class Status;
  class TaskRunner;
}

namespace trace_processor {

class TraceProcessor;

class Qemu {
public:
  Qemu(base::TaskRunner *task_runner) : task_runner_(task_runner) {}
  base::Status Debug(uint64_t target_icount, TraceProcessor &trace_processor);

  using IcountChangedFunction =
      std::function<void(uint64_t /*icount*/)>;
  void SetIcountChangedFunction(IcountChangedFunction f) {
    icount_changed_fn_ = std::move(f);
  }

  void SetDebuggerStdoutFunction(Gdb::StdoutFunction f) {
    debugger_stdout_fn_ = std::move(f);
  }

  void SetDebuggerStartedFunction(std::function<void()> f) {
    debugger_started_fn_ = std::move(f);
  }

  void SetDebuggerStoppedFunction(std::function<void()> f) {
    debugger_stopped_fn_ = std::move(f);
  }

  void DebuggerStdin(const uint8_t *data, size_t len) {
    debugger_->Stdin(data, len);
  }

  void DebuggerResize(uint16_t rows, uint16_t cols) {
    debugger_->Resize(rows, cols);
  }

private:
  IcountChangedFunction icount_changed_fn_;
  Gdb::StdoutFunction debugger_stdout_fn_;
  std::function<void()> debugger_started_fn_, debugger_stopped_fn_;
  std::unique_ptr<base::Subprocess> process_;
  std::unique_ptr<Gdb> debugger_;
  std::unique_ptr<QMPServer> qmpServer_;
  base::TaskRunner* const task_runner_;
  uint64_t target_icount_;
};

}  // namespace dejaview::trace_processor

}  // namespace dejaview

#endif  // SRC_TRACE_PROCESSOR_QEMU_QEMU_H_

