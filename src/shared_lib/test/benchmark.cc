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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>

#include <benchmark/benchmark.h>

#include "dejaview/public/abi/atomic.h"
#include "dejaview/public/data_source.h"
#include "dejaview/public/pb_utils.h"
#include "dejaview/public/producer.h"
#include "dejaview/public/protos/trace/test_event.pzc.h"
#include "dejaview/public/protos/trace/trace.pzc.h"
#include "dejaview/public/protos/trace/trace_packet.pzc.h"
#include "dejaview/public/protos/trace/track_event/debug_annotation.pzc.h"
#include "dejaview/public/protos/trace/track_event/track_event.pzc.h"
#include "dejaview/public/te_category_macros.h"
#include "dejaview/public/te_macros.h"
#include "dejaview/public/track_event.h"

#include "src/shared_lib/test/utils.h"

static struct DejaViewDs custom = DEJAVIEW_DS_INIT();

#define BENCHMARK_CATEGORIES(C) C(benchmark_cat, "benchmark", "")

DEJAVIEW_TE_CATEGORIES_DEFINE(BENCHMARK_CATEGORIES)

namespace {

using ::dejaview::shlib::test_utils::FieldView;
using ::dejaview::shlib::test_utils::IdFieldView;
using ::dejaview::shlib::test_utils::TracingSession;

constexpr char kDataSourceName[] = "com.example.custom_data_source";

bool Initialize() {
  struct DejaViewProducerInitArgs args = DEJAVIEW_PRODUCER_INIT_ARGS_INIT();
  args.backends = DEJAVIEW_BACKEND_IN_PROCESS;
  DejaViewProducerInit(args);
  DejaViewDsRegister(&custom, kDataSourceName, DejaViewDsParamsDefault());
  DejaViewTeInit();
  DEJAVIEW_TE_REGISTER_CATEGORIES(BENCHMARK_CATEGORIES);
  return true;
}

void EnsureInitialized() {
  static bool initialized = Initialize();
  (void)initialized;
}

size_t DecodePacketSizes(const std::vector<uint8_t>& data) {
  for (struct DejaViewPbDecoderField field :
       IdFieldView(data, dejaview_protos_Trace_packet_field_number)) {
    if (field.status != DEJAVIEW_PB_DECODER_OK ||
        field.wire_type != DEJAVIEW_PB_WIRE_TYPE_DELIMITED) {
      abort();
    }
    IdFieldView for_testing_fields(
        field, dejaview_protos_TracePacket_for_testing_field_number);
    if (!for_testing_fields.ok()) {
      abort();
    }
    if (for_testing_fields.size() == 0) {
      continue;
    }
    if (for_testing_fields.size() > 1 || for_testing_fields.front().wire_type !=
                                             DEJAVIEW_PB_WIRE_TYPE_DELIMITED) {
      abort();
    }
    return field.value.delimited.len;
  }

  return 0;
}

void BM_Shlib_DataSource_Disabled(benchmark::State& state) {
  EnsureInitialized();
  for (auto _ : state) {
    DEJAVIEW_DS_TRACE(custom, ctx) {}
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_DataSource_DifferentPacketSize(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session =
      TracingSession::Builder().set_data_source_name(kDataSourceName).Build();

  // This controls the number of times a field is added in the trace packet.
  // It controls the size of the trace packet. The PacketSize counter reports
  // the exact number.
  const size_t kNumFields = static_cast<size_t>(state.range(0));

  for (auto _ : state) {
    DEJAVIEW_DS_TRACE(custom, ctx) {
      struct DejaViewDsRootTracePacket trace_packet;
      DejaViewDsTracerPacketBegin(&ctx, &trace_packet);

      {
        struct dejaview_protos_TestEvent for_testing;
        dejaview_protos_TracePacket_begin_for_testing(&trace_packet.msg,
                                                      &for_testing);
        {
          struct dejaview_protos_TestEvent_TestPayload payload;
          dejaview_protos_TestEvent_begin_payload(&for_testing, &payload);
          for (size_t i = 0; i < kNumFields; i++) {
            dejaview_protos_TestEvent_TestPayload_set_cstr_str(&payload,
                                                               "ABCDEFGH");
          }
          dejaview_protos_TestEvent_end_payload(&for_testing, &payload);
        }
        dejaview_protos_TracePacket_end_for_testing(&trace_packet.msg,
                                                    &for_testing);
      }
      DejaViewDsTracerPacketEnd(&ctx, &trace_packet);
    }
    benchmark::ClobberMemory();
  }

  tracing_session.StopBlocking();
  std::vector<uint8_t> data = tracing_session.ReadBlocking();

  // Just compute the PacketSize counter.
  state.counters["PacketSize"] = static_cast<double>(DecodePacketSizes(data));
}

void BM_Shlib_TeDisabled(benchmark::State& state) {
  EnsureInitialized();
  while (state.KeepRunning()) {
    DEJAVIEW_TE(benchmark_cat, DEJAVIEW_TE_SLICE_BEGIN("DisabledEvent"));
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeBasic(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    DEJAVIEW_TE(benchmark_cat, DEJAVIEW_TE_SLICE_BEGIN("Event"));
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeBasicNoIntern(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    DEJAVIEW_TE(benchmark_cat, DEJAVIEW_TE_SLICE_BEGIN("Event"),
                DEJAVIEW_TE_NO_INTERN());
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeDebugAnnotations(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    DEJAVIEW_TE(benchmark_cat, DEJAVIEW_TE_SLICE_BEGIN("Event"),
                DEJAVIEW_TE_ARG_UINT64("value", 42));
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeCustomProto(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    DEJAVIEW_TE(
        benchmark_cat, DEJAVIEW_TE_SLICE_BEGIN("Event"),
        DEJAVIEW_TE_PROTO_FIELDS(DEJAVIEW_TE_PROTO_FIELD_NESTED(
            dejaview_protos_TrackEvent_debug_annotations_field_number,
            DEJAVIEW_TE_PROTO_FIELD_CSTR(
                dejaview_protos_DebugAnnotation_name_field_number, "value"),
            DEJAVIEW_TE_PROTO_FIELD_VARINT(
                dejaview_protos_DebugAnnotation_uint_value_field_number, 42))));
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeLlBasic(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    if (DEJAVIEW_UNLIKELY(DEJAVIEW_ATOMIC_LOAD_EXPLICIT(
            benchmark_cat.enabled, DEJAVIEW_MEMORY_ORDER_RELAXED))) {
      struct DejaViewTeTimestamp timestamp = DejaViewTeGetTimestamp();
      int32_t type = DEJAVIEW_TE_TYPE_SLICE_BEGIN;
      const char* name = "Event";
      for (struct DejaViewTeLlIterator ctx =
               DejaViewTeLlBeginSlowPath(&benchmark_cat, timestamp);
           ctx.impl.ds.tracer != nullptr;
           DejaViewTeLlNext(&benchmark_cat, timestamp, &ctx)) {
        uint64_t name_iid;
        {
          struct DejaViewDsRootTracePacket trace_packet;
          DejaViewTeLlPacketBegin(&ctx, &trace_packet);
          DejaViewTeLlWriteTimestamp(&trace_packet.msg, &timestamp);
          dejaview_protos_TracePacket_set_sequence_flags(
              &trace_packet.msg,
              dejaview_protos_TracePacket_SEQ_NEEDS_INCREMENTAL_STATE);
          {
            struct DejaViewTeLlInternContext intern_ctx;
            DejaViewTeLlInternContextInit(&intern_ctx, ctx.impl.incr,
                                          &trace_packet.msg);
            DejaViewTeLlInternRegisteredCat(&intern_ctx, &benchmark_cat);
            name_iid = DejaViewTeLlInternEventName(&intern_ctx, name);
            DejaViewTeLlInternContextDestroy(&intern_ctx);
          }
          {
            struct dejaview_protos_TrackEvent te_msg;
            dejaview_protos_TracePacket_begin_track_event(&trace_packet.msg,
                                                          &te_msg);
            dejaview_protos_TrackEvent_set_type(
                &te_msg,
                static_cast<enum dejaview_protos_TrackEvent_Type>(type));
            DejaViewTeLlWriteRegisteredCat(&te_msg, &benchmark_cat);
            DejaViewTeLlWriteInternedEventName(&te_msg, name_iid);
            dejaview_protos_TracePacket_end_track_event(&trace_packet.msg,
                                                        &te_msg);
          }
          DejaViewTeLlPacketEnd(&ctx, &trace_packet);
        }
      }
    }

    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeLlBasicNoIntern(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    if (DEJAVIEW_UNLIKELY(DEJAVIEW_ATOMIC_LOAD_EXPLICIT(
            benchmark_cat.enabled, DEJAVIEW_MEMORY_ORDER_RELAXED))) {
      struct DejaViewTeTimestamp timestamp = DejaViewTeGetTimestamp();
      int32_t type = DEJAVIEW_TE_TYPE_SLICE_BEGIN;
      const char* name = "Event";
      for (struct DejaViewTeLlIterator ctx =
               DejaViewTeLlBeginSlowPath(&benchmark_cat, timestamp);
           ctx.impl.ds.tracer != nullptr;
           DejaViewTeLlNext(&benchmark_cat, timestamp, &ctx)) {
        {
          struct DejaViewDsRootTracePacket trace_packet;
          DejaViewTeLlPacketBegin(&ctx, &trace_packet);
          DejaViewTeLlWriteTimestamp(&trace_packet.msg, &timestamp);
          dejaview_protos_TracePacket_set_sequence_flags(
              &trace_packet.msg,
              dejaview_protos_TracePacket_SEQ_NEEDS_INCREMENTAL_STATE);
          {
            struct DejaViewTeLlInternContext intern_ctx;
            DejaViewTeLlInternContextInit(&intern_ctx, ctx.impl.incr,
                                          &trace_packet.msg);
            DejaViewTeLlInternRegisteredCat(&intern_ctx, &benchmark_cat);
            DejaViewTeLlInternContextDestroy(&intern_ctx);
          }
          {
            struct dejaview_protos_TrackEvent te_msg;
            dejaview_protos_TracePacket_begin_track_event(&trace_packet.msg,
                                                          &te_msg);
            dejaview_protos_TrackEvent_set_type(
                &te_msg,
                static_cast<enum dejaview_protos_TrackEvent_Type>(type));
            DejaViewTeLlWriteRegisteredCat(&te_msg, &benchmark_cat);
            DejaViewTeLlWriteEventName(&te_msg, name);
            dejaview_protos_TracePacket_end_track_event(&trace_packet.msg,
                                                        &te_msg);
          }
          DejaViewTeLlPacketEnd(&ctx, &trace_packet);
        }
      }
    }
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeLlDebugAnnotations(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    if (DEJAVIEW_UNLIKELY(DEJAVIEW_ATOMIC_LOAD_EXPLICIT(
            benchmark_cat.enabled, DEJAVIEW_MEMORY_ORDER_RELAXED))) {
      struct DejaViewTeTimestamp timestamp = DejaViewTeGetTimestamp();
      int32_t type = DEJAVIEW_TE_TYPE_SLICE_BEGIN;
      const char* name = "Event";
      for (struct DejaViewTeLlIterator ctx =
               DejaViewTeLlBeginSlowPath(&benchmark_cat, timestamp);
           ctx.impl.ds.tracer != nullptr;
           DejaViewTeLlNext(&benchmark_cat, timestamp, &ctx)) {
        uint64_t name_iid;
        uint64_t dbg_arg_iid;
        {
          struct DejaViewDsRootTracePacket trace_packet;
          DejaViewTeLlPacketBegin(&ctx, &trace_packet);
          DejaViewTeLlWriteTimestamp(&trace_packet.msg, &timestamp);
          dejaview_protos_TracePacket_set_sequence_flags(
              &trace_packet.msg,
              dejaview_protos_TracePacket_SEQ_NEEDS_INCREMENTAL_STATE);
          {
            struct DejaViewTeLlInternContext intern_ctx;
            DejaViewTeLlInternContextInit(&intern_ctx, ctx.impl.incr,
                                          &trace_packet.msg);
            DejaViewTeLlInternRegisteredCat(&intern_ctx, &benchmark_cat);
            name_iid = DejaViewTeLlInternEventName(&intern_ctx, name);
            dbg_arg_iid = DejaViewTeLlInternDbgArgName(&intern_ctx, "value");
            DejaViewTeLlInternContextDestroy(&intern_ctx);
          }
          {
            struct dejaview_protos_TrackEvent te_msg;
            dejaview_protos_TracePacket_begin_track_event(&trace_packet.msg,
                                                          &te_msg);
            dejaview_protos_TrackEvent_set_type(
                &te_msg,
                static_cast<enum dejaview_protos_TrackEvent_Type>(type));
            DejaViewTeLlWriteRegisteredCat(&te_msg, &benchmark_cat);
            DejaViewTeLlWriteInternedEventName(&te_msg, name_iid);
            {
              struct dejaview_protos_DebugAnnotation dbg_arg;
              dejaview_protos_TrackEvent_begin_debug_annotations(&te_msg,
                                                                 &dbg_arg);
              dejaview_protos_DebugAnnotation_set_name_iid(&dbg_arg,
                                                           dbg_arg_iid);
              dejaview_protos_DebugAnnotation_set_uint_value(&dbg_arg, 42);
              dejaview_protos_TrackEvent_end_debug_annotations(&te_msg,
                                                               &dbg_arg);
            }
            dejaview_protos_TracePacket_end_track_event(&trace_packet.msg,
                                                        &te_msg);
          }
          DejaViewTeLlPacketEnd(&ctx, &trace_packet);
        }
      }
    }
    benchmark::ClobberMemory();
  }
}

void BM_Shlib_TeLlCustomProto(benchmark::State& state) {
  EnsureInitialized();
  TracingSession tracing_session = TracingSession::Builder()
                                       .set_data_source_name("track_event")
                                       .add_enabled_category("*")
                                       .Build();

  while (state.KeepRunning()) {
    if (DEJAVIEW_UNLIKELY(DEJAVIEW_ATOMIC_LOAD_EXPLICIT(
            benchmark_cat.enabled, DEJAVIEW_MEMORY_ORDER_RELAXED))) {
      struct DejaViewTeTimestamp timestamp = DejaViewTeGetTimestamp();
      int32_t type = DEJAVIEW_TE_TYPE_SLICE_BEGIN;
      const char* name = "Event";
      for (struct DejaViewTeLlIterator ctx =
               DejaViewTeLlBeginSlowPath(&benchmark_cat, timestamp);
           ctx.impl.ds.tracer != nullptr;
           DejaViewTeLlNext(&benchmark_cat, timestamp, &ctx)) {
        uint64_t name_iid;
        {
          struct DejaViewDsRootTracePacket trace_packet;
          DejaViewTeLlPacketBegin(&ctx, &trace_packet);
          DejaViewTeLlWriteTimestamp(&trace_packet.msg, &timestamp);
          dejaview_protos_TracePacket_set_sequence_flags(
              &trace_packet.msg,
              dejaview_protos_TracePacket_SEQ_NEEDS_INCREMENTAL_STATE);
          {
            struct DejaViewTeLlInternContext intern_ctx;
            DejaViewTeLlInternContextInit(&intern_ctx, ctx.impl.incr,
                                          &trace_packet.msg);
            DejaViewTeLlInternRegisteredCat(&intern_ctx, &benchmark_cat);
            name_iid = DejaViewTeLlInternEventName(&intern_ctx, name);
            DejaViewTeLlInternContextDestroy(&intern_ctx);
          }
          {
            struct dejaview_protos_TrackEvent te_msg;
            dejaview_protos_TracePacket_begin_track_event(&trace_packet.msg,
                                                          &te_msg);
            dejaview_protos_TrackEvent_set_type(
                &te_msg,
                static_cast<enum dejaview_protos_TrackEvent_Type>(type));
            DejaViewTeLlWriteRegisteredCat(&te_msg, &benchmark_cat);
            DejaViewTeLlWriteInternedEventName(&te_msg, name_iid);
            {
              struct dejaview_protos_DebugAnnotation dbg_arg;
              dejaview_protos_TrackEvent_begin_debug_annotations(&te_msg,
                                                                 &dbg_arg);
              dejaview_protos_DebugAnnotation_set_cstr_name(&dbg_arg, "value");
              dejaview_protos_DebugAnnotation_set_uint_value(&dbg_arg, 42);
              dejaview_protos_TrackEvent_end_debug_annotations(&te_msg,
                                                               &dbg_arg);
            }
            dejaview_protos_TracePacket_end_track_event(&trace_packet.msg,
                                                        &te_msg);
          }
          DejaViewTeLlPacketEnd(&ctx, &trace_packet);
        }
      }
    }
    benchmark::ClobberMemory();
  }
}

}  // namespace

BENCHMARK(BM_Shlib_DataSource_Disabled);
BENCHMARK(BM_Shlib_DataSource_DifferentPacketSize)->Range(1, 1000);
BENCHMARK(BM_Shlib_TeDisabled);
BENCHMARK(BM_Shlib_TeBasic);
BENCHMARK(BM_Shlib_TeBasicNoIntern);
BENCHMARK(BM_Shlib_TeDebugAnnotations);
BENCHMARK(BM_Shlib_TeCustomProto);
BENCHMARK(BM_Shlib_TeLlBasic);
BENCHMARK(BM_Shlib_TeLlBasicNoIntern);
BENCHMARK(BM_Shlib_TeLlDebugAnnotations);
BENCHMARK(BM_Shlib_TeLlCustomProto);
