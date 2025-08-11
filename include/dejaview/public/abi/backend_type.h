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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_BACKEND_TYPE_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_BACKEND_TYPE_H_

#include <stdint.h>

enum {
  // The in-process tracing backend. Keeps trace buffers in the process memory.
  DEJAVIEW_BACKEND_IN_PROCESS = (1 << 0),

  // The system tracing backend. Connects to the system tracing service (e.g.
  // on Linux/Android/Mac uses a named UNIX socket).
  DEJAVIEW_BACKEND_SYSTEM = (1 << 1),
};

// Or-combination of one or more of the above DEJAVIEW_BACKEND_ flags.
typedef uint32_t DejaViewBackendTypes;

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_BACKEND_TYPE_H_
