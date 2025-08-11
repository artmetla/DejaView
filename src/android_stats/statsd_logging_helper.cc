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

#include "src/android_stats/statsd_logging_helper.h"

#include <cstdint>
#include <string>
#include <vector>

#include "dejaview/base/build_config.h"
#include "src/android_stats/dejaview_atoms.h"

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) && \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)
#include "src/android_internal/lazy_library_loader.h"  // nogncheck
#include "src/android_internal/statsd_logging.h"       // nogncheck
#endif

namespace dejaview::android_stats {

// Make sure we don't accidentally log on non-Android tree build. Note that even
// removing this ifdef still doesn't make uploads work on OS_ANDROID.
// DEJAVIEW_LAZY_LOAD will return a nullptr on non-Android and non-in-tree
// builds as libdejaview_android_internal will not be available.
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) && \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)

void MaybeLogUploadEvent(DejaViewStatsdAtom atom,
                         int64_t uuid_lsb,
                         int64_t uuid_msb,
                         const std::string& trigger_name) {
  DEJAVIEW_LAZY_LOAD(android_internal::StatsdLogUploadEvent, log_event_fn);
  if (log_event_fn) {
    log_event_fn(atom, uuid_lsb, uuid_msb, trigger_name.c_str());
  }
}

void MaybeLogTriggerEvent(DejaViewTriggerAtom atom,
                          const std::string& trigger_name) {
  DEJAVIEW_LAZY_LOAD(android_internal::StatsdLogTriggerEvent, log_event_fn);
  if (log_event_fn) {
    log_event_fn(atom, trigger_name.c_str());
  }
}

void MaybeLogTriggerEvents(DejaViewTriggerAtom atom,
                           const std::vector<std::string>& triggers) {
  DEJAVIEW_LAZY_LOAD(android_internal::StatsdLogTriggerEvent, log_event_fn);
  if (log_event_fn) {
    for (const std::string& trigger_name : triggers) {
      log_event_fn(atom, trigger_name.c_str());
    }
  }
}

#else
void MaybeLogUploadEvent(DejaViewStatsdAtom,
                         int64_t,
                         int64_t,
                         const std::string&) {}
void MaybeLogTriggerEvent(DejaViewTriggerAtom, const std::string&) {}
void MaybeLogTriggerEvents(DejaViewTriggerAtom,
                           const std::vector<std::string>&) {}
#endif

}  // namespace dejaview::android_stats
