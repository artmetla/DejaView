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

#ifndef INCLUDE_DEJAVIEW_EXT_BASE_THREAD_UTILS_H_
#define INCLUDE_DEJAVIEW_EXT_BASE_THREAD_UTILS_H_

#include <string>

#include "dejaview/base/build_config.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/base/export.h"

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
#include <pthread.h>
#include <string.h>
#include <algorithm>
#endif

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
#include <sys/prctl.h>
#endif

// Internal implementation utils that aren't as widely useful/supported as
// base/thread_utils.h.

namespace dejaview {
namespace base {

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_LINUX) ||   \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
// Sets the "comm" of the calling thread to the first 15 chars of the given
// string.
inline bool MaybeSetThreadName(const std::string& name) {
  char buf[16] = {};
  StringCopy(buf, name.c_str(), sizeof(buf));

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_APPLE)
  return pthread_setname_np(buf) == 0;
#else
  return pthread_setname_np(pthread_self(), buf) == 0;
#endif
}

inline bool GetThreadName(std::string& out_result) {
  char buf[16] = {};
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
  if (prctl(PR_GET_NAME, buf) != 0)
    return false;
#else
  if (pthread_getname_np(pthread_self(), buf, sizeof(buf)) != 0)
    return false;
#endif
  out_result = std::string(buf);
  return true;
}

#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)

DEJAVIEW_EXPORT_COMPONENT bool MaybeSetThreadName(const std::string& name);
DEJAVIEW_EXPORT_COMPONENT bool GetThreadName(std::string& out_result);

#else
inline bool MaybeSetThreadName(const std::string&) {
  return false;
}
inline bool GetThreadName(std::string&) {
  return false;
}
#endif

}  // namespace base
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_EXT_BASE_THREAD_UTILS_H_
