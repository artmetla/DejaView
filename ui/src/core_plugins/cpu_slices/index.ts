// Copyright (C) 2021 The Android Open Source Project
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

import {CPU_SLICE_TRACK_KIND} from '../../public/track_kinds';
import {SchedSliceDetailsPanel} from './sched_details_tab';
import {Trace} from '../../public/trace';
import {DejaViewPlugin, PluginDescriptor} from '../../public/plugin';
import {NUM} from '../../trace_processor/query_result';
import {CpuSliceTrack} from './cpu_slice_track';
import {TrackNode} from '../../public/workspace';
import {CpuSliceSelectionAggregator} from './cpu_slice_selection_aggregator';
import {CpuSliceByProcessSelectionAggregator} from './cpu_slice_by_process_selection_aggregator';

function uriForSchedTrack(cpu: number): string {
  return `/sched_cpu${cpu}`;
}

class CpuSlices implements DejaViewPlugin {
  async onTraceLoad(ctx: Trace): Promise<void> {
    ctx.selection.registerAreaSelectionAggreagtor(
      new CpuSliceSelectionAggregator(),
    );
    ctx.selection.registerAreaSelectionAggreagtor(
      new CpuSliceByProcessSelectionAggregator(),
    );

    const cpus = ctx.traceInfo.cpus;

    for (const cpu of cpus) {
      const size = 1;
      const uri = uriForSchedTrack(cpu);

      const name = size === undefined ? `Cpu ${cpu}` : `Cpu ${cpu} (${size})`;
      ctx.tracks.registerTrack({
        uri,
        title: name,
        tags: {
          kind: CPU_SLICE_TRACK_KIND,
          cpu,
        },
        track: new CpuSliceTrack(ctx, uri, cpu),
        detailsPanel: () => new SchedSliceDetailsPanel(ctx),
      });
      const trackNode = new TrackNode({uri, title: name, sortOrder: -50});
      ctx.workspace.addChildInOrder(trackNode);
    }

    ctx.selection.registerSqlSelectionResolver({
      sqlTableName: 'sched_slice',
      callback: async (id: number) => {
        const result = await ctx.engine.query(`
          select
            cpu
          from sched_slice
          where id = ${id}
        `);

        const cpu = result.firstRow({
          cpu: NUM,
        }).cpu;

        return {
          eventId: id,
          trackUri: uriForSchedTrack(cpu),
        };
      },
    });
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'dejaview.CpuSlices',
  plugin: CpuSlices,
};
