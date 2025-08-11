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

#include "dejaview/base/status.h"
#include "dejaview/protozero/scattered_heap_buffer.h"
#include "src/trace_redaction/proto_util.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace dejaview::trace_redaction {

base::Status BroadphasePacketFilter::Transform(const Context& context,
                                               std::string* packet) const {
  if (context.packet_mask.none()) {
    return base::ErrStatus("FilterTracePacketFields: empty packet mask.");
  }

  if (!packet || packet->empty()) {
    return base::ErrStatus("FilterTracePacketFields: missing packet.");
  }

  protozero::HeapBuffered<protos::pbzero::TracePacket> message;

  protozero::ProtoDecoder decoder(*packet);

  const auto& mask = context.packet_mask;

  for (auto field = decoder.ReadField(); field.valid();
       field = decoder.ReadField()) {
    // Make sure the id can be references. If it is out of bounds, it is by
    // definition "no set".
    if (field.id() < mask.size() && mask.test(field.id())) {
      proto_util::AppendField(field, message.get());
    }
  }

  packet->assign(message.SerializeAsString());
  return base::OkStatus();
}

}  // namespace dejaview::trace_redaction
