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

#include "src/trace_redaction/populate_allow_lists.h"

#include "dejaview/base/status.h"
#include "src/trace_redaction/trace_redaction_framework.h"

#include "protos/dejaview/trace/trace_packet.pbzero.h"

namespace dejaview::trace_redaction {

base::Status PopulateAllowlists::Build(Context* context) const {
  auto& packet_mask = context->packet_mask;

  // Top-level fields - fields outside of the "oneof data" field.
  packet_mask.set(
      protos::pbzero::TracePacket::kFirstPacketOnSequenceFieldNumber);
  packet_mask.set(
      protos::pbzero::TracePacket::kIncrementalStateClearedFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kInternedDataFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kMachineIdFieldNumber);
  packet_mask.set(
      protos::pbzero::TracePacket::kPreviousPacketDroppedFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kSequenceFlagsFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTimestampClockIdFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTimestampFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTracePacketDefaultsFieldNumber);
  packet_mask.set(
      protos::pbzero::TracePacket::kTrustedPacketSequenceIdFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTrustedPidFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTrustedUidFieldNumber);

  // Trace packet data (one-of field) - Every field here should also be modified
  // by message-focused transform.
  packet_mask.set(protos::pbzero::TracePacket::kClockSnapshotFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kCpuInfoFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kProcessStatsFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kProcessTreeFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kServiceEventFieldNumber);
  packet_mask.set(
      protos::pbzero::TracePacket::kSynchronizationMarkerFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kSysStatsFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kSystemInfoFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTraceConfigFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTraceStatsFieldNumber);
  packet_mask.set(protos::pbzero::TracePacket::kTriggerFieldNumber);

  return base::OkStatus();
}

}  // namespace dejaview::trace_redaction
