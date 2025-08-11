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

#ifndef SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_GRAPH_TRAVERSAL_H_
#define SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_GRAPH_TRAVERSAL_H_

#include "dejaview/base/status.h"

namespace dejaview::trace_processor {

class DejaViewSqlEngine;
class StringPool;

// Registers the following array related functions with SQLite:
//  * __intrinsic_dfs: a scalar function which performs a DFS traversal
//    of the graph.
//  * __intrinsic_bfs: a scalar function which performs a BFS traversal
//    of the graph.
// TODO(lalitm): once we have some stability here, expand the comments
// here.
base::Status RegisterGraphTraversalFunctions(DejaViewSqlEngine& engine,
                                             StringPool& string_pool);

}  // namespace dejaview::trace_processor

#endif  // SRC_TRACE_PROCESSOR_DEJAVIEW_SQL_INTRINSICS_FUNCTIONS_GRAPH_TRAVERSAL_H_
