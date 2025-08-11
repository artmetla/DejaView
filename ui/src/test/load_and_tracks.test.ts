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

import {test, Page} from '@playwright/test';
import {DejaViewTestHelper} from './dejaview_ui_test_helper';

test.describe.configure({mode: 'serial'});

let pth: DejaViewTestHelper;
let page: Page;

test.beforeAll(async ({browser}, _testInfo) => {
  page = await browser.newPage();
  pth = new DejaViewTestHelper(page);
  await pth.openTraceFile('api34_startup_cold.dejaview-trace');
});

test('load trace', async () => {
  await pth.waitForIdleAndScreenshot('loaded.png');
});

test('info and stats', async () => {
  await page.locator('.sidebar #info_and_stats').click();
  await pth.waitForIdleAndScreenshot('into_and_stats.png');
  await page.locator('.sidebar #show_timeline').click();
  await pth.waitForIdleAndScreenshot('back_to_timeline.png');
});

test('omnibox search', async () => {
  await pth.searchSlice('composite 572441');
  await pth.resetFocus();
  await page.keyboard.press('f');
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('search_slice.png');

  // Click on show process details in the details panel.
  await page.getByText('/system/bin/surfaceflinger [598]').click();
  await page.getByText('Show process details').click();
  await pth.waitForIdleAndScreenshot('process_details.png');
});

test('mark', async () => {
  await page.keyboard.press('/');
  await pth.waitForDejaViewIdle();

  await page.keyboard.type('doFrame');
  await pth.waitForDejaViewIdle();

  for (let i = 0; i < 4; i++) {
    await page.keyboard.press('Enter');
    await pth.waitForDejaViewIdle();

    if (i == 2) {
      await page.keyboard.press('Shift+M');
    } else {
      await page.keyboard.press('m');
    }
    await pth.waitForIdleAndScreenshot(`mark_${i}.png`);
  }
});

test('track expand and collapse', async () => {
  const trackGroup = pth.locateTrackGroup('traced_probes 1054');
  await trackGroup.scrollIntoViewIfNeeded();
  await trackGroup.click();
  await pth.waitForIdleAndScreenshot('traced_probes_expanded.png');

  // Click 5 times in rapid succession.
  for (let i = 0; i < 5; i++) {
    await trackGroup.click();
    await pth.waitForDejaViewIdle(50);
  }
  await pth.waitForIdleAndScreenshot('traced_probes_compressed.png');
});

test('pin tracks', async () => {
  const trackGroup = pth.locateTrackGroup('traced 1055');
  await pth.toggleTrackGroup(trackGroup);
  let track = pth.locateTrack('traced 1055/mem.rss', trackGroup);
  await pth.pinTrackUsingShellBtn(track);
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('one_track_pinned.png');

  track = pth.locateTrack('traced 1055/traced 1055', trackGroup);
  await pth.pinTrackUsingShellBtn(track);
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('two_tracks_pinned.png');
});
