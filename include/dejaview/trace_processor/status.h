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

#ifndef INCLUDE_DEJAVIEW_TRACE_PROCESSOR_STATUS_H_
#define INCLUDE_DEJAVIEW_TRACE_PROCESSOR_STATUS_H_

#include "dejaview/base/status.h"

// Once upon a time Status used to live in dejaview::trace_processor. At some
// point it has been moved up to base. This forwarding header stayed here
// because of out-of-repo users.

namespace dejaview {
namespace trace_processor {
namespace util {

using Status = ::dejaview::base::Status;

constexpr auto OkStatus = ::dejaview::base::OkStatus;
constexpr auto ErrStatus = ::dejaview::base::ErrStatus;

}  // namespace util
}  // namespace trace_processor
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_TRACE_PROCESSOR_STATUS_H_
