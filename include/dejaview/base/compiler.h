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

#ifndef INCLUDE_DEJAVIEW_BASE_COMPILER_H_
#define INCLUDE_DEJAVIEW_BASE_COMPILER_H_

#include <cstddef>
#include <type_traits>
#include <variant>

#include "dejaview/public/compiler.h"

// __has_attribute is supported only by clang and recent versions of GCC.
// Add a layer to wrap the __has_attribute macro.
#if defined(__has_attribute)
#define DEJAVIEW_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define DEJAVIEW_HAS_ATTRIBUTE(x) 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define DEJAVIEW_WARN_UNUSED_RESULT
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_UNUSED __attribute__((unused))
#else
#define DEJAVIEW_UNUSED
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_NORETURN __attribute__((__noreturn__))
#else
#define DEJAVIEW_NORETURN __declspec(noreturn)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_DEBUG_FUNCTION_IDENTIFIER() __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define DEJAVIEW_DEBUG_FUNCTION_IDENTIFIER() __FUNCSIG__
#else
#define DEJAVIEW_DEBUG_FUNCTION_IDENTIFIER() \
  static_assert(false, "Not implemented for this compiler")
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_PRINTF_FORMAT(x, y) \
  __attribute__((__format__(__printf__, x, y)))
#else
#define DEJAVIEW_PRINTF_FORMAT(x, y)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_POPCOUNT(x) __builtin_popcountll(x)
#else
#include <intrin.h>
#define DEJAVIEW_POPCOUNT(x) __popcnt64(x)
#endif

#if defined(__clang__)
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
extern "C" void __asan_poison_memory_region(void const volatile*, size_t);
extern "C" void __asan_unpoison_memory_region(void const volatile*, size_t);
#define DEJAVIEW_ASAN_POISON(a, s) __asan_poison_memory_region((a), (s))
#define DEJAVIEW_ASAN_UNPOISON(a, s) __asan_unpoison_memory_region((a), (s))
#else
#define DEJAVIEW_ASAN_POISON(addr, size)
#define DEJAVIEW_ASAN_UNPOISON(addr, size)
#endif  // __has_feature(address_sanitizer)
#else
#define DEJAVIEW_ASAN_POISON(addr, size)
#define DEJAVIEW_ASAN_UNPOISON(addr, size)
#endif  // __clang__

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_IS_LITTLE_ENDIAN() __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#else
// Assume all MSVC targets are little endian.
#define DEJAVIEW_IS_LITTLE_ENDIAN() 1
#endif

// This is used for exporting xxxMain() symbols (e.g., DejaViewCmdMain,
// ProbesMain) from libdejaview.so when the GN arg monolithic_binaries = false.
#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_EXPORT_ENTRYPOINT __attribute__((visibility("default")))
#else
// TODO(primiano): on Windows this should be a pair of dllexport/dllimport. But
// that requires a -DXXX_IMPLEMENTATION depending on whether we are on the
// impl-site or call-site. Right now it's not worth the trouble as we
// force-export the xxxMain() symbols only on Android, where we pack all the
// code for N binaries into one .so to save binary size. On Windows we support
// only monolithic binaries, as they are easier to deal with.
#define DEJAVIEW_EXPORT_ENTRYPOINT
#endif

// Disables thread safety analysis for functions where the compiler can't
// accurate figure out which locks are being held.
#if defined(__clang__)
#define DEJAVIEW_NO_THREAD_SAFETY_ANALYSIS \
  __attribute__((no_thread_safety_analysis))
#else
#define DEJAVIEW_NO_THREAD_SAFETY_ANALYSIS
#endif

// Disables undefined behavior analysis for a function.
#if defined(__clang__)
#define DEJAVIEW_NO_SANITIZE_UNDEFINED __attribute__((no_sanitize("undefined")))
#else
#define DEJAVIEW_NO_SANITIZE_UNDEFINED
#endif

// Avoid calling the exit-time destructor on an object with static lifetime.
#if DEJAVIEW_HAS_ATTRIBUTE(no_destroy)
#define DEJAVIEW_HAS_NO_DESTROY() 1
#define DEJAVIEW_NO_DESTROY __attribute__((no_destroy))
#else
#define DEJAVIEW_HAS_NO_DESTROY() 0
#define DEJAVIEW_NO_DESTROY
#endif

// Macro for telling -Wimplicit-fallthrough that a fallthrough is intentional.
#define DEJAVIEW_FALLTHROUGH [[fallthrough]]

namespace dejaview::base {

template <typename... T>
inline void ignore_result(const T&...) {}

// Given a std::variant and a type T, returns the index of the T in the variant.
template <typename VariantType, typename T, size_t i = 0>
constexpr size_t variant_index() {
  static_assert(i < std::variant_size_v<VariantType>,
                "Type not found in variant");
  if constexpr (std::is_same_v<std::variant_alternative_t<i, VariantType>, T>) {
    return i;
  } else {
    return variant_index<VariantType, T, i + 1>();
  }
}

}  // namespace dejaview::base

#endif  // INCLUDE_DEJAVIEW_BASE_COMPILER_H_
