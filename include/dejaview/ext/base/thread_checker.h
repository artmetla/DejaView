/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_DEJAVIEW_EXT_BASE_THREAD_CHECKER_H_
#define INCLUDE_DEJAVIEW_EXT_BASE_THREAD_CHECKER_H_

#include "dejaview/base/build_config.h"

#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#include <pthread.h>
#endif
#include <atomic>

#include "dejaview/base/export.h"
#include "dejaview/base/logging.h"
#include "dejaview/ext/base/utils.h"

namespace dejaview {
namespace base {

#if DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
using ThreadID = unsigned long;
#else
using ThreadID = pthread_t;
#endif

class DEJAVIEW_EXPORT_COMPONENT ThreadChecker {
 public:
  ThreadChecker();
  ~ThreadChecker();
  ThreadChecker(const ThreadChecker&);
  ThreadChecker& operator=(const ThreadChecker&);
  bool CalledOnValidThread() const DEJAVIEW_WARN_UNUSED_RESULT;
  void DetachFromThread();

 private:
  mutable std::atomic<ThreadID> thread_id_;
};

#if DEJAVIEW_DCHECK_IS_ON() && !DEJAVIEW_BUILDFLAG(DEJAVIEW_CHROMIUM_BUILD)
// TODO(primiano) Use Chromium's thread checker in Chromium.
#define DEJAVIEW_THREAD_CHECKER(name) base::ThreadChecker name;
#define DEJAVIEW_DCHECK_THREAD(name) \
  DEJAVIEW_DCHECK((name).CalledOnValidThread())
#define DEJAVIEW_DETACH_FROM_THREAD(name) (name).DetachFromThread()
#else
#define DEJAVIEW_THREAD_CHECKER(name)
#define DEJAVIEW_DCHECK_THREAD(name)
#define DEJAVIEW_DETACH_FROM_THREAD(name)
#endif  // DEJAVIEW_DCHECK_IS_ON()

}  // namespace base
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_EXT_BASE_THREAD_CHECKER_H_
