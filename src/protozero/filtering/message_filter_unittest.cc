/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "dejaview/ext/base/base64.h"
#include "test/gtest_and_gmock.h"

#include <random>

#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/ext/base/temp_file.h"
#include "dejaview/protozero/proto_decoder.h"
#include "dejaview/protozero/proto_utils.h"
#include "dejaview/protozero/scattered_heap_buffer.h"
#include "protos/dejaview/trace/trace.pb.h"
#include "src/protozero/filtering/filter_util.h"
#include "src/protozero/filtering/message_filter.h"

namespace protozero {

namespace {

TEST(MessageFilterTest, EndToEnd) {
  auto schema = dejaview::base::TempFile::Create();
  static const char kSchema[] = R"(
  syntax = "proto2";
  message FilterSchema {
    message Nested {
      optional fixed32 f32 = 2;
      repeated string ss = 5;
    }
    optional int32 i32 = 1;
    optional string str = 3;
    repeated Nested nest = 6;
    repeated int32 f11 = 11;
    repeated int64 f12 = 12;
    repeated sint32 f13 = 13;
    repeated sint64 f14 = 14;
    repeated fixed32 f15 = 15;
    repeated fixed32 f16 = 16;
    repeated fixed64 f17 = 17;
    repeated fixed64 f18 = 18;
    repeated float f19 = 19;
    repeated double f20 = 20;
  };
  )";

  dejaview::base::WriteAll(*schema, kSchema, strlen(kSchema));
  dejaview::base::FlushFile(*schema);

  FilterUtil filter;
  ASSERT_TRUE(filter.LoadMessageDefinition(schema.path(), "", ""));
  std::string bytecode = filter.GenerateFilterBytecode();
  ASSERT_GT(bytecode.size(), 0u);

  HeapBuffered<Message> msg;
  msg->AppendVarInt(/*field_id=*/1, -1000000000ll);
  msg->AppendVarInt(/*field_id=*/2, 42);
  msg->AppendString(/*field_id=*/3, "foobar");
  msg->AppendFixed(/*field_id=*/4, 10);
  msg->AppendVarInt(/*field_id=*/11, INT32_MIN);
  msg->AppendVarInt(/*field_id=*/12, INT64_MIN);
  msg->AppendSignedVarInt(/*field_id=*/13, INT32_MIN);
  msg->AppendSignedVarInt(/*field_id=*/14, INT64_MIN);
  msg->AppendFixed(/*field_id=*/15, static_cast<int32_t>(INT32_MIN));
  msg->AppendFixed(/*field_id=*/16, static_cast<int32_t>(INT32_MAX));
  msg->AppendFixed(/*field_id=*/17, static_cast<int64_t>(INT64_MIN));
  msg->AppendFixed(/*field_id=*/18, static_cast<int64_t>(INT64_MAX));
  msg->AppendFixed(/*field_id=*/19, FLT_EPSILON);
  msg->AppendFixed(/*field_id=*/20, DBL_EPSILON);

  auto* nest = msg->BeginNestedMessage<Message>(/*field_id=*/6);
  nest->AppendFixed(/*field_id=*/1, 10);
  nest->AppendFixed(/*field_id=*/2, static_cast<int32_t>(-2000000000ll));
  nest->AppendString(/*field_id=*/4, "stripped");
  nest->AppendString(/*field_id=*/5, "");
  nest->Finalize();

  MessageFilter flt;
  ASSERT_TRUE(flt.LoadFilterBytecode(bytecode.data(), bytecode.size()));

  std::vector<uint8_t> encoded = msg.SerializeAsArray();

  for (int repetitions = 0; repetitions < 3; ++repetitions) {
    auto filtered = flt.FilterMessage(encoded.data(), encoded.size());
    ASSERT_LT(filtered.size, encoded.size());

    ProtoDecoder dec(filtered.data.get(), filtered.size);
    EXPECT_TRUE(dec.FindField(1).valid());
    EXPECT_EQ(dec.FindField(1).as_int64(), -1000000000ll);
    EXPECT_FALSE(dec.FindField(2).valid());
    EXPECT_TRUE(dec.FindField(3).valid());
    EXPECT_EQ(dec.FindField(3).as_std_string(), "foobar");
    EXPECT_FALSE(dec.FindField(4).valid());
    EXPECT_TRUE(dec.FindField(6).valid());
    for (uint32_t i = 11; i <= 20; ++i)
      EXPECT_TRUE(dec.FindField(i).valid());

    EXPECT_EQ(dec.FindField(11).as_int32(), INT32_MIN);
    EXPECT_EQ(dec.FindField(12).as_int64(), INT64_MIN);
    EXPECT_EQ(dec.FindField(13).as_sint32(), INT32_MIN);
    EXPECT_EQ(dec.FindField(14).as_sint64(), INT64_MIN);
    EXPECT_EQ(dec.FindField(15).as_int32(), INT32_MIN);
    EXPECT_EQ(dec.FindField(16).as_int32(), INT32_MAX);
    EXPECT_EQ(dec.FindField(17).as_int64(), INT64_MIN);
    EXPECT_EQ(dec.FindField(18).as_int64(), INT64_MAX);
    EXPECT_EQ(dec.FindField(19).as_float(), FLT_EPSILON);
    EXPECT_EQ(dec.FindField(20).as_double(), DBL_EPSILON);

    ProtoDecoder nest_dec(dec.FindField(6).as_bytes());
    EXPECT_FALSE(nest_dec.FindField(1).valid());
    EXPECT_TRUE(nest_dec.FindField(2).valid());
    EXPECT_EQ(nest_dec.FindField(2).as_int32(), -2000000000ll);
    EXPECT_TRUE(nest_dec.FindField(5).valid());
    EXPECT_EQ(nest_dec.FindField(5).as_bytes().size, 0u);
  }
}

TEST(MessageFilterTest, Passthrough) {
  auto schema = dejaview::base::TempFile::Create();
  static const char kSchema[] = R"(
  syntax = "proto2";
  message TracePacket {
    optional int64 timestamp = 1;
    optional TraceConfig cfg = 2;
    optional TraceConfig cfg_filtered = 3;
    optional string other = 4;
  };
  message SubConfig {
    optional string f4 = 6;
  }
  message TraceConfig {
    optional int64 f1 = 3;
    optional string f2 = 4;
    optional SubConfig f3 = 5;
  }
  )";

  dejaview::base::WriteAll(*schema, kSchema, strlen(kSchema));
  dejaview::base::FlushFile(*schema);

  FilterUtil filter;
  ASSERT_TRUE(filter.LoadMessageDefinition(
      schema.path(), "", "", {"TracePacket:other", "TracePacket:cfg"}));
  std::string bytecode = filter.GenerateFilterBytecode();
  ASSERT_GT(bytecode.size(), 0u);

  HeapBuffered<Message> msg;
  msg->AppendVarInt(/*field_id=*/1, 10);
  msg->AppendString(/*field_id=*/4, "other_string");

  // Fill `cfg`.
  auto* nest = msg->BeginNestedMessage<Message>(/*field_id=*/2);
  nest->AppendVarInt(/*field_id=*/3, 100);
  nest->AppendString(/*field_id=*/4, "f2.payload");
  nest->AppendString(/*field_id=*/99, "not_in_original_schema");
  auto* nest2 = nest->BeginNestedMessage<Message>(/*field_id=*/5);
  nest2->AppendString(/*field_id=*/6, "subconfig.f4");
  nest2->Finalize();
  nest->Finalize();

  // Fill `cfg_filtered`.
  nest = msg->BeginNestedMessage<Message>(/*field_id=*/3);
  nest->AppendVarInt(/*field_id=*/3, 200);  // This should be propagated.
  nest->AppendVarInt(/*field_id=*/6, 300);  // This shoudl be filtered out.
  nest->Finalize();

  MessageFilter flt;
  ASSERT_TRUE(flt.LoadFilterBytecode(bytecode.data(), bytecode.size()));

  std::vector<uint8_t> encoded = msg.SerializeAsArray();

  auto filtered = flt.FilterMessage(encoded.data(), encoded.size());
  ASSERT_FALSE(filtered.error);
  ASSERT_LT(filtered.size, encoded.size());

  ProtoDecoder dec(filtered.data.get(), filtered.size);
  EXPECT_EQ(dec.FindField(1).as_int64(), 10);
  EXPECT_EQ(dec.FindField(4).as_std_string(), "other_string");

  EXPECT_TRUE(dec.FindField(2).valid());
  ProtoDecoder nest_dec(dec.FindField(2).as_bytes());
  EXPECT_EQ(nest_dec.FindField(3).as_int32(), 100);
  EXPECT_EQ(nest_dec.FindField(4).as_std_string(), "f2.payload");
  EXPECT_TRUE(nest_dec.FindField(5).valid());
  ProtoDecoder nest_dec2(nest_dec.FindField(5).as_bytes());
  EXPECT_EQ(nest_dec2.FindField(6).as_std_string(), "subconfig.f4");

  // Field 99 should be preserved anyways even if it wasn't in the original
  // schema because the whole TracePacket submessage was passed through.
  EXPECT_TRUE(nest_dec.FindField(99).valid());
  EXPECT_EQ(nest_dec.FindField(99).as_std_string(), "not_in_original_schema");

  // Check that the field `cfg_filtered` contains only `f1`,`f2`,`f3`.
  EXPECT_TRUE(dec.FindField(3).valid());
  ProtoDecoder nest_dec3(dec.FindField(3).as_bytes());
  EXPECT_EQ(nest_dec3.FindField(3).as_int32(), 200);
  EXPECT_FALSE(nest_dec3.FindField(6).valid());
}

TEST(MessageFilterTest, ChangeRoot) {
  auto schema = dejaview::base::TempFile::Create();
  static const char kSchema[] = R"(
  syntax = "proto2";
  message FilterSchema {
    message Nested {
      message Nested2 {
        optional int32 e = 5;
      }
      optional int32 c = 3;
      repeated Nested2 d = 4;
    }
    optional int32 a = 1;
    optional Nested b = 2;
  };
  )";

  dejaview::base::WriteAll(*schema, kSchema, strlen(kSchema));
  dejaview::base::FlushFile(*schema);

  FilterUtil filter;
  ASSERT_TRUE(filter.LoadMessageDefinition(schema.path(), "", ""));
  std::string bytecode = filter.GenerateFilterBytecode();
  ASSERT_GT(bytecode.size(), 0u);

  HeapBuffered<Message> msg;
  msg->AppendVarInt(/*field_id=*/1, 101);
  msg->AppendVarInt(/*field_id=*/3, 103);
  msg->AppendVarInt(/*field_id=*/5, 105);
  auto* nest = msg->BeginNestedMessage<Message>(/*field_id=*/4);  // Nested b.
  nest->AppendVarInt(/*field_id=*/5, 205);
  nest->Finalize();
  std::vector<uint8_t> encoded = msg.SerializeAsArray();

  MessageFilter flt;
  ASSERT_TRUE(flt.LoadFilterBytecode(bytecode.data(), bytecode.size()));

  // First set the root to field id ".2" (.b). The fliter should happen treating
  // |Nested| as rot, so allowing only field 3 and 4 (Nested2) through.
  {
    flt.SetFilterRoot({2});
    auto filtered = flt.FilterMessage(encoded.data(), encoded.size());
    ASSERT_LT(filtered.size, encoded.size());
    ProtoDecoder dec(filtered.data.get(), filtered.size);
    EXPECT_FALSE(dec.FindField(1).valid());
    EXPECT_TRUE(dec.FindField(3).valid());
    EXPECT_EQ(dec.FindField(3).as_int32(), 103);
    EXPECT_FALSE(dec.FindField(5).valid());
    EXPECT_TRUE(dec.FindField(4).valid());
    EXPECT_EQ(dec.FindField(4).as_std_string(), "(\xCD\x01");
  }

  // Now set the root to ".2.4" (.b.d). This should allow only the field "e"
  // to pass through.
  {
    flt.SetFilterRoot({2, 4});
    auto filtered = flt.FilterMessage(encoded.data(), encoded.size());
    ASSERT_LT(filtered.size, encoded.size());
    ProtoDecoder dec(filtered.data.get(), filtered.size);
    EXPECT_FALSE(dec.FindField(1).valid());
    EXPECT_FALSE(dec.FindField(3).valid());
    EXPECT_FALSE(dec.FindField(4).valid());
    EXPECT_TRUE(dec.FindField(5).valid());
    EXPECT_EQ(dec.FindField(5).as_int32(), 105);
  }
}

TEST(MessageFilterTest, StringFilter) {
  auto schema = dejaview::base::TempFile::Create();
  static const char kSchema[] = R"(
  syntax = "proto2";
  message TracePacket {
    optional TraceConfig cfg = 1;
  };
  message TraceConfig {
    optional string f2 = 1;
  }
  )";

  dejaview::base::WriteAll(*schema, kSchema, strlen(kSchema));
  dejaview::base::FlushFile(*schema);

  FilterUtil filter;
  ASSERT_TRUE(filter.LoadMessageDefinition(schema.path(), "", "", {},
                                           {"TraceConfig:f2"}));
  std::string bytecode = filter.GenerateFilterBytecode();
  ASSERT_GT(bytecode.size(), 0u);
  DEJAVIEW_LOG(
      "%s", dejaview::base::Base64Encode(dejaview::base::StringView(bytecode))
                .c_str());

  HeapBuffered<Message> msg;
  msg->AppendVarInt(/*field_id=*/1, 10);

  // Fill `cfg`.
  auto* nest = msg->BeginNestedMessage<Message>(/*field_id=*/1);
  nest->AppendString(/*field_id=*/1, "f2.payload");
  nest->Finalize();

  MessageFilter flt;
  ASSERT_TRUE(flt.LoadFilterBytecode(bytecode.data(), bytecode.size()));

  std::vector<uint8_t> encoded = msg.SerializeAsArray();

  auto filtered = flt.FilterMessage(encoded.data(), encoded.size());
  ASSERT_FALSE(filtered.error);
  ASSERT_LT(filtered.size, encoded.size());
}

TEST(MessageFilterTest, MalformedInput) {
  // Create and load a simple filter.
  auto schema = dejaview::base::TempFile::Create();
  static const char kSchema[] = R"(
  syntax = "proto2";
  message FilterSchema {
    message Nested {
      optional fixed32 f32 = 4;
      repeated string ss = 5;
    }
    optional int32 i32 = 1;
    optional string str = 2;
    repeated Nested nest = 3;
  };
  )";
  dejaview::base::WriteAll(*schema, kSchema, strlen(kSchema));
  dejaview::base::FlushFile(*schema);
  FilterUtil filter;
  ASSERT_TRUE(filter.LoadMessageDefinition(schema.path(), "", ""));
  std::string bytecode = filter.GenerateFilterBytecode();
  ASSERT_GT(bytecode.size(), 0u);
  MessageFilter flt;
  ASSERT_TRUE(flt.LoadFilterBytecode(bytecode.data(), bytecode.size()));

  {
    // A malformed message found by the fuzzer.
    static const uint8_t kData[]{
        0x52, 0x21,  // ID=10, type=len-delimited, len=33.
        0xa0, 0xa4,  // Early terminating payload.
    };
    auto res = flt.FilterMessage(kData, sizeof(kData));
    EXPECT_TRUE(res.error);
  }

  {
    // A malformed message which contains a non-terminated varint.
    static const uint8_t kData[]{
        0x08, 0x2A,  // A valid varint field id=1 value=42 (0x2A).
        0x08, 0xFF,  // An unterminated varint.
    };
    auto res = flt.FilterMessage(kData, sizeof(kData));
    EXPECT_TRUE(res.error);
  }

  {
    // A malformed message which contains a sub-message with a field that brings
    // it out of the outer size.
    static const uint8_t kData[]{
        0x08, 0x2A,  // A valid varint field id=1 value=42 (0x2A).
        0x1A, 0x04,  // A len-delim field, id=3, length=4.
        // The nested message |nest| starts here.
        0x25, 0x0, 0x0, 0x0, 0x01,  // A fixed32 field, id=4.
        // Note that the fixed32 field has an expected length of 4 but that
        // overflows the size of the |nest| method, because with its 0x25
        // preamble it becomes 5 bytes. At this point this should cause a
        // persistent failure.
    };
    auto res = flt.FilterMessage(kData, sizeof(kData));
    EXPECT_TRUE(res.error);
  }

  // A parsing failure shoulnd't affect the ability to filter the following
  // message. Try again but this time with a valid message.
  {
    static const uint8_t kData[]{
        0x08, 0x2A,  // A valid varint field id=1 value=42 (0x2A).
        0x1A, 0x05,  // A len-delim field, id=3, length=5.
        0x25, 0x0,  0x0, 0x0, 0x01,  // A fixed32 field, id=4.
        0x38, 0x42,  // A valid but not allowed varint field id=7.
    };
    auto res = flt.FilterMessage(kData, sizeof(kData));
    EXPECT_FALSE(res.error);
    EXPECT_EQ(res.size, sizeof(kData) - 2);  // last 2 bytes should be skipped.
    EXPECT_EQ(memcmp(kData, res.data.get(), res.size), 0);
  }
}

}  // namespace
}  // namespace protozero
