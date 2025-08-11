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

from python.generators.diff_tests.testing import DataPath, TextProto
from python.generators.diff_tests.testing import Csv
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class DejaViewFiltering(TestSuite):
  # Below comparison tests are based on
  # https://www.sqlite.org/datatype3.html#comparisons.

  def test_string_null_vs_empty(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT 0 as id, NULL AS strings
        UNION ALL
        SELECT 1, 'cheese'
        UNION ALL
        SELECT 2, NULL
        UNION ALL
        SELECT 3, '';

        SELECT * FROM foo ORDER BY strings ASC;
        """,
        out=Csv("""
        "id","strings"
        0,"[NULL]"
        2,"[NULL]"
        3,""
        1,"cheese"
        """))

  def test_string_null_vs_empty_desc(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT 0 as id, NULL AS strings
        UNION ALL
        SELECT 1, 'cheese'
        UNION ALL
        SELECT 2, NULL
        UNION ALL
        SELECT 3, '';

        SELECT * FROM foo ORDER BY strings DESC;
        """,
        out=Csv("""
        "id","strings"
        1,"cheese"
        3,""
        0,"[NULL]"
        2,"[NULL]"
        """))

  def test_like_limit_one(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT 'foo' AS strings
        UNION ALL
        SELECT 'binder x'
        UNION ALL
        SELECT 'binder y'
        UNION ALL
        SELECT 'bar';

        SELECT * FROM foo WHERE strings LIKE '%binder%' LIMIT 1;
        """,
        out=Csv("""
        "strings"
        "binder x"
        """))

  def test_like_limit_multiple(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        query="""
        CREATE DEJAVIEW TABLE foo AS
        SELECT 'foo' AS strings
        UNION ALL
        SELECT 'binder x'
        UNION ALL
        SELECT 'binder y'
        UNION ALL
        SELECT 'bar'
        UNION ALL
        SELECT 'binder z';

        SELECT * FROM foo WHERE strings LIKE '%binder%' LIMIT 2;
        """,
        out=Csv("""
        "strings"
        "binder x"
        "binder y"
        """))
