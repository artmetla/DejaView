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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "dejaview/base/build_config.h"
#include "dejaview/base/logging.h"
#include "dejaview/base/status.h"
#include "dejaview/ext/base/scoped_file.h"
#include "dejaview/ext/base/string_utils.h"
#include "dejaview/trace_processor/basic_types.h"
#include "dejaview/trace_processor/iterator.h"
#include "dejaview/trace_processor/status.h"
#include "dejaview/trace_processor/trace_blob.h"
#include "dejaview/trace_processor/trace_blob_view.h"
#include "dejaview/trace_processor/trace_processor.h"
#include "protos/dejaview/common/descriptor.pbzero.h"
#include "protos/dejaview/trace_processor/trace_processor.pbzero.h"

#include "src/base/test/status_matchers.h"
#include "src/base/test/utils.h"
#include "test/gtest_and_gmock.h"

namespace dejaview::trace_processor {
namespace {

using testing::HasSubstr;

constexpr size_t kMaxChunkSize = 4ul * 1024 * 1024;

TEST(TraceProcessorCustomConfigTest, EmptyStringSkipsAllMetrics) {
  auto config = Config();
  config.skip_builtin_metric_paths = {""};
  auto processor = TraceProcessor::CreateInstance(config);
  ASSERT_OK(processor->NotifyEndOfFile());

  // Check that other metrics have been loaded.
  auto it = processor->ExecuteQuery(
      "select count(*) from trace_metrics "
      "where name = 'trace_metadata';");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).type, SqlValue::kLong);
  ASSERT_EQ(it.Get(0).long_value, 0);
}

class TraceProcessorIntegrationTest : public ::testing::Test {
 public:
  TraceProcessorIntegrationTest()
      : processor_(TraceProcessor::CreateInstance(Config())) {}

 protected:
  base::Status LoadTrace(const char* name,
                         size_t min_chunk_size = 512,
                         size_t max_chunk_size = kMaxChunkSize) {
    EXPECT_LE(min_chunk_size, max_chunk_size);
    base::ScopedFstream f(
        fopen(base::GetTestDataPath(std::string("test/data/") + name).c_str(),
              "rbe"));
    std::minstd_rand0 rnd_engine(0);
    std::uniform_int_distribution<size_t> dist(min_chunk_size, max_chunk_size);
    while (!feof(*f)) {
      size_t chunk_size = dist(rnd_engine);
      std::unique_ptr<uint8_t[]> buf(new uint8_t[chunk_size]);
      auto rsize = fread(reinterpret_cast<char*>(buf.get()), 1, chunk_size, *f);
      auto status = processor_->Parse(std::move(buf), rsize);
      if (!status.ok())
        return status;
    }
    return processor_->NotifyEndOfFile();
  }

  Iterator Query(const std::string& query) {
    return processor_->ExecuteQuery(query);
  }

  TraceProcessor* Processor() { return processor_.get(); }

  size_t RestoreInitialTables() { return processor_->RestoreInitialTables(); }

 private:
  std::unique_ptr<TraceProcessor> processor_;
};

TEST_F(TraceProcessorIntegrationTest, Hash) {
  auto it = Query("select HASH()");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xcbf29ce484222325));

  it = Query("select HASH('test')");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xf9e6e6ef197c2b25));

  it = Query("select HASH('test', 1)");
  ASSERT_TRUE(it.Next());
  ASSERT_EQ(it.Get(0).long_value, static_cast<int64_t>(0xa9cb070fdc15f7a4));
}

#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_LLVM_DEMANGLE) && \
    !DEJAVIEW_BUILDFLAG(DEJAVIEW_OS_WIN)
#define MAYBE_Demangle DISABLED_Demangle
#else
#define MAYBE_Demangle Demangle
#endif
TEST_F(TraceProcessorIntegrationTest, MAYBE_Demangle) {
  auto it = Query("select DEMANGLE('_Znwm')");
  ASSERT_TRUE(it.Next());
  EXPECT_STRCASEEQ(it.Get(0).string_value, "operator new(unsigned long)");

  it = Query("select DEMANGLE('_ZN3art6Thread14CreateCallbackEPv')");
  ASSERT_TRUE(it.Next());
  EXPECT_STRCASEEQ(it.Get(0).string_value,
                   "art::Thread::CreateCallback(void*)");

  it = Query("select DEMANGLE('test')");
  ASSERT_TRUE(it.Next());
  EXPECT_TRUE(it.Get(0).is_null());
}

#if !DEJAVIEW_BUILDFLAG(DEJAVIEW_LLVM_DEMANGLE)
#define MAYBE_DemangleRust DISABLED_DemangleRust
#else
#define MAYBE_DemangleRust DemangleRust
#endif
TEST_F(TraceProcessorIntegrationTest, MAYBE_DemangleRust) {
  auto it = Query(
      "select DEMANGLE("
      "'_RNvNvMs0_NtNtNtCsg1Z12QU66Yk_3std3sys4unix6threadNtB7_"
      "6Thread3new12thread_start')");
  ASSERT_TRUE(it.Next());
  EXPECT_STRCASEEQ(it.Get(0).string_value,
                   "<std::sys::unix::thread::Thread>::new::thread_start");

  it = Query("select DEMANGLE('_RNvCsdV139EorvfX_14keystore2_main4main')");
  ASSERT_TRUE(it.Next());
  ASSERT_STRCASEEQ(it.Get(0).string_value, "keystore2_main::main");

  it = Query("select DEMANGLE('_R')");
  ASSERT_TRUE(it.Next());
  ASSERT_TRUE(it.Get(0).is_null());
}

// Failing on DCHECKs during import because the traces aren't really valid.
#if DEJAVIEW_DCHECK_IS_ON()
#define MAYBE_Clusterfuzz20215 DISABLED_Clusterfuzz20215
#define MAYBE_Clusterfuzz20292 DISABLED_Clusterfuzz20292
#define MAYBE_Clusterfuzz21178 DISABLED_Clusterfuzz21178
#define MAYBE_Clusterfuzz21890 DISABLED_Clusterfuzz21890
#define MAYBE_Clusterfuzz23053 DISABLED_Clusterfuzz23053
#define MAYBE_Clusterfuzz28338 DISABLED_Clusterfuzz28338
#define MAYBE_Clusterfuzz28766 DISABLED_Clusterfuzz28766
#else  // DEJAVIEW_DCHECK_IS_ON()
#define MAYBE_Clusterfuzz20215 Clusterfuzz20215
#define MAYBE_Clusterfuzz20292 Clusterfuzz20292
#define MAYBE_Clusterfuzz21178 Clusterfuzz21178
#define MAYBE_Clusterfuzz21890 Clusterfuzz21890
#define MAYBE_Clusterfuzz23053 Clusterfuzz23053
#define MAYBE_Clusterfuzz28338 Clusterfuzz28338
#define MAYBE_Clusterfuzz28766 Clusterfuzz28766
#endif  // DEJAVIEW_DCHECK_IS_ON()

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz20215) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_20215", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz20292) {
  ASSERT_FALSE(LoadTrace("clusterfuzz_20292", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz21178) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_21178", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz21890) {
  ASSERT_FALSE(LoadTrace("clusterfuzz_21890", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz23053) {
  ASSERT_FALSE(LoadTrace("clusterfuzz_23053", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz28338) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_28338", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, MAYBE_Clusterfuzz28766) {
  ASSERT_TRUE(LoadTrace("clusterfuzz_28766", 4096).ok());
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesInvariant) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  uint64_t first_restore = RestoreInitialTables();
  ASSERT_EQ(RestoreInitialTables(), first_restore);
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesDejaViewSql) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);

    // 1. DejaView table
    {
      auto it = Query("CREATE DEJAVIEW TABLE obj1 AS SELECT 1 AS col;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    // 2. DejaView view
    {
      auto it = Query("CREATE DEJAVIEW VIEW obj2 AS SELECT * FROM stats;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    // 3. Runtime function
    {
      auto it =
          Query("CREATE DEJAVIEW FUNCTION obj3() RETURNS INT AS SELECT 1;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    // 4. Runtime table function
    {
      auto it = Query(
          "CREATE DEJAVIEW FUNCTION obj4() RETURNS TABLE(col INT) AS SELECT 1 "
          "AS col;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    // 5. Macro
    {
      auto it = Query("CREATE DEJAVIEW MACRO obj5(a Expr) returns Expr AS $a;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("obj5!(SELECT 1);");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    ASSERT_EQ(RestoreInitialTables(), 5u);
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesStandardSqlite) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);
    {
      auto it = Query("CREATE TABLE obj1(unused text);");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("CREATE TEMPORARY TABLE obj2(unused text);");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    // Add a view
    {
      auto it = Query("CREATE VIEW obj3 AS SELECT * FROM stats;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    ASSERT_EQ(RestoreInitialTables(), 3u);
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesModules) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);
    {
      auto it = Query("INCLUDE DEJAVIEW MODULE common.timestamps;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("SELECT trace_start();");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    RestoreInitialTables();
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesSpanJoin) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);
    {
      auto it = Query(
          "CREATE TABLE t1(ts BIGINT, dur BIGINT, PRIMARY KEY (ts, dur)) "
          "WITHOUT ROWID;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query(
          "CREATE TABLE t2(ts BIGINT, dur BIGINT, PRIMARY KEY (ts, dur)) "
          "WITHOUT ROWID;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("INSERT INTO t2(ts, dur) VALUES(1, 2), (5, 0), (1, 1);");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("CREATE VIRTUAL TABLE sp USING span_join(t1, t2);;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("SELECT ts, dur FROM sp;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    ASSERT_EQ(RestoreInitialTables(), 3u);
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesWithClause) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);
    {
      auto it = Query(
          "CREATE DEJAVIEW TABLE foo AS WITH bar AS (SELECT * FROM slice) "
          "SELECT ts FROM bar;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    ASSERT_EQ(RestoreInitialTables(), 1u);
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesIndex) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  RestoreInitialTables();

  for (int repeat = 0; repeat < 3; repeat++) {
    ASSERT_EQ(RestoreInitialTables(), 0u);
    {
      auto it = Query("CREATE TABLE foo AS SELECT * FROM slice;");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    {
      auto it = Query("CREATE INDEX ind ON foo (ts, track_id);");
      it.Next();
      ASSERT_TRUE(it.Status().ok());
    }
    ASSERT_EQ(RestoreInitialTables(), 2u);
  }
}

TEST_F(TraceProcessorIntegrationTest, RestoreInitialTablesDependents) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  {
    auto it = Query("create dejaview table foo as select 1 as x");
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok());

    it = Query("create dejaview function f() returns INT as select * from foo");
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok());

    it = Query("SELECT f()");
    ASSERT_TRUE(it.Next());
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok());
  }

  ASSERT_EQ(RestoreInitialTables(), 2u);
}

TEST_F(TraceProcessorIntegrationTest, RestoreDependentFunction) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  {
    auto it =
        Query("create dejaview function foo0() returns INT as select 1 as x");
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok());
  }
  for (int i = 1; i < 100; ++i) {
    base::StackString<1024> sql(
        "create dejaview function foo%d() returns INT as select foo%d()", i,
        i - 1);
    auto it = Query(sql.c_str());
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok()) << it.Status().c_message();
  }

  ASSERT_EQ(RestoreInitialTables(), 100u);
}

TEST_F(TraceProcessorIntegrationTest, RestoreDependentTableFunction) {
  ASSERT_OK(Processor()->NotifyEndOfFile());
  {
    auto it = Query(
        "create dejaview function foo0() returns TABLE(x INT) "
        " as select 1 as x");
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok());
  }
  for (int i = 1; i < 100; ++i) {
    base::StackString<1024> sql(
        "create dejaview function foo%d() returns TABLE(x INT) "
        " as select * from foo%d()",
        i, i - 1);
    auto it = Query(sql.c_str());
    ASSERT_FALSE(it.Next());
    ASSERT_TRUE(it.Status().ok()) << it.Status().c_message();
  }

  ASSERT_EQ(RestoreInitialTables(), 100u);
}

/*
 * This trace does not have a uuid. The uuid will be generated from the first
 * 4096 bytes, which will be read in one chunk.
 */
TEST_F(TraceProcessorIntegrationTest, TraceWithoutUuidReadInOneChunk) {
  ASSERT_TRUE(LoadTrace("example_android_trace_30s.pb", kMaxChunkSize).ok());
  auto it = Query("select str_value from metadata where name = 'trace_uuid'");
  ASSERT_TRUE(it.Next());
  EXPECT_STREQ(it.Get(0).string_value, "00000000-0000-0000-8906-ebb53e1d0738");
}

/*
 * This trace does not have a uuid. The uuid will be generated from the first
 * 4096 bytes, which will be read in multiple chunks.
 */
TEST_F(TraceProcessorIntegrationTest, TraceWithoutUuidReadInMultipleChuncks) {
  ASSERT_TRUE(LoadTrace("example_android_trace_30s.pb", 512, 2048).ok());
  auto it = Query("select str_value from metadata where name = 'trace_uuid'");
  ASSERT_TRUE(it.Next());
  EXPECT_STREQ(it.Get(0).string_value, "00000000-0000-0000-8906-ebb53e1d0738");
}

/*
 * This trace has a uuid. It will not be overriden by the hash of the first 4096
 * bytes.
 */
TEST_F(TraceProcessorIntegrationTest, TraceWithUuidReadInParts) {
  ASSERT_TRUE(LoadTrace("trace_with_uuid.pftrace", 512, 2048).ok());
  auto it = Query("select str_value from metadata where name = 'trace_uuid'");
  ASSERT_TRUE(it.Next());
  EXPECT_STREQ(it.Get(0).string_value, "123e4567-e89b-12d3-a456-426655443322");
}

TEST_F(TraceProcessorIntegrationTest, ErrorMessageExecuteQuery) {
  auto it = Query("select t from slice");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());

  ASSERT_THAT(it.Status().message(),
              testing::Eq(R"(Traceback (most recent call last):
  File "stdin" line 1 col 8
    select t from slice
           ^
no such column: t)"));
}

TEST_F(TraceProcessorIntegrationTest, ErrorMessageMetricFile) {
  ASSERT_TRUE(
      Processor()->RegisterMetric("foo/bar.sql", "select t from slice").ok());

  auto it = Query("select RUN_METRIC('foo/bar.sql');");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());

  ASSERT_EQ(it.Status().message(),
            R"(Traceback (most recent call last):
  File "stdin" line 1 col 1
    select RUN_METRIC('foo/bar.sql');
    ^
  Metric file "foo/bar.sql" line 1 col 8
    select t from slice
           ^
no such column: t)");
}

TEST_F(TraceProcessorIntegrationTest, ErrorMessageModule) {
  SqlPackage module;
  module.name = "foo";
  module.modules.push_back(std::make_pair("foo.bar", "select t from slice"));

  ASSERT_TRUE(Processor()->RegisterSqlPackage(module).ok());

  auto it = Query("include dejaview module foo.bar;");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());

  ASSERT_EQ(it.Status().message(),
            R"(Traceback (most recent call last):
  File "stdin" line 1 col 1
    include dejaview module foo.bar
    ^
  Module include "foo.bar" line 1 col 8
    select t from slice
           ^
no such column: t)");
}

TEST_F(TraceProcessorIntegrationTest, FunctionRegistrationError) {
  auto it =
      Query("create dejaview function f() returns INT as select * from foo");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());

  it = Query("SELECT foo()");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());

  it = Query("create dejaview function f() returns INT as select 1");
  ASSERT_FALSE(it.Next());
  ASSERT_TRUE(it.Status().ok());
}

TEST_F(TraceProcessorIntegrationTest, CreateTableDuplicateNames) {
  auto it = Query(
      "create dejaview table foo select 1 as duplicate_a, 2 as duplicate_a, 3 "
      "as duplicate_b, 4 as duplicate_b");
  ASSERT_FALSE(it.Next());
  ASSERT_FALSE(it.Status().ok());
  ASSERT_THAT(it.Status().message(), HasSubstr("duplicate_a"));
  ASSERT_THAT(it.Status().message(), HasSubstr("duplicate_b"));
}

TEST_F(TraceProcessorIntegrationTest, InvalidTrace) {
  constexpr char kBadData[] = "\0\0\0\0";
  EXPECT_FALSE(Processor()
                   ->Parse(TraceBlobView(
                       TraceBlob::CopyFrom(kBadData, sizeof(kBadData))))
                   .ok());
  Processor()->NotifyEndOfFile();
}

TEST_F(TraceProcessorIntegrationTest, NoNotifyEndOfFileCalled) {
  constexpr char kProtoData[] = "\x0a";
  EXPECT_TRUE(Processor()
                  ->Parse(TraceBlobView(
                      TraceBlob::CopyFrom(kProtoData, sizeof(kProtoData))))
                  .ok());
}

}  // namespace
}  // namespace dejaview::trace_processor
