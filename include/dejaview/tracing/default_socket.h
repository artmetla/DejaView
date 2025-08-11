/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_TRACING_DEFAULT_SOCKET_H_
#define INCLUDE_DEJAVIEW_TRACING_DEFAULT_SOCKET_H_

#include <string>
#include <vector>

#include "dejaview/base/export.h"

namespace dejaview {

DEJAVIEW_EXPORT_COMPONENT const char* GetConsumerSocket();
// This function is used for tokenize the |producer_socket_names| string into
// multiple producer socket names.
DEJAVIEW_EXPORT_COMPONENT std::vector<std::string> TokenizeProducerSockets(
    const char* producer_socket_names);
DEJAVIEW_EXPORT_COMPONENT const char* GetProducerSocket();

// Optionally returns the relay socket name (nullable). The relay socket is used
// for forwarding the IPC messages between the local producers and the remote
// tracing service.
DEJAVIEW_EXPORT_COMPONENT const char* GetRelaySocket();

}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_TRACING_DEFAULT_SOCKET_H_
