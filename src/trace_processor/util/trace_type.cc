/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "src/trace_processor/util/trace_type.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/protozero/proto_utils.h"

#include "protos/dejaview/trace/trace.pbzero.h"
#include "protos/dejaview/trace/trace_packet.pbzero.h"

namespace dejaview::trace_processor {
namespace {
// Fuchsia traces have a magic number as documented here:
// https://fuchsia.googlesource.com/fuchsia/+/HEAD/docs/development/tracing/trace-format/README.md#magic-number-record-trace-info-type-0
constexpr char kZipMagic[] = {'P', 'K', '\x03', '\x04'};
constexpr char kGzipMagic[] = {'\x1f', '\x8b'};

constexpr uint8_t kTracePacketTag =
    protozero::proto_utils::MakeTagLengthDelimited(
        protos::pbzero::Trace::kPacketFieldNumber);
constexpr uint16_t kModuleSymbolsTag =
    protozero::proto_utils::MakeTagLengthDelimited(
        protos::pbzero::TracePacket::kModuleSymbolsFieldNumber);

inline bool isspace(unsigned char c) {
  return ::isspace(c);
}

std::string RemoveWhitespace(std::string str) {
  str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
  return str;
}

template <size_t N>
bool MatchesMagic(const uint8_t* data, size_t size, const char (&magic)[N]) {
  if (size < N) {
    return false;
  }
  return memcmp(data, magic, N) == 0;
}

bool IsProtoTraceWithSymbols(const uint8_t* ptr, size_t size) {
  const uint8_t* const end = ptr + size;

  uint64_t tag;
  const uint8_t* next = protozero::proto_utils::ParseVarInt(ptr, end, &tag);

  if (next == ptr || tag != kTracePacketTag) {
    return false;
  }

  ptr = next;
  uint64_t field_length;
  next = protozero::proto_utils::ParseVarInt(ptr, end, &field_length);
  if (next == ptr) {
    return false;
  }
  ptr = next;

  if (field_length == 0) {
    return false;
  }

  next = protozero::proto_utils::ParseVarInt(ptr, end, &tag);
  if (next == ptr) {
    return false;
  }

  return tag == kModuleSymbolsTag;
}

}  // namespace

const char* TraceTypeToString(TraceType trace_type) {
  switch (trace_type) {
    case kJsonTraceType:
      return "json";
    case kProtoTraceType:
      return "proto";
    case kSymbolsTraceType:
      return "symbols";
    case kGzipTraceType:
      return "gzip";
    case kCtraceTraceType:
      return "ctrace";
    case kZipFile:
      return "zip";
    case kAndroidLogcatTraceType:
      return "android_logcat";
    case kAndroidDumpstateTraceType:
      return "android_dumpstate";
    case kAndroidBugreportTraceType:
      return "android_bugreport";
    case kUnknownTraceType:
      return "unknown";
  }
  DEJAVIEW_FATAL("For GCC");
}

TraceType GuessTraceType(const uint8_t* data, size_t size) {
  if (size == 0) {
    return kUnknownTraceType;
  }

  if (MatchesMagic(data, size, kZipMagic)) {
    return kZipFile;
  }

  if (MatchesMagic(data, size, kGzipMagic)) {
    return kGzipTraceType;
  }

  std::string start(reinterpret_cast<const char*>(data),
                    std::min<size_t>(size, kGuessTraceMaxLookahead));

  std::string start_minus_white_space = RemoveWhitespace(start);
  if (base::StartsWith(start_minus_white_space, "{\""))
    return kJsonTraceType;
  if (base::StartsWith(start_minus_white_space, "[{\""))
    return kJsonTraceType;

  // Traces obtained from atrace -z (compress).
  // They all have the string "TRACE:" followed by 78 9C which is a zlib header
  // for "deflate, default compression, window size=32K" (see b/208691037)
  if (base::Contains(start, "TRACE:\n\x78\x9c"))
    return kCtraceTraceType;

  if (IsProtoTraceWithSymbols(data, size))
    return kSymbolsTraceType;

  if (base::StartsWith(start, "\x0a"))
    return kProtoTraceType;

  return kUnknownTraceType;
}

}  // namespace dejaview::trace_processor
