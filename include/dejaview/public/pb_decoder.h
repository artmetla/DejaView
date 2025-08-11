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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_PB_DECODER_H_
#define INCLUDE_DEJAVIEW_PUBLIC_PB_DECODER_H_

#include "dejaview/public/abi/pb_decoder_abi.h"
#include "dejaview/public/compiler.h"
#include "dejaview/public/pb_utils.h"

// Iterator for parsing protobuf messages.
//
// Example usage:
//
// const char* msg_buf = ...
// size_t msg_size = ...
//
// for (struct DejaViewPbDecoderIterator it =
//          DejaViewPbDecoderIterateBegin(msg_buf, msg_size);
//      it.field.status == DEJAVIEW_PB_DECODER_OK;
//      DejaViewPbDecoderIterateNext(&it)) {
//   // Do something with it.field
// }
struct DejaViewPbDecoderIterator {
  struct DejaViewPbDecoder decoder;
  struct DejaViewPbDecoderField field;
};

static inline struct DejaViewPbDecoderIterator DejaViewPbDecoderIterateBegin(
    const void* start,
    size_t size) {
  struct DejaViewPbDecoderIterator ret;
  ret.decoder.read_ptr = DEJAVIEW_REINTERPRET_CAST(const uint8_t*, start);
  ret.decoder.end_ptr = DEJAVIEW_REINTERPRET_CAST(const uint8_t*, start) + size;
  ret.field = DejaViewPbDecoderParseField(&ret.decoder);
  return ret;
}

static inline struct DejaViewPbDecoderIterator
DejaViewPbDecoderIterateNestedBegin(
    struct DejaViewPbDecoderDelimitedField val) {
  struct DejaViewPbDecoderIterator ret;
  ret.decoder.read_ptr = val.start;
  ret.decoder.end_ptr = val.start + val.len;
  ret.field = DejaViewPbDecoderParseField(&ret.decoder);
  return ret;
}

static inline void DejaViewPbDecoderIterateNext(
    struct DejaViewPbDecoderIterator* iterator) {
  iterator->field = DejaViewPbDecoderParseField(&iterator->decoder);
}

static inline bool DejaViewPbDecoderFieldGetUint32(
    const DejaViewPbDecoderField* field,
    uint32_t* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_VARINT:
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = DEJAVIEW_STATIC_CAST(uint32_t, field->value.integer64);
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = field->value.integer32;
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetInt32(
    const DejaViewPbDecoderField* field,
    int32_t* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_VARINT:
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = DEJAVIEW_STATIC_CAST(int32_t, field->value.integer64);
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = DEJAVIEW_STATIC_CAST(int32_t, field->value.integer32);
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetUint64(
    const DejaViewPbDecoderField* field,
    uint64_t* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_VARINT:
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = field->value.integer64;
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = field->value.integer32;
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetInt64(
    const DejaViewPbDecoderField* field,
    int64_t* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_VARINT:
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = DEJAVIEW_STATIC_CAST(int64_t, field->value.integer64);
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = DEJAVIEW_STATIC_CAST(int64_t, field->value.integer32);
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetBool(
    const DejaViewPbDecoderField* field,
    bool* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_VARINT:
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = field->value.integer64 != 0;
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = field->value.integer32 != 0;
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetFloat(
    const DejaViewPbDecoderField* field,
    float* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = DEJAVIEW_STATIC_CAST(float, field->value.double_val);
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = field->value.float_val;
      return true;
  }
  return false;
}

static inline bool DejaViewPbDecoderFieldGetDouble(
    const DejaViewPbDecoderField* field,
    double* out) {
  switch (field->wire_type) {
    case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
      *out = field->value.double_val;
      return true;
    case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
      *out = DEJAVIEW_STATIC_CAST(double, field->value.float_val);
      return true;
  }
  return false;
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_PB_DECODER_H_
