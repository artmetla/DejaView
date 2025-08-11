/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/profiling/perf/proc_descriptors.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dejaview/ext/base/string_utils.h"

namespace dejaview {

ProcDescriptorDelegate::~ProcDescriptorDelegate() {}

ProcDescriptorGetter::~ProcDescriptorGetter() {}

// DirectDescriptorGetter:

DirectDescriptorGetter::~DirectDescriptorGetter() {}

void DirectDescriptorGetter::SetDelegate(ProcDescriptorDelegate* delegate) {
  delegate_ = delegate;
}

void DirectDescriptorGetter::GetDescriptorsForPid(pid_t pid) {
  base::StackString<128> dir_buf("/proc/%d", pid);
  auto dir_fd = base::ScopedFile(
      open(dir_buf.c_str(), O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  if (!dir_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      DEJAVIEW_PLOG("Failed to open [%s]", dir_buf.c_str());

    return;
  }

  struct stat stat_buf;
  if (fstat(dir_fd.get(), &stat_buf) == -1) {
    DEJAVIEW_PLOG("Failed to stat [%s]", dir_buf.c_str());
    return;
  }

  auto maps_fd =
      base::ScopedFile{openat(dir_fd.get(), "maps", O_RDONLY | O_CLOEXEC)};
  if (!maps_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      DEJAVIEW_PLOG("Failed to open %s/maps", dir_buf.c_str());

    return;
  }

  auto mem_fd =
      base::ScopedFile{openat(dir_fd.get(), "mem", O_RDONLY | O_CLOEXEC)};
  if (!mem_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      DEJAVIEW_PLOG("Failed to open %s/mem", dir_buf.c_str());

    return;
  }

  delegate_->OnProcDescriptors(pid, stat_buf.st_uid, std::move(maps_fd),
                               std::move(mem_fd));
}

// AndroidRemoteDescriptorGetter:

AndroidRemoteDescriptorGetter::~AndroidRemoteDescriptorGetter() = default;

void AndroidRemoteDescriptorGetter::SetDelegate(
    ProcDescriptorDelegate* delegate) {
  delegate_ = delegate;
}

#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)
void AndroidRemoteDescriptorGetter::GetDescriptorsForPid(pid_t) {
  DEJAVIEW_FATAL("Unexpected build type for AndroidRemoteDescriptorGetter");
}
#else
void AndroidRemoteDescriptorGetter::GetDescriptorsForPid(pid_t pid) {
  constexpr static int kPerfProfilerSignalValue = 1;
  constexpr static int kProfilerSignal = __SIGRTMIN + 4;

  DEJAVIEW_DLOG("Sending signal to pid [%d]", pid);
  union sigval signal_value;
  signal_value.sival_int = kPerfProfilerSignalValue;
  if (sigqueue(pid, kProfilerSignal, signal_value) != 0 && errno != ESRCH) {
    DEJAVIEW_DPLOG("Failed sigqueue(%d)", pid);
  }
}
#endif

void AndroidRemoteDescriptorGetter::OnNewIncomingConnection(
    base::UnixSocket*,
    std::unique_ptr<base::UnixSocket> new_connection) {
  DEJAVIEW_DLOG("remote fds: new connection from pid [%d]",
                static_cast<int>(new_connection->peer_pid_linux()));

  active_connections_.emplace(new_connection.get(), std::move(new_connection));
}

void AndroidRemoteDescriptorGetter::OnDisconnect(base::UnixSocket* self) {
  DEJAVIEW_DLOG("remote fds: disconnect from pid [%d]",
                static_cast<int>(self->peer_pid_linux()));

  auto it = active_connections_.find(self);
  DEJAVIEW_CHECK(it != active_connections_.end());
  active_connections_.erase(it);
}

// Note: this callback will fire twice for a given connection. Once for the file
// descriptors, and once during the disconnect (with 0 bytes available in the
// socket).
void AndroidRemoteDescriptorGetter::OnDataAvailable(base::UnixSocket* self) {
  // Expect two file descriptors (maps, followed by mem).
  base::ScopedFile fds[2];
  char buf[1];
  size_t received_bytes =
      self->Receive(buf, sizeof(buf), fds, base::ArraySize(fds));

  DEJAVIEW_DLOG("remote fds: received %zu bytes", received_bytes);
  if (!received_bytes)
    return;

  delegate_->OnProcDescriptors(self->peer_pid_linux(), self->peer_uid_posix(),
                               std::move(fds[0]), std::move(fds[1]));
}

}  // namespace dejaview
