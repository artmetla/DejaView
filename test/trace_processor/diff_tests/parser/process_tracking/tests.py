#!/usr/bin/env python3
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint, TraceInjector
from python.generators.diff_tests.testing import TestSuite


class ProcessTracking(TestSuite):

  # Process uid handling
  def test_process_tracking_uid(self):
    return DiffTestBlueprint(
        trace=Path('synth_process_tracking.py'),
        query="""
        SELECT pid, uid
        FROM process
        ORDER BY pid;
        """,
        out=Csv("""
        "pid","uid"
        0,"[NULL]"
        10,1001
        20,1002
        30,"[NULL]"
        40,"[NULL]"
        """))

  def test_process_stats_process_runtime(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          first_packet_on_sequence: true
          timestamp: 1088821452006028
          incremental_state_cleared: true
          process_tree {
            processes {
              pid: 9301
              ppid: 9251
              uid: 304336
              nspid: 4
              nspid: 1
              cmdline: "/bin/command"
              process_start_from_boot: 157620000000
            }
            collection_end_timestamp: 1088821520810204
          }
          trusted_uid: 304336
          trusted_packet_sequence_id: 3
          trusted_pid: 1137063
          previous_packet_dropped: true
        }
        packet {
          timestamp: 1088821520899054
          process_stats {
            processes {
              pid: 9301
              runtime_user_mode: 16637390000000
              runtime_kernel_mode: 1327800000000
              vm_size_kb: 1188971644
              vm_locked_kb: 0
              vm_hwm_kb: 1180568
              vm_rss_kb: 1100672
              rss_anon_kb: 1045332
              rss_file_kb: 46848
              rss_shmem_kb: 8492
              vm_swap_kb: 163936
              oom_score_adj: 300
            }
            collection_end_timestamp: 1088821539659978
          }
          trusted_uid: 304336
          trusted_packet_sequence_id: 3
          trusted_pid: 1137063
        }
        packet {
          timestamp: 1088821786436938
          process_stats {
            processes {
              pid: 9301
              runtime_user_mode: 16638280000000
              runtime_kernel_mode: 1327860000000
              vm_size_kb: 1188979836
              vm_locked_kb: 0
              vm_hwm_kb: 1180568
              vm_rss_kb: 895428
              rss_anon_kb: 832028
              rss_file_kb: 46848
              rss_shmem_kb: 16552
              vm_swap_kb: 163936
              oom_score_adj: 300
            }
            collection_end_timestamp: 1088821817629747
          }
          trusted_uid: 304336
          trusted_packet_sequence_id: 3
          trusted_pid: 1137063
        }
        """),
        query="""
        select c.ts, c.value, pct.name, p.pid, p.start_ts, p.cmdline
        from counter c
          join process_counter_track pct on (c.track_id = pct.id)
          join process p using (upid)
        where pct.name in ("runtime.user_ns", "runtime.kernel_ns")
          and p.pid = 9301
        order by ts asc, pct.name asc
        """,
        out=Csv("""
        "ts","value","name","pid","start_ts","cmdline"
        1088821520899054,1327800000000.000000,"runtime.kernel_ns",9301,157620000000,"/bin/command"
        1088821520899054,16637390000000.000000,"runtime.user_ns",9301,157620000000,"/bin/command"
        1088821786436938,1327860000000.000000,"runtime.kernel_ns",9301,157620000000,"/bin/command"
        1088821786436938,16638280000000.000000,"runtime.user_ns",9301,157620000000,"/bin/command"
        """))

  # Distinguish set-to-zero process age (can happen for kthreads) from unset
  # process age.
  def test_process_age_optionality(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          first_packet_on_sequence: true
          timestamp: 1088821452006028
          incremental_state_cleared: true
          process_tree {
            processes {
              pid: 2
              ppid: 0
              uid: 0
              cmdline: "kthreadd"
              process_start_from_boot: 0
            }
            processes {
              pid: 68
              ppid: 2
              uid: 0
              cmdline: "ksoftirqd/7"
              process_start_from_boot: 10000000
            }
            processes {
              pid: 9301
              ppid: 9251
              uid: 304336
              cmdline: "no_age_field"
            }
            collection_end_timestamp: 1088821520810204
          }
          trusted_uid: 304336
          trusted_packet_sequence_id: 3
          trusted_pid: 1137063
          previous_packet_dropped: true
        }
        """),
        query="""
        select p.pid, p.start_ts, p.cmdline
        from process p
        where pid in (2, 68, 9301)
        order by pid asc;
        """,
        out=Csv("""
        "pid","start_ts","cmdline"
        2,0,"kthreadd"
        68,10000000,"ksoftirqd/7"
        9301,"[NULL]","no_age_field"
        """))
