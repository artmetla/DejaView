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

#include "dejaview/public/abi/data_source_abi.h"

#include <bitset>

#include "dejaview/tracing/buffer_exhausted_policy.h"
#include "dejaview/tracing/data_source.h"
#include "dejaview/tracing/internal/basic_types.h"
#include "protos/dejaview/common/data_source_descriptor.gen.h"
#include "protos/dejaview/config/data_source_config.gen.h"
#include "src/shared_lib/reset_for_testing.h"
#include "src/shared_lib/stream_writer.h"

namespace {

using ::dejaview::internal::DataSourceInstanceThreadLocalState;
using ::dejaview::internal::DataSourceThreadLocalState;
using ::dejaview::internal::DataSourceType;

thread_local DataSourceThreadLocalState*
    g_tls_cache[dejaview::internal::kMaxDataSources];

}  // namespace

// Implementation of a shared library data source type (there's one of these per
// type, not per instance).
//
// Returned to the C side when invoking DejaViewDsCreateImpl(). The C side only
// has an opaque pointer to this.
struct DejaViewDsImpl {
  // Instance lifecycle callbacks.
  DejaViewDsOnSetupCb on_setup_cb = nullptr;
  DejaViewDsOnStartCb on_start_cb = nullptr;
  DejaViewDsOnStopCb on_stop_cb = nullptr;
  DejaViewDsOnDestroyCb on_destroy_cb = nullptr;
  DejaViewDsOnFlushCb on_flush_cb = nullptr;

  // These are called to create/delete custom thread-local instance state.
  DejaViewDsOnCreateCustomState on_create_tls_cb = nullptr;
  DejaViewDsOnDeleteCustomState on_delete_tls_cb = nullptr;

  // These are called to create/delete custom thread-local instance incremental
  // state.
  DejaViewDsOnCreateCustomState on_create_incr_cb = nullptr;
  DejaViewDsOnDeleteCustomState on_delete_incr_cb = nullptr;

  // Passed to all the callbacks as the `user_arg` param.
  void* cb_user_arg;

  dejaview::BufferExhaustedPolicy buffer_exhausted_policy =
      dejaview::BufferExhaustedPolicy::kDrop;

  DataSourceType cpp_type;
  std::atomic<bool> enabled{false};
  std::mutex mu;
  // Guarded by mu
  std::bitset<dejaview::internal::kMaxDataSourceInstances> enabled_instances;

  bool IsRegistered() {
    return cpp_type.static_state()->index !=
           dejaview::internal::kMaxDataSources;
  }
};

namespace dejaview {
namespace shlib {

// These are only exposed to tests.

void ResetDataSourceTls() {
  memset(g_tls_cache, 0, sizeof(g_tls_cache));
}

void DsImplDestroy(struct DejaViewDsImpl* ds_impl) {
  delete ds_impl;
}

}  // namespace shlib
}  // namespace dejaview

namespace {

// Represents a global data source instance (there can be more than one of these
// for a single data source type).
class ShlibDataSource : public dejaview::DataSourceBase {
 public:
  explicit ShlibDataSource(DejaViewDsImpl* type) : type_(*type) {}

  void OnSetup(const SetupArgs& args) override {
    if (type_.on_setup_cb) {
      std::vector<uint8_t> serialized_config = args.config->SerializeAsArray();
      inst_ctx_ = type_.on_setup_cb(
          &type_, args.internal_instance_index, serialized_config.data(),
          serialized_config.size(), type_.cb_user_arg, nullptr);
    }
    std::lock_guard<std::mutex> lock(type_.mu);
    const bool was_enabled = type_.enabled_instances.any();
    type_.enabled_instances.set(args.internal_instance_index);
    if (!was_enabled && type_.enabled_instances.any()) {
      type_.enabled.store(true, std::memory_order_release);
    }
  }

  void OnStart(const StartArgs& args) override {
    if (type_.on_start_cb) {
      type_.on_start_cb(&type_, args.internal_instance_index, type_.cb_user_arg,
                        inst_ctx_, nullptr);
    }
  }

  void OnStop(const StopArgs& args) override {
    if (type_.on_stop_cb) {
      type_.on_stop_cb(
          &type_, args.internal_instance_index, type_.cb_user_arg, inst_ctx_,
          const_cast<DejaViewDsOnStopArgs*>(
              reinterpret_cast<const DejaViewDsOnStopArgs*>(&args)));
    }

    std::lock_guard<std::mutex> lock(type_.mu);
    type_.enabled_instances.reset(args.internal_instance_index);
    if (type_.enabled_instances.none()) {
      type_.enabled.store(false, std::memory_order_release);
    }
  }

  ~ShlibDataSource() override {
    if (type_.on_destroy_cb) {
      type_.on_destroy_cb(&type_, type_.cb_user_arg, inst_ctx_);
    }
  }

  void OnFlush(const FlushArgs& args) override {
    if (type_.on_flush_cb) {
      type_.on_flush_cb(
          &type_, args.internal_instance_index, type_.cb_user_arg, inst_ctx_,
          const_cast<DejaViewDsOnFlushArgs*>(
              reinterpret_cast<const DejaViewDsOnFlushArgs*>(&args)));
    }
  }

  const DejaViewDsImpl& type() const { return type_; }

  void* inst_ctx() const { return inst_ctx_; }

 private:
  DejaViewDsImpl& type_;
  void* inst_ctx_ = nullptr;
};

struct DataSourceTraits {
  static DataSourceThreadLocalState* GetDataSourceTLS(
      dejaview::internal::DataSourceStaticState* static_state,
      dejaview::internal::TracingTLS* root_tls) {
    auto* ds_tls = &root_tls->data_sources_tls[static_state->index];
    // ds_tls->static_state can be:
    // * nullptr
    // * equal to static_state
    // * equal to the static state of a different data source, in tests (when
    //   ResetForTesting() has been used)
    // In any case, there's no need to do anything, the caller will reinitialize
    // static_state.
    return ds_tls;
  }
};

struct TracePointTraits {
  using TracePointData = DataSourceType*;
  static std::atomic<uint32_t>* GetActiveInstances(TracePointData s) {
    return s->valid_instances();
  }
};

DataSourceInstanceThreadLocalState::ObjectWithDeleter CreateShlibTls(
    DataSourceInstanceThreadLocalState* tls_inst,
    uint32_t inst_idx,
    void* ctx) {
  auto* ds_impl = reinterpret_cast<DejaViewDsImpl*>(ctx);

  void* custom_state = ds_impl->on_create_tls_cb(
      ds_impl, inst_idx, reinterpret_cast<DejaViewDsTracerImpl*>(tls_inst),
      ds_impl->cb_user_arg);
  return DataSourceInstanceThreadLocalState::ObjectWithDeleter(
      custom_state, ds_impl->on_delete_tls_cb);
}

DataSourceInstanceThreadLocalState::ObjectWithDeleter
CreateShlibIncrementalState(DataSourceInstanceThreadLocalState* tls_inst,
                            uint32_t inst_idx,
                            void* ctx) {
  auto* ds_impl = reinterpret_cast<DejaViewDsImpl*>(ctx);

  void* custom_state = ds_impl->on_create_incr_cb(
      ds_impl, inst_idx, reinterpret_cast<DejaViewDsTracerImpl*>(tls_inst),
      ds_impl->cb_user_arg);
  return DataSourceInstanceThreadLocalState::ObjectWithDeleter(
      custom_state, ds_impl->on_delete_incr_cb);
}

}  // namespace

// Exposed through data_source_abi.h
std::atomic<bool> dejaview_atomic_false{false};

struct DejaViewDsImpl* DejaViewDsImplCreate() {
  return new DejaViewDsImpl();
}

void DejaViewDsSetOnSetupCallback(struct DejaViewDsImpl* ds_impl,
                                  DejaViewDsOnSetupCb cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_setup_cb = cb;
}

void DejaViewDsSetOnStartCallback(struct DejaViewDsImpl* ds_impl,
                                  DejaViewDsOnStartCb cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_start_cb = cb;
}

void DejaViewDsSetOnStopCallback(struct DejaViewDsImpl* ds_impl,
                                 DejaViewDsOnStopCb cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_stop_cb = cb;
}

void DejaViewDsSetOnDestroyCallback(struct DejaViewDsImpl* ds_impl,
                                    DejaViewDsOnDestroyCb cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_destroy_cb = cb;
}

void DejaViewDsSetOnFlushCallback(struct DejaViewDsImpl* ds_impl,
                                  DejaViewDsOnFlushCb cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_flush_cb = cb;
}

void DejaViewDsSetOnCreateTls(struct DejaViewDsImpl* ds_impl,
                              DejaViewDsOnCreateCustomState cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_create_tls_cb = cb;
}

void DejaViewDsSetOnDeleteTls(struct DejaViewDsImpl* ds_impl,
                              DejaViewDsOnDeleteCustomState cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_delete_tls_cb = cb;
}

void DejaViewDsSetOnCreateIncr(struct DejaViewDsImpl* ds_impl,
                               DejaViewDsOnCreateCustomState cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_create_incr_cb = cb;
}

void DejaViewDsSetOnDeleteIncr(struct DejaViewDsImpl* ds_impl,
                               DejaViewDsOnDeleteCustomState cb) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->on_delete_incr_cb = cb;
}

void DejaViewDsSetCbUserArg(struct DejaViewDsImpl* ds_impl, void* user_arg) {
  DEJAVIEW_CHECK(!ds_impl->IsRegistered());
  ds_impl->cb_user_arg = user_arg;
}

bool DejaViewDsSetBufferExhaustedPolicy(struct DejaViewDsImpl* ds_impl,
                                        uint32_t policy) {
  if (ds_impl->IsRegistered()) {
    return false;
  }

  switch (policy) {
    case DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_DROP:
      ds_impl->buffer_exhausted_policy = dejaview::BufferExhaustedPolicy::kDrop;
      return true;
    case DEJAVIEW_DS_BUFFER_EXHAUSTED_POLICY_STALL_AND_ABORT:
      ds_impl->buffer_exhausted_policy =
          dejaview::BufferExhaustedPolicy::kStall;
      return true;
  }
  return false;
}

bool DejaViewDsImplRegister(struct DejaViewDsImpl* ds_impl,
                            DEJAVIEW_ATOMIC(bool) * *enabled_ptr,
                            const void* descriptor,
                            size_t descriptor_size) {
  std::unique_ptr<DejaViewDsImpl> data_source_type(ds_impl);

  dejaview::DataSourceDescriptor dsd;
  dsd.ParseFromArray(descriptor, descriptor_size);

  auto factory = [ds_impl]() {
    return std::unique_ptr<dejaview::DataSourceBase>(
        new ShlibDataSource(ds_impl));
  };

  DataSourceType::CreateCustomTlsFn create_custom_tls_fn = nullptr;
  DataSourceType::CreateIncrementalStateFn create_incremental_state_fn =
      nullptr;
  void* cb_ctx = nullptr;
  if (data_source_type->on_create_incr_cb &&
      data_source_type->on_delete_incr_cb) {
    create_incremental_state_fn = CreateShlibIncrementalState;
    cb_ctx = data_source_type.get();
  }
  if (data_source_type->on_create_tls_cb &&
      data_source_type->on_delete_tls_cb) {
    create_custom_tls_fn = CreateShlibTls;
    cb_ctx = data_source_type.get();
  }

  dejaview::internal::DataSourceParams params;
  params.supports_multiple_instances = true;
  params.requires_callbacks_under_lock = false;
  bool success = data_source_type->cpp_type.Register(
      dsd, factory, params, data_source_type->buffer_exhausted_policy,
      data_source_type->on_flush_cb == nullptr, create_custom_tls_fn,
      create_incremental_state_fn, cb_ctx);
  if (!success) {
    return false;
  }
  *enabled_ptr = &data_source_type->enabled;
  dejaview::base::ignore_result(data_source_type.release());
  return true;
}

void DejaViewDsImplUpdateDescriptor(struct DejaViewDsImpl* ds_impl,
                                    const void* descriptor,
                                    size_t descriptor_size) {
  dejaview::DataSourceDescriptor dsd;
  dsd.ParseFromArray(descriptor, descriptor_size);

  ds_impl->cpp_type.UpdateDescriptor(dsd);
}

DejaViewDsAsyncStopper* DejaViewDsOnStopArgsPostpone(
    DejaViewDsOnStopArgs* args) {
  auto* cb = new std::function<void()>();
  *cb = reinterpret_cast<const ShlibDataSource::StopArgs*>(args)
            ->HandleStopAsynchronously();
  return reinterpret_cast<DejaViewDsAsyncStopper*>(cb);
}

void DejaViewDsStopDone(DejaViewDsAsyncStopper* stopper) {
  auto* cb = reinterpret_cast<std::function<void()>*>(stopper);
  (*cb)();
  delete cb;
}

DejaViewDsAsyncFlusher* DejaViewDsOnFlushArgsPostpone(
    DejaViewDsOnFlushArgs* args) {
  auto* cb = new std::function<void()>();
  *cb = reinterpret_cast<const ShlibDataSource::FlushArgs*>(args)
            ->HandleFlushAsynchronously();
  return reinterpret_cast<DejaViewDsAsyncFlusher*>(cb);
}

void DejaViewDsFlushDone(DejaViewDsAsyncFlusher* stopper) {
  auto* cb = reinterpret_cast<std::function<void()>*>(stopper);
  (*cb)();
  delete cb;
}

void* DejaViewDsImplGetInstanceLocked(struct DejaViewDsImpl* ds_impl,
                                      DejaViewDsInstanceIndex idx) {
  auto* internal_state = ds_impl->cpp_type.static_state()->TryGet(idx);
  if (!internal_state) {
    return nullptr;
  }
  std::unique_lock<std::recursive_mutex> lock(internal_state->lock);
  auto* data_source =
      static_cast<ShlibDataSource*>(internal_state->data_source.get());
  if (&data_source->type() != ds_impl) {
    // The data source instance has been destroyed and recreated as a different
    // type while we where tracing.
    return nullptr;
  }
  void* inst_ctx = data_source->inst_ctx();
  if (inst_ctx != nullptr) {
    lock.release();
  }
  return inst_ctx;
}

void DejaViewDsImplReleaseInstanceLocked(struct DejaViewDsImpl* ds_impl,
                                         DejaViewDsInstanceIndex idx) {
  // The `valid_instances` bitmap might have changed since the lock has been
  // taken, but the instance must still be alive (we were holding the lock on
  // it).
  auto* internal_state = ds_impl->cpp_type.static_state()->GetUnsafe(idx);
  internal_state->lock.unlock();
}

void* DejaViewDsImplGetCustomTls(struct DejaViewDsImpl*,
                                 struct DejaViewDsTracerImpl* tracer,
                                 DejaViewDsInstanceIndex) {
  auto* tls_inst =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(tracer);

  DEJAVIEW_DCHECK(tls_inst->data_source_custom_tls);
  return tls_inst->data_source_custom_tls.get();
}

void* DejaViewDsImplGetIncrementalState(struct DejaViewDsImpl* ds_impl,
                                        struct DejaViewDsTracerImpl* tracer,
                                        DejaViewDsInstanceIndex idx) {
  auto* tls_inst =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(tracer);

  return ds_impl->cpp_type.GetIncrementalState(tls_inst, idx);
}

struct DejaViewDsImplTracerIterator DejaViewDsImplTraceIterateBegin(
    struct DejaViewDsImpl* ds_impl) {
  DataSourceThreadLocalState** tls =
      &g_tls_cache[ds_impl->cpp_type.static_state()->index];

  struct DejaViewDsImplTracerIterator ret = {0, nullptr, nullptr};
  uint32_t cached_instances =
      ds_impl->cpp_type.valid_instances()->load(std::memory_order_relaxed);
  if (!cached_instances) {
    return ret;
  }
  bool res =
      ds_impl->cpp_type.TracePrologue<DataSourceTraits, TracePointTraits>(
          tls, &cached_instances, &ds_impl->cpp_type);
  if (!res) {
    return ret;
  }
  DataSourceType::InstancesIterator it =
      ds_impl->cpp_type.BeginIteration<TracePointTraits>(cached_instances, *tls,
                                                         &ds_impl->cpp_type);
  ret.inst_id = it.i;
  (*tls)->root_tls->cached_instances = it.cached_instances;
  ret.tracer = reinterpret_cast<struct DejaViewDsTracerImpl*>(it.instance);
  if (!ret.tracer) {
    ds_impl->cpp_type.TraceEpilogue(*tls);
  }

  ret.tls = reinterpret_cast<struct DejaViewDsTlsImpl*>(*tls);
  return ret;
}

void DejaViewDsImplTraceIterateNext(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsImplTracerIterator* iterator) {
  auto* tls = reinterpret_cast<DataSourceThreadLocalState*>(iterator->tls);

  DataSourceType::InstancesIterator it;
  it.i = iterator->inst_id;
  it.cached_instances = tls->root_tls->cached_instances;
  it.instance =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(iterator->tracer);

  ds_impl->cpp_type.NextIteration<TracePointTraits>(&it, tls,
                                                    &ds_impl->cpp_type);

  iterator->inst_id = it.i;
  tls->root_tls->cached_instances = it.cached_instances;
  iterator->tracer =
      reinterpret_cast<struct DejaViewDsTracerImpl*>(it.instance);

  if (!iterator->tracer) {
    ds_impl->cpp_type.TraceEpilogue(tls);
  }
}

void DejaViewDsImplTraceIterateBreak(
    struct DejaViewDsImpl* ds_impl,
    struct DejaViewDsImplTracerIterator* iterator) {
  auto* tls = reinterpret_cast<DataSourceThreadLocalState*>(iterator->tls);

  ds_impl->cpp_type.TraceEpilogue(tls);
}

struct DejaViewStreamWriter DejaViewDsTracerImplPacketBegin(
    struct DejaViewDsTracerImpl* tracer) {
  auto* tls_inst =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(tracer);

  auto message_handle = tls_inst->trace_writer->NewTracePacket();
  struct DejaViewStreamWriter ret;
  protozero::ScatteredStreamWriter* sw = message_handle.TakeStreamWriter();
  ret.impl = reinterpret_cast<DejaViewStreamWriterImpl*>(sw);
  dejaview::UpdateStreamWriter(*sw, &ret);
  return ret;
}

void DejaViewDsTracerImplPacketEnd(struct DejaViewDsTracerImpl* tracer,
                                   struct DejaViewStreamWriter* w) {
  auto* tls_inst =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(tracer);
  auto* sw = reinterpret_cast<protozero::ScatteredStreamWriter*>(w->impl);

  sw->set_write_ptr(w->write_ptr);
  tls_inst->trace_writer->FinishTracePacket();
}

void DejaViewDsTracerImplFlush(struct DejaViewDsTracerImpl* tracer,
                               DejaViewDsTracerOnFlushCb cb,
                               void* user_arg) {
  auto* tls_inst =
      reinterpret_cast<DataSourceInstanceThreadLocalState*>(tracer);

  std::function<void()> fn;
  if (cb != nullptr) {
    fn = [user_arg, cb]() { cb(user_arg); };
  }
  tls_inst->trace_writer->Flush(fn);
}
