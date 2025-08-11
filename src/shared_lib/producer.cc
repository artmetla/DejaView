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

#include "dejaview/public/abi/producer_abi.h"

#include "dejaview/tracing/backend_type.h"
#include "dejaview/tracing/tracing.h"
#include "src/shared_lib/reset_for_testing.h"
#include "src/tracing/internal/tracing_muxer_impl.h"

namespace dejaview {
namespace shlib {

void ResetForTesting() {
  auto* muxer = static_cast<internal::TracingMuxerImpl*>(
      internal::TracingMuxerImpl::instance_);
  muxer->AppendResetForTestingCallback([] {
    dejaview::shlib::ResetDataSourceTls();
    dejaview::shlib::ResetTrackEventTls();
  });
  dejaview::Tracing::ResetForTesting();
}

}  // namespace shlib
}  // namespace dejaview

struct DejaViewProducerBackendInitArgs {
  uint32_t shmem_size_hint_kb = 0;
};

struct DejaViewProducerBackendInitArgs*
DejaViewProducerBackendInitArgsCreate() {
  return new DejaViewProducerBackendInitArgs();
}

void DejaViewProducerBackendInitArgsSetShmemSizeHintKb(
    struct DejaViewProducerBackendInitArgs* backend_args,
    uint32_t size) {
  backend_args->shmem_size_hint_kb = size;
}

void DejaViewProducerBackendInitArgsDestroy(
    struct DejaViewProducerBackendInitArgs* backend_args) {
  delete backend_args;
}

void DejaViewProducerInProcessInit(
    const struct DejaViewProducerBackendInitArgs* backend_args) {
  dejaview::TracingInitArgs args;
  args.backends = dejaview::kInProcessBackend;
  args.shmem_size_hint_kb = backend_args->shmem_size_hint_kb;
  dejaview::Tracing::Initialize(args);
}

void DejaViewProducerSystemInit(
    const struct DejaViewProducerBackendInitArgs* backend_args) {
  dejaview::TracingInitArgs args;
  args.backends = dejaview::kSystemBackend;
  args.shmem_size_hint_kb = backend_args->shmem_size_hint_kb;
  dejaview::Tracing::Initialize(args);
}

void DejaViewProducerActivateTriggers(const char* trigger_names[],
                                      uint32_t ttl_ms) {
  std::vector<std::string> triggers;
  for (size_t i = 0; trigger_names[i] != nullptr; i++) {
    triggers.push_back(trigger_names[i]);
  }
  dejaview::Tracing::ActivateTriggers(triggers, ttl_ms);
}
