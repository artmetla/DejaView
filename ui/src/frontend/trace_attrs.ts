// Copyright (C) 2020 The Android Open Source Project
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
import {onClickCopy} from './clipboard';
import {AppImpl} from '../core/app_impl';

export function isDownloadable() {
  const traceSource = AppImpl.instance.trace?.traceInfo.source;
  if (traceSource === undefined) {
    return false;
  }
  if (traceSource.type === 'ARRAY_BUFFER' && traceSource.localOnly) {
    return false;
  }
  if (traceSource.type === 'HTTP_RPC') {
    return false;
  }
  return true;
}

export function createTraceLink(title: string, url: string) {
  if (url === '') {
    return m('a.trace-file-name', title);
  }
  const linkProps = {
    href: url,
    title: 'Click to copy the URL',
    target: '_blank',
    onclick: onClickCopy(url),
  };
  return m('a.trace-file-name', linkProps, title);
}

export function isTraceLoaded(): boolean {
  return AppImpl.instance.trace !== undefined;
}

export function getCurrentTraceUrl(): undefined | string {
  const source = AppImpl.instance.trace?.traceInfo.source;
  if (source && source.type === 'URL') {
    return source.url;
  }
  return undefined;
}
