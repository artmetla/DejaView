/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_TYPE_BUILDERS_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_TYPE_BUILDERS_H_

#include "dejaview/base/status.h"

namespace dejaview::trace_processor {

class DejaViewSqlEngine;
class StringPool;

// Registers the following DejaViewSQL type related functions with SQLite:
//  * __intrinsic_graph_agg: an aggregate function which builds a graph.
//  * __intrinsic_array_agg: an aggregate function which allows building
//    arrays from tables.
//  * __intrinsic_struct: a scalar function which allows creating a
//    struct from its component fields.
//  * __intrinsic_row_dataframe_agg: an aggregate function which
//    creates a data structure allowing efficient lookups of rows by id.
// TODO(lalitm): once we have some stability here, expand the comments
// here.
base::Status RegisterTypeBuilderFunctions(DejaViewSqlEngine& engine);

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_TYPE_BUILDERS_H_
