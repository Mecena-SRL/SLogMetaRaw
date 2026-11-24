// Rigenera i PNG dagli SVG (richiede Playwright): node assets/icons/render.js
// OFX cerca <identificativo plugin>.png/.svg in Contents/Resources del bundle.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage({ viewport: { width: 256, height: 256 } });
  for (const f of fs.readdirSync(__dirname).filter(f => f.endsWith('.svg'))) {
    await p.setContent(`<body style="margin:0">${fs.readFileSync(path.join(__dirname, f), 'utf8')}</body>`);
    await p.locator('svg').screenshot({ path: path.join(__dirname, f.replace(/\.svg$/, '.png')), omitBackground: true });
  }
  await b.close();
})();
