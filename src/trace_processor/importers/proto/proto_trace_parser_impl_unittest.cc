/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/proto_trace_parser_impl.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "dejaview/base/status.h"
#include "dejaview/ext/base/string_view.h"
#include "dejaview/protozero/scattered_heap_buffer.h"
#include "dejaview/trace_processor/basic_types.h"
#include "dejaview/trace_processor/trace_blob.h"
#include "dejaview/trace_processor/trace_blob_view.h"
#include "src/trace_processor/db/column/types.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/args_translation_table.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/cpu_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/flow_tracker.h"
#include "src/trace_processor/importers/common/global_args_tracker.h"
#include "src/trace_processor/importers/common/machine_tracker.h"
#include "src/trace_processor/importers/common/mapping_tracker.h"
#include "src/trace_processor/importers/common/metadata_tracker.h"
#include "src/trace_processor/importers/common/process_track_translation_table.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/slice_translation_table.h"
#include "src/trace_processor/importers/common/stack_profile_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/proto/additional_modules.h"
#include "src/trace_processor/importers/proto/default_modules.h"
#include "src/trace_processor/importers/proto/proto_trace_reader.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/metadata_tables_py.h"
#include "src/trace_processor/types/variadic.h"
#include "src/trace_processor/util/descriptors.h"
#include "test/gtest_and_gmock.h"

#include "protos/dejaview/common/builtin_clock.pbzero.h"
#include "protos/dejaview/common/sys_stats_counters.pbzero.h"
#include "protos/dejaview/config/trace_config.pbzero.h"
#include "protos/dejaview/trace/clock_snapshot.pbzero.h"
#include "protos/dejaview/trace/interned_data/interned_data.pbzero.h"
#include "protos/dejaview/trace/profiling/profile_common.pbzero.h"
#include "protos/dejaview/trace/profiling/profile_packet.pbzero.h"
#include "protos/dejaview/trace/ps/process_tree.pbzero.h"
#include "protos/dejaview/trace/sys_stats/sys_stats.pbzero.h"
#include "protos/dejaview/trace/trace.pbzero.h"
#include "protos/dejaview/trace/trace_packet.pbzero.h"
#include "protos/dejaview/trace/trace_uuid.pbzero.h"
#include "protos/dejaview/trace/track_event/counter_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/debug_annotation.pbzero.h"
#include "protos/dejaview/trace/track_event/log_message.pbzero.h"
#include "protos/dejaview/trace/track_event/process_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/source_location.pbzero.h"
#include "protos/dejaview/trace/track_event/task_execution.pbzero.h"
#include "protos/dejaview/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/track_descriptor.pbzero.h"
#include "protos/dejaview/trace/track_event/track_event.pbzero.h"

namespace dejaview::trace_processor {
namespace {

using ::std::make_pair;
using ::testing::_;
using ::testing::Args;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IgnoreResult;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NiceMock;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAreArray;

namespace {
MATCHER_P(DoubleEq, exp, "Double matcher that satisfies -Wfloat-equal") {
  // The IEEE standard says that any comparison operation involving
  // a NAN must return false.
  double d_exp = exp;
  double d_arg = arg;
  if (std::isnan(d_exp) || std::isnan(d_arg))
    return false;
  return fabs(d_arg - d_exp) < 1e-128;
}
}  // namespace

class MockEventTracker : public EventTracker {
 public:
  explicit MockEventTracker(TraceProcessorContext* context)
      : EventTracker(context) {}
  ~MockEventTracker() override = default;

  MOCK_METHOD(void,
              PushSchedSwitch,
              (uint32_t cpu,
               int64_t timestamp,
               uint32_t prev_pid,
               base::StringView prev_comm,
               int32_t prev_prio,
               int64_t prev_state,
               uint32_t next_pid,
               base::StringView next_comm,
               int32_t next_prio));

  MOCK_METHOD(std::optional<CounterId>,
              PushCounter,
              (int64_t timestamp, double value, TrackId track_id),
              (override));
};

class MockProcessTracker : public ProcessTracker {
 public:
  explicit MockProcessTracker(TraceProcessorContext* context)
      : ProcessTracker(context) {}

  MOCK_METHOD(UniquePid,
              SetProcessMetadata,
              (uint32_t pid,
               std::optional<uint32_t> ppid,
               base::StringView process_name,
               base::StringView cmdline),
              (override));

  MOCK_METHOD(UniqueTid,
              UpdateThreadName,
              (uint32_t tid,
               StringId thread_name_id,
               ThreadNamePriority priority),
              (override));
  MOCK_METHOD(void,
              UpdateThreadNameByUtid,
              (UniqueTid utid,
               StringId thread_name_id,
               ThreadNamePriority priority),
              (override));
  MOCK_METHOD(UniqueTid,
              UpdateThread,
              (uint32_t tid, uint32_t tgid),
              (override));

  MOCK_METHOD(UniquePid, GetOrCreateProcess, (uint32_t pid), (override));
  MOCK_METHOD(void,
              SetProcessNameIfUnset,
              (UniquePid upid, StringId process_name_id),
              (override));
};

class MockBoundInserter : public ArgsTracker::BoundInserter {
 public:
  MockBoundInserter()
      : ArgsTracker::BoundInserter(&tracker_, nullptr, 0u), tracker_(nullptr) {
    ON_CALL(*this, AddArg(_, _, _, _)).WillByDefault(ReturnRef(*this));
  }

  MOCK_METHOD(ArgsTracker::BoundInserter&,
              AddArg,
              (StringId flat_key,
               StringId key,
               Variadic v,
               ArgsTracker::UpdatePolicy update_policy),
              (override));

 private:
  ArgsTracker tracker_;
};

class MockSliceTracker : public SliceTracker {
 public:
  explicit MockSliceTracker(TraceProcessorContext* context)
      : SliceTracker(context) {}

  MOCK_METHOD(std::optional<SliceId>,
              Begin,
              (int64_t timestamp,
               TrackId track_id,
               StringId cat,
               StringId name,
               SetArgsCallback args_callback),
              (override));
  MOCK_METHOD(std::optional<SliceId>,
              End,
              (int64_t timestamp,
               TrackId track_id,
               StringId cat,
               StringId name,
               SetArgsCallback args_callback),
              (override));
  MOCK_METHOD(std::optional<SliceId>,
              Scoped,
              (int64_t timestamp,
               TrackId track_id,
               StringId cat,
               StringId name,
               int64_t duration,
               SetArgsCallback args_callback),
              (override));
  MOCK_METHOD(std::optional<SliceId>,
              StartSlice,
              (int64_t timestamp,
               TrackId track_id,
               SetArgsCallback args_callback,
               std::function<SliceId()> inserter),
              (override));
};

class ProtoTraceParserTest : public ::testing::Test {
 public:
  ProtoTraceParserTest() {
    storage_ = new TraceStorage();
    context_.storage.reset(storage_);
    context_.track_tracker = std::make_unique<TrackTracker>(&context_);
    context_.global_args_tracker =
        std::make_unique<GlobalArgsTracker>(context_.storage.get());
    context_.mapping_tracker.reset(new MappingTracker(&context_));
    context_.stack_profile_tracker =
        std::make_unique<StackProfileTracker>(&context_);
    context_.args_tracker = std::make_unique<ArgsTracker>(&context_);
    context_.args_translation_table.reset(new ArgsTranslationTable(storage_));
    context_.metadata_tracker.reset(
        new MetadataTracker(context_.storage.get()));
    context_.machine_tracker.reset(new MachineTracker(&context_, 0));
    context_.cpu_tracker.reset(new CpuTracker(&context_));
    event_ = new MockEventTracker(&context_);
    context_.event_tracker.reset(event_);
    process_ = new NiceMock<MockProcessTracker>(&context_);
    context_.process_tracker.reset(process_);
    context_.process_track_translation_table.reset(
        new ProcessTrackTranslationTable(storage_));
    slice_ = new NiceMock<MockSliceTracker>(&context_);
    context_.slice_tracker.reset(slice_);
    context_.slice_translation_table =
        std::make_unique<SliceTranslationTable>(storage_);
    clock_ = new ClockTracker(&context_);
    context_.clock_tracker.reset(clock_);
    context_.flow_tracker = std::make_unique<FlowTracker>(&context_);
    context_.proto_trace_parser =
        std::make_unique<ProtoTraceParserImpl>(&context_);
    context_.sorter = std::make_shared<TraceSorter>(
        &context_, TraceSorter::SortingMode::kFullSort);
    context_.descriptor_pool_ = std::make_unique<DescriptorPool>();

    RegisterDefaultModules(&context_);
    RegisterAdditionalModules(&context_);
  }

  void ResetTraceBuffers() { trace_.Reset(); }

  void SetUp() override { ResetTraceBuffers(); }

  base::Status Tokenize() {
    trace_->Finalize();
    std::vector<uint8_t> trace_bytes = trace_.SerializeAsArray();
    std::unique_ptr<uint8_t[]> raw_trace(new uint8_t[trace_bytes.size()]);
    memcpy(raw_trace.get(), trace_bytes.data(), trace_bytes.size());
    context_.chunk_readers.push_back(
        std::make_unique<ProtoTraceReader>(&context_));
    auto status = context_.chunk_readers.back()->Parse(TraceBlobView(
        TraceBlob::TakeOwnership(std::move(raw_trace), trace_bytes.size())));

    ResetTraceBuffers();
    return status;
  }

  bool HasArg(ArgSetId set_id, StringId key_id, Variadic value) {
    const auto& args = storage_->arg_table();
    Query q;
    q.constraints = {args.arg_set_id().eq(set_id)};

    bool found = false;
    for (auto it = args.FilterToIterator(q); it; ++it) {
      if (it.key() == key_id) {
        EXPECT_EQ(it.flat_key(), key_id);
        if (storage_->GetArgValue(it.row_number().row_number()) == value) {
          found = true;
          break;
        }
      }
    }
    return found;
  }

 protected:
  protozero::HeapBuffered<protos::pbzero::Trace> trace_;
  TraceProcessorContext context_;
  MockEventTracker* event_;
  MockProcessTracker* process_;
  MockSliceTracker* slice_;
  ClockTracker* clock_;
  TraceStorage* storage_;
};

TEST_F(ProtoTraceParserTest, LoadCpuFreqKHz) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  bundle->add_cpufreq_khz(2650000u);
  bundle->add_cpufreq_khz(3698200u);

  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts), DoubleEq(2650000u),
                                   TrackId{0u}));
  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts), DoubleEq(3698200u),
                                   TrackId{1u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 2u);
  EXPECT_EQ(context_.storage->cpu_counter_track_table().row_count(), 2u);

  auto row = context_.storage->cpu_counter_track_table().FindById(TrackId(0));
  EXPECT_EQ(context_.storage->GetString(row->name()), "cpufreq");
  EXPECT_EQ(row->ucpu().value, 0u);

  row = context_.storage->cpu_counter_track_table().FindById(TrackId(1));
  EXPECT_EQ(row->ucpu().value, 1u);
}

TEST_F(ProtoTraceParserTest, LoadCpuIdleStats) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* cpuidle_state = bundle->add_cpuidle_state();
  cpuidle_state->set_cpu_id(0);
  auto* cpuidle_state_entry = cpuidle_state->add_cpuidle_state_entry();
  cpuidle_state_entry->set_state("mock_state0");
  cpuidle_state_entry->set_duration_us(20000);
  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts),
                                   static_cast<double>(20000), TrackId{0u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 1u);
}

TEST_F(ProtoTraceParserTest, LoadGpuFreqStats) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  bundle->add_gpufreq_mhz(300);
  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts),
                                   static_cast<double>(300), TrackId{1u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 2u);
}

TEST_F(ProtoTraceParserTest, LoadMemInfo) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* meminfo = bundle->add_meminfo();
  meminfo->set_key(protos::pbzero::MEMINFO_MEM_TOTAL);
  uint32_t value = 10;
  meminfo->set_value(value);

  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts),
                                   DoubleEq(value * 1024.0), TrackId{1u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 2u);
}

TEST_F(ProtoTraceParserTest, LoadVmStats) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* meminfo = bundle->add_vmstat();
  meminfo->set_key(protos::pbzero::VMSTAT_COMPACT_SUCCESS);
  uint32_t value = 10;
  meminfo->set_value(value);

  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts), DoubleEq(value),
                                   TrackId{1u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 2u);
}

TEST_F(ProtoTraceParserTest, LoadThermal) {
  auto* packet = trace_->add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* thermal_zone = bundle->add_thermal_zone();
  thermal_zone->set_type("MOCKTYPE");
  uint64_t temp = 10000;
  thermal_zone->set_temp(temp);

  EXPECT_CALL(*event_,
              PushCounter(static_cast<int64_t>(ts),
                          DoubleEq(static_cast<double>(temp)), TrackId{1u}));
  Tokenize();
  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(context_.storage->track_table().row_count(), 2u);
}

TEST_F(ProtoTraceParserTest, LoadProcessPacket) {
  auto* tree = trace_->add_packet()->set_process_tree();
  auto* process = tree->add_processes();
  static const char kProcName1[] = "proc1";

  process->add_cmdline(kProcName1);
  process->set_pid(1);
  process->set_ppid(3);

  EXPECT_CALL(*process_,
              SetProcessMetadata(1, Eq(3u), base::StringView(kProcName1),
                                 base::StringView(kProcName1)));
  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, LoadProcessPacket_FirstCmdline) {
  auto* tree = trace_->add_packet()->set_process_tree();
  auto* process = tree->add_processes();
  static const char kProcName1[] = "proc1";
  static const char kProcName2[] = "proc2";

  process->add_cmdline(kProcName1);
  process->add_cmdline(kProcName2);
  process->set_pid(1);
  process->set_ppid(3);

  EXPECT_CALL(*process_,
              SetProcessMetadata(1, Eq(3u), base::StringView(kProcName1),
                                 base::StringView("proc1 proc2")));
  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, LoadThreadPacket) {
  auto* tree = trace_->add_packet()->set_process_tree();
  auto* thread = tree->add_threads();
  thread->set_tid(1);
  thread->set_tgid(2);

  EXPECT_CALL(*process_, UpdateThread(1, 2));
  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, ProcessNameFromProcessDescriptor) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(15);
    process_desc->set_process_name("OldProcessName");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(15);
    process_desc->set_process_name("NewProcessName");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(16);
    process_desc->set_process_name("DifferentProcessName");
  }

  EXPECT_CALL(*process_, GetOrCreateProcess(15))
      .WillRepeatedly(testing::Return(1u));
  EXPECT_CALL(*process_, GetOrCreateProcess(16)).WillOnce(testing::Return(2u));

  EXPECT_CALL(*process_, SetProcessNameIfUnset(
                             1u, storage_->InternString("OldProcessName")));
  // Packet with same thread, but different name should update the name.
  EXPECT_CALL(*process_, SetProcessNameIfUnset(
                             1u, storage_->InternString("NewProcessName")));
  EXPECT_CALL(*process_,
              SetProcessNameIfUnset(
                  2u, storage_->InternString("DifferentProcessName")));

  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, ThreadNameFromThreadDescriptor) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("OldThreadName");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("NewThreadName");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(11);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("DifferentThreadName");
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(testing::Return(1u));
  EXPECT_CALL(*process_, UpdateThread(11, 15)).WillOnce(testing::Return(2u));

  EXPECT_CALL(*process_, UpdateThreadNameByUtid(
                             1u, storage_->InternString("OldThreadName"),
                             ThreadNamePriority::kTrackDescriptor));
  // Packet with same thread, but different name should update the name.
  EXPECT_CALL(*process_, UpdateThreadNameByUtid(
                             1u, storage_->InternString("NewThreadName"),
                             ThreadNamePriority::kTrackDescriptor));
  EXPECT_CALL(*process_, UpdateThreadNameByUtid(
                             2u, storage_->InternString("DifferentThreadName"),
                             ThreadNamePriority::kTrackDescriptor));

  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutInternedData) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->set_thread_time_delta_us(5);  // absolute: 2010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->set_thread_time_absolute_us(2003);
    event->add_category_iids(2);
    event->add_category_iids(3);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('X');
    legacy_event->set_duration_us(23);         // absolute end: 1028.
    legacy_event->set_thread_duration_us(12);  // absolute end: 2015.
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  MockBoundInserter inserter;

  StringId unknown_cat = storage_->InternString("unknown(1)");

  constexpr TrackId track{0u};
  constexpr TrackId thread_time_track{1u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  // Only the begin thread time can be imported into the counter table.
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(2003000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, StartSlice(1005000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(0u))));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, StartSlice(1010000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(1u))));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, End(1020000, track, unknown_cat, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(SliceId(1u))));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->slice_table().row_count(), 2u);
  auto rr_0 = storage_->slice_table().FindById(SliceId(0u));
  EXPECT_TRUE(rr_0);
  EXPECT_EQ(rr_0->thread_ts(), 2003000);
  EXPECT_EQ(rr_0->thread_dur(), 12000);
  auto rr_1 = storage_->slice_table().FindById(SliceId(1u));
  EXPECT_TRUE(rr_1);
  EXPECT_EQ(rr_1->thread_ts(), 2005000);
  EXPECT_EQ(rr_1->thread_dur(), 5000);
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutInternedDataWithTypes) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->set_thread_time_delta_us(5);  // absolute: 2010.
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_END);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1015);
    event->set_thread_time_absolute_us(2007);
    event->add_category_iids(2);
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  MockBoundInserter inserter;

  StringId unknown_cat1 = storage_->InternString("unknown(1)");

  constexpr TrackId track{0u};
  constexpr TrackId thread_time_track{1u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, StartSlice(1010000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(0u))));
  EXPECT_CALL(*event_, PushCounter(1015000, testing::DoubleEq(2007000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, StartSlice(1015000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(1u))));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, End(1020000, track, unknown_cat1, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(SliceId(0u))));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->slice_table().row_count(), 2u);
  auto rr_0 = storage_->slice_table().FindById(SliceId(0u));
  EXPECT_TRUE(rr_0);
  EXPECT_EQ(rr_0->thread_ts(), 2005000);
  EXPECT_EQ(rr_0->thread_dur(), 5000);
  auto rr_1 = storage_->slice_table().FindById(SliceId(1u));
  EXPECT_TRUE(rr_1);
  EXPECT_EQ(rr_1->thread_ts(), 2007000);
  EXPECT_EQ(rr_1->thread_dur(), 0);
}

TEST_F(ProtoTraceParserTest, TrackEventWithInternedData) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_reference_thread_instruction_count(3000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1010.
    event->set_thread_time_delta_us(5);             // absolute: 2005.
    event->set_thread_instruction_count_delta(20);  // absolute: 3020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1040);
    event->set_thread_time_absolute_us(2030);
    event->set_thread_instruction_count_absolute(3100);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('I');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1050);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('i');
    legacy_event->set_instant_event_scope(
        protos::pbzero::TrackEvent::LegacyEvent::SCOPE_PROCESS);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1020.
    event->set_thread_time_delta_us(5);             // absolute: 2010.
    event->set_thread_instruction_count_delta(20);  // absolute: 3040.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->set_thread_time_absolute_us(2003);
    event->set_thread_instruction_count_absolute(3010);
    event->add_category_iids(2);
    event->add_category_iids(3);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(4);
    legacy_event->set_phase('X');
    legacy_event->set_duration_us(23);               // absolute end: 1028.
    legacy_event->set_thread_duration_us(12);        // absolute end: 2015.
    legacy_event->set_thread_instruction_delta(50);  // absolute end: 3060.
    legacy_event->set_bind_id(9999);
    legacy_event->set_flow_direction(
        protos::pbzero::TrackEvent::LegacyEvent::FLOW_OUT);

    auto* interned_data = packet->set_interned_data();
    auto* cat2 = interned_data->add_event_categories();
    cat2->set_iid(2);
    cat2->set_name("cat2");
    auto* cat3 = interned_data->add_event_categories();
    cat3->set_iid(3);
    cat3->set_name("cat3");
    auto* ev2 = interned_data->add_event_names();
    ev2->set_iid(4);
    ev2->set_name("ev2");
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(4);
    legacy_event->set_phase('t');
    legacy_event->set_unscoped_id(220);
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(4);
    legacy_event->set_phase('f');
    legacy_event->set_unscoped_id(330);
    legacy_event->set_bind_to_enclosing(false);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 2u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId thread_1_track{0u};
  constexpr TrackId thread_time_track{1u};
  constexpr TrackId thread_instruction_count_track{2u};
  constexpr TrackId process_2_track{3u};

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  // Only the begin timestamp counters can be imported into the counter table.
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(2003000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(3010),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, StartSlice(1005000, thread_1_track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(0u))));

  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(3020),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, StartSlice(1010000, thread_1_track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(1u))));

  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(3040),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, End(1020000, thread_1_track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(SliceId(1u))));

  EXPECT_CALL(*event_, PushCounter(1040000, testing::DoubleEq(2030000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1040000, testing::DoubleEq(3100),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, StartSlice(1040000, thread_1_track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(2u))));

  EXPECT_CALL(*slice_, Scoped(1050000, process_2_track, cat_1, ev_1, 0, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(SliceId(3u))));
  // Second slice should have a legacy_event.passthrough_utid arg.
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::UnsignedInteger(1u), _));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->slice_table().row_count(), 3u);
  auto rr_0 = storage_->slice_table().FindById(SliceId(0u));
  EXPECT_TRUE(rr_0);
  EXPECT_EQ(rr_0->thread_ts(), 2003000);
  EXPECT_EQ(rr_0->thread_dur(), 12000);
  EXPECT_EQ(rr_0->thread_instruction_count(), 3010);
  EXPECT_EQ(rr_0->thread_instruction_delta(), 50);
  auto rr_1 = storage_->slice_table().FindById(SliceId(1u));
  EXPECT_TRUE(rr_1);
  EXPECT_EQ(rr_1->thread_ts(), 2005000);
  EXPECT_EQ(rr_1->thread_dur(), 5000);
  EXPECT_EQ(rr_1->thread_instruction_count(), 3020);
  EXPECT_EQ(rr_1->thread_instruction_delta(), 20);
  auto rr_2 = storage_->slice_table().FindById(SliceId(2u));
  EXPECT_TRUE(rr_2);
  EXPECT_EQ(rr_2->thread_ts(), 2030000);
  EXPECT_EQ(rr_2->thread_dur(), 0);
  EXPECT_EQ(rr_2->thread_instruction_count(), 3100);
  EXPECT_EQ(rr_2->thread_instruction_delta(), 0);
}

TEST_F(ProtoTraceParserTest, TrackEventAsyncEvents) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_reference_thread_instruction_count(3000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1010.
    event->set_thread_time_delta_us(5);             // absolute: 2005.
    event->set_thread_instruction_count_delta(20);  // absolute: 3020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('b');
    legacy_event->set_global_id(10);
    legacy_event->set_use_async_tts(true);

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1020.
    event->set_thread_time_delta_us(5);             // absolute: 2010.
    event->set_thread_instruction_count_delta(20);  // absolute: 3040.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('e');
    legacy_event->set_global_id(10);
    legacy_event->set_use_async_tts(true);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1015);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_global_id(10);

    auto* interned_data = packet->set_interned_data();
    auto* ev2 = interned_data->add_event_names();
    ev2->set_iid(2);
    ev2->set_name("ev2");
  }
  {
    // Different category but same global_id -> separate track.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1018);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_global_id(15);

    auto* interned_data = packet->set_interned_data();
    auto* cat2 = interned_data->add_event_categories();
    cat2->set_iid(2);
    cat2->set_name("cat2");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1030);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_local_id(15);
    legacy_event->set_id_scope("scope1");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId cat_2 = storage_->InternString("cat2");
  StringId ev_2 = storage_->InternString("ev2");

  TrackId thread_time_track{2u};
  TrackId thread_instruction_count_track{3u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(3020),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, Begin(1010000, TrackId{1}, cat_1, ev_1, _))
      .WillOnce(Return(SliceId(0u)));
  EXPECT_CALL(*slice_, Scoped(1015000, TrackId{1}, cat_1, ev_2, 0, _));
  EXPECT_CALL(*slice_, Scoped(1018000, TrackId{4}, cat_2, ev_2, 0, _));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(3040),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, End(1020000, TrackId{1}, cat_1, ev_1, _))
      .WillOnce(Return(SliceId(SliceId(0u))));
  EXPECT_CALL(*slice_, Scoped(1030000, TrackId{5}, cat_2, ev_2, 0, _));

  context_.sorter->ExtractEventsForced();

  // First track is for the thread; second first async, third and fourth for
  // thread time and instruction count, others are the async event tracks.
  EXPECT_EQ(storage_->track_table().row_count(), 6u);
  EXPECT_EQ(storage_->track_table()[1].name(), ev_1);
  EXPECT_EQ(storage_->track_table()[4].name(), ev_2);
  EXPECT_EQ(storage_->track_table()[5].name(), ev_2);

  EXPECT_EQ(storage_->process_track_table().row_count(), 3u);
  EXPECT_EQ(storage_->process_track_table()[0].upid(), 1u);
  EXPECT_EQ(storage_->process_track_table()[1].upid(), 1u);
  EXPECT_EQ(storage_->process_track_table()[2].upid(), 1u);

  EXPECT_EQ(storage_->virtual_track_slices().slice_count(), 1u);
  EXPECT_EQ(storage_->virtual_track_slices().slice_ids()[0], SliceId(0u));
  EXPECT_EQ(storage_->virtual_track_slices().thread_timestamp_ns()[0], 2005000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_duration_ns()[0], 5000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_counts()[0],
            3020);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_deltas()[0],
            20);
}

TEST_F(ProtoTraceParserTest, TrackEventWithResortedCounterDescriptor) {
  // Descriptors with timestamps after the event below. They will be tokenized
  // in the order they appear here, but then resorted before parsing to appear
  // after the events below.
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    packet->set_timestamp(3000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(1);
    auto* thread_desc = track_desc->set_thread();
    thread_desc->set_pid(5);
    thread_desc->set_tid(1);
    thread_desc->set_thread_name("t1");
    // Default to track for "t1" and an extra counter for thread time.
    auto* track_event_defaults =
        packet->set_trace_packet_defaults()->set_track_event_defaults();
    track_event_defaults->set_track_uuid(1);
    // Thread-time counter track defined below.
    track_event_defaults->add_extra_counter_track_uuids(10);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(3000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(10);
    track_desc->set_parent_uuid(1);
    auto* counter = track_desc->set_counter();
    counter->set_type(
        protos::pbzero::CounterDescriptor::COUNTER_THREAD_TIME_NS);
    counter->set_unit_multiplier(1000);  // provided in us.
    counter->set_is_incremental(true);
  }
  {
    // Event with timestamps before the descriptors above. The thread time
    // counter values should still be imported as counter values and as args for
    // JSON export. Should appear on default track "t1" with
    // extra_counter_values for "c1".
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    packet->set_timestamp(1000);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    event->add_extra_counter_values(1000);  // absolute: 1000000.
  }
  {
    // End for "ev1".
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(1100);
    auto* event = packet->set_track_event();
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_END);
    event->add_extra_counter_values(10);  // absolute: 1010000.
  }

  EXPECT_CALL(*process_, UpdateThread(1, 5)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row t1(16);
  t1.upid = 1u;
  storage_->mutable_thread_table()->Insert(t1);

  Tokenize();

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*event_,
              PushCounter(1000, testing::DoubleEq(1000000), TrackId{1}));
  EXPECT_CALL(*slice_, StartSlice(1000, TrackId{0}, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()), Return(SliceId(0u))));

  EXPECT_CALL(*event_,
              PushCounter(1100, testing::DoubleEq(1010000), TrackId{1}));
  EXPECT_CALL(*slice_, End(1100, TrackId{0}, kNullStringId, kNullStringId, _))
      .WillOnce(Return(SliceId(0u)));

  EXPECT_CALL(*process_,
              UpdateThreadNameByUtid(1u, storage_->InternString("t1"),
                                     ThreadNamePriority::kTrackDescriptor));

  context_.sorter->ExtractEventsForced();

  // First track is thread time track, second is "t1".
  EXPECT_EQ(storage_->track_table().row_count(), 2u);
  EXPECT_EQ(storage_->thread_track_table().row_count(), 1u);
  EXPECT_EQ(storage_->thread_track_table()[0].utid(), 1u);

  // Counter values should also be imported into thread slices.
  EXPECT_EQ(storage_->slice_table().row_count(), 1u);
  auto rr_0 = storage_->slice_table().FindById(SliceId(0u));
  EXPECT_TRUE(rr_0);
  EXPECT_EQ(rr_0->thread_ts(), 1000000);
  EXPECT_EQ(rr_0->thread_dur(), 10000);
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutIncrementalStateReset) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    // Event should be discarded because delta timestamps require valid
    // incremental state + thread descriptor.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Event should be discarded because it specifies
    // SEQ_NEEDS_INCREMENTAL_STATE.
    auto* packet = trace_->add_packet();
    packet->set_timestamp(2000000);
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    auto* event = packet->set_track_event();
    event->add_categories("cat");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }
  {
    // Event should be accepted because it does not specify
    // SEQ_NEEDS_INCREMENTAL_STATE and uses absolute timestamps.
    auto* packet = trace_->add_packet();
    packet->set_timestamp(2100000);
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev2");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }

  Tokenize();

  StringId cat1 = storage_->InternString("cat1");
  StringId ev2 = storage_->InternString("ev2");

  EXPECT_CALL(*slice_, Scoped(2100000, TrackId{0}, cat1, ev2, 0, _))
      .WillOnce(Return(SliceId(0u)));
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutThreadDescriptor) {
  {
    // Event should be discarded because it specifies delta timestamps and no
    // thread descriptor was seen yet.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->set_thread_time_delta_us(5);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Events that specify SEQ_NEEDS_INCREMENTAL_STATE should be accepted even
    // if there's no valid thread descriptor.
    auto* packet = trace_->add_packet();
    packet->set_timestamp(2000000);
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }

  Tokenize();

  StringId cat1 = storage_->InternString("cat1");
  StringId ev1 = storage_->InternString("ev1");

  EXPECT_CALL(*slice_, Scoped(2000000, TrackId{0}, cat1, ev1, 0, _))
      .WillOnce(Return(SliceId(0u)));
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithDataLoss) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Event should be dropped because data loss occurred before.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_previous_packet_dropped(true);  // Data loss occurred.
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    // Event should be dropped because incremental state is invalid.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    // Event should be dropped because no new thread descriptor was seen yet.
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(2000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 2010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId unknown_cat = storage_->InternString("unknown(1)");
  constexpr TrackId track{0u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  EXPECT_CALL(*slice_, StartSlice(1010000, track, _, _));
  EXPECT_CALL(*slice_, End(2010000, track, unknown_cat, kNullStringId, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventMultipleSequences) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(17);
    thread_desc->set_reference_timestamp_us(995);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(2);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev2 = interned_data->add_event_names();
    ev2->set_iid(1);
    ev2->set_name("ev2");
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(2);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1015.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));
  EXPECT_CALL(*process_, UpdateThread(17, 15)).WillRepeatedly(Return(2u));

  tables::ThreadTable::Row t1(16);
  t1.upid = 1u;
  storage_->mutable_thread_table()->Insert(t1);

  tables::ThreadTable::Row t2(17);
  t2.upid = 1u;
  storage_->mutable_thread_table()->Insert(t2);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_2 = storage_->InternString("ev2");
  StringId ev_1 = storage_->InternString("ev1");

  constexpr TrackId thread_2_track{0u};
  constexpr TrackId thread_1_track{1u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*slice_, StartSlice(1005000, thread_2_track, _, _));
  EXPECT_CALL(*slice_, StartSlice(1010000, thread_1_track, _, _));
  EXPECT_CALL(*slice_, End(1015000, thread_2_track, cat_1, ev_2, _));
  EXPECT_CALL(*slice_, End(1020000, thread_1_track, cat_1, ev_1, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithTaskExecution) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010.
    event->add_category_iids(1);
    auto* task_execution = event->set_task_execution();
    task_execution->set_posted_from_iid(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
    auto* loc1 = interned_data->add_source_locations();
    loc1->set_iid(1);
    loc1->set_file_name("file1");
    loc1->set_function_name("func1");
    loc1->set_line_number(42);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId track{0u};

  StringId file_1 = storage_->InternString("file1");
  StringId func_1 = storage_->InternString("func1");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  EXPECT_CALL(*slice_, StartSlice(1010000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(0u))));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(file_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(func_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::UnsignedInteger(42), _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithLogMessage) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010.
    event->add_category_iids(1);

    auto* log_message = event->set_log_message();
    log_message->set_body_iid(1);
    log_message->set_source_location_iid(1);

    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('I');

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");

    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");

    auto* body = interned_data->add_log_message_body();
    body->set_iid(1);
    body->set_body("body1");

    auto* loc1 = interned_data->add_source_locations();
    loc1->set_iid(1);
    loc1->set_file_name("file1");
    loc1->set_function_name("func1");
    loc1->set_line_number(1);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId body_1 = storage_->InternString("body1");
  StringId file_1 = storage_->InternString("file1");
  StringId func_1 = storage_->InternString("func1");
  StringId source_location_id = storage_->InternString("file1:1");

  constexpr TrackId track{0};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  EXPECT_CALL(*slice_, StartSlice(1010000, track, _, _))
      .WillOnce(DoAll(IgnoreResult(InvokeArgument<3>()),
                      InvokeArgument<2>(&inserter), Return(SliceId(0u))));

  // Call with logMessageBody (body1 in this case).
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(body_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(file_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(func_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::Integer(1), _));

  context_.sorter->ExtractEventsForced();

  EXPECT_GT(context_.storage->android_log_table().row_count(), 0u);
  EXPECT_EQ(context_.storage->android_log_table()[0].ts(), 1010000);
  EXPECT_EQ(context_.storage->android_log_table()[0].msg(), body_1);
  EXPECT_EQ(context_.storage->android_log_table()[0].tag(), source_location_id);
}

TEST_F(ProtoTraceParserTest, TrackEventParseLegacyEventIntoRawTable) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);

    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    // Represents a phase that isn't parsed into regular trace processor tables.
    legacy_event->set_phase('?');
    legacy_event->set_duration_us(23);
    legacy_event->set_thread_duration_us(15);
    legacy_event->set_global_id(99u);
    legacy_event->set_id_scope("scope1");
    legacy_event->set_use_async_tts(true);

    auto* annotation1 = event->add_debug_annotations();
    annotation1->set_name_iid(1);
    annotation1->set_uint_value(10u);

    auto* interned_data = packet->set_interned_data();
    auto* cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto* ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
    auto* an1 = interned_data->add_debug_annotation_names();
    an1->set_iid(1);
    an1->set_name("an1");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));
  // Only the begin thread time can be imported into the counter table.
  EXPECT_CALL(*event_,
              PushCounter(1010000, testing::DoubleEq(2005000), TrackId{1}));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId scope_1 = storage_->InternString("scope1");
  StringId question = storage_->InternString("?");
  StringId debug_an_1 = storage_->InternString("debug.an1");

  context_.sorter->ExtractEventsForced();

  ::testing::Mock::VerifyAndClearExpectations(storage_);

  // Verify raw_table and args contents.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
  EXPECT_EQ(raw_table[0].ts(), 1010000);
  EXPECT_EQ(raw_table[0].name(),
            storage_->InternString("track_event.legacy_event"));
  auto ucpu = raw_table[0].ucpu();
  const auto& cpu_table = storage_->cpu_table();
  EXPECT_EQ(cpu_table[ucpu.value].cpu(), 0u);
  EXPECT_EQ(raw_table[0].utid(), 1u);
  EXPECT_EQ(raw_table[0].arg_set_id(), 2u);

  EXPECT_GE(storage_->arg_table().row_count(), 10u);

  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.category"),
                     Variadic::String(cat_1)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.name"),
                     Variadic::String(ev_1)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.phase"),
                     Variadic::String(question)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.duration_ns"),
                     Variadic::Integer(23000)));
  EXPECT_TRUE(HasArg(2u,
                     storage_->InternString("legacy_event.thread_timestamp_ns"),
                     Variadic::Integer(2005000)));
  EXPECT_TRUE(HasArg(2u,
                     storage_->InternString("legacy_event.thread_duration_ns"),
                     Variadic::Integer(15000)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.use_async_tts"),
                     Variadic::Boolean(true)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.global_id"),
                     Variadic::UnsignedInteger(99u)));
  EXPECT_TRUE(HasArg(2u, storage_->InternString("legacy_event.id_scope"),
                     Variadic::String(scope_1)));
  EXPECT_TRUE(HasArg(2u, debug_an_1, Variadic::UnsignedInteger(10u)));
}

TEST_F(ProtoTraceParserTest, TrackEventLegacyTimestampsWithClockSnapshot) {
  clock_->AddSnapshot({{protos::pbzero::BUILTIN_CLOCK_BOOTTIME, 0},
                       {protos::pbzero::BUILTIN_CLOCK_MONOTONIC, 1000000}});

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);  // MONOTONIC.
  }
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010 (mon), 10 (boot).
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId track{0u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  // Timestamp should be adjusted to trace time (BOOTTIME).
  EXPECT_CALL(*slice_, StartSlice(10000, track, _, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, ParseCPUProfileSamplesIntoTable) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);

    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1);
    thread_desc->set_reference_thread_time_us(2);

    auto* interned_data = packet->set_interned_data();

    auto* mapping = interned_data->add_mappings();
    mapping->set_iid(1);
    mapping->set_build_id(1);

    auto* build_id = interned_data->add_build_ids();
    build_id->set_iid(1);
    build_id->set_str("3BBCFBD372448A727265C3E7C4D954F91");

    auto* frame = interned_data->add_frames();
    frame->set_iid(1);
    frame->set_rel_pc(0x42);
    frame->set_mapping_id(1);

    auto* frame2 = interned_data->add_frames();
    frame2->set_iid(2);
    frame2->set_rel_pc(0x4242);
    frame2->set_mapping_id(1);

    auto* callstack = interned_data->add_callstacks();
    callstack->set_iid(1);
    callstack->add_frame_ids(1);

    auto* callstack2 = interned_data->add_callstacks();
    callstack2->set_iid(42);
    callstack2->add_frame_ids(2);
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);

    auto* samples = packet->set_streaming_profile_packet();
    samples->add_callstack_iid(42);
    samples->add_timestamp_delta_us(10);

    samples->add_callstack_iid(1);
    samples->add_timestamp_delta_us(15);
    samples->set_process_priority(20);
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* samples = packet->set_streaming_profile_packet();

    samples->add_callstack_iid(42);
    samples->add_timestamp_delta_us(42);
    samples->set_process_priority(30);
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  Tokenize();
  context_.sorter->ExtractEventsForced();

  // Verify cpu_profile_samples.
  const auto& samples = storage_->cpu_profile_stack_sample_table();
  EXPECT_EQ(samples.row_count(), 3u);

  EXPECT_EQ(samples[0].ts(), 11000);
  EXPECT_EQ(samples[0].callsite_id(), CallsiteId{0});
  EXPECT_EQ(samples[0].utid(), 1u);
  EXPECT_EQ(samples[0].process_priority(), 20);

  EXPECT_EQ(samples[1].ts(), 26000);
  EXPECT_EQ(samples[1].callsite_id(), CallsiteId{1});
  EXPECT_EQ(samples[1].utid(), 1u);
  EXPECT_EQ(samples[1].process_priority(), 20);

  EXPECT_EQ(samples[2].ts(), 68000);
  EXPECT_EQ(samples[2].callsite_id(), CallsiteId{0});
  EXPECT_EQ(samples[2].utid(), 1u);
  EXPECT_EQ(samples[2].process_priority(), 30);

  // Breakpad build_ids should not be modified/mangled.
  ASSERT_STREQ(
      context_.storage
          ->GetString(storage_->stack_profile_mapping_table()[0].build_id())
          .c_str(),
      "3BBCFBD372448A727265C3E7C4D954F91");
}

TEST_F(ProtoTraceParserTest, CPUProfileSamplesTimestampsAreClockMonotonic) {
  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(0);

    // 1000 us monotonic == 10000 us boottime.
    auto* clock_snapshot = packet->set_clock_snapshot();
    auto* clock_boot = clock_snapshot->add_clocks();
    clock_boot->set_clock_id(protos::pbzero::BUILTIN_CLOCK_BOOTTIME);
    clock_boot->set_timestamp(10000000);
    auto* clock_monotonic = clock_snapshot->add_clocks();
    clock_monotonic->set_clock_id(protos::pbzero::BUILTIN_CLOCK_MONOTONIC);
    clock_monotonic->set_timestamp(1000000);
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);

    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);

    auto* interned_data = packet->set_interned_data();

    auto* mapping = interned_data->add_mappings();
    mapping->set_iid(1);
    mapping->set_build_id(1);

    auto* build_id = interned_data->add_build_ids();
    build_id->set_iid(1);
    build_id->set_str("3BBCFBD372448A727265C3E7C4D954F91");

    auto* frame = interned_data->add_frames();
    frame->set_iid(1);
    frame->set_rel_pc(0x42);
    frame->set_mapping_id(1);

    auto* callstack = interned_data->add_callstacks();
    callstack->set_iid(1);
    callstack->add_frame_ids(1);
  }

  {
    auto* packet = trace_->add_packet();
    packet->set_trusted_packet_sequence_id(1);

    auto* samples = packet->set_streaming_profile_packet();
    samples->add_callstack_iid(1);
    samples->add_timestamp_delta_us(15);
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1u));

  Tokenize();
  context_.sorter->ExtractEventsForced();

  const auto& samples = storage_->cpu_profile_stack_sample_table();
  EXPECT_EQ(samples.row_count(), 1u);

  // Should have been translated to boottime, i.e. 10015 us absolute.
  EXPECT_EQ(samples[0].ts(), 10015000);
  EXPECT_EQ(samples[0].callsite_id(), CallsiteId{0});
  EXPECT_EQ(samples[0].utid(), 1u);
}

TEST_F(ProtoTraceParserTest, ConfigUuid) {
  auto* config = trace_->add_packet()->set_trace_config();
  config->set_trace_uuid_lsb(1);
  config->set_trace_uuid_msb(2);

  ASSERT_TRUE(Tokenize().ok());
  context_.sorter->ExtractEventsForced();

  SqlValue value =
      context_.metadata_tracker->GetMetadata(metadata::trace_uuid).value();
  EXPECT_STREQ(value.string_value, "00000000-0000-0002-0000-000000000001");
  ASSERT_TRUE(context_.uuid_found_in_trace);
}

TEST_F(ProtoTraceParserTest, PacketUuid) {
  auto* uuid = trace_->add_packet()->set_trace_uuid();
  uuid->set_lsb(1);
  uuid->set_msb(2);

  ASSERT_TRUE(Tokenize().ok());
  context_.sorter->ExtractEventsForced();

  SqlValue value =
      context_.metadata_tracker->GetMetadata(metadata::trace_uuid).value();
  EXPECT_STREQ(value.string_value, "00000000-0000-0002-0000-000000000001");
  ASSERT_TRUE(context_.uuid_found_in_trace);
}

// If both the TraceConfig and TracePacket.trace_uuid are present, the latter
// is considered the source of truth.
TEST_F(ProtoTraceParserTest, PacketAndConfigUuid) {
  auto* uuid = trace_->add_packet()->set_trace_uuid();
  uuid->set_lsb(1);
  uuid->set_msb(2);

  auto* config = trace_->add_packet()->set_trace_config();
  config->set_trace_uuid_lsb(42);
  config->set_trace_uuid_msb(42);

  ASSERT_TRUE(Tokenize().ok());
  context_.sorter->ExtractEventsForced();

  SqlValue value =
      context_.metadata_tracker->GetMetadata(metadata::trace_uuid).value();
  EXPECT_STREQ(value.string_value, "00000000-0000-0002-0000-000000000001");
  ASSERT_TRUE(context_.uuid_found_in_trace);
}

TEST_F(ProtoTraceParserTest, ConfigPbtxt) {
  auto* config = trace_->add_packet()->set_trace_config();
  config->add_buffers()->set_size_kb(42);

  ASSERT_TRUE(Tokenize().ok());
  context_.sorter->ExtractEventsForced();

  SqlValue value =
      context_.metadata_tracker->GetMetadata(metadata::trace_config_pbtxt)
          .value();
  EXPECT_THAT(value.string_value, HasSubstr("size_kb: 42"));
}

}  // namespace
}  // namespace dejaview::trace_processor
