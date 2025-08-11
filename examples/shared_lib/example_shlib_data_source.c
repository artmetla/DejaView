/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <unistd.h>

#include "dejaview/public/data_source.h"
#include "dejaview/public/producer.h"
#include "dejaview/public/protos/trace/test_event.pzc.h"
#include "dejaview/public/protos/trace/trace_packet.pzc.h"

static struct DejaViewDs custom = DEJAVIEW_DS_INIT();

int main(void) {
  struct DejaViewProducerInitArgs args = DEJAVIEW_PRODUCER_INIT_ARGS_INIT();
  args.backends = DEJAVIEW_BACKEND_SYSTEM;
  DejaViewProducerInit(args);

  DejaViewDsRegister(&custom, "com.example.custom_data_source",
                     DejaViewDsParamsDefault());

  for (;;) {
    DEJAVIEW_DS_TRACE(custom, ctx) {
      struct DejaViewDsRootTracePacket root;
      DejaViewDsTracerPacketBegin(&ctx, &root);

      dejaview_protos_TracePacket_set_timestamp(&root.msg, 42);
      {
        struct dejaview_protos_TestEvent for_testing;
        dejaview_protos_TracePacket_begin_for_testing(&root.msg, &for_testing);

        dejaview_protos_TestEvent_set_cstr_str(&for_testing,
                                               "This is a long string");
        {
          struct dejaview_protos_TestEvent_TestPayload payload;
          dejaview_protos_TestEvent_begin_payload(&for_testing, &payload);

          for (int i = 0; i < 1000; i++) {
            dejaview_protos_TestEvent_TestPayload_set_cstr_str(&payload,
                                                               "nested");
          }
          dejaview_protos_TestEvent_end_payload(&for_testing, &payload);
        }
        dejaview_protos_TracePacket_end_for_testing(&root.msg, &for_testing);
      }
      DejaViewDsTracerPacketEnd(&ctx, &root);
    }
    sleep(1);
  }
}
