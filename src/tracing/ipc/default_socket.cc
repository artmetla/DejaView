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

#include "dejaview/tracing/default_socket.h"

#include "dejaview/base/build_config.h"
#include "dejaview/base/logging.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/ext/base/utils.h"
#include "dejaview/ext/ipc/basic_types.h"
#include "dejaview/ext/tracing/core/basic_types.h"

#include <stdlib.h>

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
#include <unistd.h>
#endif

namespace dejaview {
namespace {

const char* kRunDejaViewBaseDir = "/run/dejaview/";

// On Linux and CrOS, check /run/dejaview/ before using /tmp/ as the socket
// base directory.
bool UseRunDejaViewBaseDir() {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX)
  // Note that the trailing / in |kRunDejaViewBaseDir| ensures we are checking
  // against a directory, not a file.
  int res = DEJAVIEW_EINTR(access(kRunDejaViewBaseDir, X_OK));
  if (!res)
    return true;

  // If the path doesn't exist (ENOENT), fail silently to the caller. Otherwise,
  // fail with an explicit error message.
  if (errno != ENOENT
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_CHROMIUM_BUILD)
      // access(2) won't return EPERM, but Chromium sandbox returns EPERM if the
      // sandbox doesn't allow the call (e.g. in the child processes).
      && errno != EPERM
#endif
  ) {
    DEJAVIEW_PLOG("%s exists but cannot be accessed. Falling back on /tmp/ ",
                  kRunDejaViewBaseDir);
  }
  return false;
#else
  base::ignore_result(kRunDejaViewBaseDir);
  return false;
#endif
}

}  // anonymous namespace

const char* GetProducerSocket() {
  const char* name = getenv("DEJAVIEW_PRODUCER_SOCK_NAME");
  if (name == nullptr) {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
    name = "127.0.0.1:32278";
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
    name = "/dev/socket/traced_producer";
#else
    // Use /run/dejaview if it exists. Then fallback to /tmp.
    static const char* producer_socket =
        UseRunDejaViewBaseDir() ? "/run/dejaview/traced-producer.sock"
                                : "/tmp/dejaview-producer";
    name = producer_socket;
#endif
  }
  base::ignore_result(UseRunDejaViewBaseDir);  // Silence unused func warnings.
  return name;
}

const char* GetRelaySocket() {
  // The relay socket is optional and is connected only when the env var is set.
  return getenv("DEJAVIEW_RELAY_SOCK_NAME");
}

std::vector<std::string> TokenizeProducerSockets(
    const char* producer_socket_names) {
  return base::SplitString(producer_socket_names, ",");
}

const char* GetConsumerSocket() {
  const char* name = getenv("DEJAVIEW_CONSUMER_SOCK_NAME");
  if (name == nullptr) {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
    name = "127.0.0.1:32279";
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
    name = "/dev/socket/traced_consumer";
#else
    // Use /run/dejaview if it exists. Then fallback to /tmp.
    static const char* consumer_socket =
        UseRunDejaViewBaseDir() ? "/run/dejaview/traced-consumer.sock"
                                : "/tmp/dejaview-consumer";
    name = consumer_socket;
#endif
  }
  return name;
}

}  // namespace dejaview
