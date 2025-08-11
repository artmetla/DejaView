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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_PRODUCER_ABI_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_PRODUCER_ABI_H_

#include <stdint.h>

#include "dejaview/public/abi/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer to an object that stores the initialization params.
struct DejaViewProducerBackendInitArgs;

// Creates an object to store the configuration params for initializing a
// backend.
DEJAVIEW_SDK_EXPORT struct DejaViewProducerBackendInitArgs*
DejaViewProducerBackendInitArgsCreate(void);

// Tunes the size of the shared memory buffer between the current
// process and the service backend(s). This is a trade-off between memory
// footprint and the ability to sustain bursts of trace writes (see comments
// in shared_memory_abi.h).
// If set, the value must be a multiple of 4KB. The value can be ignored if
// larger than kMaxShmSize (32MB) or not a multiple of 4KB
DEJAVIEW_SDK_EXPORT void DejaViewProducerBackendInitArgsSetShmemSizeHintKb(
    struct DejaViewProducerBackendInitArgs*,
    uint32_t size);

DEJAVIEW_SDK_EXPORT void DejaViewProducerBackendInitArgsDestroy(
    struct DejaViewProducerBackendInitArgs*);

// Initializes the global system dejaview producer.
//
// It's ok to call this function multiple times, but if the producer was
// already initialized, most of `args` would be ignored.
//
// Does not take ownership of `args`. `args` can be destroyed immediately
// after this call returns.
DEJAVIEW_SDK_EXPORT void DejaViewProducerSystemInit(
    const struct DejaViewProducerBackendInitArgs* args);

// Initializes the global in-process dejaview producer.
//
// It's ok to call this function multiple times, but if the producer was
// already initialized, most of `args` would be ignored.
//
// Does not take ownership of `args`. `args` can be destroyed immediately
// after this call returns.
DEJAVIEW_SDK_EXPORT void DejaViewProducerInProcessInit(
    const struct DejaViewProducerBackendInitArgs* args);

// Informs the tracing services to activate any of these triggers if any tracing
// session was waiting for them.
//
// `trigger_names` is an array of `const char*` (zero terminated strings). The
// last pointer in the array must be NULL.
//
// Sends the trigger signal to all the initialized backends that are currently
// connected and that connect in the next `ttl_ms` milliseconds (but
// returns immediately anyway).
DEJAVIEW_SDK_EXPORT void DejaViewProducerActivateTriggers(
    const char* trigger_names[],
    uint32_t ttl_ms);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_PRODUCER_ABI_H_
