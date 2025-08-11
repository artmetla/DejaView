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


class TablesCounters(TestSuite):

  def test_counter_dur_example_android_trace_30s(self):
    return DiffTestBlueprint(
        trace=DataPath('example_android_trace_30s.pb'),
        query=Path('counter_dur_test.sql'),
        out=Csv("""
        "ts","dur"
        100351738640,-1
        100351738640,-1
        100351738640,-1
        70731059648,19510835
        70731059648,19510835
        70731059648,19510835
        73727335051,23522762
        73727335051,23522762
        73727335051,23522762
        86726132752,24487554
        """))
