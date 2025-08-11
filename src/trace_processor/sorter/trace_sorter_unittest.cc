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
#include "src/trace_processor/sorter/trace_sorter.h"

#include <map>
#include <random>
#include <vector>

#include "dejaview/trace_processor/basic_types.h"
#include "dejaview/trace_processor/trace_blob.h"
#include "dejaview/trace_processor/trace_blob_view.h"
#include "src/trace_processor/importers/common/parser_types.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/importers/proto/proto_trace_parser_impl.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "test/gtest_and_gmock.h"

namespace dejaview {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;

class MockTraceParser : public ProtoTraceParserImpl {
 public:
  explicit MockTraceParser(TraceProcessorContext* context)
      : ProtoTraceParserImpl(context), machine_id_(context->machine_id()) {}

  MOCK_METHOD(void,
              MOCK_ParseTracePacket,
              (int64_t ts, const uint8_t* data, size_t length));

  void ParseTrackEvent(int64_t, TrackEventData) override {}

  void ParseTracePacket(int64_t ts, TracePacketData data) override {
    TraceBlobView& tbv = data.packet;
    MOCK_ParseTracePacket(ts, tbv.data(), tbv.length());
  }

  std::optional<MachineId> machine_id_;
};

class MockTraceStorage : public TraceStorage {
 public:
  MockTraceStorage() : TraceStorage() {}

  MOCK_METHOD(StringId, InternString, (base::StringView view), (override));
};

class TraceSorterTest : public ::testing::Test {
 public:
  TraceSorterTest() : test_buffer_(TraceBlob::Allocate(8)) {
    storage_ = new NiceMock<MockTraceStorage>();
    context_.storage.reset(storage_);
    CreateSorter();
  }

  void CreateSorter(bool full_sort = true) {
    parser_ = new MockTraceParser(&context_);
    context_.proto_trace_parser.reset(parser_);
    auto sorting_mode = full_sort ? TraceSorter::SortingMode::kFullSort
                                  : TraceSorter::SortingMode::kDefault;
    context_.sorter.reset(new TraceSorter(&context_, sorting_mode));
  }

 protected:
  TraceProcessorContext context_;
  MockTraceParser* parser_;
  NiceMock<MockTraceStorage>* storage_;
  TraceBlobView test_buffer_;
};

TEST_F(TraceSorterTest, TestTracePacket) {
  auto state = PacketSequenceStateGeneration::CreateFirst(&context_);
  TraceBlobView view = test_buffer_.slice_off(0, 1);
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1000, view.data(), 1));
  context_.sorter->PushTracePacket(1000, state, std::move(view));
  context_.sorter->ExtractEventsForced();
}

TEST_F(TraceSorterTest, Ordering) {
  auto state = PacketSequenceStateGeneration::CreateFirst(&context_);
  TraceBlobView view_1 = test_buffer_.slice_off(0, 1);
  TraceBlobView view_2 = test_buffer_.slice_off(0, 2);
  TraceBlobView view_3 = test_buffer_.slice_off(0, 3);
  TraceBlobView view_4 = test_buffer_.slice_off(0, 4);

  InSequence s;
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1001, view_2.data(), 2));
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1100, view_3.data(), 3));

  context_.sorter->PushTracePacket(1001, state, std::move(view_2));
  context_.sorter->PushTracePacket(1100, state, std::move(view_3));
  context_.sorter->ExtractEventsForced();
}

TEST_F(TraceSorterTest, IncrementalExtraction) {
  CreateSorter(false);

  auto state = PacketSequenceStateGeneration::CreateFirst(&context_);

  TraceBlobView view_1 = test_buffer_.slice_off(0, 1);
  TraceBlobView view_2 = test_buffer_.slice_off(0, 2);
  TraceBlobView view_3 = test_buffer_.slice_off(0, 3);
  TraceBlobView view_4 = test_buffer_.slice_off(0, 4);
  TraceBlobView view_5 = test_buffer_.slice_off(0, 5);

  // Flush at the start of packet sequence to match behavior of the
  // service.
  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1200, state, std::move(view_2));
  context_.sorter->PushTracePacket(1100, state, std::move(view_1));

  // No data should be exttracted at this point because we haven't
  // seen two flushes yet.
  context_.sorter->NotifyReadBufferEvent();

  // Now that we've seen two flushes, we should be ready to start extracting
  // data on the next OnReadBufer call (after two flushes as usual).
  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyReadBufferEvent();

  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1400, state, std::move(view_4));
  context_.sorter->PushTracePacket(1300, state, std::move(view_3));

  // This ReadBuffer call should finally extract until the first OnReadBuffer
  // call.
  {
    InSequence s;
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1100, test_buffer_.data(), 1));
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1200, test_buffer_.data(), 2));
  }
  context_.sorter->NotifyReadBufferEvent();

  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1500, state, std::move(view_5));

  // Nothing should be extracted as we haven't seen the second flush.
  context_.sorter->NotifyReadBufferEvent();

  // Now we've seen the second flush we should extract the next two packets.
  context_.sorter->NotifyFlushEvent();
  {
    InSequence s;
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1300, test_buffer_.data(), 3));
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1400, test_buffer_.data(), 4));
  }
  context_.sorter->NotifyReadBufferEvent();

  // The forced extraction should get the last packet.
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1500, test_buffer_.data(), 5));
  context_.sorter->ExtractEventsForced();
}

// Simulate a producer bug where the third packet is emitted
// out of order. Verify that we track the stats correctly.
TEST_F(TraceSorterTest, OutOfOrder) {
  CreateSorter(false);

  auto state = PacketSequenceStateGeneration::CreateFirst(&context_);

  TraceBlobView view_1 = test_buffer_.slice_off(0, 1);
  TraceBlobView view_2 = test_buffer_.slice_off(0, 2);
  TraceBlobView view_3 = test_buffer_.slice_off(0, 3);
  TraceBlobView view_4 = test_buffer_.slice_off(0, 4);

  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1200, state, std::move(view_2));
  context_.sorter->PushTracePacket(1100, state, std::move(view_1));
  context_.sorter->NotifyReadBufferEvent();

  // Both of the packets should have been pushed through.
  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  {
    InSequence s;
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1100, test_buffer_.data(), 1));
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1200, test_buffer_.data(), 2));
  }
  context_.sorter->NotifyReadBufferEvent();

  // Now, pass the third packet out of order.
  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1150, state, std::move(view_3));
  context_.sorter->NotifyReadBufferEvent();

  // The third packet should still be pushed through.
  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1150, test_buffer_.data(), 3));
  context_.sorter->NotifyReadBufferEvent();

  // But we should also increment the stat that this was out of order.
  ASSERT_EQ(
      context_.storage->stats()[stats::sorter_push_event_out_of_order].value,
      1);

  // Push the fourth packet also out of order but after third.
  context_.sorter->NotifyFlushEvent();
  context_.sorter->NotifyFlushEvent();
  context_.sorter->PushTracePacket(1170, state, std::move(view_4));
  context_.sorter->NotifyReadBufferEvent();

  // The fourt packet should still be pushed through.
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1170, test_buffer_.data(), 4));
  context_.sorter->ExtractEventsForced();

  // But we should also increment the stat that this was out of order.
  ASSERT_EQ(
      context_.storage->stats()[stats::sorter_push_event_out_of_order].value,
      2);
}

}  // namespace
}  // namespace trace_processor
}  // namespace dejaview
