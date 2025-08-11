// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <functional>
#include <random>
#include <unordered_map>

#include <benchmark/benchmark.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "dejaview/base/logging.h"
#include "dejaview/ext/base/file_utils.h"
#include "dejaview/ext/base/flat_hash_map.h"
#include "dejaview/ext/base/hash.h"
#include "dejaview/ext/base/scoped_file.h"
#include "dejaview/ext/base/string_view.h"

// This benchmark allows to compare our FlatHashMap implementation against
// reference implementations from Absl (Google), Folly F14 (FB), and Tssil's
// reference RobinHood hashmap.
// Those libraries are not checked in into the repo. If you want to reproduce
// the benchmark you need to:
// - Manually install the three libraries using following the instructions in
//   their readme (they all use cmake).
// - When running cmake, remember to pass
//   -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-DNDEBUG -O3 -msse4.2 -mavx'.
//   That sets cflags for a more fair comparison.
// - Set is_debug=false in the GN args.
// - Set the GN var dejaview_benchmark_3p_libs_prefix="/usr/local" (or whatever
//   other directory you set as DESTDIR when running make install).
// The presence of the dejaview_benchmark_3p_libs_prefix GN variable will
// automatically define DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS.

#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)

// Last tested: https://github.com/abseil/abseil-cpp @ f2dbd918d.
#include <absl/container/flat_hash_map.h>

// Last tested: https://github.com/facebook/folly @ 028a9abae3.
#include <folly/container/F14Map.h>

// Last tested: https://github.com/Tessil/robin-map @ a603419b9.
#include <tsl/robin_map.h>
#endif

namespace {

using namespace dejaview;
using benchmark::Counter;
using dejaview::base::AlreadyHashed;
using dejaview::base::LinearProbe;
using dejaview::base::QuadraticHalfProbe;
using dejaview::base::QuadraticProbe;

// Our FlatHashMap doesn't have a STL-like interface, mainly because we use
// columnar-oriented storage, not array-of-tuples, so we can't easily map into
// that interface. This wrapper makes our FlatHashMap compatible with STL (just
// for what it takes to build this translation unit), at the cost of some small
// performance penalty (around 1-2%).
template <typename Key, typename Value, typename Hasher, typename Probe>
class Ours : public base::FlatHashMap<Key, Value, Hasher, Probe> {
 public:
  struct Iterator {
    using value_type = std::pair<const Key&, Value&>;
    Iterator(const Key& k, Value& v) : pair_{k, v} {}
    value_type* operator->() { return &pair_; }
    value_type& operator*() { return pair_; }
    bool operator==(const Iterator& other) const {
      return &pair_.first == &other.pair_.first;
    }
    bool operator!=(const Iterator& other) const { return !operator==(other); }
    value_type pair_;
  };

  void insert(std::pair<Key, Value>&& pair) {
    this->Insert(std::move(pair.first), std::move(pair.second));
  }

  Iterator find(const Key& key) {
    const size_t idx = this->FindInternal(key);
    return Iterator(this->keys_[idx], this->values_[idx]);
  }

  Iterator end() {
    return Iterator(this->keys_[this->kNotFound],
                    this->values_[this->kNotFound]);
  }
};

bool IsBenchmarkFunctionalOnly() {
  return getenv("BENCHMARK_FUNCTIONAL_TEST_ONLY") != nullptr;
}

size_t num_samples() {
  return IsBenchmarkFunctionalOnly() ? size_t(100) : size_t(10 * 1000 * 1000);
}

template <typename MapType>
void BM_HashMap_InsertRandInts(benchmark::State& state) {
  std::minstd_rand0 rng(0);
  std::vector<size_t> keys(static_cast<size_t>(num_samples()));
  std::shuffle(keys.begin(), keys.end(), rng);
  for (auto _ : state) {
    MapType mapz;
    for (const auto key : keys)
      mapz.insert({key, key});
    benchmark::DoNotOptimize(mapz);
    benchmark::ClobberMemory();
  }
  state.counters["insertions"] = Counter(static_cast<double>(keys.size()),
                                         Counter::kIsIterationInvariantRate);
}

// This test is performs insertions on integers that are designed to create
// lot of clustering on the same small set of buckets.
// This covers the unlucky case of using a map with a poor hashing function.
template <typename MapType>
void BM_HashMap_InsertCollidingInts(benchmark::State& state) {
  std::vector<size_t> keys;
  const size_t kNumSamples = num_samples();

  // Generates numbers that are all distinct from each other, but that are
  // designed to collide on the same buckets.
  constexpr size_t kShift = 8;  // Collide on the same 2^8 = 256 buckets.
  for (size_t i = 0; i < kNumSamples; i++) {
    size_t bucket = i & ((1 << kShift) - 1);  // [0, 256].
    size_t multiplier = i >> kShift;          // 0,0,0... 1,1,1..., 2,2,2...
    size_t key = 8192 * multiplier + bucket;
    keys.push_back(key);
  }
  for (auto _ : state) {
    MapType mapz;
    for (const size_t key : keys)
      mapz.insert({key, key});
    benchmark::DoNotOptimize(mapz);
    benchmark::ClobberMemory();
  }
  state.counters["insertions"] = Counter(static_cast<double>(keys.size()),
                                         Counter::kIsIterationInvariantRate);
}

// Unlike the previous benchmark, here integers don't just collide on the same
// buckets, they have a large number of duplicates with the same values.
// Most of those insertions are no-ops. This tests the ability of the hashmap
// to deal with cases where the hash function is good but the insertions contain
// lot of dupes (e.g. dealing with pids).
template <typename MapType>
void BM_HashMap_InsertDupeInts(benchmark::State& state) {
  std::vector<size_t> keys;
  const size_t kNumSamples = num_samples();

  for (size_t i = 0; i < kNumSamples; i++)
    keys.push_back(i % 16384);

  for (auto _ : state) {
    MapType mapz;
    for (const size_t key : keys)
      mapz.insert({key, key});
    benchmark::DoNotOptimize(mapz);
    benchmark::ClobberMemory();
  }
  state.counters["insertions"] = Counter(static_cast<double>(keys.size()),
                                         Counter::kIsIterationInvariantRate);
}

template <typename MapType>
void BM_HashMap_LookupRandInts(benchmark::State& state) {
  std::minstd_rand0 rng(0);
  std::vector<size_t> keys(static_cast<size_t>(num_samples()));
  std::shuffle(keys.begin(), keys.end(), rng);

  MapType mapz;
  for (const size_t key : keys)
    mapz.insert({key, key});

  for (auto _ : state) {
    int64_t total = 0;
    for (const size_t key : keys) {
      auto it = mapz.find(static_cast<uint64_t>(key));
      DEJAVIEW_CHECK(it != mapz.end());
      total += it->second;
    }
    benchmark::DoNotOptimize(total);
    benchmark::ClobberMemory();
    state.counters["sum"] = Counter(static_cast<double>(total));
  }
  state.counters["lookups"] = Counter(static_cast<double>(keys.size()),
                                      Counter::kIsIterationInvariantRate);
}

}  // namespace

using Ours_LinearProbing =
    Ours<uint64_t, uint64_t, AlreadyHashed<uint64_t>, LinearProbe>;
using Ours_QuadProbing =
    Ours<uint64_t, uint64_t, AlreadyHashed<uint64_t>, QuadraticProbe>;
using Ours_QuadCompProbing =
    Ours<uint64_t, uint64_t, AlreadyHashed<uint64_t>, QuadraticHalfProbe>;
using StdUnorderedMap =
    std::unordered_map<uint64_t, uint64_t, AlreadyHashed<uint64_t>>;

#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
using RobinMap = tsl::robin_map<uint64_t, uint64_t, AlreadyHashed<uint64_t>>;
using AbslFlatHashMap =
    absl::flat_hash_map<uint64_t, uint64_t, AlreadyHashed<uint64_t>>;
using FollyF14FastMap =
    folly::F14FastMap<uint64_t, uint64_t, AlreadyHashed<uint64_t>>;
#endif

#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_InsertTraceStrings, RobinMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertTraceStrings, AbslFlatHashMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertTraceStrings, FollyF14FastMap);
#endif

#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_TraceTids, tsl::robin_map<TID_ARGS>);
BENCHMARK_TEMPLATE(BM_HashMap_TraceTids, absl::flat_hash_map<TID_ARGS>);
BENCHMARK_TEMPLATE(BM_HashMap_TraceTids, folly::F14FastMap<TID_ARGS>);
#endif

BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, Ours_LinearProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, Ours_QuadProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, StdUnorderedMap);
#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, RobinMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, AbslFlatHashMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertRandInts, FollyF14FastMap);
#endif

BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, Ours_LinearProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, Ours_QuadProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, Ours_QuadCompProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, StdUnorderedMap);
#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, RobinMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, AbslFlatHashMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertCollidingInts, FollyF14FastMap);
#endif

BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, Ours_LinearProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, Ours_QuadProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, Ours_QuadCompProbing);
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, StdUnorderedMap);
#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, RobinMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, AbslFlatHashMap);
BENCHMARK_TEMPLATE(BM_HashMap_InsertDupeInts, FollyF14FastMap);
#endif

BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, Ours_LinearProbing);
BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, Ours_QuadProbing);
BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, StdUnorderedMap);
#if defined(DEJAVIEW_HASH_MAP_COMPARE_THIRD_PARTY_LIBS)
BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, RobinMap);
BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, AbslFlatHashMap);
BENCHMARK_TEMPLATE(BM_HashMap_LookupRandInts, FollyF14FastMap);
#endif
