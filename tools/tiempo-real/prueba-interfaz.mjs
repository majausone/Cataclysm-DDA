// Prueba de la interfaz web (Encargo 7, B), con Playwright (Chromium sin ventana):
//   node tools/tiempo-real/prueba-interfaz.mjs [--url http://localhost:8095/] [--fotos carpeta]
// Arranca una partida y comprueba, con fotos de cada paso:
//  1. el HUD (hora, necesidades, cuerpo, mano) y el registro de mensajes se pintan y se actualizan;
//  2. clic en una casilla del mapa: sale un menú con sus acciones; en un NPC, «Hablar» abre el diálogo y se puede
//     salir de él (el juego no se queda colgado);
//  3. el botón del menú abre el panel, el juego se pausa, cada pestaña se pinta, y al cerrarlo sigue a su velocidad;
//  4. con 100 zombis alrededor, el día normal sigue a 24 tics por segundo (y cuánto cuesta cada tic);
//  5. en un móvil (390×844) se ve entero.
// Devuelve 0 si todo va bien.
import { mkdirSync } from 'node:fs';
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8095/'), FOTOS = arg('fotos', 'fotos-interfaz');
mkdirSync(FOTOS, { recursive: true });
const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });
let mal = 0;
const r = {};
const comprobar = (n, ok, info = {}) => { r[n] = { ok, ...info }; if (!ok) mal++; console.log(ok ? 'OK ' : 'MAL', n, JSON.stringify(info)); };
const esperar = (p, ms) => p.waitForTimeout(ms);

async function arrancar(p) {
  await p.goto(URL);
  await p.waitForFunction(() => window.wasmExports && window.wasmExports.cdda_turno, null, { timeout: 240000 });
  await esperar(p, 20000); await p.keyboard.press('Enter'); await esperar(p, 15000); await p.keyboard.press('d');
  await p.waitForFunction(() => { const h = document.getElementById('hud'); return h && !h.classList.contains('oculto'); }, null, { timeout: 240000 });
  await esperar(p, 4000);
}
const estado = (p) => p.evaluate(() => ({ turno: wasmExports.cdda_turno(), vel: wasmExports.cdda_rt_velocidad(), ventanas: wasmExports.cdda_ventanas(), tps: wasmExports.cdda_rt_turnos_por_segundo() }));
// el píxel (en coordenadas de la página) del centro de la casilla (dx, dy) respecto del jugador
const pixelDe = (p, dx, dy) => p.evaluate(([dx, dy]) => {
  const c = document.getElementById('canvas'), rect = c.getBoundingClientRect();
  const leer = (ptr) => { const m = new Uint8Array(wasmMemory.buffer); let f = ptr; while (m[f]) f++; return new TextDecoder().decode(m.subarray(ptr, f)); };
  const hits = [];
  for (let y = 0; y < c.height; y += 4) for (let x = 0; x < c.width; x += 4) {
    const j = JSON.parse(leer(wasmExports.cdda_ui_casilla_en_pixel(x, y)));
    if (j && j.dx === dx && j.dy === dy) hits.push([x, y]);
  }
  if (!hits.length) return null;
  const mx = hits.reduce((s, h) => s + h[0], 0) / hits.length, my = hits.reduce((s, h) => s + h[1], 0) / hits.length;
  return { x: rect.left + mx * rect.width / c.width, y: rect.top + my * rect.height / c.height };
}, [dx, dy]);

const errores = [];
const p = await b.newPage({ viewport: { width: 1280, height: 800 } });
p.on('pageerror', (e) => errores.push(e.message.slice(0, 300)));
await arrancar(p);

// 1. HUD y registro
const hud = await p.evaluate(() => ({
  hora: document.querySelector('#hud .hora').textContent, necesidades: document.querySelectorAll('#hud .necesidad').length,
  partes: document.querySelectorAll('#hud .parte').length, mano: document.querySelector('#hud .mano').textContent,
  mensajes: document.querySelectorAll('#registro .mensaje').length,
}));
comprobar('el HUD se pinta (hora, 7 necesidades, cuerpo, mano)', /\d/.test(hud.hora) && hud.necesidades === 7 && hud.partes >= 6 && hud.mano.includes('mano'), hud);
comprobar('el registro tiene mensajes', hud.mensajes > 0, { mensajes: hud.mensajes });
const h0 = hud.hora; await esperar(p, 6000);
const h1 = await p.evaluate(() => document.querySelector('#hud .hora').textContent);
comprobar('la hora del HUD avanza sola', h1 !== h0, { antes: h0, despues: h1 });
await p.screenshot({ path: `${FOTOS}/1-hud-y-mensajes.png` });

// 2. menú de una casilla (la de al lado) y hablar con un NPC
await p.evaluate(() => wasmExports.cdda_rt_poner_velocidad(1));
let px = await pixelDe(p, 1, 0);
comprobar('se encuentra en pantalla la casilla de al lado del jugador', !!px, px || {});
if (px) {
  await p.mouse.click(px.x, px.y);
  await esperar(p, 600);
  const menu = await p.evaluate(() => ({ visible: !document.getElementById('menu-casilla').classList.contains('oculto'), acciones: [...document.querySelectorAll('#menu-casilla button')].map((x) => x.textContent.trim()) }));
  comprobar('clic en una casilla: sale su menú con acciones', menu.visible && menu.acciones.length > 0, menu);
  await p.screenshot({ path: `${FOTOS}/2-menu-casilla.png` });
  await p.keyboard.press('Escape');
}
await p.evaluate(() => wasmExports.cdda_sim_npc_al_lado());
await esperar(p, 2500);
let npc = null;
for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [-1, -1], [1, -1], [-1, 1]]) {
  const cas = await p.evaluate(([dx, dy]) => { const m = new Uint8Array(wasmMemory.buffer); const ptr = wasmExports.cdda_ui_casilla(dx, dy); let f = ptr; while (m[f]) f++; return JSON.parse(new TextDecoder().decode(m.subarray(ptr, f))); }, [dx, dy]);
  if (cas && cas.criatura && cas.acciones.some((a) => a.id === 'hablar')) { npc = { dx, dy, nombre: cas.criatura }; break; }
}
comprobar('un NPC al lado tiene «Hablar» en su menú', !!npc, npc || {});
if (npc) {
  px = await pixelDe(p, npc.dx, npc.dy);
  await p.mouse.click(px.x, px.y);
  await esperar(p, 600);
  await p.screenshot({ path: `${FOTOS}/3-menu-npc.png` });
  const hablar = p.locator('#menu-casilla button', { hasText: 'Hablar' });
  await hablar.click();
  await esperar(p, 2500);
  const e = await estado(p);
  comprobar('«Hablar» abre el diálogo', e.ventanas > 1, e);
  const tapado = await p.evaluate(() => !document.getElementById('hud').classList.contains('oculto') || !document.getElementById('registro').classList.contains('oculto'));
  comprobar('con el diálogo abierto, el HUD y los mensajes se apartan', !tapado);
  await p.screenshot({ path: `${FOTOS}/4-dialogo.png` });
  // salir del diálogo: Escape (y si pregunta, la última respuesta)
  // (algunos NPC no tienen respuesta de despedida y Escape no cierra: entonces se contesta, y se confirma con «y»)
  const salir = ['Escape', 'Escape', 'Escape', 'd', 'y', 'c', 'y', 'b', 'y', 'a', 'y', 'Escape', 'Escape'];
  for (let i = 0; i < salir.length && (await estado(p)).ventanas > 1; i++) { await p.keyboard.press(salir[i]); await esperar(p, 800); }
  const t0 = (await estado(p)).turno; await esperar(p, 3000); const e2 = await estado(p);
  comprobar('después de hablar, el juego sigue (no se cuelga)', e2.ventanas <= 1 && e2.turno > t0, { ...e2, turnos: e2.turno - t0 });
}

// 3. panel con pestañas
await p.evaluate(() => wasmExports.cdda_rt_poner_velocidad(2));
await esperar(p, 1000);
await p.click('#boton-menu');
await esperar(p, 800);
const e3 = await estado(p);
comprobar('al abrir el panel el juego se pausa', e3.vel === 0, e3);
for (const pest of ['inventario', 'fabricar', 'construir', 'salud', 'personaje', 'mapa']) {
  await p.click(`#panel .pestanas button[data-p="${pest}"]`);
  await esperar(p, 1500);
  const filas = await p.evaluate(() => document.querySelectorAll('#panel .contenido > *, #panel .contenido .fila-item').length);
  comprobar(`la pestaña ${pest} se pinta`, filas > 0, { elementos: filas });
  await p.screenshot({ path: `${FOTOS}/5-panel-${pest}.png` });
}
await p.click('#panel .cerrar');
await esperar(p, 1500);
const e4 = await estado(p);
comprobar('al cerrar el panel sigue a su velocidad', e4.vel === 2, e4);

// 4. zona cargada: 100 zombis alrededor
await p.evaluate(() => wasmExports.cdda_sim_carga(100));
await esperar(p, 4000);
let a = await estado(p), t = Date.now(); await esperar(p, 8000); let z = await estado(p);
const tpsNormal = (z.turno - a.turno) / ((Date.now() - t) / 1000);
await p.screenshot({ path: `${FOTOS}/6-cien-zombis.png` });
// (lo que cuesta cada tic: cuántos podría hacer por segundo. La máxima no sirve para medirlo aquí: con zombis a la
// vista, el aviso de peligro la baja sola)
const msTic = await p.evaluate(() => wasmExports.cdda_rt_ms_turno());
r.cargada = { tpsNormal: +tpsNormal.toFixed(1), msPorTic: +msTic.toFixed(1), ticsPorSegundoPosibles: Math.round(1000 / msTic) };
comprobar('con 100 zombis alrededor, el día normal sigue a 24 tics por segundo', tpsNormal >= 24 * 0.9, r.cargada);
await p.evaluate(() => wasmExports.cdda_rt_poner_velocidad(2));
comprobar('sin errores en la página', errores.length === 0, { errores: errores.slice(0, 3) });
await p.close();

// 5. móvil
const m = await b.newPage({ viewport: { width: 390, height: 844 }, isMobile: true, hasTouch: true });
await arrancar(m);
await m.screenshot({ path: `${FOTOS}/7-movil.png` });
await m.click('#boton-menu');
await esperar(m, 1200);
await m.screenshot({ path: `${FOTOS}/8-movil-panel.png` });
const anchoPanel = await m.evaluate(() => document.getElementById('panel').getBoundingClientRect().width);
comprobar('en el móvil el panel ocupa la pantalla', anchoPanel >= 380, { anchoPanel });

await b.close();
console.log(JSON.stringify(r, null, 1));
process.exit(mal ? 1 : 0);
