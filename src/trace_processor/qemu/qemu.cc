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

#include "src/trace_processor/qemu/qemu.h"

#include <filesystem>

#include "dejaview/base/status.h"
#include "dejaview/base/task_runner.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/trace_processor/trace_processor.h"

namespace dejaview::trace_processor {

namespace {

int FreeTcpPort() {
  base::UnixSocketRaw socket =
    base::UnixSocketRaw::CreateMayFail(base::SockFamily::kInet, base::SockType::kStream);
  if (!socket) {
    return -1;
  }

  if (!socket.Bind("127.0.0.1:0")) {
    return -1;
  }

  std::string full_address = socket.GetSockAddr();
  if (full_address.empty()) {
    return -1;
  }

  size_t colon_pos = full_address.rfind(':');
  if (colon_pos == std::string::npos) {
    return -1;
  }

  std::string port_str = full_address.substr(colon_pos + 1);
  auto port = base::StringToInt32(port_str);
  return port.value_or(-1);
}

std::string AbsolutePath(const std::string& pathStr, const std::string& relativeToStr) {
  std::filesystem::path path(pathStr);
  std::filesystem::path relativeTo(relativeToStr);
  if (path.is_absolute()) {
    return path.string();
  } else {
    return (relativeTo / path).lexically_normal().string();
  }
}

std::optional<std::string> ReplayCwd(TraceProcessor &trace_processor) {
  auto cwd_sql = "SELECT str_value FROM metadata WHERE name = 'qemu_record_cwd'";
  auto it = trace_processor.ExecuteQuery(cwd_sql);
  if (!it.Next())
    return {};
  return it.Get(0).AsString();
}

std::vector<std::string> ReplayCmd(TraceProcessor &trace_processor, std::string &elfPath) {
  std::vector<std::string> ret;
  auto cmdSql = "SELECT str_value FROM metadata WHERE name = 'qemu_record_cmd'";
  bool isIcount = false;
  for (auto it = trace_processor.ExecuteQuery(cmdSql); it.Next();) {
    auto val = std::string(it.Get(0).AsString());

    // Skip flags without parameters
    if (val == "-s" || val == "-S")
      continue;

    // Skip flags with parameters
    if (val == "-gdb") {
      it.Next();
      continue;
    }

    // But parse the arguments provided to the plugin
    if (val == "-plugin") {
      it.Next();
      val = std::string(it.Get(0).AsString());
      size_t start = 0;
      size_t end = val.find(",", start);
      while (end != std::string::npos) {
          auto arg = val.substr(start, end - start);

          size_t eq_pos = arg.find('=');
          if (eq_pos != std::string::npos) {
            std::string key = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);

            if (key == "symbols_from") {
              elfPath = std::string(value);
              break;
            }
          }

          start = end + 1;
          end = val.find(",", start);
      }

      continue;
    }

    // Replay instead of recording
    if (isIcount)
      val = base::ReplaceAll(val, "record", "replay");
    isIcount = (val == "-icount");

    ret.push_back(val);
  }
  return ret;
}

}

base::Status Qemu::Debug(uint64_t target_icount, TraceProcessor &trace_processor) {
  debugger_.reset();

  process_ = std::make_unique<base::Subprocess>();
  process_->args.stdin_mode = base::Subprocess::InputMode::kDevNull;
  process_->args.stdout_mode = base::Subprocess::OutputMode::kDevNull;
  process_->args.stderr_mode = base::Subprocess::OutputMode::kDevNull;

  process_->args.cwd = ReplayCwd(trace_processor);
  std::string elfPath;
  process_->args.exec_cmd = ReplayCmd(trace_processor, elfPath);
  if (process_->args.cwd.has_value()) {
    elfPath = AbsolutePath(elfPath, process_->args.cwd.value());
  }

  int gdbPort = FreeTcpPort();
  if (gdbPort == -1) {
    return base::ErrStatus("Failed to find a free port");
  }
  process_->args.exec_cmd.push_back("-gdb");
  process_->args.exec_cmd.push_back("tcp::" + std::to_string(gdbPort));
  process_->args.exec_cmd.push_back("-S");

  qmpServer_ = std::make_unique<QMPServer>(task_runner_);
  target_icount_ = target_icount;
  qmpServer_->SetOnConnected([this] () {
    qmpServer_->SeekTo(target_icount_);
  });
  qmpServer_->SetOnIcountUpdate([this, gdbPort, elfPath] (uint64_t current_icount) {
    icount_changed_fn_(current_icount);

    if (current_icount == target_icount_) {
      target_icount_ = std::numeric_limits<uint64_t>::max();
      debugger_ = std::make_unique<Gdb>(task_runner_, elfPath, gdbPort);
      debugger_->SetStdoutFunction([this] (const uint8_t* data, size_t size) {
        debugger_stdout_fn_(data, size);
      });
      debugger_->SetStoppedFunction([this] () {
        debugger_stopped_fn_();
      });
      debugger_started_fn_();
      debugger_->Start();
    }
  });
  process_->args.exec_cmd.push_back("-qmp");
  process_->args.exec_cmd.push_back("unix:" + qmpServer_->sock_path());

  process_->Start();
  if (process_->status() != base::Subprocess::kRunning)
    return base::ErrStatus("QEMU failed to start");

  return base::OkStatus();
}

}
