// Prueba del movimiento suave (Encargo 8, B), con Playwright: arranca una partida, mantiene pulsada una flecha y
// hace fotos seguidas del centro de la pantalla; mide cuántas imágenes por segundo pinta el juego (cdda_imagenes, si
// lo tiene) y cuántas fotos seguidas salen distintas (si el dibujo salta una vez por segundo, casi todas iguales).
//   node tools/tiempo-real/prueba-suave.mjs [--url http://localhost:8095/] [--fotos carpeta]
import { mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8095/'), FOTOS = arg('fotos', 'fotos-suave');
mkdirSync(FOTOS, { recursive: true });
const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });
const p = await b.newPage({ viewport: { width: 1280, height: 800 } });
await p.goto(URL);
await p.waitForFunction(() => window.wasmExports && window.wasmExports.cdda_turno, null, { timeout: 240000 });
await p.waitForTimeout(20000); await p.keyboard.press('d');
await p.waitForFunction(() => { const h = document.getElementById('hud'); return h && !h.classList.contains('oculto'); }, null, { timeout: 240000 });
await p.waitForTimeout(4000);
const imagenes = () => p.evaluate(() => (wasmExports.cdda_imagenes ? wasmExports.cdda_imagenes() : -1));
const caja = { x: 440, y: 200, width: 400, height: 400 };
const hashes = [];
const i0 = await imagenes(), t0 = Date.now();
await p.keyboard.down('ArrowRight');
for (let i = 0; i < 25; i++) {
  const buf = await p.screenshot({ path: `${FOTOS}/paso-${String(i).padStart(2, '0')}.png`, clip: caja });
  hashes.push(createHash('md5').update(buf).digest('hex'));
}
await p.keyboard.up('ArrowRight');
const segundos = (Date.now() - t0) / 1000, i1 = await imagenes();
let distintas = 0;
for (let i = 1; i < hashes.length; i++) if (hashes[i] !== hashes[i - 1]) distintas++;
const r = { segundos: +segundos.toFixed(1), fotos: hashes.length, seguidasDistintas: distintas, imagenesPorSegundo: i0 >= 0 ? +((i1 - i0) / segundos).toFixed(1) : null };
console.log(JSON.stringify(r));
await b.close();
