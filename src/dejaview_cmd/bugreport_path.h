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

#ifndef SRC_DEJAVIEW_CMD_BUGREPORT_PATH_H_
#define SRC_DEJAVIEW_CMD_BUGREPORT_PATH_H_

#include <string>

#include "dejaview/base/build_config.h"
#include "dejaview/ext/base/temp_file.h"

namespace dejaview {

// Expose for testing

inline std::string GetBugreportTraceDir() {
#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) && \
    DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)
  return "/data/misc/dejaview-traces/bugreport";
#else
  // Only for tests, SaveTraceForBugreport is not used on other OSes.
  return base::GetSysTempDir();
#endif
}

inline std::string GetBugreportTracePath() {
  return GetBugreportTraceDir() + "/systrace.pftrace";
}

}  // namespace dejaview

#endif  // SRC_DEJAVIEW_CMD_BUGREPORT_PATH_H_
