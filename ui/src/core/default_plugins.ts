// Copyright (C) 2024 The Android Open Source Project
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

// Plugins in this list will run by default when users boot up the UI.
// Users may choose to enable plugins which are not in this list, but they will
// need to do this manually.
// In order to get a plugin into this list it must:
// - Use only the available plugin API (no hacks).
// - Follow naming conventions for tracks and plugins.
// - Not directly rely on any other plugins.
// - Be approved by one of DejaView UI owners.
export const defaultPlugins = [
  'dev.dejaview.DeeplinkQuerystring',
  'dev.dejaview.RestorePinnedTrack',
  'dev.dejaview.TimelineSync',
  'dejaview.AndroidLog',
  'dejaview.Annotation',
  'dejaview.AsyncSlices',
  'dejaview.CoreCommands',
  'dejaview.Counter',
  'dejaview.CpuFreq',
  'dejaview.CpuProfile',
  'dejaview.CpuSlices',
  'dejaview.CriticalPath',
  'dejaview.DebugTracks',
  'dejaview.Frames',
  'dejaview.FtraceRaw',
  'dejaview.HeapProfile',
  'dejaview.PerfSamplesProfile',
  'dejaview.Process',
  'dejaview.ProcessSummary',
  'dejaview.ProcessThreadGroups',
  'dejaview.Sched',
  'dejaview.Slice',
  'dejaview.Thread',
  'dejaview.ThreadState',
  'dejaview.TrackUtils',
];
