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

#include "src/shared_lib/test/utils.h"

#include "dejaview/public/abi/heap_buffer.h"
#include "dejaview/public/pb_msg.h"
#include "dejaview/public/pb_utils.h"
#include "dejaview/public/protos/config/data_source_config.pzc.h"
#include "dejaview/public/protos/config/trace_config.pzc.h"
#include "dejaview/public/protos/config/track_event/track_event_config.pzc.h"
#include "dejaview/public/tracing_session.h"

namespace dejaview {
namespace shlib {
namespace test_utils {
namespace {

std::string ToHexChars(uint8_t val) {
  std::string ret;
  uint8_t high_nibble = (val & 0xF0) >> 4;
  uint8_t low_nibble = (val & 0xF);
  static const char hex_chars[] = "0123456789ABCDEF";
  ret.push_back(hex_chars[high_nibble]);
  ret.push_back(hex_chars[low_nibble]);
  return ret;
}

}  // namespace

TracingSession TracingSession::Builder::Build() {
  struct DejaViewPbMsgWriter writer;
  struct DejaViewHeapBuffer* hb = DejaViewHeapBufferCreate(&writer.writer);

  struct dejaview_protos_TraceConfig cfg;
  DejaViewPbMsgInit(&cfg.msg, &writer);

  {
    struct dejaview_protos_TraceConfig_BufferConfig buffers;
    dejaview_protos_TraceConfig_begin_buffers(&cfg, &buffers);

    dejaview_protos_TraceConfig_BufferConfig_set_size_kb(&buffers, 1024);

    dejaview_protos_TraceConfig_end_buffers(&cfg, &buffers);
  }

  {
    struct dejaview_protos_TraceConfig_DataSource data_sources;
    dejaview_protos_TraceConfig_begin_data_sources(&cfg, &data_sources);

    {
      struct dejaview_protos_DataSourceConfig ds_cfg;
      dejaview_protos_TraceConfig_DataSource_begin_config(&data_sources,
                                                          &ds_cfg);

      dejaview_protos_DataSourceConfig_set_cstr_name(&ds_cfg,
                                                     data_source_name_.c_str());
      if (!enabled_categories_.empty() || !disabled_categories_.empty()) {
        dejaview_protos_TrackEventConfig te_cfg;
        dejaview_protos_DataSourceConfig_begin_track_event_config(&ds_cfg,
                                                                  &te_cfg);
        for (const std::string& cat : enabled_categories_) {
          dejaview_protos_TrackEventConfig_set_enabled_categories(
              &te_cfg, cat.data(), cat.size());
        }
        for (const std::string& cat : disabled_categories_) {
          dejaview_protos_TrackEventConfig_set_disabled_categories(
              &te_cfg, cat.data(), cat.size());
        }
        dejaview_protos_DataSourceConfig_end_track_event_config(&ds_cfg,
                                                                &te_cfg);
      }

      dejaview_protos_TraceConfig_DataSource_end_config(&data_sources, &ds_cfg);
    }

    dejaview_protos_TraceConfig_end_data_sources(&cfg, &data_sources);
  }
  size_t cfg_size = DejaViewStreamWriterGetWrittenSize(&writer.writer);
  std::unique_ptr<uint8_t[]> ser(new uint8_t[cfg_size]);
  DejaViewHeapBufferCopyInto(hb, &writer.writer, ser.get(), cfg_size);
  DejaViewHeapBufferDestroy(hb, &writer.writer);

  struct DejaViewTracingSessionImpl* ts =
      DejaViewTracingSessionCreate(DEJAVIEW_BACKEND_IN_PROCESS);

  DejaViewTracingSessionSetup(ts, ser.get(), cfg_size);

  DejaViewTracingSessionStartBlocking(ts);

  return TracingSession::Adopt(ts);
}

TracingSession TracingSession::Adopt(
    struct DejaViewTracingSessionImpl* session) {
  TracingSession ret;
  ret.session_ = session;
  ret.stopped_ = std::make_unique<WaitableEvent>();
  DejaViewTracingSessionSetStopCb(
      ret.session_,
      [](struct DejaViewTracingSessionImpl*, void* arg) {
        static_cast<WaitableEvent*>(arg)->Notify();
      },
      ret.stopped_.get());
  return ret;
}

TracingSession::TracingSession(TracingSession&& other) noexcept {
  session_ = other.session_;
  other.session_ = nullptr;
  stopped_ = std::move(other.stopped_);
  other.stopped_ = nullptr;
}

TracingSession::~TracingSession() {
  if (!session_) {
    return;
  }
  if (!stopped_->IsNotified()) {
    DejaViewTracingSessionStopBlocking(session_);
    stopped_->WaitForNotification();
  }
  DejaViewTracingSessionDestroy(session_);
}

bool TracingSession::FlushBlocking(uint32_t timeout_ms) {
  WaitableEvent notification;
  bool result;
  auto* cb = new std::function<void(bool)>([&](bool success) {
    result = success;
    notification.Notify();
  });
  DejaViewTracingSessionFlushAsync(
      session_, timeout_ms,
      [](DejaViewTracingSessionImpl*, bool success, void* user_arg) {
        auto* f = reinterpret_cast<std::function<void(bool)>*>(user_arg);
        (*f)(success);
        delete f;
      },
      cb);
  notification.WaitForNotification();
  return result;
}

void TracingSession::WaitForStopped() {
  stopped_->WaitForNotification();
}

void TracingSession::StopAsync() {
  DejaViewTracingSessionStopAsync(session_);
}

void TracingSession::StopBlocking() {
  DejaViewTracingSessionStopBlocking(session_);
}

std::vector<uint8_t> TracingSession::ReadBlocking() {
  std::vector<uint8_t> data;
  DejaViewTracingSessionReadTraceBlocking(
      session_,
      [](struct DejaViewTracingSessionImpl*, const void* trace_data,
         size_t size, bool, void* user_arg) {
        auto& dst = *static_cast<std::vector<uint8_t>*>(user_arg);
        auto* src = static_cast<const uint8_t*>(trace_data);
        dst.insert(dst.end(), src, src + size);
      },
      &data);
  return data;
}

}  // namespace test_utils
}  // namespace shlib
}  // namespace dejaview

void PrintTo(const DejaViewPbDecoderField& field, std::ostream* pos) {
  std::ostream& os = *pos;
  DejaViewPbDecoderStatus status =
      static_cast<DejaViewPbDecoderStatus>(field.status);
  switch (status) {
    case DEJAVIEW_PB_DECODER_ERROR:
      os << "MALFORMED PROTOBUF";
      break;
    case DEJAVIEW_PB_DECODER_DONE:
      os << "DECODER DONE";
      break;
    case DEJAVIEW_PB_DECODER_OK:
      switch (field.wire_type) {
        case DEJAVIEW_PB_WIRE_TYPE_DELIMITED:
          os << "\"";
          for (size_t i = 0; i < field.value.delimited.len; i++) {
            os << dejaview::shlib::test_utils::ToHexChars(
                      field.value.delimited.start[i])
               << " ";
          }
          os << "\"";
          break;
        case DEJAVIEW_PB_WIRE_TYPE_VARINT:
          os << "varint: " << field.value.integer64;
          break;
        case DEJAVIEW_PB_WIRE_TYPE_FIXED32:
          os << "fixed32: " << field.value.integer32;
          break;
        case DEJAVIEW_PB_WIRE_TYPE_FIXED64:
          os << "fixed64: " << field.value.integer64;
          break;
      }
      break;
  }
}
