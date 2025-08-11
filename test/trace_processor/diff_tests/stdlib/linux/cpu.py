#!/usr/bin/env python3
# Copyright (C) 2024 The Android Open Source Project
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

from python.generators.diff_tests.testing import Csv, DataPath, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class LinuxCpu(TestSuite):

  def test_linux_cpu_idle_time_in_state(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          sys_stats {
            cpuidle_state {
              cpu_id: 0
              cpuidle_state_entry {
                state: "C8"
                duration_us: 1000000
              }
            }
          }
          timestamp: 200000000000
          trusted_packet_sequence_id: 2
        }
        packet {
          sys_stats {
            cpuidle_state {
              cpu_id: 0
              cpuidle_state_entry {
                state: "C8"
                duration_us: 1000100
              }
            }
          }
          timestamp: 200001000000
          trusted_packet_sequence_id: 2
        }
        packet {
          sys_stats {
            cpuidle_state {
              cpu_id: 0
              cpuidle_state_entry {
                state: "C8"
                duration_us: 1000200
              }
            }
          }
          timestamp: 200002000000
          trusted_packet_sequence_id: 2
        }
         """),
        query="""
         INCLUDE DEJAVIEW MODULE linux.cpu.idle_time_in_state;
         SELECT * FROM cpu_idle_time_in_state_counters;
         """,
        out=Csv("""
         "ts","state_name","idle_percentage","total_residency","time_slice"
          200001000000,"cpuidle.C8",10.000000,100.000000,1000
          200002000000,"cpuidle.C8",10.000000,100.000000,1000
          200001000000,"cpuidle.C0",90.000000,900.000000,1000
          200002000000,"cpuidle.C0",90.000000,900.000000,1000
         """))
