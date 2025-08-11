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

#include "dejaview/public/abi/track_event_abi.h"
#include "dejaview/public/producer.h"
#include "dejaview/public/protos/trace/track_event/track_event.pzc.h"
#include "dejaview/public/te_category_macros.h"
#include "dejaview/public/te_macros.h"
#include "dejaview/public/track_event.h"

#include <stdio.h>
#include <unistd.h>

#define EXAMPLE_CATEGORIES(C)                                   \
  C(rendering, "rendering", "Rendering events", "tag1", "tag2") \
  C(physics, "physics", "Physics events", "tag1")               \
  C(cat, "cat", "Sample category")                              \
  C(c3, "c3", "c3", "tag1", "tag2", "tag3")                     \
  C(c4, "c4", "c4", "tag1", "tag2", "tag3", "tag4")

DEJAVIEW_TE_CATEGORIES_DEFINE(EXAMPLE_CATEGORIES)

static struct DejaViewTeRegisteredTrack mytrack;
static struct DejaViewTeRegisteredTrack mycounter;

static void EnabledCb(struct DejaViewTeCategoryImpl* c,
                      DejaViewDsInstanceIndex inst_id,
                      bool enabled,
                      bool global_state_changed,
                      void* user_arg) {
  printf("Callback: %p id: %u on: %d, global_state_changed: %d, user_arg:%p\n",
         DEJAVIEW_STATIC_CAST(void*, c), inst_id,
         DEJAVIEW_STATIC_CAST(int, enabled),
         DEJAVIEW_STATIC_CAST(int, global_state_changed), user_arg);
  if (enabled) {
    DEJAVIEW_TE(physics, DEJAVIEW_TE_INSTANT("callback"), DEJAVIEW_TE_FLUSH());
  }
}

int main(void) {
  uint64_t flow_counter = 0;
  struct DejaViewProducerInitArgs args = DEJAVIEW_PRODUCER_INIT_ARGS_INIT();
  args.backends = DEJAVIEW_BACKEND_SYSTEM;
  DejaViewProducerInit(args);
  DejaViewTeInit();
  DEJAVIEW_TE_REGISTER_CATEGORIES(EXAMPLE_CATEGORIES);
  DejaViewTeNamedTrackRegister(&mytrack, "mytrack", 0,
                               DejaViewTeProcessTrackUuid());
  DejaViewTeCounterTrackRegister(&mycounter, "mycounter",
                                 DejaViewTeProcessTrackUuid());
  DejaViewTeCategorySetCallback(&physics, EnabledCb, DEJAVIEW_NULL);
  for (;;) {
    DEJAVIEW_TE(rendering, DEJAVIEW_TE_INSTANT("name1"));
    DEJAVIEW_TE(physics, DEJAVIEW_TE_INSTANT("name2"),
                DEJAVIEW_TE_ARG_BOOL("dbg_arg", false),
                DEJAVIEW_TE_ARG_STRING("dbg_arg2", "mystring"));
    DEJAVIEW_TE(cat, DEJAVIEW_TE_SLICE_BEGIN("name"));
    DEJAVIEW_TE(cat, DEJAVIEW_TE_SLICE_END());
    flow_counter++;
    DEJAVIEW_TE(physics, DEJAVIEW_TE_SLICE_BEGIN("name4"),
                DEJAVIEW_TE_REGISTERED_TRACK(&mytrack),
                DEJAVIEW_TE_FLOW(DejaViewTeProcessScopedFlow(flow_counter)));
    DEJAVIEW_TE(physics, DEJAVIEW_TE_SLICE_END(),
                DEJAVIEW_TE_REGISTERED_TRACK(&mytrack));
    DEJAVIEW_TE(cat, DEJAVIEW_TE_INSTANT("name5"),
                DEJAVIEW_TE_TIMESTAMP(DejaViewTeGetTimestamp()));
    DEJAVIEW_TE(DEJAVIEW_TE_DYNAMIC_CATEGORY, DEJAVIEW_TE_INSTANT("name6"),
                DEJAVIEW_TE_DYNAMIC_CATEGORY_STRING("physics"),
                DEJAVIEW_TE_TERMINATING_FLOW(
                    DejaViewTeProcessScopedFlow(flow_counter)));
    DEJAVIEW_TE(physics, DEJAVIEW_TE_COUNTER(),
                DEJAVIEW_TE_REGISTERED_TRACK(&mycounter),
                DEJAVIEW_TE_INT_COUNTER(79));
    DEJAVIEW_TE(physics, DEJAVIEW_TE_INSTANT("name8"),
                DEJAVIEW_TE_NAMED_TRACK("dynamictrack", 2,
                                        DejaViewTeProcessTrackUuid()),
                DEJAVIEW_TE_TIMESTAMP(DejaViewTeGetTimestamp()));
    DEJAVIEW_TE(physics, DEJAVIEW_TE_INSTANT("name9"),
                DEJAVIEW_TE_PROTO_FIELDS(DEJAVIEW_TE_PROTO_FIELD_NESTED(
                    dejaview_protos_TrackEvent_source_location_field_number,
                    DEJAVIEW_TE_PROTO_FIELD_CSTR(2, __FILE__),
                    DEJAVIEW_TE_PROTO_FIELD_VARINT(4, __LINE__))));
    DEJAVIEW_TE(DEJAVIEW_TE_DYNAMIC_CATEGORY, DEJAVIEW_TE_COUNTER(),
                DEJAVIEW_TE_DOUBLE_COUNTER(3.14),
                DEJAVIEW_TE_REGISTERED_TRACK(&mycounter),
                DEJAVIEW_TE_DYNAMIC_CATEGORY_STRING("physics"));
    sleep(1);
  }
}
