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
#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_LL_ABI_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_LL_ABI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dejaview/public/abi/data_source_abi.h"
#include "dejaview/public/abi/track_event_abi.h"

// Low level ABI to emit track events.
//
// The library provides functions to iterate the active data source instances
// (DejaViewTeLlImplBegin and DejaViewTeLlImplNext). The app is responsible for
// serializing the "track event" protobuf messages on each instance.
// In contrast to the high-level ABI (see track_event_hl_abi.h) this gives the
// developer more flexibility and exposes more tracing features, at the cost of
// more machine-code per event.

#ifdef __cplusplus
extern "C" {
#endif

// Thread local incremental data of a track event data source instance. Opaque
// type.
struct DejaViewTeLlImplIncr;

// Thread local data of a track event data source instance. Opaque type.
struct DejaViewTeLlImplTls;

// Iterator for all the active instances (on this thread) of the track event
// data source.
struct DejaViewTeLlImplIterator {
  struct DejaViewDsImplTracerIterator ds;
  struct DejaViewTeLlImplIncr* incr;
  struct DejaViewTeLlImplTls* tls;
};

// Starts the iteration of all the active track event data source instances for
// the category `cat`.
//
// Returns an iterator. If the returned value `ds.tracer` is NULL, there are no
// active data source instances.
DEJAVIEW_SDK_EXPORT struct DejaViewTeLlImplIterator DejaViewTeLlImplBegin(
    struct DejaViewTeCategoryImpl* cat,
    struct DejaViewTeTimestamp ts);

// Advances the iterator over the next active track event data source instance
// for the category `cat`.
//
// If iterator->ds.tracer is NULL, there are no more active data source
// instances.
DEJAVIEW_SDK_EXPORT void DejaViewTeLlImplNext(
    struct DejaViewTeCategoryImpl* cat,
    struct DejaViewTeTimestamp ts,
    struct DejaViewTeLlImplIterator* iterator);

// Prematurely terminates an iteration started by DejaViewTeLlImplBegin().
DEJAVIEW_SDK_EXPORT void DejaViewTeLlImplBreak(
    struct DejaViewTeCategoryImpl*,
    struct DejaViewTeLlImplIterator*);

// Returns true if the category desc `dyn_cat` (which does not need to be
// previously registered) is enabled for the track event instance represented by
// `tracer` and `inst_id` (from `struct DejaViewTeLlImplIterator`).
DEJAVIEW_SDK_EXPORT bool DejaViewTeLlImplDynCatEnabled(
    struct DejaViewDsTracerImpl* tracer,
    DejaViewDsInstanceIndex inst_id,
    const struct DejaViewTeCategoryDescriptor* dyn_cat);

// Returns true if the track event incremental state has already seen in the
// past the given track UUID.
DEJAVIEW_SDK_EXPORT bool DejaViewTeLlImplTrackSeen(struct DejaViewTeLlImplIncr*,
                                                   uint64_t uuid);

// Interning:
//
// it's possible to avoid repeating the same data over and over in a trace by
// using "interning".
//
// `type` is a field id in the `dejaview.protos.InternedData` protobuf message.
// `data` and `data_size` point to the raw data that is potentially repeated.
// The buffer pointed by `data` can be anything (e.g. a serialized protobuf
// message, or a small integer) that uniquely identifies the potentially
// repeated data.
//
// The function returns an integer (the iid) that can be used instead of
// serializing the data directly in the packet. `*seen` is set to false if this
// is the first time the library observed this data for this specific type
// (therefore it allocated a new iid).
DEJAVIEW_SDK_EXPORT uint64_t
DejaViewTeLlImplIntern(struct DejaViewTeLlImplIncr* incr,
                       int32_t type,
                       const void* data,
                       size_t data_size,
                       bool* seen);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_LL_ABI_H_
