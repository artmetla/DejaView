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
import os
import sys
from typing import List

from python.generators.diff_tests import testing

# A hack to import using `diff_tests.` which prevents the risk name conflicts,
# i.e importing a module when user has a different package of the same name
# installed.
TRACE_PROCESSOR_TEST_DIR = os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))
sys.path.append(TRACE_PROCESSOR_TEST_DIR)

from diff_tests.parser.parsing.tests import Parsing
from diff_tests.parser.parsing.tests_debug_annotation import ParsingDebugAnnotation
from diff_tests.parser.parsing.tests_memory_counters import ParsingMemoryCounters
from diff_tests.parser.parsing.tests_sys_stats import ParsingSysStats
from diff_tests.parser.parsing.tests_traced_stats import ParsingTracedStats
from diff_tests.parser.process_tracking.tests import ProcessTracking
from diff_tests.parser.profiling.tests import Profiling
from diff_tests.parser.profiling.tests_heap_graph import ProfilingHeapGraph
from diff_tests.parser.profiling.tests_heap_profiling import ProfilingHeapProfiling
from diff_tests.parser.profiling.tests_llvm_symbolizer import ProfilingLlvmSymbolizer
from diff_tests.parser.track_event.tests import TrackEvent
from diff_tests.stdlib.common.tests import StdlibCommon
from diff_tests.stdlib.dynamic_tables.tests import DynamicTables
from diff_tests.stdlib.intervals.intersect_tests import IntervalsIntersect
from diff_tests.stdlib.intervals.tests import StdlibIntervals
from diff_tests.stdlib.linux.cpu import LinuxCpu
from diff_tests.stdlib.prelude.math_functions_tests import PreludeMathFunctions
from diff_tests.stdlib.prelude.slices_tests import PreludeSlices
from diff_tests.stdlib.prelude.window_functions_tests import PreludeWindowFunctions
from diff_tests.stdlib.slices.tests import Slices
from diff_tests.stdlib.span_join.tests_left_join import SpanJoinLeftJoin
from diff_tests.stdlib.span_join.tests_outer_join import SpanJoinOuterJoin
from diff_tests.stdlib.span_join.tests_regression import SpanJoinRegression
from diff_tests.stdlib.span_join.tests_smoke import SpanJoinSmoke
from diff_tests.stdlib.tests import StdlibSmoke
from diff_tests.stdlib.timestamps.tests import Timestamps
from diff_tests.syntax.filtering_tests import DejaViewFiltering
from diff_tests.syntax.function_tests import DejaViewFunction
from diff_tests.syntax.macro_tests import DejaViewMacro
from diff_tests.syntax.table_function_tests import DejaViewTableFunction
from diff_tests.syntax.table_tests import DejaViewTable
from diff_tests.syntax.view_tests import DejaViewView
from diff_tests.tables.tests import Tables
from diff_tests.tables.tests_counters import TablesCounters

sys.path.pop()


def fetch_all_diff_tests(index_path: str) -> List['testing.TestCase']:
  parser_tests = [
      *ProcessTracking(index_path, 'parser/process_tracking',
                       'ProcessTracking').fetch(),
      *Profiling(index_path, 'parser/profiling', 'Profiling').fetch(),
      *ProfilingHeapGraph(index_path, 'parser/profiling',
                          'ProfilingHeapGraph').fetch(),
      *ProfilingHeapProfiling(index_path, 'parser/profiling',
                              'ProfilingHeapProfiling').fetch(),
      *ProfilingLlvmSymbolizer(index_path, 'parser/profiling',
                               'ProfilingLlvmSymbolizer').fetch(),
      *TrackEvent(index_path, 'parser/track_event', 'TrackEvent').fetch(),
      # TODO(altimin, lalitm): "parsing" should be split into more specific
      # directories.
      *Parsing(index_path, 'parser/parsing', 'Parsing').fetch(),
      *ParsingDebugAnnotation(index_path, 'parser/parsing',
                              'ParsingDebugAnnotation').fetch(),
      *ParsingSysStats(index_path, 'parser/parsing', 'ParsingSysStats').fetch(),
      *ParsingMemoryCounters(index_path, 'parser/parsing',
                             'ParsingMemoryCounters').fetch(),
      *ParsingTracedStats(index_path, 'parser/parsing',
                          'ParsingTracedStats').fetch(),
  ]

  metrics_tests = [
  ]

  stdlib_tests = [
      *LinuxCpu(index_path, 'stdlib/linux/cpu', 'LinuxCpu').fetch(),
      *DynamicTables(index_path, 'stdlib/dynamic_tables',
                     'DynamicTables').fetch(),
      *PreludeMathFunctions(index_path, 'stdlib/prelude',
                            'PreludeMathFunctions').fetch(),
      *PreludeWindowFunctions(index_path, 'stdlib/prelude',
                              'PreludeWindowFunctions').fetch(),
      *PreludeSlices(index_path, 'stdlib/prelude', 'PreludeSlices').fetch(),
      *StdlibSmoke(index_path, 'stdlib', 'StdlibSmoke').fetch(),
      *StdlibCommon(index_path, 'stdlib/common', 'StdlibCommon').fetch(),
      *Slices(index_path, 'stdlib/slices', 'Slices').fetch(),
      *SpanJoinLeftJoin(index_path, 'stdlib/span_join',
                        'SpanJoinLeftJoin').fetch(),
      *SpanJoinOuterJoin(index_path, 'stdlib/span_join',
                         'SpanJoinOuterJoin').fetch(),
      *SpanJoinRegression(index_path, 'stdlib/span_join',
                          'SpanJoinRegression').fetch(),
      *SpanJoinSmoke(index_path, 'stdlib/span_join', 'SpanJoinSmoke').fetch(),
      *StdlibCommon(index_path, 'stdlib/common', 'StdlibCommon').fetch(),
      *StdlibIntervals(index_path, 'stdlib/intervals',
                       'StdlibIntervals').fetch(),
      *IntervalsIntersect(index_path, 'stdlib/intervals',
                          'StdlibIntervalsIntersect').fetch(),
      *Timestamps(index_path, 'stdlib/timestamps', 'Timestamps').fetch(),
  ]

  syntax_tests = [
      *DejaViewFiltering(index_path, 'syntax', 'DejaViewFiltering').fetch(),
      *DejaViewFunction(index_path, 'syntax', 'DejaViewFunction').fetch(),
      *DejaViewMacro(index_path, 'syntax', 'DejaViewMacro').fetch(),
      *DejaViewTable(index_path, 'syntax', 'DejaViewTable').fetch(),
      *DejaViewTableFunction(index_path, 'syntax',
                             'DejaViewTableFunction').fetch(),
      *DejaViewView(index_path, 'syntax', 'DejaViewView').fetch(),
  ]

  return parser_tests + metrics_tests + stdlib_tests + syntax_tests + [
      *Tables(index_path, 'tables', 'Tables').fetch(),
      *TablesCounters(index_path, 'tables', 'TablesCounters').fetch(),
  ]
