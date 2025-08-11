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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_DESCENDANT_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_DESCENDANT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dejaview/ext/base/status_or.h"
#include "dejaview/trace_processor/basic_types.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/dejaview_sql/intrinsics/table_functions/static_table_function.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/slice_tables_py.h"

namespace dejaview::trace_processor {

class TraceProcessorContext;

// Implements the following dynamic tables:
// * descendant_slice
// * descendant_slice_by_stack
//
// See docs/analysis/trace-processor for usage.
class Descendant : public StaticTableFunction {
 public:
  enum class Type { kSlice = 1, kSliceByStack = 2 };

  Descendant(Type type, const TraceStorage*);

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  base::StatusOr<std::unique_ptr<Table>> ComputeTable(
      const std::vector<SqlValue>& arguments) override;

  // Returns a vector of slice rows which are descendants of |slice_id|. Returns
  // std::nullopt if an invalid |slice_id| is given. This is used by
  // ConnectedFlow to traverse flow indirectly connected flow events.
  static std::optional<std::vector<tables::SliceTable::RowNumber>>
  GetDescendantSlices(const tables::SliceTable& slices, SliceId slice_id);

 private:
  Type type_;
  const TraceStorage* storage_ = nullptr;
};

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_DESCENDANT_H_
