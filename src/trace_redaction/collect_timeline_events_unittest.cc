
/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "src/trace_redaction/collect_timeline_events.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/trace_redaction_framework.h"

#include "protos/dejaview/trace/ps/process_tree.gen.h"
#include "protos/dejaview/trace/trace_packet.gen.h"

namespace dejaview::trace_redaction {

namespace {

constexpr uint64_t kPackage = 0;
constexpr int32_t kPid = 1093;

constexpr uint64_t kTimeA = 0;

}  // namespace

// Base class for all collect timeline event tests. Creates a simple trace that
// contains trace elements that should create timeline events.
class CollectTimelineEventsTest : public testing::Test {
 protected:
  void SetUp() { ASSERT_OK(collector_.Begin(&context_)); }

  Context context_;
  CollectTimelineEvents collector_;
};

TEST_F(CollectTimelineEventsTest, OpenEventForProcessTreeProcess) {
  {
    protos::gen::TracePacket packet;
    packet.set_timestamp(kTimeA);

    auto* process_tree = packet.mutable_process_tree();

    auto* process = process_tree->add_processes();
    process->set_pid(kPid);
    process->set_ppid(1);
    process->set_uid(kPackage);

    auto buffer = packet.SerializeAsString();

    protos::pbzero::TracePacket::Decoder decoder(buffer);

    ASSERT_OK(collector_.Collect(decoder, &context_));
  }

  ASSERT_OK(collector_.End(&context_));

  const auto* event = context_.timeline->GetOpeningEvent(kTimeA, kPid);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->valid());
}

TEST_F(CollectTimelineEventsTest, OpenEventForProcessTreeThread) {
  {
    protos::gen::TracePacket packet;
    packet.set_timestamp(kTimeA);

    auto* process_tree = packet.mutable_process_tree();

    auto* process = process_tree->add_threads();
    process->set_tid(kPid);
    process->set_tgid(1);

    auto buffer = packet.SerializeAsString();

    protos::pbzero::TracePacket::Decoder decoder(buffer);

    ASSERT_OK(collector_.Collect(decoder, &context_));
  }

  ASSERT_OK(collector_.End(&context_));

  const auto* event = context_.timeline->GetOpeningEvent(kTimeA, kPid);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->valid());
}

}  // namespace dejaview::trace_redaction
