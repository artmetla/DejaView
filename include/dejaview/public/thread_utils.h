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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_THREAD_UTILS_H_
#define INCLUDE_DEJAVIEW_PUBLIC_THREAD_UTILS_H_

#include <stdlib.h>

#include "dejaview/public/abi/thread_utils_abi.h"
#include "dejaview/public/compiler.h"

#if defined(__BIONIC__)

#include <unistd.h>

static inline DejaViewThreadId DejaViewGetThreadId(void) {
  return gettid();
}
#elif defined(__linux__)

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
static inline DejaViewThreadId DejaViewGetThreadId(void) {
  return DEJAVIEW_STATIC_CAST(DejaViewThreadId, syscall(__NR_gettid));
}

#else

static inline DejaViewThreadId DejaViewGetThreadId(void) {
  return DejaViewGetThreadIdImpl();
}

#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_THREAD_UTILS_H_
