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

#include <stdio.h>

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/scoped_file.h"
#include "dejaview/ext/base/string_splitter.h"
#include "dejaview/ext/base/utils.h"
#include "dejaview/trace_processor/trace_processor.h"
#include "src/profiling/deobfuscator.h"
#include "src/traceconv/deobfuscate_profile.h"
#include "src/traceconv/utils.h"

namespace dejaview {
namespace trace_to_text {

int DeobfuscateProfile(std::istream* input, std::ostream* output) {
  base::ignore_result(input);
  base::ignore_result(output);
  auto maybe_map = profiling::GetDejaViewProguardMapPath();
  if (maybe_map.empty()) {
    DEJAVIEW_ELOG("No DEJAVIEW_PROGUARD_MAP specified.");
    return 1;
  }
  if (!profiling::ReadProguardMapsToDeobfuscationPackets(
          maybe_map, [output](const std::string& trace_proto) {
            *output << trace_proto;
          })) {
    return 1;
  }

  return 0;
}

}  // namespace trace_to_text
}  // namespace dejaview
