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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_TRACING_SESSION_H_
#define INCLUDE_DEJAVIEW_PUBLIC_TRACING_SESSION_H_

#include "dejaview/public/abi/backend_type.h"
#include "dejaview/public/abi/tracing_session_abi.h"

static inline struct DejaViewTracingSessionImpl* DejaViewTracingSessionCreate(
    DejaViewBackendTypes backend) {
  if (backend == DEJAVIEW_BACKEND_IN_PROCESS) {
    return DejaViewTracingSessionInProcessCreate();
  }
  if (backend == DEJAVIEW_BACKEND_SYSTEM) {
    return DejaViewTracingSessionSystemCreate();
  }
  return nullptr;
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_TRACING_SESSION_H_
