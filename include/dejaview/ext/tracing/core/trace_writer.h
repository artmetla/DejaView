/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_EXT_TRACING_CORE_TRACE_WRITER_H_
#define INCLUDE_DEJAVIEW_EXT_TRACING_CORE_TRACE_WRITER_H_

#include <functional>

#include "dejaview/base/export.h"
#include "dejaview/ext/tracing/core/basic_types.h"
#include "dejaview/protozero/message_handle.h"
#include "dejaview/tracing/trace_writer_base.h"

namespace dejaview {

namespace protos {
namespace pbzero {
class TracePacket;
}  // namespace pbzero
}  // namespace protos

// See comments in include/dejaview/tracing/trace_writer_base.h
class DEJAVIEW_EXPORT_COMPONENT TraceWriter : public TraceWriterBase {
 public:
  using TracePacketHandle =
      protozero::MessageHandle<protos::pbzero::TracePacket>;

  TraceWriter();
  ~TraceWriter() override;

  virtual WriterID writer_id() const = 0;

 private:
  TraceWriter(const TraceWriter&) = delete;
  TraceWriter& operator=(const TraceWriter&) = delete;
};

}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_EXT_TRACING_CORE_TRACE_WRITER_H_
