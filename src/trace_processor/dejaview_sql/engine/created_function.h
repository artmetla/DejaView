/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_ENGINE_CREATED_FUNCTION_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_ENGINE_CREATED_FUNCTION_H_

#include <sqlite3.h>
#include <cstddef>
#include <memory>

#include "dejaview/base/status.h"
#include "dejaview/trace_processor/basic_types.h"
#include "src/trace_processor/dejaview_sql/intrinsics/functions/sql_function.h"
#include "src/trace_processor/dejaview_sql/parser/function_util.h"
#include "src/trace_processor/sqlite/sql_source.h"
#include "src/trace_processor/types/destructible.h"
#include "src/trace_processor/util/sql_argument.h"

namespace dejaview::trace_processor {

class DejaViewSqlEngine;

struct CreatedFunction : public SqlFunction {
  // Expose a do-nothing context
  using Context = Destructible;

  // SqlFunction implementation.
  static base::Status Run(Context* ctx,
                          size_t argc,
                          sqlite3_value** argv,
                          SqlValue& out,
                          Destructors&);
  static base::Status VerifyPostConditions(Context*);
  static void Cleanup(Context*);

  // Glue code for DejaViewSqlEngine.
  static std::unique_ptr<Context> MakeContext(DejaViewSqlEngine*);
  static bool IsValid(Context*);
  static void Reset(Context*, DejaViewSqlEngine*);
  static base::Status Prepare(Context*,
                              FunctionPrototype,
                              sql_argument::Type return_type,
                              SqlSource sql);
  static base::Status EnableMemoization(Context*);
};

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_ENGINE_CREATED_FUNCTION_H_
