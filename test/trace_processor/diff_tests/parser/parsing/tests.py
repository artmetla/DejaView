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
from python.generators.diff_tests.testing import Csv, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint, TraceInjector
from python.generators.diff_tests.testing import TestSuite


class Parsing(TestSuite):

  def test_process_stats_poll_oom_score(self):
    return DiffTestBlueprint(
        trace=DataPath('process_stats_poll.pb'),
        query="""
        SELECT ts, name, value, upid
        FROM counter c
        JOIN process_counter_track t
          ON c.track_id = t.id
        WHERE name = "oom_score_adj"
        ORDER BY ts
        LIMIT 20;
        """,
        out=Path('process_stats_poll_oom_score.out'))

  # Config & metadata
  def test_config_metadata(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          clock_snapshot {
            clocks {
              clock_id: 6
              timestamp: 101000002
            }
            clocks {
              clock_id: 128
              timestamp: 2
            }
          }
          timestamp: 101000002
        }
        packet {
          trace_config {
            trace_uuid_msb: 1314564453825188563
            trace_uuid_lsb: -6605018796207623390
          }
        }
        packet {
          system_info {
            android_build_fingerprint: "the fingerprint"
          }
        }
        """),
        query="""
        SELECT name, str_value FROM metadata WHERE str_value IS NOT NULL ORDER BY name;
        """,
        out=Csv("""
        "name","str_value"
        "android_build_fingerprint","the fingerprint"
        "trace_config_pbtxt","trace_uuid_msb: 1314564453825188563
        trace_uuid_lsb: -6605018796207623390"
        "trace_type","proto"
        "trace_uuid","123e4567-e89b-12d3-a456-426655443322"
        """))

  def test_triggers_packets_trigger_packet_trace(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          trigger {
            trigger_name: "test1"
            trusted_producer_uid: 3
            producer_name: "producer1"
          }
          timestamp: 101000002
        }
        packet {
          trigger {
            trigger_name: "test2"
            trusted_producer_uid: 4
            producer_name: "producer2"
          }
          timestamp: 101000004
        }
        """),
        query=Path('triggers_packets_test.sql'),
        out=Csv("""
        "ts","name","string_value","int_value"
        101000002,"test1","producer1",3
        101000004,"test2","producer2",4
        """))

  # CPU info
  def test_cpu(self):
    return DiffTestBlueprint(
        trace=Path('cpu_info.textproto'),
        query="""
        SELECT
          cpu,
          cluster_id,
          processor
        FROM cpu;
        """,
        out=Csv("""
        "cpu","cluster_id","processor"
        0,0,"AArch64 Processor rev 13 (aarch64)"
        1,0,"AArch64 Processor rev 13 (aarch64)"
        2,0,"AArch64 Processor rev 13 (aarch64)"
        3,0,"AArch64 Processor rev 13 (aarch64)"
        4,0,"AArch64 Processor rev 13 (aarch64)"
        5,0,"AArch64 Processor rev 13 (aarch64)"
        6,1,"AArch64 Processor rev 13 (aarch64)"
        7,1,"AArch64 Processor rev 13 (aarch64)"
        """))

  def test_cpu_freq(self):
    return DiffTestBlueprint(
        trace=Path('cpu_info.textproto'),
        query="""
        SELECT
          freq,
          GROUP_CONCAT(cpu) AS cpus
        FROM cpu_available_frequencies
        GROUP BY freq
        ORDER BY freq;
        """,
        out=Path('cpu_freq.out'))

  # Trace size
  def test_android_sched_and_ps_trace_size(self):
    return DiffTestBlueprint(
        trace=DataPath('android_sched_and_ps.pb'),
        query="""
        SELECT int_value FROM metadata WHERE name = 'trace_size_bytes';
        """,
        out=Csv("""
        "int_value"
        18761615
        """))

  def test_all_data_source_flushed_metadata(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 12344
          service_event {
            all_data_sources_flushed: true
          }
        }
        packet {
          timestamp: 12345
          service_event {
            all_data_sources_flushed: true
          }
        }
        """),
        query="""
        SELECT name, int_value FROM metadata WHERE name = 'all_data_source_flushed_ns'""",
        out=Csv("""
        "name","int_value"
        "all_data_source_flushed_ns",12344
        "all_data_source_flushed_ns",12345
        """))

  def test_slow_starting_data_sources(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          timestamp: 108227060089867
          trusted_uid: 679634
          trusted_packet_sequence_id: 1
          service_event {
            slow_starting_data_sources {
              data_source {
                producer_name: "producer2"
                data_source_name: "track_event"
              }
              data_source {
                producer_name: "producer3"
                data_source_name: "track_event"
              }
            }
          }
        }
        """),
        query="""
        SELECT str_value FROM metadata WHERE name = 'slow_start_data_source'""",
        out=Csv("""
        "str_value"
        "producer2 track_event"
        "producer3 track_event"
        """))

  def test_cpu_capacity_present(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
      packet {
        cpu_info {
          cpus {
            processor: "AArch64 Processor rev 13 (aarch64)"
            frequencies: 150000
            frequencies: 300000
            capacity: 256
          }
          cpus {
            processor: "AArch64 Processor rev 13 (aarch64)"
            frequencies: 300000
            frequencies: 576000
            capacity: 1024
          }
        }
      }
      """),
        query="""
      SELECT
        cpu,
        cluster_id,
        capacity,
        processor
      FROM cpu
      ORDER BY cpu
      """,
        out=Csv("""
      "cpu","cluster_id","capacity","processor"
      0,0,256,"AArch64 Processor rev 13 (aarch64)"
      1,1,1024,"AArch64 Processor rev 13 (aarch64)"
      """))

  def test_cpu_capacity_not_present(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
      packet {
        cpu_info {
          cpus {
            processor: "AArch64 Processor rev 13 (aarch64)"
            frequencies: 150000
            frequencies: 300000
          }
          cpus {
            processor: "AArch64 Processor rev 13 (aarch64)"
            frequencies: 300000
            frequencies: 576000
          }
        }
      }
      """),
        query="""
      SELECT
        cpu,
        cluster_id,
        capacity,
        processor
      FROM cpu
      ORDER BY cpu
      """,
        out=Csv("""
      "cpu","cluster_id","capacity","processor"
      0,0,"[NULL]","AArch64 Processor rev 13 (aarch64)"
      1,1,"[NULL]","AArch64 Processor rev 13 (aarch64)"
      """))
