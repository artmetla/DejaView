/*
 * Copyright (C) 2022 The Android Open Source Project
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

// This example demonstrates startup tracing with a custom data source.
// Startup tracing can work only with kSystemBackend. Before running
// this example, `traced` must already be running in a separate process.

// Run system tracing: ninja -C out/default/ traced && ./out/default/traced
// And then run this example: ninja -C out/default example_startup_trace &&
//                            ./out/default/example_startup_trace

#if defined(DEJAVIEW_SDK_EXAMPLE_USE_INTERNAL_HEADERS)
#include "dejaview/tracing.h"
#include "dejaview/tracing/core/data_source_descriptor.h"
#include "dejaview/tracing/core/trace_config.h"
#include "dejaview/tracing/data_source.h"
#include "dejaview/tracing/tracing.h"
#include "protos/dejaview/trace/test_event.pbzero.h"
#else
#include <dejaview.h>
#endif

#include <fstream>
#include <iostream>
#include <thread>

namespace {

// The definition of our custom data source. Instances of this class will be
// automatically created and destroyed by DejaView.
class CustomDataSource : public dejaview::DataSource<CustomDataSource> {};

void InitializeDejaView() {
  dejaview::TracingInitArgs args;
  // The backends determine where trace events are recorded. For this example we
  // are going to use the system-wide tracing service, because the in-process
  // backend doesn't support startup tracing.
  args.backends = dejaview::kSystemBackend;
  dejaview::Tracing::Initialize(args);

  // Register our custom data source. Only the name is required, but other
  // properties can be advertised too.
  dejaview::DataSourceDescriptor dsd;
  dsd.set_name("com.example.startup_trace");
  CustomDataSource::Register(dsd);
}

// The trace config defines which types of data sources are enabled for
// recording.
dejaview::TraceConfig GetTraceConfig() {
  dejaview::TraceConfig cfg;
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("com.example.startup_trace");
  return cfg;
}

void StartStartupTracing() {
  dejaview::Tracing::SetupStartupTracingOpts args;
  args.backend = dejaview::kSystemBackend;
  dejaview::Tracing::SetupStartupTracingBlocking(GetTraceConfig(), args);
}

std::unique_ptr<dejaview::TracingSession> StartTracing() {
  auto tracing_session = dejaview::Tracing::NewTrace();
  tracing_session->Setup(GetTraceConfig());
  tracing_session->StartBlocking();
  return tracing_session;
}

void StopTracing(std::unique_ptr<dejaview::TracingSession> tracing_session) {
  // Flush to make sure the last written event ends up in the trace.
  CustomDataSource::Trace(
      [](CustomDataSource::TraceContext ctx) { ctx.Flush(); });

  // Stop tracing and read the trace data.
  tracing_session->StopBlocking();
  std::vector<char> trace_data(tracing_session->ReadTraceBlocking());

  // Write the result into a file.
  // Note: To save memory with longer traces, you can tell DejaView to write
  // directly into a file by passing a file descriptor into Setup() above.
  std::ofstream output;
  const char* filename = "example_startup_trace.pftrace";
  output.open(filename, std::ios::out | std::ios::binary);
  output.write(trace_data.data(),
               static_cast<std::streamsize>(trace_data.size()));
  output.close();
  DEJAVIEW_LOG(
      "Trace written in %s file. To read this trace in "
      "text form, run `./tools/traceconv text %s`",
      filename, filename);
}

}  // namespace

DEJAVIEW_DECLARE_DATA_SOURCE_STATIC_MEMBERS(CustomDataSource);
DEJAVIEW_DEFINE_DATA_SOURCE_STATIC_MEMBERS(CustomDataSource);

int main(int, const char**) {
  InitializeDejaView();

  StartStartupTracing();

  // Write an event using our custom data source before starting tracing
  // session.
  CustomDataSource::Trace([](CustomDataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(41);
    packet->set_for_testing()->set_str("Startup Event");
  });

  auto tracing_session = StartTracing();

  // Write an event using our custom data source.
  CustomDataSource::Trace([](CustomDataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(42);
    packet->set_for_testing()->set_str("Main Event");
  });
  StopTracing(std::move(tracing_session));

  return 0;
}
