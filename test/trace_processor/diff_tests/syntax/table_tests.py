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


class DejaViewTable(TestSuite):

  def test_create_table(self):
    return DiffTestBlueprint(
        trace=TextProto(r''),
        query="""
        CREATE DEJAVIEW TABLE foo AS SELECT 42 as a;

        SELECT * FROM foo;
        """,
        out=Csv("""
        "a"
        42
        """))

  def test_replace_table(self):
    return DiffTestBlueprint(
        trace=TextProto(r''),
        query="""
        CREATE DEJAVIEW TABLE foo AS SELECT 42 as a;
        CREATE OR REPLACE DEJAVIEW TABLE foo AS SELECT 43 as a;

        SELECT * FROM foo;
        """,
        out=Csv("""
        "a"
        43
        """))

  def test_dejaview_table_info_static_table(self):
    return DiffTestBlueprint(
        trace=DataPath('android_boot.pftrace'),
        query="""
        SELECT * FROM dejaview_table_info('counter');
        """,
        out=Csv("""
        "id","type","name","col_type","nullable","sorted"
        0,"dejaview_table_info","id","id",0,1
        1,"dejaview_table_info","type","string",0,0
        2,"dejaview_table_info","ts","int64",0,1
        3,"dejaview_table_info","track_id","uint32",0,0
        4,"dejaview_table_info","value","double",0,0
        5,"dejaview_table_info","arg_set_id","uint32",1,0
        """))

  def test_dejaview_table_info_runtime_table(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT * FROM
        (SELECT 2 AS c
        UNION
        SELECT 0 AS c
        UNION
        SELECT 1 AS c)
        ORDER BY c desc;

        SELECT * from dejaview_table_info('foo');
        """,
        out=Csv("""
        "id","type","name","col_type","nullable","sorted"
        0,"dejaview_table_info","c","int64",0,0
        """))

  def test_create_dejaview_table_nullable_column(self):
    return DiffTestBlueprint(
        trace=DataPath('android_boot.pftrace'),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT thread_ts FROM slice
        WHERE thread_ts IS NOT NULL;

        SELECT nullable FROM dejaview_table_info('foo')
        WHERE name = 'thread_ts';
        """,
        out=Csv("""
        "nullable"
        0
        """))

  def test_create_dejaview_table_nullable_column(self):
    return DiffTestBlueprint(
        trace=DataPath('android_boot.pftrace'),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT dur FROM slice ORDER BY dur;

        SELECT sorted FROM dejaview_table_info('foo')
        WHERE name = 'dur';
        """,
        out=Csv("""
        "sorted"
        1
        """))

  def test_create_dejaview_table_id_column(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT 2 AS c
        UNION
        SELECT 4
        UNION
        SELECT 6;

        SELECT col_type FROM dejaview_table_info('foo')
        WHERE name = 'c';
        """,
        out=Csv("""
        "col_type"
        "id"
        """))

  def test_distinct_multi_column(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        WITH data(a, b) AS (
          VALUES
            -- Needed to defeat any id/sorted detection.
            (2, 3),
            (0, 2),
            (0, 1)
        )
        SELECT * FROM data;

        CREATE TABLE bar AS
        SELECT 1 AS b;

        WITH multi_col_distinct AS (
          SELECT DISTINCT a FROM foo CROSS JOIN bar USING (b)
        ), multi_col_group_by AS (
          SELECT a FROM foo CROSS JOIN bar USING (b) GROUP BY a
        )
        SELECT
          (SELECT COUNT(*) FROM multi_col_distinct) AS cnt_distinct,
          (SELECT COUNT(*) FROM multi_col_group_by) AS cnt_group_by
        """,
        out=Csv("""
        "cnt_distinct","cnt_group_by"
        1,1
        """))

  def test_limit(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        WITH data(a, b) AS (
          VALUES
            (0, 1),
            (1, 10),
            (2, 20),
            (3, 30),
            (4, 40),
            (5, 50)
        )
        SELECT * FROM data LIMIT 3;
        """,
        out=Csv("""
        "a","b"
        0,1
        1,10
        2,20
        """))

  def test_limit_and_offset_in_bounds(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        WITH data(a, b) AS (
          VALUES
            (0, 1),
            (1, 10),
            (2, 20),
            (3, 30),
            (4, 40),
            (5, 50),
            (6, 60),
            (7, 70),
            (8, 80),
            (9, 90)
        )
        SELECT * FROM data LIMIT 2 OFFSET 3;
        """,
        out=Csv("""
        "a","b"
        3,30
        4,40
        """))

  def test_limit_and_offset_not_in_bounds(self):
    return DiffTestBlueprint(
        trace=TextProto(''),
        query="""
        WITH data(a, b) AS (
          VALUES
            (0, 1),
            (1, 10),
            (2, 20),
            (3, 30),
            (4, 40),
            (5, 50),
            (6, 60),
            (7, 70),
            (8, 80),
            (9, 90)
        )
        SELECT * FROM data LIMIT 5 OFFSET 6;
        """,
        out=Csv("""
        "a","b"
        6,60
        7,70
        8,80
        9,90
        """))
