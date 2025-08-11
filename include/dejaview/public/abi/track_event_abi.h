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

#ifndef INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_ABI_H_
#define INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_ABI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dejaview/public/abi/atomic.h"
#include "dejaview/public/abi/data_source_abi.h"
#include "dejaview/public/abi/export.h"

#ifdef __cplusplus
extern "C" {
#endif

DEJAVIEW_SDK_EXPORT void DejaViewTeInit(void);

// The attributes of a single category.
struct DejaViewTeCategoryDescriptor {
  // The category name. Null terminated string.
  const char* name;
  // A human readable string shown by the tracing service when listing the data
  // sources. Null terminated string.
  const char* desc;
  // List of tags, can be null if num_tags is 0. Array of pointers to null
  // terminated strings.
  const char** tags;
  // Number of elements in the `tags` array.
  size_t num_tags;
};

// Opaque pointer to a registered category.
struct DejaViewTeCategoryImpl;

// An already registered category that's considered enabled if the track event
// data source is enabled. Useful for dynamic categories.
extern DEJAVIEW_SDK_EXPORT struct DejaViewTeCategoryImpl*
    dejaview_te_any_categories;

// Points to true if the track event data source is enabled.
extern DEJAVIEW_SDK_EXPORT DEJAVIEW_ATOMIC(bool) *
    dejaview_te_any_categories_enabled;

// Registers a category.
//
// `desc` (and all the objects pointed by it) need to be alive until
// DejaViewTeCategoryImplDestroy() is called.
DEJAVIEW_SDK_EXPORT struct DejaViewTeCategoryImpl* DejaViewTeCategoryImplCreate(
    struct DejaViewTeCategoryDescriptor* desc);

// Tells the tracing service about newly registered categories. Must be called
// after one or more call to DejaViewTeCategoryImplCreate() or
// DejaViewTeCategoryImplDestroy().
DEJAVIEW_SDK_EXPORT void DejaViewTePublishCategories(void);

// Returns a pointer to a boolean that tells if the category is enabled or not.
// The pointer is valid until the category is destroyed.
DEJAVIEW_SDK_EXPORT DEJAVIEW_ATOMIC(bool) *
    DejaViewTeCategoryImplGetEnabled(struct DejaViewTeCategoryImpl*);

// Called when a data source instance is created (if `created` is true) or
// destroyed (if `created` is false) with a registered category enabled.
// `global_state_changed` is true if this was the first instance created with
// the category enabled or the last instance destroyed with the category
// enabled.
typedef void (*DejaViewTeCategoryImplCallback)(struct DejaViewTeCategoryImpl*,
                                               DejaViewDsInstanceIndex inst_id,
                                               bool created,
                                               bool global_state_changed,
                                               void* user_arg);

// Registers `cb` to be called every time a data source instance with `cat`
// enabled is created or destroyed. `user_arg` will be passed unaltered to `cb`.
//
// `cb` can be NULL to disable the callback.
DEJAVIEW_SDK_EXPORT void DejaViewTeCategoryImplSetCallback(
    struct DejaViewTeCategoryImpl* cat,
    DejaViewTeCategoryImplCallback cb,
    void* user_arg);

// Returns the interning id (iid) associated with the registered category `cat`.
DEJAVIEW_SDK_EXPORT uint64_t
DejaViewTeCategoryImplGetIid(struct DejaViewTeCategoryImpl* cat);

// Destroys a previously registered category. The category cannot be used for
// tracing anymore after this.
DEJAVIEW_SDK_EXPORT void DejaViewTeCategoryImplDestroy(
    struct DejaViewTeCategoryImpl*);

enum DejaViewTeTimestampType {
  DEJAVIEW_TE_TIMESTAMP_TYPE_MONOTONIC = 3,
  DEJAVIEW_TE_TIMESTAMP_TYPE_BOOT = 6,
  DEJAVIEW_TE_TIMESTAMP_TYPE_INCREMENTAL = 64,
  DEJAVIEW_TE_TIMESTAMP_TYPE_ABSOLUTE = 65,
};

enum {
#ifdef __linux__
  DEJAVIEW_I_CLOCK_INCREMENTAL_UNDERNEATH = DEJAVIEW_TE_TIMESTAMP_TYPE_BOOT,
#else
  DEJAVIEW_I_CLOCK_INCREMENTAL_UNDERNEATH =
      DEJAVIEW_TE_TIMESTAMP_TYPE_MONOTONIC,
#endif
};

struct DejaViewTeTimestamp {
  // DejaViewTeTimestampType
  uint32_t clock_id;
  uint64_t value;
};

// Returns the current timestamp.
DEJAVIEW_SDK_EXPORT struct DejaViewTeTimestamp DejaViewTeGetTimestamp(void);

struct DejaViewTeRegisteredTrackImpl {
  void* descriptor;  // Owned (malloc).
  size_t descriptor_size;
  uint64_t uuid;
};

// The UUID of the process track for the current process.
extern DEJAVIEW_SDK_EXPORT uint64_t dejaview_te_process_track_uuid;

// The type of an event.
enum DejaViewTeType {
  DEJAVIEW_TE_TYPE_SLICE_BEGIN = 1,
  DEJAVIEW_TE_TYPE_SLICE_END = 2,
  DEJAVIEW_TE_TYPE_INSTANT = 3,
  DEJAVIEW_TE_TYPE_COUNTER = 4,
};

#ifdef __cplusplus
}
#endif

#endif  // INCLUDE_DEJAVIEW_PUBLIC_ABI_TRACK_EVENT_ABI_H_
