// SPDX-License-Identifier: GPL-3.0-only
// Playwright CLI run-code. Attach only to the embedded companion sample.
async page => {
  const url = new URL(page.url());
  if (url.origin !== 'http://tauri.localhost' || url.pathname !== '/prototype.html' || url.searchParams.get('scenario') !== 'programs') {
    throw Error('Expected the standalone companion sample');
  }
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  const group = page.locator('.programs-group'), rows = group.locator('.program-row');
  const advanced = page.locator('.advanced-trigger');
  const settle = () => page.waitForFunction(() => !document.documentElement.dataset.appearanceTransition
    && !document.querySelector('.menu-popup') && document.getAnimations().every(a => a.playState !== 'running'));
  const choose = async label => {
    await page.locator('.prototype-bar fieldset button').click();
    await page.getByRole('menuitemradio', { name: label, exact: true }).click();
    await settle();
  };
  const language = async (label, tag) => {
    if (await page.locator('html').getAttribute('lang') !== tag) {
      await page.locator('.appearance-controls button').first().click();
      await page.getByRole('menuitemradio', { name: label, exact: true }).click();
      await page.waitForFunction(expected => document.documentElement.lang === expected, tag);
      await settle();
    }
  };
  const geometry = async () => {
    const result = await group.evaluate(el => ({
      width: el.clientWidth, overflows: el.scrollWidth > el.clientWidth,
      overlap: [...el.querySelectorAll('.program-row:has(.program-tags)')].some(row => {
        const name = row.querySelector('.program-name').getBoundingClientRect();
        const status = row.querySelector('.program-tags').getBoundingClientRect();
        return name.right > status.left + 1 && name.bottom > status.top + 1;
      }),
    }));
    if (result.overflows || result.overlap) throw Error('Row layout overlaps or overflows: ' + JSON.stringify(result));
    return result;
  };
  await rows.first().waitFor();
  await settle();
  const denied = await page.evaluate(async () => {
    const codes = [];
    for (const command of ['read_setup_state', 'list_setup_programs']) {
      try { await window.__TAURI_INTERNALS__.invoke(command); codes.push(0); }
      catch (error) { codes.push(error.nativeError); }
    }
    return codes;
  });
  if (denied.some(code => code !== 5)) throw Error('Sample accessed a native configuration command');
  if (await advanced.getAttribute('aria-expanded') !== 'true' || await rows.count() !== 7) throw Error('Sample did not open on all examples');
  if (await rows.first().locator('.program-name').innerText() !== 'Audio Monitor') throw Error('Unexpected sample order');
  if (!await group.evaluate(el => {
    const r = el.getBoundingClientRect(), v = el.closest('.settings-scroll > .scroll-viewport').getBoundingClientRect();
    return r.top >= v.top - 1 && r.bottom <= v.bottom + 1;
  })) throw Error('The sample section is outside the opening viewport');

  await language('简体中文', 'zh-CN');
  const archive = rows.filter({ hasText: 'Meeting Notes and Transcription Archive' });
  await archive.focus();
  await page.keyboard.press('Enter');
  const dialog = page.getByRole('dialog', { name: '程序详情' });
  await dialog.getByText('--folder "会议记录" --mode archive', { exact: true }).waitFor();
  if (!await dialog.getByText('已停用', { exact: true }).isVisible() || !await dialog.getByText('已适配', { exact: true }).isVisible()) throw Error('Status missing from details');
  if (!await dialog.locator('dd').first().evaluate(el => getComputedStyle(el).userSelect === 'text' && el.innerText.endsWith('archive.exe'))) throw Error('Full path is not selectable');
  await page.keyboard.press('Escape');
  await dialog.waitFor({ state: 'detached' });
  await page.waitForFunction(() => document.activeElement?.classList.contains('program-row'));
  if (!await archive.evaluate(el => el === document.activeElement)) throw Error('Focus did not return to the same program');
  await page.keyboard.press('Tab');
  await page.mouse.move(2, 2);

  const layouts = [];
  for (const [label, tag] of [['English','en-US'], ['日本語','ja-JP'], ['简体中文','zh-CN']]) {
    await language(label, tag);
    await group.scrollIntoViewIfNeeded();
    layouts.push(await geometry());
  }
  for (const dark of [false, true]) {
    if (await page.locator('html').evaluate(el => el.classList.contains('dark')) !== dark) {
      await page.locator('.appearance-controls button').nth(1).click();
      await page.waitForFunction(want => document.documentElement.classList.contains('dark') === want, dark);
      await settle();
    }
    await group.scrollIntoViewIfNeeded();
    await group.locator('.program-scroll > .scroll-viewport').evaluate(el => { el.scrollTop = 0; });
    await page.mouse.move(2, 2);
    await group.screenshot({ path: 'output/playwright/companion-sample-' + (dark ? 'dark' : 'light') + '.png' });
  }
  await page.setViewportSize({ width: 338, height: 470 });
  await group.scrollIntoViewIfNeeded();
  layouts.push(await geometry());
  if (!await archive.locator('.program-name').evaluate(el => el.scrollWidth > el.clientWidth)) throw Error('Long-name example did not exercise truncation');
  await archive.click();
  await dialog.waitFor();
  if (await dialog.evaluate(el => el.scrollWidth > el.clientWidth || el.getBoundingClientRect().right > innerWidth)) throw Error('Narrow details overflowed');
  await page.keyboard.press('Escape');
  await dialog.waitFor({ state: 'detached' });

  await choose('暂无附加程序');
  await group.getByText('暂无附加程序', { exact: true }).waitFor();
  await group.getByRole('button', { name: '添加程序…' }).click();
  await page.getByRole('dialog').getByRole('button', { name: '选择' }).click();
  await rows.filter({ hasText: 'Notes.lnk' }).waitFor();
  await page.locator('.save-button').click();
  await page.locator('.save-button').waitFor({ state: 'detached' });
  for (const [label, message] of [['部分程序不可读','部分程序未能列出'], ['读取失败后重试','未能刷新程序列表']]) {
    await choose(label);
    await group.getByText(message, { exact: true }).waitFor();
    if (label === '读取失败后重试') {
      await group.getByRole('button', { name: '查看详情' }).click();
      await page.getByRole('dialog').getByText(/错误码\s*5$/).waitFor();
      await page.keyboard.press('Escape');
      await page.getByRole('dialog').waitFor({ state: 'detached' });
    }
    await group.getByRole('button', { name: '刷新附加程序列表' }).click();
    await rows.filter({ hasText: 'Audio Monitor' }).waitFor();
    if (await rows.count() !== 7) throw Error('Refresh failed to restore the examples');
  }
  if (errors.length) throw Error(errors.join('\n'));
  return { programs:7, scenarios:4, languages:3, themes:2, keyboardAndFocus:true, layouts, errors };
}
