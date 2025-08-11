/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "dejaview/public/abi/tracing_session_abi.h"

#include <condition_variable>
#include <mutex>

#include "dejaview/tracing/tracing.h"
#include "protos/dejaview/config/trace_config.gen.h"

struct DejaViewTracingSessionImpl* DejaViewTracingSessionSystemCreate() {
  std::unique_ptr<dejaview::TracingSession> tracing_session =
      dejaview::Tracing::NewTrace(dejaview::kSystemBackend);
  return reinterpret_cast<struct DejaViewTracingSessionImpl*>(
      tracing_session.release());
}

struct DejaViewTracingSessionImpl* DejaViewTracingSessionInProcessCreate() {
  std::unique_ptr<dejaview::TracingSession> tracing_session =
      dejaview::Tracing::NewTrace(dejaview::kInProcessBackend);
  return reinterpret_cast<struct DejaViewTracingSessionImpl*>(
      tracing_session.release());
}

void DejaViewTracingSessionSetup(struct DejaViewTracingSessionImpl* session,
                                 void* cfg_begin,
                                 size_t cfg_len) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  dejaview::TraceConfig cfg;
  cfg.ParseFromArray(cfg_begin, cfg_len);
  ts->Setup(cfg);
}

void DejaViewTracingSessionSetStopCb(struct DejaViewTracingSessionImpl* session,
                                     DejaViewTracingSessionStopCb cb,
                                     void* user_arg) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  ts->SetOnStopCallback([session, cb, user_arg]() { cb(session, user_arg); });
}

void DejaViewTracingSessionStartAsync(
    struct DejaViewTracingSessionImpl* session) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  ts->Start();
}

void DejaViewTracingSessionStartBlocking(
    struct DejaViewTracingSessionImpl* session) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  ts->StartBlocking();
}

void DejaViewTracingSessionFlushAsync(
    struct DejaViewTracingSessionImpl* session,
    uint32_t timeout_ms,
    DejaViewTracingSessionFlushCb cb,
    void* user_arg) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  std::function<void(bool)> flush_cb = [](bool) {};
  if (cb) {
    flush_cb = [cb, session, user_arg](bool success) {
      cb(session, success, user_arg);
    };
  }
  ts->Flush(std::move(flush_cb), timeout_ms);
}

bool DejaViewTracingSessionFlushBlocking(
    struct DejaViewTracingSessionImpl* session,
    uint32_t timeout_ms) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  return ts->FlushBlocking(timeout_ms);
}

void DejaViewTracingSessionStopAsync(
    struct DejaViewTracingSessionImpl* session) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  ts->Stop();
}

void DejaViewTracingSessionStopBlocking(
    struct DejaViewTracingSessionImpl* session) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  ts->StopBlocking();
}

void DejaViewTracingSessionReadTraceBlocking(
    struct DejaViewTracingSessionImpl* session,
    DejaViewTracingSessionReadCb callback,
    void* user_arg) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);

  std::mutex mutex;
  std::condition_variable cv;

  bool all_read = false;

  ts->ReadTrace([&mutex, &all_read, &cv, session, callback, user_arg](
                    dejaview::TracingSession::ReadTraceCallbackArgs args) {
    callback(session, static_cast<const void*>(args.data), args.size,
             args.has_more, user_arg);
    std::unique_lock<std::mutex> lock(mutex);
    all_read = !args.has_more;
    if (all_read)
      cv.notify_one();
  });

  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&all_read] { return all_read; });
  }
}

void DejaViewTracingSessionDestroy(struct DejaViewTracingSessionImpl* session) {
  auto* ts = reinterpret_cast<dejaview::TracingSession*>(session);
  delete ts;
}
