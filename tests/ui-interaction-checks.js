// SPDX-License-Identifier: GPL-3.0-only
// Playwright CLI run-code: attach only to ui-prototype-tests.ps1 -Functional -RemoteDebugging.
// All input and preference changes stay in that isolated test process. Never click Save here.
async page => {
  if (page.url() !== 'http://tauri.localhost/') throw Error('Expected the embedded operational page');
  const errors = [], onError = error => errors.push(error.message);
  page.on('pageerror', onError);
  const cdp = await page.context().newCDPSession(page);
  const settle = async () => {
    await page.waitForFunction(() => !document.documentElement.dataset.appearanceTransition
      && !document.querySelector('.menu-popup, .dialog')
      && document.getAnimations().every(animation => animation.playState !== 'running'));
  };
  const language = async (label, tag) => {
    if (await page.evaluate(() => document.documentElement.lang) === tag) return;
    await page.getByRole('button', {name: /^(语言|Language|言語)$/}).click();
    await page.getByRole('menuitemradio', {name: label, exact: true}).click();
    await page.waitForFunction(expected => document.documentElement.lang === expected, tag);
    await settle();
  };
  const theme = async dark => {
    if (await page.evaluate(() => document.documentElement.classList.contains('dark')) === dark) return;
    await page.locator('.appearance-controls button').nth(1).click();
    await page.waitForFunction(expected => document.documentElement.classList.contains('dark') === expected, dark);
    await settle();
  };
  const layout = () => page.evaluate(() => {
    const within = element => {
      const rect = element.getBoundingClientRect();
      return rect.width > 0 && rect.height > 0 && rect.left >= -1 && rect.right <= innerWidth + 1
        && rect.top >= -1 && rect.bottom <= innerHeight + 1;
    };
    const footer = document.querySelector('.settings-footer');
    const actions = [...footer.querySelectorAll('button')];
    const leaked = [...document.querySelectorAll('[style]')].filter(element =>
      element.style.viewTransitionName || element.style.getPropertyValue('transition') === 'none');
    return {width: innerWidth, height: innerHeight, dpr: devicePixelRatio,
      overflow: document.documentElement.scrollWidth > innerWidth,
      footerVisible: within(footer) && actions.every(within), leakedStyles: leaked.length,
      language: document.documentElement.lang, dark: document.documentElement.classList.contains('dark'),
      sheets: document.styleSheets.length};
  });
  const retained = async () => {
    // Sample the same hover state; delayed help tooltips otherwise change DOM counts.
    await page.mouse.move(2, 2);
    await page.locator('.tooltip-popup').waitFor({state: 'detached'});
    await settle();
    await cdp.send('HeapProfiler.collectGarbage');
    return {...await cdp.send('Memory.getDOMCounters'), ...await cdp.send('Runtime.getHeapUsage'), layout: await layout()};
  };
  try {
    const advanced = page.locator('.advanced-trigger');
    if (await advanced.getAttribute('aria-expanded') !== 'true') await advanced.click();
    const directory = await page.locator('.advanced-card input').first().inputValue();
    if (!/\/work\/ui-functional-tests\/[a-f0-9]{32}\/state$/i.test(directory.replaceAll('\\', '/'))) {
      throw Error('Refusing preference changes outside an isolated functional session');
    }
    if (await page.locator('.section-heading .switch').getAttribute('aria-checked') !== 'true') {
      await page.locator('.section-heading .switch').click();
      await page.waitForFunction(() => document.querySelector('#proxy-port')?.value !== '');
    }
    const extra = page.locator('.advanced-card input').nth(2);
    const literal = '"" "中文 参数" "quote\\"end"';
    await extra.fill(literal);
    await page.waitForFunction(() => !document.querySelector('.footer-actions .primary-button')?.disabled);
    // Let the one-shot detection notice finish before comparing persistent UI resources.
    const dismiss = page.locator('.notice-close');
    if (await dismiss.isVisible()) await dismiss.click();
    await settle();
    const languages = [['简体中文', 'zh-CN'], ['English', 'en-US'], ['日本語', 'ja-JP']];
    const matrix = [];
    for (const scale of [1, 1.5, 2]) {
      // Zero dimensions retain the actual native window size. Only rendering density is emulated.
      await cdp.send('Emulation.setDeviceMetricsOverride', {width: 0, height: 0, deviceScaleFactor: scale, mobile: false});
      for (const dark of [true, false]) {
        await theme(dark);
        for (const [label, tag] of languages) {
          await language(label, tag);
          const state = await layout();
          if (state.overflow || !state.footerVisible || state.leakedStyles || state.dpr !== scale) throw Error(JSON.stringify(state));
          if (await extra.inputValue() !== literal) throw Error('Appearance change lost input');
          const help = page.locator('.section-heading .help-button');
          await help.click();
          const dialog = page.getByRole('dialog');
          await dialog.waitFor({state: 'visible'});
          await page.waitForFunction(() => document.querySelector('.dialog')?.getAnimations().every(animation => animation.playState !== 'running'));
          const buttonsVisible = await page.locator('.dialog-actions').evaluate(element => {
            const r = element.getBoundingClientRect();
            return r.top >= 0 && r.bottom <= innerHeight && r.left >= 0 && r.right <= innerWidth;
          });
          if (!buttonsVisible) throw Error('Dialog actions clipped');
          await page.keyboard.press('Escape');
          await dialog.waitFor({state: 'detached'});
          if (!await help.evaluate(element => element === document.activeElement)) throw Error('Help did not restore keyboard focus');
          matrix.push({...state, dialogActionsVisible: buttonsVisible});
        }
      }
    }
    await cdp.send('Emulation.clearDeviceMetricsOverride');
    await language('简体中文', 'zh-CN'); await theme(true);
    const before = await retained();
    for (let round = 0; round < 30; round++) {
      const [label, tag] = languages[(round + 1) % languages.length];
      await language(label, tag); await theme(round % 2 === 1);
      await page.getByRole('radio', {name: ['HTTPS', 'SOCKS5', 'HTTP'][round % 3], exact: true}).click();
      await advanced.click(); await advanced.click();
      await settle();
      if (await extra.inputValue() !== literal) throw Error('Repeated operations lost input');
    }
    await language('简体中文', 'zh-CN'); await theme(true);
    await page.getByRole('radio', {name: 'HTTP', exact: true}).click();
    await page.keyboard.press('ArrowRight');
    if (await page.getByRole('radio', {name: 'HTTPS', exact: true}).getAttribute('aria-checked') !== 'true') throw Error('Protocol arrow key failed');
    await page.keyboard.press('ArrowLeft');
    const startup = page.locator('.setting-row .switch').first();
    const checked = await startup.getAttribute('aria-checked');
    await page.locator('.setting-label [data-motion-text]').first().click();
    if (await startup.getAttribute('aria-checked') !== checked) throw Error('Option text toggled the switch');
    await startup.click(); await page.keyboard.press('Space');
    if (await startup.getAttribute('aria-checked') !== checked) throw Error('Switch keyboard behavior changed');
    await page.waitForFunction(() => !document.querySelector('.footer-actions .primary-button')?.disabled);
    const after = await retained();
    if (after.layout.sheets !== before.layout.sheets || after.layout.leakedStyles) throw Error('Temporary appearance styles accumulated');
    if (after.nodes > before.nodes + 20 || after.jsEventListeners > before.jsEventListeners + 20) {
      throw Error(JSON.stringify({message: 'Retained DOM resources increased', before, after}));
    }
    if (errors.length) throw Error(JSON.stringify(errors));
    return {matrix, repeatedRounds: 30, before, after, errors};
  } finally {
    await cdp.send('Emulation.clearDeviceMetricsOverride');
    await cdp.detach();
    page.off('pageerror', onError);
  }
}
