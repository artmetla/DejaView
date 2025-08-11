/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/metadata_minimal_module.h"

#include "dejaview/ext/base/base64.h"
#include "dejaview/ext/base/string_utils.h"
#include "src/trace_processor/importers/common/metadata_tracker.h"
#include "src/trace_processor/importers/proto/packet_sequence_state_generation.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace dejaview {
namespace trace_processor {

using dejaview::protos::pbzero::TracePacket;

MetadataMinimalModule::MetadataMinimalModule(TraceProcessorContext*) {
}

ModuleResult MetadataMinimalModule::TokenizePacket(
    const protos::pbzero::TracePacket::Decoder&,
    TraceBlobView*,
    int64_t,
    RefPtr<PacketSequenceStateGeneration>,
    uint32_t) {
  return ModuleResult::Ignored();
}

}  // namespace trace_processor
}  // namespace dejaview
