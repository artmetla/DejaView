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

#include "dejaview/tracing/internal/system_tracing_backend.h"

#include "dejaview/base/logging.h"
#include "dejaview/base/task_runner.h"
#include "dejaview/ext/tracing/core/tracing_service.h"
#include "dejaview/ext/tracing/ipc/producer_ipc_client.h"
#include "dejaview/tracing/default_socket.h"

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_SYSTEM_CONSUMER)
#include "dejaview/ext/tracing/ipc/consumer_ipc_client.h"
#endif

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#include "src/tracing/ipc/shared_memory_windows.h"
#else
#include "src/tracing/ipc/posix_shared_memory.h"
#endif

namespace dejaview {
namespace internal {

// static
TracingProducerBackend* SystemProducerTracingBackend::GetInstance() {
  static auto* instance = new SystemProducerTracingBackend();
  return instance;
}

SystemProducerTracingBackend::SystemProducerTracingBackend() {}

std::unique_ptr<ProducerEndpoint> SystemProducerTracingBackend::ConnectProducer(
    const ConnectProducerArgs& args) {
  DEJAVIEW_DCHECK(args.task_runner->RunsTasksOnCurrentThread());

  std::unique_ptr<SharedMemory> shm;
  std::unique_ptr<SharedMemoryArbiter> arbiter;
  uint32_t shmem_size_hint = args.shmem_size_hint_bytes;
  uint32_t shmem_page_size_hint = args.shmem_page_size_hint_bytes;
  if (args.use_producer_provided_smb) {
    if (shmem_size_hint == 0)
      shmem_size_hint = TracingService::kDefaultShmSize;
    if (shmem_page_size_hint == 0)
      shmem_page_size_hint = TracingService::kDefaultShmPageSize;
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
    shm = SharedMemoryWindows::Create(shmem_size_hint);
#else
    shm = PosixSharedMemory::Create(shmem_size_hint);
#endif
    arbiter = SharedMemoryArbiter::CreateUnboundInstance(
        shm.get(), shmem_page_size_hint, SharedMemoryABI::ShmemMode::kDefault);
  }

  ipc::Client::ConnArgs conn_args(GetProducerSocket(), true);
  auto endpoint = ProducerIPCClient::Connect(
      std::move(conn_args), args.producer, args.producer_name, args.task_runner,
      TracingService::ProducerSMBScrapingMode::kEnabled, shmem_size_hint,
      shmem_page_size_hint, std::move(shm), std::move(arbiter),
      args.create_socket_async);
  DEJAVIEW_CHECK(endpoint);
  return endpoint;
}

// static
TracingConsumerBackend* SystemConsumerTracingBackend::GetInstance() {
  static auto* instance = new SystemConsumerTracingBackend();
  return instance;
}

SystemConsumerTracingBackend::SystemConsumerTracingBackend() {}

std::unique_ptr<ConsumerEndpoint> SystemConsumerTracingBackend::ConnectConsumer(
    const ConnectConsumerArgs& args) {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_SYSTEM_CONSUMER)
  auto endpoint = ConsumerIPCClient::Connect(GetConsumerSocket(), args.consumer,
                                             args.task_runner);
  DEJAVIEW_CHECK(endpoint);
  return endpoint;
#else
  base::ignore_result(args);
  DEJAVIEW_FATAL("System backend consumer support disabled");
  return nullptr;
#endif
}

}  // namespace internal
}  // namespace dejaview
