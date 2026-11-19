// SPDX-License-Identifier: GPL-3.0-only
// Run in a fresh functional window. After the explicit reload, inspect with -ExpectedReads 2.
async page => {
  if (page.url() !== 'http://tauri.localhost/') throw Error('Expected the embedded settings page');
  const advanced = page.locator('.advanced-trigger');
  const wasOpen = await advanced.getAttribute('aria-expanded') === 'true';
  if (!wasOpen) await advanced.click();
  try {
    const directory = await page.locator('.advanced-card input').first().inputValue();
    if (!/\/work\/ui-functional-tests\/[a-f0-9]{32}\/state$/i.test(directory.replaceAll('\\', '/'))) {
      throw Error('Expected an isolated functional session');
    }
    if (await page.locator('#unsaved-status').count()) throw Error('Expected a clean form');
    // Reload inside the existing WebView so the observer is present before React mounts.
    await page.addInitScript(() => {
      const observer = new MutationObserver(() => {
        if (document.querySelector('.save-button:enabled')) {
          performance.mark('settings-save-enabled');
          observer.disconnect();
        }
      });
      observer.observe(document, { childList: true, attributes: true, subtree: true });
    });
    await page.reload({ waitUntil: 'domcontentloaded' });
    await page.waitForFunction(() => performance.getEntriesByName('settings-save-enabled').length > 0);
    const pageReadyMs = await page.evaluate(() => performance.getEntriesByName('settings-save-enabled')[0].startTime);
    await advanced.click();
    const interactions = await page.evaluate(async () => {
      const input = document.querySelectorAll('.advanced-card input')[2];
      const toggle = document.querySelectorAll('[aria-labelledby="startup-title"] [role=switch]')[1];
      const save = document.querySelector('.save-button');
      const preview = document.querySelector('.argument-preview');
      const original = input.value, checked = toggle.getAttribute('aria-checked');
      const setValue = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
      function edit(value) {
        setValue.call(input, value);
        input.dispatchEvent(new Event('input', { bubbles: true }));
      }
      function until(ready) {
        if (ready()) return Promise.resolve(performance.now());
        return new Promise((resolve, reject) => {
          const observer = new MutationObserver(() => {
            if (ready()) {
              observer.disconnect();
              clearTimeout(timeout);
              resolve(performance.now());
            }
          });
          const timeout = setTimeout(() => { observer.disconnect(); reject(Error('Preview did not settle')); }, 4000);
          observer.observe(document.querySelector('#root'), { childList: true, attributes: true, characterData: true, subtree: true });
        });
      }
      const toggles = [], typing = [];
      try {
        for (let round = 0; round < 4; round++) {
          const before = toggle.getAttribute('aria-checked'), start = performance.now();
          toggle.click();
          toggles.push((await until(() => toggle.getAttribute('aria-checked') !== before && !save.disabled)) - start);
          const prefix = '--perf-' + round + '-';
          for (let length = 1; length < 7; length++) {
            edit(prefix + 'x'.repeat(length));
            await new Promise(resolve => setTimeout(resolve, 12));
          }
          const last = performance.now(), final = prefix + 'x'.repeat(7);
          edit(final);
          typing.push((await until(() => preview.textContent.includes(final) && !save.disabled)) - last);
        }
      } finally {
        if (toggle.getAttribute('aria-checked') !== checked) toggle.click();
        if (input.value !== original) edit(original);
        await until(() => !document.getElementById('unsaved-status') && !save.disabled);
      }
      return { toggles, typing };
    });
    return { pageReadyMs, ...interactions };
  } finally {
    if (!wasOpen && await advanced.getAttribute('aria-expanded') === 'true') await advanced.click();
  }
}
