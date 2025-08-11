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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_DATA_SOURCE_ABI_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_DATA_SOURCE_ABI_H_

#include <stdbool.h>
#include <stdint.h>

#include "dejaview/public/abi/atomic.h"
#include "dejaview/public/abi/export.h"
#include "dejaview/public/abi/stream_writer_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Internal representation of a data source type.
struct DejaViewDsImpl;

// Internal thread local state of a data source type.
struct DejaViewDsTlsImpl;

// Internal thread local state of a data source instance used for tracing.
struct DejaViewDsTracerImpl;

// A global atomic boolean that's always false.
extern DEJAVIEW_SDK_EXPORT DEJAVIEW_ATOMIC(bool) dejaview_atomic_false;

// There can be more than one data source instance for each data source type.
// This index identifies one of them.
typedef uint32_t DejaViewDsInstanceIndex;

// Creates a data source type.
//
// The data source type needs to be registered later with
// DejaViewDsImplRegister().
DEJAVIEW_SDK_EXPORT struct DejaViewDsImpl* DejaViewDsImplCreate(void);

// Opaque handle used to perform operations from the OnSetup callback. Unused
// for now.
struct DejaViewDsOnSetupArgs;

// Called when a data source instance of a specific type is created. `ds_config`
// points to a serialized dejaview.protos.DataSourceConfig message,
// `ds_config_size` bytes long. `user_arg` is the value passed to
// DejaViewDsSetCbUserArg(). The return value of this is passed to all other
// callbacks (for this data source instance) as `inst_ctx` and can be accessed
// during tracing with DejaViewDsImplGetInstanceLocked().
//
// Can be called from any thread.
typedef void* (*DejaViewDsOnSetupCb)(struct DejaViewDsImpl*,
                                     DejaViewDsInstanceIndex inst_id,
                                     void* ds_config,
                                     size_t ds_config_size,
                                     void* user_arg,
                                     struct DejaViewDsOnSetupArgs* args);

// Opaque handle used to perform operations from the OnSetup callback. Unused
// for now.
struct DejaViewDsOnStartArgs;

// Called when tracing starts for a data source instance. `user_arg` is the
// value passed to DejaViewDsSetCbUserArg(). `inst_ctx` is the return
// value of DejaViewDsOnSetupCb.
//
// Can be called from any thread.
typedef void (*DejaViewDsOnStartCb)(struct DejaViewDsImpl*,
                                    DejaViewDsInstanceIndex inst_id,
                                    void* user_arg,
                                    void* inst_ctx,
                                    struct DejaViewDsOnStartArgs* args);

// Opaque handle used to perform operations from the OnStop callback.
struct DejaViewDsOnStopArgs;

// Opaque handle used to signal when the data source stop operation is
// complete.
struct DejaViewDsAsyncStopper;

// Tells the tracing service to postpone the stopping of a data source instance.
// The returned handle can be used to signal the tracing service when the data
// source instance can be stopped.
DEJAVIEW_SDK_EXPORT struct DejaViewDsAsyncStopper* DejaViewDsOnStopArgsPostpone(
    struct DejaViewDsOnStopArgs*);

// Tells the tracing service to stop a data source instance (whose stop
// operation was previously postponed with DejaViewDsOnStopArgsPostpone).
DEJAVIEW_SDK_EXPORT void DejaViewDsStopDone(struct DejaViewDsAsyncStopper*);

// Called when tracing stops for a data source instance. `user_arg` is the value
// passed to DejaViewDsSetCbUserArg(). `inst_ctx` is the return value of
// DejaViewDsOnSetupCb.`args` can be used to postpone stopping this data source
// instance. Note that, in general, it's not a good idea to destroy `inst_ctx`
// here: DejaViewDsOnDestroyCb should be used instead.
//
// Can be called from any thread. Blocking this for too long it's not a good
// idea and can cause deadlocks. Use DejaViewDsOnStopArgsPostpone() to postpone
// disabling the data source instance.
typedef void (*DejaViewDsOnStopCb)(struct DejaViewDsImpl*,
                                   DejaViewDsInstanceIndex inst_id,
                                   void* user_arg,
                                   void* inst_ctx,
                                   struct DejaViewDsOnStopArgs* args);

// Called after tracing has been stopped for a data source instance, to signal
// that `inst_ctx` (which is the return value of DejaViewDsOnSetupCb) can
// potentially be destroyed. `user_arg` is the value passed to
// DejaViewDsSetCbUserArg().
//
// Can be called from any thread.
typedef void (*DejaViewDsOnDestroyCb)(struct DejaViewDsImpl*,
                                      void* user_arg,
                                      void* inst_ctx);

// Opaque handle used to perform operations from the OnFlush callback.
struct DejaViewDsOnFlushArgs;

// Opaque handle used to signal when the data source flush operation is
// complete.
struct DejaViewDsAsyncFlusher;

// Tells the tracing service to postpone acknowledging the flushing of a data
// source instance. The returned handle can be used to signal the tracing
// service when the data source instance flushing has completed.
DEJAVIEW_SDK_EXPORT struct DejaViewDsAsyncFlusher*
DejaViewDsOnFlushArgsPostpone(struct DejaViewDsOnFlushArgs*);

// Tells the tracing service that the flush operation is complete for a data
// source instance (whose stop operation was previously postponed with
// DejaViewDsOnFlushArgsPostpone).
DEJAVIEW_SDK_EXPORT void DejaViewDsFlushDone(struct DejaViewDsAsyncFlusher*);

// Called when the tracing service requires all the pending tracing data to be
// flushed for a data source instance. `user_arg` is the value passed to
// DejaViewDsSetCbUserArg(). `inst_ctx` is the return value of
// DejaViewDsOnSetupCb. `args` can be used to postpone stopping this data source
// instance.
//
// Can be called from any thread. Blocking this for too long it's not a good
// idea and can cause deadlocks. Use DejaViewDsOnFlushArgsPostpone() to postpone
// disabling the data source instance.
typedef void (*DejaViewDsOnFlushCb)(struct DejaViewDsImpl*,
                                    DejaViewDsInstanceIndex inst_id,
                                    void* user_arg,
                                    void* inst_ctx,
                                    struct DejaViewDsOnFlushArgs* args);

// Creates custom state (either thread local state or incremental state) for
// instance `inst_id`. `user_arg` is the value passed to
// DejaViewDsSetCbUserArg().
typedef void* (*DejaViewDsOnCreateCustomState)(
    struct DejaViewDsImpl*,
    DejaViewDsInstanceIndex inst_id,
    struct DejaViewDsTracerImpl* tracer,
    void* user_arg);

// Deletes the previously created custom state `obj`.
typedef void (*DejaViewDsOnDeleteCustomState)(void* obj);

// Setters for callbacks: can not be called after DejaViewDsImplRegister().

DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnSetupCallback(struct DejaViewDsImpl*,
                                                      DejaViewDsOnSetupCb);

DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnStartCallback(struct DejaViewDsImpl*,
                                                      DejaViewDsOnStartCb);

DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnStopCallback(struct DejaViewDsImpl*,
                                                     DejaViewDsOnStopCb);

DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnDestroyCallback(struct DejaViewDsImpl*,
                                                        DejaViewDsOnDestroyCb);

DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnFlushCallback(struct DejaViewDsImpl*,
                                                      DejaViewDsOnFlushCb);

// Callbacks for custom per instance thread local state.
//
// Called from inside a trace point. Trace points inside these will be
// ignored.
DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnCreateTls(
    struct DejaViewDsImpl*,
    DejaViewDsOnCreateCustomState);
DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnDeleteTls(
    struct DejaViewDsImpl*,
    DejaViewDsOnDeleteCustomState);

// Callbacks for custom per instance thread local incremental state.
//
// Called from inside a trace point. Trace points inside these will be
// ignored.
DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnCreateIncr(
    struct DejaViewDsImpl*,
    DejaViewDsOnCreateCustomState);
DEJAVIEW_SDK_EXPORT void DejaViewDsSetOnDeleteIncr(
    struct DejaViewDsImpl*,
    DejaViewDsOnDeleteCustomState);

// Stores the `user_arg` that's going to be passed later to the callbacks for
// this data source type.
DEJAVIEW_SDK_EXPORT void DejaViewDsSetCbUserArg(struct DejaViewDsImpl*,
                                                void* user_arg);

enum DejaViewDsBufferExhaustedPolicy {
  // If the data source runs out of space when trying to acquire a new chunk,
  // it will drop data.
  DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_DROP = 0,
  // If the data source runs out of space when trying to acquire a new chunk,
  // it will stall, retry and eventually abort if a free chunk is not acquired
  // after a while.
  DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_STALL_AND_ABORT = 1,
};

// If the data source doesn't find an empty chunk when trying to emit tracing
// data, it will behave according to `policy` (which is a `enum
// DejaViewDsBufferExhaustedPolicy`).
//
// Should not be called after DejaViewDsImplRegister().
//
// Returns true if successful, false otherwise.
DEJAVIEW_SDK_EXPORT bool DejaViewDsSetBufferExhaustedPolicy(
    struct DejaViewDsImpl*,
    uint32_t policy);

// Registers the `*ds_impl` data source type.
//
// `ds_impl` must be obtained via a call to `DejaViewDsImplCreate()`.
//
// `**enabled_ptr` will be set to true when the data source type has been
// enabled.
//
// `descriptor` should point to a serialized
// dejaview.protos.DataSourceDescriptor message, `descriptor_size` bytes long.
//
// Returns `true` in case of success, `false` in case of failure (in which case
// `ds_impl is invalid`).
DEJAVIEW_SDK_EXPORT bool DejaViewDsImplRegister(struct DejaViewDsImpl* ds_impl,
                                                DEJAVIEW_ATOMIC(bool) *
                                                    *enabled_ptr,
                                                const void* descriptor,
                                                size_t descriptor_size);

// Updates the descriptor the `*ds_impl` data source type.
//
// `descriptor` should point to a serialized
// dejaview.protos.DataSourceDescriptor message, `descriptor_size` bytes long.
DEJAVIEW_SDK_EXPORT void DejaViewDsImplUpdateDescriptor(
    struct DejaViewDsImpl* ds_impl,
    const void* descriptor,
    size_t descriptor_size);

// Tries to get the `inst_ctx` returned by DejaViewDsOnSetupCb() for the
// instance with index `inst_id`.
//
// If successful, returns a non-null pointer and acquires a lock, which must be
// released with DejaViewDsImplReleaseInstanceLocked.
//
// If unsuccessful (because the instance was destroyed in the meantime) or if
// DejaViewDsOnSetupCb() returned a null value, returns null and does not
// acquire any lock.
DEJAVIEW_SDK_EXPORT void* DejaViewDsImplGetInstanceLocked(
    struct DejaViewDsImpl* ds_impl,
    DejaViewDsInstanceIndex inst_id);

// Releases a lock previouly acquired by a DejaViewDsImplGetInstanceLocked()
// call, which must have returned a non null value.
DEJAVIEW_SDK_EXPORT void DejaViewDsImplReleaseInstanceLocked(
    struct DejaViewDsImpl* ds_impl,
    DejaViewDsInstanceIndex inst_id);

// Gets the data source thread local instance custom state created by
// the callback passed to `DejaViewDsSetOnCreateTls`.
DEJAVIEW_SDK_EXPORT void* DejaViewDsImplGetCustomTls(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsTracerImpl* tracer,
    DejaViewDsInstanceIndex inst_id);

// Gets the data source thread local instance incremental state created by
// the callback passed to `DejaViewDsSetOnCreateIncr`.
DEJAVIEW_SDK_EXPORT void* DejaViewDsImplGetIncrementalState(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsTracerImpl* tracer,
    DejaViewDsInstanceIndex inst_id);

// Iterator for all the active instances (on this thread) of a data source type.
struct DejaViewDsImplTracerIterator {
  // Instance id.
  DejaViewDsInstanceIndex inst_id;
  // Caches a pointer to the internal thread local state of the data source
  // type.
  struct DejaViewDsTlsImpl* tls;
  // Pointer to the object used to output trace packets. When nullptr, the
  // iteration is over.
  struct DejaViewDsTracerImpl* tracer;
};

// Start iterating over all the active instances of the data source type
// (`ds_impl`).
//
// If the returned tracer is not nullptr, the user must continue the iteration
// with DejaViewDsImplTraceIterateNext(), until it is. The iteration can
// only be interrupted early by calling DejaViewDsImplTraceIterateBreak().
DEJAVIEW_SDK_EXPORT struct DejaViewDsImplTracerIterator
DejaViewDsImplTraceIterateBegin(struct DejaViewDsImpl* ds_impl);

// Advances the iterator to the next active instance of the data source type
// (`ds_impl`).
//
// The user must call DejaViewDsImplTraceIterateNext(), until it returns a
// nullptr tracer. The iteration can only be interrupted early by calling
// DejaViewDsImplTraceIterateBreak().
DEJAVIEW_SDK_EXPORT void DejaViewDsImplTraceIterateNext(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsImplTracerIterator* iterator);

// Prematurely interrupts iteration over all the active instances of the data
// source type (`ds_impl`).
DEJAVIEW_SDK_EXPORT void DejaViewDsImplTraceIterateBreak(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsImplTracerIterator* iterator);

// Creates a new trace packet on `tracer`. Returns a stream writer that can be
// used to write data to the packet. The caller must use
// DejaViewDsTracerImplPacketEnd() when done.
DEJAVIEW_SDK_EXPORT struct DejaViewStreamWriter DejaViewDsTracerImplPacketBegin(
    struct DejaViewDsTracerImpl* tracer);

// Signals that the trace packets created previously on `tracer` with
// DejaViewDsTracerImplBeginPacket(), has been fully written.
//
// `writer` should point to the writer returned by
// DejaViewDsTracerImplBeginPacket() and cannot be used anymore after this call.
DEJAVIEW_SDK_EXPORT void DejaViewDsTracerImplPacketEnd(
    struct DejaViewDsTracerImpl* tracer,
    struct DejaViewStreamWriter* writer);

// Called when a flush request is complete.
typedef void (*DejaViewDsTracerOnFlushCb)(void* user_arg);

// Forces a commit of the thread-local tracing data written so far to the
// service.
//
// If `cb` is not NULL, it is called on a dedicated internal thread (with
// `user_arg`), when flushing is complete. It may never be called (e.g. if the
// tracing service disconnects).
//
// This is almost never required (tracing data is periodically committed as
// trace pages are filled up) and has a non-negligible performance hit.
DEJAVIEW_SDK_EXPORT void DejaViewDsTracerImplFlush(
    struct DejaViewDsTracerImpl* tracer,
    DejaViewDsTracerOnFlushCb cb,
    void* user_arg);

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_DATA_SOURCE_ABI_H_
