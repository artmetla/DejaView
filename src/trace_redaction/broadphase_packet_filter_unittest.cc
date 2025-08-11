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

#include "src/trace_redaction/broadphase_packet_filter.h"
#include "protos/dejaview/trace/trace_packet.gen.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

#include "protos/dejaview/trace/trace_packet.pbzero.h"

namespace dejaview::trace_redaction {

class BroadphasePacketFilterTest : public testing::Test {
 protected:
  BroadphasePacketFilter transform_;
  Context context_;
  protos::gen::TracePacket builder_;
};

TEST_F(BroadphasePacketFilterTest, ReturnErrorForEmptyMasks) {
  auto buffer = builder_.SerializeAsString();
  ASSERT_FALSE(transform_.Transform(context_, &buffer).ok());
}

}  // namespace dejaview::trace_redaction
