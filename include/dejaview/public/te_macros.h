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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_TE_MACROS_H_
#define INCLUDE_DEJAVIEW_PUBLIC_TE_MACROS_H_

#include <assert.h>

#ifdef __cplusplus
#include <initializer_list>
#endif

#include "dejaview/public/abi/track_event_hl_abi.h"
#include "dejaview/public/pb_utils.h"
#include "dejaview/public/track_event.h"

// This header defines the DEJAVIEW_TE macros and its possible params (at the
// end of the file). The rest of the file contains internal implementation
// details of the macros, which are subject to change at any time.
//
// The macro uses the High level ABI to emit track events.

#define DEJAVIEW_I_TE_HL_MACRO_PARAMS_(NAME_AND_TYPE, ...)            \
  NAME_AND_TYPE.type, NAME_AND_TYPE.name,                             \
      DEJAVIEW_I_TE_COMPOUND_LITERAL_ARRAY(struct DejaViewTeHlExtra*, \
                                           {__VA_ARGS__})

// Provides an initializer for `struct DejaViewTeHlMacroParams` and sets all the
// unused extra fields to DEJAVIEW_NULL.
#define DEJAVIEW_I_TE_HL_MACRO_PARAMS(...) \
  DEJAVIEW_I_TE_HL_MACRO_PARAMS_(__VA_ARGS__, DEJAVIEW_NULL)

// Implementation of the DEJAVIEW_TE macro. If `CAT` is enabled, emits the
// tracing event specified by the params.
//
// Uses `?:` instead of `if` because this might be used as an expression, where
// statements are not allowed.
#define DEJAVIEW_I_TE_IMPL(CAT, ...)                                        \
  ((DEJAVIEW_UNLIKELY(DEJAVIEW_ATOMIC_LOAD_EXPLICIT(                        \
       (CAT).enabled, DEJAVIEW_MEMORY_ORDER_RELAXED)))                      \
       ? (DejaViewTeHlEmitImpl((CAT).impl,                                  \
                               DEJAVIEW_I_TE_HL_MACRO_PARAMS(__VA_ARGS__)), \
          0)                                                                \
       : 0)

#ifndef __cplusplus
#define DEJAVIEW_I_TE_COMPOUND_LITERAL(STRUCT, ...) (struct STRUCT) __VA_ARGS__
#define DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(STRUCT, ...) \
  &(struct STRUCT)__VA_ARGS__
#define DEJAVIEW_I_TE_COMPOUND_LITERAL_ARRAY(TYPE, ...) (TYPE[]) __VA_ARGS__
#define DEJAVIEW_I_TE_EXTRA(STRUCT, ...)                           \
  ((struct DejaViewTeHlExtra*)DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR( \
      STRUCT, __VA_ARGS__))
#else
#define DEJAVIEW_I_TE_COMPOUND_LITERAL(STRUCT, ...) STRUCT __VA_ARGS__
#define DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(STRUCT, ...) \
  &(STRUCT{} = STRUCT __VA_ARGS__)
#define DEJAVIEW_I_TE_COMPOUND_LITERAL_ARRAY(TYPE, ...) \
  static_cast<TYPE const*>((std::initializer_list<TYPE> __VA_ARGS__).begin())
#define DEJAVIEW_I_TE_EXTRA(STRUCT, ...)       \
  reinterpret_cast<struct DejaViewTeHlExtra*>( \
      DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(STRUCT, __VA_ARGS__))
#endif

#define DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(NAME, TYPE) \
  (DEJAVIEW_I_TE_COMPOUND_LITERAL(DejaViewTeHlMacroNameAndType, {NAME, TYPE}))

#define DEJAVIEW_I_TE_CONCAT2(a, b) a##b
#define DEJAVIEW_I_TE_CONCAT(a, b) DEJAVIEW_I_TE_CONCAT2(a, b)
// Generate a unique name with a given prefix.
#define DEJAVIEW_I_TE_UID(prefix) DEJAVIEW_I_TE_CONCAT(prefix, __LINE__)

struct DejaViewTeHlMacroNameAndType {
  const char* name;
  int32_t type;
};

// Instead of a previously registered category, this macro can be used to
// specify that the category will be provided dynamically as a param.
#define DEJAVIEW_TE_DYNAMIC_CATEGORY DejaViewTeRegisteredDynamicCategory()

// ---------------------------------------------------------------
// Possible types of fields for the DEJAVIEW_TE_PROTO_FIELDS macro
// ---------------------------------------------------------------

// A string or bytes protobuf field (with field id `ID`) and value `VAL` (a null
// terminated string).
#define DEJAVIEW_TE_PROTO_FIELD_CSTR(ID, VAL)                    \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,      \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR( \
                                DejaViewTeHlProtoFieldCstr,      \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_CSTR, ID}, VAL}))

// A string or bytes protobuf field (with field id `ID`) with a `SIZE` long
// value starting from `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_BYTES(ID, VAL, SIZE) \
  DEJAVIEW_REINTERPRET_CAST(                         \
      struct DejaViewTeHlProtoField*,                \
      DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(           \
          DejaViewTeHlProtoFieldBytes,               \
          {{DEJAVIEW_TE_HL_PROTO_TYPE_BYTES, ID}, VAL, SIZE}))

// An varint protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_VARINT(ID, VAL)                          \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,              \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(         \
                                DejaViewTeHlProtoFieldVarInt,            \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_VARINT, ID}, \
                                 DEJAVIEW_STATIC_CAST(uint64_t, VAL)}))

// An zigzag (sint*) protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_ZIGZAG(ID, VAL)    \
  DEJAVIEW_REINTERPRET_CAST(                       \
      struct DejaViewTeHlProtoField*,              \
      DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(         \
          DejaViewTeHlProtoFieldVarInt,            \
          {{DEJAVIEW_TE_HL_PROTO_TYPE_VARINT, ID}, \
           DejaViewPbZigZagEncode64(DEJAVIEW_STATIC_CAST(int64_t, VAL))}))

// A fixed64 protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_FIXED64(ID, VAL)                          \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,               \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(          \
                                DejaViewTeHlProtoFieldFixed64,            \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_FIXED64, ID}, \
                                 DEJAVIEW_STATIC_CAST(uint64_t, VAL)}))

// A fixed32 protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_FIXED32(ID, VAL)                          \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,               \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(          \
                                DejaViewTeHlProtoFieldFixed32,            \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_FIXED32, ID}, \
                                 DEJAVIEW_STATIC_CAST(uint32_t, VAL)}))

// A double protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_DOUBLE(ID, VAL)                          \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,              \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(         \
                                DejaViewTeHlProtoFieldDouble,            \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_DOUBLE, ID}, \
                                 DEJAVIEW_STATIC_CAST(double, VAL)}))

// A float protobuf field (with field id `ID`) and value `VAL`.
#define DEJAVIEW_TE_PROTO_FIELD_FLOAT(ID, VAL)                                 \
  DEJAVIEW_REINTERPRET_CAST(                                                   \
      struct DejaViewTeHlProtoField*,                                          \
      DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(                                     \
          DejaViewTeHlProtoFieldFloat, {{DEJAVIEW_TE_HL_PROTO_TYPE_FLOAT, ID}, \
                                        DEJAVIEW_STATIC_CAST(float, VAL)}))

// A nested message protobuf field (with field id `ID`). The rest of the
// argument can be DEJAVIEW_TE_PROTO_FIELD_*.
#define DEJAVIEW_TE_PROTO_FIELD_NESTED(ID, ...)                          \
  DEJAVIEW_REINTERPRET_CAST(struct DejaViewTeHlProtoField*,              \
                            DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(         \
                                DejaViewTeHlProtoFieldNested,            \
                                {{DEJAVIEW_TE_HL_PROTO_TYPE_NESTED, ID}, \
                                 DEJAVIEW_I_TE_COMPOUND_LITERAL_ARRAY(   \
                                     struct DejaViewTeHlProtoField*,     \
                                     {__VA_ARGS__, DEJAVIEW_NULL})}))

// -------------------------------------------------
// Possible types of event for the DEJAVIEW_TE macro
// -------------------------------------------------

// Begins a slice named `const char* NAME` on a track.
#define DEJAVIEW_TE_SLICE_BEGIN(NAME) \
  DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(NAME, DEJAVIEW_TE_TYPE_SLICE_BEGIN)

// Ends the last slice opened on a track.
#define DEJAVIEW_TE_SLICE_END()                       \
  DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(DEJAVIEW_NULL, \
                                       DEJAVIEW_TE_TYPE_SLICE_END)

// Reports an instant event named `const char* NAME`.
#define DEJAVIEW_TE_INSTANT(NAME) \
  DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(NAME, DEJAVIEW_TE_TYPE_INSTANT)

// Reports the value of a counter. The counter value must be specified
// separately on another param with DEJAVIEW_TE_INT_COUNTER() or
// DEJAVIEW_TE_DOUBLE_COUNTER().
#define DEJAVIEW_TE_COUNTER() \
  DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(DEJAVIEW_NULL, DEJAVIEW_TE_TYPE_COUNTER)

// -----------------------------------------------------------
// Possible types of extra arguments for the DEJAVIEW_TE macro
// -----------------------------------------------------------

// The value (`C`) of an integer counter. A separate parameter must describe the
// counter track this refers to. This should only be used for events with
// type DEJAVIEW_TE_COUNTER().
#define DEJAVIEW_TE_INT_COUNTER(C)                   \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtraCounterInt64, \
                      {{DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_INT64}, C})

// The value (`C`) of a floating point. A separate parameter must describe the
// counter track this refers to. This should only be used for events with type
// DEJAVIEW_TE_COUNTER().
#define DEJAVIEW_TE_DOUBLE_COUNTER(C)                 \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtraCounterDouble, \
                      {{DEJAVIEW_TE_HL_EXTRA_TYPE_COUNTER_DOUBLE}, C})

// Uses the timestamp `struct DejaViewTeTimestamp T` to report this event. If
// this is not specified, DEJAVIEW_TE() reads the current timestamp with
// DejaViewTeGetTimestamp().
#define DEJAVIEW_TE_TIMESTAMP(T)                  \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtraTimestamp, \
                      {{DEJAVIEW_TE_HL_EXTRA_TYPE_TIMESTAMP}, T})

// Specifies that the current track for this event is
// `struct DejaViewTeRegisteredTrack* T`, which must have been previously
// registered.
#define DEJAVIEW_TE_REGISTERED_TRACK(T) \
  DEJAVIEW_I_TE_EXTRA(                  \
      DejaViewTeHlExtraRegisteredTrack, \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_REGISTERED_TRACK}, &(T)->impl})

// Specifies that the current track for this event is a track named `const char
// *NAME`, child of a track whose uuid is `PARENT_UUID`. `NAME`, `uint64_t ID`
// and `PARENT_UUID` uniquely identify a track. Common values for `PARENT_UUID`
// include DejaViewTeProcessTrackUuid(), DejaViewTeThreadTrackUuid() or
// DejaViewTeGlobalTrackUuid().
#define DEJAVIEW_TE_NAMED_TRACK(NAME, ID, PARENT_UUID) \
  DEJAVIEW_I_TE_EXTRA(                                 \
      DejaViewTeHlExtraNamedTrack,                     \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_NAMED_TRACK}, NAME, ID, PARENT_UUID})

// When DEJAVIEW_TE_DYNAMIC_CATEGORY is used, this is used to specify `const
// char* S` as a category name.
#define DEJAVIEW_TE_DYNAMIC_CATEGORY_STRING(S)                       \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtraDynamicCategory,              \
                      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DYNAMIC_CATEGORY}, \
                       DEJAVIEW_I_TE_COMPOUND_LITERAL_ADDR(          \
                           DejaViewTeCategoryDescriptor,             \
                           {S, DEJAVIEW_NULL, DEJAVIEW_NULL, 0})})

// Adds the debug annotation named `const char * NAME` with value `bool VALUE`.
#define DEJAVIEW_TE_ARG_BOOL(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                    \
      DejaViewTeHlExtraDebugArgBool,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_BOOL}, NAME, VALUE})

// Adds the debug annotation named `const char * NAME` with value `uint64_t
// VALUE`.
#define DEJAVIEW_TE_ARG_UINT64(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                      \
      DejaViewTeHlExtraDebugArgUint64,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_UINT64}, NAME, VALUE})

// Adds the debug annotation named `const char * NAME` with value `int64_t
// VALUE`.
#define DEJAVIEW_TE_ARG_INT64(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                     \
      DejaViewTeHlExtraDebugArgInt64,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_INT64}, NAME, VALUE})

// Adds the debug annotation named `const char * NAME` with value `double
// VALUE`.
#define DEJAVIEW_TE_ARG_DOUBLE(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                      \
      DejaViewTeHlExtraDebugArgDouble,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_DOUBLE}, NAME, VALUE})

// Adds the debug annotation named `const char * NAME` with value `const char*
// VALUE`.
#define DEJAVIEW_TE_ARG_STRING(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                      \
      DejaViewTeHlExtraDebugArgString,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_STRING}, NAME, VALUE})

// Adds the debug annotation named `const char * NAME` with value `void* VALUE`.
#define DEJAVIEW_TE_ARG_POINTER(NAME, VALUE) \
  DEJAVIEW_I_TE_EXTRA(                       \
      DejaViewTeHlExtraDebugArgPointer,      \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_DEBUG_ARG_POINTER}, NAME, VALUE})

// Specifies that this event is part (or starts) a "flow" (i.e. a link among
// different events). The flow is identified by `struct DejaViewTeFlow VALUE`.
#define DEJAVIEW_TE_FLOW(VALUE)              \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtraFlow, \
                      {{DEJAVIEW_TE_HL_EXTRA_TYPE_FLOW}, (VALUE).id})

// Specifies that this event terminates a "flow" (i.e. a link among different
// events). The flow is identified by `struct DejaViewTeFlow VALUE`.
#define DEJAVIEW_TE_TERMINATING_FLOW(VALUE) \
  DEJAVIEW_I_TE_EXTRA(                      \
      DejaViewTeHlExtraFlow,                \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_TERMINATING_FLOW}, (VALUE).id})

// Flushes the shared memory buffer and makes sure that all the previous events
// emitted by this thread are visibile in the central tracing buffer.
#define DEJAVIEW_TE_FLUSH() \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtra, {DEJAVIEW_TE_HL_EXTRA_TYPE_FLUSH})

// Turns off interning for event names.
#define DEJAVIEW_TE_NO_INTERN() \
  DEJAVIEW_I_TE_EXTRA(DejaViewTeHlExtra, {DEJAVIEW_TE_HL_EXTRA_TYPE_NO_INTERN})

// Adds some proto fields to the event. The arguments should use the
// DEJAVIEW_TE_PROTO_FIELD_* macros and should be fields of the
// dejaview.protos.TrackEvent protobuf message.
#define DEJAVIEW_TE_PROTO_FIELDS(...)                                       \
  DEJAVIEW_I_TE_EXTRA(                                                      \
      DejaViewTeHlExtraProtoFields,                                         \
      {{DEJAVIEW_TE_HL_EXTRA_TYPE_PROTO_FIELDS},                            \
       DEJAVIEW_I_TE_COMPOUND_LITERAL_ARRAY(struct DejaViewTeHlProtoField*, \
                                            {__VA_ARGS__, DEJAVIEW_NULL})})

// ----------------------------------
// The main DEJAVIEW_TE tracing macro
// ----------------------------------
//
// If tracing is active and the passed tracing category is enabled, adds an
// entry in the tracing stream of the dejaview track event data source.
// Parameters:
// * `CAT`: The tracing category (it should be a struct
//   DejaViewTeCategory object). It can be
//   DEJAVIEW_TE_DYNAMIC_CATEGORY for dynamic categories (the dynamic category
//   name should be passed later with)
// * The type of the event. It can be one of:
//   * DEJAVIEW_TE_SLICE_BEGIN(name)
//   * DEJAVIEW_TE_SLICE_END()
//   * DEJAVIEW_TE_INSTANT()
//   * DEJAVIEW_TE_COUNTER()
// * `...`: One or more macro parameters from the above list that specify the
//   data to be traced.
//
// Examples:
//
// DEJAVIEW_TE(category, DEJAVIEW_TE_SLICE_BEGIN("name"),
//             DEJAVIEW_TE_ARG_UINT64("extra_arg", 42));
// DEJAVIEW_TE(category, DEJAVIEW_TE_SLICE_END());
// DEJAVIEW_TE(category, DEJAVIEW_TE_COUNTER(),
//             DEJAVIEW_TE_REGISTERED_TRACK(&mycounter),
//             DEJAVIEW_TE_INT_COUNTER(79));
// DEJAVIEW_TE(DEJAVIEW_TE_DYNAMIC_CATEGORY, DEJAVIEW_TE_INSTANT("instant"),
//             DEJAVIEW_TE_DYNAMIC_CATEGORY_STRING("category"));
//
#define DEJAVIEW_TE(CAT, ...) DEJAVIEW_I_TE_IMPL(CAT, __VA_ARGS__)

#ifdef __cplusplus

// Begins a slice named `const char* NAME` on the current thread track.
//
// This is supposed to be used with DEJAVIEW_TE_SCOPED(). The implementation is
// identical to DEJAVIEW_TE_SLICE_BEGIN(): this has a different name to
// highlight the fact that DEJAVIEW_TE_SCOPED() also adds a
// DEJAVIEW_TE_SLICE_END().
#define DEJAVIEW_TE_SLICE(NAME) \
  DEJAVIEW_I_TE_HL_MACRO_NAME_AND_TYPE(NAME, DEJAVIEW_TE_TYPE_SLICE_BEGIN)

namespace dejaview::internal {
template <typename F>
class TeCleanup {
 public:
  explicit TeCleanup(F&& f) DEJAVIEW_ALWAYS_INLINE : f_(std::forward<F>(f)) {}

  ~TeCleanup() DEJAVIEW_ALWAYS_INLINE { f_(); }

 private:
  TeCleanup(const TeCleanup&) = delete;
  TeCleanup(TeCleanup&&) = delete;
  TeCleanup& operator=(const TeCleanup&) = delete;
  TeCleanup& operator=(TeCleanup&&) = delete;
  F f_;
};

template <typename F>
TeCleanup<F> MakeTeCleanup(F&& f) {
  return TeCleanup<F>(std::forward<F>(f));
}

}  // namespace dejaview::internal

// ------------------------
// DEJAVIEW_TE_SCOPED macro
// ------------------------
//
// Emits an event immediately and a DEJAVIEW_TE_SLICE_END event when the current
// scope terminates.
//
// All the extra params are added only to the event emitted immediately, not to
// the END event.
//
// TRACK params are not supported.
//
// This
// {
//   DEJAVIEW_TE_SCOPED(category, DEJAVIEW_TE_SLICE("name"), ...);
//   ...
// }
// is equivalent to
// {
//   DEJAVIEW_TE(category, DEJAVIEW_TE_SLICE_BEGIN("name"), ...);
//   ...
//   DEJAVIEW_TE(category, DEJAVIEW_TE_SLICE_END());
// }
//
// Examples:
//
// DEJAVIEW_TE_SCOPED(category, DEJAVIEW_TE_SLICE("name"));
// DEJAVIEW_TE_SCOPED(category, DEJAVIEW_TE_SLICE("name"),
//                    DEJAVIEW_TE_ARG_UINT64("count", 42));
//
#define DEJAVIEW_TE_SCOPED(CAT, ...)              \
  auto DEJAVIEW_I_TE_UID(dejaview_i_te_cleanup) = \
      (DEJAVIEW_I_TE_IMPL(CAT, __VA_ARGS__),      \
       dejaview::internal::MakeTeCleanup(         \
           [&] { DEJAVIEW_TE(CAT, DEJAVIEW_TE_SLICE_END()); }))

#endif  // __cplusplus

#endif  // INCLUDE_DEJAVIEW_PUBLIC_TE_MACROS_H_
