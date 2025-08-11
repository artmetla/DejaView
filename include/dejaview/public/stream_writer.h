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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_STREAM_WRITER_H_
#define INCLUDE_DEJAVIEW_PUBLIC_STREAM_WRITER_H_

#include <assert.h>
#include <string.h>

#include "dejaview/public/abi/stream_writer_abi.h"
#include "dejaview/public/compiler.h"

// Returns the number of bytes available for writing in the current chunk.
static inline size_t DejaViewStreamWriterAvailableBytes(
    const struct DejaViewStreamWriter* w) {
  return DEJAVIEW_STATIC_CAST(size_t, w->end - w->write_ptr);
}

// Writes `size` bytes from `src` to the writer.
//
// WARNING: DejaViewStreamWriterAvailableBytes(`w`) must be >= `size`.
static inline void DejaViewStreamWriterAppendBytesUnsafe(
    struct DejaViewStreamWriter* w,
    const uint8_t* src,
    size_t size) {
  assert(size <= DejaViewStreamWriterAvailableBytes(w));
  memcpy(w->write_ptr, src, size);
  w->write_ptr += size;
}

// Writes `size` bytes from `src` to the writer.
static inline void DejaViewStreamWriterAppendBytes(
    struct DejaViewStreamWriter* w,
    const uint8_t* src,
    size_t size) {
  if (DEJAVIEW_LIKELY(size <= DejaViewStreamWriterAvailableBytes(w))) {
    DejaViewStreamWriterAppendBytesUnsafe(w, src, size);
  } else {
    DejaViewStreamWriterAppendBytesSlowpath(w, src, size);
  }
}

// Writes the single byte `value` to the writer.
static inline void DejaViewStreamWriterAppendByte(
    struct DejaViewStreamWriter* w,
    uint8_t value) {
  if (DEJAVIEW_UNLIKELY(DejaViewStreamWriterAvailableBytes(w) < 1)) {
    DejaViewStreamWriterNewChunk(w);
  }
  *w->write_ptr++ = value;
}

// Returns a pointer to an area of the chunk long `size` for writing. The
// returned area is considered already written by the writer (it will not be
// used again).
//
// WARNING: DejaViewStreamWriterAvailableBytes(`w`) must be >= `size`.
static inline uint8_t* DejaViewStreamWriterReserveBytesUnsafe(
    struct DejaViewStreamWriter* w,
    size_t size) {
  uint8_t* ret = w->write_ptr;
  assert(size <= DejaViewStreamWriterAvailableBytes(w));
  w->write_ptr += size;
  return ret;
}

// Returns a pointer to an area of the chunk long `size` for writing. The
// returned area is considered already written by the writer (it will not be
// used again).
//
// WARNING: `size` should be smaller than the chunk size returned by the
// `delegate`.
static inline uint8_t* DejaViewStreamWriterReserveBytes(
    struct DejaViewStreamWriter* w,
    size_t size) {
  if (DEJAVIEW_LIKELY(size <= DejaViewStreamWriterAvailableBytes(w))) {
    return DejaViewStreamWriterReserveBytesUnsafe(w, size);
  }
  DejaViewStreamWriterReserveBytesSlowpath(w, size);
  return w->write_ptr - size;
}

// Returns the number of bytes written to the stream writer from the start.
static inline size_t DejaViewStreamWriterGetWrittenSize(
    const struct DejaViewStreamWriter* w) {
  return w->written_previously +
         DEJAVIEW_STATIC_CAST(size_t, w->write_ptr - w->begin);
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_STREAM_WRITER_H_
