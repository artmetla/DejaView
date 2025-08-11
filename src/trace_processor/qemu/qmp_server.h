/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_QEMU_QMP_SERVER_H_
#define SRC_TRACE_PROCESSOR_QEMU_QMP_SERVER_H_

#include "dejaview/ext/base/periodic_task.h"
#include "dejaview/ext/base/temp_file.h"
#include "dejaview/ext/base/unix_socket.h"

namespace dejaview::trace_processor {

class QMPServer : public base::UnixSocket::EventListener {
 public:
  QMPServer(base::TaskRunner *task_runner);
  ~QMPServer() override;

  void SeekTo(uint64_t target_icount);

  std::string sock_path();

  void OnNewIncomingConnection(base::UnixSocket*,
                                std::unique_ptr<base::UnixSocket>) override;
  void OnDataAvailable(base::UnixSocket*) override;

  void SetOnIcountUpdate(std::function<void(uint64_t)> callback);
  void SetOnConnected(std::function<void()> callback);

 private:
  base::TempDir tmp_dir_;
  base::TaskRunner *task_runner_;
  std::unique_ptr<base::UnixSocket> socket_;
  std::unique_ptr<base::UnixSocket> client_socket_;
  base::PeriodicTask icount_poller_;
  std::function<void(uint64_t)> icount_update_;
  std::function<void()> connected_;
  bool version_received_ = false, capabilities_negotiated_ = false, inhibit_status_send_ = false;
  base::PeriodicTask::Args icount_poller_args_;
};

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_QEMU_QMP_SERVER_H_
