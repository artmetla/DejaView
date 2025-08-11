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

import m from 'mithril';
import {classNames} from '../base/classnames';
import {raf} from '../core/raf_scheduler';
import {globals} from './globals';
import {taskTracker} from './task_tracker';
import {Popup, PopupPosition} from '../widgets/popup';
import {assertFalse} from '../base/logging';
import {OmniboxMode} from '../core/omnibox_manager';
import {AppImpl} from '../core/app_impl';
import {TraceImpl, TraceImplAttrs} from '../core/trace_impl';
import {toggleHelp} from './help_modal';
import {assertExists} from '../base/logging';
import {TRACE_SUFFIX} from '../common/constants';
import {featureFlags} from '../core/feature_flags';
import {downloadUrl} from './download_utils';
import {isDownloadable, isTraceLoaded} from './trace_attrs';
import {Trace} from '../public/trace';
import {Router} from '../core/router';
import {isInVSCode} from './vscode';

const VIZ_PAGE_IN_NAV_FLAG = featureFlags.register({
  id: 'showVizPageInNav',
  name: 'Show viz page',
  description: 'Show a link to the viz page in the top bar.',
  defaultValue: true,
});

function openHelp(e: Event) {
  e.preventDefault();
  toggleHelp();
}

function closeTrace(e: Event) {
  e.preventDefault();
  AppImpl.instance.closeCurrentTrace();
}

function downloadTraceFromUrl(url: string): Promise<File> {
  return m.request({
    method: 'GET',
    url,
    // TODO(hjd): Once mithril is updated we can use responseType here rather
    // than using config and remove the extract below.
    config: (xhr) => {
      xhr.responseType = 'blob';
      xhr.onprogress = (progress) => {
        const percent = ((100 * progress.loaded) / progress.total).toFixed(1);
        const msg = `Downloading trace ${percent}%`;
        AppImpl.instance.omnibox.showStatusMessage(msg);
      };
    },
    extract: (xhr) => {
      return xhr.response;
    },
  });
}

export async function getCurrentTrace(): Promise<Blob> {
  // Caller must check engine exists.
  const src = assertExists(AppImpl.instance.trace?.traceInfo.source);
  if (src.type === 'ARRAY_BUFFER') {
    return new Blob([src.buffer]);
  } else if (src.type === 'FILE') {
    return src.file;
  } else if (src.type === 'URL') {
    return downloadTraceFromUrl(src.url);
  } else {
    throw new Error(`Loading to catapult from source with type ${src.type}`);
  }
}

function navigateQuery(e: Event) {
  e.preventDefault();
  Router.navigate('#!/query');
}

function navigateViz(e: Event) {
  e.preventDefault();
  Router.navigate('#!/viz');
}

function navigateMetrics(e: Event) {
  e.preventDefault();
  Router.navigate('#!/metrics');
}

function navigateInfo(e: Event) {
  e.preventDefault();
  Router.navigate('#!/info');
}

function navigateViewer(e: Event) {
  e.preventDefault();
  Router.navigate('#!/viewer');
}

function downloadTrace(e: Event, trace: Trace) {
  e.preventDefault();
  if (!isDownloadable() || !isTraceLoaded()) return;

  let url = '';
  let fileName = `trace${TRACE_SUFFIX}`;
  const src = trace.traceInfo.source;
  if (src.type === 'URL') {
    url = src.url;
    fileName = url.split('/').slice(-1)[0];
  } else if (src.type === 'ARRAY_BUFFER') {
    const blob = new Blob([src.buffer], {type: 'application/octet-stream'});
    const inputFileName = window.prompt(
      'Please enter a name for your file or leave blank',
    );
    if (inputFileName) {
      fileName = `${inputFileName}.dejaview_trace.gz`;
    } else if (src.fileName) {
      fileName = src.fileName;
    }
    url = URL.createObjectURL(blob);
  } else if (src.type === 'FILE') {
    const file = src.file;
    url = URL.createObjectURL(file);
    fileName = file.name;
  } else {
    throw new Error(`Download from ${JSON.stringify(src)} is not supported`);
  }
  downloadUrl(fileName, url);
}

export const DISMISSED_PANNING_HINT_KEY = 'dismissedPanningHint';

class Progress implements m.ClassComponent<TraceImplAttrs> {
  view({attrs}: m.CVnode<TraceImplAttrs>): m.Children {
    const engine = attrs.trace.engine;
    const isLoading =
      AppImpl.instance.isLoadingTrace ||
      engine.numRequestsPending > 0 ||
      taskTracker.hasPendingTasks();
    const classes = classNames(isLoading && 'progress-anim');
    return m('.progress', {class: classes});
  }
}

class HelpPanningNotification implements m.ClassComponent {
  view() {
    const dismissed = localStorage.getItem(DISMISSED_PANNING_HINT_KEY);
    // Do not show the help notification in embedded mode because local storage
    // does not persist for iFrames. The host is responsible for communicating
    // to users that they can press '?' for help.
    if (
      globals.embeddedMode ||
      dismissed === 'true' ||
      !globals.showPanningHint
    ) {
      return;
    }
    return m(
      '.helpful-hint',
      m(
        '.hint-text',
        'Are you trying to pan? Use the WASD keys or hold shift to click ' +
          "and drag. Press '?' for more help.",
      ),
      m(
        'button.hint-dismiss-button',
        {
          onclick: () => {
            globals.showPanningHint = false;
            localStorage.setItem(DISMISSED_PANNING_HINT_KEY, 'true');
            raf.scheduleFullRedraw();
          },
        },
        'Dismiss',
      ),
    );
  }
}

class TraceErrorIcon implements m.ClassComponent<TraceImplAttrs> {
  private tracePopupErrorDismissed = false;

  view({attrs}: m.CVnode<TraceImplAttrs>) {
    const trace = attrs.trace;
    if (globals.embeddedMode) return;

    const mode = AppImpl.instance.omnibox.mode;
    const totErrors = trace.traceInfo.importErrors + trace.loadingErrors.length;
    if (totErrors === 0 || mode === OmniboxMode.Command) {
      return;
    }
    const message = Boolean(totErrors)
      ? `${totErrors} import or data loss errors detected.`
      : `Metric error detected.`;
    return m(
      '.error-box',
      m(
        Popup,
        {
          trigger: m('.popup-trigger'),
          isOpen: !this.tracePopupErrorDismissed,
          position: PopupPosition.Left,
          onChange: (shouldOpen: boolean) => {
            assertFalse(shouldOpen);
            this.tracePopupErrorDismissed = true;
          },
        },
        m('.error-popup', 'Data-loss/import error. Click for more info.'),
      ),
      m(
        'a.error',
        {href: '#!/info'},
        m(
          'i.material-icons',
          {
            title: message + ` Click for more info.`,
          },
          'announcement',
        ),
      ),
    );
  }
}

export interface TopbarAttrs {
  omnibox: m.Children;
  trace?: TraceImpl;
}

export class Topbar implements m.ClassComponent<TopbarAttrs> {
  view({attrs}: m.Vnode<TopbarAttrs>) {
    const buttonsBeforeOmnibox = [];
    const buttonsAfterOmnibox = [];
    if (isTraceLoaded()) {
      buttonsBeforeOmnibox.push(
        m(
          'button.topbar-button',
          {
            onclick: (e: Event) => {
              navigateViewer(e);
            },
          },
          m('i.material-icons', {title: 'Show timeline'}, 'line_style'),
        ),
      );
      buttonsBeforeOmnibox.push(
        m(
          'button.topbar-button',
          {
            onclick: (e: Event) => {
              navigateQuery(e);
            },
          },
          m('i.material-icons', {title: 'Query (SQL)'}, 'database'),
        ),
      );
      if (VIZ_PAGE_IN_NAV_FLAG.get()) {
        buttonsBeforeOmnibox.push(
          m(
            'button.topbar-button',
            {
              onclick: (e: Event) => {
                navigateViz(e);
              },
            },
            m('i.material-icons', {title: 'Viz'}, 'area_chart'),
          ),
        );
      }
      buttonsBeforeOmnibox.push(
        m(
          'button.topbar-button',
          {
            onclick: (e: Event) => {
              navigateMetrics(e);
            },
          },
          m('i.material-icons', {title: 'Metrics'}, 'speed'),
        ),
      );
      buttonsBeforeOmnibox.push(
        m(
          'button.topbar-button',
          {
            onclick: (e: Event) => {
              navigateInfo(e);
            },
          },
          m('i.material-icons', {title: 'Info and stats'}, 'info'),
        ),
      );

      if (isDownloadable()) {
        buttonsAfterOmnibox.push(
          m(
            'button.topbar-button',
            {
              onclick: (e: Event) => {
                attrs.trace && downloadTrace(e, attrs.trace);
              },
            },
            m('i.material-icons', {title: 'Download trace'}, 'file_download'),
          ),
        );
      }
      buttonsAfterOmnibox.push(
        m(
          'button.topbar-button',
          {
            onclick: (e: Event) => {
              openHelp(e);
            },
          },
          m('i.material-icons', {title: 'UI help'}, 'help'),
        ),
      );
      if (!isInVSCode()) {
        buttonsAfterOmnibox.push(
          m(
            'button.topbar-button',
            {
              onclick: (e: Event) => {
                closeTrace(e);
              },
            },
            m('i.material-icons', {title: 'Close trace'}, 'close'),
          ),
        );
      }
    }

    const {omnibox} = attrs;
    return m(
      '.topbar',
      ...buttonsBeforeOmnibox,
      omnibox,
      attrs.trace && m(Progress, {trace: attrs.trace}),
      m(HelpPanningNotification),
      attrs.trace && m(TraceErrorIcon, {trace: attrs.trace}),
      ...buttonsAfterOmnibox,
    );
  }
}
