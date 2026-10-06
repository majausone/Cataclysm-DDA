// Medida rápida: arranca una partida y mide los turnos por segundo a x72 y a la máxima (6 s cada una).
//   node tools/tiempo-real/medir-rapido.mjs [--url http://localhost:8095/] [--foto f.png]
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });
const p = await b.newPage({ viewport: { width: 1280, height: 800 } });
await p.goto(arg('url', 'http://localhost:8095/'));
await p.waitForFunction(() => window.wasmExports && window.wasmExports.cdda_turno, null, { timeout: 180000 });
await p.waitForTimeout(20000); await p.keyboard.press('Enter'); await p.waitForTimeout(15000); await p.keyboard.press('d');
const turno = () => p.evaluate(() => [window.wasmExports.cdda_turno(), window.wasmExports.cdda_rt_ms_turno()]);
let a = await turno();
for (let i = 0; i < 80; i++) { await p.waitForTimeout(3000); const z = await turno(); if (z[0] > a[0]) break; a = z; }
for (const v of [5, 6]) {
  await p.evaluate((v) => window.wasmExports.cdda_rt_poner_velocidad(v), v);
  await p.waitForTimeout(1500);
  const x = await turno(), t = Date.now(); await p.waitForTimeout(6000); const y = await turno();
  console.log(v === 5 ? 'x72' : 'max', ((y[0] - x[0]) / ((Date.now() - t) / 1000)).toFixed(1), 't/s,', y[1].toFixed(1), 'ms/turno');
}
if (arg('foto')) await p.screenshot({ path: arg('foto') });
await b.close();
