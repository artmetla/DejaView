#!/usr/bin/env python3
# Copyright (C) 2024 The Android Open Source Project
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

import subprocess
"""
Compile the gRPC python code for Bigtrace
and modify the include paths to point to the correct file paths

"""


def main():
  subprocess.run([
      "python",
      "-m",
      "grpc_tools.protoc",
      "-I.",
      "--python_out=python/dejaview/bigtrace",
      "--pyi_out=python/dejaview/bigtrace",
      "protos/dejaview/bigtrace/orchestrator.proto",
      "protos/dejaview/trace_processor/trace_processor.proto",
      "protos/dejaview/common/descriptor.proto",
      "protos/dejaview/trace_processor/metatrace_categories.proto",
  ])
  subprocess.run([
      "python",
      "-m",
      "grpc_tools.protoc",
      "-I.",
      "--python_out=python/dejaview/bigtrace",
      "--pyi_out=python/dejaview/bigtrace",
      "--grpc_python_out=python/dejaview/bigtrace",
      "protos/dejaview/bigtrace/orchestrator.proto",
  ])
  subprocess.run([
      "sed",
      "-i",
      "-e",
      "s/protos\.dejaview/dejaview\.bigtrace\.protos\.dejaview/",
      "python/dejaview/bigtrace/protos/dejaview/bigtrace/orchestrator_pb2_grpc.py",
      "python/dejaview/bigtrace/protos/dejaview/bigtrace/orchestrator_pb2.py",
      "python/dejaview/bigtrace/protos/dejaview/bigtrace/orchestrator_pb2.pyi",
      "python/dejaview/bigtrace/protos/dejaview/trace_processor/trace_processor_pb2.py",
      "python/dejaview/bigtrace/protos/dejaview/trace_processor/trace_processor_pb2.pyi",
  ])
  return 0


if __name__ == "__main__":
  main()
