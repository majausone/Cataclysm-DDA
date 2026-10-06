// Prueba del tiempo real en la versión web, con Playwright (Chromium sin ventana):
//   node tools/tiempo-real/prueba-web.mjs [--url http://localhost:8095/] [--fotos carpeta] [--minutos 3]
// Arranca una partida («Play Now! (Default Scenario)») y comprueba, leyendo el estado del juego desde JS (las
// funciones cdda_* que exporta src/realtime.cpp, en wasmExports):
//  1. a cada velocidad pasan los turnos por segundo que tocan (y cuánto tarda cada turno);
//  2. con un menú abierto (el inventario) el turno no avanza, y al cerrarlo sigue;
//  3. en pausa (F8) no avanza, y al quitarla sigue;
//  4. pulsar teclas mientras corre no lo para ni lo atasca;
//  5. una partida de unos minutos a x72 moviéndose, sin cuelgues ni errores.
// Devuelve 0 si todo va bien. Hace fotos de cada paso.
import { mkdirSync } from 'node:fs';
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8095/'), FOTOS = arg('fotos', 'fotos-tiempo-real'), MINUTOS = +arg('minutos', 3);
mkdirSync(FOTOS, { recursive: true });
const VEL = { pausa: 0, x1: 1, x3: 2, x10: 3, x30: 4, x72: 5, max: 6 };
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
const poner = (v) => p.evaluate((v) => window.wasmExports.cdda_rt_poner_velocidad(v), v);
const foto = (n) => p.screenshot({ path: `${FOTOS}/${n}.png` });
const esperar = (ms) => p.waitForTimeout(ms);

await p.goto(URL);
// el primer menú (idioma), y la partida rápida
await p.waitForFunction(() => window.wasmExports && window.wasmExports.cdda_turno, null, { timeout: 180000 });
await esperar(20000);
await p.keyboard.press('Enter');
await esperar(15000);
await p.keyboard.press('d');
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

// 1. velocidades
for (const [nombre, v, seg] of [['x1', VEL.x1, 10], ['x3', VEL.x3, 8], ['x10', VEL.x10, 6], ['x30', VEL.x30, 6], ['x72', VEL.x72, 6], ['max', VEL.max, 6]]) {
  await poner(v);
  await esperar(1500);
  const a = await estado(), ta = Date.now();
  await esperar(seg * 1000);
  const z = await estado(), s = (Date.now() - ta) / 1000;
  const tps = (z.turno - a.turno) / s, pedido = [0, 1, 3, 10, 30, 72][v];
  r.velocidades[nombre] = { turnosPorSegundo: +tps.toFixed(1), msPorTurno: +z.msTurno.toFixed(2), retrasado: !!z.retrasado, hora: z.hora };
  // (a las de verdad, lo pedido con un 10 % de margen; si no llega, que lo diga en pantalla: retrasado)
  if (pedido) comprobar(`a ${nombre} pasan ${pedido} turnos por segundo (o dice que no llega)`, Math.abs(tps - pedido) <= pedido * 0.1 + 0.3 || (tps < pedido && z.retrasado), r.velocidades[nombre]);
  else {
    // (a la máxima, lo más rápido que puede: como la más rápida de las otras, que ya iban al tope, o más)
    const tope = Math.max(...Object.entries(r.velocidades).filter(([k]) => k !== 'max').map(([, x]) => x.turnosPorSegundo));
    comprobar('a la máxima va todo lo rápido que puede', tps >= tope * 0.9, { ...r.velocidades[nombre], topeDeLasOtras: tope });
  }
  await foto(`2-velocidad-${nombre}`);
}

// 2. un menú abierto para el reloj
await poner(VEL.x10);
await esperar(1000);
await p.keyboard.press('i');
await esperar(1500);
const m0 = await estado();
await esperar(5000);
const m1 = await estado();
await foto('3-menu');
await p.keyboard.press('Escape');
await esperar(3000);
const m2 = await estado();
comprobar('con el inventario abierto el turno no avanza', m1.turno === m0.turno && m0.ventanas > 1, { turnos: m1.turno - m0.turno, ventanas: m0.ventanas });
comprobar('y al cerrarlo sigue', m2.turno > m1.turno, { turnos: m2.turno - m1.turno });

// 3. pausa con F8
await p.keyboard.press('F8');
await esperar(1000);
const p0 = await estado();
await esperar(4000);
const p1 = await estado();
await foto('4-pausa');
await p.keyboard.press('F8');
await esperar(3000);
const p2 = await estado();
comprobar('en pausa (F8) no avanza', p1.turno === p0.turno && p0.vel === VEL.pausa, { turnos: p1.turno - p0.turno, vel: p0.vel });
comprobar('y al quitar la pausa sigue a la velocidad de antes', p2.turno > p1.turno && p2.vel === VEL.x10, { turnos: p2.turno - p1.turno, vel: p2.vel });

// 4. teclas mientras corre (andar por ahí): el reloj sigue. Solo cuenta el tiempo sin ningún menú abierto (al
// chocar con alguien o con un mueble el juego pregunta, y con la pregunta abierta el reloj se para, como debe)
await poner(VEL.x1);
await esperar(1000);
let turnosAndando = 0, segAndando = 0, pasos = 0, menus = 0;
let prev = await estado(), tprev = Date.now();
for (let i = 0; i < 24; i++) {
  await p.keyboard.press(['ArrowUp', 'ArrowLeft', 'ArrowDown', 'ArrowRight'][Math.floor(i / 3) % 4]);
  pasos++;
  await esperar(500);
  const e = await estado(), t = Date.now();
  if (e.ventanas <= 1 && prev.ventanas <= 1) { turnosAndando += e.turno - prev.turno; segAndando += (t - tprev) / 1000; }
  if (e.ventanas > 1) { menus++; await p.keyboard.press('Escape'); await esperar(300); }
  prev = await estado(); tprev = Date.now();
}
const tpsAndando = turnosAndando / Math.max(0.001, segAndando);
comprobar('andando a x1, el reloj sigue a su paso', Math.abs(tpsAndando - 1) < 0.2, { turnosPorSegundo: +tpsAndando.toFixed(2), segundosSinMenus: +segAndando.toFixed(1), pasos, menusCerrados: menus });
await foto('5-andando');

// 5. una partida de unos minutos a x72, moviéndose de vez en cuando
await poner(VEL.x72);
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
// (sin cuelgues: nunca parado, sin errores, y a buen paso: al menos el 75 % del tope medido, con teclas y menús por medio)
const tope = Math.max(...Object.values(r.velocidades).map((x) => x.turnosPorSegundo));
r.partida.tope = tope;
comprobar(`${MINUTOS} minutos a x72 sin cuelgues`, quietos === 0 && errores.length === 0 && (l1.turno - l0.turno) / sl >= 0.75 * Math.min(72, tope), r.partida);
await foto('6-partida-larga');

await b.close();
console.log(JSON.stringify(r, null, 1));
if (errores.length) console.log('errores de la página:', errores.slice(0, 5));
process.exit(mal || errores.length ? 1 : 0);
