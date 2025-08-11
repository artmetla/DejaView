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

#include "dejaview/base/build_config.h"

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_FUCHSIA) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)

#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/thread_task_runner.h"
#include "dejaview/tracing/internal/tracing_tls.h"
#include "dejaview/tracing/platform.h"
#include "dejaview/tracing/trace_writer_base.h"

#include <pthread.h>
#include <stdlib.h>

namespace dejaview {

namespace {

class PlatformPosix : public Platform {
 public:
  PlatformPosix();
  ~PlatformPosix() override;

  ThreadLocalObject* GetOrCreateThreadLocalObject() override;

  std::unique_ptr<base::TaskRunner> CreateTaskRunner(
      const CreateTaskRunnerArgs&) override;
  std::string GetCurrentProcessName() override;
  void Shutdown() override;

 private:
  pthread_key_t tls_key_{};
};

PlatformPosix* g_instance = nullptr;

using ThreadLocalObject = Platform::ThreadLocalObject;

PlatformPosix::PlatformPosix() {
  DEJAVIEW_CHECK(!g_instance);
  g_instance = this;
  auto tls_dtor = [](void* obj) {
    // The Posix TLS implementation resets the key before calling this dtor.
    // Here we re-reset it to the object we are about to delete. This is to
    // handle re-entrant usages of tracing in the PostTask done during the dtor
    // (see comments in TracingTLS::~TracingTLS()). Chromium's platform
    // implementation (which does NOT use this platform impl) has a similar
    // workaround (https://crrev.com/c/2748300).
    pthread_setspecific(g_instance->tls_key_, obj);
    delete static_cast<ThreadLocalObject*>(obj);
    pthread_setspecific(g_instance->tls_key_, nullptr);
  };
  DEJAVIEW_CHECK(pthread_key_create(&tls_key_, tls_dtor) == 0);
}

PlatformPosix::~PlatformPosix() {
  // pthread_key_delete doesn't call destructors, so do it manually for the
  // calling thread.
  void* tls_ptr = pthread_getspecific(tls_key_);
  delete static_cast<ThreadLocalObject*>(tls_ptr);

  pthread_key_delete(tls_key_);
  g_instance = nullptr;
}

void PlatformPosix::Shutdown() {
  DEJAVIEW_CHECK(g_instance == this);
  delete this;
  DEJAVIEW_CHECK(!g_instance);
  // We're not clearing out the instance in GetDefaultPlatform() since it's not
  // possible to re-initialize DejaView after calling this function anyway.
}

ThreadLocalObject* PlatformPosix::GetOrCreateThreadLocalObject() {
  // In chromium this should be implemented using base::ThreadLocalStorage.
  void* tls_ptr = pthread_getspecific(tls_key_);

  // This is needed to handle re-entrant calls during TLS dtor.
  // See comments in platform.cc and aosp/1712371 .
  ThreadLocalObject* tls = static_cast<ThreadLocalObject*>(tls_ptr);
  if (!tls) {
    tls = ThreadLocalObject::CreateInstance().release();
    pthread_setspecific(tls_key_, tls);
  }
  return tls;
}

std::unique_ptr<base::TaskRunner> PlatformPosix::CreateTaskRunner(
    const CreateTaskRunnerArgs& args) {
  return std::unique_ptr<base::TaskRunner>(new base::ThreadTaskRunner(
      base::ThreadTaskRunner::CreateAndStart(args.name_for_debugging)));
}

std::string PlatformPosix::GetCurrentProcessName() {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
  std::string cmdline;
  base::ReadFile("/proc/self/cmdline", &cmdline);
  return cmdline.substr(0, cmdline.find('\0'));
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
  return std::string(getprogname());
#else
  return "unknown_producer";
#endif
}

}  // namespace

// static
Platform* Platform::GetDefaultPlatform() {
  static PlatformPosix* instance = new PlatformPosix();
  return instance;
}

}  // namespace dejaview
#endif  // OS_LINUX || OS_ANDROID || OS_APPLE || OS_FUCHSIA
