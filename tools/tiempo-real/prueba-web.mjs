// Prueba del tiempo real en la versión web, con Playwright (Chromium sin ventana):
//   node tools/tiempo-real/prueba-web.mjs [--url http://localhost:8095/] [--fotos carpeta] [--minutos 3]
// Arranca una partida desde nuestra pantalla de inicio y comprueba, leyendo el estado del juego desde JS (las
// funciones cdda_* que exporta src/realtime.cpp, en wasmExports):
//  1. pasan 24 tics por segundo (cada tic, un segundo del mundo), siempre: no hay velocidades que elegir;
//  2. con el inventario abierto el reloj sigue (ningún menú para el mundo);
//  3. F8 (la antigua pausa) no para nada;
//  4. andando, el reloj sigue a su paso (24 tics por segundo);
//  5. una partida de unos minutos moviéndose, sin cuelgues ni errores.
// Devuelve 0 si todo va bien. Hace fotos de cada paso.
import { mkdirSync } from 'node:fs';
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8095/'), FOTOS = arg('fotos', 'fotos-tiempo-real'), MINUTOS = +arg('minutos', 3);
mkdirSync(FOTOS, { recursive: true });
const VEL = { pausa: 0, lento: 1, normal: 2, rapido: 3, max: 4 };
const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });
const p = await b.newPage({ viewport: { width: 1280, height: 800 } });
const errores = [];
p.on('pageerror', (e) => errores.push(e.message.slice(0, 300)));
const r = { velocidades: {}, comprobaciones: {} };
let mal = 0;
const comprobar = (nombre, ok, info) => { r.comprobaciones[nombre] = { ok, ...info }; if (!ok) mal++; console.log(ok ? 'OK ' : 'MAL', nombre, JSON.stringify(info)); };
// el estado del juego (las funciones exportadas por src/realtime.cpp)
const estado = () => p.evaluate(() => {
  const e = window.wasmExports;
  if (!e || !e.cdda_turno) return null;
  const ptr = e.cdda_hora(), m = new Uint8Array(window.wasmMemory.buffer);
  let fin = ptr; while (m[fin]) fin++;
  return { turno: e.cdda_turno(), hora: new TextDecoder().decode(m.subarray(ptr, fin)), vel: e.cdda_rt_velocidad(), tps: e.cdda_rt_turnos_por_segundo(),
    msTurno: e.cdda_rt_ms_turno(), retrasado: e.cdda_rt_retrasado(), ventanas: e.cdda_ventanas() };
});
const foto = (n) => p.screenshot({ path: `${FOTOS}/${n}.png` });
const esperar = (ms) => p.waitForTimeout(ms);

await p.goto(URL);
// nuestra pantalla de inicio: partida nueva
await p.waitForSelector('#inicio:not(.oculto)', { timeout: 240000 });
await esperar(1000);
await p.locator('#inicio .nueva').first().click();
await esperar(500);
await p.locator('#inicio .empezar').first().click();
// hasta que el turno avanza solo (la partida ya está en marcha)
const t0 = Date.now();
let antes = null, enMarcha = false;
while (Date.now() - t0 < 240000) {
  await esperar(3000);
  const e = await estado();
  if (antes && e && e.turno > antes.turno && e.ventanas <= 2) { enMarcha = true; break; }
  antes = e;
}
comprobar('la partida arranca y el reloj corre solo', enMarcha, { segundos: Math.round((Date.now() - t0) / 1000) });
await foto('1-partida');

// 1. 24 tics por segundo
{
  await esperar(1500);
  const a = await estado(), ta = Date.now();
  await esperar(8000);
  const z = await estado(), s = (Date.now() - ta) / 1000;
  const tps = (z.turno - a.turno) / s;
  r.velocidades.normal = { turnosPorSegundo: +tps.toFixed(1), msPorTurno: +z.msTurno.toFixed(2), retrasado: !!z.retrasado, hora: z.hora };
  comprobar('pasan 24 turnos por segundo (o dice que no llega)', Math.abs(tps - 24) <= 24 * 0.1 + 0.3 || (tps < 24 && z.retrasado), r.velocidades.normal);
  await foto('2-a-24');
}

// 2. con el inventario abierto el reloj sigue
await p.keyboard.press('i');
await esperar(1500);
const m0 = await estado();
await esperar(5000);
const m1 = await estado();
await foto('3-inventario');
await p.keyboard.press('Escape');
await esperar(1000);
comprobar('con el inventario abierto el reloj sigue', m1.turno - m0.turno >= 5 * 24 * 0.8, { turnos: m1.turno - m0.turno });

// 3. F8 ya no pausa
await p.keyboard.press('F8');
await esperar(1000);
const p0 = await estado();
await esperar(4000);
const p1 = await estado();
await foto('4-sin-pausa');
comprobar('F8 no para el mundo', p1.turno - p0.turno >= 4 * 24 * 0.8 && p1.vel === VEL.normal, { turnos: p1.turno - p0.turno, vel: p1.vel });

// 4. teclas mientras corre (andar por ahí): el reloj sigue
let turnosAndando = 0, segAndando = 0, pasos = 0, menus = 0;
let prev = await estado(), tprev = Date.now();
for (let i = 0; i < 24; i++) {
  await p.keyboard.press(['ArrowUp', 'ArrowLeft', 'ArrowDown', 'ArrowRight'][Math.floor(i / 3) % 4]);
  pasos++;
  await esperar(500);
  const e = await estado(), t = Date.now();
  { turnosAndando += e.turno - prev.turno; segAndando += (t - tprev) / 1000; }
  if (e.ventanas > 1) { menus++; await p.keyboard.press('Escape'); await esperar(300); }
  prev = await estado(); tprev = Date.now();
}
const tpsAndando = turnosAndando / Math.max(0.001, segAndando);
comprobar('andando, el reloj sigue a su paso (24 tics/s)', Math.abs(tpsAndando - 24) < 24 * 0.15, { turnosPorSegundo: +tpsAndando.toFixed(2), segundosSinMenus: +segAndando.toFixed(1), pasos, menusCerrados: menus });
await foto('5-andando');

// 5. una partida de unos minutos, moviéndose de vez en cuando
const l0 = await estado(), tl = Date.now();
const teclas = ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowRight', '.'];
let quietos = 0, ultimo = l0.turno;
for (let i = 0; Date.now() - tl < MINUTOS * 60000; i++) {
  await p.keyboard.press(teclas[i % teclas.length]);
  await esperar(2000);
  const e = await estado();
  // (si un menú o una pregunta se ha quedado abierta, se cierra)
  if (e.ventanas > 1) { await p.keyboard.press('Escape'); await esperar(300); }
  if (e.turno === ultimo) quietos++;
  ultimo = e.turno;
}
const l1 = await estado(), sl = (Date.now() - tl) / 1000;
r.partida = { minutos: +(sl / 60).toFixed(1), turnos: l1.turno - l0.turno, turnosPorSegundo: +((l1.turno - l0.turno) / sl).toFixed(1), msPorTurno: +l1.msTurno.toFixed(2), hora: l1.hora, vecesParado: quietos };
// (sin cuelgues: nunca parado, sin errores, y a buen paso: al menos el 75 % de 24, con teclas y menús por medio)
comprobar(`${MINUTOS} minutos sin cuelgues`, quietos === 0 && errores.length === 0 && (l1.turno - l0.turno) / sl >= 0.75 * 24, r.partida);
await foto('6-partida-larga');

await b.close();
console.log(JSON.stringify(r, null, 1));
if (errores.length) console.log('errores de la página:', errores.slice(0, 5));
process.exit(mal || errores.length ? 1 : 0);
