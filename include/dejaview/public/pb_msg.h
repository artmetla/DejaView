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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_PB_MSG_H_
#define INCLUDE_DEJAVIEW_PUBLIC_PB_MSG_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dejaview/public/abi/stream_writer_abi.h"
#include "dejaview/public/compiler.h"
#include "dejaview/public/pb_utils.h"
#include "dejaview/public/stream_writer.h"

// The number of bytes reserved by this implementation to encode a protobuf type
// 2 field size as var-int. Keep this in sync with kMessageLengthFieldSize in
// proto_utils.h.
#define PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE 4

// Points to the memory used by a `DejaViewPbMsg` for writing.
struct DejaViewPbMsgWriter {
  struct DejaViewStreamWriter writer;
};

struct DejaViewPbMsg {
  // Pointer to a non-aligned pre-reserved var-int slot of
  // PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE bytes. If not NULL,
  // protozero_length_buf_finalize() will write the size of proto-encoded
  // message in the pointed memory region.
  uint8_t* size_field;

  // Current size of the buffer.
  uint32_t size;

  struct DejaViewPbMsgWriter* writer;

  struct DejaViewPbMsg* nested;
  struct DejaViewPbMsg* parent;
};

static inline void DejaViewPbMsgInit(struct DejaViewPbMsg* msg,
                                     struct DejaViewPbMsgWriter* writer) {
  msg->size_field = DEJAVIEW_NULL;
  msg->size = 0;
  msg->writer = writer;
  msg->nested = DEJAVIEW_NULL;
  msg->parent = DEJAVIEW_NULL;
}

static inline void DejaViewPbMsgPatch(struct DejaViewPbMsg* msg) {
  static_assert(
      PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE == DEJAVIEW_STREAM_WRITER_PATCH_SIZE,
      "PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE doesn't match patch size");
  msg->size_field =
      DejaViewStreamWriterAnnotatePatch(&msg->writer->writer, msg->size_field);
}

static inline void DejaViewPbMsgPatchStack(struct DejaViewPbMsg* msg) {
  uint8_t* const cur_range_end = msg->writer->writer.end;
  uint8_t* const cur_range_begin = msg->writer->writer.begin;
  while (msg && cur_range_begin <= msg->size_field &&
         msg->size_field < cur_range_end) {
    DejaViewPbMsgPatch(msg);
    msg = msg->parent;
  }
}

static inline void DejaViewPbMsgAppendBytes(struct DejaViewPbMsg* msg,
                                            const uint8_t* begin,
                                            size_t size) {
  if (DEJAVIEW_UNLIKELY(
          size > DejaViewStreamWriterAvailableBytes(&msg->writer->writer))) {
    DejaViewPbMsgPatchStack(msg);
  }
  DejaViewStreamWriterAppendBytes(&msg->writer->writer, begin, size);
  msg->size += size;
}

static inline void DejaViewPbMsgAppendByte(struct DejaViewPbMsg* msg,
                                           uint8_t value) {
  DejaViewPbMsgAppendBytes(msg, &value, 1);
}

static inline void DejaViewPbMsgAppendVarInt(struct DejaViewPbMsg* msg,
                                             uint64_t value) {
  uint8_t* buf_end;
  uint8_t buf[DEJAVIEW_PB_VARINT_MAX_SIZE_64];
  buf_end = DejaViewPbWriteVarInt(value, buf);

  DejaViewPbMsgAppendBytes(msg, buf,
                           DEJAVIEW_STATIC_CAST(size_t, buf_end - buf));
}

static inline void DejaViewPbMsgAppendFixed64(struct DejaViewPbMsg* msg,
                                              uint64_t value) {
  uint8_t buf[8];
  DejaViewPbWriteFixed64(value, buf);

  DejaViewPbMsgAppendBytes(msg, buf, 8);
}

static inline void DejaViewPbMsgAppendFixed32(struct DejaViewPbMsg* msg,
                                              uint32_t value) {
  uint8_t buf[4];
  DejaViewPbWriteFixed32(value, buf);

  DejaViewPbMsgAppendBytes(msg, buf, 4);
}

static inline void DejaViewPbMsgAppendType0Field(struct DejaViewPbMsg* msg,
                                                 int32_t field_id,
                                                 uint64_t value) {
  uint8_t* buf_end;
  uint8_t buf[DEJAVIEW_PB_VARINT_MAX_SIZE_64 + DEJAVIEW_PB_VARINT_MAX_SIZE_32];
  buf_end = DejaViewPbWriteVarInt(
      DejaViewPbMakeTag(field_id, DEJAVIEW_PB_WIRE_TYPE_VARINT), buf);
  buf_end = DejaViewPbWriteVarInt(value, buf_end);

  DejaViewPbMsgAppendBytes(msg, buf,
                           DEJAVIEW_STATIC_CAST(size_t, buf_end - buf));
}

static inline void DejaViewPbMsgAppendType2Field(struct DejaViewPbMsg* msg,
                                                 int32_t field_id,
                                                 const uint8_t* data,
                                                 size_t size) {
  uint8_t* buf_end;
  uint8_t buf[DEJAVIEW_PB_VARINT_MAX_SIZE_64 + DEJAVIEW_PB_VARINT_MAX_SIZE_32];
  buf_end = DejaViewPbWriteVarInt(
      DejaViewPbMakeTag(field_id, DEJAVIEW_PB_WIRE_TYPE_DELIMITED), buf);
  buf_end =
      DejaViewPbWriteVarInt(DEJAVIEW_STATIC_CAST(uint64_t, size), buf_end);
  DejaViewPbMsgAppendBytes(msg, buf,
                           DEJAVIEW_STATIC_CAST(size_t, buf_end - buf));

  DejaViewPbMsgAppendBytes(msg, data, size);
}

static inline void DejaViewPbMsgAppendFixed32Field(struct DejaViewPbMsg* msg,
                                                   int32_t field_id,
                                                   uint32_t value) {
  uint8_t* buf_end;
  uint8_t buf[DEJAVIEW_PB_VARINT_MAX_SIZE_32 + 4];
  buf_end = DejaViewPbWriteVarInt(
      DejaViewPbMakeTag(field_id, DEJAVIEW_PB_WIRE_TYPE_FIXED32), buf);
  buf_end = DejaViewPbWriteFixed32(value, buf_end);

  DejaViewPbMsgAppendBytes(msg, buf,
                           DEJAVIEW_STATIC_CAST(size_t, buf_end - buf));
}

static inline void DejaViewPbMsgAppendFloatField(struct DejaViewPbMsg* msg,
                                                 int32_t field_id,
                                                 float value) {
  uint32_t val;
  memcpy(&val, &value, sizeof val);
  DejaViewPbMsgAppendFixed32Field(msg, field_id, val);
}

static inline void DejaViewPbMsgAppendFixed64Field(struct DejaViewPbMsg* msg,
                                                   int32_t field_id,
                                                   uint64_t value) {
  uint8_t* buf_end;
  uint8_t buf[DEJAVIEW_PB_VARINT_MAX_SIZE_32 + 8];
  buf_end = DejaViewPbWriteVarInt(
      DejaViewPbMakeTag(field_id, DEJAVIEW_PB_WIRE_TYPE_FIXED64), buf);
  buf_end = DejaViewPbWriteFixed64(value, buf_end);

  DejaViewPbMsgAppendBytes(msg, buf,
                           DEJAVIEW_STATIC_CAST(size_t, buf_end - buf));
}

static inline void DejaViewPbMsgAppendDoubleField(struct DejaViewPbMsg* msg,
                                                  int32_t field_id,
                                                  double value) {
  uint64_t val;
  memcpy(&val, &value, sizeof val);
  DejaViewPbMsgAppendFixed64Field(msg, field_id, val);
}

static inline void DejaViewPbMsgAppendCStrField(struct DejaViewPbMsg* msg,
                                                int32_t field_id,
                                                const char* c_str) {
  DejaViewPbMsgAppendType2Field(
      msg, field_id, DEJAVIEW_REINTERPRET_CAST(const uint8_t*, c_str),
      strlen(c_str));
}

static inline void DejaViewPbMsgBeginNested(struct DejaViewPbMsg* parent,
                                            struct DejaViewPbMsg* nested,
                                            int32_t field_id) {
  DejaViewPbMsgAppendVarInt(
      parent, DejaViewPbMakeTag(field_id, DEJAVIEW_PB_WIRE_TYPE_DELIMITED));

  DejaViewPbMsgInit(nested, parent->writer);
  if (DEJAVIEW_UNLIKELY(
          PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE >
          DejaViewStreamWriterAvailableBytes(&parent->writer->writer))) {
    DejaViewPbMsgPatchStack(parent);
  }
  nested->size_field = DEJAVIEW_REINTERPRET_CAST(
      uint8_t*,
      DejaViewStreamWriterReserveBytes(&nested->writer->writer,
                                       PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE));
  nested->parent = parent;
  parent->size += PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE;
  parent->nested = nested;
}

static inline size_t DejaViewPbMsgFinalize(struct DejaViewPbMsg* msg);

static inline void DejaViewPbMsgEndNested(struct DejaViewPbMsg* parent) {
  parent->size += DejaViewPbMsgFinalize(parent->nested);
  parent->nested = DEJAVIEW_NULL;
}

static inline size_t DejaViewPbMsgFinalize(struct DejaViewPbMsg* msg) {
  if (msg->nested)
    DejaViewPbMsgEndNested(msg);

  // Write the length of the nested message a posteriori, using a leading-zero
  // redundant varint encoding.
  if (msg->size_field) {
    uint32_t size_to_write;
    size_to_write = msg->size;
    for (size_t i = 0; i < PROTOZERO_MESSAGE_LENGTH_FIELD_SIZE; i++) {
      const uint8_t msb = (i < 3) ? 0x80 : 0;
      msg->size_field[i] = (size_to_write & 0xFF) | msb;
      size_to_write >>= 7;
    }
    msg->size_field = DEJAVIEW_NULL;
  }

  return msg->size;
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_PB_MSG_H_
