// La interfaz de la versión web (Encargos 7 y 8). Todo lo que no es el mapa va aquí, en HTML encima del juego, y
// habla con él por el enchufe (cdda_ui_*, src/interfaz.cpp y src/interfaz_menus.cpp):
//  - HUD: hora y día, tiempo, necesidades con iconos y barras, el cuerpo (salud por partes) y lo que hay en la mano;
//  - la barra de la actividad en curso (fabricar, leer...), con cancelar; el juego no se para nunca;
//  - un panel con pestañas: Inventario (el muñequito con lo puesto y las bolsas con lo que llevan), Fabricar (con
//    componentes, herramientas, tiempo y habilidad), Construir, Salud, Personaje, Mapa del mundo y Mensajes;
//  - clic en una casilla: solo las acciones que tienen sentido ahí; coger lo que se elija del suelo; hablar con un
//    NPC sin parar el mundo;
//  - la pantalla de inicio (partida nueva, cargar) y la de muerte; las opciones (idioma, pantalla completa,
//    guardar, salir);
//  - avisos breves de lo importante; todo en español o en inglés, nunca mezclado.
// Los iconos son los sprites del propio juego, recortados de las imágenes del tileset.
(() => {
  'use strict';
  const $ = (sel, raiz = document) => raiz.querySelector(sel);
  const $$ = (sel, raiz = document) => [...raiz.querySelectorAll(sel)];
  const crear = (html) => { const t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; };
  const limpio = (s) => String(s ?? '').replace(/<\/?color[^>]*>/g, '');
  const esc = (s) => limpio(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

  // ------------------------------------------------------------------ el enchufe
  const ex = () => window.wasmExports;
  const listo = () => !!(ex() && ex().cdda_ui_estado && window.wasmMemory);
  const leer = (ptr) => { const m = new Uint8Array(window.wasmMemory.buffer); let f = ptr; while (m[f]) f++; return new TextDecoder().decode(m.subarray(ptr, f)); };
  const json = (f, ...args) => { try { return JSON.parse(leer(ex()[f](...args))); } catch { return null; } };
  // las que reciben texto: se escribe en el búfer del juego y se pasa la longitud
  const alBufer = (texto) => {
    const datos = new TextEncoder().encode(texto);
    const ptr = ex().cdda_ui_bufer(), tam = ex().cdda_ui_bufer_tam();
    if (datos.length > tam) return -1;
    new Uint8Array(window.wasmMemory.buffer).set(datos, ptr);
    return datos.length;
  };
  const jsonCon = (f, texto) => { const n = alBufer(texto); return n < 0 ? null : json(f, n); };
  const ordenar = (o) => { const n = alBufer(JSON.stringify(o)); if (n >= 0) ex().cdda_ui_orden(n); };
  const ventanas = () => (ex() && ex().cdda_ventanas ? ex().cdda_ventanas() : 1);

  // ------------------------------------------------------------------ idioma (lo nuestro; lo del juego lo traduce él)
  const TEXTOS = {
    es: {
      dia: 'Día', de: 'de', fuera: 'fuera', en_mano: 'En la mano', manos_vacias: 'Manos vacías', cancelar: 'Cancelar',
      hambre: 'Hambre', sed: 'Sed', sueno: 'Sueño', temperatura: 'Temperatura', animo: 'Ánimo', aguante: 'Aguante', dolor: 'Dolor',
      bien_hambre: 'Saciado', bien_sed: 'Bien', bien_sueno: 'Descansado', bien_dolor: 'Sin dolor', bien_animo: 'Normal',
      inventario: 'Inventario', fabricar: 'Fabricar', construir: 'Construir', salud: 'Salud', personaje: 'Personaje', mapa: 'Mapa', mensajes: 'Mensajes',
      menu: 'Menú (Tab)', opciones: 'Opciones', cerrar: 'Cerrar',
      buscar_inv: 'Buscar en lo que llevas…', buscar_rec: 'Buscar una receta…', buscar_con: 'Buscar una construcción…',
      llevas: 'Llevas', puesto: 'Puesto', nada: 'No llevas nada.', peso: 'Peso', volumen: 'Volumen', cabe: 'cabe',
      cabeza: 'Cabeza', torso: 'Torso', brazos: 'Brazos', manos: 'Manos', piernas: 'Piernas', pies: 'Pies', espalda: 'Espalda', mano: 'Mano',
      comer: 'Comer', beber: 'Beber', ponerse: 'Ponerse', quitarse: 'Quitarse', empunar: 'Empuñar', leer: 'Leer', usar: 'Usar', soltar: 'Soltar',
      todas: 'Todas', puedes: 'Puedes hacer', falta: 'Te falta algo', por_aprender: 'Por aprender', se_aprende: 'Se aprende con',
      componentes: 'Componentes', herramientas: 'Herramientas', cualidades: 'Cualidades', tiempo: 'Tiempo', habilidad: 'Habilidad',
      tu_nivel: 'tu nivel', sale: 'Sale', competencias: 'Competencias', o: 'o', nivel: 'nivel', tienes: 'tienes',
      elige_receta: 'Elige una receta para ver qué hace falta.', elige_con: 'Elige una construcción.',
      donde: 'Dónde (pulsa una casilla de alrededor)', no_hay_sitio: 'No hay sitio a tu alrededor para esto.', puedes_con: 'Puedes construir',
      cuerpo: 'Cuerpo', necesidades: 'Necesidades', sangra: 'sangra', rota: 'rota', atributos: 'Atributos',
      fuerza: 'Fuerza', destreza: 'Destreza', inteligencia: 'Inteligencia', percepcion: 'Percepción', habilidades: 'Habilidades', ninguna: 'Ninguna todavía.',
      todo: 'Todo', combate: 'Combate', ambiente: 'Ambiente', sin_mensajes: 'Sin mensajes.',
      ir: 'Ir aquí', abrir: 'Abrir', cerrar_puerta: 'Cerrar', coger: 'Coger', beber_llenar: 'Beber o llenar', pescar: 'Pescar', examinar: 'Examinar',
      vehiculo: 'Vehículo', hablar: 'Hablar', atacar: 'Atacar', mirar: 'Mirar', objetos: 'objetos',
      coger_titulo: 'Coger del suelo', coger_sel: 'Coger lo marcado', todos: 'Todos', ninguno: 'Ninguno', suelo_vacio: 'No hay nada.',
      el_mundo_sigue: 'El mundo sigue mientras hablas.', adios: 'Despedirse',
      titulo: 'Cataclysm: Dark Days Ahead', subtitulo: 'en tiempo real', nueva: 'Partida nueva', cargar: 'Cargar partida',
      nombre: 'Nombre', nombre_ph: 'Déjalo vacío para uno al azar', sexo: 'Sexo', hombre: 'Hombre', mujer: 'Mujer', empezar: 'Empezar',
      volver: 'Volver', sin_partidas: 'No hay partidas guardadas.', cargando: 'Cargando el mundo…', has_muerto: 'Has muerto',
      sobreviviste: 'Sobreviviste', dias: 'días', horas: 'horas', idioma: 'Idioma', pantalla_completa: 'Pantalla completa',
      guardar: 'Guardar la partida', salir: 'Guardar y salir al menú', guardado: 'Partida guardada.',
      mapa_cerca: 'Más cerca', mapa_lejos: 'Más lejos', aqui: 'Estás aquí', sin_explorar: 'Sin explorar',
      bosque: 'Bosque', campo: 'Campo', agua: 'Agua', carretera: 'Carretera', edificio: 'Edificio', refugio: 'Refugio',
    },
    en: {
      dia: 'Day', de: 'of', fuera: 'outside', en_mano: 'In hand', manos_vacias: 'Empty hands', cancelar: 'Cancel',
      hambre: 'Hunger', sed: 'Thirst', sueno: 'Sleep', temperatura: 'Temperature', animo: 'Mood', aguante: 'Stamina', dolor: 'Pain',
      bien_hambre: 'Full', bien_sed: 'Fine', bien_sueno: 'Rested', bien_dolor: 'No pain', bien_animo: 'Normal',
      inventario: 'Inventory', fabricar: 'Craft', construir: 'Build', salud: 'Health', personaje: 'Character', mapa: 'Map', mensajes: 'Messages',
      menu: 'Menu (Tab)', opciones: 'Options', cerrar: 'Close',
      buscar_inv: 'Search what you carry…', buscar_rec: 'Search a recipe…', buscar_con: 'Search a construction…',
      llevas: 'Carrying', puesto: 'Worn', nada: 'You carry nothing.', peso: 'Weight', volumen: 'Volume', cabe: 'fits',
      cabeza: 'Head', torso: 'Torso', brazos: 'Arms', manos: 'Hands', piernas: 'Legs', pies: 'Feet', espalda: 'Back', mano: 'Hand',
      comer: 'Eat', beber: 'Drink', ponerse: 'Wear', quitarse: 'Take off', empunar: 'Wield', leer: 'Read', usar: 'Use', soltar: 'Drop',
      todas: 'All', puedes: 'You can make', falta: 'Missing something', por_aprender: 'To learn', se_aprende: 'Learned with',
      componentes: 'Components', herramientas: 'Tools', cualidades: 'Qualities', tiempo: 'Time', habilidad: 'Skill',
      tu_nivel: 'your level', sale: 'Makes', competencias: 'Proficiencies', o: 'or', nivel: 'level', tienes: 'you have',
      elige_receta: 'Pick a recipe to see what it needs.', elige_con: 'Pick a construction.',
      donde: 'Where (click a tile around you)', no_hay_sitio: 'There is no room around you for this.', puedes_con: 'You can build',
      cuerpo: 'Body', necesidades: 'Needs', sangra: 'bleeding', rota: 'broken', atributos: 'Attributes',
      fuerza: 'Strength', destreza: 'Dexterity', inteligencia: 'Intelligence', percepcion: 'Perception', habilidades: 'Skills', ninguna: 'None yet.',
      todo: 'All', combate: 'Combat', ambiente: 'Ambient', sin_mensajes: 'No messages.',
      ir: 'Go here', abrir: 'Open', cerrar_puerta: 'Close', coger: 'Pick up', beber_llenar: 'Drink or fill', pescar: 'Fish', examinar: 'Examine',
      vehiculo: 'Vehicle', hablar: 'Talk', atacar: 'Attack', mirar: 'Look', objetos: 'items',
      coger_titulo: 'Pick up from the ground', coger_sel: 'Pick up selected', todos: 'All', ninguno: 'None', suelo_vacio: 'Nothing here.',
      el_mundo_sigue: 'The world goes on while you talk.', adios: 'Leave',
      titulo: 'Cataclysm: Dark Days Ahead', subtitulo: 'in real time', nueva: 'New game', cargar: 'Load game',
      nombre: 'Name', nombre_ph: 'Leave empty for a random one', sexo: 'Sex', hombre: 'Male', mujer: 'Female', empezar: 'Start',
      volver: 'Back', sin_partidas: 'No saved games.', cargando: 'Loading the world…', has_muerto: 'You died',
      sobreviviste: 'You survived', dias: 'days', horas: 'hours', idioma: 'Language', pantalla_completa: 'Fullscreen',
      guardar: 'Save game', salir: 'Save and quit to menu', guardado: 'Game saved.',
      mapa_cerca: 'Zoom in', mapa_lejos: 'Zoom out', aqui: 'You are here', sin_explorar: 'Unexplored',
      bosque: 'Forest', campo: 'Field', agua: 'Water', carretera: 'Road', edificio: 'Building', refugio: 'Shelter',
    },
  };
  let idioma = 'es';
  const T = (k) => (TEXTOS[idioma] && TEXTOS[idioma][k]) || TEXTOS.es[k] || k;
  function leerIdioma() { if (ex() && ex().cdda_ui_idioma) idioma = ex().cdda_ui_idioma() ? 'es' : 'en'; }
  function ponerIdioma(i) {
    idioma = i;
    if (ex() && ex().cdda_ui_poner_idioma) ex().cdda_ui_poner_idioma(i === 'es' ? 1 : 0);
    window.dispatchEvent(new Event('cambioidioma'));
  }

  // ------------------------------------------------------------------ iconos: los sprites del tileset
  const atlas = { listo: false, imagenes: [], ancho: 32, alto: 32 };
  const sprites = new Map();      // "id|cat|var" -> [delante, detrás] o null mientras se pide
  const esperando = new Map();    // clave -> lienzos que esperan
  let pedidosIconos = [];
  async function cargarAtlas() {
    const a = json('cdda_ui_atlas');
    if (!a || !a.imagenes || !a.imagenes.length) return false;
    const fs = window.FS || (window.Module && window.Module.FS);
    if (!fs) return false;
    atlas.ancho = a.ancho; atlas.alto = a.alto;
    atlas.imagenes = [];
    for (const im of a.imagenes) {
      try {
        const ruta = im.ruta.startsWith('/') ? im.ruta : '/' + im.ruta;
        const datos = fs.readFile(ruta);
        const bmp = await createImageBitmap(new Blob([datos], { type: 'image/png' }));
        atlas.imagenes.push({ ...im, bmp, columnas: Math.max(1, Math.floor(bmp.width / im.ancho)) });
      } catch { /* esa imagen no está */ }
    }
    atlas.listo = atlas.imagenes.length > 0;
    return atlas.listo;
  }
  function pintarSprite(ctx, indice, w, h) {
    if (indice < 0) return false;
    const im = atlas.imagenes.find((x) => indice >= x.desde && indice < x.desde + x.cuantos);
    if (!im) return false;
    const local = indice - im.desde;
    const sx = (local % im.columnas) * im.ancho, sy = Math.floor(local / im.columnas) * im.alto;
    const escala = Math.min(w / im.ancho, h / im.alto);
    const dw = im.ancho * escala, dh = im.alto * escala;
    ctx.drawImage(im.bmp, sx, sy, im.ancho, im.alto, (w - dw) / 2, (h - dh) / 2, dw, dh);
    return true;
  }
  function pintarIcono(lienzo, par) {
    const ctx = lienzo.getContext('2d');
    ctx.imageSmoothingEnabled = false;
    ctx.clearRect(0, 0, lienzo.width, lienzo.height);
    if (!par || (par[0] < 0 && par[1] < 0)) { lienzo.dataset.sin = '1'; return; }
    pintarSprite(ctx, par[1], lienzo.width, lienzo.height);
    pintarSprite(ctx, par[0], lienzo.width, lienzo.height);
  }
  // un icono (lienzo) para una cosa del juego; se pinta en cuanto se sepa su sprite
  function icono(id, cat = 'item', variante = '', tam = 32) {
    const c = document.createElement('canvas');
    c.className = 'icono'; c.width = tam; c.height = tam; c.style.width = tam + 'px'; c.style.height = tam + 'px';
    if (!id) return c;
    const clave = `${id}|${cat}|${variante || ''}`;
    if (sprites.has(clave) && sprites.get(clave)) { if (atlas.listo) pintarIcono(c, sprites.get(clave)); else apuntar(clave, c); return c; }
    apuntar(clave, c);
    if (!sprites.has(clave)) { sprites.set(clave, null); pedidosIconos.push([id, cat, variante || '']); }
    return c;
  }
  function apuntar(clave, c) { if (!esperando.has(clave)) esperando.set(clave, []); esperando.get(clave).push(c); }
  async function resolverIconos() {
    if (!listo()) return;
    if (!atlas.listo) { if (!(await cargarAtlas())) return; }
    if (pedidosIconos.length) {
      const lote = pedidosIconos.splice(0, 400);
      const r = jsonCon('cdda_ui_iconos', JSON.stringify(lote)) || {};
      for (const [k, v] of Object.entries(r)) sprites.set(k, v);
      for (const [id, cat, variante] of lote) { const k = `${id}|${cat}|${variante}`; if (!sprites.get(k)) sprites.set(k, [-1, -1]); }
    }
    for (const [clave, lienzos] of esperando) {
      const par = sprites.get(clave);
      if (!par) continue;
      for (const c of lienzos) pintarIcono(c, par);
      esperando.delete(clave);
    }
  }
  setInterval(resolverIconos, 120);
  const marco = (id, cat, variante, tam = 32, num = '') => {
    const m = crear(`<div class="marco-icono"></div>`);
    m.appendChild(icono(id, cat, variante, tam));
    if (num) m.appendChild(crear(`<span class="num">${esc(num)}</span>`));
    return m;
  };

  // ------------------------------------------------------------------ colores del juego y figura del cuerpo
  const COLORES = {
    red: '#e05a5a', light_red: '#ff8a80', green: '#5cb85c', light_green: '#9be07c', yellow: '#e8d44d', light_blue: '#79b8ff', blue: '#4a7fd8',
    white: '#ececec', light_gray: '#c4c4c4', dark_gray: '#7a7a7a', magenta: '#d36fd3', pink: '#ff99d6', cyan: '#4dd0e1', light_cyan: '#a6f3ff', brown: '#b5823f',
  };
  const color = (c) => COLORES[String(c || '').replace(/^[a-z]_/, '')] || '#c4c4c4';
  const colorVida = (f) => (f > 0.75 ? '#5cb85c' : f > 0.45 ? '#e8d44d' : f > 0.2 ? '#e8913d' : '#e05a5a');
  // la silueta: cada parte con el color de su salud
  function figura(cuerpo, alto = 120) {
    const vida = {};
    for (const p of cuerpo || []) vida[p.id] = p.max > 0 ? p.vida / p.max : 0;
    const c = (id) => (id in vida ? colorVida(vida[id]) : '#444');
    const w = alto * 0.55;
    return `<svg class="cuerpo-svg" width="${w}" height="${alto}" viewBox="0 0 55 100">
      <circle class="parte" cx="27.5" cy="10" r="8" fill="${c('head')}"/>
      <rect class="parte" x="17" y="20" width="21" height="32" rx="5" fill="${c('torso')}"/>
      <rect class="parte" x="7" y="21" width="8.5" height="30" rx="4" fill="${c('arm_r')}"/>
      <rect class="parte" x="39.5" y="21" width="8.5" height="30" rx="4" fill="${c('arm_l')}"/>
      <rect class="parte" x="18" y="53.5" width="9" height="40" rx="4" fill="${c('leg_r')}"/>
      <rect class="parte" x="28" y="53.5" width="9" height="40" rx="4" fill="${c('leg_l')}"/>
    </svg>`;
  }

  // ------------------------------------------------------------------ HUD
  const ICONOS_NEC = { hambre: 'fa-utensils', sed: 'fa-droplet', sueno: 'fa-bed', temperatura: 'fa-temperature-half', animo: 'fa-face-smile', aguante: 'fa-person-running', dolor: 'fa-bandage' };
  const hud = crear(`<div id="hud" class="ui oculto">
      <div class="fila"><span class="hora">--:--</span><span class="fecha"></span><button class="plegar"><i class="fa-solid fa-chevron-up"></i></button></div>
      <div class="clima"></div>
      <div class="necesidades plegable"></div>
      <div class="hud-abajo plegable"><div class="mini-cuerpo"></div><div class="mano"></div></div>
    </div>`);
  $('.plegar', hud).onclick = () => hud.classList.toggle('plegado');
  function textoNecesidad(n) {
    const t = limpio(n.texto).trim();
    if (n.id === 'animo') return { ':D': T('bien_animo'), ':)': T('bien_animo'), ':|': T('bien_animo') }[t] || t || T('bien_animo');
    return t || T('bien_' + n.id) || '';
  }
  let firmaMano = '';
  function pintarHud(e) {
    $('.hora', hud).textContent = e.hora.replace(/:\d\d(\s?[AP]M)$/, '$1');
    $('.fecha', hud).textContent = `${T('dia')} ${e.dia} ${T('de')} ${e.estacion}`;
    $('.clima', hud).innerHTML = `<i class="fa-solid ${e.exterior ? 'fa-cloud-sun' : 'fa-house'}"></i><span style="color:${color(e.tiempoColor)}">${esc(e.tiempo)}</span> · ${Math.round(e.temperaturaC)} °C${e.exterior ? '' : ' ' + T('fuera')}`;
    $('.necesidades', hud).innerHTML = e.necesidades.map((n) => `<i class="fa-solid ${ICONOS_NEC[n.id] || 'fa-circle'}"></i>
      <div class="necesidad"><div class="rotulo"><span>${T(n.id)}</span><span style="color:${color(n.color)}">${esc(textoNecesidad(n))}</span></div>
      <div class="barra"><div style="width:${Math.round(n.barra * 100)}%;background:${color(n.color)}"></div></div></div>`).join('');
    $('.mini-cuerpo', hud).innerHTML = figura(e.cuerpo, 64);
    const firma = e.enMano || '';
    if (firma !== firmaMano) {
      firmaMano = firma;
      const m = $('.mano', hud);
      m.innerHTML = '';
      if (e.enManoTipo) m.appendChild(marco(e.enManoTipo, 'item', '', 28));
      m.appendChild(crear(`<div style="min-width:0"><small>${T('en_mano')}</small><b>${esc(e.enMano || T('manos_vacias'))}</b></div>`));
    }
  }

  // ------------------------------------------------------------------ barra de la actividad
  const actividad = crear(`<div id="actividad" class="ui oculto"><div class="fila"><i class="fa-solid fa-hourglass-half"></i><span class="nombre"></span>
    <button class="boton cancelar"></button></div><div class="barra"><div></div></div></div>`);
  $('.cancelar', actividad).onclick = () => ordenar({ a: 'cancelar_actividad' });
  function pintarActividad(e) {
    const a = e.actividad;
    if (!a || ['ACT_NULL', 'ACT_WAIT_STAMINA'].includes(a.id)) { actividad.classList.add('oculto'); return; }
    actividad.classList.remove('oculto');
    $('.nombre', actividad).textContent = limpio(a.nombre) || a.id;
    $('.cancelar', actividad).textContent = T('cancelar');
    $('.cancelar', actividad).classList.toggle('oculto', !a.cancelable);
    const bar = $('.barra', actividad);
    bar.classList.toggle('oculto', !(a.progreso >= 0));
    bar.firstElementChild.style.width = `${Math.round(Math.max(0, a.progreso) * 100)}%`;
  }

  // ------------------------------------------------------------------ avisos breves
  const avisos = crear(`<div id="avisos" class="ui"></div>`);
  const ultimosAvisos = new Map();
  function aviso(texto, tipo) {
    const ahora = Date.now();
    if (ultimosAvisos.has(texto) && ahora - ultimosAvisos.get(texto) < 12000) return;
    ultimosAvisos.set(texto, ahora);
    const [cls, ico] = TIPOS[tipo] || TIPOS[5];
    const a = crear(`<div class="aviso-breve ${cls}"><i class="fa-solid ${ico}"></i> ${esc(texto)}</div>`);
    avisos.appendChild(a);
    while (avisos.children.length > 3) avisos.firstElementChild.remove();
    setTimeout(() => a.classList.add('sale'), 4200);
    setTimeout(() => a.remove(), 5000);
  }

  // ------------------------------------------------------------------ mensajes (pestaña)
  const TIPOS = { 0: ['t-bien', 'fa-circle-check'], 1: ['t-mal', 'fa-circle-exclamation'], 2: ['t-mixto', 'fa-circle-half-stroke'], 3: ['t-aviso', 'fa-triangle-exclamation'], 4: ['t-info', 'fa-circle-info'], 5: ['t-normal', 'fa-circle'], 7: ['t-mal', 'fa-crosshairs'], 8: ['t-mal', 'fa-burst'], 9: ['t-normal', 'fa-crosshairs'] };
  const GRUPOS = {
    combate: /\b(hit|hits|miss|misses|attack|bite|bites|punch|kick|slash|stab|shoot|shot|dodge|block|kill|dies|died|wound|damage|zombie|claw|swing|grab|golpe|golpea|muerde|ataca|esquiva|dispara|mata|muere|herida|daño|zombi)/i,
    salud: /\b(pain|bleed|bleeding|hungry|thirsty|tired|sleep|wake|feel|hurt|bandage|sick|nause|eat|drink|warm|cold|heal|dolor|sangr|hambre|sed|cansad|dorm|despiert|sientes|comes|bebes|calor|frío|curas)/i,
    ambiente: /\b(hear|sound|rain|weather|sun|night|wind|snow|thunder|light|dark|smell|oyes|sonido|lluvia|tiempo|sol|noche|viento|nieve|trueno|luz|oscur|huele)/i,
  };
  const grupo = (t) => Object.keys(GRUPOS).find((g) => GRUPOS[g].test(t)) || 'general';
  let mensajes = [], totalVisto = -1, filtroMensajes = 'todo', primerosMensajes = true;
  function actualizarMensajes() {
    const r = json('cdda_ui_mensajes', 120);
    if (!r || r.total === totalVisto) return;
    const antes = mensajes.length ? mensajes[mensajes.length - 1] : null;
    // (los repetidos seguidos, juntos en uno con un contador)
    const lista = [];
    for (const m of r.mensajes) {
      const t = limpio(m.texto).replace(/\s+x\s*\d+$/, '');
      const ult = lista[lista.length - 1];
      if (ult && ult.base === t) { ult.veces++; ult.hora = m.hora; continue; }
      lista.push({ ...m, base: t, texto: t, veces: Number((limpio(m.texto).match(/x\s*(\d+)$/) || [])[1] || 1), grupo: grupo(t) });
    }
    // lo nuevo e importante, como aviso breve
    if (!primerosMensajes) {
      const idx = antes ? lista.findIndex((m) => m.turno > antes.turno || (m.turno === antes.turno && m.base !== antes.base && lista.indexOf(m) > lista.length - 4)) : -1;
      for (const m of idx >= 0 ? lista.slice(idx) : []) if ([1, 3, 7, 8].includes(m.tipo)) aviso(m.texto, m.tipo);
    }
    primerosMensajes = false;
    mensajes = lista;
    totalVisto = r.total;
    if (pestana === 'mensajes' && !panel.classList.contains('oculto')) pintarMensajes($('.contenido', panel));
  }
  function pintarMensajes(c) {
    c.innerHTML = '';
    const col = crear(`<div class="columna entera"></div>`);
    const chips = crear(`<div class="chips"></div>`);
    for (const g of ['todo', 'combate', 'salud', 'ambiente']) {
      const ch = crear(`<span class="chip ${g === filtroMensajes ? 'activa' : ''}">${T(g)}</span>`);
      ch.onclick = () => { filtroMensajes = g; pintarMensajes(c); };
      chips.appendChild(ch);
    }
    col.appendChild(chips);
    const vis = mensajes.filter((m) => filtroMensajes === 'todo' || m.grupo === filtroMensajes);
    col.appendChild(crear(`<div>${vis.slice().reverse().map((m) => {
      const [cls, ico] = TIPOS[m.tipo] || TIPOS[5];
      return `<div class="mensaje ${cls}"><i class="fa-solid ${ico}"></i><span>${esc(m.texto)}${m.veces > 1 ? `<span class="veces">×${m.veces}</span>` : ''}</span><span class="h">${esc(m.hora.replace(/:\d\d(\s?[AP]M)$/, '$1'))}</span></div>`;
    }).join('') || `<div class="vacio">${T('sin_mensajes')}</div>`}</div>`));
    c.appendChild(col);
  }

  // ------------------------------------------------------------------ panel con pestañas
  const PESTANAS = [['inventario', 'fa-briefcase'], ['fabricar', 'fa-hammer'], ['construir', 'fa-trowel-bricks'], ['salud', 'fa-heart-pulse'], ['personaje', 'fa-user'], ['mapa', 'fa-map'], ['mensajes', 'fa-comment-dots']];
  const panel = crear(`<div id="panel" class="ui oculto"><div class="pestanas"></div><div class="contenido"></div></div>`);
  let pestana = 'inventario';
  function pintarPestanas() {
    const p = $('.pestanas', panel);
    p.innerHTML = PESTANAS.map(([id, ico]) => `<button data-p="${id}" class="${id === pestana ? 'activa' : ''}"><i class="fa-solid ${ico}"></i>${T(id)}</button>`).join('') +
      `<button class="cerrar" title="${T('cerrar')}"><i class="fa-solid fa-xmark"></i></button>`;
    $$('button[data-p]', p).forEach((b) => b.onclick = () => abrirPestana(b.dataset.p));
    $('.cerrar', p).onclick = cerrarPanel;
  }
  panel.addEventListener('keydown', (e) => { if (e.key === 'Escape') cerrarPanel(); e.stopPropagation(); });
  panel.addEventListener('keyup', (e) => e.stopPropagation());
  function abrirPanel(p) { panel.classList.remove('oculto'); abrirPestana(p || pestana); }
  function cerrarPanel() { panel.classList.add('oculto'); document.getElementById('canvas').focus(); }
  function abrirPestana(p) {
    pestana = p;
    pintarPestanas();
    const c = $('.contenido', panel);
    c.innerHTML = '';
    ({ inventario: pintarInventario, fabricar: pintarFabricar, construir: pintarConstruir, salud: pintarSalud, personaje: pintarPersonaje, mapa: pintarMapa, mensajes: pintarMensajes })[p](c);
  }

  // --- inventario: el muñequito y las bolsas
  const HUECOS = [
    ['cabeza', ['head', 'eyes', 'mouth'], 124, 0], ['torso', ['torso'], 124, 112], ['brazos', ['arm_l', 'arm_r'], 30, 96],
    ['manos', ['hand_l', 'hand_r'], 218, 180], ['piernas', ['leg_l', 'leg_r'], 124, 236], ['pies', ['foot_l', 'foot_r'], 124, 346],
    ['espalda', [], 218, 40], ['mano', [], 30, 210],
  ];
  function huecoDe(o) {
    if (o.contenedor && o.capacidad > 1500) return 'espalda';
    const c = o.cubre || [];
    if (c.includes('torso')) return 'torso';
    if (c.some((b) => ['leg_l', 'leg_r'].includes(b))) return 'piernas';
    if (c.some((b) => ['head', 'eyes', 'mouth'].includes(b))) return 'cabeza';
    if (c.some((b) => ['hand_l', 'hand_r'].includes(b))) return 'manos';
    if (c.some((b) => ['foot_l', 'foot_r'].includes(b))) return 'pies';
    if (c.some((b) => ['arm_l', 'arm_r'].includes(b))) return 'brazos';
    return 'torso';
  }
  function pintarInventario(c) {
    const eq = json('cdda_ui_equipo');
    if (!eq) return;
    const objs = eq.objetos.filter((o) => o.nombre !== 'none');
    const izq = crear(`<div class="columna lista"></div>`), der = crear(`<div class="columna detalle"></div>`);
    c.append(izq, der);
    // el muñequito
    const muneco = crear(`<div class="muneco">${figura([], 300).replace('width="165"', 'width="150"')}</div>`);
    const porHueco = {};
    for (const o of objs) {
      if (o.enMano) (porHueco.mano = porHueco.mano || []).push(o);
      else if (o.puesto) (porHueco[huecoDe(o)] = porHueco[huecoDe(o)] || []).push(o);
    }
    for (const [h, , x, y] of HUECOS) {
      const lista = porHueco[h] || [];
      const hueco = crear(`<div class="hueco" style="left:${x}px;top:${y}px"></div>`);
      const m = lista.length ? marco(lista[lista.length - 1].tipo, 'item', lista[lista.length - 1].variante, 36) : crear(`<div class="marco-icono vacio"></div>`);
      if (lista.length) m.onclick = () => (lista.length === 1 ? abrirObjeto(lista[0]) : abrirLista(T(h), lista));
      hueco.appendChild(m);
      if (lista.length > 1) hueco.appendChild(crear(`<span class="mas">${lista.length}</span>`));
      hueco.appendChild(crear(`<small>${T(h)}</small>`));
      muneco.appendChild(hueco);
    }
    izq.appendChild(muneco);
    izq.appendChild(crear(`<div class="dato"><span>${T('peso')}</span><span>${(eq.peso / 1000).toFixed(1)} / ${(eq.pesoMax / 1000).toFixed(1)} kg</span></div>`));
    izq.appendChild(crear(`<div class="dato"><span>${T('volumen')}</span><span>${(eq.volumen / 1000).toFixed(1)} / ${(eq.volumenMax / 1000).toFixed(1)} L</span></div>`));
    // las bolsas: cada contenedor que se lleva, con lo de dentro
    const buscar = crear(`<input class="buscar" placeholder="${T('buscar_inv')}">`);
    der.appendChild(buscar);
    const bolsas = crear(`<div></div>`);
    der.appendChild(bolsas);
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      bolsas.innerHTML = '';
      const hijos = (id) => objs.filter((o) => o.dentroDe === id);
      const contenedores = objs.filter((o) => (o.puesto || o.enMano) && o.contenedor && hijos(o.id).length >= 0 && o.capacidad > 0);
      for (const b of contenedores) {
        const dentro = hijos(b.id).filter((o) => !q || o.nombre.toLowerCase().includes(q));
        if (q && !dentro.length) continue;
        const caja = crear(`<div class="bolsa"><div class="cabecera"></div><div class="rejilla"></div></div>`);
        $('.cabecera', caja).append(icono(b.tipo, 'item', b.variante, 24), crear(`<span class="nombre">${esc(b.nombre)}</span>`),
          crear(`<small>${(b.lleno / 1000).toFixed(1)} / ${(b.capacidad / 1000).toFixed(1)} L</small>`));
        const rej = $('.rejilla', caja);
        for (const o of dentro) {
          const cel = crear(`<div class="casilla-obj" title="${esc(o.nombre)}"></div>`);
          cel.appendChild(marco(o.tipo, 'item', o.variante, 32, o.cantidad > 1 ? String(o.cantidad) : (o.cargas > 1 ? String(o.cargas) : '')));
          cel.onclick = () => abrirObjeto(o);
          rej.appendChild(cel);
        }
        if (!dentro.length) rej.appendChild(crear(`<span class="vacio">—</span>`));
        bolsas.appendChild(caja);
      }
      if (!bolsas.children.length) bolsas.appendChild(crear(`<div class="vacio">${T('nada')}</div>`));
    };
    buscar.oninput = pintar;
    buscar.addEventListener('keydown', (e) => e.stopPropagation());
    pintar();
  }
  // la tarjeta de un objeto: icono, datos y lo que se puede hacer con él
  const ventanaObjeto = crear(`<div id="ventana-objeto" class="ventana ui oculto"><div class="cabeza"><span class="titulo"></span><button class="cerrar"><i class="fa-solid fa-xmark"></i></button></div><div class="cuerpo"></div><div class="pie"></div></div>`);
  $('.cerrar', ventanaObjeto).onclick = () => ventanaObjeto.classList.add('oculto');
  function abrirObjeto(o) {
    $('.titulo', ventanaObjeto).textContent = limpio(o.nombre);
    const cu = $('.cuerpo', ventanaObjeto), pie = $('.pie', ventanaObjeto);
    cu.innerHTML = ''; pie.innerHTML = '';
    const cab = crear(`<div class="tarjeta-cabeza"></div>`);
    cab.append(marco(o.tipo, 'item', o.variante, 56), crear(`<div><b>${esc(o.nombre)}</b><div class="descripcion">${esc(o.categoria)}</div></div>`));
    cu.append(cab, crear(`<div class="dato"><span>${T('peso')}</span><span>${(o.peso / 1000).toFixed(2)} kg</span></div>`),
      crear(`<div class="dato"><span>${T('volumen')}</span><span>${(o.volumen / 1000).toFixed(2)} L</span></div>`));
    for (const a of o.acciones || []) {
      const b = crear(`<button class="boton ${['comer', 'beber', 'usar', 'ponerse'].includes(a) ? 'principal' : ''}">${T(a)}</button>`);
      b.onclick = () => { ordenar({ a: 'objeto', id: o.id, que: a }); ventanaObjeto.classList.add('oculto'); setTimeout(() => { if (!panel.classList.contains('oculto') && pestana === 'inventario') abrirPestana('inventario'); }, 700); };
      pie.appendChild(b);
    }
    ventanaObjeto.classList.remove('oculto');
  }
  function abrirLista(titulo, lista) {
    $('.titulo', ventanaObjeto).textContent = titulo;
    const cu = $('.cuerpo', ventanaObjeto);
    cu.innerHTML = ''; $('.pie', ventanaObjeto).innerHTML = '';
    for (const o of lista) {
      const f = crear(`<div class="fila-icono"><span class="nombre">${esc(o.nombre)}</span></div>`);
      f.prepend(marco(o.tipo, 'item', o.variante, 28));
      f.onclick = () => abrirObjeto(o);
      cu.appendChild(f);
    }
    ventanaObjeto.classList.remove('oculto');
  }

  // --- fabricar: la lista con iconos y el detalle de la elegida
  const CATEGORIAS = { CC_FOOD: ['fa-utensils', 'Comida', 'Food'], CC_DRINK: ['fa-mug-hot', 'Bebida', 'Drinks'], CC_CHEM: ['fa-flask', 'Química', 'Chemistry'],
    CC_ELECTRONIC: ['fa-microchip', 'Electrónica', 'Electronics'], CC_ARMOR: ['fa-shirt', 'Ropa', 'Clothing'], CC_WEAPON: ['fa-khanda', 'Armas', 'Weapons'],
    CC_AMMO: ['fa-bullseye', 'Munición', 'Ammo'], CC_OTHER: ['fa-box', 'Otros', 'Other'], CC_ANIMALS: ['fa-paw', 'Animales', 'Animals'],
    CC_BUILDING: ['fa-house', 'Construcción', 'Building'], CC_APPLIANCE: ['fa-plug', 'Aparatos', 'Appliances'], CC_PRACTICE: ['fa-graduation-cap', 'Práctica', 'Practice'],
    'CC_*': ['fa-star', 'Varias', 'Misc'] };
  const nombreCat = (k) => { const c = CATEGORIAS[k]; return c ? c[idioma === 'es' ? 1 : 2] : k.replace('CC_', ''); };
  let recetaElegida = null, catFabricar = 'todas';
  function pintarFabricar(c) {
    const r = json('cdda_ui_recetas') || { conocidas: [], porAprender: [] };
    const izq = crear(`<div class="columna lista"></div>`), der = crear(`<div class="columna detalle"></div>`);
    c.append(izq, der);
    const buscar = crear(`<input class="buscar" placeholder="${T('buscar_rec')}">`);
    buscar.addEventListener('keydown', (e) => e.stopPropagation());
    const chips = crear(`<div class="chips"></div>`);
    const lista = crear(`<div></div>`);
    izq.append(buscar, chips, lista);
    const cats = ['todas', ...new Set(r.conocidas.map((x) => x.categoria))];
    for (const k of cats) {
      const ch = crear(`<span class="chip ${k === catFabricar ? 'activa' : ''}">${k === 'todas' ? T('todas') : `<i class="fa-solid ${(CATEGORIAS[k] || ['fa-box'])[0]}"></i> ${nombreCat(k)}`}</span>`);
      ch.onclick = () => { catFabricar = k; $$('.chip', chips).forEach((x) => x.classList.toggle('activa', x === ch)); pintar(); };
      chips.appendChild(ch);
    }
    const fila = (x, clase, sub) => {
      const f = crear(`<div class="fila-icono ${clase} ${recetaElegida === x.id ? 'elegida' : ''}"><span class="nombre">${esc(x.nombre)}${sub ? `<small>${esc(sub)}</small>` : ''}</span></div>`);
      f.prepend(marco(x.resultado || x.id, 'item', '', 28));
      return f;
    };
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      const vale = (x) => (catFabricar === 'todas' || x.categoria === catFabricar) && (!q || x.nombre.toLowerCase().includes(q));
      const puede = r.conocidas.filter((x) => vale(x) && x.puede).sort((a, b) => a.nombre.localeCompare(b.nombre));
      const no = r.conocidas.filter((x) => vale(x) && !x.puede).sort((a, b) => a.nombre.localeCompare(b.nombre));
      const bloq = r.porAprender.filter(vale);
      lista.innerHTML = '';
      const seccion = (titulo, cosas, clase, sub, max) => {
        lista.appendChild(crear(`<h3>${titulo} (${cosas.length})</h3>`));
        for (const x of cosas.slice(0, max)) {
          const f = fila(x, clase, sub(x));
          f.onclick = () => { recetaElegida = x.id; $$('.fila-icono', lista).forEach((y) => y.classList.remove('elegida')); f.classList.add('elegida'); detalleReceta(der, x); };
          lista.appendChild(f);
        }
      };
      seccion(T('puedes'), puede, '', () => '', 150);
      seccion(T('falta'), no, 'no-puede', () => '', 150);
      seccion(`<i class="fa-solid fa-lock"></i> ${T('por_aprender')}`, bloq, 'no-puede', (x) => `${T('se_aprende')} ${x.requisito}`, 60);
    };
    buscar.oninput = pintar;
    pintar();
    const elegida = r.conocidas.find((x) => x.id === recetaElegida);
    if (elegida) detalleReceta(der, elegida); else der.appendChild(crear(`<div class="vacio">${T('elige_receta')}</div>`));
  }
  function detalleReceta(der, x) {
    const d = jsonCon('cdda_ui_receta', x.id);
    der.innerHTML = '';
    if (!d) { der.appendChild(crear(`<div class="vacio">${esc(x.nombre)}</div>`)); return; }
    const cab = crear(`<div class="tarjeta-cabeza"></div>`);
    cab.append(marco(d.resultado, 'item', '', 56, d.cantidad > 1 ? String(d.cantidad) : ''), crear(`<div><h2>${esc(d.nombre)}</h2><div class="descripcion">${esc(d.descripcion).slice(0, 220)}</div></div>`));
    der.appendChild(cab);
    der.appendChild(crear(`<div class="dato"><span><i class="fa-regular fa-clock"></i> ${T('tiempo')}</span><span>${esc(d.tiempo)}</span></div>`));
    if (d.habilidad) der.appendChild(crear(`<div class="dato"><span><i class="fa-solid fa-graduation-cap"></i> ${T('habilidad')}</span><span style="color:${d.tieneHabilidad ? 'var(--bien)' : 'var(--mal)'}">${esc(d.habilidad)} ${d.dificultad} <small>(${T('tu_nivel')} ${d.tuNivel})</small></span></div>`));
    if (d.competencias) der.appendChild(crear(`<div class="dato"><span>${T('competencias')}</span><span>${esc(d.competencias)}</span></div>`));
    const grupoAlt = (titulo, grupos, pintarUno) => {
      if (!grupos.length) return;
      der.appendChild(crear(`<h3>${titulo}</h3>`));
      for (const g of grupos) {
        const caja = crear(`<div class="grupo-alt"></div>`);
        g.forEach((alt, i) => { if (i > 0) caja.appendChild(crear(`<div class="o">${T('o')}</div>`)); caja.appendChild(pintarUno(alt)); });
        der.appendChild(caja);
      }
    };
    grupoAlt(T('componentes'), d.componentes, (a) => {
      const f = crear(`<div class="necesita ${a.tiene >= a.necesita ? 'si' : 'no'}"><span>${esc(a.nombre)}</span><span class="cuenta">${a.tiene} / ${a.necesita}</span></div>`);
      f.prepend(icono(a.tipo, 'item', '', 24));
      return f;
    });
    grupoAlt(T('herramientas'), d.herramientas, (a) => {
      const f = crear(`<div class="necesita ${a.tiene ? 'si' : 'no'}"><span>${esc(a.nombre)}</span><span class="cuenta">${a.tiene ? '✔' : '✘'}</span></div>`);
      f.prepend(icono(a.tipo, 'item', '', 24));
      return f;
    });
    grupoAlt(T('cualidades'), d.cualidades, (a) => crear(`<div class="necesita ${a.tiene ? 'si' : 'no'}"><i class="fa-solid fa-screwdriver-wrench"></i><span>${esc(a.nombre)} (${T('nivel')} ${a.nivel})</span><span class="cuenta">${a.tiene ? '✔' : '✘'}</span></div>`));
    const botones = crear(`<div class="botones"></div>`);
    const b = crear(`<button class="boton principal grande"><i class="fa-solid fa-hammer"></i> ${T('fabricar')}</button>`);
    b.disabled = !d.puede;
    b.onclick = () => { ordenar({ a: 'fabricar', receta: d.id, cantidad: 1 }); cerrarPanel(); };
    botones.appendChild(b);
    der.appendChild(botones);
    if (!d.puede && x.motivo) der.appendChild(crear(`<div class="motivo">${esc(resumirMotivo(x.motivo))}</div>`));
  }
  const resumirMotivo = (m) => limpio(m).replace(/These tools are missing:/g, idioma === 'es' ? 'Faltan herramientas:' : 'Missing tools:').replace(/These components are missing:/g, idioma === 'es' ? 'Faltan materiales:' : 'Missing components:').replace(/>\s*/g, '').replace(/\s+/g, ' ').trim().slice(0, 260);

  // --- construir
  let construccionElegida = null;
  function pintarConstruir(c) {
    const cs = json('cdda_ui_construcciones') || [];
    const izq = crear(`<div class="columna lista"></div>`), der = crear(`<div class="columna detalle"></div>`);
    c.append(izq, der);
    const buscar = crear(`<input class="buscar" placeholder="${T('buscar_con')}">`);
    buscar.addEventListener('keydown', (e) => e.stopPropagation());
    const lista = crear(`<div></div>`);
    izq.append(buscar, lista);
    const detalle = (x) => {
      der.innerHTML = '';
      der.appendChild(crear(`<div class="tarjeta-cabeza"><div class="marco-icono" style="width:56px;height:56px"><i class="fa-solid fa-trowel-bricks" style="font-size:24px;color:var(--acento)"></i></div><h2>${esc(x.nombre)}</h2></div>`));
      if (!x.puede) { der.appendChild(crear(`<div class="motivo">${esc(resumirMotivo(x.motivo))}</div>`)); return; }
      // dónde: las casillas de alrededor (el centro es el personaje); las que valen, se pueden pulsar
      der.appendChild(crear(`<h3>${T('donde')}</h3>`));
      const rej = crear(`<div class="rejilla-donde"></div>`);
      const vale = new Set((x.donde || []).map(([dx, dy]) => `${dx},${dy}`));
      for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
        const yo = dx === 0 && dy === 0, ok = vale.has(`${dx},${dy}`);
        const celda = crear(`<button class="celda-donde ${yo ? 'yo' : ok ? 'vale' : ''}" ${ok ? '' : 'disabled'}>${yo ? '<i class="fa-solid fa-person"></i>' : ok ? '<i class="fa-solid fa-trowel-bricks"></i>' : ''}</button>`);
        if (ok) celda.onclick = () => { ordenar({ a: 'construir', grupo: x.grupo, dx, dy }); cerrarPanel(); };
        rej.appendChild(celda);
      }
      der.appendChild(rej);
      if (!vale.size) der.appendChild(crear(`<div class="motivo">${T('no_hay_sitio')}</div>`));
    };
    const pintar = () => {
      const q = buscar.value.toLowerCase();
      lista.innerHTML = '';
      for (const [titulo, cosas, clase] of [[T('puedes_con'), cs.filter((x) => x.puede), ''], [T('falta'), cs.filter((x) => !x.puede), 'no-puede']]) {
        const v = cosas.filter((x) => !q || x.nombre.toLowerCase().includes(q));
        lista.appendChild(crear(`<h3>${titulo} (${v.length})</h3>`));
        for (const x of v.slice(0, 200)) {
          const f = crear(`<div class="fila-icono ${clase} ${construccionElegida === x.nombre ? 'elegida' : ''}"><i class="fa-solid fa-trowel-bricks" style="width:28px;text-align:center;color:var(--suave)"></i><span class="nombre">${esc(x.nombre)}</span></div>`);
          f.onclick = () => { construccionElegida = x.nombre; $$('.fila-icono', lista).forEach((y) => y.classList.remove('elegida')); f.classList.add('elegida'); detalle(x); };
          lista.appendChild(f);
        }
      }
    };
    buscar.oninput = pintar;
    pintar();
    der.appendChild(crear(`<div class="vacio">${T('elige_con')}</div>`));
  }

  // --- salud y personaje
  function pintarSalud(c) {
    const e = json('cdda_ui_estado');
    if (!e) return;
    const izq = crear(`<div class="columna lista" style="display:flex;flex-direction:column;align-items:center"><h3>${T('cuerpo')}</h3>${figura(e.cuerpo, 280)}</div>`);
    const der = crear(`<div class="columna detalle"><h3>${T('cuerpo')}</h3></div>`);
    for (const p of e.cuerpo) {
      const f = p.max > 0 ? p.vida / p.max : 0;
      der.appendChild(crear(`<div style="margin-bottom:8px"><div class="dato"><span>${esc(p.nombre)}${p.sangra ? ` · <span class="t-mal">${T('sangra')}</span>` : ''}${p.rota ? ` · <span class="t-mal">${T('rota')}</span>` : ''}</span><span>${p.vida} / ${p.max}</span></div><div class="barra"><div style="width:${Math.round(f * 100)}%;background:${colorVida(f)}"></div></div></div>`));
    }
    der.appendChild(crear(`<h3>${T('necesidades')}</h3>`));
    for (const n of e.necesidades) der.appendChild(crear(`<div style="margin-bottom:8px"><div class="dato"><span><i class="fa-solid ${ICONOS_NEC[n.id]}"></i> ${T(n.id)}</span><span style="color:${color(n.color)}">${esc(textoNecesidad(n))}</span></div><div class="barra"><div style="width:${Math.round(n.barra * 100)}%;background:${color(n.color)}"></div></div></div>`));
    c.append(izq, der);
  }
  function pintarPersonaje(c) {
    const p = json('cdda_ui_personaje');
    if (!p) return;
    const col = crear(`<div class="columna entera"></div>`);
    col.appendChild(crear(`<h2>${esc(p.nombre)}</h2>`));
    col.appendChild(crear(`<h3>${T('atributos')}</h3>`));
    col.appendChild(crear(`<div class="atributos">${['fuerza', 'destreza', 'inteligencia', 'percepcion'].map((k) => `<div class="atributo"><b>${p[k]}</b><span>${T(k)}</span></div>`).join('')}</div>`));
    col.appendChild(crear(`<h3>${T('habilidades')}</h3>`));
    for (const h of p.habilidades.sort((a, b) => b.nivel - a.nivel || a.nombre.localeCompare(b.nombre))) {
      col.appendChild(crear(`<div class="habilidad"><span style="${h.nivel ? '' : 'color:var(--tenue)'}">${esc(h.nombre)}</span><div class="barra"><div style="width:${Math.min(100, h.nivel * 10)}%;background:var(--acento)"></div></div><span>${h.nivel}</span></div>`));
    }
    col.appendChild(crear(`<h3>${T('competencias')} (${p.competencias.length})</h3>`));
    col.appendChild(crear(`<div class="chips">${p.competencias.map((x) => `<span class="etiqueta">${esc(x)}</span>`).join('') || `<span class="vacio">${T('ninguna')}</span>`}</div>`));
    c.appendChild(col);
  }

  // --- el mapa del mundo
  let zoomMapa = 18;
  const PALETA = [
    [/^(forest_thick|forest_water)/, '#22401f', 'bosque'], [/^forest/, '#2f5a2a', 'bosque'], [/^(field|grass|meadow)/, '#5f7d35', 'campo'],
    [/^(river|lake|pond|water|bay|ocean)/, '#2a5b8a', 'agua'], [/^(road|hiway|highway|bridge|roadstop)/, '#6b6b6b', 'carretera'],
    [/^(swamp|marsh)/, '#46573a', 'campo'], [/(shelter|evac)/, '#c9a227', 'refugio'], [/^(open_air|empty_rock|rock)/, '#1a1a1a', null],
  ];
  const colorTerreno = (tipo) => { for (const [re, c] of PALETA) if (re.test(tipo)) return c; return '#8a6a48'; };
  function pintarMapa(c) {
    const caja = crear(`<div class="mapa-caja">
      <div class="mapa-barra"><span class="lugar"></span><button class="boton menos" title="${T('mapa_lejos')}"><i class="fa-solid fa-minus"></i></button><button class="boton mas" title="${T('mapa_cerca')}"><i class="fa-solid fa-plus"></i></button></div>
      <canvas></canvas>
      <div class="leyenda">${[['bosque', '#2f5a2a'], ['campo', '#5f7d35'], ['agua', '#2a5b8a'], ['carretera', '#6b6b6b'], ['edificio', '#8a6a48'], ['refugio', '#c9a227'], ['sin_explorar', '#15171c']].map(([k, col]) => `<span style="--c:${col}">${T(k)}</span>`).join('')}</div></div>`);
    c.appendChild(caja);
    const lienzo = $('canvas', caja), lugar = $('.lugar', caja);
    const radio = 40;
    const m = json('cdda_ui_mapa', radio);
    if (!m) return;
    const lado = 2 * radio + 1;
    let desX = 0, desY = 0;
    const dibujar = () => {
      const w = lienzo.clientWidth, h = lienzo.clientHeight;
      lienzo.width = w * devicePixelRatio; lienzo.height = h * devicePixelRatio;
      const ctx = lienzo.getContext('2d');
      ctx.scale(devicePixelRatio, devicePixelRatio);
      ctx.fillStyle = '#0b0d11'; ctx.fillRect(0, 0, w, h);
      const t = zoomMapa;
      const ox = w / 2 - (radio + 0.5) * t + desX, oy = h / 2 - (radio + 0.5) * t + desY;
      ctx.font = `${Math.floor(t * 0.7)}px Terminus, monospace`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      for (let i = 0; i < m.casillas.length; i++) {
        const x = ox + (i % lado) * t, y = oy + Math.floor(i / lado) * t;
        if (x < -t || y < -t || x > w || y > h) continue;
        const cel = m.casillas[i];
        if (!cel) { ctx.fillStyle = '#15171c'; ctx.fillRect(x, y, t, t); continue; }
        const [tipo, , sim, col] = cel;
        ctx.fillStyle = colorTerreno(tipo);
        ctx.fillRect(x, y, t - (t > 8 ? 1 : 0), t - (t > 8 ? 1 : 0));
        if (t >= 14 && !/^(forest|field|river|lake|open_air|empty_rock)/.test(tipo)) { ctx.fillStyle = color(col); ctx.fillText(sim, x + t / 2, y + t / 2 + 1); }
      }
      // donde estás
      const px = ox + radio * t + t / 2, py = oy + radio * t + t / 2;
      ctx.strokeStyle = '#fff'; ctx.lineWidth = 2; ctx.beginPath(); ctx.arc(px, py, Math.max(5, t * 0.45), 0, Math.PI * 2); ctx.stroke();
      ctx.fillStyle = '#e0a84f'; ctx.beginPath(); ctx.arc(px, py, Math.max(3, t * 0.25), 0, Math.PI * 2); ctx.fill();
      lugar.textContent = `${T('aqui')}: ${limpio((m.casillas[radio * lado + radio] || [])[1] || '')}`;
    };
    lienzo.onmousemove = (e) => {
      const r = lienzo.getBoundingClientRect(), t = zoomMapa;
      const ox = r.width / 2 - (radio + 0.5) * t + desX, oy = r.height / 2 - (radio + 0.5) * t + desY;
      const cx = Math.floor((e.clientX - r.left - ox) / t), cy = Math.floor((e.clientY - r.top - oy) / t);
      if (arrastre) { desX += e.movementX; desY += e.movementY; dibujar(); return; }
      if (cx < 0 || cy < 0 || cx >= lado || cy >= lado) return;
      const cel = m.casillas[cy * lado + cx];
      lugar.textContent = cel ? limpio(cel[1]) : T('sin_explorar');
    };
    let arrastre = false;
    lienzo.onmousedown = () => { arrastre = true; };
    window.addEventListener('mouseup', () => { arrastre = false; });
    lienzo.onwheel = (e) => { e.preventDefault(); zoomMapa = Math.max(6, Math.min(40, zoomMapa + (e.deltaY < 0 ? 2 : -2))); dibujar(); };
    $('.mas', caja).onclick = () => { zoomMapa = Math.min(40, zoomMapa + 4); dibujar(); };
    $('.menos', caja).onclick = () => { zoomMapa = Math.max(6, zoomMapa - 4); dibujar(); };
    requestAnimationFrame(dibujar);
  }

  // ------------------------------------------------------------------ menú de una casilla, coger, diálogo
  const ICONOS_ACCION = { ir: 'fa-person-walking', abrir: 'fa-door-open', cerrar: 'fa-door-closed', coger: 'fa-hand', beber: 'fa-glass-water', pescar: 'fa-fish', examinar: 'fa-magnifying-glass', vehiculo: 'fa-car', hablar: 'fa-comments', atacar: 'fa-hand-fist', mirar: 'fa-eye' };
  const NOMBRE_ACCION = { ir: 'ir', abrir: 'abrir', cerrar: 'cerrar_puerta', coger: 'coger', beber: 'beber_llenar', pescar: 'pescar', examinar: 'examinar', vehiculo: 'vehiculo', hablar: 'hablar', atacar: 'atacar', mirar: 'mirar' };
  const menuCasilla = crear(`<div id="menu-casilla" class="ui oculto"></div>`);
  const cerrarMenuCasilla = () => menuCasilla.classList.add('oculto');
  function abrirMenuCasilla(cx, cy, cas) {
    const que = [cas.criatura, cas.mueble, cas.vehiculo, cas.terreno].filter(Boolean)[0] || '';
    const extra = cas.numObjetos ? `${cas.numObjetos} ${T('objetos')}: ${cas.objetos.slice(0, 3).map(limpio).join(', ')}${cas.numObjetos > 3 ? '…' : ''}` : (cas.mueble ? cas.terreno : '');
    menuCasilla.innerHTML = `<div class="que"><div><b>${esc(que)}</b>${extra ? `<small>${esc(extra)}</small>` : ''}</div></div>`;
    for (const a of cas.acciones) {
      const b = crear(`<button class="accion"><i class="fa-solid ${ICONOS_ACCION[a.id] || 'fa-circle'}"></i>${T(NOMBRE_ACCION[a.id] || a.id)}</button>`);
      b.onclick = () => {
        cerrarMenuCasilla();
        if (a.id === 'coger') abrirCoger(cas.dx, cas.dy);
        else ordenar({ a: a.id, dx: cas.dx, dy: cas.dy });
        document.getElementById('canvas').focus();
      };
      menuCasilla.appendChild(b);
    }
    menuCasilla.classList.remove('oculto');
    const w = menuCasilla.offsetWidth, h = menuCasilla.offsetHeight;
    menuCasilla.style.left = `${Math.min(cx + 8, window.innerWidth - w - 8)}px`;
    menuCasilla.style.top = `${Math.min(cy + 8, window.innerHeight - h - 8)}px`;
  }
  const ventanaCoger = crear(`<div id="ventana-coger" class="ventana ui oculto"><div class="cabeza"><i class="fa-solid fa-hand"></i><span class="titulo"></span><button class="cerrar"><i class="fa-solid fa-xmark"></i></button></div><div class="cuerpo"></div><div class="pie"></div></div>`);
  $('.cerrar', ventanaCoger).onclick = () => ventanaCoger.classList.add('oculto');
  function abrirCoger(dx, dy) {
    const s = json('cdda_ui_suelo', dx, dy);
    if (!s) return;
    $('.titulo', ventanaCoger).textContent = `${T('coger_titulo')} · ${limpio(s.lugar)}`;
    const cu = $('.cuerpo', ventanaCoger), pie = $('.pie', ventanaCoger);
    cu.innerHTML = ''; pie.innerHTML = '';
    for (const o of s.objetos) {
      const f = crear(`<label class="casilla-coger"><input type="checkbox" checked data-i="${o.indice}"><span class="nombre" style="flex:1">${esc(o.nombre)}${o.cantidad > 1 ? ` ×${o.cantidad}` : ''}</span><small style="color:var(--suave)">${(o.peso / 1000).toFixed(2)} kg</small></label>`);
      f.insertBefore(marco(o.tipo, 'item', o.variante, 28), f.children[1]);
      cu.appendChild(f);
    }
    if (!s.objetos.length) cu.appendChild(crear(`<div class="vacio">${T('suelo_vacio')}</div>`));
    const todos = crear(`<button class="boton">${T('todos')}</button>`), ninguno = crear(`<button class="boton">${T('ninguno')}</button>`);
    todos.onclick = () => $$('input', cu).forEach((x) => { x.checked = true; });
    ninguno.onclick = () => $$('input', cu).forEach((x) => { x.checked = false; });
    const coger = crear(`<button class="boton principal">${T('coger_sel')}</button>`);
    coger.onclick = () => {
      const objetos = $$('input', cu).filter((x) => x.checked).map((x) => ({ indice: Number(x.dataset.i), cantidad: 0 }));
      if (objetos.length) ordenar({ a: 'coger_objetos', dx, dy, objetos });
      ventanaCoger.classList.add('oculto');
    };
    pie.append(todos, ninguno, coger);
    ventanaCoger.classList.remove('oculto');
  }
  // el diálogo: el mundo sigue mientras se habla
  const dialogo = crear(`<div id="dialogo" class="ventana ui oculto"><div class="cabeza"><i class="fa-solid fa-comments"></i><span class="titulo"></span><span class="vivo"></span><button class="cerrar"><i class="fa-solid fa-xmark"></i></button></div><div class="historia"></div><div class="respuestas"></div></div>`);
  $('.cerrar', dialogo).onclick = () => ordenar({ a: 'cerrar_dialogo' });
  let firmaDialogo = '';
  function pintarDialogo() {
    const d = json('cdda_ui_dialogo');
    if (!d) { dialogo.classList.add('oculto'); firmaDialogo = ''; return; }
    const firma = JSON.stringify([d.historia.length, d.linea, d.respuestas.length]);
    dialogo.classList.remove('oculto');
    if (firma === firmaDialogo) return;
    firmaDialogo = firma;
    $('.titulo', dialogo).textContent = limpio(d.npc);
    $('.vivo', dialogo).textContent = T('el_mundo_sigue');
    const h = $('.historia', dialogo);
    h.innerHTML = d.historia.map(([quien, que]) => `<p class="${quien === 'Tú' ? 'yo' : ''}">${quien ? `<b>${esc(quien === 'Tú' ? (idioma === 'es' ? 'Tú' : 'You') : quien)}:</b> ` : ''}${esc(que)}</p>`).join('');
    h.scrollTop = h.scrollHeight;
    const r = $('.respuestas', dialogo);
    r.innerHTML = '';
    d.respuestas.forEach((x, i) => {
      const b = crear(`<button>${esc(x.texto)}</button>`);
      b.disabled = !x.vale;
      b.onclick = () => ordenar({ a: 'responder', i });
      r.appendChild(b);
    });
  }

  // ------------------------------------------------------------------ opciones (nuestro menú, arriba a la derecha)
  const botonera = crear(`<div id="botonera" class="ui oculto"><button class="boton-redondo opciones"><i class="fa-solid fa-sliders"></i></button><button class="boton-redondo principal menu"><i class="fa-solid fa-bars"></i></button></div>`);
  $('.menu', botonera).onclick = () => (panel.classList.contains('oculto') ? abrirPanel() : cerrarPanel());
  const ventanaOpciones = crear(`<div id="ventana-opciones" class="ventana ui oculto"><div class="cabeza"><i class="fa-solid fa-sliders"></i><span class="titulo"></span><button class="cerrar"><i class="fa-solid fa-xmark"></i></button></div><div class="cuerpo"></div></div>`);
  $('.opciones', botonera).onclick = () => { pintarOpciones(); ventanaOpciones.classList.toggle('oculto'); };
  $('.cerrar', ventanaOpciones).onclick = () => ventanaOpciones.classList.add('oculto');
  function pintarOpciones() {
    $('.titulo', ventanaOpciones).textContent = T('opciones');
    const cu = $('.cuerpo', ventanaOpciones);
    cu.innerHTML = `<h3>${T('idioma')}</h3><div class="opciones-sexo"><button class="boton es ${idioma === 'es' ? 'activa' : ''}">Español</button><button class="boton en ${idioma === 'en' ? 'activa' : ''}">English</button></div>
      <h3>${T('pantalla_completa')}</h3><button class="boton completa"><i class="fa-solid fa-expand"></i> ${T('pantalla_completa')}</button>
      <h3>${T('guardar')}</h3><div class="botones"><button class="boton guardar"><i class="fa-solid fa-floppy-disk"></i> ${T('guardar')}</button><button class="boton salir"><i class="fa-solid fa-door-open"></i> ${T('salir')}</button></div>`;
    $('.es', cu).onclick = () => { ponerIdioma('es'); pintarOpciones(); };
    $('.en', cu).onclick = () => { ponerIdioma('en'); pintarOpciones(); };
    $('.completa', cu).onclick = () => { if (window.screenfull) screenfull.toggle(); else if (document.fullscreenElement) document.exitFullscreen(); else document.documentElement.requestFullscreen(); };
    $('.guardar', cu).onclick = () => { ordenar({ a: 'guardar' }); aviso(T('guardado'), 0); ventanaOpciones.classList.add('oculto'); };
    $('.salir', cu).onclick = () => { ordenar({ a: 'salir' }); ventanaOpciones.classList.add('oculto'); };
  }

  // ------------------------------------------------------------------ pantalla de inicio y de muerte
  const inicio = crear(`<div id="inicio" class="ui oculto"><div class="caja"></div></div>`);
  let vistaInicio = 'menu', esHombre = true, esperandoPartida = false;
  inicio.addEventListener('keydown', (e) => e.stopPropagation());
  inicio.addEventListener('keyup', (e) => e.stopPropagation());
  function pintarInicio() {
    const caja = $('.caja', inicio);
    const muerte = json('cdda_ui_muerte');
    let html = `<h1>${T('titulo')}</h1><div class="sub">${T('subtitulo')}</div>`;
    if (muerte && vistaInicio === 'menu') {
      html += `<div class="muerte"><h2><i class="fa-solid fa-skull"></i> ${T('has_muerto')}</h2><div>${esc(muerte.nombre)} · ${T('sobreviviste')} ${muerte.dias} ${T('dias')} ${muerte.horas} ${T('horas')}</div>
        <div class="descripcion">${muerte.mensajes.slice(-4).map(esc).join('<br>')}</div></div>`;
    }
    if (esperandoPartida) {
      html += `<div class="cargando"><i class="fa-solid fa-spinner fa-spin"></i> ${T('cargando')}</div>`;
    } else if (vistaInicio === 'menu') {
      html += `<div class="botones" style="flex-direction:column"><button class="boton principal grande nueva"><i class="fa-solid fa-play"></i> ${T('nueva')}</button><button class="boton grande cargar"><i class="fa-solid fa-folder-open"></i> ${T('cargar')}</button></div>`;
    } else if (vistaInicio === 'nueva') {
      html += `<label>${T('nombre')}</label><input type="text" class="nombre" maxlength="30" placeholder="${T('nombre_ph')}">
        <label>${T('sexo')}</label><div class="opciones-sexo"><button class="boton h ${esHombre ? 'activa' : ''}"><i class="fa-solid fa-mars"></i> ${T('hombre')}</button><button class="boton m ${!esHombre ? 'activa' : ''}"><i class="fa-solid fa-venus"></i> ${T('mujer')}</button></div>
        <div class="botones"><button class="boton volver">${T('volver')}</button><button class="boton principal grande empezar"><i class="fa-solid fa-play"></i> ${T('empezar')}</button></div>`;
    } else if (vistaInicio === 'cargar') {
      const mundos = json('cdda_ui_partidas') || [];
      const partidas = mundos.flatMap((m) => m.partidas.map((p) => ({ mundo: m.mundo, partida: p })));
      html += partidas.map((p, i) => `<div class="partida" data-i="${i}"><i class="fa-solid fa-user"></i><div style="flex:1"><b>${esc(p.partida)}</b><br><small style="color:var(--suave)">${esc(p.mundo)}</small></div><i class="fa-solid fa-chevron-right"></i></div>`).join('') || `<div class="vacio">${T('sin_partidas')}</div>`;
      html += `<div class="botones"><button class="boton volver">${T('volver')}</button></div>`;
      caja.innerHTML = html + idiomasHtml();
      $$('.partida', caja).forEach((d) => d.onclick = () => { const p = partidas[Number(d.dataset.i)]; pedirMenu({ a: 'cargar', mundo: p.mundo, partida: p.partida }); });
      enganchar(caja);
      return;
    }
    caja.innerHTML = html + idiomasHtml();
    enganchar(caja);
  }
  const idiomasHtml = () => `<div class="idiomas"><button class="boton es">Español</button><button class="boton en">English</button></div>`;
  function enganchar(caja) {
    const on = (sel, f) => { const el = $(sel, caja); if (el) el.onclick = f; };
    on('.nueva', () => { vistaInicio = 'nueva'; pintarInicio(); });
    on('.cargar', () => { vistaInicio = 'cargar'; pintarInicio(); });
    on('.volver', () => { vistaInicio = 'menu'; pintarInicio(); });
    on('.h', () => { esHombre = true; pintarInicio(); });
    on('.m', () => { esHombre = false; pintarInicio(); });
    on('.empezar', () => pedirMenu({ a: 'nueva', nombre: ($('.nombre', caja).value || '').trim(), hombre: esHombre }));
    on('.es', () => { ponerIdioma('es'); pintarInicio(); });
    on('.en', () => { ponerIdioma('en'); pintarInicio(); });
  }
  function pedirMenu(o) {
    const n = alBufer(JSON.stringify(o));
    if (n < 0) return;
    ex().cdda_ui_menu_principal(n);
    esperandoPartida = true;
    pintarInicio();
  }

  // ------------------------------------------------------------------ clic en el mapa y teclas
  function clicEnMapa(e) {
    const canvas = document.getElementById('canvas');
    if (e.target !== canvas || !listo() || !enPartida || ventanas() > 1 || e.button !== 0) return;
    const r = canvas.getBoundingClientRect();
    const px = Math.round((e.clientX - r.left) * canvas.width / r.width), py = Math.round((e.clientY - r.top) * canvas.height / r.height);
    const pos = json('cdda_ui_casilla_en_pixel', px, py);
    if (!pos) return;
    e.stopPropagation(); e.preventDefault();
    if (e.type !== 'mousedown') return;
    const cas = json('cdda_ui_casilla', pos.dx, pos.dy);
    if (cas && cas.acciones && cas.acciones.length) abrirMenuCasilla(e.clientX, e.clientY, cas); else cerrarMenuCasilla();
  }
  const DIRECCIONES = { ArrowUp: [0, -1], ArrowDown: [0, 1], ArrowLeft: [-1, 0], ArrowRight: [1, 0], Numpad8: [0, -1], Numpad2: [0, 1], Numpad4: [-1, 0], Numpad6: [1, 0], Numpad7: [-1, -1], Numpad9: [1, -1], Numpad1: [-1, 1], Numpad3: [1, 1] };
  const pulsadas = new Set();
  let direccionEnviada = '0,0';
  function enviarDireccion() {
    let dx = 0, dy = 0;
    for (const k of pulsadas) { const [x, y] = DIRECCIONES[k]; dx += x; dy += y; }
    dx = Math.max(-1, Math.min(1, dx)); dy = Math.max(-1, Math.min(1, dy));
    const d = `${dx},${dy}`;
    if (d === direccionEnviada) return;
    direccionEnviada = d;
    ordenar({ a: 'mantener', dx, dy });
  }
  function teclaDireccion(e) {
    if (!DIRECCIONES[e.code] || !listo() || !enPartida) return;
    if (e.target && /INPUT|TEXTAREA/.test(e.target.tagName)) return;
    if (ventanas() > 1) { if (pulsadas.size) { pulsadas.clear(); enviarDireccion(); } return; }
    e.preventDefault(); e.stopPropagation();
    if (e.type === 'keydown') pulsadas.add(e.code); else pulsadas.delete(e.code);
    enviarDireccion();
  }

  // ------------------------------------------------------------------ arranque y refresco
  let montado = false, enPartida = false, ultimoEstado = null;
  function montar() {
    document.body.append(hud, actividad, avisos, botonera, panel, menuCasilla, ventanaCoger, ventanaObjeto, dialogo, ventanaOpciones, inicio);
    window.addEventListener('mousedown', (e) => { if (!menuCasilla.contains(e.target)) cerrarMenuCasilla(); }, true);
    ['mousedown', 'mouseup', 'click'].forEach((t) => window.addEventListener(t, clicEnMapa, true));
    window.addEventListener('keydown', teclaDireccion, true);
    window.addEventListener('keyup', teclaDireccion, true);
    window.addEventListener('blur', () => { pulsadas.clear(); if (enPartida) enviarDireccion(); });
    window.addEventListener('keydown', (e) => {
      if (!enPartida || ventanas() > 1) return;
      if (e.target && /INPUT|TEXTAREA/.test(e.target.tagName)) return;
      if (e.key === 'Tab') { e.preventDefault(); e.stopPropagation(); panel.classList.contains('oculto') ? abrirPanel() : cerrarPanel(); return; }
      // las teclas de siempre del juego abren lo nuestro, que no para el mundo (las suyas son menús que lo paran)
      const atajos = { i: 'inventario', '&': 'fabricar', '*': 'construir', m: 'mapa', '@': 'personaje' };
      if (atajos[e.key] && !e.ctrlKey && !e.altKey) {
        e.preventDefault(); e.stopPropagation();
        if (!panel.classList.contains('oculto') && pestana === atajos[e.key]) cerrarPanel(); else abrirPanel(atajos[e.key]);
        return;
      }
      if (e.key === 'g' && !e.ctrlKey) { e.preventDefault(); e.stopPropagation(); abrirCoger(0, 0); return; }
      if (e.key === 'C' && !e.ctrlKey) {
        e.preventDefault(); e.stopPropagation();
        for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [-1, -1], [1, -1], [-1, 1]]) {
          const cas = json('cdda_ui_casilla', dx, dy);
          if (cas && cas.acciones && cas.acciones.some((a) => a.id === 'hablar')) { ordenar({ a: 'hablar', dx, dy }); break; }
        }
        return;
      }
      if (e.key === 'Escape' && menuCasilla.classList.contains('oculto') && ventanaCoger.classList.contains('oculto') && ventanaObjeto.classList.contains('oculto') && panel.classList.contains('oculto')) {
        // (el menú de Escape del juego para el mundo: aquí, nuestras opciones)
        e.preventDefault(); e.stopPropagation();
        pintarOpciones(); ventanaOpciones.classList.toggle('oculto');
        return;
      }
      if (e.key === 'Escape' && (!menuCasilla.classList.contains('oculto') || !ventanaCoger.classList.contains('oculto') || !ventanaObjeto.classList.contains('oculto') || !ventanaOpciones.classList.contains('oculto') || !panel.classList.contains('oculto'))) {
        e.preventDefault(); e.stopPropagation();
        [menuCasilla, ventanaCoger, ventanaObjeto, ventanaOpciones].forEach((x) => x.classList.add('oculto'));
        cerrarPanel();
      }
    }, true);
    window.addEventListener('cambioidioma', () => { firmaMano = ''; if (!panel.classList.contains('oculto')) abrirPestana(pestana); });
    // la rueda de ajustes de play-cdda: fuera (las opciones son las nuestras)
    const tuerca = document.getElementById('settings-gear'); if (tuerca) tuerca.remove();
    const menuTuerca = document.getElementById('settings-menu'); if (menuTuerca) menuTuerca.remove();
    leerIdioma();
  }
  let menuListo = false;
  window.addEventListener('menuready', () => { menuListo = true; });
  function refrescar() {
    if (!listo()) return;
    if (!montado) { montar(); montado = true; }
    const e = json('cdda_ui_estado');
    const antes = enPartida;
    enPartida = !!e;
    if (!e) {
      // en el menú principal: nuestra pantalla de inicio (o de muerte)
      [hud, actividad, botonera, panel, menuCasilla, ventanaCoger, ventanaObjeto, dialogo, ventanaOpciones].forEach((x) => x.classList.add('oculto'));
      if (menuListo) {
        if (inicio.classList.contains('oculto') || antes) { vistaInicio = 'menu'; esperandoPartida = false; pintarInicio(); }
        inicio.classList.remove('oculto');
      }
      return;
    }
    if (!antes) { inicio.classList.add('oculto'); esperandoPartida = false; primerosMensajes = true; totalVisto = -1; }
    if (e.idioma && e.idioma !== idioma) { idioma = e.idioma; }
    ultimoEstado = e;
    // (con una ventana del juego abierta, lo nuestro se aparta para no taparla)
    const ventanaDelJuego = ventanas() > 1;
    [hud, botonera].forEach((x) => x.classList.toggle('oculto', ventanaDelJuego));
    if (ventanaDelJuego) { cerrarMenuCasilla(); actividad.classList.add('oculto'); return; }
    pintarHud(e);
    pintarActividad(e);
    actualizarMensajes();
    pintarDialogo();
  }
  setInterval(refrescar, 250);
  window.interfazCdda = { abrirPanel, cerrarPanel, abrirPestana, ordenar, json, jsonCon, icono, ponerIdioma };
})();
