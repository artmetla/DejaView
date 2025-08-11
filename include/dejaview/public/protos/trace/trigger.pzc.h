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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_PROTOS_TRACE_TRIGGER_PZC_H_
#define INCLUDE_DEJAVIEW_PUBLIC_PROTOS_TRACE_TRIGGER_PZC_H_

#include <stdbool.h>
#include <stdint.h>

#include "dejaview/public/pb_macros.h"

DEJAVIEW_PB_MSG(dejaview_protos_Trigger);
DEJAVIEW_PB_FIELD(dejaview_protos_Trigger,
                  STRING,
                  const char*,
                  trigger_name,
                  1);
DEJAVIEW_PB_FIELD(dejaview_protos_Trigger,
                  STRING,
                  const char*,
                  producer_name,
                  2);
DEJAVIEW_PB_FIELD(dejaview_protos_Trigger,
                  VARINT,
                  int32_t,
                  trusted_producer_uid,
                  3);

#endif  // INCLUDE_DEJAVIEW_PUBLIC_PROTOS_TRACE_TRIGGER_PZC_H_
