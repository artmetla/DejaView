/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "src/tracing/internal/tracing_muxer_fake.h"

namespace dejaview {
namespace internal {
namespace {

DEJAVIEW_NORETURN void FailUninitialized() {
  DEJAVIEW_FATAL(
      "Tracing not initialized. Call dejaview::Tracing::Initialize() first.");
}

}  // namespace

#if DEJAVIEW_HAS_NO_DESTROY()
// static
DEJAVIEW_NO_DESTROY TracingMuxerFake::FakePlatform
    TracingMuxerFake::FakePlatform::instance{};
// static
DEJAVIEW_NO_DESTROY TracingMuxerFake TracingMuxerFake::instance{};
#endif  // DEJAVIEW_HAS_NO_DESTROY()

TracingMuxerFake::~TracingMuxerFake() = default;

TracingMuxerFake::FakePlatform::~FakePlatform() = default;

Platform::ThreadLocalObject*
TracingMuxerFake::FakePlatform::GetOrCreateThreadLocalObject() {
  FailUninitialized();
}

std::unique_ptr<base::TaskRunner>
TracingMuxerFake::FakePlatform::CreateTaskRunner(const CreateTaskRunnerArgs&) {
  FailUninitialized();
}

std::string TracingMuxerFake::FakePlatform::GetCurrentProcessName() {
  FailUninitialized();
}

bool TracingMuxerFake::RegisterDataSource(const DataSourceDescriptor&,
                                          DataSourceFactory,
                                          DataSourceParams,
                                          bool,
                                          DataSourceStaticState*) {
  FailUninitialized();
}

void TracingMuxerFake::UpdateDataSourceDescriptor(
    const DataSourceDescriptor&,
    const DataSourceStaticState*) {
  FailUninitialized();
}

std::unique_ptr<TraceWriterBase> TracingMuxerFake::CreateTraceWriter(
    DataSourceStaticState*,
    uint32_t,
    DataSourceState*,
    BufferExhaustedPolicy) {
  FailUninitialized();
}

void TracingMuxerFake::DestroyStoppedTraceWritersForCurrentThread() {
  FailUninitialized();
}

void TracingMuxerFake::RegisterInterceptor(
    const InterceptorDescriptor&,
    InterceptorFactory,
    InterceptorBase::TLSFactory,
    InterceptorBase::TracePacketCallback) {
  FailUninitialized();
}

void TracingMuxerFake::ActivateTriggers(const std::vector<std::string>&,
                                        uint32_t) {
  FailUninitialized();
}

}  // namespace internal
}  // namespace dejaview
