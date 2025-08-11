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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_HEAP_BUFFER_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_HEAP_BUFFER_H_

#include "dejaview/public/abi/stream_writer_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

// A DejaViewHeapBuffer can be used to serialize protobuf data using the
// DejaViewStreamWriter interface. Stores data on heap allocated buffers, which
// can be read back with DejaViewHeapBufferCopyInto().

struct DejaViewHeapBuffer;

// Creates a DejaViewHeapBuffer. Takes a pointer to an (uninitialized)
// DejaViewStreamWriter (owned by the caller). The stream writer can be user
// later to serialize protobuf data.
DEJAVIEW_SDK_EXPORT struct DejaViewHeapBuffer* DejaViewHeapBufferCreate(
    struct DejaViewStreamWriter*);

// Copies data from the heap buffer to `dst` (up to `size` bytes).
DEJAVIEW_SDK_EXPORT void DejaViewHeapBufferCopyInto(
    struct DejaViewHeapBuffer*,
    struct DejaViewStreamWriter*,
    void* dst,
    size_t size);

// Destroys the heap buffer.
DEJAVIEW_SDK_EXPORT void DejaViewHeapBufferDestroy(
    struct DejaViewHeapBuffer*,
    struct DejaViewStreamWriter*);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_HEAP_BUFFER_H_
