/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/dejaview_sql/parser/dejaview_sql_parser.h"

#include <cstdint>
#include <variant>
#include <vector>

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/status_or.h"
#include "src/trace_processor/dejaview_sql/parser/dejaview_sql_test_utils.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "test/gtest_and_gmock.h"

namespace dejaview {
namespace trace_processor {

using Result = DejaViewSqlParser::Statement;
using Statement = DejaViewSqlParser::Statement;
using SqliteSql = DejaViewSqlParser::SqliteSql;
using CreateFn = DejaViewSqlParser::CreateFunction;
using CreateTable = DejaViewSqlParser::CreateTable;
using CreateView = DejaViewSqlParser::CreateView;
using Include = DejaViewSqlParser::Include;
using CreateMacro = DejaViewSqlParser::CreateMacro;
using CreateIndex = DejaViewSqlParser::CreateIndex;

namespace {

class DejaViewSqlParserTest : public ::testing::Test {
 protected:
  base::StatusOr<std::vector<DejaViewSqlParser::Statement>> Parse(
      SqlSource sql) {
    DejaViewSqlParser parser(sql, macros_);
    std::vector<DejaViewSqlParser::Statement> results;
    while (parser.Next()) {
      results.push_back(std::move(parser.statement()));
    }
    if (!parser.status().ok()) {
      return parser.status();
    }
    return results;
  }

  base::FlatHashMap<std::string, DejaViewSqlPreprocessor::Macro> macros_;
};

TEST_F(DejaViewSqlParserTest, Empty) {
  ASSERT_THAT(*Parse(SqlSource::FromExecuteQuery("")), testing::IsEmpty());
}

TEST_F(DejaViewSqlParserTest, SemiColonTerminatedStatement) {
  SqlSource res = SqlSource::FromExecuteQuery("SELECT * FROM slice;");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement{SqliteSql{}});
  ASSERT_EQ(parser.statement_sql(), FindSubstr(res, "SELECT * FROM slice;"));
}

TEST_F(DejaViewSqlParserTest, MultipleStmts) {
  auto res =
      SqlSource::FromExecuteQuery("SELECT * FROM slice; SELECT * FROM s");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement{SqliteSql{}});
  ASSERT_EQ(parser.statement_sql().sql(),
            FindSubstr(res, "SELECT * FROM slice;").sql());
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement{SqliteSql{}});
  ASSERT_EQ(parser.statement_sql().sql(),
            FindSubstr(res, "SELECT * FROM s").sql());
}

TEST_F(DejaViewSqlParserTest, IgnoreOnlySpace) {
  auto res = SqlSource::FromExecuteQuery(" ; SELECT * FROM s; ; ;");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement{SqliteSql{}});
  ASSERT_EQ(parser.statement_sql().sql(),
            FindSubstr(res, "SELECT * FROM s;").sql());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewFunctionScalar) {
  auto res = SqlSource::FromExecuteQuery(
      "create dejaview function foo() returns INT as select 1");
  ASSERT_THAT(*Parse(res), testing::ElementsAre(CreateFn{
                               false, FunctionPrototype{"foo", {}}, "INT",
                               FindSubstr(res, "select 1"), false}));

  res = SqlSource::FromExecuteQuery(
      "create dejaview function bar(x INT, y LONG) returns STRING as "
      "select 'foo'");
  ASSERT_THAT(*Parse(res),
              testing::ElementsAre(
                  CreateFn{false,
                           FunctionPrototype{
                               "bar",
                               {
                                   {"$x", sql_argument::Type::kInt},
                                   {"$y", sql_argument::Type::kLong},
                               },
                           },
                           "STRING", FindSubstr(res, "select 'foo'"), false}));

  res = SqlSource::FromExecuteQuery(
      "CREATE dejaview FuNcTiOn bar(x INT, y LONG) returnS STRING As "
      "select 'foo'");
  ASSERT_THAT(*Parse(res),
              testing::ElementsAre(
                  CreateFn{false,
                           FunctionPrototype{
                               "bar",
                               {
                                   {"$x", sql_argument::Type::kInt},
                                   {"$y", sql_argument::Type::kLong},
                               },
                           },
                           "STRING", FindSubstr(res, "select 'foo'"), false}));
}

TEST_F(DejaViewSqlParserTest, CreateOrReplaceDejaViewFunctionScalar) {
  auto res = SqlSource::FromExecuteQuery(
      "create or replace dejaview function foo() returns INT as select 1");
  ASSERT_THAT(*Parse(res), testing::ElementsAre(CreateFn{
                               true, FunctionPrototype{"foo", {}}, "INT",
                               FindSubstr(res, "select 1"), false}));
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewFunctionScalarError) {
  auto res = SqlSource::FromExecuteQuery(
      "create dejaview function foo( returns INT as select 1");
  ASSERT_FALSE(Parse(res).status().ok());

  res = SqlSource::FromExecuteQuery(
      "create dejaview function foo(x INT) as select 1");
  ASSERT_FALSE(Parse(res).status().ok());

  res = SqlSource::FromExecuteQuery(
      "create dejaview function foo(x INT) returns INT");
  ASSERT_FALSE(Parse(res).status().ok());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewFunctionAndOther) {
  auto res = SqlSource::FromExecuteQuery(
      "create dejaview function foo() returns INT as select 1; select foo()");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  CreateFn fn{false, FunctionPrototype{"foo", {}}, "INT",
              FindSubstr(res, "select 1"), false};
  ASSERT_EQ(parser.statement(), Statement{fn});
  ASSERT_EQ(
      parser.statement_sql().sql(),
      FindSubstr(res, "create dejaview function foo() returns INT as select 1")
          .sql());
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement{SqliteSql{}});
  ASSERT_EQ(parser.statement_sql().sql(),
            FindSubstr(res, "select foo()").sql());
}

TEST_F(DejaViewSqlParserTest, IncludeDejaViewTrivial) {
  auto res =
      SqlSource::FromExecuteQuery("include dejaview module cheese.bre_ad;");
  ASSERT_THAT(*Parse(res), testing::ElementsAre(Include{"cheese.bre_ad"}));
}

TEST_F(DejaViewSqlParserTest, IncludeDejaViewErrorAdditionalChars) {
  auto res = SqlSource::FromExecuteQuery(
      "include dejaview module cheese.bre_ad blabla;");
  ASSERT_FALSE(Parse(res).status().ok());
}

TEST_F(DejaViewSqlParserTest, IncludeDejaViewErrorWrongModuleName) {
  auto res =
      SqlSource::FromExecuteQuery("include dejaview module chees*e.bre_ad;");
  ASSERT_FALSE(Parse(res).status().ok());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewMacro) {
  auto res = SqlSource::FromExecuteQuery(
      "create dejaview macro foo(a1 Expr, b1 TableOrSubquery,c3_d "
      "TableOrSubquery2 ) returns TableOrSubquery3 as random sql snippet");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(
      parser.statement(),
      Statement(CreateMacro{
          false,
          FindSubstr(res, "foo"),
          {
              {FindSubstr(res, "a1"), FindSubstr(res, "Expr")},
              {FindSubstr(res, "b1"), FindSubstr(res, "TableOrSubquery")},
              {FindSubstr(res, "c3_d"), FindSubstr(res, "TableOrSubquery2")},
          },
          FindSubstr(res, "TableOrSubquery3"),
          FindSubstr(res, "random sql snippet")}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateOrReplaceDejaViewMacro) {
  auto res = SqlSource::FromExecuteQuery(
      "create or replace dejaview macro foo() returns Expr as 1");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(CreateMacro{true,
                                                      FindSubstr(res, "foo"),
                                                      {},
                                                      FindSubstr(res, "Expr"),
                                                      FindSubstr(res, "1")}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewMacroAndOther) {
  auto res = SqlSource::FromExecuteQuery(
      "create dejaview macro foo() returns sql1 as random sql snippet; "
      "select 1");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(CreateMacro{
                                    false,
                                    FindSubstr(res, "foo"),
                                    {},
                                    FindSubstr(res, "sql1"),
                                    FindSubstr(res, "random sql snippet"),
                                }));
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(SqliteSql{}));
  ASSERT_EQ(parser.statement_sql(), FindSubstr(res, "select 1"));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewTable) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW TABLE foo AS SELECT 42 AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(),
            Statement(CreateTable{
                false, "foo", FindSubstr(res, "SELECT 42 AS bar"), {}}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateOrReplaceDejaViewTable) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE OR REPLACE DEJAVIEW TABLE foo AS SELECT 42 AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(),
            Statement(CreateTable{
                true, "foo", FindSubstr(res, "SELECT 42 AS bar"), {}}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewTableWithSchema) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW TABLE foo(bar INT) AS SELECT 42 AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(CreateTable{
                                    false,
                                    "foo",
                                    FindSubstr(res, "SELECT 42 AS bar"),
                                    {{"$bar", sql_argument::Type::kInt}},
                                }));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewTableAndOther) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW TABLE foo AS SELECT 42 AS bar; select 1");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(),
            Statement(CreateTable{
                false, "foo", FindSubstr(res, "SELECT 42 AS bar"), {}}));
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(SqliteSql{}));
  ASSERT_EQ(parser.statement_sql(), FindSubstr(res, "select 1"));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewView) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW VIEW foo AS SELECT 42 AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(
      parser.statement(),
      Statement(CreateView{
          false,
          "foo",
          SqlSource::FromExecuteQuery("SELECT 42 AS bar"),
          SqlSource::FromExecuteQuery("CREATE VIEW foo AS SELECT 42 AS bar"),
          {}}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateOrReplaceDejaViewView) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE OR REPLACE DEJAVIEW VIEW foo AS SELECT 42 AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(
      parser.statement(),
      Statement(CreateView{
          true,
          "foo",
          SqlSource::FromExecuteQuery("SELECT 42 AS bar"),
          SqlSource::FromExecuteQuery("CREATE VIEW foo AS SELECT 42 AS bar"),
          {}}));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewViewAndOther) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW VIEW foo AS SELECT 42 AS bar; select 1");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(
      parser.statement(),
      Statement(CreateView{
          false,
          "foo",
          SqlSource::FromExecuteQuery("SELECT 42 AS bar"),
          SqlSource::FromExecuteQuery("CREATE VIEW foo AS SELECT 42 AS bar"),
          {}}));
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(), Statement(SqliteSql{}));
  ASSERT_EQ(parser.statement_sql(), FindSubstr(res, "select 1"));
  ASSERT_FALSE(parser.Next());
}

TEST_F(DejaViewSqlParserTest, CreateDejaViewViewWithSchema) {
  auto res = SqlSource::FromExecuteQuery(
      "CREATE DEJAVIEW VIEW foo(foo STRING, bar INT) AS SELECT 'a' as foo, 42 "
      "AS bar");
  DejaViewSqlParser parser(res, macros_);
  ASSERT_TRUE(parser.Next());
  ASSERT_EQ(parser.statement(),
            Statement(CreateView{
                false,
                "foo",
                SqlSource::FromExecuteQuery("SELECT 'a' as foo, 42 AS bar"),
                SqlSource::FromExecuteQuery(
                    "CREATE VIEW foo AS SELECT 'a' as foo, 42 AS bar"),
                {{"$foo", sql_argument::Type::kString},
                 {"$bar", sql_argument::Type::kInt}},
            }));
  ASSERT_FALSE(parser.Next());
}

}  // namespace
}  // namespace trace_processor
}  // namespace dejaview
