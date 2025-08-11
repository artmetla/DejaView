// Copyright (C) 2025 The Android Open Source Project
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

export function isInVSCode(): boolean {
  return window.location.protocol === 'vscode-webview:';
}

type VSCode = {
  postMessage(message: any): void;
  getState(): any;
  setState(state: any): void;
};

declare function acquireVsCodeApi(): any;

function getVsCodeApi(): VSCode {
  if (typeof acquireVsCodeApi === 'function') {
    return acquireVsCodeApi();
  }

  return {
    postMessage: (_: any) => {},
    getState: () => { return undefined; },
    setState: (_: any) => {},
  };
}

const vscode: VSCode = getVsCodeApi();

export function openSymbol(symbol: string) {
  vscode.postMessage({ command: "open", symbol: symbol });
}