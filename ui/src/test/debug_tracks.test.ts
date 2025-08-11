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

const SQL_QUERY = `select id, ts, dur, name, category, track_id from slices
where category is not null  limit 1000`;

test.beforeAll(async ({browser}, _testInfo) => {
  page = await browser.newPage();
  pth = new DejaViewTestHelper(page);
  await pth.openTraceFile('api34_startup_cold.dejaview-trace');
});

test('debug tracks', async () => {
  const omnibox = page.locator('input[ref=omnibox]');
  await omnibox.focus();
  await omnibox.selectText();
  await omnibox.press(':');
  await pth.waitForDejaViewIdle();
  await omnibox.fill(SQL_QUERY);
  await pth.waitForDejaViewIdle();
  await omnibox.press('Enter');
  await pth.waitForDejaViewIdle();

  await page.getByRole('button', {name: 'Show debug track'}).click();
  await pth.waitForDejaViewIdle();
  await page.keyboard.type('debug track'); // The track name
  await page.keyboard.press('Enter');
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('debug track added.png');

  // Click on a slice on the debug track.
  await page.mouse.click(1454, 290);
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('debug slice clicked.png');

  // Close the debug track.
  await pth.locateTrack('debug track').getByText('close').first().click();
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('debug track removed.png');
});

test('debug tracks pivot', async () => {
  const omnibox = page.locator('input[ref=omnibox]');
  await omnibox.focus();
  await omnibox.selectText();
  await omnibox.press(':');
  await pth.waitForDejaViewIdle();
  await omnibox.fill(SQL_QUERY);
  await pth.waitForDejaViewIdle();
  await omnibox.press('Enter');

  await page.getByRole('button', {name: 'Show debug track'}).click();
  await pth.waitForDejaViewIdle();
  await page.keyboard.type('pivot'); // The track name
  await page.locator('.pf-popup-portal #pivot').selectOption('category');
  await page.keyboard.press('Enter');
  await pth.waitForDejaViewIdle();
  await pth.waitForIdleAndScreenshot('debug track pivot.png', {
    clip: {
      x: (await pth.sidebarSize()).width,
      y: 180,
      width: 1920,
      height: 600,
    },
  });
});
