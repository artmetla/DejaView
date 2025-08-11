// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';
import {time} from '../base/time';
import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {
  getMaxMajorTicks,
  MIN_PX_PER_STEP,
  generateTicks,
  TickType,
} from './gridline_helper';
import {Size2D} from '../base/geom';
import {Panel} from './panel_container';
import {TimeScale} from '../base/time_scale';
import {canvasClip} from '../base/canvas_utils';

export class TimeAxisPanel implements Panel {
  readonly kind = 'panel';
  readonly selectable = false;
  readonly id = 'time-axis-panel';

  render(): m.Children {
    return m('.time-axis-panel');
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: Size2D) {
    ctx.fillStyle = '#999';
    ctx.textAlign = 'left';
    ctx.font = '11px Roboto Condensed';

    const trackSize = {...size, width: size.width - TRACK_SHELL_WIDTH};
    ctx.save();
    ctx.translate(TRACK_SHELL_WIDTH, 0);
    canvasClip(ctx, 0, 0, trackSize.width, trackSize.height);
    this.renderPanel(ctx, trackSize);
    ctx.restore();

    ctx.fillRect(TRACK_SHELL_WIDTH - 2, 0, 2, size.height);
  }

  private renderPanel(ctx: CanvasRenderingContext2D, size: Size2D): void {
    const visibleWindow = globals.timeline.visibleWindow;
    const timescale = new TimeScale(visibleWindow, {
      left: 0,
      right: size.width,
    });
    const timespan = visibleWindow.toTimeSpan();
    const offset = globals.trace.timeline.timestampOffset();

    // Draw time axis.
    if (size.width > 0 && timespan.duration > 0n) {
      const maxMajorTicks = getMaxMajorTicks(size.width);
      const tickGen = generateTicks(timespan, maxMajorTicks, offset);
      for (const {type, time} of tickGen) {
        if (type === TickType.MAJOR) {
          const position = Math.floor(timescale.timeToPx(time));
          ctx.fillRect(position, 0, 1, size.height);
          const domainTime = globals.trace.timeline.toDomainTime(time);
          renderTimestamp(ctx, domainTime, position + 5, 10, MIN_PX_PER_STEP);
        }
      }
    }
  }
}

function renderTimestamp(
  ctx: CanvasRenderingContext2D,
  time: time,
  x: number,
  y: number,
  minWidth: number,
) {
  const timeStr = time.toString();
  ctx.font = '11px Roboto Condensed';
  ctx.fillText(timeStr, x, y, minWidth);
  return ctx.measureText(timeStr).width;
}
