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

#ifndef INCLUDE_DEJAVIEW_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_
#define INCLUDE_DEJAVIEW_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_

#include "dejaview/base/flat_set.h"
#include "dejaview/protozero/scattered_heap_buffer.h"
#include "dejaview/tracing/core/forward_decls.h"
#include "dejaview/tracing/data_source.h"
#include "dejaview/tracing/debug_annotation.h"
#include "dejaview/tracing/trace_writer_base.h"
#include "dejaview/tracing/traced_value.h"
#include "dejaview/tracing/track.h"
#include "protos/dejaview/common/builtin_clock.pbzero.h"
#include "protos/dejaview/trace/interned_data/interned_data.pbzero.h"
#include "protos/dejaview/trace/track_event/track_event.pbzero.h"

#include <unordered_map>

namespace dejaview {

// Represents a point in time for the clock specified by |clock_id|.
struct TraceTimestamp {
  // Clock IDs have the following semantic:
  // [1, 63]:    Builtin types, see BuiltinClock from
  //             ../common/builtin_clock.proto.
  // [64, 127]:  User-defined clocks. These clocks are sequence-scoped. They
  //             are only valid within the same |trusted_packet_sequence_id|
  //             (i.e. only for TracePacket(s) emitted by the same TraceWriter
  //             that emitted the clock snapshot).
  // [128, MAX]: Reserved for future use. The idea is to allow global clock
  //             IDs and setting this ID to hash(full_clock_name) & ~127.
  // Learn more: `clock_snapshot.proto`
  uint32_t clock_id;
  uint64_t value;
};

class EventContext;
class TrackEventSessionObserver;
struct Category;
struct TraceTimestamp;
namespace protos {
namespace gen {
class TrackEventConfig;
}  // namespace gen
namespace pbzero {
class DebugAnnotation;
}  // namespace pbzero
}  // namespace protos

// A callback interface for observing track event tracing sessions starting and
// stopping. See TrackEvent::{Add,Remove}SessionObserver. Note that all methods
// will be called on an internal DejaView thread.
class DEJAVIEW_EXPORT_COMPONENT TrackEventSessionObserver {
 public:
  virtual ~TrackEventSessionObserver();
  // Called when a track event tracing session is configured. Note tracing isn't
  // active yet, so track events emitted here won't be recorded. See
  // DataSourceBase::OnSetup.
  virtual void OnSetup(const DataSourceBase::SetupArgs&);
  // Called when a track event tracing session is started. It is possible to
  // emit track events from this callback.
  virtual void OnStart(const DataSourceBase::StartArgs&);
  // Called when a track event tracing session is stopped. It is still possible
  // to emit track events from this callback.
  virtual void OnStop(const DataSourceBase::StopArgs&);
  // Called when tracing muxer requests to clear incremental state.
  virtual void WillClearIncrementalState(
      const DataSourceBase::ClearIncrementalStateArgs&);
};

// A class that the embedder can store arbitrary data user data per thread.
class DEJAVIEW_EXPORT_COMPONENT TrackEventTlsStateUserData {
 public:
  TrackEventTlsStateUserData() = default;
  // Not clonable.
  TrackEventTlsStateUserData(const TrackEventTlsStateUserData&) = delete;
  TrackEventTlsStateUserData& operator=(const TrackEventTlsStateUserData&) =
      delete;

  virtual ~TrackEventTlsStateUserData();
};

namespace internal {
class TrackEventCategoryRegistry;

class DEJAVIEW_EXPORT_COMPONENT BaseTrackEventInternedDataIndex {
 public:
  virtual ~BaseTrackEventInternedDataIndex();

#if DEJAVIEW_DCHECK_IS_ON()
  const char* type_id_ = nullptr;
  const void* add_function_ptr_ = nullptr;
#endif  // DEJAVIEW_DCHECK_IS_ON()
};

struct TrackEventTlsState {
  template <typename TraceContext>
  explicit TrackEventTlsState(const TraceContext& trace_context);
  bool enable_thread_time_sampling = false;
  bool filter_debug_annotations = false;
  bool filter_dynamic_event_names = false;
  uint64_t timestamp_unit_multiplier = 1;
  uint32_t default_clock;
  std::map<const void*, std::unique_ptr<TrackEventTlsStateUserData>> user_data;
};

struct TrackEventIncrementalState {
  static constexpr size_t kMaxInternedDataFields = 32;

  // Packet-sequence-scoped clock that encodes nanosecond timestamps in the
  // domain of the clock returned by GetClockId() as delta values - see
  // Clock::is_incremental in dejaview/trace/clock_snapshot.proto.
  // Default unit: nanoseconds.
  static constexpr uint32_t kClockIdIncremental = 64;

  // Packet-sequence-scoped clock that encodes timestamps in the domain of the
  // clock returned by GetClockId() with custom unit_multiplier.
  // Default unit: nanoseconds.
  static constexpr uint32_t kClockIdAbsolute = 65;

  bool was_cleared = true;

  // A heap-allocated message for storing newly seen interned data while we are
  // in the middle of writing a track event. When a track event wants to write
  // new interned data into the trace, it is first serialized into this message
  // and then flushed to the real trace in EventContext when the packet ends.
  // The message is cached here as a part of incremental state so that we can
  // reuse the underlying buffer allocation for subsequently written interned
  // data.
  protozero::HeapBuffered<protos::pbzero::InternedData>
      serialized_interned_data;

  // In-memory indices for looking up interned data ids.
  // For each intern-able field (up to a max of 32) we keep a dictionary of
  // field-value -> interning-key. Depending on the type we either keep the full
  // value or a hash of it (See track_event_interned_data_index.h)
  using InternedDataIndex =
      std::pair</* interned_data.proto field number */ size_t,
                std::unique_ptr<BaseTrackEventInternedDataIndex>>;
  std::array<InternedDataIndex, kMaxInternedDataFields> interned_data_indices =
      {};

  // Track uuids for which we have written descriptors into the trace. If a
  // trace event uses a track which is not in this set, we'll write out a
  // descriptor for it.
  base::FlatSet<uint64_t> seen_tracks;

  // Dynamically registered category names that have been encountered during
  // this tracing session. The value in the map indicates whether the category
  // is enabled or disabled.
  std::unordered_map<std::string, bool> dynamic_categories;

  // The latest reference timestamp that was used in a TracePacket or in a
  // ClockSnapshot. The increment between this timestamp and the current trace
  // time (GetTimeNs) is a value in kClockIdIncremental's domain.
  uint64_t last_timestamp_ns = 0;

  // The latest known counter values that was used in a TracePacket for each
  // counter track. The key (uint64_t) is the uuid of counter track.
  // The value is used for delta encoding of counter values.
  std::unordered_map<uint64_t, int64_t> last_counter_value_per_track;
  int64_t last_thread_time_ns = 0;
};

// The backend portion of the track event trace point implemention. Outlined to
// a separate .cc file so it can be shared by different track event category
// namespaces.
class DEJAVIEW_EXPORT_COMPONENT TrackEventInternal {
 public:
  static bool Initialize(
      const TrackEventCategoryRegistry&,
      bool (*register_data_source)(const DataSourceDescriptor&));

  static bool AddSessionObserver(const TrackEventCategoryRegistry&,
                                 TrackEventSessionObserver*);
  static void RemoveSessionObserver(const TrackEventCategoryRegistry&,
                                    TrackEventSessionObserver*);

  static void EnableTracing(const TrackEventCategoryRegistry& registry,
                            const protos::gen::TrackEventConfig& config,
                            const DataSourceBase::SetupArgs&);
  static void OnStart(const TrackEventCategoryRegistry&,
                      const DataSourceBase::StartArgs&);
  static void OnStop(const TrackEventCategoryRegistry&,
                     const DataSourceBase::StopArgs&);
  static void DisableTracing(const TrackEventCategoryRegistry& registry,
                             uint32_t internal_instance_index);
  static void WillClearIncrementalState(
      const TrackEventCategoryRegistry&,
      const DataSourceBase::ClearIncrementalStateArgs&);

  static bool IsCategoryEnabled(const TrackEventCategoryRegistry& registry,
                                const protos::gen::TrackEventConfig& config,
                                const Category& category);

  static void WriteEventName(dejaview::DynamicString event_name,
                             dejaview::EventContext& event_ctx,
                             const TrackEventTlsState&);

  static void WriteEventName(dejaview::StaticString event_name,
                             dejaview::EventContext& event_ctx,
                             const TrackEventTlsState&);

  static dejaview::EventContext WriteEvent(
      TraceWriterBase*,
      TrackEventIncrementalState*,
      TrackEventTlsState& tls_state,
      const Category* category,
      dejaview::protos::pbzero::TrackEvent::Type,
      const TraceTimestamp& timestamp,
      bool on_current_thread_track);

  static void ResetIncrementalStateIfRequired(
      TraceWriterBase* trace_writer,
      TrackEventIncrementalState* incr_state,
      const TrackEventTlsState& tls_state,
      const TraceTimestamp& timestamp) {
    if (incr_state->was_cleared) {
      incr_state->was_cleared = false;
      ResetIncrementalState(trace_writer, incr_state, tls_state, timestamp);
    }
  }

  // TODO(altimin): Remove this method once Chrome uses
  // EventContext::AddDebugAnnotation directly.
  template <typename NameType, typename ValueType>
  static void AddDebugAnnotation(dejaview::EventContext* event_ctx,
                                 NameType&& name,
                                 ValueType&& value) {
    auto annotation =
        AddDebugAnnotation(event_ctx, std::forward<NameType>(name));
    WriteIntoTracedValue(
        internal::CreateTracedValueFromProto(annotation, event_ctx),
        std::forward<ValueType>(value));
  }

  // If the given track hasn't been seen by the trace writer yet, write a
  // descriptor for it into the trace. Doesn't take a lock unless the track
  // descriptor is new.
  template <typename TrackType>
  static void WriteTrackDescriptorIfNeeded(
      const TrackType& track,
      TraceWriterBase* trace_writer,
      TrackEventIncrementalState* incr_state,
      const TrackEventTlsState& tls_state,
      const TraceTimestamp& timestamp) {
    auto it_and_inserted = incr_state->seen_tracks.insert(track.uuid);
    if (DEJAVIEW_LIKELY(!it_and_inserted.second))
      return;
    WriteTrackDescriptor(track, trace_writer, incr_state, tls_state, timestamp);
  }

  // Unconditionally write a track descriptor into the trace.
  template <typename TrackType>
  static void WriteTrackDescriptor(const TrackType& track,
                                   TraceWriterBase* trace_writer,
                                   TrackEventIncrementalState* incr_state,
                                   const TrackEventTlsState& tls_state,
                                   const TraceTimestamp& timestamp) {
    ResetIncrementalStateIfRequired(trace_writer, incr_state, tls_state,
                                    timestamp);
    TrackRegistry::Get()->SerializeTrack(
        track, NewTracePacket(trace_writer, incr_state, tls_state, timestamp));
  }

  // Get the current time in nanoseconds in the trace clock timebase.
  static uint64_t GetTimeNs();

  static TraceTimestamp GetTraceTime();

  static inline protos::pbzero::BuiltinClock GetClockId() { return clock_; }
  static inline void SetClockId(protos::pbzero::BuiltinClock clock) {
    clock_ = clock;
  }

  static inline bool GetDisallowMergingWithSystemTracks() {
    return disallow_merging_with_system_tracks_;
  }
  static inline void SetDisallowMergingWithSystemTracks(
      bool disallow_merging_with_system_tracks) {
    disallow_merging_with_system_tracks_ = disallow_merging_with_system_tracks;
  }

  static int GetSessionCount();

  // Represents the default track for the calling thread.
  static const Track kDefaultTrack;

 private:
  static void ResetIncrementalState(TraceWriterBase* trace_writer,
                                    TrackEventIncrementalState* incr_state,
                                    const TrackEventTlsState& tls_state,
                                    const TraceTimestamp& timestamp);

  static protozero::MessageHandle<protos::pbzero::TracePacket> NewTracePacket(
      TraceWriterBase*,
      TrackEventIncrementalState*,
      const TrackEventTlsState& tls_state,
      TraceTimestamp,
      uint32_t seq_flags =
          protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);

  static protos::pbzero::DebugAnnotation* AddDebugAnnotation(
      dejaview::EventContext*,
      const char* name);

  static protos::pbzero::DebugAnnotation* AddDebugAnnotation(
      dejaview::EventContext*,
      dejaview::DynamicString name);

  static std::atomic<int> session_count_;

  static protos::pbzero::BuiltinClock clock_;
  static bool disallow_merging_with_system_tracks_;
};

template <typename TraceContext>
TrackEventTlsState::TrackEventTlsState(const TraceContext& trace_context) {
  auto locked_ds = trace_context.GetDataSourceLocked();
  bool disable_incremental_timestamps = false;
  if (locked_ds.valid()) {
    const auto& config = locked_ds->GetConfig();
    disable_incremental_timestamps = config.disable_incremental_timestamps();
    filter_debug_annotations = config.filter_debug_annotations();
    filter_dynamic_event_names = config.filter_dynamic_event_names();
    enable_thread_time_sampling = config.enable_thread_time_sampling();
    if (config.has_timestamp_unit_multiplier()) {
      timestamp_unit_multiplier = config.timestamp_unit_multiplier();
    }
  }
  if (disable_incremental_timestamps) {
    if (timestamp_unit_multiplier == 1) {
      default_clock = static_cast<uint32_t>(TrackEventInternal::GetClockId());
    } else {
      default_clock = TrackEventIncrementalState::kClockIdAbsolute;
    }
  } else {
    default_clock = TrackEventIncrementalState::kClockIdIncremental;
  }
}

}  // namespace internal
}  // namespace dejaview

#endif  // INCLUDE_DEJAVIEW_TRACING_INTERNAL_TRACK_EVENT_INTERNAL_H_
