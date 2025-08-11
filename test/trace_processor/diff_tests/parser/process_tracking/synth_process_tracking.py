#!/usr/bin/env python3
# Copyright (C) 2019 The Android Open Source Project
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

# This synthetic trace is designed to test the process tracking logic. It
# synthesizes a combination of sched_switch, task_newtask, and process tree
# packets (i.e. the result of the userspace /proc/pid scraper).

from os import sys, path

from synth_common import CLONE_THREAD
import synth_common

trace = synth_common.create_trace()

# In the synthetic /proc/pic scraper packet, we pretend we missed p1-1. At the
# SQL level we should be able to tell that p1-t0 and p1-t2 belong to 'process1'
# but p1-t1 should be left unjoinable.
trace.add_packet(ts=5)
trace.add_process(10, 0, "process1", 1001)
trace.add_thread(12, 10, "p1-t2")

# From the process tracker viewpoint we pretend we only scraped tids=20,21.
trace.add_packet(ts=15)
trace.add_process(20, 0, "process_2", 1002)
trace.add_thread(21, 20, "p2-t1")

trace.add_packet(ts=29)
trace.add_process(40, 30, "process_4")

sys.stdout.buffer.write(trace.trace.SerializeToString())
