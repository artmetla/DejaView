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

#include "src/trace_processor/qemu/qmp_server.h"
#include "dejaview/ext/base/string_splitter.h"

#include <json/json.h>

#include "dejaview/base/task_runner.h"

namespace {

std::string ToJsonString(const Json::Value& value) {
  Json::StreamWriterBuilder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "";
  return Json::writeString(builder, value);
}

}

namespace dejaview::trace_processor {

QMPServer::QMPServer(base::TaskRunner *task_runner) : base::UnixSocket::EventListener(), tmp_dir_(base::TempDir::Create()),
                                            task_runner_(task_runner), icount_poller_(task_runner_) {
  socket_ = base::UnixSocket::Listen(sock_path(), this, task_runner_,
                                      base::SockFamily::kUnix,
                                      base::SockType::kStream);

  icount_poller_args_.period_ms = 40;
  icount_poller_args_.task = [&] {
    if (!client_socket_ || !capabilities_negotiated_ || inhibit_status_send_) return;

    Json::Value cmd;
    cmd["execute"] = "query-replay";
    client_socket_->SendStr(ToJsonString(cmd));
    inhibit_status_send_ = true;
  };
}

QMPServer::~QMPServer() {
  remove(sock_path().c_str());
}

void QMPServer::SeekTo(uint64_t target_icount) {
  if (!client_socket_ || !capabilities_negotiated_) return;

  Json::Value cmd;
  cmd["execute"] = "replay-seek";
  cmd["arguments"]["icount"] = static_cast<Json::UInt64>(target_icount);

  client_socket_->SendStr(ToJsonString(cmd));
}

std::string QMPServer::sock_path() { return tmp_dir_.path() + "/qmp.sock"; }

void QMPServer::SetOnIcountUpdate(std::function<void(uint64_t)> callback) {
  icount_update_ = callback;
}

void QMPServer::SetOnConnected(std::function<void()> callback) {
  connected_ = callback;
}

void QMPServer::OnNewIncomingConnection(base::UnixSocket *,
                              std::unique_ptr<base::UnixSocket> client) {
  client_socket_ = std::move(client);
}

void QMPServer::OnDataAvailable(base::UnixSocket *sock) {
    if (sock != client_socket_.get())
      return;
  auto received = sock->ReceiveString();
  base::StringSplitter s(received, '\n');
  for (; s.Next();) {
    if (!version_received_) {
      // First message from QEMU is a capabilities handshake.
      version_received_ = true;
      Json::Value cmd;
      cmd["execute"] = "qmp_capabilities";
      sock->SendStr(ToJsonString(cmd));
      return; // Wait for the capabilities response
    }

    if (!capabilities_negotiated_) {
      // Second message is the response to our qmp_capabilities command.
      capabilities_negotiated_ = true;
      connected_();
      return; // Handshake complete, ready for periodic queries
    }

    // Parse responses to our "query-replay" commands.
    Json::Value root;
    Json::CharReaderBuilder reader_builder;
    std::string errors;
    std::unique_ptr<Json::CharReader> const reader(reader_builder.newCharReader());

    if (!reader->parse(s.cur_token(), s.cur_token() + s.cur_token_size(), &root, &errors)) {
      fprintf(stderr, "Failed to parse JSON: %s %s\n", s.cur_token(), errors.c_str());
      return;
    }

    // The icount is inside the "return" object of a successful query.
    const Json::Value& returnValue = root["return"];
    if (returnValue.isObject() && returnValue.isMember("icount")) {
      const Json::Value& icountValue = returnValue["icount"];
      if (icountValue.isUInt64()) {
        uint64_t icount = icountValue.asUInt64();
        static uint64_t last_icount = 0;
        inhibit_status_send_ = false;
        if (last_icount != icount) {
          icount_update_(icount);
          last_icount = icount;
        }
      }
    }
    const Json::Value& eventValue = root["event"];
    if (eventValue.isString()) {
      std::string event = eventValue.asString();
      if (event == "STOP") {
        icount_poller_.Reset();
        icount_poller_args_.task();
      } else {
        icount_poller_.Start(icount_poller_args_);
      }
    }
  }
}

}  // namespace dejaview::trace_processor
