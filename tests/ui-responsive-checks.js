// SPDX-License-Identifier: GPL-3.0-only
// Run through Playwright CLI in a fresh, isolated functional WebView2 window.
async page => {
  if (page.url() !== 'http://tauri.localhost/') throw Error('Expected embedded settings');
  const advanced = page.locator('.advanced-trigger');
  if (await advanced.getAttribute('aria-expanded') !== 'true') await advanced.click();
  const directory = page.locator('.advanced-card input').first();
  const original = await directory.inputValue();
  if (!/\/work\/ui-functional-tests\/[a-f0-9]{32}\/state$/i.test(original.replaceAll('\\', '/'))) throw Error('Expected isolated functional paths');
  const proxy = page.locator('.section-heading .switch');
  if (await proxy.getAttribute('aria-checked') !== 'true') await proxy.click();
  const dismiss = page.locator('.notice-close');
  if (await dismiss.isVisible()) await dismiss.click();

  const menu = page.locator('.appearance-controls .menu-trigger'), theme = page.locator('.appearance-controls button').nth(1);
  const reset = page.locator('.field-reset'), caption = reset.locator('[data-motion-text]');
  const cdp = await page.context().newCDPSession(page);
  const size = (width, height) => cdp.send('Emulation.setDeviceMetricsOverride', { width, height, deviceScaleFactor: 1, mobile: false });
  const settled = () => page.waitForFunction(() => !document.documentElement.hasAttribute('data-window-resizing')
    && !document.documentElement.dataset.appearanceTransition && document.getAnimations().every(a => a.playState !== 'running'));
  const waitForLanguage = tag => page.waitForFunction(lang => document.documentElement.lang === lang && !document.documentElement.dataset.appearanceTransition, tag);
  const choose = async (tag, name) => {
    if (await page.locator('html').getAttribute('lang') === tag) return;
    await menu.click(); await page.getByRole('menuitemradio', { name, exact: true }).click();
    await waitForLanguage(tag);
  };
  const geometry = () => page.evaluate(() => {
    const html = document.documentElement, body = document.body;
    const native = [...document.querySelectorAll('*')].filter(el => {
      if (!el.getClientRects().length) return false;
      const css = getComputedStyle(el);
      const scrolls = (['auto', 'scroll'].includes(css.overflowY) && el.scrollHeight > el.clientHeight + 1)
        || (['auto', 'scroll'].includes(css.overflowX) && el.scrollWidth > el.clientWidth + 1);
      return scrolls && css.scrollbarWidth !== 'none' && getComputedStyle(el, '::-webkit-scrollbar').display !== 'none';
    });
    if (native.length || html.clientWidth !== innerWidth || html.scrollWidth !== innerWidth || html.scrollHeight !== innerHeight
      || body.clientWidth !== innerWidth || body.clientHeight !== innerHeight) {
      throw Error('Unexpected page scrollbar or gutter at ' + innerWidth + 'x' + innerHeight + ': ' + native.map(el => el.className));
    }
    for (const el of document.querySelectorAll('select, input[type=number], input[type=search], input[type=radio], input[type=checkbox]')) {
      const r = el.getBoundingClientRect(), css = getComputedStyle(el);
      if (r.width > 3 && r.height > 3 && css.clipPath === 'none' && css.visibility === 'visible' && css.opacity !== '0' && css.appearance !== 'none') {
        throw Error('An unstyled native control is visible: ' + el.tagName + ' ' + el.type);
      }
    }
    for (const button of document.querySelectorAll('.footer-actions button, .dialog-actions button')) {
      const r = button.getBoundingClientRect();
      if (r.left < -1 || r.top < -1 || r.right > innerWidth + 1 || r.bottom > innerHeight + 1) throw Error('Clipped action: ' + button.textContent);
    }
  });

  // Measure the actual painted text. DOM styles cannot see extracted transition layers.
  let images = [];
  cdp.on('Page.screencastFrame', event => {
    images.push(event.data);
    void cdp.send('Page.screencastFrameAck', { sessionId: event.sessionId });
  });
  const screenshot = async () => (await cdp.send('Page.captureScreenshot', { format: 'png' })).data;
  const captureLabel = async (tag, name) => {
    await page.locator('.settings-scroll > .scroll-viewport').evaluate(el => { el.scrollTop = 0; });
    await page.mouse.move(1, 1); await settled();
    const old = await caption.boundingBox(), first = await screenshot();
    await menu.click();
    images = [];
    await cdp.send('Page.startScreencast', { format: 'png', everyNthFrame: 1 });
    try {
      await page.getByRole('menuitemradio', { name, exact: true }).click();
      await waitForLanguage(tag);
    } finally { await cdp.send('Page.stopScreencast'); }
    const end = await caption.boundingBox(), last = await screenshot();
    const levels = await page.evaluate(async ({ old, end, frames }) => {
      const x = Math.floor(Math.min(old.x, end.x)) - 1, y = Math.floor(Math.min(old.y, end.y)) - 1;
      const right = Math.ceil(Math.max(old.x + old.width, end.x + end.width)) + 1;
      const bottom = Math.ceil(Math.max(old.y + old.height, end.y + end.height)) + 1;
      if (x < 0 || y < 0 || right > innerWidth || bottom > document.querySelector('.settings-footer').getBoundingClientRect().top) {
        throw Error('The reset label must be visible throughout the recording');
      }
      const canvas = document.createElement('canvas'), ctx = canvas.getContext('2d', { willReadFrequently: true });
      const values = [];
      for (const png of frames) {
        const bytes = Uint8Array.from(atob(png), char => char.charCodeAt(0));
        const bitmap = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
        const scale = bitmap.width / innerWidth;
        canvas.width = Math.ceil((right - x) * scale); canvas.height = Math.ceil((bottom - y) * scale);
        ctx.drawImage(bitmap, -Math.floor(x * scale), -Math.floor(y * scale)); bitmap.close();
        const pixels = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
        // Only the blue label contributes to this channel difference on the neutral card.
        let peak = 0;
        for (let i = 0; i < pixels.length; i += 4) peak = Math.max(peak, pixels[i + 2] - pixels[i]);
        values.push(peak);
      }
      return values;
    }, { old, end, frames: [first, ...images, last] });
    const start = levels[0], finish = levels.at(-1), peak = Math.max(...levels), low = Math.min(...levels);
    if (Math.min(start, finish) < 20 || levels.length < 5 || low >= .8 * Math.min(start, finish) || peak > Math.max(start, finish) + 6) {
      throw Error('Unexpected label fade: ' + JSON.stringify({ tag, start, finish, low, peak }));
    }
    return { tag, start, finish, peak };
  };

  // Keep the port focused and measure both axes throughout the layout change.
  const recordReflow = async (width, reduced) => {
    const port = page.locator('#proxy-port');
    await port.focus();
    const value = await port.inputValue();
    await page.evaluate(() => {
      const protocol = document.querySelector('.protocol-field'), endpoints = document.querySelector('.connection-endpoints');
      const sample = () => {
        const p = protocol.getBoundingClientRect(), e = endpoints.getBoundingClientRect();
        return { row: e.top - p.top, indent: e.left - p.left, overlap: Math.min(p.right - e.left, p.bottom - e.top) };
      };
      const samples = [sample()], startWidth = innerWidth, started = performance.now();
      window.reflowSamples = new Promise(resolve => {
        let changedAt;
        const frame = time => {
          if (innerWidth !== startWidth) {
            changedAt ??= time;
            samples.push(sample());
            if (time - changedAt >= 320) { resolve(samples); return; }
          }
          if (time - started > 2000) { resolve(samples); return; }
          requestAnimationFrame(frame);
        };
        requestAnimationFrame(frame);
      });
    });
    await size(width, 650);
    const frames = await page.evaluate(async () => { const frames = await window.reflowSamples; delete window.reflowSamples; return frames; });
    const start = frames[0].row, end = frames.at(-1).row;
    const between = key => frames.slice(1).filter(frame => frame[key] > Math.min(frames[0][key], frames.at(-1)[key]) + 1
      && frame[key] < Math.max(frames[0][key], frames.at(-1)[key]) - 1).length;
    const rowFrames = between('row'), inlineFrames = between('indent'), overlap = Math.max(...frames.map(frame => frame.overlap));
    if ((width <= 540 ? end < 36 : Math.abs(end) > 1) || Math.abs(end - start) < 36 || overlap > 1 ||
      (reduced ? rowFrames || inlineFrames : !rowFrames || !inlineFrames)) {
      throw Error('Responsive motion: ' + JSON.stringify({ width, reduced, start, end, rowFrames, inlineFrames, overlap }));
    }
    if (!await port.evaluate((el, before) => el.value === before && document.activeElement === el, value)) throw Error('Resizing lost the active input');
    const radio = page.getByRole('radio', { name: 'HTTPS', exact: true });
    await radio.click();
    if (await radio.getAttribute('aria-checked') !== 'true') throw Error('Protocol is unreachable after resizing');
    await geometry();
    return { width, reduced, start, end, rowFrames, inlineFrames };
  };
  try {
    await size(780, 1000); await settled(); await choose('zh-CN', '简体中文');
    const labels = [];
    for (const [dark, enabled, tag, name] of [[true, false, 'en-US', 'English'], [false, false, 'ja-JP', '日本語'],
      [false, true, 'zh-CN', '简体中文'], [true, true, 'en-US', 'English']]) {
      if (await theme.evaluate(() => document.documentElement.classList.contains('dark')) !== dark) await theme.click();
      if (enabled && await reset.isDisabled()) await directory.fill(original + '-preview');
      if (await reset.isEnabled() !== enabled) throw Error('Unexpected reset button state');
      labels.push({ dark, enabled, ...await captureLabel(tag, name) });
    }
    await reset.click();
    const motion = [];
    for (const width of [540, 541]) motion.push(await recordReflow(width, false));
    await page.emulateMedia({ reducedMotion: 'reduce' });
    for (const width of [464, 640]) motion.push(await recordReflow(width, true));
    await page.emulateMedia({ reducedMotion: null });
    // Reverse direction again while the previous transition is still in progress.
    for (const width of [530, 550, 529, 555]) { await size(width, 400); await page.evaluate(() => new Promise(requestAnimationFrame)); }
    await settled();
    const fields = page.locator('.connection-fields .field');
    const tops = await fields.evaluateAll(els => els.map(el => el.getBoundingClientRect().top));
    if (Math.max(...tops) - Math.min(...tops) > 1) throw Error('Fields did not return to one row');
    for (const [tag, name] of [['ja-JP', '日本語'], ['en-US', 'English'], ['zh-CN', '简体中文']]) {
      await choose(tag, name);
      for (const [width, height] of [[464, 361], [540, 650], [541, 650], [984, 361], [984, 911]]) {
        await size(width, height); await settled(); await geometry();
        await page.locator('.programs-label .help-button').click(); await settled(); await geometry();
        await page.keyboard.press('Escape'); await page.getByRole('dialog').waitFor({ state: 'detached' });
        await menu.click(); await geometry(); await settled(); await geometry();
        await page.keyboard.press('Escape'); await page.locator('.menu-popup').waitFor({ state: 'detached' });
      }
    }
    return { labels, motion, layouts: 15 };
  } finally {
    await page.emulateMedia({ reducedMotion: null });
    await cdp.send('Emulation.clearDeviceMetricsOverride');
    await cdp.detach();
  }
}
