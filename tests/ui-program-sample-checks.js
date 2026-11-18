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
    const result = await group.evaluate(el => {
      const invalidBadge = [...el.querySelectorAll('.program-row:has(.program-badge)')].some(row => {
        const label = row.querySelector('.program-name'), name = label.getBoundingClientRect();
        const badge = row.querySelector('.program-badge').getBoundingClientRect();
        const end = row.querySelector('.program-state, .program-chevron').getBoundingClientRect();
        const gap = badge.left - name.right;
        return gap < 3 || gap > 7 || Math.abs(badge.top - name.top) > 2 || name.width < 16 || badge.right > end.left
          || (label.scrollWidth > label.clientWidth && end.left - badge.right > 10);
      });
      return { width: el.clientWidth, overflows: el.scrollWidth > el.clientWidth, invalidBadge };
    });
    if (result.overflows || result.invalidBadge) throw Error('Badge or row layout incorrect: ' + JSON.stringify(result));
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
  const logo = rows.first().locator('.program-icon img');
  await logo.evaluate(img => img.decode());
  if (!await rows.filter({ hasText: 'Quick Notes.lnk' }).locator('.program-icon svg').count()) throw Error('Missing file-type fallback');
  const viewport = group.locator('.program-scroll > .scroll-viewport'), rail = group.locator('.program-scroll > .scroll-track');
  const thumbOpacity = async visible => page.waitForFunction(want => {
    const rail = document.querySelector('.program-scroll > .scroll-track');
    return !!rail && Number(getComputedStyle(rail).opacity) === (want ? 1 : 0);
  }, visible);
  await thumbOpacity(false);
  const rightBefore = (await rows.first().locator('.program-chevron').boundingBox()).x;
  await viewport.hover();
  await page.mouse.wheel(0, 95);
  await thumbOpacity(true);
  await thumbOpacity(false); // Remain inside the list even after the pointer comes to rest.
  const target = await rail.boundingBox();
  const thumb = await rail.locator('.scroll-thumb').boundingBox();
  const grabY = thumb.y + thumb.height / 2;
  await page.mouse.move(target.x + target.width / 2, grabY);
  await thumbOpacity(true);
  await thumbOpacity(false); // A parked pointer over the rail also fades out.
  await page.mouse.down();
  const scrollBefore = await viewport.evaluate(el => el.scrollTop);
  await page.mouse.move(target.x + target.width / 2, grabY - 25, { steps: 4 });
  await thumbOpacity(true);
  await page.mouse.up();
  if (await viewport.evaluate(el => el.scrollTop) >= scrollBefore) throw Error('Hidden scrollbar did not support dragging');
  await page.mouse.move(2, 2);
  await viewport.evaluate(el => { el.scrollTop = 0; });
  await thumbOpacity(false);
  await viewport.focus();
  await page.keyboard.press('End');
  await thumbOpacity(true);
  await thumbOpacity(false);
  await page.keyboard.press('Home');
  await thumbOpacity(false);
  if (Math.abs((await rows.first().locator('.program-chevron').boundingBox()).x - rightBefore) > 1) throw Error('Idle scrollbar shifted the row');
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
    await thumbOpacity(false);
    await group.screenshot({ path: 'output/playwright/companion-sample-' + (dark ? 'dark' : 'light') + '.png' });
  }
  await page.setViewportSize({ width: 338, height: 470 });
  for (const [label, tag] of [['English','en-US'], ['日本語','ja-JP'], ['简体中文','zh-CN']]) {
    await language(label, tag);
    await group.scrollIntoViewIfNeeded();
    layouts.push(await geometry());
  }
  if (!await archive.locator('.program-name').evaluate(el => el.scrollWidth > el.clientWidth)) throw Error('Long-name example did not exercise truncation');
  await viewport.evaluate(el => { el.scrollTop = 0; });
  await page.mouse.move(2, 2);
  await thumbOpacity(false);
  await group.screenshot({ path: 'output/playwright/companion-sample-narrow.png' });
  await page.emulateMedia({ reducedMotion: 'reduce' });
  if (await rail.evaluate(el => getComputedStyle(el).transitionDuration !== '0s')) throw Error('Scrollbar ignored reduced motion');
  await page.emulateMedia({ reducedMotion: null });
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
  return { programs:7, scenarios:4, languages:3, themes:2, keyboardAndFocus:true, idleScrollbar:true, icons:true, layouts, errors };
}
