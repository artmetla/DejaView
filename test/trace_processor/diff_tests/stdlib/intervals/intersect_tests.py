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

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class IntervalsIntersect(TestSuite):

  def test_simple_interval_intersect(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((A, B), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        1,1,0,0
        3,2,0,1
        6,1,0,2
        """))

  def test_simple_interval_intersect_two_tabs(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((B, A), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        1,1,0,0
        3,2,1,0
        6,1,2,0
        """))

  def test_simple_interval_intersect_three_tabs(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   0 1 1 1 1 1 1 0
        # B:   1 1 0 1 1 0 1 1
        # C:   1 0 1 1 1 1 0 1
        # res: 0 0 0 1 1 0 0 0
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE C AS
          WITH data(id, ts, dur) AS (
            VALUES
            (10, 0, 1),
            (20, 2, 4),
            (30, 7, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1, id_2
        FROM _interval_intersect!((A, B, C), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1","id_2"
        3,2,0,1,20
        """))

  def test_no_overlap(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((A, B), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_overlap_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((B, A), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_empty(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
        SELECT * FROM A LIMIT 0;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((A, B), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_empty_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
        SELECT * FROM A LIMIT 0;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((B, A), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_point_overlap(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   _-
        # B:   -_
        # res: __
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((A, B), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_point_overlap_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   _-
        # B:   -_
        # res: __
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 1)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!((B, A), ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_interval(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE DEJAVIEW TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT *
        FROM _interval_intersect_single!(1, 6, B)
        ORDER BY ts;
        """,
        out=Csv("""
        "id","ts","dur"
        0,1,1
        1,3,2
        2,6,1
        """))

  def test_ii_wrong_partition(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        INCLUDE DEJAVIEW MODULE intervals.intersect;

        CREATE DEJAVIEW TABLE A
        AS
        WITH x(id, ts, dur, c0) AS (VALUES(1, 1, 1, 1), (2, 3, 1, 2))
        SELECT * FROM x;

        CREATE DEJAVIEW TABLE B
        AS
        WITH x(id, ts, dur, c0) AS (VALUES(1, 5, 1, 3))
        SELECT * FROM x;

        SELECT ts FROM _interval_intersect!((A, B), (c0));
        """,
        out=Csv("""
        "ts"
        """))
