// SPDX-License-Identifier: GPL-3.0-only
// Playwright CLI run-code, attached only to an isolated functional WebView2 session.
// Changes inputs and in-memory preferences. Never saves or opens native system actions.
async page => {
  if (page.url() !== 'http://tauri.localhost/') throw Error('Expected embedded operational page');
  const advanced = page.locator('.advanced-trigger');
  if (await advanced.getAttribute('aria-expanded') !== 'true') await advanced.click();
  const directoryInput = page.locator('.advanced-card input').first();
  const directory = await directoryInput.inputValue();
  if (!/\/work\/ui-functional-tests\/[a-f0-9]{32}\/state$/i.test(directory.replaceAll('\\', '/'))) throw Error('Expected isolated functional paths');
  if (await page.evaluate(() => document.documentElement.lang) !== 'zh-CN') throw Error('Start this check with Simplified Chinese selected');

  const errors = [], onError = error => errors.push(error.message);
  const cdp = await page.context().newCDPSession(page);
  page.on('pageerror', onError);
  // Focus guards redirect on the next frame. Observe the settled keyboard destination.
  const focusFrame = () => page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  const key = async value => { await page.keyboard.press(value); await focusFrame(); };
  const tabTo = async target => {
    for (let step = 0; step < 70; step++) {
      if (await target.evaluate(element => element === document.activeElement)) return;
      await key('Tab');
    }
    throw Error('Keyboard cannot reach ' + await target.getAttribute('class'));
  };
  const settle = () => page.waitForFunction(() => !document.documentElement.dataset.appearanceTransition
    && document.getAnimations().every(animation => animation.playState !== 'running'));
  const geometry = async () => {
    const value = await page.evaluate(() => ({ width: innerWidth, height: innerHeight,
      overflow: document.documentElement.scrollWidth > innerWidth,
      actionsVisible: [...document.querySelectorAll('.footer-actions button, .dialog-actions button')].every(element => {
        const r = element.getBoundingClientRect(); return r.top >= 0 && r.bottom <= innerHeight && r.left >= 0 && r.right <= innerWidth;
      }) }));
    if (value.overflow || !value.actionsVisible) throw Error('Clipped layout: ' + JSON.stringify(value));
    return value;
  };
  const modalKeyboard = async () => {
    await page.getByRole('dialog').waitFor({ state: 'visible' }); await settle();
    for (const value of ['Tab', 'Tab', 'Tab', 'Shift+Tab', 'Shift+Tab', 'Shift+Tab']) {
      await key(value);
      if (!await page.evaluate(() => !!document.activeElement?.closest('.dialog'))) throw Error('Keyboard focus escaped modal');
    }
    return geometry();
  };
  const closeModal = async trigger => {
    await key('Escape'); await page.getByRole('dialog').waitFor({ state: 'detached' });
    const handle = await trigger.elementHandle();
    try { await page.waitForFunction(element => element === document.activeElement, handle); }
    finally { await handle.dispose(); }
  };
  const valid = () => page.waitForFunction(() => !document.querySelector('.footer-actions .primary-button')?.disabled);
  try {
    const proxy = page.getByRole('switch', { name: '使用指定代理' });
    if (await proxy.getAttribute('aria-checked') !== 'true') {
      await tabTo(proxy); await key('Space');
      await page.waitForFunction(() => document.querySelector('#proxy-port')?.value === '7897');
    }
    const dismiss = page.locator('.notice-close');
    if (await dismiss.isVisible()) await dismiss.click();
    const port = page.getByRole('textbox', { name: '端口', exact: true });
    await port.fill('7897');
    await port.blur();
    await port.hover({ position: { x: 8, y: 12 } }); await settle();
    const portBorder = () => port.evaluate(element => getComputedStyle(element).borderTopColor);
    const fieldHover = await portBorder();
    await page.locator('.port-field .input-action').hover(); await settle();
    if (await portBorder() !== fieldHover) throw Error('Port border changed over its embedded button');
    const extra = page.getByRole('textbox', { name: '额外参数', exact: true });
    const literal = '"" "中文 参数" "quote\\"end" ' + Array(60).fill('--sample="长文本 abcdefghijklmnopqrstuvwxyz"').join(' ');
    await extra.fill(literal); await valid();
    const normal = await geometry();
    const nativeErrors = [];
    const details = page.locator('.preview-details:not([inert]) button');
    const previewOffset = () => page.locator('.preview-heading').evaluate(heading =>
      heading.nextElementSibling.getBoundingClientRect().top - heading.getBoundingClientRect().top);
    const restingOffset = await previewOffset();
    for (const scenario of ['port', 'directory']) {
      if (scenario === 'port') await port.fill('70000');
      else await directoryInput.fill(directory + '\\' + Array(12).fill('长路径abcdefghijklmnopqrstuvwxyz').join('\\'));
      await details.waitFor({ state: 'visible' });
      if (Math.abs(await previewOffset() - restingOffset) > 1) throw Error('Opening details moved the argument preview');
      if (scenario === 'port') {
        const review = page.getByRole('button', { name: '查看输入问题' });
        await review.hover(); await tabTo(review); await settle();
        const fullyVisible = await review.evaluate(button => {
          const rect = button.getBoundingClientRect(), viewport = button.closest('.scroll-viewport').getBoundingClientRect();
          const style = getComputedStyle(button);
          const ring = Math.max(0, parseFloat(style.outlineWidth) + parseFloat(style.outlineOffset));
          return rect.left - ring >= viewport.left && rect.right + ring <= viewport.right
            && rect.top - ring >= viewport.top && rect.bottom + ring <= viewport.bottom;
        });
        if (!fullyVisible) throw Error('The input warning or its focus ring is clipped');
        const centered = await page.locator('.save-button[data-edited]').evaluate(button => {
          const label = button.querySelector('[data-motion-text]').getBoundingClientRect();
          const startInset = button.clientLeft + parseFloat(getComputedStyle(button, '::before').insetInlineStart);
          const endInset = button.getBoundingClientRect().right - label.right;
          return Math.abs(startInset - endInset) < 1;
        });
        if (!centered) throw Error('The unsaved marker and save label are off-center');
      }
      await tabTo(details); await key('Enter');
      const layout = await modalKeyboard();
      const code = await page.locator('.error-heading code').innerText();
      if (!new RegExp('(?:^|\\s)' + (scenario === 'port' ? '87' : '206') + '$').test(code)) throw Error('Incorrect native error: ' + code);
      if (!await page.locator('.error-advice').innerText()) throw Error('Missing recovery advice');
      const summary = page.locator('.error-guide summary');
      if (!await summary.evaluate(el => getComputedStyle(el).listStyleType === 'none' && !!el.querySelector('svg[aria-hidden="true"]'))) {
        throw Error('The technical details must display one custom disclosure arrow');
      }
      await tabTo(summary); await key('Enter');
      if (!await page.locator('.error-guide details').evaluate(element => element.open)) throw Error('Technical details did not expand');
      await geometry(); await closeModal(details);
      nativeErrors.push({ scenario, code, layout });
      if (scenario === 'port') await port.fill('7897'); else await directoryInput.fill(directory);
      await valid();
      if (await details.count() || Math.abs(await previewOffset() - restingOffset) > 1) throw Error('Dismissed input error left an active action or moved the preview');
    }
    const modes = [
      { name: 'reduced', reducedMotion: 'reduce', forcedColors: 'none', contrast: 'no-preference', colorScheme: 'light' },
      { name: 'forced-light', reducedMotion: 'no-preference', forcedColors: 'active', contrast: 'no-preference', colorScheme: 'light' },
      { name: 'forced-dark', reducedMotion: 'no-preference', forcedColors: 'active', contrast: 'no-preference', colorScheme: 'dark' },
      { name: 'contrast', reducedMotion: 'no-preference', forcedColors: 'none', contrast: 'more', colorScheme: 'light' },
    ];
    const appearances = [];
    for (const mode of modes) {
      for (const dark of [true, false]) {
        // Let the native theme command finish before applying a controlled media fixture.
        await page.emulateMedia({ reducedMotion: null, forcedColors: null, contrast: null, colorScheme: null });
        if (await page.evaluate(() => document.documentElement.classList.contains('dark')) !== dark) {
          await page.locator('.appearance-controls button').nth(1).click();
          await page.waitForFunction(value => document.documentElement.classList.contains('dark') === value, dark);
          await settle(); await focusFrame();
        }
        const { name, ...media } = mode; await page.emulateMedia(media); await focusFrame();
        const help = page.getByRole('button', { name: 'Codex .env 配置说明' });
        await tabTo(help); await key('Enter');
        const layout = await modalKeyboard();
        const body = page.locator('.dialog-scroll > .scroll-viewport');
        const overflow = await body.evaluate(element => element.scrollHeight > element.clientHeight);
        if (overflow) {
          const title = page.getByRole('heading', { name: 'Codex .env 配置说明' });
          const top = await title.evaluate(element => element.getBoundingClientRect().top);
          await tabTo(body); await key('End');
          if (!await body.evaluate(element => element.scrollTop > 0)) throw Error('Keyboard cannot scroll help');
          const after = await title.evaluate(element => element.getBoundingClientRect().top);
          if (Math.abs(after - top) > 1 || after < 0) throw Error('Help title moved out of view');
        }
        await closeModal(help); await key('Tab'); await page.mouse.move(2, 2);
        await page.locator('.tooltip-popup').waitFor({ state: 'detached' }); await settle();
        const styles = await page.evaluate(() => {
          const selected = getComputedStyle(document.querySelector('.segment[data-checked]'));
          const card = getComputedStyle(document.querySelector('.settings-card'));
          return { background: selected.backgroundColor, color: selected.color, border: card.borderTopColor,
            cardColor: card.color, transition: selected.transitionDuration, scheme: getComputedStyle(document.documentElement).colorScheme,
            reduced: matchMedia('(prefers-reduced-motion: reduce)').matches, forced: matchMedia('(forced-colors: active)').matches,
            animations: document.getAnimations().filter(animation => animation.playState === 'running').length };
        });
        if (name === 'reduced' && (!styles.reduced || styles.transition !== '0s' || styles.animations)) throw Error('Reduced motion still animates');
        if (name.startsWith('forced') && (!styles.forced || styles.background === styles.color || styles.border !== styles.cardColor)) throw Error('Forced colors lost contrast: ' + JSON.stringify(styles));
        const tree = await cdp.send('Accessibility.getFullAXTree');
        if (tree.nodes.some(node => !node.ignored && ['button', 'switch', 'radio', 'textbox'].includes(node.role?.value) && !node.name?.value)) throw Error('Unnamed accessible control');
        appearances.push({ name, dark, layout, scrollableHelp: overflow, styles });
      }
    }
    if (await extra.inputValue() !== literal) throw Error('Appearance changes lost input');
    if (errors.length) throw Error(JSON.stringify(errors));
    return { normal, nativeErrors, appearances, inputRetained: true, errors };
  } finally {
    await page.emulateMedia({ reducedMotion: null, forcedColors: null, contrast: null, colorScheme: null });
    await cdp.detach(); page.off('pageerror', onError);
  }
}
