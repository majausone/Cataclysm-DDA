// La interfaz de la versión web (Encargo 7, B). Lee el estado del juego por el enchufe (cdda_ui_*, src/interfaz.cpp)
// y le manda órdenes. Pinta:
//  - el HUD: hora y día, tiempo, velocidad, necesidades con iconos y barras, salud por partes, lo que hay en la mano;
//  - el registro de mensajes, por tipos, con iconos y colores, recogible;
//  - un botón que abre un panel con pestañas: Inventario, Fabricar, Construir, Salud, Personaje, Mapa (mientras está
//    abierto, el juego está en pausa, como con los menús del juego);
//  - y un menú al hacer clic en una casilla del mapa, con solo las acciones que tienen sentido ahí.
(() => {
  'use strict';
  const $ = (sel, raiz = document) => raiz.querySelector(sel);
  const crear = (html) => { const t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; };
  // (sin las etiquetas de color del juego, que a veces vienen en los nombres)
  const esc = (s) => String(s ?? '').replace(/<\/?color[^>]*>/g, '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  // --- el enchufe
  const ex = () => window.wasmExports;
  const listo = () => !!(ex() && ex().cdda_ui_estado && window.wasmMemory);
  const leer = (ptr) => {
    const m = new Uint8Array(window.wasmMemory.buffer);
    let fin = ptr; while (m[fin]) fin++;
    return new TextDecoder().decode(m.subarray(ptr, fin));
  };
  const json = (f, ...args) => { try { return JSON.parse(leer(ex()[f](...args))); } catch { return null; } };
  const ordenar = (o) => {
    const datos = new TextEncoder().encode(JSON.stringify(o));
    const ptr = ex().cdda_ui_bufer(), tam = ex().cdda_ui_bufer_tam();
    if (datos.length > tam) return;
    new Uint8Array(window.wasmMemory.buffer).set(datos, ptr);
    ex().cdda_ui_orden(datos.length);
  };
  const ventanas = () => (ex() && ex().cdda_ventanas ? ex().cdda_ventanas() : 1);
  const ponerVelocidad = (v) => ex() && ex().cdda_rt_poner_velocidad && ex().cdda_rt_poner_velocidad(v);

  // --- colores del juego a colores de verdad
  const COLORES = {
    red: '#e05a5a', light_red: '#ff8a80', green: '#5cb85c', light_green: '#9be07c', yellow: '#e8d44d',
    light_blue: '#79b8ff', blue: '#4a7fd8', white: '#ececec', light_gray: '#c4c4c4', dark_gray: '#7a7a7a',
    magenta: '#d36fd3', pink: '#ff99d6', cyan: '#4dd0e1', light_cyan: '#a6f3ff', brown: '#b5823f', black: '#555',
  };
  const color = (c) => COLORES[String(c || '').replace(/^[a-z]_/, '')] || '#c4c4c4';

  // --- HUD
  const ICONOS = { hambre: 'fa-utensils', sed: 'fa-droplet', sueno: 'fa-bed', temperatura: 'fa-temperature-half', animo: 'fa-face-smile', aguante: 'fa-person-running', dolor: 'fa-bandage' };
  // (lo que elige el jugador: la pausa y cuánto dura un día; nada más)
  const VELOCIDADES = [['fa-pause', 'Pausa'], ['', 'Día 2 h'], ['', 'Día 1 h'], ['', 'Día 30 min']];
  const PARTES = { head: 'Cabeza', torso: 'Torso', arm_l: 'Brazo izq.', arm_r: 'Brazo der.', leg_l: 'Pierna izq.', leg_r: 'Pierna der.', hand_l: 'Mano izq.', hand_r: 'Mano der.', foot_l: 'Pie izq.', foot_r: 'Pie der.', eyes: 'Ojos', mouth: 'Boca' };
  const ORDEN_PARTES = ['head', 'torso', 'arm_l', 'arm_r', 'leg_l', 'leg_r'];
  const ESTACIONES = { Spring: 'primavera', Summer: 'verano', Autumn: 'otoño', Winter: 'invierno' };
  const TIEMPOS = { Sunny: 'Soleado', Clear: 'Despejado', Cloudy: 'Nublado', Overcast: 'Cubierto', Drizzle: 'Llovizna', Rain: 'Lluvia', 'Rain Storm': 'Tormenta', Thunderstorm: 'Tormenta eléctrica', Lightning: 'Rayos', Flurries: 'Copos de nieve', Snowing: 'Nieva', Snowstorm: 'Ventisca', Mist: 'Neblina', Fog: 'Niebla', Portal: 'Portal', 'Acid Drizzle': 'Llovizna ácida', 'Acid Rain': 'Lluvia ácida' };
  const BIEN = { hambre: 'Saciado', sed: 'Bien', sueno: 'Descansado', dolor: 'Sin dolor', animo: 'Normal' };
  const hud = crear(`<div id="hud" class="ui">
      <div class="fila"><span class="hora">--:--</span><span class="fecha"></span>
        <button class="plegar" title="Plegar"><i class="fa-solid fa-chevron-up"></i></button></div>
      <div class="clima"></div>
      <div class="velocidades plegable"></div>
      <div class="necesidades plegable"></div>
      <div class="cuerpo plegable"></div>
      <div class="mano plegable"></div>
    </div>`);
  VELOCIDADES.forEach(([ico, txt], i) => {
    const b = crear(`<button title="${txt}">${ico ? `<i class="fa-solid ${ico}"></i>` : txt.replace('Día ', '')}</button>`);
    b.onclick = () => { ponerVelocidad(i); pausadoPorPanel = false; };
    $('.velocidades', hud).appendChild(b);
  });
  $('.plegar', hud).onclick = () => hud.classList.toggle('plegado');

  function pintarHud(e) {
    $('.hora', hud).textContent = e.hora.replace(/:\d\d(\s?[AP]M)$/, '$1');
    $('.fecha', hud).textContent = `Día ${e.dia} de ${ESTACIONES[e.estacion] || e.estacion}`;
    $('.clima', hud).innerHTML = `<i class="fa-solid ${e.exterior ? 'fa-cloud-sun' : 'fa-house'}"></i> <span style="color:${color(e.tiempoColor)}">${esc(TIEMPOS[e.tiempo] || e.tiempo)}</span> · ${Math.round(e.temperaturaC)} °C${e.exterior ? '' : ' fuera'}`;
    [...$('.velocidades', hud).children].forEach((b, i) => b.classList.toggle('activa', i === e.vel));
    $('.necesidades', hud).innerHTML = e.necesidades.map((n) => `
      <i class="fa-solid ${ICONOS[n.id] || 'fa-circle'}" title="${esc(n.nombre)}"></i>
      <div class="necesidad"><div class="rotulo"><span>${esc(n.nombre)}</span><span style="color:${color(n.color)}">${esc(textoNecesidad(n))}</span></div>
      <div class="barra"><div style="width:${Math.round(n.barra * 100)}%;background:${color(n.color)}"></div></div></div>`).join('');
    $('.cuerpo', hud).innerHTML = ordenarPartes(e.cuerpo).map((p) => {
      const f = p.max > 0 ? p.vida / p.max : 0;
      const c = f > 0.75 ? '#5cb85c' : f > 0.4 ? '#e8d44d' : '#e05a5a';
      const marcas = `${p.sangra ? '<i class="fa-solid fa-droplet" title="Sangra"></i>' : ''}${p.rota ? ' <i class="fa-solid fa-bone" title="Rota"></i>' : ''}`;
      return `<div class="parte"><div class="rotulo"><span>${esc(PARTES[p.id] || p.nombre)}</span><span class="marcas">${marcas}</span></div>
        <div class="barra"><div style="width:${Math.round(f * 100)}%;background:${c}"></div></div></div>`;
    }).join('');
    $('.mano', hud).innerHTML = `<i class="fa-solid fa-hand"></i> En la mano: <b>${esc(e.enMano)}</b>`;
  }

  // (el texto de una necesidad: el del juego, o si no dice nada porque va bien, «bien»)
  function textoNecesidad(n) {
    const t = String(n.texto || '').trim();
    if (n.id === 'animo') return { ':D': 'Feliz', ':)': 'Contento', ':|': 'Normal', ':(': 'Triste', 'D:': 'Muy mal' }[t] || t || BIEN.animo;
    return t || BIEN[n.id] || '';
  }
  const ordenarPartes = (ps) => [...ps].sort((a, b) => (ORDEN_PARTES.indexOf(a.id) + 99) % 99 - (ORDEN_PARTES.indexOf(b.id) + 99) % 99);

  // --- registro de mensajes
  const TIPOS = { 0: ['t-bien', 'fa-circle-check'], 1: ['t-mal', 'fa-circle-exclamation'], 2: ['t-mixto', 'fa-circle-half-stroke'], 3: ['t-aviso', 'fa-triangle-exclamation'], 4: ['t-info', 'fa-circle-info'], 5: ['t-normal', 'fa-circle'], 7: ['t-mal', 'fa-crosshairs'], 8: ['t-mal', 'fa-burst'], 9: ['t-normal', 'fa-crosshairs'] };
  const GRUPOS = {
    combate: /\b(hit|hits|miss|misses|attack|bite|bites|punch|kick|slash|stab|shoot|shot|dodge|block|kill|dies|died|wound|damage|zombie|claw|swing|grab)/i,
    salud: /\b(pain|bleed|bleeding|hungry|thirsty|tired|sleep|wake|feel|hurt|bandage|sick|nause|eat|drink|warm|cold|heal)/i,
    ambiente: /\b(hear|sound|rain|weather|sun|night|wind|snow|thunder|light|dark|smell)/i,
  };
  const grupo = (t) => Object.keys(GRUPOS).find((g) => GRUPOS[g].test(t)) || 'general';
  const registro = crear(`<div id="registro" class="ui">
      <div class="cabecera"><b>Mensajes</b>
        <span class="chip activa" data-g="todo">Todo</span><span class="chip" data-g="combate">Combate</span>
        <span class="chip" data-g="salud">Salud</span><span class="chip" data-g="ambiente">Ambiente</span>
        <button class="plegar" title="Recoger"><i class="fa-solid fa-chevron-down"></i></button></div>
      <div class="lista"></div></div>`);
  let filtro = 'todo', totalVisto = -1, mensajes = [];
  registro.querySelectorAll('.chip').forEach((c) => c.onclick = () => {
    filtro = c.dataset.g; registro.querySelectorAll('.chip').forEach((x) => x.classList.toggle('activa', x === c)); pintarMensajes(false);
  });
  $('.plegar', registro).onclick = () => registro.classList.toggle('plegado');
  function pintarMensajes(nuevos) {
    const lista = $('.lista', registro);
    const vis = mensajes.filter((m) => filtro === 'todo' || m.grupo === filtro).slice(-60);
    lista.innerHTML = vis.map((m, i) => {
      const [cls, ico] = TIPOS[m.tipo] || TIPOS[5];
      return `<div class="mensaje ${cls} ${nuevos && i >= vis.length - nuevos ? 'nuevo' : ''}"><i class="fa-solid ${ico}"></i><span>${esc(m.texto)}</span><span class="h">${esc(m.hora.replace(/:\d\d(\s?[AP]M)$/, '$1'))}</span></div>`;
    }).join('') || '<div class="vacio">Sin mensajes.</div>';
    lista.scrollTop = lista.scrollHeight;
  }
  function actualizarMensajes() {
    const r = json('cdda_ui_mensajes', 80);
    if (!r) return;
    if (r.total === totalVisto && mensajes.length) return;
    const antes = mensajes.length;
    mensajes = r.mensajes.map((m) => ({ ...m, grupo: grupo(m.texto) }));
    totalVisto = r.total;
    pintarMensajes(Math.max(0, Math.min(5, mensajes.length - antes)));
  }

  // --- panel con pestañas
  const PESTANAS = [['inventario', 'Inventario', 'fa-briefcase'], ['fabricar', 'Fabricar', 'fa-hammer'], ['construir', 'Construir', 'fa-trowel-bricks'], ['salud', 'Salud', 'fa-heart-pulse'], ['personaje', 'Personaje', 'fa-user'], ['mapa', 'Mapa', 'fa-map']];
  const boton = crear(`<button id="boton-menu" class="ui" title="Menú (Tab)"><i class="fa-solid fa-bars"></i></button>`);
  const panel = crear(`<div id="panel" class="ui oculto">
      <div class="pestanas">${PESTANAS.map(([id, n, i]) => `<button data-p="${id}"><i class="fa-solid ${i}"></i> ${n}</button>`).join('')}
        <button class="cerrar" title="Cerrar (Esc)"><i class="fa-solid fa-xmark"></i></button></div>
      <div class="pausado"><i class="fa-solid fa-pause"></i> El juego está en pausa mientras miras el menú.</div>
      <div class="contenido"></div></div>`);
  let pestana = 'inventario', velAntes = 2, pausadoPorPanel = false, ultimoEstado = null;
  panel.querySelectorAll('.pestanas button[data-p]').forEach((b) => b.onclick = () => abrirPestana(b.dataset.p));
  $('.cerrar', panel).onclick = () => cerrarPanel();
  boton.onclick = () => (panel.classList.contains('oculto') ? abrirPanel() : cerrarPanel());
  // (las teclas que se escriben en el panel no le llegan al juego)
  panel.addEventListener('keydown', (e) => { if (e.key === 'Escape') cerrarPanel(); e.stopPropagation(); });
  panel.addEventListener('keyup', (e) => e.stopPropagation());
  panel.addEventListener('keypress', (e) => e.stopPropagation());

  function abrirPanel(p) {
    if (!listo()) return;
    if (panel.classList.contains('oculto')) {
      velAntes = ultimoEstado ? ultimoEstado.vel : 2;
      if (velAntes !== 0) { ponerVelocidad(0); pausadoPorPanel = true; }
    }
    panel.classList.remove('oculto');
    abrirPestana(p || pestana);
  }
  function cerrarPanel() {
    panel.classList.add('oculto');
    if (pausadoPorPanel) { ponerVelocidad(velAntes); pausadoPorPanel = false; }
    document.getElementById('canvas').focus();
  }
  // una orden que se hace con el juego en marcha (fabricar, comer...): se cierra el panel
  const ordenYCerrar = (o) => { cerrarPanel(); ordenar(o); };

  function abrirPestana(p) {
    pestana = p;
    panel.querySelectorAll('.pestanas button[data-p]').forEach((b) => b.classList.toggle('activa', b.dataset.p === p));
    const c = $('.contenido', panel);
    c.innerHTML = '';
    ({ inventario: pintarInventario, fabricar: pintarFabricar, construir: pintarConstruir, salud: pintarSalud, personaje: pintarPersonaje, mapa: pintarMapa })[p](c);
  }

  const NOMBRES_ACCION = { comer: 'Comer', ponerse: 'Ponerse', quitarse: 'Quitarse', empunar: 'Empuñar', leer: 'Leer', usar: 'Usar', soltar: 'Soltar' };
  function pintarInventario(c) {
    const items = json('cdda_ui_inventario') || [];
    const buscar = crear(`<input class="buscar" placeholder="Buscar en el inventario...">`);
    const lista = crear(`<div></div>`);
    c.append(buscar, lista);
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      const grupos = { 'En la mano': [], 'Puesto': [], 'Llevas': [] };
      items.filter((it) => it.nombre !== 'none' && (!q || it.nombre.toLowerCase().includes(q))).forEach((it) => {
        (it.enMano ? grupos['En la mano'] : it.puesto ? grupos.Puesto : grupos.Llevas).push(it);
      });
      lista.innerHTML = '';
      for (const [g, its] of Object.entries(grupos)) {
        if (!its.length) continue;
        lista.appendChild(crear(`<h3>${g} (${its.length})</h3>`));
        its.sort((a, b) => a.categoria.localeCompare(b.categoria) || a.nombre.localeCompare(b.nombre)).forEach((it) => {
          const fila = crear(`<div class="fila-item"><div class="nombre">${esc(it.nombre)}${it.cantidad > 1 ? ` ×${it.cantidad}` : ''}
              <small>${esc(it.categoria)} · ${(it.peso / 1000).toFixed(2)} kg</small></div><div class="acciones-item"></div></div>`);
          it.acciones.forEach((a) => {
            const b = crear(`<button class="boton ${a === 'comer' || a === 'usar' ? 'principal' : ''}">${NOMBRES_ACCION[a] || a}</button>`);
            b.onclick = () => ordenYCerrar({ a: 'objeto', id: it.id, que: a });
            $('.acciones-item', fila).appendChild(b);
          });
          lista.appendChild(fila);
        });
      }
      if (!lista.children.length) lista.innerHTML = '<div class="vacio">No llevas nada.</div>';
    };
    buscar.oninput = pintar;
    pintar();
  }

  const CATEGORIAS = { CC_FOOD: 'Comida', CC_DRINK: 'Bebida', CC_CHEM: 'Química', CC_ELECTRONIC: 'Electrónica', CC_ARMOR: 'Ropa', CC_WEAPON: 'Armas', CC_AMMO: 'Munición', CC_OTHER: 'Otros', CC_ANIMALS: 'Animales', CC_BUILDING: 'Construcción', CC_APPLIANCE: 'Aparatos', CC_CAMP: 'Campamento', CC_PRACTICE: 'Práctica', 'CC_*': 'Varias' };
  function pintarFabricar(c) {
    const r = json('cdda_ui_recetas') || { conocidas: [], porAprender: [] };
    const buscar = crear(`<input class="buscar" placeholder="Buscar una receta...">`);
    const cats = crear(`<div style="display:flex;gap:4px;flex-wrap:wrap;margin-bottom:6px"></div>`);
    const lista = crear(`<div></div>`);
    c.append(buscar, cats, lista);
    let cat = 'todas';
    const usadas = ['todas', ...new Set(r.conocidas.map((x) => x.categoria))];
    usadas.forEach((k) => {
      const chip = crear(`<span class="chip ${k === cat ? 'activa' : ''}">${k === 'todas' ? 'Todas' : CATEGORIAS[k] || k.replace('CC_', '')}</span>`);
      chip.onclick = () => { cat = k; cats.querySelectorAll('.chip').forEach((x) => x.classList.toggle('activa', x === chip)); pintar(); };
      cats.appendChild(chip);
    });
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      const vale = (x) => (cat === 'todas' || x.categoria === cat) && (!q || x.nombre.toLowerCase().includes(q));
      const puede = r.conocidas.filter((x) => vale(x) && x.puede).sort((a, b) => a.nombre.localeCompare(b.nombre));
      const no = r.conocidas.filter((x) => vale(x) && !x.puede).sort((a, b) => a.nombre.localeCompare(b.nombre));
      const bloq = r.porAprender.filter(vale);
      lista.innerHTML = '';
      lista.appendChild(crear(`<h3>Puedes hacer (${puede.length})</h3>`));
      puede.slice(0, 200).forEach((x) => {
        const f = crear(`<div class="fila-item"><div class="nombre">${esc(x.nombre)}</div><div class="acciones-item"><button class="boton principal">Fabricar</button></div></div>`);
        $('button', f).onclick = () => ordenYCerrar({ a: 'fabricar', receta: x.id, cantidad: 1 });
        lista.appendChild(f);
      });
      if (!puede.length) lista.appendChild(crear(`<div class="vacio">Nada con lo que tienes a mano.</div>`));
      lista.appendChild(crear(`<h3>Te falta algo (${no.length})</h3>`));
      no.slice(0, 200).forEach((x) => lista.appendChild(crear(`<div class="fila-item no-puede"><div class="nombre">${esc(x.nombre)}<small>${esc(resumirMotivo(x.motivo))}</small></div><div class="acciones-item"><button class="boton" disabled>Fabricar</button></div></div>`)));
      lista.appendChild(crear(`<h3>Por aprender (${bloq.length})</h3>`));
      bloq.slice(0, 80).forEach((x) => lista.appendChild(crear(`<div class="fila-item bloqueada"><div class="nombre"><i class="fa-solid fa-lock"></i> ${esc(x.nombre)}<small>Se aprende con ${esc(x.requisito)}</small></div></div>`)));
    };
    buscar.oninput = pintar;
    pintar();
  }
  // («These components are missing: > 2 rags ...» a una línea legible)
  const resumirMotivo = (m) => String(m || '').replace(/These tools are missing:/g, 'Faltan herramientas:').replace(/These components are missing:/g, 'Faltan materiales:').replace(/>\s*/g, '').replace(/\s+/g, ' ').trim().slice(0, 220);

  function pintarConstruir(c) {
    const cs = json('cdda_ui_construcciones') || [];
    const buscar = crear(`<input class="buscar" placeholder="Buscar una construcción...">`);
    const lista = crear(`<div></div>`);
    c.append(crear(`<div class="vacio" style="padding-top:0">Al elegir una, se abre el menú de construir del juego para escoger dónde.</div>`), buscar, lista);
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      const vale = (x) => !q || x.nombre.toLowerCase().includes(q);
      const si = cs.filter((x) => x.puede && vale(x)), no = cs.filter((x) => !x.puede && vale(x));
      lista.innerHTML = '';
      lista.appendChild(crear(`<h3>Puedes construir (${si.length})</h3>`));
      si.forEach((x) => {
        const f = crear(`<div class="fila-item"><div class="nombre">${esc(x.nombre)}</div><div class="acciones-item"><button class="boton principal">Construir</button></div></div>`);
        $('button', f).onclick = () => ordenYCerrar({ a: 'construir' });
        lista.appendChild(f);
      });
      lista.appendChild(crear(`<h3>Te falta algo (${no.length})</h3>`));
      no.slice(0, 250).forEach((x) => lista.appendChild(crear(`<div class="fila-item no-puede"><div class="nombre">${esc(x.nombre)}<small>${esc(resumirMotivo(x.motivo))}</small></div></div>`)));
    };
    buscar.oninput = pintar;
    pintar();
  }

  function pintarSalud(c) {
    const e = json('cdda_ui_estado');
    if (!e) return;
    c.appendChild(crear(`<h3>Cuerpo</h3>`));
    e.cuerpo.forEach((p) => {
      const f = p.max > 0 ? p.vida / p.max : 0;
      const col = f > 0.75 ? '#5cb85c' : f > 0.4 ? '#e8d44d' : '#e05a5a';
      c.appendChild(crear(`<div class="fila-item"><div class="nombre">${esc(PARTES[p.id] || p.nombre)}
        <small>${p.vida} / ${p.max}${p.sangra ? ' · <span class="t-mal">sangra</span>' : ''}${p.rota ? ' · <span class="t-mal">rota</span>' : ''}</small>
        <div class="barra"><div style="width:${Math.round(f * 100)}%;background:${col}"></div></div></div></div>`));
    });
    c.appendChild(crear(`<h3>Necesidades</h3>`));
    e.necesidades.forEach((n) => c.appendChild(crear(`<div class="fila-item"><i class="fa-solid ${ICONOS[n.id]}"></i><div class="nombre">${esc(n.nombre)}<small style="color:${color(n.color)}">${esc(textoNecesidad(n))}</small>
      <div class="barra"><div style="width:${Math.round(n.barra * 100)}%;background:${color(n.color)}"></div></div></div></div>`)));
  }

  function pintarPersonaje(c) {
    const p = json('cdda_ui_personaje');
    if (!p) return;
    c.appendChild(crear(`<h3>${esc(p.nombre)}</h3>`));
    c.appendChild(crear(`<div class="atributos">${[['fuerza', 'Fuerza'], ['destreza', 'Destreza'], ['inteligencia', 'Inteligencia'], ['percepcion', 'Percepción']].map(([k, n]) => `<div class="atributo"><b>${p[k]}</b><span>${n}</span></div>`).join('')}</div>`));
    c.appendChild(crear(`<h3>Habilidades</h3>`));
    p.habilidades.sort((a, b) => b.nivel - a.nivel || a.nombre.localeCompare(b.nombre)).forEach((h) => c.appendChild(crear(`<div class="habilidad"><span style="${h.nivel ? '' : 'color:#7a828b'}">${esc(h.nombre)}</span>
      <div class="barra"><div style="width:${Math.min(100, h.nivel * 10)}%;background:var(--acento)"></div></div><span>${h.nivel}</span></div>`)));
    c.appendChild(crear(`<h3>Competencias (${p.competencias.length})</h3>`));
    c.appendChild(crear(`<div style="display:flex;gap:4px;flex-wrap:wrap">${p.competencias.map((x) => `<span class="etiqueta">${esc(x)}</span>`).join('') || '<span class="vacio">Ninguna todavía.</span>'}</div>`));
  }

  function pintarMapa(c) {
    const b = crear(`<button class="boton principal" style="font-size:14px;padding:10px 16px"><i class="fa-solid fa-map"></i> Abrir el mapa del mundo</button>`);
    b.onclick = () => ordenYCerrar({ a: 'mapa' });
    c.append(crear(`<div class="vacio" style="padding-top:0">El mapa del mundo se abre en el juego. Para cerrarlo, Esc.</div>`), b);
  }

  // --- menú al hacer clic en una casilla
  const menuCasilla = crear(`<div id="menu-casilla" class="ui oculto"></div>`);
  const ICONOS_ACCION = { ir: 'fa-person-walking', abrir: 'fa-door-open', cerrar: 'fa-door-closed', coger: 'fa-hand', beber: 'fa-glass-water', pescar: 'fa-fish', examinar: 'fa-magnifying-glass', vehiculo: 'fa-car', hablar: 'fa-comments', atacar: 'fa-hand-fist', mirar: 'fa-eye' };
  function cerrarMenuCasilla() { menuCasilla.classList.add('oculto'); }
  function abrirMenuCasilla(cx, cy, cas) {
    const que = [cas.criatura, cas.mueble, cas.vehiculo, cas.terreno].filter(Boolean)[0] || 'Casilla';
    const extra = cas.numObjetos ? `${cas.numObjetos} objeto${cas.numObjetos > 1 ? 's' : ''}: ${cas.objetos.slice(0, 3).join(', ')}${cas.numObjetos > 3 ? '…' : ''}` : (cas.mueble ? cas.terreno : '');
    menuCasilla.innerHTML = `<div class="que"><b>${esc(que)}</b>${extra ? `<small>${esc(extra)}</small>` : ''}</div>`;
    cas.acciones.forEach((a) => {
      const b = crear(`<button><i class="fa-solid ${ICONOS_ACCION[a.id] || 'fa-circle'}"></i>${esc(a.nombre)}</button>`);
      b.onclick = () => { cerrarMenuCasilla(); ordenar({ a: a.id, dx: cas.dx, dy: cas.dy }); document.getElementById('canvas').focus(); };
      menuCasilla.appendChild(b);
    });
    menuCasilla.classList.remove('oculto');
    const w = menuCasilla.offsetWidth, h = menuCasilla.offsetHeight;
    menuCasilla.style.left = `${Math.min(cx + 8, window.innerWidth - w - 8)}px`;
    menuCasilla.style.top = `${Math.min(cy + 8, window.innerHeight - h - 8)}px`;
  }
  // (el clic en el mapa, antes de que le llegue al juego; con un menú del juego abierto, se deja pasar)
  function clicEnMapa(e) {
    const canvas = document.getElementById('canvas');
    if (e.target !== canvas || !listo() || ventanas() > 1 || e.button !== 0) return;
    const r = canvas.getBoundingClientRect();
    const px = Math.round((e.clientX - r.left) * canvas.width / r.width);
    const py = Math.round((e.clientY - r.top) * canvas.height / r.height);
    const pos = json('cdda_ui_casilla_en_pixel', px, py);
    if (!pos) return;
    e.stopPropagation(); e.preventDefault();
    if (e.type !== 'mousedown') return;
    const cas = json('cdda_ui_casilla', pos.dx, pos.dy);
    if (cas && cas.acciones && cas.acciones.length) abrirMenuCasilla(e.clientX, e.clientY, cas); else cerrarMenuCasilla();
  }

  // --- arranque: cuando hay partida, se monta y se refresca solo
  function montar() {
    document.body.append(hud, registro, boton, panel, menuCasilla);
    // (primero, un clic fuera del menú de casilla lo cierra; luego, si es en el mapa, se abre el de la casilla nueva)
    window.addEventListener('mousedown', (e) => { if (!menuCasilla.contains(e.target)) cerrarMenuCasilla(); }, true);
    ['mousedown', 'mouseup', 'click'].forEach((t) => window.addEventListener(t, clicEnMapa, true));
    window.addEventListener('keydown', (e) => {
      if (e.key === 'Tab' && ventanas() <= 1) { e.preventDefault(); e.stopPropagation(); panel.classList.contains('oculto') ? abrirPanel() : cerrarPanel(); }
      // (con el menú de casilla abierto, Escape solo lo cierra a él: no le llega al juego)
      if (e.key === 'Escape' && !menuCasilla.classList.contains('oculto')) { e.preventDefault(); e.stopPropagation(); cerrarMenuCasilla(); }
    }, true);
  }
  let montado = false;
  function refrescar() {
    if (!listo()) return;
    const e = json('cdda_ui_estado');
    if (!e) { [hud, registro, boton].forEach((x) => x.classList.add('oculto')); return; }
    if (!montado) { montar(); montado = true; }
    // (con una ventana del juego abierta, un diálogo, su inventario, el mapa..., el HUD y lo demás se apartan para no
    // taparla; vuelven al cerrarla)
    const ventanaDelJuego = ventanas() > 1 && panel.classList.contains('oculto');
    [hud, registro, boton].forEach((x) => x.classList.toggle('oculto', ventanaDelJuego));
    if (ventanaDelJuego) cerrarMenuCasilla();
    ultimoEstado = e;
    pintarHud(e);
    actualizarMensajes();
  }
  setInterval(refrescar, 300);
  window.interfazCdda = { abrirPanel, cerrarPanel, abrirPestana, ordenar, json };
})();
