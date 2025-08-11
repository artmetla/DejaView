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
import {globals} from './globals';
import {Anchor} from '../widgets/anchor';
import {PageAttrs} from '../core/router';
import {isInVSCode} from './vscode';

export class Actions implements m.ClassComponent {
  view() {
    if (isInVSCode())
      return;

    return m(
      '.home-page-actions',
      m(
        'ul',
        m(
          'li',
          m(
            Anchor,
            {
              onclick: () => {
                globals.commandManager.runCommand(
                  'dejaview.CoreCommands#openTrace',
                );
              },
            },
            m('i.material-icons .home-page-actions-icon', 'folder_open'),
            'Open trace file', // ${formatHotkey(cmd.defaultHotkey)}
          ),
        ),
        m(
          'li',
          m(
            Anchor,
            {
              onclick: () => {
                globals.commandManager.runCommand(
                  'dejaview.CoreCommands#OpenExampleTrace',
                );
              },
            },
            m('i.material-icons .home-page-actions-icon', 'description'),
            'Open demo trace',
          ),
        ),
      ),
    );
  }
}

export class HomePage implements m.ClassComponent<PageAttrs> {
  view() {
    return m(
      '.page.home-page',
      m(
        '.home-page-center',
        m(
          '.home-page-title',
          m(`img.logo[src=${globals.root}assets/logo-3d.png]`),
          'DejaView',
        ),
        m(Actions),
      ),
    );
  }
}
