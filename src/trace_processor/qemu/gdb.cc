/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "src/trace_processor/qemu/gdb.h"

#include "dejaview/base/task_runner.h"

#include <pty.h>
#include <signal.h>
#include <termios.h>

namespace dejaview::trace_processor {

Gdb::Gdb(base::TaskRunner *task_runner, std::string elfPath, int port) : task_runner_(task_runner) {
    int master_fd = -1, slave_fd = -1;
    DEJAVIEW_CHECK(openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) != -1);

    pty_master_fd_.reset(master_fd);
    task_runner_->AddFileDescriptorWatch(*pty_master_fd_, [this]() {
        unsigned char buffer[4096];
        long bytes_read = read(*pty_master_fd_, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            stdout_fn_(buffer, static_cast<size_t>(bytes_read));
        } else {
            task_runner_->RemoveFileDescriptorWatch(*pty_master_fd_);
            pty_master_fd_.reset();
            stopped_fn_();
        }
    });

    args.stdin_mode = base::Subprocess::InputMode::kFd;
    args.stdout_mode = base::Subprocess::OutputMode::kFd;
    args.stderr_mode = base::Subprocess::OutputMode::kFd;
    args.fd_is_pty = true;
    args.out_fd.reset(slave_fd);
    args.exec_cmd.push_back("/usr/bin/gdb");
    args.exec_cmd.push_back(elfPath);
    args.exec_cmd.push_back("-q");
    args.exec_cmd.push_back("-ex");
    args.exec_cmd.push_back("target remote :" + std::to_string(port));
    args.env.push_back("TERM=xterm-256color");
    args.env.push_back("HOME=" + std::string(getenv("HOME")));
}

Gdb::~Gdb() {
    if (!pty_master_fd_)
        return;

    task_runner_->RemoveFileDescriptorWatch(*pty_master_fd_);
    pty_master_fd_.reset();
    stopped_fn_();
}

void Gdb::Stdin(const uint8_t *data, size_t len) {
    if (!pty_master_fd_)
        return;

    write(*pty_master_fd_, data, len);
}

void Gdb::Resize(uint16_t rows, uint16_t cols) {
    if (!pty_master_fd_)
        return;

    struct winsize new_size;
    new_size.ws_row = rows;
    new_size.ws_col = cols;
    new_size.ws_xpixel = 0;
    new_size.ws_ypixel = 0;

    ioctl(*pty_master_fd_, TIOCSWINSZ, &new_size);

    kill(pid(), SIGWINCH);
}

}  // namespace dejaview::trace_processor
