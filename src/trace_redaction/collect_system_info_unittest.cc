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

#include "src/trace_redaction/collect_system_info.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_processor/util/status_macros.h"
#include "test/gtest_and_gmock.h"
#include "protos/dejaview/trace/trace_packet.gen.h"

namespace dejaview::trace_redaction {

class CollectSystemInfoTest : public testing::Test {
 protected:
  base::Status Collect() {
    auto buffer = packet_.SerializeAsString();
    protos::pbzero::TracePacket::Decoder decoder(buffer);

    RETURN_IF_ERROR(collect_.Begin(&context_));
    RETURN_IF_ERROR(collect_.Collect(decoder, &context_));
    return collect_.End(&context_);
  }

  protos::gen::TracePacket packet_;
  Context context_;
  CollectSystemInfo collect_;
};

// The first synth thread pid should be beyond the range of valid pids.
TEST(SystemInfoTest, FirstSynthThreadPidIsNotAValidPid) {
  SystemInfo info;

  auto pid = info.AllocateSynthThread();
  ASSERT_GT(pid, 1 << 22);
}

TEST(BuildSyntheticProcessTest, CreatesThreadsPerCpu) {
  Context context;
  context.system_info.emplace();

  // The first CPU is always 0, so CPU 7 means there are 8 CPUs.
  context.system_info->ReserveCpu(7);

  BuildSyntheticThreads build;
  ASSERT_OK(build.Build(&context));

  ASSERT_NE(context.synthetic_process->tgid(), 0);

  // One main thread and 1 thread per CPU.
  ASSERT_EQ(context.synthetic_process->tids().size(), 9u);
}

}  // namespace dejaview::trace_redaction
