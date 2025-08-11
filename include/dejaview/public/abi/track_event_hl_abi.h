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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_HL_ABI_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_HL_ABI_H_

#include <stdbool.h>
#include <stdint.h>

#include "dejaview/public/abi/track_event_abi.h"

// High level ABI to emit track events.
//
// For each tracepoint, the user must call DejaViewTeHlEmitImpl() once and pass
// it all the required data for the event. The function will iterate all enabled
// data source instances and serialize the tracing data as protobuf messages.
//
// This tries to cover the most common cases of track event. When hitting these
// we minimize binary size at the trace-event call site, but we trade off the
// ability to serialize custom protobuf messages.

#ifdef __cplusplus
extern "C" {
#endif

// The type of the proto field.
enum DejaViewTeHlProtoFieldType {
  DEJAVIEW_TE_HL_PROTO_TYPE_CSTR = 0,
  DEJAVIEW_TE_HL_PROTO_TYPE_BYTES = 1,
  DEJAVIEW_TE_HL_PROTO_TYPE_NESTED = 2,
  DEJAVIEW_TE_HL_PROTO_TYPE_VARINT = 3,
  DEJAVIEW_TE_HL_PROTO_TYPE_FIXED64 = 4,
  DEJAVIEW_TE_HL_PROTO_TYPE_FIXED32 = 5,
  DEJAVIEW_TE_HL_PROTO_TYPE_DOUBLE = 6,
  DEJAVIEW_TE_HL_PROTO_TYPE_FLOAT = 7,
};

// Common header for all the proto fields.
struct DejaViewTeHlProtoField {
  enum DejaViewTeHlProtoFieldType type;
  // Proto field id.
  uint32_t id;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_CSTR
struct DejaViewTeHlProtoFieldCstr {
  struct DejaViewTeHlProtoField header;
  // Null terminated string.
  const char* str;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_BYTES
struct DejaViewTeHlProtoFieldBytes {
  struct DejaViewTeHlProtoField header;
  const void* buf;
  size_t len;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_NESTED
struct DejaViewTeHlProtoFieldNested {
  struct DejaViewTeHlProtoField header;
  // Array of pointers to the fields. The last pointer should be NULL.
  struct DejaViewTeHlProtoField* const* fields;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_VARINT
struct DejaViewTeHlProtoFieldVarInt {
  struct DejaViewTeHlProtoField header;
  uint64_t value;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_FIXED64
struct DejaViewTeHlProtoFieldFixed64 {
  struct DejaViewTeHlProtoField header;
  uint64_t value;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_FIXED32
struct DejaViewTeHlProtoFieldFixed32 {
  struct DejaViewTeHlProtoField header;
  uint32_t value;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_DOUBLE
struct DejaViewTeHlProtoFieldDouble {
  struct DejaViewTeHlProtoField header;
  double value;
};

// DEJAVIEW_TE_HL_PROTO_TYPE_FLOAT
struct DejaViewTeHlProtoFieldFloat {
  struct DejaViewTeHlProtoField header;
  float value;
};

// The type of an event extra parameter.
enum DejaViewTeHlExtraType {
  DEJAVIEW_TE_HL_EXTRA_TYPE_REGISTERED_TRACK = 1,
  DEJAVIEW_TE_HL_EXTRA_TYPE_NAMED_TRACK = 2,
  DEJAVIEW_TE_HL_EXTRA_TYPE_TIMESTAMP = 3,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DYNAMIC_CATEGORY = 4,
  DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_INT64 = 5,
  DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_DOUBLE = 6,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_BOOL = 7,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_UINT64 = 8,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_INT64 = 9,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_DOUBLE = 10,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_STRING = 11,
  DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_POINTER = 12,
  DEJAVIEW_TE_HL_EXTRA_TYPE_FLOW = 13,
  DEJAVIEW_TE_HL_EXTRA_TYPE_TERMINATING_FLOW = 14,
  DEJAVIEW_TE_HL_EXTRA_TYPE_FLUSH = 15,
  DEJAVIEW_TE_HL_EXTRA_TYPE_NO_INTERN = 16,
  DEJAVIEW_TE_HL_EXTRA_TYPE_PROTO_FIELDS = 17,
};

// An extra event parameter. Each type of parameter should embed this as its
// first member.
struct DejaViewTeHlExtra {
  // enum DejaViewTeHlExtraType. Identifies the exact type of this.
  uint32_t type;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_REGISTERED_TRACK
struct DejaViewTeHlExtraRegisteredTrack {
  struct DejaViewTeHlExtra header;
  // Pointer to a registered track.
  const struct DejaViewTeRegisteredTrackImpl* track;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_NAMED_TRACK
struct DejaViewTeHlExtraNamedTrack {
  struct DejaViewTeHlExtra header;
  // The name of the track
  const char* name;
  uint64_t id;
  uint64_t parent_uuid;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_TIMESTAMP
struct DejaViewTeHlExtraTimestamp {
  struct DejaViewTeHlExtra header;
  // The timestamp for this event.
  struct DejaViewTeTimestamp timestamp;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DYNAMIC_CATEGORY
struct DejaViewTeHlExtraDynamicCategory {
  struct DejaViewTeHlExtra header;
  // Pointer to a category descriptor. The descriptor will be evaluated against
  // the configuration. If the descriptor is considered disabled, the trace
  // point will be skipped.
  const struct DejaViewTeCategoryDescriptor* desc;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_INT64
struct DejaViewTeHlExtraCounterInt64 {
  struct DejaViewTeHlExtra header;
  // The counter value.
  int64_t value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_DOUBLE
struct DejaViewTeHlExtraCounterDouble {
  struct DejaViewTeHlExtra header;
  // The counter value.
  double value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_BOOL
struct DejaViewTeHlExtraDebugArgBool {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  bool value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_UINT64
struct DejaViewTeHlExtraDebugArgUint64 {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  uint64_t value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_INT64
struct DejaViewTeHlExtraDebugArgInt64 {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  int64_t value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_DOUBLE
struct DejaViewTeHlExtraDebugArgDouble {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  double value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_STRING
struct DejaViewTeHlExtraDebugArgString {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  const char* value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_POINTER
struct DejaViewTeHlExtraDebugArgPointer {
  struct DejaViewTeHlExtra header;
  // Pointer to the name of this debug annotation.
  const char* name;
  // The value of this debug annotation.
  uintptr_t value;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_FLOW
// DEJAVIEW_TE_HL_EXTRA_TYPE_TERMINATING_FLOW
struct DejaViewTeHlExtraFlow {
  struct DejaViewTeHlExtra header;
  // Specifies that this event starts (or terminates) a flow (i.e. a link
  // between two events) identified by this id.
  uint64_t id;
};

// DEJAVIEW_TE_HL_EXTRA_TYPE_PROTO_FIELDS
struct DejaViewTeHlExtraProtoFields {
  struct DejaViewTeHlExtra header;
  // Array of pointers to the fields. The last pointer should be NULL.
  struct DejaViewTeHlProtoField* const* fields;
};

// Emits an event on all active instances of the track event data source.
// * `cat`: The registered category of the event, it knows on which data source
//          instances the event should be emitted. Use
//          `dejaview_te_all_categories` for dynamic categories.
// * `type`: the event type (slice begin, slice end, ...). See `enum
//           DejaViewTeType`.
// * `name`: All events (except when DEJAVIEW_TE_TYPE_SLICE_END) can have an
//           associated name. It can be nullptr.
// * `extra_data`: Optional parameters associated with the events. Array of
// pointers to each event. The last pointer should be NULL.
DEJAVIEW_SDK_EXPORT void DejaViewTeHlEmitImpl(
    struct DejaViewTeCategoryImpl* cat,
    int32_t type,
    const char* name,
    struct DejaViewTeHlExtra* const* extra_data);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_HL_ABI_H_
