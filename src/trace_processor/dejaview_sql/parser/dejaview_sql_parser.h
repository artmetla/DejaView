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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_PARSER_DEJAVIEW_SQL_PARSER_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_PARSER_DEJAVIEW_SQL_PARSER_H_

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "dejaview/ext/base/flat_hash_map.h"
#include "src/trace_processor/dejaview_sql/parser/function_util.h"
#include "src/trace_processor/dejaview_sql/preprocessor/dejaview_sql_preprocessor.h"
#include "src/trace_processor/dejaview_sql/tokenizer/sqlite_tokenizer.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "src/trace_processor/util/sql_argument.h"

namespace dejaview {
namespace trace_processor {

// Parser for DejaViewSQL statements. This class provides an iterator-style
// interface for reading all DejaViewSQL statements from a block of SQL.
//
// Usage:
// DejaViewSqlParser parser(my_sql_string.c_str());
// while (parser.Next()) {
//   auto& stmt = parser.statement();
//   // Handle |stmt| here
// }
// RETURN_IF_ERROR(r.status());
class DejaViewSqlParser {
 public:
  // Indicates that the specified SQLite SQL was extracted directly from a
  // DejaViewSQL statement and should be directly executed with SQLite.
  struct SqliteSql {};
  // Indicates that the specified SQL was a CREATE DEJAVIEW FUNCTION statement
  // with the following parameters.
  struct CreateFunction {
    bool replace;
    FunctionPrototype prototype;
    std::string returns;
    SqlSource sql;
    bool is_table;
  };
  // Indicates that the specified SQL was a CREATE DEJAVIEW TABLE statement
  // with the following parameters.
  struct CreateTable {
    bool replace;
    std::string name;
    // SQL source for the select statement.
    SqlSource sql;
    std::vector<sql_argument::ArgumentDefinition> schema;
  };
  // Indicates that the specified SQL was a CREATE DEJAVIEW VIEW statement
  // with the following parameters.
  struct CreateView {
    bool replace;
    std::string name;
    // SQL source for the select statement.
    SqlSource select_sql;
    // SQL source corresponding to the rewritten statement creating the
    // underlying view.
    SqlSource create_view_sql;
    std::vector<sql_argument::ArgumentDefinition> schema;
  };
  // Indicates that the specified SQL was a CREATE DEJAVIEW INDEX statement
  // with the following parameters.
  struct CreateIndex {
    bool replace = false;
    std::string name;
    std::string table_name;
    std::vector<std::string> col_names;
  };
  // Indicates that the specified SQL was a DROP DEJAVIEW INDEX statement
  // with the following parameters.
  struct DropIndex {
    std::string name;
    std::string table_name;
  };
  // Indicates that the specified SQL was a INCLUDE DEJAVIEW MODULE statement
  // with the following parameter.
  struct Include {
    std::string key;
  };
  // Indicates that the specified SQL was a CREATE DEJAVIEW MACRO statement
  // with the following parameter.
  struct CreateMacro {
    bool replace;
    SqlSource name;
    std::vector<std::pair<SqlSource, SqlSource>> args;
    SqlSource returns;
    SqlSource sql;
  };
  using Statement = std::variant<CreateFunction,
                                 CreateIndex,
                                 CreateMacro,
                                 CreateTable,
                                 CreateView,
                                 DropIndex,
                                 Include,
                                 SqliteSql>;

  // Creates a new SQL parser with the a block of DejaViewSQL statements.
  // Concretely, the passed string can contain >1 statement.
  explicit DejaViewSqlParser(
      SqlSource,
      const base::FlatHashMap<std::string, DejaViewSqlPreprocessor::Macro>&);

  // Attempts to parse to the next statement in the SQL. Returns true if
  // a statement was successfully parsed and false if EOF was reached or the
  // statement was not parsed correctly.
  //
  // Note: if this function returns false, callers *must* call |status()|: it
  // is undefined behaviour to not do so.
  bool Next();

  // Returns the current statement which was parsed. This function *must not* be
  // called unless |Next()| returned true.
  Statement& statement() {
    DEJAVIEW_DCHECK(statement_.has_value());
    return statement_.value();
  }

  // Returns the full statement which was parsed. This should return
  // |statement()| and DejaView SQL code that's in front. This function *must
  // not* be called unless |Next()| returned true.
  const SqlSource& statement_sql() const {
    DEJAVIEW_CHECK(statement_sql_);
    return *statement_sql_;
  }

  // Returns the error status for the parser. This will be |base::OkStatus()|
  // until an unrecoverable error is encountered.
  const base::Status& status() const { return status_; }

 private:
  // This cannot be moved because we keep pointers into |sql_| in
  // |preprocessor_|.
  DejaViewSqlParser(DejaViewSqlParser&&) = delete;
  DejaViewSqlParser& operator=(DejaViewSqlParser&&) = delete;

  // Most of the code needs sql_argument::ArgumentDefinition, but we explcitly
  // track raw arguments separately, as macro implementations need access to
  // the underlying tokens.
  struct RawArgument {
    SqliteTokenizer::Token name;
    SqliteTokenizer::Token type;
  };

  bool ParseCreateDejaViewFunction(
      bool replace,
      SqliteTokenizer::Token first_non_space_token);

  enum class TableOrView {
    kTable,
    kView,
  };
  bool ParseCreateDejaViewTableOrView(
      bool replace,
      SqliteTokenizer::Token first_non_space_token,
      TableOrView table_or_view);

  bool ParseIncludeDejaViewModule(SqliteTokenizer::Token first_non_space_token);

  bool ParseCreateDejaViewMacro(bool replace);

  bool ParseCreateDejaViewIndex(bool replace,
                                SqliteTokenizer::Token first_non_space_token);

  bool ParseDropDejaViewIndex(SqliteTokenizer::Token first_non_space_token);

  // Convert a "raw" argument (i.e. one that points to specific tokens) to the
  // argument definition consumed by the rest of the SQL code.
  // Guarantees to call ErrorAtToken if std::nullopt is returned.
  std::optional<sql_argument::ArgumentDefinition> ResolveRawArgument(
      RawArgument arg);
  // Parse the arguments in their raw token form.
  bool ParseRawArguments(std::vector<RawArgument>&);
  // Same as above, but also convert the raw tokens into argument definitions.
  bool ParseArguments(std::vector<sql_argument::ArgumentDefinition>&);

  bool ErrorAtToken(const SqliteTokenizer::Token&, const char* error, ...);

  DejaViewSqlPreprocessor preprocessor_;
  SqliteTokenizer tokenizer_;

  base::Status status_;
  std::optional<SqlSource> statement_sql_;
  std::optional<Statement> statement_;
};

}  // namespace trace_processor
}  // namespace dejaview

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_PARSER_DEJAVIEW_SQL_PARSER_H_
