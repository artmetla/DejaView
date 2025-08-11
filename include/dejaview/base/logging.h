/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_BASE_LOGGING_H_
#define INCLUDE_DEJAVIEW_BASE_LOGGING_H_

#include <errno.h>
#include <string.h>  // For strerror.

#include "dejaview/base/build_config.h"
#include "dejaview/base/compiler.h"
#include "dejaview/base/export.h"

#if defined(__GNUC__) || defined(__clang__)
// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header
#endif

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_FORCE_DCHECK_ON)
#define DEJAVIEW_DCHECK_IS_ON() 1
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_FORCE_DCHECK_OFF)
#define DEJAVIEW_DCHECK_IS_ON() 0
#elif defined(DCHECK_ALWAYS_ON) ||                                         \
    (!defined(NDEBUG) && (DEJAVIEW_BUILDFLAG(DEJAVIEW_STANDALONE_BUILD) || \
                          DEJAVIEW_BUILDFLAG(DEJAVIEW_CHROMIUM_BUILD) ||   \
                          DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)))
#define DEJAVIEW_DCHECK_IS_ON() 1
#else
#define DEJAVIEW_DCHECK_IS_ON() 0
#endif

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_FORCE_DLOG_ON)
#define DEJAVIEW_DLOG_IS_ON() 1
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_FORCE_DLOG_OFF)
#define DEJAVIEW_DLOG_IS_ON() 0
#else
#define DEJAVIEW_DLOG_IS_ON() DEJAVIEW_DCHECK_IS_ON()
#endif

#if defined(DEJAVIEW_ANDROID_ASYNC_SAFE_LOG)
#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) || \
    !DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)
#error "Async-safe logging is limited to Android tree builds"
#endif
// For binaries which need a very lightweight logging implementation.
// Note that this header is incompatible with android/log.h.
#include <async_safe/log.h>
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID)
// Normal android logging.
#include <android/log.h>
#endif

// Enable the "Print the most recent DEJAVIEW_LOG(s) before crashing" feature
// on Android in-tree builds and on standalone builds (mainly for testing).
// This is deliberately no DEJAVIEW_OS_ANDROID because we don't want this
// feature when dejaview is embedded in other Android projects (e.g. SDK).
// TODO(b/203795298): TFLite is using the client library in blaze builds and is
// targeting API 19. For now disable the feature based on API level.
#if defined(DEJAVIEW_ANDROID_ASYNC_SAFE_LOG)
#define DEJAVIEW_ENABLE_LOG_RING_BUFFER() 0
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_ANDROID_BUILD)
#define DEJAVIEW_ENABLE_LOG_RING_BUFFER() 1
#elif DEJAVIEW_BUILDFLAG(DEJAVIEW_STANDALONE_BUILD) && \
    (!DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_ANDROID) ||       \
     (defined(__ANDROID_API__) && __ANDROID_API__ >= 21))
#define DEJAVIEW_ENABLE_LOG_RING_BUFFER() 1
#else
#define DEJAVIEW_ENABLE_LOG_RING_BUFFER() 0
#endif

namespace dejaview {
namespace base {

// Constexpr functions to extract basename(__FILE__), e.g.: ../foo/f.c -> f.c .
constexpr const char* StrEnd(const char* s) {
  return *s ? StrEnd(s + 1) : s;
}

constexpr const char* BasenameRecursive(const char* s,
                                        const char* begin,
                                        const char* end) {
  return (*s == '/' && s < end)
             ? (s + 1)
             : ((s > begin) ? BasenameRecursive(s - 1, begin, end) : s);
}

constexpr const char* Basename(const char* str) {
  return BasenameRecursive(StrEnd(str), str, StrEnd(str));
}

enum LogLev { kLogDebug = 0, kLogInfo, kLogImportant, kLogError };

struct LogMessageCallbackArgs {
  LogLev level;
  int line;
  const char* filename;
  const char* message;
};

using LogMessageCallback = void (*)(LogMessageCallbackArgs);

// This is not thread safe and must be called before using tracing from other
// threads.
DEJAVIEW_EXPORT_COMPONENT void SetLogMessageCallback(
    LogMessageCallback callback);

DEJAVIEW_EXPORT_COMPONENT void LogMessage(LogLev,
                                          const char* fname,
                                          int line,
                                          const char* fmt,
                                          ...) DEJAVIEW_PRINTF_FORMAT(4, 5);

// This is defined in debug_crash_stack_trace.cc, but that is only linked in
// standalone && debug builds, see enable_dejaview_stderr_crash_dump in
// dejaview.gni.
DEJAVIEW_EXPORT_COMPONENT void EnableStacktraceOnCrashForDebug();

#if DEJAVIEW_ENABLE_LOG_RING_BUFFER()
// Gets a snapshot of the logs from the internal log ring buffer and:
// - On Android in-tree builds: Passes that to android_set_abort_message().
//   That will attach the logs to the crash report.
// - On standalone builds (all otther OSes) prints that on stderr.
// This function must called only once, right before inducing a crash (This is
// because android_set_abort_message() can only be called once).
DEJAVIEW_EXPORT_COMPONENT void MaybeSerializeLastLogsForCrashReporting();
#else
inline void MaybeSerializeLastLogsForCrashReporting() {}
#endif

#if defined(DEJAVIEW_ANDROID_ASYNC_SAFE_LOG)
#define DEJAVIEW_XLOG(level, fmt, ...)                                        \
  do {                                                                        \
    async_safe_format_log((ANDROID_LOG_DEBUG + level), "dejaview",            \
                          "%s:%d " fmt, ::dejaview::base::Basename(__FILE__), \
                          __LINE__, ##__VA_ARGS__);                           \
  } while (0)
#elif defined(DEJAVIEW_DISABLE_LOG)
#define DEJAVIEW_XLOG(level, fmt, ...) \
  ::dejaview::base::ignore_result(level, fmt, ##__VA_ARGS__)
#else
#define DEJAVIEW_XLOG(level, fmt, ...)                                      \
  ::dejaview::base::LogMessage(level, ::dejaview::base::Basename(__FILE__), \
                               __LINE__, fmt, ##__VA_ARGS__)
#endif

#if defined(_MSC_VER)
#define DEJAVIEW_IMMEDIATE_CRASH()                               \
  do {                                                           \
    ::dejaview::base::MaybeSerializeLastLogsForCrashReporting(); \
    __debugbreak();                                              \
    __assume(0);                                                 \
  } while (0)
#else
#define DEJAVIEW_IMMEDIATE_CRASH()                               \
  do {                                                           \
    ::dejaview::base::MaybeSerializeLastLogsForCrashReporting(); \
    __builtin_trap();                                            \
    __builtin_unreachable();                                     \
  } while (0)
#endif

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_VERBOSE_LOGS)
#define DEJAVIEW_LOG(fmt, ...) \
  DEJAVIEW_XLOG(::dejaview::base::kLogInfo, fmt, ##__VA_ARGS__)
#else  // DEJAVIEW_BUILDFLAG(DEJAVIEW_VERBOSE_LOGS)
#define DEJAVIEW_LOG(...) ::dejaview::base::ignore_result(__VA_ARGS__)
#endif  // DEJAVIEW_BUILDFLAG(DEJAVIEW_VERBOSE_LOGS)

#define DEJAVIEW_ILOG(fmt, ...) \
  DEJAVIEW_XLOG(::dejaview::base::kLogImportant, fmt, ##__VA_ARGS__)
#define DEJAVIEW_ELOG(fmt, ...) \
  DEJAVIEW_XLOG(::dejaview::base::kLogError, fmt, ##__VA_ARGS__)
#define DEJAVIEW_FATAL(fmt, ...)       \
  do {                                 \
    DEJAVIEW_PLOG(fmt, ##__VA_ARGS__); \
    DEJAVIEW_IMMEDIATE_CRASH();        \
  } while (0)

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_PLOG(x, ...) \
  DEJAVIEW_ELOG(x " (errno: %d, %s)", ##__VA_ARGS__, errno, strerror(errno))
#else
// MSVC expands __VA_ARGS__ in a different order. Give up, not worth it.
#define DEJAVIEW_PLOG DEJAVIEW_ELOG
#endif

#define DEJAVIEW_CHECK(x)                            \
  do {                                               \
    if (DEJAVIEW_UNLIKELY(!(x))) {                   \
      DEJAVIEW_PLOG("%s", "DEJAVIEW_CHECK(" #x ")"); \
      DEJAVIEW_IMMEDIATE_CRASH();                    \
    }                                                \
  } while (0)

#if DEJAVIEW_DLOG_IS_ON()

#define DEJAVIEW_DLOG(fmt, ...) \
  DEJAVIEW_XLOG(::dejaview::base::kLogDebug, fmt, ##__VA_ARGS__)

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_DPLOG(x, ...) \
  DEJAVIEW_DLOG(x " (errno: %d, %s)", ##__VA_ARGS__, errno, strerror(errno))
#else
// MSVC expands __VA_ARGS__ in a different order. Give up, not worth it.
#define DEJAVIEW_DPLOG DEJAVIEW_DLOG
#endif

#else  // DEJAVIEW_DLOG_IS_ON()

#define DEJAVIEW_DLOG(...) ::dejaview::base::ignore_result(__VA_ARGS__)
#define DEJAVIEW_DPLOG(...) ::dejaview::base::ignore_result(__VA_ARGS__)

#endif  // DEJAVIEW_DLOG_IS_ON()

#if DEJAVIEW_DCHECK_IS_ON()

#define DEJAVIEW_DCHECK(x) DEJAVIEW_CHECK(x)
#define DEJAVIEW_DFATAL(...) DEJAVIEW_FATAL(__VA_ARGS__)
#define DEJAVIEW_DFATAL_OR_ELOG(...) DEJAVIEW_DFATAL(__VA_ARGS__)

#else  // DEJAVIEW_DCHECK_IS_ON()

#define DEJAVIEW_DCHECK(x) \
  do {                     \
  } while (false && (x))

#define DEJAVIEW_DFATAL(...) ::dejaview::base::ignore_result(__VA_ARGS__)
#define DEJAVIEW_DFATAL_OR_ELOG(...) DEJAVIEW_ELOG(__VA_ARGS__)

#endif  // DEJAVIEW_DCHECK_IS_ON()

}  // namespace base
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_BASE_LOGGING_H_
