// Jugar como una persona (Encargo 8, F), con Playwright, haciendo fotos de cada paso para mirarlas:
//   node tools/tiempo-real/prueba-jugar.mjs [--url http://localhost:8095/] [--fotos carpeta] [--movil]
// Crea un personaje desde nuestra pantalla de inicio y anda; mira el inventario y suelta y coge algo; fabrica algo;
// mira construir, salud, personaje, el mapa y los mensajes; habla con un NPC; cambia de idioma; guarda, muere y carga
// la partida. Apunta lo que falla (sin pararse) y devuelve 1 si algo ha fallado.
import { mkdirSync } from 'node:fs';
const { chromium, devices } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8096/'), FOTOS = arg('fotos', 'fotos-jugar'), MOVIL = process.argv.includes('--movil');
mkdirSync(FOTOS, { recursive: true });
const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });
const ctx = await b.newContext(MOVIL ? { ...devices['Pixel 7'] } : { viewport: { width: 1366, height: 820 } });
const p = await ctx.newPage();
const errores = [];
p.on('pageerror', (e) => errores.push(e.message.slice(0, 300)));
let paso = 0, mal = 0;
const foto = async (n) => { await p.screenshot({ path: `${FOTOS}/${String(++paso).padStart(2, '0')}-${n}.png` }); };
// (mientras espera, si el juego pregunta algo en nuestra ventana, se contesta como una persona: la primera opción,
// con su foto la primera vez que sale cada pregunta)
const preguntasVistas = new Set();
async function contestar() {
  const l = await p.evaluate(() => (window.interfazCdda ? window.interfazCdda.json('cdda_ui_lista') : null)).catch(() => null);
  if (!l || !(await p.locator('#ventana-lista:not(.oculto)').count())) return;
  const clave = l.titulo || l.texto || '';
  if (!preguntasVistas.has(clave)) { preguntasVistas.add(clave); await p.screenshot({ path: `${FOTOS}/${String(++paso).padStart(2, '0')}-pregunta.png` }); console.log('PREGUNTA', JSON.stringify(clave), '->', JSON.stringify(l.opciones[0] && l.opciones[0].texto)); }
  await p.locator('#ventana-lista button:not(.cerrar):not([disabled])').first().click().catch(() => {});
}
const esperar = async (ms) => { const t0 = Date.now(); while (Date.now() - t0 < ms) { await p.waitForTimeout(Math.min(400, ms - (Date.now() - t0))); await contestar(); } };
const comprobar = (n, ok, info = {}) => { if (!ok) mal++; console.log(ok ? 'OK ' : 'MAL', n, JSON.stringify(info)); };
const estado = () => p.evaluate(() => { try { const e = window.interfazCdda.json('cdda_ui_estado'); return e && { hora: e.hora, pos: e.pos, enCamino: e.enCamino, actividad: e.actividad && e.actividad.id, ventanas: wasmExports.cdda_ventanas(), turno: wasmExports.cdda_turno(), avisos: wasmExports.cdda_avisos ? wasmExports.cdda_avisos() : 0 }; } catch { return null; } });
const clic = async (sel) => { const l = p.locator(sel).first(); await l.click({ timeout: 5000 }); };
const intentar = async (n, f) => {
  try { await f(); } catch (e) {
    comprobar(n, false, { error: e.message.split('\n').filter((l) => /intercepts|not visible|not enabled|Timeout|waiting for|resolved/.test(l)).slice(0, 6).join(' | ') });
    await foto('fallo-' + n.replace(/[^a-z]+/gi, '-'));
  }
};

await p.goto(URL);
await p.waitForSelector('#inicio:not(.oculto)', { timeout: 240000 });
await esperar(1500);
await foto('inicio');
comprobar('sale nuestra pantalla de inicio', true);

// 1. crear un personaje
await intentar('crear personaje', async () => {
  await clic('#inicio .nueva');
  await esperar(500);
  await p.fill('#inicio input.nombre', 'Ana Prueba');
  await clic('#inicio .m');
  await foto('nueva-partida');
  await clic('#inicio .empezar');
  await p.waitForSelector('#hud:not(.oculto)', { timeout: 240000 });
  await esperar(3000);
  await foto('partida');
  const e = await estado();
  comprobar('la partida empieza', !!e, e || {});
});

// 2. andar (manteniendo flechas): se mueve al pulsar, a 60 imágenes por segundo, y gira a mitad de paso
const imagenes = () => p.evaluate(() => wasmExports.cdda_imagenes ? wasmExports.cdda_imagenes() : 0);
const lado = (q) => Math.max(Math.abs(q[0]), Math.abs(q[1]));
await intentar('andar', async () => {
  const a = await estado(), i0 = await imagenes(), t0 = Date.now();
  await p.keyboard.down('ArrowRight');
  await esperar(150);
  const tras = await estado();
  comprobar('al pulsar, el paso empieza enseguida (antes de 150 ms)', tras.pos[0] !== a.pos[0] || tras.pos[1] !== a.pos[1], { antes: a.pos, despues: tras.pos });
  await esperar(2350);
  const fps = ((await imagenes()) - i0) / ((Date.now() - t0) / 1000);
  await foto('andando');
  await p.keyboard.up('ArrowRight');
  const z = await estado();
  comprobar('manteniendo la flecha anda seguido (2-3 casillas en 2,5 s)', z.pos[0] - a.pos[0] >= 2, { casillas: z.pos[0] - a.pos[0] });
  const msImagen = await p.evaluate(() => wasmExports.cdda_rt_ms_imagen ? wasmExports.cdda_rt_ms_imagen() : -1);
  const raf = await p.evaluate(() => new Promise((ok) => { let n = 0; const t0 = performance.now(); const f = () => { n++; if (performance.now() - t0 < 1000) requestAnimationFrame(f); else ok(n); }; requestAnimationFrame(f); }));
  comprobar('andando se pinta a unas 60 imágenes por segundo', fps >= 50, { imagenesPorSegundo: +fps.toFixed(1), msPorImagen: +msImagen.toFixed(1), fotogramasDelNavegador: raf });
  comprobar('el tiempo sigue andando', z.turno > a.turno, { turnos: z.turno - a.turno });
  // girar a mitad de paso: derecha y, enseguida, abajo; se ve abajo al momento (no al acabar el paso)
  await esperar(1500);
  const g0 = await estado();
  await p.keyboard.down('ArrowRight'); await esperar(120);
  const g1 = await estado();
  await p.keyboard.up('ArrowRight'); await p.keyboard.down('ArrowDown'); await esperar(250);
  const g2 = await estado();
  await p.keyboard.up('ArrowDown');
  await foto('girando');
  const d1 = [g1.pos[0] - g0.pos[0], g1.pos[1] - g0.pos[1]], d2 = [g2.pos[0] - g0.pos[0], g2.pos[1] - g0.pos[1]];
  comprobar('si cambia de dirección a mitad de paso, gira al momento', d1[0] === 1 && d2[0] === 0 && d2[1] === 1, { primero: d1, alGirar: d2 });
});

// 2b. ir a una casilla con un clic: anda todo el camino sin pararse
await intentar('ir con un clic', async () => {
  await esperar(1500);
  const destino = await p.evaluate(() => {
    const leer = (ptr) => { const m = new Uint8Array(wasmMemory.buffer); let f = ptr; while (m[f]) f++; return JSON.parse(new TextDecoder().decode(m.subarray(ptr, f))); };
    for (let r = 5; r >= 3; r--) for (let dx = -r; dx <= r; dx++) for (let dy = -r; dy <= r; dy++) {
      if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
      const c = leer(wasmExports.cdda_ui_casilla(dx, dy));
      if (c && c.acciones.some((a) => a.id === 'ir')) return { dx, dy };
    }
    return null;
  });
  if (!destino) { comprobar('hay una casilla libre a 3-5 pasos para ir', false); return; }
  const a = await estado();
  await p.evaluate(({ dx, dy }) => window.interfazCdda.ordenar({ a: 'ir', dx, dy }), destino);
  let ultimo = a.pos, tUltimo = Date.now(), maxQuieto = 0, llegado = false;
  const t0 = Date.now();
  while (Date.now() - t0 < 9000) {
    await esperar(200);
    const e = await estado();
    if (e.pos[0] !== ultimo[0] || e.pos[1] !== ultimo[1]) { maxQuieto = Math.max(maxQuieto, Date.now() - tUltimo); ultimo = e.pos; tUltimo = Date.now(); }
    if (e.pos[0] === a.pos[0] + destino.dx && e.pos[1] === a.pos[1] + destino.dy) { llegado = true; break; }
  }
  await foto('ido');
  comprobar('con un clic va hasta la casilla', llegado, { destino, desde: a.pos, hasta: ultimo });
  comprobar('y por el camino no se para (ningún hueco de más de 2 s entre pasos; en diagonal un paso cuesta 1,4)', maxQuieto <= 2000, { msMasLargoQuieto: maxQuieto });
});

// 3. el inventario: el muñequito, una tarjeta, soltar algo y cogerlo del suelo
await intentar('inventario', async () => {
  await p.keyboard.press('i');
  await esperar(1500);
  await foto('inventario');
  const huecos = await p.locator('#panel .hueco .marco-icono:not(.vacio)').count();
  comprobar('el muñequito tiene cosas puestas', huecos > 0, { huecos });
  const iconos = await p.evaluate(() => [...document.querySelectorAll('#panel .icono')].filter((c) => !c.dataset.sin).length);
  comprobar('salen iconos (sprites del juego)', iconos > 0, { iconos });
  // (algo de los bolsillos; si no lleva nada en ellos, algo de lo puesto)
  let enBolsa = p.locator('#panel .casilla-obj').first();
  if (!(await enBolsa.count())) enBolsa = p.locator('#panel .hueco .marco-icono:not(.vacio)').first();
  if (await enBolsa.count()) {
    await enBolsa.click();
    await esperar(600);
    await foto('tarjeta-objeto');
    const soltar = p.locator('#ventana-objeto .pie button', { hasText: /Soltar|Drop/ });
    if (await soltar.count()) { await soltar.click(); await esperar(2500); }
  }
  await p.keyboard.press('Escape');
  await esperar(500);
  await p.keyboard.press('g');
  await esperar(1000);
  await foto('coger-del-suelo');
  const enSuelo = await p.locator('#ventana-coger .casilla-coger').count();
  comprobar('lo soltado está en el suelo para cogerlo', enSuelo > 0, { enSuelo });
  if (enSuelo) { await clic('#ventana-coger .pie .principal'); await esperar(2500); }
  await foto('cogido');
});

// 4. fabricar algo
await intentar('fabricar', async () => {
  await p.keyboard.press('&');
  await esperar(2000);
  await foto('fabricar');
  const primera = p.locator('#panel .columna.lista .fila-icono:not(.no-puede)').first();
  if (await primera.count()) {
    await primera.click();
    await esperar(1000);
    await foto('receta');
    const boton = p.locator('#panel .columna.detalle .boton.principal');
    if (await boton.isEnabled()) {
      const antes = await p.evaluate(() => JSON.stringify(window.interfazCdda.json('cdda_ui_inventario')).length);
      await boton.click();
      await esperar(1200);
      await foto('fabricando');
      const e = await estado();
      const barra = await p.locator('#actividad:not(.oculto)').count();
      const despues = await p.evaluate(() => JSON.stringify(window.interfazCdda.json('cdda_ui_inventario')).length);
      // (lo corto se acaba en un par de segundos: vale ver la barra o que ya haya salido)
      comprobar('al fabricar sale la barra (o ya está hecho)', (e && e.actividad && barra > 0) || despues !== antes, { actividad: e && e.actividad, barra });
    }
  } else {
    const otra = p.locator('#panel .columna.lista .fila-icono').first();
    if (await otra.count()) { await otra.click(); await esperar(1000); await foto('receta-no-se-puede'); }
  }
  await p.keyboard.press('Escape');
});

// 5. las demás pestañas
for (const [tecla, nombre] of [['*', 'construir'], ['m', 'mapa'], ['@', 'personaje']]) {
  await intentar(nombre, async () => { await p.keyboard.press(tecla); await esperar(1500); await foto(nombre); await p.keyboard.press('Escape'); await esperar(300); });
}
await intentar('salud y mensajes', async () => {
  await p.keyboard.press('Tab'); await esperar(800);
  await clic('#panel .pestanas button[data-p="salud"]'); await esperar(800); await foto('salud');
  await clic('#panel .pestanas button[data-p="mensajes"]'); await esperar(800); await foto('mensajes');
  await p.keyboard.press('Escape');
});

// 6. hablar con un NPC (sin parar el mundo)
await intentar('hablar', async () => {
  await p.evaluate(() => wasmExports.cdda_sim_npc_al_lado());
  await esperar(2500);
  await p.keyboard.press('C');
  await esperar(2000);
  await foto('dialogo');
  const visible = await p.locator('#dialogo:not(.oculto)').count();
  comprobar('el diálogo sale en nuestra ventana', visible > 0);
  const a = await estado(); await esperar(2000); const z = await estado();
  comprobar('mientras se habla, el mundo sigue', z && a && z.turno > a.turno, { turnos: z && a ? z.turno - a.turno : null });
  const r = p.locator('#dialogo .respuestas button:not([disabled])').first();
  if (await r.count()) { await r.click(); await esperar(1500); await foto('dialogo-respuesta'); }
  if (await p.locator('#dialogo:not(.oculto)').count()) await clic('#dialogo .cerrar');
  await esperar(800);
});

// 7. idioma
await intentar('idioma', async () => {
  await p.keyboard.press('Escape'); await esperar(500);
  await clic('#ventana-opciones .en'); await esperar(2500);
  await p.keyboard.press('Escape'); await esperar(300);
  await p.keyboard.press('i'); await esperar(1500);
  await foto('en-ingles');
  await p.keyboard.press('Escape'); await esperar(300);
  await p.keyboard.press('Escape'); await esperar(500);
  await clic('#ventana-opciones .es'); await esperar(2500);
  await p.keyboard.press('Escape');
});

// 8. guardar y salir, cargar la partida, y morir (al morir, la partida va al cementerio: por eso se carga antes)
await intentar('guardar, salir y cargar', async () => {
  await p.keyboard.press('Escape'); await esperar(500);
  await clic('#ventana-opciones .salir');
  await p.waitForSelector('#inicio:not(.oculto)', { timeout: 120000 });
  await esperar(1500);
  await clic('#inicio .cargar'); await esperar(1000);
  await foto('cargar');
  const partidas = await p.locator('#inicio .partida').count();
  comprobar('hay partidas para cargar', partidas > 0, { partidas });
  if (partidas) {
    await clic('#inicio .partida');
    await p.waitForSelector('#hud:not(.oculto)', { timeout: 240000 });
    await esperar(3000);
    await foto('cargada');
    const e = await estado();
    comprobar('la partida se carga y el reloj sigue', !!e && e.turno > 0, e || {});
  }
});
await intentar('morir', async () => {
  await p.evaluate(() => wasmExports.cdda_sim_morir());
  // (si el juego pregunta algo al morir, se contesta lo último, «No», en nuestra ventana)
  let preguntas = 0;
  for (let t0 = Date.now(); Date.now() - t0 < 120000 && !(await p.locator('#inicio:not(.oculto)').count());) {
    if (await p.locator('#ventana-lista:not(.oculto)').count()) {
      if (!preguntas++) await foto('pregunta-al-morir');
      await p.locator('#ventana-lista button:not(.cerrar):not([disabled])').last().click();
    }
    await esperar(700);
  }
  await p.waitForSelector('#inicio:not(.oculto)', { timeout: 5000 });
  await esperar(1500);
  await foto('muerte');
  comprobar('sale nuestra pantalla de muerte', await p.locator('#inicio .muerte').count() > 0);
});

const e = await estado();
comprobar('sin avisos de error del juego', !e || !e.avisos, { avisos: e && e.avisos });
comprobar('sin errores de la página', errores.length === 0, { errores: errores.slice(0, 3) });
await b.close();
process.exit(mal ? 1 : 0);
