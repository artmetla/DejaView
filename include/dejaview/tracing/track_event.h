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

#ifndef INCLUDE_DEJAVIEW_TRACING_TRACK_EVENT_H_
#define INCLUDE_DEJAVIEW_TRACING_TRACK_EVENT_H_

#include "dejaview/tracing/internal/track_event_data_source.h"
#include "dejaview/tracing/internal/track_event_internal.h"
#include "dejaview/tracing/internal/track_event_macros.h"
#include "dejaview/tracing/string_helpers.h"
#include "dejaview/tracing/track.h"
#include "dejaview/tracing/track_event_category_registry.h"
#include "protos/dejaview/trace/track_event/track_event.pbzero.h"

#include <type_traits>

// This file contains a set of macros designed for instrumenting applications
// with track event trace points. While the underlying TrackEvent API can also
// be used directly, doing so efficiently requires some care (e.g., to avoid
// evaluating arguments while tracing is disabled). These types of optimizations
// are abstracted away by the macros below.
//
// ================
// Quickstart guide
// ================
//
//   To add track events to your application, first define your categories in,
//   e.g., my_tracing.h:
//
//       DEJAVIEW_DEFINE_CATEGORIES(
//           dejaview::Category("base"),
//           dejaview::Category("v8"),
//           dejaview::Category("cc"));
//
//   Then in a single .cc file, e.g., my_tracing.cc:
//
//       #include "my_tracing.h"
//       DEJAVIEW_TRACK_EVENT_STATIC_STORAGE();
//
//   Finally, register track events at startup, after which you can record
//   events with the TRACE_EVENT macros:
//
//       #include "my_tracing.h"
//
//       int main() {
//         dejaview::TrackEvent::Register();
//
//         // A basic track event with just a name.
//         TRACE_EVENT("category", "MyEvent");
//
//         // A track event with (up to two) debug annotations.
//         TRACE_EVENT("category", "MyEvent", "parameter", 42);
//
//         // A track event with a strongly typed parameter.
//         TRACE_EVENT("category", "MyEvent", [](dejaview::EventContext ctx) {
//           ctx.event()->set_foo(42);
//           ctx.event()->set_bar(.5f);
//         });
//       }
//
//  Note that track events must be nested consistently, i.e., the following is
//  not allowed:
//
//    TRACE_EVENT_BEGIN("a", "bar", ...);
//    TRACE_EVENT_BEGIN("b", "foo", ...);
//    TRACE_EVENT_END("a");  // "foo" must be closed before "bar".
//    TRACE_EVENT_END("b");
//
// ====================
// Implementation notes
// ====================
//
// The track event library consists of the following layers and components. The
// classes the internal namespace shouldn't be considered part of the public
// API.
//                    .--------------------------------.
//               .----|  TRACE_EVENT                   |----.
//      write   |     |   - App instrumentation point  |     |  write
//      event   |     '--------------------------------'     |  arguments
//              V                                            V
//  .----------------------------------.    .-----------------------------.
//  | TrackEvent                       |    | EventContext                |
//  |  - Registry of event categories  |    |  - One track event instance |
//  '----------------------------------'    '-----------------------------'
//              |                                            |
//              |                                            | look up
//              | is                                         | interning ids
//              V                                            V
//  .----------------------------------.    .-----------------------------.
//  | internal::TrackEventDataSource   |    | TrackEventInternedDataIndex |
//  | - DejaView data source           |    | - Corresponds to a field in |
//  | - Has TrackEventIncrementalState |    |   in interned_data.proto    |
//  '----------------------------------'    '-----------------------------'
//              |                  |                         ^
//              |                  |       owns (1:many)     |
//              | write event      '-------------------------'
//              V
//  .----------------------------------.
//  | internal::TrackEventInternal     |
//  | - Outlined code to serialize     |
//  |   one track event                |
//  '----------------------------------'
//

// DEPRECATED: Please use DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE to implement
// multiple track event category sets in one program.
//
// Each compilation unit can be in exactly one track event namespace,
// allowing the overall program to use multiple track event data sources and
// category lists if necessary. Use this macro to select the namespace for the
// current compilation unit.
//
// If the program uses multiple track event namespaces, category & track event
// registration (see quickstart above) needs to happen for both namespaces
// separately.

#ifndef DEJAVIEW_TRACK_EVENT_NAMESPACE
#define DEJAVIEW_TRACK_EVENT_NAMESPACE dejaview_track_event
#endif

// Deprecated; see dejaview::Category().
#define DEJAVIEW_CATEGORY(name) \
  ::dejaview::Category {        \
    #name                       \
  }

// Internal helpers for determining if a given category is defined at build or
// runtime.
namespace DEJAVIEW_TRACK_EVENT_NAMESPACE {
namespace internal {

// By default no statically defined categories are dynamic, but this can be
// overridden with DEJAVIEW_DEFINE_TEST_CATEGORY_PREFIXES.
template <typename... T>
constexpr bool IsDynamicCategory(const char*) {
  return false;
}

// Explicitly dynamic categories are always dynamic.
constexpr bool IsDynamicCategory(const ::dejaview::DynamicCategory&) {
  return true;
}

}  // namespace internal
}  // namespace DEJAVIEW_TRACK_EVENT_NAMESPACE

// Normally all categories are defined statically at build-time (see
// DEJAVIEW_DEFINE_CATEGORIES). However, some categories are only used for
// testing, and we shouldn't publish them to the tracing service or include them
// in a production binary. Use this macro to define a list of prefixes for these
// types of categories. Note that trace points using these categories will be
// slightly less efficient compared to regular trace points.
#define DEJAVIEW_DEFINE_TEST_CATEGORY_PREFIXES(...)                       \
  namespace DEJAVIEW_TRACK_EVENT_NAMESPACE {                              \
  namespace internal {                                                    \
  template <>                                                             \
  constexpr bool IsDynamicCategory(const char* name) {                    \
    return ::dejaview::internal::IsStringInPrefixList(name, __VA_ARGS__); \
  }                                                                       \
  } /* namespace internal */                                              \
  } /* namespace DEJAVIEW_TRACK_EVENT_NAMESPACE */                        \
  DEJAVIEW_INTERNAL_SWALLOW_SEMICOLON()

// Register the set of available categories by passing a list of categories to
// this macro: dejaview::Category("cat1"), dejaview::Category("cat2"), ...
// `ns` is the name of the namespace in which the categories should be declared.
// `attrs` are linkage attributes for the underlying data source. See
// DEJAVIEW_DECLARE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS.
//
// Implementation note: the extra namespace (DEJAVIEW_TRACK_EVENT_NAMESPACE) is
// kept here only for backward compatibility.
#define DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(ns, attrs, ...) \
  namespace ns {                                                           \
  namespace DEJAVIEW_TRACK_EVENT_NAMESPACE {                               \
  /* The list of category names */                                         \
  DEJAVIEW_INTERNAL_DECLARE_CATEGORIES(attrs, __VA_ARGS__)                 \
  /* The track event data source for this set of categories */             \
  DEJAVIEW_INTERNAL_DECLARE_TRACK_EVENT_DATA_SOURCE(attrs);                \
  } /* namespace DEJAVIEW_TRACK_EVENT_NAMESPACE  */                        \
  using DEJAVIEW_TRACK_EVENT_NAMESPACE::TrackEvent;                        \
  } /* namespace ns */                                                     \
  DEJAVIEW_DECLARE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(                  \
      attrs, ns::DEJAVIEW_TRACK_EVENT_NAMESPACE::TrackEvent,               \
      ::dejaview::internal::TrackEventDataSourceTraits)

// Register the set of available categories by passing a list of categories to
// this macro: dejaview::Category("cat1"), dejaview::Category("cat2"), ...
// `ns` is the name of the namespace in which the categories should be declared.
#define DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE(ns, ...) \
  DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(    \
      ns, DEJAVIEW_COMPONENT_EXPORT, __VA_ARGS__)

// Make categories in a given namespace the default ones used by track events
// for the current translation unit. Can only be used *once* in a given global
// or namespace scope.
#define DEJAVIEW_USE_CATEGORIES_FROM_NAMESPACE(ns)                         \
  namespace DEJAVIEW_TRACK_EVENT_NAMESPACE {                               \
  using ::ns::DEJAVIEW_TRACK_EVENT_NAMESPACE::TrackEvent;                  \
  namespace internal {                                                     \
  using ::ns::DEJAVIEW_TRACK_EVENT_NAMESPACE::internal::kCategoryRegistry; \
  using ::ns::DEJAVIEW_TRACK_EVENT_NAMESPACE::internal::                   \
      kConstExprCategoryRegistry;                                          \
  } /* namespace internal */                                               \
  } /* namespace DEJAVIEW_TRACK_EVENT_NAMESPACE */                         \
  DEJAVIEW_INTERNAL_SWALLOW_SEMICOLON()

// Make categories in a given namespace the default ones used by track events
// for the current block scope. Can only be used in a function or block scope.
#define DEJAVIEW_USE_CATEGORIES_FROM_NAMESPACE_SCOPED(ns) \
  namespace DEJAVIEW_TRACK_EVENT_NAMESPACE = ns::DEJAVIEW_TRACK_EVENT_NAMESPACE

// Register categories in the default (global) namespace. Warning: only one set
// of global categories can be defined in a single program. Create namespaced
// categories with DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE to work around this
// limitation.
#define DEJAVIEW_DEFINE_CATEGORIES(...)                           \
  DEJAVIEW_DEFINE_CATEGORIES_IN_NAMESPACE(dejaview, __VA_ARGS__); \
  DEJAVIEW_USE_CATEGORIES_FROM_NAMESPACE(dejaview)

// Allocate storage for each category by using this macro once per track event
// namespace. `ns` is the name of the namespace in which the categories should
// be declared and `attrs` specify linkage attributes for the data source.
#define DEJAVIEW_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE_WITH_ATTRS(ns, attrs) \
  namespace ns {                                                               \
  namespace DEJAVIEW_TRACK_EVENT_NAMESPACE {                                   \
  DEJAVIEW_INTERNAL_CATEGORY_STORAGE(attrs)                                    \
  DEJAVIEW_INTERNAL_DEFINE_TRACK_EVENT_DATA_SOURCE()                           \
  } /* namespace DEJAVIEW_TRACK_EVENT_NAMESPACE */                             \
  } /* namespace ns */                                                         \
  DEJAVIEW_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(                       \
      attrs, ns::DEJAVIEW_TRACK_EVENT_NAMESPACE::TrackEvent,                   \
      ::dejaview::internal::TrackEventDataSourceTraits)

// Allocate storage for each category by using this macro once per track event
// namespace.
#define DEJAVIEW_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(ns)   \
  DEJAVIEW_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE_WITH_ATTRS( \
      ns, DEJAVIEW_COMPONENT_EXPORT)

// Allocate storage for each category by using this macro once per track event
// namespace.
#define DEJAVIEW_TRACK_EVENT_STATIC_STORAGE() \
  DEJAVIEW_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(dejaview)

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC system_header
#endif

// Begin a slice under |category| with the title |name|. Both strings must be
// static constants. The track event is only recorded if |category| is enabled
// for a tracing session.
//
// The slice is thread-scoped (i.e., written to the default track of the current
// thread) unless overridden with a custom track object (see Track).
//
// |name| must be a string with static lifetime (i.e., the same
// address must not be used for a different event name in the future). If you
// want to use a dynamically allocated name, do this:
//
//  TRACE_EVENT("category", nullptr, [&](dejaview::EventContext ctx) {
//    ctx.event()->set_name(dynamic_name);
//  });
//
// The following optional arguments can be passed to `TRACE_EVENT` to add extra
// information to events:
//
// TRACE_EVENT("cat", "name"[, track][, timestamp]
//                          [, "debug_name1", debug_value1]
//                          [, "debug_name2", debug_value2]
//                          ...
//                          [, "debug_nameN", debug_valueN]
//                          [, lambda]);
//
// Some examples of valid combinations:
//
// 1. A lambda for writing custom TrackEvent fields:
//
//   TRACE_EVENT("category", "Name", [&](dejaview::EventContext ctx) {
//     ctx.event()->set_custom_value(...);
//   });
//
// 2. A timestamp and a lambda:
//
//   TRACE_EVENT("category", "Name", time_in_nanoseconds,
//       [&](dejaview::EventContext ctx) {
//     ctx.event()->set_custom_value(...);
//   });
//
//   |time_in_nanoseconds| should be an uint64_t by default. To support custom
//   timestamp types,
//   |dejaview::TraceTimestampTraits<T>::ConvertTimestampToTraceTimeNs|
//   should be defined. See |ConvertTimestampToTraceTimeNs| for more details.
//
// 3. Arbitrary number of debug annotations:
//
//   TRACE_EVENT("category", "Name", "arg", value);
//   TRACE_EVENT("category", "Name", "arg", value, "arg2", value2);
//   TRACE_EVENT("category", "Name", "arg", value, "arg2", value2,
//                                   "arg3", value3);
//
//   See |TracedValue| for recording custom types as debug annotations.
//
// 4. Arbitrary number of debug annotations and a lambda:
//
//   TRACE_EVENT("category", "Name", "arg", value,
//       [&](dejaview::EventContext ctx) {
//     ctx.event()->set_custom_value(...);
//   });
//
// 5. An overridden track:
//
//   TRACE_EVENT("category", "Name", dejaview::Track(1234));
//
//   See |Track| for other types of tracks which may be used.
//
// 6. A track and a lambda:
//
//   TRACE_EVENT("category", "Name", dejaview::Track(1234),
//       [&](dejaview::EventContext ctx) {
//     ctx.event()->set_custom_value(...);
//   });
//
// 7. A track and a timestamp:
//
//   TRACE_EVENT("category", "Name", dejaview::Track(1234),
//       time_in_nanoseconds);
//
// 8. A track, a timestamp and a lambda:
//
//   TRACE_EVENT("category", "Name", dejaview::Track(1234),
//       time_in_nanoseconds, [&](dejaview::EventContext ctx) {
//     ctx.event()->set_custom_value(...);
//   });
//
// 9. A track and an arbitrary number of debug annotions:
//
//   TRACE_EVENT("category", "Name", dejaview::Track(1234),
//               "arg", value);
//   TRACE_EVENT("category", "Name", dejaview::Track(1234),
//               "arg", value, "arg2", value2);
//
#define TRACE_EVENT_BEGIN(category, name, ...) \
  DEJAVIEW_INTERNAL_TRACK_EVENT_WITH_METHOD(   \
      TraceForCategory, category, name,        \
      ::dejaview::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN, ##__VA_ARGS__)

// End a slice under |category|.
#define TRACE_EVENT_END(category, ...)              \
  DEJAVIEW_INTERNAL_TRACK_EVENT_WITH_METHOD(        \
      TraceForCategory, category, /*name=*/nullptr, \
      ::dejaview::protos::pbzero::TrackEvent::TYPE_SLICE_END, ##__VA_ARGS__)

// Begin a slice which gets automatically closed when going out of scope.
#define TRACE_EVENT(category, name, ...) \
  DEJAVIEW_INTERNAL_SCOPED_TRACK_EVENT(category, name, ##__VA_ARGS__)

// Emit a slice which has zero duration.
#define TRACE_EVENT_INSTANT(category, name, ...) \
  DEJAVIEW_INTERNAL_TRACK_EVENT_WITH_METHOD(     \
      TraceForCategory, category, name,          \
      ::dejaview::protos::pbzero::TrackEvent::TYPE_INSTANT, ##__VA_ARGS__)

// Efficiently determine if the given static or dynamic trace category or
// category group is enabled for tracing.
#define TRACE_EVENT_CATEGORY_ENABLED(category) \
  DEJAVIEW_INTERNAL_CATEGORY_ENABLED(category)

// Time-varying numeric data can be recorded with the TRACE_COUNTER macro:
//
//   TRACE_COUNTER("cat", counter_track[, timestamp], value);
//
// For example, to record a single value for a counter called "MyCounter":
//
//   TRACE_COUNTER("category", "MyCounter", 1234.5);
//
// This data is displayed as a counter track in the DejaView UI.
//
// Both integer and floating point counter values are supported. Counters can
// also be annotated with additional information such as units, for example, for
// tracking the rendering framerate in terms of frames per second or "fps":
//
//   TRACE_COUNTER("category", dejaview::CounterTrack("Framerate", "fps"), 120);
//
// As another example, a memory counter that records bytes but accepts samples
// as kilobytes (to reduce trace binary size) can be defined like this:
//
//   dejaview::CounterTrack memory_track = dejaview::CounterTrack("Memory")
//       .set_unit("bytes")
//       .set_multiplier(1024);
//   TRACE_COUNTER("category", memory_track, 4 /* = 4096 bytes */);
//
// See /protos/dejaview/trace/track_event/counter_descriptor.proto
// for the full set of attributes for a counter track.
//
// To record a counter value at a specific point in time (instead of the current
// time), you can pass in a custom timestamp:
//
//   // First record the current time and counter value.
//   uint64_t timestamp = dejaview::TrackEvent::GetTraceTimeNs();
//   int64_t value = 1234;
//
//   // Later, emit a sample at that point in time.
//   TRACE_COUNTER("category", "MyCounter", timestamp, value);
//
#define TRACE_COUNTER(category, track, ...)                 \
  DEJAVIEW_INTERNAL_TRACK_EVENT_WITH_METHOD(                \
      TraceForCategory, category, /*name=*/nullptr,         \
      ::dejaview::protos::pbzero::TrackEvent::TYPE_COUNTER, \
      ::dejaview::CounterTrack(track), ##__VA_ARGS__)

// TODO(skyostil): Add flow events.

#endif  // INCLUDE_DEJAVIEW_TRACING_TRACK_EVENT_H_
