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

#include <sys/stat.h>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/getopt.h"
#include "src/tools/ftrace_proto_gen/ftrace_descriptor_gen.h"
#include "src/tools/ftrace_proto_gen/ftrace_proto_gen.h"
#include "src/traced/probes/ftrace/format_parser/format_parser.h"

namespace {
inline std::unique_ptr<std::ostream> MakeOFStream(const std::string& filename) {
  return std::unique_ptr<std::ostream>(new std::ofstream(filename));
}

inline std::unique_ptr<std::ostream> MakeVerifyStream(
    const std::string& filename) {
  return std::unique_ptr<std::ostream>(new dejaview::VerifyStream(filename));
}

void PrintUsage(const char* bin_name) {
  fprintf(stderr,
          "Usage: %s -w event_list_path -o output_dir -d proto_descriptor "
          "[--check_only] input_dir...\n",
          bin_name);
}
}  // namespace

int main(int argc, char** argv) {
  static option long_options[] = {
      {"event_list", required_argument, nullptr, 'w'},
      {"output_dir", required_argument, nullptr, 'o'},
      {"proto_descriptor", required_argument, nullptr, 'd'},
      {"update_build_files", no_argument, nullptr, 'b'},
      {"check_only", no_argument, nullptr, 'c'},
      {nullptr, 0, nullptr, 0}};

  int c;

  std::string event_list_path;
  std::string output_dir;
  std::string proto_descriptor;
  bool update_build_files = false;

  std::unique_ptr<std::ostream> (*ostream_factory)(const std::string&) =
      &MakeOFStream;

  while ((c = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (c) {
      case 'w':
        event_list_path = optarg;
        break;
      case 'o':
        output_dir = optarg;
        break;
      case 'd':
        proto_descriptor = optarg;
        break;
      case 'b':
        update_build_files = true;
        break;
      case 'c':
        ostream_factory = &MakeVerifyStream;
        break;
      default: {
        PrintUsage(argv[0]);
        return 1;
      }
    }
  }

  if (optind >= argc) {
    PrintUsage(argv[0]);
    return 1;
  }

  DEJAVIEW_CHECK(!event_list_path.empty());
  DEJAVIEW_CHECK(!output_dir.empty());
  DEJAVIEW_CHECK(!proto_descriptor.empty());

  std::vector<dejaview::FtraceEventName> event_list =
      dejaview::ReadAllowList(event_list_path);
  std::vector<std::string> events_info;

  google::protobuf::DescriptorPool descriptor_pool;
  descriptor_pool.AllowUnknownDependencies();
  {
    google::protobuf::FileDescriptorSet file_descriptor_set;
    std::string descriptor_bytes;
    if (!dejaview::base::ReadFile(proto_descriptor, &descriptor_bytes)) {
      fprintf(stderr, "Failed to open %s\n", proto_descriptor.c_str());
      return 1;
    }
    file_descriptor_set.ParseFromString(descriptor_bytes);

    for (const auto& d : file_descriptor_set.file()) {
      DEJAVIEW_CHECK(descriptor_pool.BuildFile(d));
    }
  }

  std::set<std::string> groups;
  std::multimap<std::string, const dejaview::FtraceEventName*> group_to_event;
  std::set<std::string> new_events;
  for (const auto& event : event_list) {
    if (!event.valid())
      continue;
    groups.emplace(event.group());
    group_to_event.emplace(event.group(), &event);
    struct stat buf;
    if (stat(
            ("protos/dejaview/trace/ftrace/" + event.name() + ".proto").c_str(),
            &buf) == -1) {
      new_events.insert(event.name());
    }
  }

  {
    std::unique_ptr<std::ostream> out =
        ostream_factory(output_dir + "/ftrace_event.proto");
    dejaview::GenerateFtraceEventProto(event_list, groups, out.get());
  }

  for (const std::string& group : groups) {
    std::string proto_file_name = group + ".proto";
    std::string output_path = output_dir + std::string("/") + proto_file_name;
    std::unique_ptr<std::ostream> fout = ostream_factory(output_path);
    if (!fout) {
      fprintf(stderr, "Failed to open %s\n", output_path.c_str());
      return 1;
    }
    *fout << dejaview::ProtoHeader();

    auto range = group_to_event.equal_range(group);
    for (auto it = range.first; it != range.second; ++it) {
      const auto& event = *it->second;
      if (!event.valid())
        continue;

      std::string proto_name =
          dejaview::EventNameToProtoName(group, event.name());
      dejaview::Proto proto;
      proto.name = proto_name;
      proto.event_name = event.name();
      const google::protobuf::Descriptor* d =
          descriptor_pool.FindMessageTypeByName("dejaview.protos." +
                                                proto_name);
      if (d)
        proto = dejaview::Proto(event.name(), *d);
      else
        DEJAVIEW_LOG("Did not find %s", proto_name.c_str());
      for (int i = optind; i < argc; ++i) {
        std::string input_dir = argv[i];
        std::string input_path = input_dir + event.group() + "/" +
                                 event.name() + std::string("/format");

        std::string contents;
        if (!dejaview::base::ReadFile(input_path, &contents)) {
          continue;
        }

        dejaview::FtraceEvent format;
        if (!dejaview::ParseFtraceEvent(contents, &format)) {
          fprintf(stderr, "Could not parse file %s.\n", input_path.c_str());
          return 1;
        }

        auto proto_fields = dejaview::ToProtoFields(format);
        proto.UnionFields(proto_fields);
      }

      uint32_t i = 0;
      for (; it->second != &event_list[i]; i++)
        ;

      // The first id used for events in FtraceEvent proto is 3.
      uint32_t proto_field = i + 3;

      // The generic event has field id 327 so any event with a id higher
      // than that has to be incremented by 1.
      if (proto_field >= 327)
        proto_field++;

      events_info.push_back(
          dejaview::SingleEventInfo(proto, event.group(), proto_field));

      *fout << proto.ToString();
      DEJAVIEW_CHECK(!fout->fail());
    }
  }

  {
    std::unique_ptr<std::ostream> out = ostream_factory(
        "src/trace_processor/importers/ftrace/ftrace_descriptors.cc");
    dejaview::GenerateFtraceDescriptors(descriptor_pool, out.get());
    DEJAVIEW_CHECK(!out->fail());
  }

  {
    std::unique_ptr<std::ostream> out =
        ostream_factory("src/traced/probes/ftrace/event_info.cc");
    dejaview::GenerateEventInfo(events_info, out.get());
    DEJAVIEW_CHECK(!out->fail());
  }

  if (update_build_files) {
    std::unique_ptr<std::ostream> f =
        ostream_factory(output_dir + "/all_protos.gni");

    *f << R"(# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Autogenerated by ftrace_proto_gen.

ftrace_proto_names = [
  "ftrace_event.proto",
  "ftrace_event_bundle.proto",
  "ftrace_stats.proto",
  "test_bundle_wrapper.proto",
  "generic.proto",
)";
    for (const std::string& group : groups) {
      *f << "  \"" << group << ".proto\",\n";
    }
    *f << "]\n";
    DEJAVIEW_CHECK(!f->fail());
  }
}
