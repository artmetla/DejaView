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

import {addSqlTableTab} from '../../frontend/sql_table_tab_interface';
import {sqlTableRegistry} from '../../frontend/widgets/sql/table/sql_table_registry';
import {Trace} from '../../public/trace';
import {DejaViewPlugin, PluginDescriptor} from '../../public/plugin';
import {getThreadTable} from './table';

class ThreadPlugin implements DejaViewPlugin {
  async onTraceLoad(ctx: Trace) {
    sqlTableRegistry['thread'] = getThreadTable();
    ctx.commands.registerCommand({
      id: 'dejaview.ShowTable.thread',
      name: 'Open table: thread',
      callback: () => {
        addSqlTableTab(ctx, {
          table: getThreadTable(),
        });
      },
    });
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'dejaview.Thread',
  plugin: ThreadPlugin,
};
