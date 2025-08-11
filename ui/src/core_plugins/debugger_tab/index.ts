// Copyright (C) 2025 The Android Open Source Project
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

import m from 'mithril';
import {globals} from '../../frontend/globals';
import {Trace} from '../../public/trace';
import {DejaViewPlugin, PluginDescriptor} from '../../public/plugin';
import {Terminal, ITerminalOptions} from '@xterm/xterm';
import {FitAddon} from '@xterm/addon-fit';

export interface DebuggerPanelAttrs {
  options?: ITerminalOptions;
}

export class DebuggerPanel implements m.ClassComponent<DebuggerPanelAttrs> {
  private term?: Terminal;
  private fitAddon?: FitAddon;
  private resizeObserver?: ResizeObserver;

  resize() {
    const prevRows = this.term?.rows;
    const prevCols = this.term?.cols;
    this.fitAddon?.fit();
    if (this.term && (this.term.rows != prevRows || this.term.cols != prevCols)) {
      globals.trace.engine.resizeDebugger(this.term.rows, this.term.cols);
    }
  }

  oncreate(vnode: m.VnodeDOM<DebuggerPanelAttrs, this>) {
    this.term = new Terminal({
      cursorBlink: true,
      fontFamily: 'monospace',
      fontSize: 14,
      ...vnode.attrs.options,
    });

    this.fitAddon = new FitAddon();
    this.term.loadAddon(this.fitAddon);

    this.term.open(vnode.dom as HTMLElement);

    this.resize();

    globals.trace.engine.setOnDebuggerStdout((data: Uint8Array) => {
      this.term?.write(data);
    });
    const encoder = new TextEncoder();
    this.term.onData((data: string) => {
      globals.trace.engine.sendToDebugger(encoder.encode(data));
    });

    this.resizeObserver = new ResizeObserver(() => {
      this.resize();
    });
    this.resizeObserver.observe(vnode.dom);
  }

  onremove() {
    this.resizeObserver?.disconnect();
    this.term?.dispose();
  }

  view() {
    return m('div', {
      class: 'terminal-container',
      style: { height: '100%', width: '100%', backgroundColor: "black" }
    });
  }
}

class Debugger implements DejaViewPlugin {
  async onTraceLoad(ctx: Trace): Promise<void> {
    const debuggerTabUri = 'dejaview.Debugger#tab';

    globals.trace.engine.setOnDebuggerStarted(() => {
      ctx.tabs.registerTab({
        isEphemeral: true,
        uri: debuggerTabUri,
        content: {
          render: () => m(DebuggerPanel),
          getTitle: () => 'Debugger',
        },
      });
      ctx.tabs.showTab(debuggerTabUri);
    });

    globals.trace.engine.setOnDebuggerStopped(() => {
      ctx.tabs.hideTab(debuggerTabUri);
    });
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'dejaview.Debugger',
  plugin: Debugger,
};
