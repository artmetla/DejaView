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

#ifndef INCLUDE_DEJAVIEW_PROTOZERO_SCATTERED_STREAM_NULL_DELEGATE_H_
#define INCLUDE_DEJAVIEW_PROTOZERO_SCATTERED_STREAM_NULL_DELEGATE_H_

#include <memory>
#include <vector>

#include "dejaview/base/export.h"
#include "dejaview/base/logging.h"
#include "dejaview/protozero/contiguous_memory_range.h"
#include "dejaview/protozero/scattered_stream_writer.h"

namespace protozero {

class DEJAVIEW_EXPORT_COMPONENT ScatteredStreamWriterNullDelegate
    : public ScatteredStreamWriter::Delegate {
 public:
  explicit ScatteredStreamWriterNullDelegate(size_t chunk_size);
  ~ScatteredStreamWriterNullDelegate() override;

  // protozero::ScatteredStreamWriter::Delegate implementation.
  ContiguousMemoryRange GetNewBuffer() override;

 private:
  const size_t chunk_size_;
  std::unique_ptr<uint8_t[]> chunk_;
};

}  // namespace protozero

#endif  // INCLUDE_DEJAVIEW_PROTOZERO_SCATTERED_STREAM_NULL_DELEGATE_H_
