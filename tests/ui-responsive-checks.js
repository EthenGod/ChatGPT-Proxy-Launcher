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
  async function captureLanguageChange(tag, name) {
    await menu.click();
    images = [];
    await cdp.send('Page.startScreencast', { format: 'png', everyNthFrame: 1 });
    try {
      await page.getByRole('menuitemradio', { name, exact: true }).click();
      await waitForLanguage(tag);
    } finally { await cdp.send('Page.stopScreencast'); }
    return images;
  }
  const captureLabel = async (tag, name) => {
    await page.locator('.settings-scroll > .scroll-viewport').evaluate(el => { el.scrollTop = 0; });
    await page.mouse.move(1, 1); await settled();
    const old = await caption.boundingBox(), first = await screenshot();
    const frames = await captureLanguageChange(tag, name);
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
    }, { old, end, frames: [first, ...frames, last] });
    const start = levels[0], finish = levels.at(-1), peak = Math.max(...levels), low = Math.min(...levels);
    if (Math.min(start, finish) < 20 || levels.length < 5 || low >= .8 * Math.min(start, finish) || peak > Math.max(start, finish) + 6) {
      throw Error('Unexpected label fade: ' + JSON.stringify({ tag, start, finish, low, peak }));
    }
    return { tag, start, finish, peak };
  };

  async function checkStatusIcon(icon, tag, name, wraps) {
    const start = await icon.boundingBox();
    const frames = await captureLanguageChange(tag, name);
    const end = await icon.boundingBox();
    if (wraps && Math.abs(start.y - end.y) < 4) throw Error('Expected the translated status to wrap and move the icon');
    // The snapshot can stretch even when the live SVG reports its original bounding box.
    const sizes = await page.evaluate(async ({ frames, start, end }) => {
      const canvas = document.createElement('canvas'), ctx = canvas.getContext('2d', { willReadFrequently: true });
      const left = Math.floor(Math.min(start.x, end.x)) - 6, top = Math.floor(Math.min(start.y, end.y)) - 6;
      const right = Math.ceil(Math.max(start.x + start.width, end.x + end.width)) + 6;
      const bottom = Math.ceil(Math.max(start.y + start.height, end.y + end.height)) + 6;
      const sizes = [];
      for (const frame of frames) {
        const png = Uint8Array.from(atob(frame), c => c.charCodeAt(0));
        const bitmap = await createImageBitmap(new Blob([png], { type: 'image/png' })), scale = bitmap.width / innerWidth;
        canvas.width = Math.ceil((right - left) * scale); canvas.height = Math.ceil((bottom - top) * scale);
        ctx.drawImage(bitmap, -Math.floor(left * scale), -Math.floor(top * scale)); bitmap.close();
        const pixels = ctx.getImageData(0, 0, canvas.width, canvas.height).data;
        let x0 = canvas.width, y0 = canvas.height, x1 = -1, y1 = -1;
        for (let i = 0; i < pixels.length; i += 4) {
          if (pixels[i] - pixels[i + 1] < 80 || pixels[i + 2] - pixels[i + 1] < 80) continue;
          const x = i / 4 % canvas.width, y = Math.floor(i / 4 / canvas.width);
          x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y);
        }
        sizes.push({ width: (x1 - x0 + 1) / scale, height: (y1 - y0 + 1) / scale, y: top + y0 / scale });
      }
      return sizes;
    }, { frames, start, end });
    const moved = sizes.some(s => s.y > Math.min(start.y, end.y) && s.y < Math.max(start.y, end.y));
    if ((wraps && !moved) || sizes.length < 5 || sizes.some(s => Math.abs(s.width - start.width) > 1 || Math.abs(s.height - start.height) > 1)) {
      throw Error('Distorted or missing status icon: ' + JSON.stringify({ tag, start, end, sizes }));
    }
    return { tag, frames: sizes.length };
  }

  async function checkSaveFade() {
    const save = page.locator('.save-button'), port = page.locator('#proxy-port');
    await settled();
    async function sample(change, endDisabled) {
      // Arm the recorder before fill/click can synchronously change the button.
      const recorder = await save.evaluateHandle((el, endDisabled) => ({ frames: new Promise((resolve, reject) => {
        const initial = el.disabled, started = performance.now(), frames = [];
        let changed = false;
        const observer = new MutationObserver(() => { changed = true; });
        observer.observe(el, { attributes: true, attributeFilter: ['disabled'] });
        function record(time) {
          const opacity = Number(getComputedStyle(el).opacity);
          changed ||= el.disabled !== initial;
          const fading = el.getAnimations().some(a => a.transitionProperty === 'opacity');
          frames.push({ disabled: el.disabled, opacity, fading });
          if (changed && el.disabled === endDisabled && !fading) { observer.disconnect(); resolve(frames); }
          else if (time - started > 3000) { observer.disconnect(); reject(Error('Save availability did not settle')); }
          else requestAnimationFrame(record);
        }
        record(started);
      }) }), endDisabled);
      try { await change(); return await recorder.evaluate(recording => recording.frames); }
      finally { await recorder.dispose(); }
    }
    const value = await port.inputValue();
    const quick = await sample(() => page.locator('.setting-row .switch').first().click(), false);
    // Text edits use the real validation debounce. A switch can validate between two painted frames.
    const checking = await sample(() => port.fill(value === '12345' ? '12346' : '12345'), false);
    const invalid = await sample(() => port.fill('0'), true);
    const valid = await sample(() => port.fill(value), false);
    const min = Math.min(...checking.map(frame => frame.opacity)), dim = invalid.at(-1).opacity;
    const quickMin = Math.min(...quick.map(frame => frame.opacity));
    const intermediate = frames => frames.some(f => f.opacity > dim + .02 && f.opacity < .98);
    if (quick.at(-1).opacity !== 1 || quickMin <= dim + .1 || !checking.some(f => f.disabled) || checking.at(-1).disabled || checking.at(-1).opacity !== 1 ||
      !checking.some(f => f.opacity > min + .01 && f.opacity < .99) || min <= dim + .1 || !invalid.at(-1).disabled || valid.at(-1).disabled ||
      valid.at(-1).opacity !== 1 || !intermediate(invalid) || !intermediate(valid)) {
      throw Error('Save button fade: ' + JSON.stringify({ quick, checking, invalid, valid }));
    }
    await page.emulateMedia({ reducedMotion: 'reduce' });
    try {
      const off = await sample(() => port.fill('0'), true), on = await sample(() => port.fill(value), false);
      if ([...off, ...on].some(frame => frame.fading)) throw Error('Reduced motion still animated save opacity');
    } finally { await page.emulateMedia({ reducedMotion: null }); }
    return { quickCheck: quickMin, checking: min, invalid: dim, valid: valid.at(-1).opacity };
  }

  async function checkStatusSwap() {
    const port = page.locator('#proxy-port'), valid = await port.inputValue();
    await size(380, 690); await settled();
    const recording = await page.evaluateHandle(() => {
      const actions = document.querySelector('.footer-actions'), buttonY = actions.getBoundingClientRect().top;
      const footer = actions.closest('.settings-footer'), tops = [], steps = [];
      const positions = new Map(), errors = new Set();
      let enter = 0, exit = 0, frame;
      function sample() {
        const top = footer.getBoundingClientRect().top;
        if (tops.length) steps.push(top - tops.at(-1));
        tops.push(top);
        let visible = 0;
        for (const row of document.querySelectorAll('.footer-status .status-presence')) {
          const opacity = Number(getComputedStyle(row).opacity);
          const link = row.querySelector('.status-input-link');
          if (link && row.matches('[data-waiting], [data-reserving]') && !opacity) {
            const r = link.getBoundingClientRect();
            if (link.contains(document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2))) {
              errors.add('invisible input link intercepted a pointer');
            }
          }
          if (opacity < .05) continue;
          visible++;
          const caption = row.querySelector('[data-motion-text]'), range = document.createRange();
          range.selectNodeContents(caption);
          const ink = range.getBoundingClientRect(), clip = row.querySelector('.status-clip').getBoundingClientRect();
          if (ink.top < clip.top - 1 || ink.bottom > clip.bottom + 1) errors.add('caption clipped during fade');
          const top = positions.get(caption.textContent) ?? ink.top;
          positions.set(caption.textContent, top);
          if (Math.abs(ink.top - top) > 1) errors.add('caption moved during swap');
          if (opacity < .95) {
            if (row.hasAttribute('data-exiting')) exit++; else enter++;
          }
        }
        if (visible > 1) errors.add('both messages visible');
        if (Math.abs(actions.getBoundingClientRect().top - buttonY) > 1) errors.add('buttons moved');
        frame = requestAnimationFrame(sample);
      }
      sample();
      return { stop() {
        cancelAnimationFrame(frame);
        const travel = Math.max(...tops) - Math.min(...tops);
        if (travel > 5 && [1, -1].some(direction => !steps.some(step => direction * step > 0 && direction * step < travel - 1))) {
          errors.add('footer height jumped between short and wrapped messages');
        }
        return { enter, exit, travel, errors: [...errors] };
      } };
    });
    let result;
    try {
      for (const value of ['70000', valid]) {
        await port.fill(value);
        await page.waitForFunction(invalid => !!document.querySelector('.status-presence:not([data-exiting]) .status-input-link') === invalid, value === '70000');
        await page.waitForFunction(() => !document.querySelector('.status-presence:is([data-exiting], [data-waiting], [data-reserving])'));
        await settled();
      }
    } finally {
      result = await recording.evaluate(r => r.stop()); await recording.dispose();
      await size(600, 700); await settled();
    }
    if (!result.enter || !result.exit || result.errors.length) throw Error('Footer swap: ' + JSON.stringify(result));
    return result;
  }

  async function checkPreviewShrink() {
    const extra = page.getByRole('textbox', { name: '额外参数', exact: true });
    const originalExtra = await extra.inputValue(), marker = '--a=1111111111';
    await extra.fill(Array(15).fill(marker).join(' '));
    await page.waitForFunction(marker => document.querySelector('.argument-current').textContent.includes(marker), marker);
    await settled();
    const box = page.locator('.argument-scroll');
    const recording = await box.evaluateHandle(el => {
      const initial = el.getBoundingClientRect().height, snapshot = el.querySelector('.argument-previous');
      let visible = 0, lost = 0, frame;
      function sample() {
        if (snapshot.hasAttribute('data-text') && Number(getComputedStyle(snapshot).opacity) > .05) {
          visible++;
          lost = Math.max(lost, initial - el.getBoundingClientRect().height);
        }
        frame = requestAnimationFrame(sample);
      }
      sample();
      return { stop() { cancelAnimationFrame(frame); return { visible, lost, shrink: initial - el.getBoundingClientRect().height }; } };
    });
    let result;
    try {
      await extra.fill(originalExtra);
      await page.waitForFunction(marker => !document.querySelector('.argument-current').textContent.includes(marker), marker);
      await settled();
    } finally { result = await recording.evaluate(recorder => recorder.stop()); await recording.dispose(); }
    if (!result.visible || result.lost > 1 || result.shrink < 20) throw Error('Preview lost a visible line: ' + JSON.stringify(result));
  }

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
    await size(600, 700); await settled(); await choose('zh-CN', '简体中文');
    const codex = page.getByRole('switch', { name: '应用于 Codex 环境变量' });
    if (await codex.getAttribute('aria-checked') !== 'true') await codex.click();
    await page.waitForFunction(() => !document.querySelector('.save-button')?.disabled);
    const statusSwap = await checkStatusSwap();
    await checkPreviewShrink();
    const ink = await page.addStyleTag({ content: '.footer-status svg { color: #ff00ee !important; }' });
    const icons = [];
    try {
      const info = page.locator('.status-line[data-tone=info] > svg');
      icons.push(await checkStatusIcon(info, 'en-US', 'English', true), await checkStatusIcon(info, 'zh-CN', '简体中文', true));
      const port = page.locator('#proxy-port'), value = await port.inputValue();
      await port.fill('0'); await settled();
      icons.push(await checkStatusIcon(page.locator('.status-input-link > svg'), 'ja-JP', '日本語', false));
      await port.fill(value);
      await page.waitForFunction(() => !document.querySelector('.save-button')?.disabled);
    } finally { await ink.evaluate(el => el.remove()); }
    const saveFade = await checkSaveFade();
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
    return { icons, statusSwap, saveFade, labels, motion, layouts: 15 };
  } finally {
    await page.emulateMedia({ reducedMotion: null });
    await cdp.send('Emulation.clearDeviceMetricsOverride');
    await cdp.detach();
  }
}
