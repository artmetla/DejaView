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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_TRACK_EVENT_H_
#define INCLUDE_DEJAVIEW_PUBLIC_TRACK_EVENT_H_

#include <stdint.h>
#include <stdlib.h>

#include "dejaview/public/abi/heap_buffer.h"
#include "dejaview/public/abi/track_event_abi.h"     // IWYU pragma: export
#include "dejaview/public/abi/track_event_hl_abi.h"  // IWYU pragma: export
#include "dejaview/public/abi/track_event_ll_abi.h"  // IWYU pragma: export
#include "dejaview/public/compiler.h"
#include "dejaview/public/data_source.h"
#include "dejaview/public/fnv1a.h"
#include "dejaview/public/pb_msg.h"
#include "dejaview/public/protos/trace/interned_data/interned_data.pzc.h"
#include "dejaview/public/protos/trace/trace_packet.pzc.h"
#include "dejaview/public/protos/trace/track_event/counter_descriptor.pzc.h"
#include "dejaview/public/protos/trace/track_event/track_descriptor.pzc.h"
#include "dejaview/public/protos/trace/track_event/track_event.pzc.h"
#include "dejaview/public/thread_utils.h"

// A registered category.
struct DejaViewTeCategory {
  DEJAVIEW_ATOMIC(bool) * enabled;
  struct DejaViewTeCategoryImpl* impl;
  struct DejaViewTeCategoryDescriptor desc;
  uint64_t cat_iid;
};

// Registers the category `cat`. `cat->desc` must be filled before calling this.
// The rest of the structure is filled by the function.
static inline void DejaViewTeCategoryRegister(struct DejaViewTeCategory* cat) {
  cat->impl = DejaViewTeCategoryImplCreate(&cat->desc);
  cat->enabled = DejaViewTeCategoryImplGetEnabled(cat->impl);
  cat->cat_iid = DejaViewTeCategoryImplGetIid(cat->impl);
}

// Calls DejaViewTeCategoryRegister() on multiple categories.
static inline void DejaViewTeRegisterCategories(
    struct DejaViewTeCategory* cats[],
    size_t size) {
  for (size_t i = 0; i < size; i++) {
    DejaViewTeCategoryRegister(cats[i]);
  }
}

// Registers `cb` to be called every time a data source instance with `reg_cat`
// enabled is created or destroyed. `user_arg` will be passed unaltered to `cb`.
//
// `cb` can be NULL to disable the callback.
static inline void DejaViewTeCategorySetCallback(
    struct DejaViewTeCategory* reg_cat,
    DejaViewTeCategoryImplCallback cb,
    void* user_arg) {
  DejaViewTeCategoryImplSetCallback(reg_cat->impl, cb, user_arg);
}

// Unregisters the category `cat`.
//
// WARNING: The category cannot be used for tracing anymore after this.
// Executing DEJAVIEW_TE() on an unregistered category will cause a null pointer
// dereference.
static inline void DejaViewTeCategoryUnregister(
    struct DejaViewTeCategory* cat) {
  DejaViewTeCategoryImplDestroy(cat->impl);
  cat->impl = DEJAVIEW_NULL;
  cat->enabled = &dejaview_atomic_false;
  cat->cat_iid = 0;
}

// Calls DejaViewTeCategoryUnregister() on multiple categories.
//
// WARNING: The categories cannot be used for tracing anymore after this.
// Executing DEJAVIEW_TE() on unregistered categories will cause a null pointer
// dereference.
static inline void DejaViewTeUnregisterCategories(
    struct DejaViewTeCategory* cats[],
    size_t size) {
  for (size_t i = 0; i < size; i++) {
    DejaViewTeCategoryUnregister(cats[i]);
  }
}

// A track. Must be registered before it can be used in trace events.
struct DejaViewTeRegisteredTrack {
  struct DejaViewTeRegisteredTrackImpl impl;
};

// Returns the track uuid for the current process.
static inline uint64_t DejaViewTeProcessTrackUuid(void) {
  return dejaview_te_process_track_uuid;
}

// Returns the track uuid for the current thread.
static inline uint64_t DejaViewTeThreadTrackUuid(void) {
  return dejaview_te_process_track_uuid ^
         DEJAVIEW_STATIC_CAST(uint64_t, DejaViewGetThreadId());
}

// Returns the root track uuid.
static inline uint64_t DejaViewTeGlobalTrackUuid(void) {
  return 0;
}

// Computes the track uuid for a counter track named `name` whose parent track
// has `parent_uuid`.
static inline uint64_t DejaViewTeCounterTrackUuid(const char* name,
                                                  uint64_t parent_uuid) {
  const uint64_t kCounterMagic = 0xb1a4a67d7970839eul;
  uint64_t uuid = kCounterMagic;
  uuid ^= parent_uuid;
  uuid ^= DejaViewFnv1a(name, strlen(name));
  return uuid;
}

// Computes the track uuid for a track named `name` with unique `id` whose
// parent track has `parent_uuid`.
static inline uint64_t DejaViewTeNamedTrackUuid(const char* name,
                                                uint64_t id,
                                                uint64_t parent_uuid) {
  uint64_t uuid = parent_uuid;
  uuid ^= DejaViewFnv1a(name, strlen(name));
  uuid ^= id;
  return uuid;
}

// Serializes the descriptor for a counter track named `name` with
// `parent_uuid`. `track_uuid` must be the return value of
// DejaViewTeCounterTrackUuid().
static inline void DejaViewTeCounterTrackFillDesc(
    struct dejaview_protos_TrackDescriptor* desc,
    const char* name,
    uint64_t parent_track_uuid,
    uint64_t track_uuid) {
  dejaview_protos_TrackDescriptor_set_uuid(desc, track_uuid);
  if (parent_track_uuid) {
    dejaview_protos_TrackDescriptor_set_parent_uuid(desc, parent_track_uuid);
  }
  dejaview_protos_TrackDescriptor_set_cstr_name(desc, name);
  {
    struct dejaview_protos_CounterDescriptor counter;
    dejaview_protos_TrackDescriptor_begin_counter(desc, &counter);
    dejaview_protos_TrackDescriptor_end_counter(desc, &counter);
  }
}

// Serializes the descriptor for a track named `name` with unique `id` and
// `parent_uuid`. `track_uuid` must be the return value of
// DejaViewTeNamedTrackUuid().
static inline void DejaViewTeNamedTrackFillDesc(
    struct dejaview_protos_TrackDescriptor* desc,
    const char* track_name,
    uint64_t id,
    uint64_t parent_track_uuid,
    uint64_t track_uuid) {
  (void)id;
  dejaview_protos_TrackDescriptor_set_uuid(desc, track_uuid);
  if (parent_track_uuid) {
    dejaview_protos_TrackDescriptor_set_parent_uuid(desc, parent_track_uuid);
  }
  dejaview_protos_TrackDescriptor_set_cstr_name(desc, track_name);
}

// Registers a track named `name` with unique `id` and `parent_uuid` into
// `track`.
static inline void DejaViewTeNamedTrackRegister(
    struct DejaViewTeRegisteredTrack* track,
    const char* name,
    uint64_t id,
    uint64_t parent_track_uuid) {
  uint64_t uuid;
  // Build the TrackDescriptor protobuf message.
  struct DejaViewPbMsgWriter writer;
  struct DejaViewHeapBuffer* hb = DejaViewHeapBufferCreate(&writer.writer);
  struct dejaview_protos_TrackDescriptor desc;
  DejaViewPbMsgInit(&desc.msg, &writer);

  uuid = DejaViewTeNamedTrackUuid(name, id, parent_track_uuid);

  DejaViewTeNamedTrackFillDesc(&desc, name, id, parent_track_uuid, uuid);

  track->impl.descriptor_size =
      DejaViewStreamWriterGetWrittenSize(&writer.writer);
  track->impl.descriptor = malloc(track->impl.descriptor_size);
  track->impl.uuid = uuid;
  DejaViewHeapBufferCopyInto(hb, &writer.writer, track->impl.descriptor,
                             track->impl.descriptor_size);
  DejaViewHeapBufferDestroy(hb, &writer.writer);
}

// Registers a counter track named `name` with and `parent_uuid` into `track`.
static inline void DejaViewTeCounterTrackRegister(
    struct DejaViewTeRegisteredTrack* track,
    const char* name,
    uint64_t parent_track_uuid) {
  uint64_t uuid;
  struct DejaViewPbMsgWriter writer;
  struct DejaViewHeapBuffer* hb = DejaViewHeapBufferCreate(&writer.writer);
  struct dejaview_protos_TrackDescriptor desc;
  DejaViewPbMsgInit(&desc.msg, &writer);

  uuid = DejaViewTeCounterTrackUuid(name, parent_track_uuid);

  DejaViewTeCounterTrackFillDesc(&desc, name, parent_track_uuid, uuid);

  track->impl.descriptor_size =
      DejaViewStreamWriterGetWrittenSize(&writer.writer);
  track->impl.descriptor = malloc(track->impl.descriptor_size);
  track->impl.uuid = uuid;
  DejaViewHeapBufferCopyInto(hb, &writer.writer, track->impl.descriptor,
                             track->impl.descriptor_size);
  DejaViewHeapBufferDestroy(hb, &writer.writer);
}

// Unregisters the previously registered track `track`.
static inline void DejaViewTeRegisteredTrackUnregister(
    struct DejaViewTeRegisteredTrack* track) {
  free(track->impl.descriptor);
  track->impl.descriptor = DEJAVIEW_NULL;
  track->impl.descriptor_size = 0;
}

// Identifies a flow: a link between two events.
struct DejaViewTeFlow {
  uint64_t id;
};

// Returns a flow that's scoped to this process. It can be used to link events
// inside this process.
static inline struct DejaViewTeFlow DejaViewTeProcessScopedFlow(uint64_t id) {
  struct DejaViewTeFlow ret;
  ret.id = id ^ dejaview_te_process_track_uuid;
  return ret;
}

// Returns a global flow. It can be used to link events between different
// processes.
static inline struct DejaViewTeFlow DejaViewTeGlobalFlow(uint64_t id) {
  struct DejaViewTeFlow ret;
  ret.id = id;
  return ret;
}

// Returns a static-category-like object used when dynamic categories are passed
// as extra parameters.
static inline struct DejaViewTeCategory DejaViewTeRegisteredDynamicCategory(
    void) {
  struct DejaViewTeCategory ret = {
      dejaview_te_any_categories_enabled,
      dejaview_te_any_categories,
      {DEJAVIEW_NULL, DEJAVIEW_NULL, DEJAVIEW_NULL, 0},
      0};
  return ret;
}

// Iterator for all the active instances (on this thread) of a data source type.
struct DejaViewTeLlIterator {
  struct DejaViewTeLlImplIterator impl;
};

static inline struct DejaViewTeLlIterator DejaViewTeLlBeginSlowPath(
    struct DejaViewTeCategory* cat,
    struct DejaViewTeTimestamp ts) {
  struct DejaViewTeLlIterator ret;
  ret.impl = DejaViewTeLlImplBegin(cat->impl, ts);
  return ret;
}

static inline void DejaViewTeLlNext(struct DejaViewTeCategory* cat,
                                    struct DejaViewTeTimestamp ts,
                                    struct DejaViewTeLlIterator* iterator) {
  DejaViewTeLlImplNext(cat->impl, ts, &iterator->impl);
}

static inline void DejaViewTeLlBreak(struct DejaViewTeCategory* cat,
                                     struct DejaViewTeLlIterator* iterator) {
  if (iterator->impl.ds.tracer) {
    DejaViewTeLlImplBreak(cat->impl, &iterator->impl);
  }
}

// Checks if the category descriptor `dyn_cat` is enabled in the current active
// instance pointed by `iterator`.
static inline bool DejaViewTeLlDynCatEnabled(
    struct DejaViewTeLlIterator* iterator,
    const struct DejaViewTeCategoryDescriptor* dyn_cat) {
  return DejaViewTeLlImplDynCatEnabled(iterator->impl.ds.tracer,
                                       iterator->impl.ds.inst_id, dyn_cat);
}

// Initializes `root` to write a new packet to the data source instance pointed
// by `iterator`.
static inline void DejaViewTeLlPacketBegin(
    struct DejaViewTeLlIterator* iterator,
    struct DejaViewDsRootTracePacket* root) {
  root->writer.writer =
      DejaViewDsTracerImplPacketBegin(iterator->impl.ds.tracer);
  DejaViewPbMsgInit(&root->msg.msg, &root->writer);
}

// Finishes writing the packet pointed by `root` on the data source instance
// pointer by `iterator`.
static inline void DejaViewTeLlPacketEnd(
    struct DejaViewTeLlIterator* iterator,
    struct DejaViewDsRootTracePacket* root) {
  DejaViewPbMsgFinalize(&root->msg.msg);
  DejaViewDsTracerImplPacketEnd(iterator->impl.ds.tracer, &root->writer.writer);
}

static inline void DejaViewTeLlFlushPacket(
    struct DejaViewTeLlIterator* iterator) {
  DejaViewDsTracerImplFlush(iterator->impl.ds.tracer, DEJAVIEW_NULL,
                            DEJAVIEW_NULL);
}

// Returns true if the track event incremental state has already seen in the
// past a track with `uuid` as track UUID.
static inline bool DejaViewTeLlTrackSeen(struct DejaViewTeLlImplIncr* incr,
                                         uint64_t uuid) {
  return DejaViewTeLlImplTrackSeen(incr, uuid);
}

// Interning:
//
// it's possible to avoid repeating the same data over and over in a trace by
// using "interning".
//
// `type` is a field id in the `dejaview.protos.InternedData` protobuf message.
// `data` and `data_size` point to the raw data that is potentially repeated.
// The function returns an integer (the iid) that can be used instead of
// serializing the data directly in the packet. `*seen` is set to false if this
// is the first time the library observed this data for this specific type
// (therefore it allocated a new iid).
static inline uint64_t DejaViewTeLlIntern(struct DejaViewTeLlImplIncr* incr,
                                          int32_t type,
                                          const void* data,
                                          size_t data_size,
                                          bool* seen) {
  return DejaViewTeLlImplIntern(incr, type, data, data_size, seen);
}

// Used to lazily start, only if required, a nested InternedData submessage for
// a TracePacket `tp`. `incr` is the incremental state ABI pointer received from
// DejaViewTeLlIterator.
struct DejaViewTeLlInternContext {
  struct DejaViewTeLlImplIncr* incr;
  struct dejaview_protos_TracePacket* tp;
  struct dejaview_protos_InternedData interned;
  // true if the nested `interned` submessage has been started, false otherwise.
  bool started;
};

static inline void DejaViewTeLlInternContextInit(
    struct DejaViewTeLlInternContext* ctx,
    struct DejaViewTeLlImplIncr* incr,
    struct dejaview_protos_TracePacket* tp) {
  ctx->incr = incr;
  ctx->tp = tp;
  ctx->started = false;
}

static inline void DejaViewTeLlInternContextStartIfNeeded(
    struct DejaViewTeLlInternContext* ctx) {
  if (!ctx->started) {
    ctx->started = true;
    dejaview_protos_TracePacket_begin_interned_data(ctx->tp, &ctx->interned);
  }
}

static inline void DejaViewTeLlInternContextDestroy(
    struct DejaViewTeLlInternContext* ctx) {
  if (ctx->started) {
    dejaview_protos_TracePacket_end_interned_data(ctx->tp, &ctx->interned);
  }
}

static inline void DejaViewTeLlInternRegisteredCat(
    struct DejaViewTeLlInternContext* ctx,
    struct DejaViewTeCategory* reg_cat) {
  uint64_t iid = reg_cat->cat_iid;
  bool seen;

  if (!iid) {
    return;
  }
  DejaViewTeLlIntern(ctx->incr,
                     dejaview_protos_InternedData_event_categories_field_number,
                     &iid, sizeof(iid), &seen);
  if (!seen) {
    struct dejaview_protos_EventCategory event_category;
    DejaViewTeLlInternContextStartIfNeeded(ctx);

    dejaview_protos_InternedData_begin_event_categories(&ctx->interned,
                                                        &event_category);
    dejaview_protos_EventCategory_set_iid(&event_category, iid);
    dejaview_protos_EventCategory_set_cstr_name(&event_category,
                                                reg_cat->desc.name);
    dejaview_protos_InternedData_end_event_categories(&ctx->interned,
                                                      &event_category);
  }
}

static inline void DejaViewTeLlWriteRegisteredCat(
    struct dejaview_protos_TrackEvent* te,
    struct DejaViewTeCategory* reg_cat) {
  if (reg_cat->cat_iid) {
    dejaview_protos_TrackEvent_set_category_iids(te, reg_cat->cat_iid);
  } else if (reg_cat->desc.name) {
    dejaview_protos_TrackEvent_set_cstr_categories(te, reg_cat->desc.name);
  }
}

static inline void DejaViewTeLlWriteDynamicCat(
    struct dejaview_protos_TrackEvent* te,
    struct DejaViewTeCategoryDescriptor* dyn_cat,
    int32_t type) {
  if (dyn_cat && type != DEJAVIEW_TE_TYPE_SLICE_END &&
      type != DEJAVIEW_TE_TYPE_COUNTER) {
    dejaview_protos_TrackEvent_set_cstr_categories(te, dyn_cat->name);
  }
}

static inline uint64_t DejaViewTeLlInternEventName(
    struct DejaViewTeLlInternContext* ctx,
    const char* name) {
  uint64_t iid = 0;
  if (name) {
    bool seen;
    iid = DejaViewTeLlIntern(
        ctx->incr, dejaview_protos_InternedData_event_names_field_number, name,
        strlen(name), &seen);
    if (!seen) {
      struct dejaview_protos_EventName event_name;
      DejaViewTeLlInternContextStartIfNeeded(ctx);
      dejaview_protos_InternedData_begin_event_names(&ctx->interned,
                                                     &event_name);
      dejaview_protos_EventName_set_iid(&event_name, iid);
      dejaview_protos_EventName_set_cstr_name(&event_name, name);
      dejaview_protos_InternedData_end_event_names(&ctx->interned, &event_name);
    }
  }
  return iid;
}

static inline void DejaViewTeLlWriteEventName(
    struct dejaview_protos_TrackEvent* te,
    const char* name) {
  if (name) {
    dejaview_protos_TrackEvent_set_cstr_name(te, name);
  }
}

static inline void DejaViewTeLlWriteInternedEventName(
    struct dejaview_protos_TrackEvent* te,
    uint64_t iid) {
  if (iid != 0) {
    dejaview_protos_TrackEvent_set_name_iid(te, iid);
  }
}

static inline void DejaViewTeLlWriteTimestamp(
    struct dejaview_protos_TracePacket* tp,
    const struct DejaViewTeTimestamp* ts) {
  uint32_t clock_id = ts->clock_id;
  dejaview_protos_TracePacket_set_timestamp(tp, ts->value);
  dejaview_protos_TracePacket_set_timestamp_clock_id(tp, clock_id);
}

static inline uint64_t DejaViewTeLlInternDbgArgName(
    struct DejaViewTeLlInternContext* ctx,
    const char* name) {
  uint64_t iid = 0;
  if (name) {
    bool seen;
    iid = DejaViewTeLlIntern(
        ctx->incr,
        dejaview_protos_InternedData_debug_annotation_names_field_number, name,
        strlen(name), &seen);
    if (!seen) {
      struct dejaview_protos_EventName event_name;
      DejaViewTeLlInternContextStartIfNeeded(ctx);
      dejaview_protos_InternedData_begin_event_names(&ctx->interned,
                                                     &event_name);
      dejaview_protos_EventName_set_iid(&event_name, iid);
      dejaview_protos_EventName_set_cstr_name(&event_name, name);
      dejaview_protos_InternedData_end_event_names(&ctx->interned, &event_name);
    }
  }
  return iid;
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_TRACK_EVENT_H_
