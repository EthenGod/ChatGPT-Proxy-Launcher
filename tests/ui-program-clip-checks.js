// SPDX-License-Identifier: GPL-3.0-only
// Playwright CLI run-code. Inspect the composited frames, including snapshot layers outside the live DOM.
async page => {
  const url = new URL(page.url());
  if (url.origin !== 'http://tauri.localhost' || url.pathname !== '/prototype.html' || url.searchParams.get('scenario') !== 'programs') {
    throw Error('Expected the standalone companion sample');
  }
  await page.setViewportSize({ width: 640, height: 1240 });
  const area = page.locator('.program-scroll > .scroll-viewport');
  const row = area.locator('.program-list > li');
  await row.last().waitFor();
  await page.locator('.settings-scroll > .scroll-viewport').evaluate(el => { el.scrollTop = 0; });
  const cdp = await page.context().newCDPSession(page);
  let frames = [];
  cdp.on('Page.screencastFrame', event => {
    frames.push(event.data);
    void cdp.send('Page.screencastFrameAck', { sessionId: event.sessionId });
  });
  const records = [];
  try {
    for (const above of [false, true]) {
      await area.evaluate((el, end) => { el.scrollTop = end ? el.scrollHeight : 0; }, above);
      await page.waitForFunction(() => !document.querySelector('.program-scroll > .scroll-track[data-scrolling]') &&
        document.getAnimations().every(animation => animation.playState !== 'running'));
      const selector = above ? '.program-list li:first-child .program-badge [data-motion-text]' : '.program-list li:last-child .program-state [data-motion-text]';
      const style = await page.addStyleTag({ content: selector + ' {color:#fa00f0!important;background:#fa00f0!important;} ' +
        '.program-list li:nth-child(2) .program-badge [data-motion-text] {color:cyan!important;background:cyan!important;}' });
      try {
        const box = await page.locator(selector).boundingBox();
        const firstClip = await area.boundingBox();
        if (!box || box.y < 0 || box.y + box.height > 1240 || (above ? box.y + box.height > firstClip.y : box.y < firstClip.y + firstClip.height)) {
          throw Error('The marked label must lie inside the window and outside its own viewport');
        }
        const tag = await page.locator('html').getAttribute('lang') === 'en-US' ? 'ja-JP' : 'en-US';
        await page.locator('.appearance-controls button').first().click();
        frames = [];
        await cdp.send('Page.startScreencast', { format: 'png', everyNthFrame: 1 });
        await page.getByRole('menuitemradio', { name: tag === 'en-US' ? 'English' : '日本語', exact: true }).click();
        await page.waitForFunction(value => document.documentElement.lang === value &&
          !document.documentElement.hasAttribute('data-appearance-transition'), tag);
        await cdp.send('Page.stopScreencast');
        const lastClip = await area.boundingBox();
        const edge = above ? Math.min(firstClip.y, lastClip.y) : Math.max(firstClip.y + firstClip.height, lastClip.y + lastClip.height);
        const stats = await page.evaluate(async ({ frames, above, edge }) => {
          const canvas = document.createElement('canvas'), context = canvas.getContext('2d', { willReadFrequently: true });
          let leaked = 0, visible = 0, firstLeak = null;
          for (const png of frames) {
            const bytes = Uint8Array.from(atob(png), c => c.charCodeAt(0));
            const bitmap = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
            canvas.width = bitmap.width; canvas.height = bitmap.height;
            context.drawImage(bitmap, 0, 0); bitmap.close();
            const pixels = context.getImageData(0, 0, canvas.width, canvas.height).data;
            const boundary = Math.round(edge * canvas.width / innerWidth);
            // A solid run marks the painted label. An individual colored subpixel can be ordinary ClearType text.
            let outside = false, control = false, pinkRun = 0, cyanRun = 0;
            for (let i = 0; i < pixels.length && !(outside && control); i += 4) {
              const x = (i / 4) % canvas.width;
              if (x === 0) { pinkRun = 0; cyanRun = 0; }
              cyanRun = pixels[i] < 40 && pixels[i + 1] > 195 && pixels[i + 2] > 195 ? cyanRun + 1 : 0;
              pinkRun = pixels[i] > 145 && pixels[i + 1] < 55 && pixels[i + 2] > 140 ? pinkRun + 1 : 0;
              if (cyanRun >= 8) control = true;
              if (pinkRun >= 8) {
                const y = Math.floor(i / 4 / canvas.width);
                if (above ? y < boundary - 1 : y > boundary + 1) {
                  outside = true;
                  firstLeak ??= { x, y, boundary, width: canvas.width, height: canvas.height };
                }
              }
            }
            if (outside) leaked++;
            if (control) visible++;
          }
          return { frames: frames.length, visible, leaked, firstLeak };
        }, { frames, above, edge });
        if (stats.frames < 4 || !stats.visible || stats.leaked) throw Error('Snapshot clipping: ' + JSON.stringify({ above, firstClip, lastClip, box, stats }));
        records.push({ edge: above ? 'top' : 'bottom', ...stats });
      } finally { await style.evaluate(el => el.remove()); }
    }
  } finally { await cdp.detach(); }
  return records;
}
