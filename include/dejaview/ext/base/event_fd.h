/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_EXT_BASE_EVENT_FD_H_
#define INCLUDE_DEJAVIEW_EXT_BASE_EVENT_FD_H_

#include "dejaview/base/build_config.h"
#include "dejaview/base/platform_handle.h"
#include "dejaview/ext/base/scoped_file.h"

namespace dejaview {
namespace base {

// A waitable event that can be used with poll/select.
// This is really a wrapper around eventfd_create with a pipe-based fallback
// for other platforms where eventfd is not supported.
class EventFd {
 public:
  EventFd();
  ~EventFd();
  EventFd(EventFd&&) noexcept = default;
  EventFd& operator=(EventFd&&) = default;

  // The non-blocking file descriptor that can be polled to wait for the event.
  PlatformHandle fd() const { return event_handle_.get(); }

  // Can be called from any thread.
  void Notify();

  // Can be called from any thread. If more Notify() are queued a Clear() call
  // can clear all of them (up to 16 per call).
  void Clear();

 private:
  // The eventfd, when eventfd is supported, otherwise this is the read end of
  // the pipe for fallback mode.
  ScopedPlatformHandle event_handle_;

#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) &&   \
    !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) && \
    !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
  // On Mac and other non-Linux UNIX platforms a pipe-based fallback is used.
  // The write end of the wakeup pipe.
  ScopedFile write_fd_;
#endif
};

}  // namespace base
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_EXT_BASE_EVENT_FD_H_
