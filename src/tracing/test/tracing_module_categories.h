/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef SRC_TRACING_TEST_TRACING_MODULE_CATEGORIES_H_
#define SRC_TRACING_TEST_TRACING_MODULE_CATEGORIES_H_

// This header defines the tracing categories (and track event data source) used
// in the external tracing test module. These categories are distinct from the
// ones defined in api_integrationtest.cc, but events for both sets of
// categories can be written to the same trace writer.

#define DEJAVIEW_ENABLE_LEGACY_TRACE_EVENTS 1

#include "dejaview/tracing.h"

// Note: Using the old syntax here to ensure backwards compatibility.
DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE(tracing_module,
                                        DEJAVIEW_CATEGORY(cat1),
                                        DEJAVIEW_CATEGORY(cat2),
                                        DEJAVIEW_CATEGORY(cat3),
                                        DEJAVIEW_CATEGORY(cat4),
                                        DEJAVIEW_CATEGORY(cat5),
                                        DEJAVIEW_CATEGORY(cat6),
                                        DEJAVIEW_CATEGORY(cat7),
                                        DEJAVIEW_CATEGORY(cat8),
                                        DEJAVIEW_CATEGORY(cat9),
                                        DEJAVIEW_CATEGORY(foo));

// Define a second set of categories in a different namespace with custom
// linkage attributes.
DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(
    tracing_extra,
    [[maybe_unused]],
    dejaview::Category("extra"),
    dejaview::Category("extra2"));

#endif  // SRC_TRACING_TEST_TRACING_MODULE_CATEGORIES_H_
