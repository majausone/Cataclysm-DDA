// Cazafallos de la versión web: varias partidas a la vez, cada una en su pestaña (con su almacenamiento aparte), que
// juegan solas pulsando teclas al azar (andar, inventario, comer, hablar, fabricar, mirar... y lo que abran) a la
// máxima velocidad, y vigilan:
//  - CUELGUE: la página deja de contestar (un bucle sin fin en el juego) o el juego deja de mirar el teclado
//    (cdda_latidos no sube);
//  - ATASCO: el turno no avanza ni con 10 Escape (un menú que no se cierra);
//  - ERROR: un error de la página (el wasm se ha caído);
//  - AVISOS: avisos del juego (debugmsg: cdda_avisos), que se apuntan y se quitan con la barra espaciadora.
// De cada fallo, una foto y las teclas con su turno (para repetirlo). Con la misma semilla, las mismas teclas.
//   node tools/tiempo-real/cazafallos-web.mjs [--url http://localhost:8095/] [--sesiones 3] [--minutos 5]
//        [--semilla 1] [--velocidad 6] [--salida carpeta] [--teclas hablar] [--npc]
// (--teclas hablar: casi todo, ir hacia un lado y hablar; --npc: un superviviente al lado del jugador cada 30 s;
// las dos juntas, para buscar fallos en el diálogo)
// Devuelve 0 si ninguna partida ha fallado.
import { mkdirSync, writeFileSync } from 'node:fs';
const { chromium } = await import('playwright');
const arg = (n, d) => { const i = process.argv.indexOf('--' + n); return i > 0 ? process.argv[i + 1] : d; };
const URL = arg('url', 'http://localhost:8095/'), SESIONES = +arg('sesiones', 3), MINUTOS = +arg('minutos', 5);
const SEMILLA = +arg('semilla', 1), VELOCIDAD = +arg('velocidad', 6), SALIDA = arg('salida', 'cazafallos'), TECLAS = arg('teclas', 'todas');
const NPC = process.argv.includes('--npc');
mkdirSync(SALIDA, { recursive: true });

// azar con semilla (mulberry32)
const azar = (s) => () => { s |= 0; s = (s + 0x6d2b79f5) | 0; let t = Math.imul(s ^ (s >>> 15), 1 | s); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
const TODAS = [
  ['ArrowUp', 8], ['ArrowDown', 8], ['ArrowLeft', 8], ['ArrowRight', 8], ['y', 3], ['u', 3], ['b', 3], ['n', 3], ['.', 3],
  ['i', 2], ['E', 2], ['W', 1], ['g', 2], ['C', 2], ['&', 1], ['x', 1], ['e', 2], ['o', 1], ['c', 1], ['w', 1], ['@', 1],
  ['m', 1], ['a', 2], ['1', 2], ['2', 1], ['Enter', 4], ['Escape', 5],
];
const HABLAR = [['ArrowUp', 3], ['ArrowDown', 3], ['ArrowLeft', 3], ['ArrowRight', 3], ['C', 6], ['Enter', 3], ['1', 2], ['a', 2], ['Escape', 1]];
const LISTA = TECLAS === 'hablar' ? HABLAR : TODAS;
const total = LISTA.reduce((s, [, p]) => s + p, 0);
const elegir = (r) => { let x = r() * total; for (const [k, p] of LISTA) { if ((x -= p) < 0) return k; } return LISTA[0][0]; };
const conTiempo = (pr, ms) => Promise.race([pr, new Promise((_, no) => setTimeout(() => no(new Error('sin respuesta')), ms))]);
const esperar = (ms) => new Promise((r) => setTimeout(r, ms));

const b = await chromium.launch({ headless: true, args: ['--enable-features=WebAssemblyExperimentalJSPI', '--use-angle=d3d11', '--enable-gpu', '--ignore-gpu-blocklist'] });

async function sesion(n) {
  const semilla = SEMILLA + n, r = azar(semilla), nombre = `partida-${semilla}`;
  const ctx = await b.newContext({ viewport: { width: 1280, height: 800 } });
  const p = await ctx.newPage();
  const errores = [], teclas = [];
  p.on('pageerror', (e) => errores.push(e.message.slice(0, 300)));
  p.on('console', (m) => { if (m.type() === 'error' && /abort|RuntimeError|unreachable|memory access/i.test(m.text())) errores.push(m.text().slice(0, 300)); });
  const estado = () => conTiempo(p.evaluate(() => {
    const e = window.wasmExports;
    if (!e || !e.cdda_turno) return null;
    let aviso = '';
    if (e.cdda_ultimo_aviso) { const ptr = e.cdda_ultimo_aviso(), m = new Uint8Array(window.wasmMemory.buffer); let fin = ptr; while (m[fin]) fin++; aviso = new TextDecoder().decode(m.subarray(ptr, fin)); }
    return { turno: e.cdda_turno(), latidos: e.cdda_latidos ? e.cdda_latidos() : -1, ventanas: e.cdda_ventanas(), avisos: e.cdda_avisos ? e.cdda_avisos() : 0, aviso };
  }), 8000);
  const res = { nombre, semilla, resultado: 'BIEN', turnos: 0, teclas: 0, avisos: [] };
  const fallo = async (tipo, detalle) => {
    res.resultado = tipo; res.detalle = detalle;
    await conTiempo(p.screenshot({ path: `${SALIDA}/${nombre}-${tipo}.png` }), 10000).catch(() => {});
  };
  try {
    await p.goto(URL);
    await p.waitForFunction(() => window.wasmExports && window.wasmExports.cdda_turno, null, { timeout: 240000 });
    await esperar(20000); await p.keyboard.press('Enter'); await esperar(15000); await p.keyboard.press('d');
    let antes = null;
    const t0 = Date.now();
    for (;;) {
      await esperar(3000);
      // (mientras arranca, la página puede tardar en contestar: no es un fallo)
      const e = await estado().catch(() => null);
      if (antes && e && e.turno > antes.turno) break;
      antes = e;
      if (Date.now() - t0 > 240000) throw new Error('no empieza la partida');
    }
    await p.evaluate((v) => window.wasmExports.cdda_rt_poner_velocidad(v), VELOCIDAD);
    const inicio = await conTiempo(estado(), 30000).catch(() => null) || antes;
    let ult = inicio, tTurno = Date.now(), tLatido = Date.now(), tEstado = 0, escapes = 0, sinRespuesta = 0, tNpc = 0;
    const fin = Date.now() + MINUTOS * 60000;
    while (Date.now() < fin) {
      if (errores.length) { await fallo('ERROR', errores[0]); break; }
      if (NPC && Date.now() - tNpc > 30000) {
        tNpc = Date.now();
        teclas.push(`${ult.turno} (NPC al lado)`);
        await conTiempo(p.evaluate(() => window.wasmExports.cdda_sim_npc_al_lado()), 8000).catch(() => {});
      }
      const k = elegir(r);
      teclas.push(`${ult.turno} ${k}`);
      await conTiempo(p.keyboard.press(k), 8000).catch(() => {});
      await esperar(150);
      if (Date.now() - tEstado < 1000) continue;
      tEstado = Date.now();
      let e;
      try { e = await estado(); sinRespuesta = 0; } catch { if (++sinRespuesta >= 2) { await fallo('CUELGUE', 'la página no contesta en 16 s'); break; } continue; }
      if (!e) continue;
      // (la velocidad: el peligro la baja; se vuelve a poner)
      await p.evaluate((v) => window.wasmExports.cdda_rt_poner_velocidad(v), VELOCIDAD).catch(() => {});
      // (los avisos del juego: cada uno es un fallo; con el aviso en pantalla el juego espera la barra espaciadora)
      if (e.avisos > (ult.avisos || 0)) { res.avisos.push(`turno ${e.turno}: ${e.aviso}`); teclas.push(`${e.turno} AVISO ${e.aviso}`); await p.keyboard.press(' ').catch(() => {}); }
      if (e.latidos !== ult.latidos) tLatido = Date.now();
      if (e.turno !== ult.turno) { tTurno = Date.now(); escapes = 0; }
      ult = e;
      if (e.latidos >= 0 && Date.now() - tLatido > 20000) { await fallo('CUELGUE', `20 s sin mirar el teclado, turno ${e.turno}, ventanas ${e.ventanas}`); break; }
      if (Date.now() - tTurno > 10000 + 2000 * escapes) {
        if (escapes >= 10) { await fallo('ATASCO', `el turno no avanza ni con 10 Escape, turno ${e.turno}, ventanas ${e.ventanas}`); break; }
        escapes++;
        teclas.push(`${e.turno} Escape (no avanza)`);
        await p.keyboard.press('Escape').catch(() => {});
      }
    }
    res.turnos = ult.turno - inicio.turno;
    if (res.resultado === 'BIEN' && res.avisos.length) res.resultado = 'AVISOS';
  } catch (err) {
    if (res.resultado === 'BIEN') await fallo('ERROR', err.message.slice(0, 300));
  }
  res.teclas = teclas.length;
  res.ultimasTeclas = teclas.slice(-20);
  writeFileSync(`${SALIDA}/${nombre}.txt`, teclas.join('\n') + '\n');
  console.log(res.resultado === 'BIEN' ? 'OK ' : 'MAL', JSON.stringify(res));
  await ctx.close().catch(() => {});
  return res;
}

const resultados = await Promise.all(Array.from({ length: SESIONES }, (_, i) => sesion(i)));
await b.close().catch(() => {});
writeFileSync(`${SALIDA}/resumen.json`, JSON.stringify(resultados, null, 1));
const malas = resultados.filter((x) => x.resultado !== 'BIEN');
console.log(`${resultados.length - malas.length} de ${resultados.length} partidas sin fallos`);
process.exit(malas.length ? 1 : 0);
