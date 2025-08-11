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
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class ParsingMemoryCounters(TestSuite):

  def test_memory_counters_args_string_filter_null(self):
    return DiffTestBlueprint(
        trace=DataPath('memory_counters.pb'),
        query=Path('args_string_filter_null_test.sql'),
        out=Csv("""
        "string_value"
        """))

  def test_memory_counters_args_string_is_null(self):
    return DiffTestBlueprint(
        trace=DataPath('memory_counters.pb'),
        query="""
        SELECT string_value
        FROM args
        WHERE string_value IS NULL
        LIMIT 10;
        """,
        out=Csv("""
        "string_value"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        "[NULL]"
        """))

  def test_global_memory_counter_memory_counters(self):
    return DiffTestBlueprint(
        trace=DataPath('memory_counters.pb'),
        query="""
        SELECT ts, value, name
        FROM counter
        JOIN counter_track ON counter.track_id = counter_track.id
        WHERE name = 'MemAvailable' AND counter_track.type = 'counter_track'
        LIMIT 10;
        """,
        out=Csv("""
        "ts","value","name"
        22240334823167,2696392704.000000,"MemAvailable"
        22240356169836,2696392704.000000,"MemAvailable"
        22240468594483,2696392704.000000,"MemAvailable"
        22240566948190,2696392704.000000,"MemAvailable"
        22240667383304,2696392704.000000,"MemAvailable"
        22240766505085,2696392704.000000,"MemAvailable"
        22240866794106,2696392704.000000,"MemAvailable"
        22240968271928,2696392704.000000,"MemAvailable"
        22241065777407,2696392704.000000,"MemAvailable"
        22241165839708,2696392704.000000,"MemAvailable"
        """))
