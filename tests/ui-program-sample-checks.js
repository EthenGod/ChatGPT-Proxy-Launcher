// SPDX-License-Identifier: GPL-3.0-only
// Playwright CLI run-code. Attach only to the embedded companion sample.
async page => {
  const url = new URL(page.url());
  if (url.origin !== 'http://tauri.localhost' || url.pathname !== '/prototype.html' || url.searchParams.get('scenario') !== 'programs') {
    throw Error('Expected the standalone companion sample');
  }
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.addInitScript(() => {
    window.programArrivals = [];
    window.settingsArrivals = 0;
    window.layoutChanges = [];
    document.addEventListener('animationstart', event => {
      if (event.animationName === 'settings-arrive') window.settingsArrivals++;
      if (event.animationName === 'program-arrive' && event.target.matches('.program-row')) {
        window.programArrivals.push(event.target.querySelector('.program-name').textContent);
      }
    });
    const animate = Element.prototype.animate;
    Element.prototype.animate = function (keyframes, options) {
      if (options?.id === 'program-layout') window.layoutChanges.push(this.className || this.tagName);
      return animate.call(this, keyframes, options);
    };
  });
  await page.reload();
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
  if (!await page.evaluate(() => window.programArrivals.length === 7 && window.settingsArrivals > 0)) throw Error('Missing first-load motion');
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
    const content = document.querySelector('.program-scroll .scroll-content');
    return !!rail && Number(getComputedStyle(rail).opacity) === (want ? 1 : 0)
      && parseFloat(getComputedStyle(content).marginInlineEnd) === (want ? 12 : 0);
  }, visible);
  await thumbOpacity(false);
  const peek = await viewport.evaluate(el => {
    const sixth = el.querySelectorAll('li')[5].getBoundingClientRect();
    return el.getBoundingClientRect().bottom - sixth.top;
  });
  if (peek < 16 || peek > 30) throw Error('Sixth program must be partially visible: ' + peek);
  const rightBefore = (await rows.first().locator('.program-chevron').boundingBox()).x;
  await viewport.hover();
  const recording = await rail.evaluateHandle(track => {
    const thumb = track.querySelector('.scroll-thumb');
    const row = track.parentElement.querySelector('.program-row:has(.program-state)');
    const state = row.querySelector('.program-state'), arrow = row.querySelector('.program-chevron');
    const frames = [];
    let frame;
    function sample() {
      const hit = thumb.getBoundingClientRect().left;
      const offset = parseFloat(getComputedStyle(thumb, '::before').translate) || 0;
      frames.push({ hit, ink: hit + offset, state: state.getBoundingClientRect().right, arrow: arrow.getBoundingClientRect().right });
      frame = requestAnimationFrame(sample);
    }
    sample();
    return { finish() { cancelAnimationFrame(frame); return frames; } };
  });
  let frames, activeRight;
  try {
    await page.mouse.wheel(0, 95);
    await thumbOpacity(true);
    activeRight = (await rows.first().locator('.program-chevron').boundingBox()).x;
    await thumbOpacity(false); // A parked pointer inside the list also lets the thumb slide out.
  } finally {
    frames = await recording.evaluate(capture => capture.finish());
    await recording.dispose();
  }
  if (Math.abs(rightBefore - activeRight - 12) > 1) throw Error('Active rail did not receive space from the row');
  const idle = frames[0], travel = frame => idle.ink - frame.ink;
  const openFrame = frames.findIndex(frame => travel(frame) >= 11.9);
  const moving = frame => travel(frame) > .5 && travel(frame) < 11.5;
  const drift = Math.max(...frames.map(frame => Math.max(Math.abs(frame.hit - idle.hit),
    Math.abs(idle.state - frame.state - travel(frame)), Math.abs(idle.arrow - frame.arrow - travel(frame)))));
  if (openFrame < 1 || !frames.slice(0, openFrame).some(moving) || !frames.slice(openFrame + 1).some(moving) ||
      Math.abs(travel(frames.at(-1))) > .2 || drift > .15) throw Error('Scrollbar and row ends moved out of sync: ' + JSON.stringify({ openFrame, drift, frames }));
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
  if (!await viewport.evaluate(el => !el.parentElement.hasAttribute('data-overflow-y-end') &&
    Number(getComputedStyle(el.parentElement, '::after').opacity) === 0)) throw Error('End cue remained at the bottom');
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
    if (await page.getByText('窗口外观更新失败。', { exact: true }).count()) throw Error('Native window colors were rejected');
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
  if (await rail.evaluate(el => [el, el.parentElement].some(node => getComputedStyle(node).transitionDuration !== '0s'))) throw Error('Scrollbar ignored reduced motion');
  await page.emulateMedia({ reducedMotion: null });
  await archive.click();
  await dialog.waitFor();
  if (await dialog.evaluate(el => el.scrollWidth > el.clientWidth || el.getBoundingClientRect().right > innerWidth)) throw Error('Narrow details overflowed');
  await page.keyboard.press('Escape');
  await dialog.waitFor({ state: 'detached' });

  const addDefault = async () => {
    await group.getByRole('button', { name: '添加程序…' }).click();
    const picker = page.getByRole('dialog', { name: '选择要随 ChatGPT 启动的程序', exact: true });
    await picker.getByRole('button', { name: '选择' }).click();
    await picker.waitFor({ state: 'detached' });
    await settle();
  };
  await page.evaluate(() => { window.programArrivals = []; window.layoutChanges = []; });
  await viewport.evaluate(el => { el.scrollTop = el.scrollHeight; });
  await addDefault();
  await page.waitForFunction(() => window.programArrivals.includes('Notes.lnk'));
  if (!await page.evaluate(() => window.programArrivals.join() === 'Notes.lnk' && window.layoutChanges.includes('LI'))) throw Error('Adding a program did not move only the affected rows');
  const inserted = page.locator('.program-row').filter({ has: page.getByText('Notes.lnk', { exact: true }) });
  if (!await inserted.evaluate(el => {
    const r = el.getBoundingClientRect(), v = el.closest('.scroll-viewport').getBoundingClientRect();
    return r.top >= v.top - 1 && r.bottom <= v.bottom + 1;
  })) throw Error('Newly added program was hidden');
  await addDefault();
  await group.getByRole('button', { name: '刷新附加程序列表' }).click();
  await page.waitForFunction(() => document.querySelector('.program-results').getAttribute('aria-busy') === 'false');
  if (await rows.count() !== 8 || !await page.evaluate(() => window.programArrivals.length === 1)) throw Error('A duplicate or refresh replayed row motion');
  await choose('暂无附加程序');
  await group.getByText('暂无附加程序', { exact: true }).waitFor();
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await page.evaluate(() => { window.programArrivals = []; window.layoutChanges = []; });
  await addDefault();
  await rows.filter({ hasText: 'Notes.lnk' }).waitFor();
  if (!await page.evaluate(() => !window.programArrivals.length && !window.layoutChanges.length)) throw Error('Added row ignored reduced motion');
  await page.emulateMedia({ reducedMotion: null });
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
  return { programs:7, scenarios:4, languages:3, themes:2, keyboardAndFocus:true, gutterRecovered:rightBefore - activeRight, scrollMotionFrames:frames.length, scrollMotionDrift:drift, peek, entryAndReorder:true, icons:true, layouts, errors };
}
