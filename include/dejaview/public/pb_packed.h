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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_PB_PACKED_H_
#define INCLUDE_DEJAVIEW_PUBLIC_PB_PACKED_H_

#include <stdint.h>
#include <string.h>

#include "dejaview/public/compiler.h"
#include "dejaview/public/pb_msg.h"
#include "dejaview/public/pb_utils.h"

// This file provides a way of serializing packed repeated fields. All the
// strongly typed `struct DejaViewPbPackedMsg*` variants behave as protozero
// nested messages and allow zero-copy serialization. A protobuf message that
// has a packed repeated field provides begin and end operations that accept a
// DejaViewPbPackedMsg. The downside of this approach is that (like all
// protozero nested messages), it reserves 4 bytes to encode the length, so it
// might add overhead for lots of small messages.

// ***
// Sample usage of DejaViewPbPackedMsg*
// ***
// ```
// struct DejaViewPbPackedMsgUint64 f;
// PROTO_begin_FIELD_NAME(&msg, &f);
// DejaViewPbPackedMsgUint64Append(&f, 1);
// DejaViewPbPackedMsgUint64Append(&f, 2);
// PROTO_end_FIELD_NAME(&msg, &f);
// ```

// ***
// Implementations of struct DejaViewPbPackedMsg for all supported field types.
// ***
struct DejaViewPbPackedMsgUint64 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgUint64Append(
    struct DejaViewPbPackedMsgUint64* buf,
    uint64_t value) {
  DejaViewPbMsgAppendVarInt(&buf->msg, value);
}

struct DejaViewPbPackedMsgUint32 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgUint32Append(
    struct DejaViewPbPackedMsgUint32* buf,
    uint32_t value) {
  DejaViewPbMsgAppendVarInt(&buf->msg, value);
}

struct DejaViewPbPackedMsgInt64 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgInt64Append(
    struct DejaViewPbPackedMsgInt64* buf,
    int64_t value) {
  DejaViewPbMsgAppendVarInt(&buf->msg, DEJAVIEW_STATIC_CAST(uint64_t, value));
}

struct DejaViewPbPackedMsgInt32 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgInt32Append(
    struct DejaViewPbPackedMsgInt32* buf,
    int32_t value) {
  DejaViewPbMsgAppendVarInt(&buf->msg, DEJAVIEW_STATIC_CAST(uint64_t, value));
}

struct DejaViewPbPackedMsgSint64 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgSint64Append(
    struct DejaViewPbPackedMsgSint64* buf,
    int64_t value) {
  uint64_t encoded = DejaViewPbZigZagEncode64(value);
  DejaViewPbMsgAppendVarInt(&buf->msg, encoded);
}

struct DejaViewPbPackedMsgSint32 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgSint32Append(
    struct DejaViewPbPackedMsgSint32* buf,
    int32_t value) {
  uint64_t encoded =
      DejaViewPbZigZagEncode64(DEJAVIEW_STATIC_CAST(int64_t, value));
  DejaViewPbMsgAppendVarInt(&buf->msg, encoded);
}

struct DejaViewPbPackedMsgFixed64 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgFixed64Append(
    struct DejaViewPbPackedMsgFixed64* buf,
    uint64_t value) {
  DejaViewPbMsgAppendFixed64(&buf->msg, value);
}

struct DejaViewPbPackedMsgFixed32 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgFixed32Append(
    struct DejaViewPbPackedMsgFixed32* buf,
    uint32_t value) {
  DejaViewPbMsgAppendFixed32(&buf->msg, value);
}

struct DejaViewPbPackedMsgSfixed64 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgSfixed64Append(
    struct DejaViewPbPackedMsgSfixed64* buf,
    int64_t value) {
  uint64_t encoded;
  memcpy(&encoded, &value, sizeof(encoded));
  DejaViewPbMsgAppendFixed64(&buf->msg, encoded);
}

struct DejaViewPbPackedMsgSfixed32 {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgSfixed32Append(
    struct DejaViewPbPackedMsgSfixed32* buf,
    int32_t value) {
  uint32_t encoded;
  memcpy(&encoded, &value, sizeof(encoded));
  DejaViewPbMsgAppendFixed32(&buf->msg, encoded);
}

struct DejaViewPbPackedMsgDouble {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgDoubleAppend(
    struct DejaViewPbPackedMsgDouble* buf,
    double value) {
  uint64_t encoded;
  memcpy(&encoded, &value, sizeof(encoded));
  DejaViewPbMsgAppendFixed64(&buf->msg, encoded);
}

struct DejaViewPbPackedMsgFloat {
  struct DejaViewPbMsg msg;
};
static inline void DejaViewPbPackedMsgFloatAppend(
    struct DejaViewPbPackedMsgFloat* buf,
    float value) {
  uint32_t encoded;
  memcpy(&encoded, &value, sizeof(encoded));
  DejaViewPbMsgAppendFixed32(&buf->msg, encoded);
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_PB_PACKED_H_
