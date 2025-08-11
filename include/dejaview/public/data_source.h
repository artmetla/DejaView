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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_DATA_SOURCE_H_
#define INCLUDE_DEJAVIEW_PUBLIC_DATA_SOURCE_H_

#include <stdlib.h>
#include <string.h>

#include "dejaview/public/abi/atomic.h"
#include "dejaview/public/abi/data_source_abi.h"
#include "dejaview/public/abi/heap_buffer.h"
#include "dejaview/public/compiler.h"
#include "dejaview/public/pb_msg.h"
#include "dejaview/public/pb_utils.h"
#include "dejaview/public/protos/common/data_source_descriptor.pzc.h"
#include "dejaview/public/protos/trace/trace_packet.pzc.h"

// A data source type.
struct DejaViewDs {
  // Pointer to a (atomic) boolean, which is set to true if there is at
  // least one enabled instance of this data source type.
  DEJAVIEW_ATOMIC(bool) * enabled;
  struct DejaViewDsImpl* impl;
};

// Initializes a DejaViewDs struct.
#define DEJAVIEW_DS_INIT() \
  { &dejaview_atomic_false, DEJAVIEW_NULL }

// All the callbacks are optional and can be NULL if not needed.
//
struct DejaViewDsParams {
  // Instance lifecycle callbacks.
  //
  // Can be called from any thread.
  DejaViewDsOnSetupCb on_setup_cb;
  DejaViewDsOnStartCb on_start_cb;
  DejaViewDsOnStopCb on_stop_cb;
  DejaViewDsOnDestroyCb on_destroy_cb;
  DejaViewDsOnFlushCb on_flush_cb;

  // These are called to create/delete custom thread-local instance state, which
  // can be accessed with DejaViewDsTracerImplGetCustomTls().
  //
  // Called from inside a trace point. Trace points inside these will be
  // ignored.
  DejaViewDsOnCreateCustomState on_create_tls_cb;
  DejaViewDsOnDeleteCustomState on_delete_tls_cb;

  // These are called to create/delete custom thread-local instance incremental
  // state. Incremental state may be cleared periodically by the tracing service
  // and can be accessed with DejaViewDsTracerImplGetIncrementalState().
  //
  // Called from inside a trace point. Trace points inside these will be
  // ignored.
  DejaViewDsOnCreateCustomState on_create_incr_cb;
  DejaViewDsOnDeleteCustomState on_delete_incr_cb;

  // Passed to all the callbacks as the `user_arg` param.
  void* user_arg;

  // How to behave when running out of shared memory buffer space.
  enum DejaViewDsBufferExhaustedPolicy buffer_exhausted_policy;

  // When true the data source is expected to ack the stop request through the
  // NotifyDataSourceStopped() IPC.
  bool will_notify_on_stop;
};

static inline struct DejaViewDsParams DejaViewDsParamsDefault(void) {
  struct DejaViewDsParams ret = {DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_NULL,
                                 DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_DROP,
                                 true};
  return ret;
}

// Registers the data source type `ds`, named `data_source_name` with the global
// dejaview producer.
static inline bool DejaViewDsRegister(struct DejaViewDs* ds,
                                      const char* data_source_name,
                                      struct DejaViewDsParams params) {
  struct DejaViewDsImpl* ds_impl;
  bool success;
  void* desc_buf;
  size_t desc_size;

  ds->enabled = &dejaview_atomic_false;
  ds->impl = DEJAVIEW_NULL;

  {
    struct DejaViewPbMsgWriter writer;
    struct DejaViewHeapBuffer* hb = DejaViewHeapBufferCreate(&writer.writer);
    struct dejaview_protos_DataSourceDescriptor desc;
    DejaViewPbMsgInit(&desc.msg, &writer);

    dejaview_protos_DataSourceDescriptor_set_cstr_name(&desc, data_source_name);
    dejaview_protos_DataSourceDescriptor_set_will_notify_on_stop(
        &desc, params.will_notify_on_stop);

    desc_size = DejaViewStreamWriterGetWrittenSize(&writer.writer);
    desc_buf = malloc(desc_size);
    DejaViewHeapBufferCopyInto(hb, &writer.writer, desc_buf, desc_size);
    DejaViewHeapBufferDestroy(hb, &writer.writer);
  }

  if (!desc_buf) {
    return false;
  }

  ds_impl = DejaViewDsImplCreate();
  if (params.on_setup_cb) {
    DejaViewDsSetOnSetupCallback(ds_impl, params.on_setup_cb);
  }
  if (params.on_start_cb) {
    DejaViewDsSetOnStartCallback(ds_impl, params.on_start_cb);
  }
  if (params.on_stop_cb) {
    DejaViewDsSetOnStopCallback(ds_impl, params.on_stop_cb);
  }
  if (params.on_destroy_cb) {
    DejaViewDsSetOnDestroyCallback(ds_impl, params.on_destroy_cb);
  }
  if (params.on_flush_cb) {
    DejaViewDsSetOnFlushCallback(ds_impl, params.on_flush_cb);
  }
  if (params.on_create_tls_cb) {
    DejaViewDsSetOnCreateTls(ds_impl, params.on_create_tls_cb);
  }
  if (params.on_delete_tls_cb) {
    DejaViewDsSetOnDeleteTls(ds_impl, params.on_delete_tls_cb);
  }
  if (params.on_create_incr_cb) {
    DejaViewDsSetOnCreateIncr(ds_impl, params.on_create_incr_cb);
  }
  if (params.on_delete_incr_cb) {
    DejaViewDsSetOnDeleteIncr(ds_impl, params.on_delete_incr_cb);
  }
  if (params.user_arg) {
    DejaViewDsSetCbUserArg(ds_impl, params.user_arg);
  }
  if (params.buffer_exhausted_policy !=
      DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_DROP) {
    DejaViewDsSetBufferExhaustedPolicy(ds_impl, params.buffer_exhausted_policy);
  }

  success = DejaViewDsImplRegister(ds_impl, &ds->enabled, desc_buf, desc_size);
  free(desc_buf);
  if (!success) {
    return false;
  }
  ds->impl = ds_impl;
  return true;
}

// Iterator for all the active instances (on this thread) of a data source type.
struct DejaViewDsTracerIterator {
  struct DejaViewDsImplTracerIterator impl;
};

static inline struct DejaViewDsTracerIterator DejaViewDsTraceIterateBegin(
    struct DejaViewDs* ds) {
  struct DejaViewDsTracerIterator ret;
  DEJAVIEW_ATOMIC(bool)* enabled = ds->enabled;
  if (DEJAVIEW_LIKELY(!DEJAVIEW_ATOMIC_LOAD_EXPLICIT(
          enabled, DEJAVIEW_MEMORY_ORDER_RELAXED))) {
    // Tracing fast path: bail out immediately if the enabled flag is false.
    ret.impl.tracer = DEJAVIEW_NULL;
  } else {
    // Else, make an ABI call to start iteration over the data source type
    // active instances.
    ret.impl = DejaViewDsImplTraceIterateBegin(ds->impl);
  }
  return ret;
}

static inline void DejaViewDsTraceIterateNext(
    struct DejaViewDs* ds,
    struct DejaViewDsTracerIterator* iterator) {
  DejaViewDsImplTraceIterateNext(ds->impl, &iterator->impl);
}

static inline void DejaViewDsTraceIterateBreak(
    struct DejaViewDs* ds,
    struct DejaViewDsTracerIterator* iterator) {
  if (iterator->impl.tracer) {
    DejaViewDsImplTraceIterateBreak(ds->impl, &iterator->impl);
  }
}

// For loop over the active instances of a data source type.
//
// `NAME` is the data source type (struct DejaViewDs).
//
// A local variable called `ITERATOR` will be instantiated. It can be used to
// perform tracing on each instance.
//
// N.B. The iteration MUST NOT be interrupted early with `break`.
// DEJAVIEW_DS_TRACE_BREAK should be used instead.
#define DEJAVIEW_DS_TRACE(NAME, ITERATOR)         \
  for (struct DejaViewDsTracerIterator ITERATOR = \
           DejaViewDsTraceIterateBegin(&(NAME));  \
       (ITERATOR).impl.tracer != NULL;            \
       DejaViewDsTraceIterateNext(&(NAME), &(ITERATOR)))

// Used to break the iteration in a DEJAVIEW_DS_TRACE loop.
#define DEJAVIEW_DS_TRACE_BREAK(NAME, ITERATOR)      \
  DejaViewDsTraceIterateBreak(&(NAME), &(ITERATOR)); \
  break

static inline void* DejaViewDsGetCustomTls(
    struct DejaViewDs* ds,
    struct DejaViewDsTracerIterator* iterator) {
  return DejaViewDsImplGetCustomTls(ds->impl, iterator->impl.tracer,
                                    iterator->impl.inst_id);
}

static inline void* DejaViewDsGetIncrementalState(
    struct DejaViewDs* ds,
    struct DejaViewDsTracerIterator* iterator) {
  return DejaViewDsImplGetIncrementalState(ds->impl, iterator->impl.tracer,
                                           iterator->impl.inst_id);
}

// Used to write a TracePacket on a data source instance. Stores the writer and
// the TracePacket message.
struct DejaViewDsRootTracePacket {
  struct DejaViewPbMsgWriter writer;
  struct dejaview_protos_TracePacket msg;
};

// Initializes `root` to write a new packet to the data source instance pointed
// by `iterator`.
static inline void DejaViewDsTracerPacketBegin(
    struct DejaViewDsTracerIterator* iterator,
    struct DejaViewDsRootTracePacket* root) {
  root->writer.writer = DejaViewDsTracerImplPacketBegin(iterator->impl.tracer);
  DejaViewPbMsgInit(&root->msg.msg, &root->writer);
}

// Finishes writing the packet pointed by `root` on the data source instance
// pointer by `iterator`.
static inline void DejaViewDsTracerPacketEnd(
    struct DejaViewDsTracerIterator* iterator,
    struct DejaViewDsRootTracePacket* root) {
  DejaViewPbMsgFinalize(&root->msg.msg);
  DejaViewDsTracerImplPacketEnd(iterator->impl.tracer, &root->writer.writer);
}

static inline void DejaViewDsTracerFlush(
    struct DejaViewDsTracerIterator* iterator,
    DejaViewDsTracerOnFlushCb cb,
    void* ctx) {
  DejaViewDsTracerImplFlush(iterator->impl.tracer, cb, ctx);
}

#endif  // INCLUDE_DEJAVIEW_PUBLIC_DATA_SOURCE_H_
