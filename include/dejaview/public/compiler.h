/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_COMPILER_H_
#define INCLUDE_DEJAVIEW_PUBLIC_COMPILER_H_

#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define DEJAVIEW_LIKELY(_x) __builtin_expect(!!(_x), 1)
#define DEJAVIEW_UNLIKELY(_x) __builtin_expect(!!(_x), 0)
#else
#define DEJAVIEW_LIKELY(_x) (_x)
#define DEJAVIEW_UNLIKELY(_x) (_x)
#endif

// DEJAVIEW_STATIC_CAST(TYPE, VAL): avoids the -Wold-style-cast warning when
// writing code that needs to be compiled as C and C++.
#ifdef __cplusplus
#define DEJAVIEW_STATIC_CAST(TYPE, VAL) static_cast<TYPE>(VAL)
#else
#define DEJAVIEW_STATIC_CAST(TYPE, VAL) ((TYPE)(VAL))
#endif

// DEJAVIEW_REINTERPRET_CAST(TYPE, VAL): avoids the -Wold-style-cast warning
// when writing code that needs to be compiled as C and C++.
#ifdef __cplusplus
#define DEJAVIEW_REINTERPRET_CAST(TYPE, VAL) reinterpret_cast<TYPE>(VAL)
#else
#define DEJAVIEW_REINTERPRET_CAST(TYPE, VAL) ((TYPE)(VAL))
#endif

// DEJAVIEW_NULL: avoids the -Wzero-as-null-pointer-constant warning when
// writing code that needs to be compiled as C and C++.
#ifdef __cplusplus
#define DEJAVIEW_NULL nullptr
#else
#define DEJAVIEW_NULL NULL
#endif

#if defined(__clang__)
#define DEJAVIEW_ALWAYS_INLINE __attribute__((__always_inline__))
#define DEJAVIEW_NO_INLINE __attribute__((__noinline__))
#else
// GCC is too pedantic and often fails with the error:
// "always_inline function might not be inlinable"
#define DEJAVIEW_ALWAYS_INLINE
#define DEJAVIEW_NO_INLINE
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_COMPILER_H_
