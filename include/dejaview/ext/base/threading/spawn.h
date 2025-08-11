/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_EXT_BASE_THREADING_SPAWN_H_
#define INCLUDE_DEJAVIEW_EXT_BASE_THREADING_SPAWN_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "dejaview/base/compiler.h"
#include "dejaview/base/flat_set.h"
#include "dejaview/base/platform_handle.h"
#include "dejaview/base/task_runner.h"
#include "dejaview/ext/base/event_fd.h"
#include "dejaview/ext/base/flat_hash_map.h"
#include "dejaview/ext/base/thread_checker.h"
#include "dejaview/ext/base/threading/channel.h"
#include "dejaview/ext/base/threading/future.h"
#include "dejaview/ext/base/threading/poll.h"
#include "dejaview/ext/base/threading/stream.h"
#include "dejaview/ext/base/threading/stream_combinators.h"
#include "dejaview/ext/base/threading/util.h"
#include "dejaview/ext/base/uuid.h"
#include "dejaview/ext/base/weak_ptr.h"

namespace dejaview {
namespace base {

class PolledFuture;

// A RAII object which tracks the polling of a Future.
//
// When this object is dropped, the backing Future will be cancelled as
// soon as possible. In practice, the cancellation happens on the TaskRunner
// thread so there can be some delay.
class SpawnHandle {
 public:
  SpawnHandle(TaskRunner* task_runner, std::function<Future<FVoid>()> fn);

  SpawnHandle(SpawnHandle&&) = default;
  SpawnHandle& operator=(SpawnHandle&&) = default;

  ~SpawnHandle();

 private:
  SpawnHandle(const SpawnHandle&) = delete;
  SpawnHandle& operator=(const SpawnHandle&) = delete;

  TaskRunner* task_runner_ = nullptr;
  std::shared_ptr<std::unique_ptr<PolledFuture>> polled_future_;
};

// "Spawns" a Future<FVoid> on the given TaskRunner and returns an RAII
// SpawnHandle which can be used to cancel the spawn.
//
// Spawning a Future means to poll it to completion. In DejaView, this is done
// by using a TaskRunner object to track FD readiness and polling the Future
// when progress can be made.
//
// The returned SpawnHandle should be stashed as it is responsible for the
// lifetime of the pollling. If the SpawnHandle is dropped, the Future is
// cancelled and dropped ASAP (this happens on the TaskRunner thread so there
// can be some delay).
DEJAVIEW_WARN_UNUSED_RESULT inline SpawnHandle SpawnFuture(
    TaskRunner* task_runner,
    std::function<Future<FVoid>()> fn) {
  return SpawnHandle(task_runner, std::move(fn));
}

// Variant of |SpawnFuture| for a Stream<T> allowing returning items of T.
//
// The Stream<T> returned by this function can be consumed on any thread, not
// just the thread which ran this function.
//
// Dropping the returned stream does not affect the polling of the underlying
// stream (i.e. the stream returned by |fn|); the polled values will simply be
// dropped.
//
// Dropping the returned SpawnHandle causes the underlying stream to be
// cancelled and dropped ASAP (this happens on the TaskRunner thread so there
// can be some delay). The returned channel will return all the values that were
// produced by the underlying stream before the cancellation.
template <typename T>
DEJAVIEW_WARN_UNUSED_RESULT std::pair<SpawnHandle, Stream<T>> SpawnResultStream(
    TaskRunner* runner,
    std::function<Stream<T>()> fn) {
  class AllVoidCollector : public Collector<FVoid, FVoid> {
   public:
    std::optional<FVoid> OnNext(FVoid) override { return std::nullopt; }
    FVoid OnDone() override { return FVoid(); }
  };
  auto channel = std::make_shared<Channel<T>>(4);
  auto control = std::make_shared<Channel<FVoid>>(1);
  SpawnHandle handle(runner, [channel, control, fn = std::move(fn)]() {
    return fn()
        .MapFuture([channel, control](T value) mutable {
          if (control->ReadNonBlocking().is_closed) {
            return base::Future<base::FVoid>(base::FVoid());
          }
          return WriteChannelFuture(channel.get(), std::move(value));
        })
        .Concat(OnDestroyStream<FVoid>([c = channel]() { c->Close(); }))
        .template Collect<base::FVoid>(std::make_unique<AllVoidCollector>());
  });
  Stream<T> stream = ReadChannelStream(channel.get())
                         .Concat(OnDestroyStream<T>([channel, control]() {
                           // Close the control stream and drain an element from
                           // the channel to unblock it in case it was blocked.
                           // NOTE: the ordering here is important as we could
                           // deadlock if it was the other way around!
                           control->Close();
                           base::ignore_result(channel->ReadNonBlocking());
                         }));
  return std::make_pair(std::move(handle), std::move(stream));
}

// Variant of |SpawnResultStream| but for Future<T>.
template <typename T>
DEJAVIEW_WARN_UNUSED_RESULT inline std::pair<SpawnHandle, Future<T>>
SpawnResultFuture(TaskRunner* task_runner, std::function<Future<T>()> fn) {
  auto [handle, stream] = SpawnResultStream<T>(
      task_runner, [fn = std::move(fn)]() { return StreamFromFuture(fn()); });
  return std::make_pair(std::move(handle), std::move(stream).Collect(
                                               ToFutureCheckedCollector<T>()));
}

}  // namespace base
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_EXT_BASE_THREADING_SPAWN_H_
