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

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <cinttypes>
#include <string>

#include "dejaview/base/logging.h"
#include "dejaview/base/time.h"
#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/getopt.h"
#include "dejaview/ext/base/scoped_file.h"

// Periodically prints an un-normalized cpu usage ratio (full use of a single
// core = 1.0) of a target process. Based on /proc/pid/stat utime (userspace) &
// stime (kernel space).

namespace dejaview {
namespace {

uint64_t TimespecToMs(struct timespec ts) {
  DEJAVIEW_CHECK(ts.tv_sec >= 0 && ts.tv_nsec >= 0);
  return static_cast<uint64_t>(ts.tv_sec) * 1000 +
         static_cast<uint64_t>(ts.tv_nsec) / 1000 / 1000;
}

uint64_t ReadWallTimeMs(clockid_t clk) {
  struct timespec ts = {};
  DEJAVIEW_CHECK(clock_gettime(clk, &ts) == 0);
  return TimespecToMs(ts);
}

void ReadUtimeStime(const base::ScopedFile& stat_fd,
                    unsigned long* utime_out,
                    unsigned long* stime_out) {
  char buf[1024] = {};
  lseek(stat_fd.get(), 0, SEEK_SET);
  DEJAVIEW_CHECK(read(stat_fd.get(), buf, sizeof(buf)) > 0);
  buf[sizeof(buf) - 1] = '\0';

  DEJAVIEW_CHECK(
      sscanf(buf, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
             utime_out, stime_out) == 2);
}

int CpuUtilizationMain(int argc, char** argv) {
  unsigned sleep_duration_us = 10 * 1000 * 1000;  // 10s
  int sleep_intervals = 6;
  int target_pid = -1;

  static option long_options[] = {
      {"pid", required_argument, nullptr, 'p'},
      {"sleep-duration-us", required_argument, nullptr, 't'},
      {"sleep-intervals", required_argument, nullptr, 'n'},
      {nullptr, 0, nullptr, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (c) {
      case 'p':
        target_pid = atoi(optarg);
        break;
      case 't':
        sleep_duration_us = static_cast<unsigned>(atoi(optarg));
        break;
      case 'n':
        sleep_intervals = atoi(optarg);
        break;
      default:
        break;
    }
  }

  if (target_pid < 1) {
    DEJAVIEW_ELOG(
        "Usage: %s --pid=target_pid [--sleep-duration-us=N] "
        "[--sleep-intervals=N]",
        argv[0]);
    return 1;
  }

  // Resolution of utime/stime from procfs, at least 10 ms.
  long ticks = sysconf(_SC_CLK_TCK);
  DEJAVIEW_CHECK(ticks >= 100);
  unsigned long ticks_per_s = static_cast<unsigned long>(ticks);

  // Resolution of wallclock time, at least 1 ms. Should be O(ns) in practice.
  auto clk = CLOCK_MONOTONIC_RAW;
  struct timespec ts = {};
  DEJAVIEW_CHECK(clock_getres(clk, &ts) == 0);
  DEJAVIEW_CHECK(ts.tv_sec == 0 && ts.tv_nsec <= 1 * 1000 * 1000);

  DEJAVIEW_LOG("--- setup: ---");
  DEJAVIEW_LOG("target pid: %d", target_pid);
  DEJAVIEW_LOG("intervals: %d x %uus", sleep_intervals, sleep_duration_us);
  DEJAVIEW_LOG("utime/stime ticks per sec: %ld", ticks_per_s);
  DEJAVIEW_LOG("wall clock resolution (ns): %ld", ts.tv_nsec);
  DEJAVIEW_LOG("--- timings: ---");

  base::ScopedFile fd = base::OpenFile(
      std::string("/proc/") + std::to_string(target_pid) + std::string("/stat"),
      O_RDONLY);
  DEJAVIEW_CHECK(fd);

  // Read the base times.
  unsigned long first_utime = 0;
  unsigned long first_stime = 0;
  ReadUtimeStime(fd, &first_utime, &first_stime);
  uint64_t first_walltime_ms = ReadWallTimeMs(clk);

  uint64_t last_walltime_ms = first_walltime_ms;
  unsigned long last_utime = first_utime;
  unsigned long last_stime = first_stime;

  // Report the utilization for each fixed duration chunk.
  for (int i = 0; i < sleep_intervals; i++) {
    base::SleepMicroseconds(sleep_duration_us);

    unsigned long utime = 0;
    unsigned long stime = 0;
    ReadUtimeStime(fd, &utime, &stime);
    uint64_t walltime_ms = ReadWallTimeMs(clk);

    uint64_t wall_diff_ms = walltime_ms - last_walltime_ms;
    DEJAVIEW_LOG("wall_ms    : [%" PRIu64 "] - [%" PRIu64 "] = [%" PRIu64 "]",
                 walltime_ms, last_walltime_ms, wall_diff_ms);

    unsigned long utime_diff = utime - last_utime;
    unsigned long stime_diff = stime - last_stime;
    DEJAVIEW_LOG("utime_ticks: [%lu] - [%lu] = [%lu]", utime, last_utime,
                 utime_diff);
    DEJAVIEW_LOG("stime_ticks: [%lu] - [%lu] = [%lu]", stime, last_stime,
                 stime_diff);

    // Calculate the utilization, resolution of inputs will be no worse than
    // 10ms due to the above assert. At the default 10s wall time, we therefore
    // get a resolution of at least 0.1%.
    double utime_diff_ms = static_cast<double>(utime_diff * 1000 / ticks_per_s);
    double stime_diff_ms = static_cast<double>(stime_diff * 1000 / ticks_per_s);

    double utime_ratio = utime_diff_ms / static_cast<double>(wall_diff_ms);
    double stime_ratio = stime_diff_ms / static_cast<double>(wall_diff_ms);

    DEJAVIEW_LOG("utime ratio   : %f", utime_ratio);
    DEJAVIEW_LOG("stime ratio   : %f", stime_ratio);
    DEJAVIEW_LOG("combined ratio: %f\n", utime_ratio + stime_ratio);

    last_walltime_ms = walltime_ms;
    last_utime = utime;
    last_stime = stime;
  }

  DEJAVIEW_LOG("--- timings over the whole period: ---");
  unsigned long utime_diff = last_utime - first_utime;
  unsigned long stime_diff = last_stime - first_stime;
  uint64_t wall_diff_ms = last_walltime_ms - first_walltime_ms;
  double utime_diff_ms = static_cast<double>(utime_diff * 1000 / ticks_per_s);
  double stime_diff_ms = static_cast<double>(stime_diff * 1000 / ticks_per_s);
  double utime_ratio = utime_diff_ms / static_cast<double>(wall_diff_ms);
  double stime_ratio = stime_diff_ms / static_cast<double>(wall_diff_ms);
  DEJAVIEW_LOG("utime ratio   : %f", utime_ratio);
  DEJAVIEW_LOG("stime ratio   : %f", stime_ratio);
  DEJAVIEW_LOG("combined ratio: %f\n", utime_ratio + stime_ratio);

  return 0;
}

}  // namespace
}  // namespace dejaview

int main(int argc, char** argv) {
  return dejaview::CpuUtilizationMain(argc, argv);
}
