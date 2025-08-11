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


class Tables(TestSuite):

  # Null printing
  def test_nulls(self):
    return DiffTestBlueprint(
        trace=Path('../common/synth_1.py'),
        query="""
        CREATE TABLE null_test (
          primary_key INTEGER PRIMARY KEY,
          int_nulls INTEGER,
          string_nulls STRING,
          double_nulls DOUBLE,
          start_int_nulls INTEGER,
          start_string_nulls STRING,
          start_double_nulls DOUBLE,
          all_nulls INTEGER
        );

        INSERT INTO null_test(
          int_nulls,
          string_nulls,
          double_nulls,
          start_int_nulls,
          start_string_nulls,
          start_double_nulls
        )
        VALUES
        (1, "test", 2.0, NULL, NULL, NULL),
        (2, NULL, NULL, NULL, "test", NULL),
        (1, "other", NULL, NULL, NULL, NULL),
        (4, NULL, NULL, NULL, NULL, 1.0),
        (NULL, "test", 1.0, 1, NULL, NULL);

        SELECT * FROM null_test;
        """,
        out=Path('nulls.out'))

  # Ftrace stats imports in metadata and stats tables
  def test_filter_stats(self):
    return DiffTestBlueprint(
        trace=TextProto("""
          packet { trace_stats{ filter_stats {
            input_packets: 836
            input_bytes: 25689644
            output_bytes: 24826981
            errors: 12
            time_taken_ns: 1228178548
            bytes_discarded_per_buffer: 1
            bytes_discarded_per_buffer: 34
            bytes_discarded_per_buffer: 29
            bytes_discarded_per_buffer: 0
            bytes_discarded_per_buffer: 862588
          }}}"""),
        query="""
        SELECT name, value FROM stats
        WHERE name like 'filter_%' OR name = 'traced_buf_bytes_filtered_out'
        ORDER by name ASC
        """,
        out=Csv("""
        "name","value"
        "filter_errors",12
        "filter_input_bytes",25689644
        "filter_input_packets",836
        "filter_output_bytes",24826981
        "filter_time_taken_ns",1228178548
        "traced_buf_bytes_filtered_out",1
        "traced_buf_bytes_filtered_out",34
        "traced_buf_bytes_filtered_out",29
        "traced_buf_bytes_filtered_out",0
        "traced_buf_bytes_filtered_out",862588
        """))

  def test_metadata(self):
    return DiffTestBlueprint(
        trace=TextProto(r"""
        packet {
          system_info {
            tracing_service_version: "DejaView v38.0-0bb49ab54 (0bb49ab54dbe55ce5b9dfea3a2ada68b87aecb65)"
            timezone_off_mins: 60
            utsname {
              sysname: "Darwin"
              version: "Foobar"
              machine: "x86_64"
              release: "22.6.0"
            }
          }
          trusted_uid: 158158
          trusted_packet_sequence_id: 1
        }
        """),
        query=r"""SELECT name, COALESCE(str_value, int_value) as val
              FROM metadata
              WHERE name IN (
                  "system_name", "system_version", "system_machine",
                  "system_release", "timezone_off_mins")
              ORDER BY name
        """,
        out=Csv(r"""
                "name","val"
                "system_machine","x86_64"
                "system_name","Darwin"
                "system_release","22.6.0"
                "system_version","Foobar"
                "timezone_off_mins",60
                """))

  def test_flow_table_trace_id(self):
    return DiffTestBlueprint(
        trace=TextProto("""
          packet {
            timestamp: 0
            track_event {
              name: "Track 0 Event"
              type: TYPE_SLICE_BEGIN
              track_uuid: 10
              flow_ids: 57
            }
            trusted_packet_sequence_id: 123
          }
          packet {
            timestamp: 10
            track_event {
              name: "Track 0 Nested Event"
              type: TYPE_SLICE_BEGIN
              track_uuid: 10
              flow_ids: 57
            }
            trusted_packet_sequence_id: 123
          }
          packet {
            timestamp: 50
            track_event {
              name: "Track 0 Short Event"
              type: TYPE_SLICE_BEGIN
              track_uuid: 10
              terminating_flow_ids: 57
            }
            trusted_packet_sequence_id: 123
          }
        """),
        query="SELECT * FROM flow;",
        out=Csv("""
          "id","type","slice_out","slice_in","trace_id","arg_set_id"
          0,"flow",0,1,57,0
          1,"flow",1,2,57,0
                """))

  def test_clock_snapshot_table_multiplier(self):
    return DiffTestBlueprint(
        trace=TextProto("""
          packet {
            clock_snapshot {
              clocks {
                clock_id: 1
                timestamp: 42
                unit_multiplier_ns: 10
              }
              clocks {
                clock_id: 6
                timestamp: 0
              }
            }
          }
        """),
        query="SELECT TO_REALTIME(0);",
        out=Csv("""
          "TO_REALTIME(0)"
          420
        """))
