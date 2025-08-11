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

#include "dejaview/base/build_config.h"

#include <errno.h>
#include <stdint.h>

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#include <Windows.h>
#include <synchapi.h>
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
#include <sys/eventfd.h>
#include <unistd.h>
#else  // Mac, Fuchsia and other non-Linux UNIXes
#include <unistd.h>
#endif

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/event_fd.h"
#include "dejaview/ext/base/pipe.h"
#include "dejaview/ext/base/utils.h"

namespace dejaview {
namespace base {

EventFd::~EventFd() = default;

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
EventFd::EventFd() {
  event_handle_.reset(
      CreateEventA(/*lpEventAttributes=*/nullptr, /*bManualReset=*/true,
                   /*bInitialState=*/false, /*bInitialState=*/nullptr));
}

void EventFd::Notify() {
  if (!SetEvent(event_handle_.get()))  // 0: fail, !0: success, unlike UNIX.
    DEJAVIEW_DFATAL("EventFd::Notify()");
}

void EventFd::Clear() {
  if (!ResetEvent(event_handle_.get()))  // 0: fail, !0: success, unlike UNIX.
    DEJAVIEW_DFATAL("EventFd::Clear()");
}

#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)

EventFd::EventFd() {
  event_handle_.reset(eventfd(/*initval=*/0, EFD_CLOEXEC | EFD_NONBLOCK));
  DEJAVIEW_CHECK(event_handle_);
}

void EventFd::Notify() {
  const uint64_t value = 1;
  ssize_t ret = write(event_handle_.get(), &value, sizeof(value));
  if (ret <= 0 && errno != EAGAIN)
    DEJAVIEW_DFATAL("EventFd::Notify()");
}

void EventFd::Clear() {
  uint64_t value;
  ssize_t ret =
      DEJAVIEW_EINTR(read(event_handle_.get(), &value, sizeof(value)));
  if (ret <= 0 && errno != EAGAIN)
    DEJAVIEW_DFATAL("EventFd::Clear()");
}

#else

EventFd::EventFd() {
  // Make the pipe non-blocking so that we never block the waking thread (either
  // the main thread or another one) when scheduling a wake-up.
  Pipe pipe = Pipe::Create(Pipe::kBothNonBlock);
  event_handle_ = ScopedPlatformHandle(std::move(pipe.rd).release());
  write_fd_ = std::move(pipe.wr);
}

void EventFd::Notify() {
  const uint64_t value = 1;
  ssize_t ret = write(write_fd_.get(), &value, sizeof(uint8_t));
  if (ret <= 0 && errno != EAGAIN)
    DEJAVIEW_DFATAL("EventFd::Notify()");
}

void EventFd::Clear() {
  // Drain the byte(s) written to the wake-up pipe. We can potentially read
  // more than one byte if several wake-ups have been scheduled.
  char buffer[16];
  ssize_t ret =
      DEJAVIEW_EINTR(read(event_handle_.get(), &buffer[0], sizeof(buffer)));
  if (ret <= 0 && errno != EAGAIN)
    DEJAVIEW_DFATAL("EventFd::Clear()");
}
#endif

}  // namespace base
}  // namespace dejaview
