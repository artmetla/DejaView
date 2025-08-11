// Copyright (C) 2018 The Android Open Source Project
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

import {time} from '../base/time';
import {TraceSource} from '../public/trace_source';

/**
 * A plain js object, holding objects of type |Class| keyed by string id.
 * We use this instead of using |Map| object since it is simpler and faster to
 * serialize for use in postMessage.
 */
export interface ObjectById<Class extends {id: string}> {
  [id: string]: Class;
}

// Same as ObjectById but the key parameter is called `key` rather than `id`.
export interface ObjectByKey<Class extends {key: string}> {
  [key: string]: Class;
}

export const MAX_TIME = 180;

// 3: TrackKindPriority and related sorting changes.
// 5: Move a large number of items off frontendLocalState and onto state.
// 6: Common PivotTableConfig and pivot table specific PivotTableState.
// 7: Split Chrome categories in two and add 'symbolize ksyms' flag.
// 8: Rename several variables
// "[...]HeapProfileFlamegraph[...]" -> "[...]Flamegraph[...]".
// 9: Add a field to track last loaded recording profile name
// 10: Change last loaded profile tracking type to accommodate auto-save.
// 11: Rename updateChromeCategories to fetchChromeCategories.
// 12: Add a field to cache mapping from UI track ID to trace track ID in order
//     to speed up flow arrows rendering.
// 13: FlamegraphState changed to support area selection.
// 14: Changed the type of uiTrackIdByTraceTrackId from `Map` to an object with
// typed key/value because a `Map` does not preserve type during
// serialisation+deserialisation.
// 15: Added state for Pivot Table V2
// 16: Added boolean tracking if the flamegraph modal was dismissed
// 17:
// - add currentEngineId to track the id of the current engine
// - remove nextNoteId, nextAreaId and use nextId as a unique counter for all
//   indexing except the indexing of the engines
// 18: areaSelection change see b/235869542
// 19: Added visualisedArgs state.
// 20: Refactored thread sorting order.
// 21: Updated perf sample selection to include a ts range instead of single ts
// 22: Add log selection kind.
// 23: Add log filtering criteria for Android log entries.
// 24: Store only a single Engine.
// 25: Move omnibox state off VisibleState.
// 26: Add tags for filtering Android log entries.
// 27. Add a text entry for filtering Android log entries.
// 28. Add a boolean indicating if non matching log entries are hidden.
// 29. Add ftrace state. <-- Borked, state contains a non-serializable object.
// 30. Convert ftraceFilter.excludedNames from Set<string> to string[].
// 31. Convert all timestamps to bigints.
// 32. Add pendingDeeplink.
// 33. Add plugins state.
// 34. Add additional pendingDeeplink fields (query, pid).
// 35. Add force to OmniboxState
// 36. Remove metrics
// 37. Add additional pendingDeeplink fields (visStart, visEnd).
// 38. Add track tags.
// 39. Ported cpu_slice, ftrace, and android_log tracks to plugin tracks. Track
//     state entries now require a URI and old track implementations are no
//     longer registered.
// 40. Ported counter, process summary/sched, & cpu_freq to plugin tracks.
// 41. Ported all remaining tracks.
// 42. Rename trackId -> trackKey.
// 43. Remove visibleTracks.
// 44. Add TabsV2 state.
// 45. Remove v1 tracks.
// 46. Remove trackKeyByTrackId.
// 47. Selection V2
// 48. Rename legacySelection -> selection and introduce new Selection type.
// 49. Remove currentTab, which is only relevant to TabsV1.
// 50. Remove ftrace filter state.
// 51. Changed structure of FlamegraphState.expandedCallsiteByViewingOption.
// 52. Update track group state - don't make the summary track the first track.
// 53. Remove android log state.
// 54. Remove traceTime.
// 55. Rename TrackGroupState.id -> TrackGroupState.key.
// 56. Renamed chrome slice to thread slice everywhere.
// 57. Remove flamegraph related code from state.
// 58. Remove area map.
// 59. Deprecate old area selection type.
// 60. Deprecate old note selection type.
// 61. Remove params/state from TrackState.
export const STATE_VERSION = 61;

export const SCROLLING_TRACK_GROUP = 'ScrollingTracks';

export interface EngineConfig {
  id: string;
  source: TraceSource;
}

export interface QueryConfig {
  id: string;
  engineId?: string;
  query: string;
}

export interface Pagination {
  offset: number;
  count: number;
}

export interface LoadedConfigNone {
  type: 'NONE';
}

export interface LoadedConfigAutomatic {
  type: 'AUTOMATIC';
}

export interface LoadedConfigNamed {
  type: 'NAMED';
  name: string;
}

export type LoadedConfig =
  | LoadedConfigNone
  | LoadedConfigAutomatic
  | LoadedConfigNamed;

export interface PendingDeeplinkState {
  ts?: string;
  dur?: string;
  tid?: string;
  pid?: string;
  query?: string;
  visStart?: string;
  visEnd?: string;
}

export interface State {
  version: number;

  /**
   * State of the ConfigEditor.
   */
  displayConfigAsPbtxt: boolean;
  lastLoadedConfig: LoadedConfig;

  /**
   * Open traces.
   */
  engine?: EngineConfig;

  debugTrackId?: string;
  lastTrackReloadRequest?: number;
  flamegraphModalDismissed: boolean;

  // Show track perf debugging overlay
  perfDebug: boolean;

  // Hovered and focused events
  hoveredUtid: number;
  hoveredPid: number;
  hoveredNoteTimestamp: time;
  highlightedSliceId: number;

  trackFilterTerm: string | undefined;

  // TODO(primiano): this is a hack to force-re-run controllers required for the
  // controller->managers migration. Remove once controllers are gone.
  forceRunControllers: number;
}
