/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_EXPERIMENTAL_SCHED_UPID_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_EXPERIMENTAL_SCHED_UPID_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dejaview/ext/base/status_or.h"
#include "dejaview/trace_processor/basic_types.h"
#include "src/trace_processor/db/column_storage.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/dejaview_sql/intrinsics/table_functions/static_table_function.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/metadata_tables_py.h"
#include "src/trace_processor/tables/sched_tables_py.h"

namespace dejaview::trace_processor {

class ExperimentalSchedUpid : public StaticTableFunction {
 public:
  ExperimentalSchedUpid(const tables::SchedSliceTable&,
                        const tables::ThreadTable&);
  virtual ~ExperimentalSchedUpid() override;

  Table::Schema CreateSchema() override;
  std::string TableName() override;
  uint32_t EstimateRowCount() override;
  base::StatusOr<std::unique_ptr<Table>> ComputeTable(
      const std::vector<SqlValue>& arguments) override;

 private:
  ColumnStorage<std::optional<UniquePid>> ComputeUpidColumn();

  const tables::SchedSliceTable* sched_slice_table_;
  const tables::ThreadTable* thread_table_;
  std::unique_ptr<Table> sched_upid_table_;
};

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_TABLE_FUNCTIONS_EXPERIMENTAL_SCHED_UPID_H_
