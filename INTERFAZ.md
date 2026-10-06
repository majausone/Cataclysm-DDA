# La interfaz de la versión web

Encargos 7 (B) y 8. En el navegador, el juego solo dibuja el mapa; todo lo demás es HTML y JavaScript
alrededor (rama `tiempo-real-web`: `build-data/web/interfaz.js` e `interfaz.css`), y se cambia sin
recompilar el juego. **Nada para el mundo**: ni los menús, ni el panel, ni hablar; el tiempo va siempre a 24
tics por segundo (ver TIEMPO-24.md).

## Qué hay

- **Inicio y muerte**: nuestra pantalla, con partida nueva (nombre y sexo), cargar (los mundos y sus
  partidas) y el idioma. Al morir, el resumen (quién, cuánto sobrevivió, lo último que pasó) y lo mismo.
- **HUD** (arriba a la izquierda, plegable): hora y día; tiempo y temperatura; las necesidades con icono,
  texto y barra; una figura del cuerpo coloreada por la salud de cada parte; y lo que llevas en la mano, con
  su dibujo.
- **Barra de actividad**: al fabricar, construir, leer, dormir..., lo que haces, cuánto lleva y «Cancelar».
- **Avisos breves** arriba, para lo malo o importante.
- **Panel** (el botón redondo, Tab, o las teclas de siempre: i, &, *, m, @), con pestañas:
  - **Inventario:** un muñeco con lo que llevas puesto en cada parte y, al lado, cada bolsa con lo que hay
    dentro, todo con los dibujos del tileset. Al pulsar una cosa, su ficha con lo que se puede hacer con ella.
  - **Fabricar:** por categorías y con buscador; cada receta con su dibujo; el detalle con componentes y
    herramientas (con cuántos tienes de cuántos hacen falta), tiempo, habilidad y lo que sale. El botón solo
    si se puede; si no, el motivo. Las prácticas, en su categoría.
  - **Construir:** lo mismo; al elegir una, una rejilla de 3×3 alrededor con las casillas donde se puede, y
    se construye al pulsar una (sin el menú del juego).
  - **Salud**, **Personaje**, **Mapa** (el mapa del mundo con los dibujos de su tileset, Larwick Overmap:
    zoom, arrastrar y el nombre de cada sitio al pasar) y **Mensajes** (con filtros, y los repetidos juntos
    con «×N»).
- **Clic en una casilla del mapa:** lo que hay y solo lo que tiene sentido: abrir o cerrar, coger (nuestra
  ventana con casillas para elegir), beber o llenar, pescar, examinar, hablar, robar (nuestra ventana, con lo
  que lleva el NPC), atacar, ir (anda todo el camino) y mirar (una ficha con la descripción).
- **Hablar:** nuestra ventana de diálogo abajo; el mundo sigue mientras hablas.
- **Las listas y preguntas del juego** («¿qué haces con lo que empuñas?», «¿seguro?»...): ya no las pinta
  el juego; salen en nuestra ventana, con sus teclas de siempre o con el ratón (como el menú nativo de
  Android). Mientras una está abierta, el juego espera la respuesta: es lo único que sigue siendo modal.
- **Opciones** (arriba a la derecha o Escape): idioma (español o inglés, el juego entero y la página),
  pantalla completa, guardar, y guardar y salir. La tuerca de play-cdda ya no está.
- **Idioma:** uno solo para todo, español (la traducción del propio juego, `lang/mo/es_ES`) o inglés.
- La barra lateral, el registro de mensajes y las ventanitas de progreso del juego no se pintan.
- En el móvil, el HUD se estrecha y el panel ocupa la pantalla.

## El enchufe (src/interfaz.h, src/interfaz_menus.cpp)

**Hacia fuera** (JSON, en la web `cdda_ui_*`): `estado`, `mensajes(n)`, `inventario`, `equipo` (el muñeco y
las bolsas), `recetas`, `receta(id)`, `construcciones`, `personaje`, `casilla(dx, dy)`, `casilla_en_pixel`,
`suelo(dx, dy)`, `robo(dx, dy)`, `dialogo`, `mapa(radio)`, `atlas` (las imágenes del tileset y las del mapa)
e `iconos([...])` (qué sprite tiene cada cosa), `lista` (la lista o pregunta del juego abierta), `partidas`,
`muerte`, `idioma`.

**Hacia dentro**, órdenes en JSON (búfer `cdda_ui_bufer` y `cdda_ui_orden(n)`), que se hacen en el hilo del
juego al empezar el siguiente tic o mientras espera:

| Orden | Qué hace |
|---|---|
| `{"a":"mantener","dx":1,"dy":0}` | la dirección mantenida (flechas): anda seguido, y gira a mitad de paso |
| `{"a":"ir","dx":3,"dy":-1}` | andar hasta esa casilla |
| `{"a":"abrir"/"cerrar"/"examinar"/"beber"/"pescar","dx":..}` | lo de esa casilla |
| `{"a":"coger_objetos","dx":..,"objetos":[{"indice":0,"cantidad":0}]}` | coger lo elegido del suelo |
| `{"a":"hablar"/"responder"/"cerrar_dialogo"}`, `{"a":"robar","dx":..,"id":..}` | diálogo y robo |
| `{"a":"objeto","id":"...","que":"comer"}` | comer, ponerse, quitarse, empuñar, leer, usar o soltar |
| `{"a":"fabricar","receta":"..."}`, `{"a":"construir","grupo":..,"dx":..,"dy":..}` | fabricar y construir |
| `{"a":"cancelar_actividad"}`, `{"a":"guardar"}`, `{"a":"salir"}`, `{"a":"idioma","v":"es"}` | lo demás |

Las listas se contestan con `cdda_ui_elegir(i)` (-1, cancelar).

## Compilar (apartado G)

- Solo interfaz: `bash temp/web_interfaz.sh [prueba|buena]` (copia; basta con recargar).
- Todo: `bash temp/web_todo.sh [prueba|medir|buena]`: los .o en Windows (`temp/objetos_win.sh`, emsdk-win),
  el enlace en Windows (prueba: -O0, 4 s; medir y buena: -Os), y el paquete de datos solo si cambian data/,
  gfx/ o lang/mo. `prueba` y `medir` van al 8096 (repos/cdda-web-prueba); `buena`, al 8095 del humano.

## Pruebas

- `./tests/cata_test.exe "[interfaz]"`: el estado, el inventario y las órdenes sobre objetos, las acciones de
  una casilla, recetas, personaje, mensajes, construcciones, equipo, receta, suelo y el diálogo.
- `node tools/tiempo-real/prueba-jugar.mjs [--movil]` (rama web, Playwright): juega como una persona con
  fotos de cada paso: crea un personaje, anda (empieza al pulsar, 60 imágenes por segundo, gira a mitad de
  paso, ir con un clic sin pararse), inventario, soltar y coger, fabricar, las pestañas, hablar, idioma,
  guardar y salir, cargar y morir; contesta las preguntas del juego como lo haría una persona.
- `node tools/tiempo-real/prueba-interfaz.mjs` y `prueba-web.mjs`: el HUD, el menú de una casilla, hablar,
  las pestañas sin parar el mundo, 100 zombis a 24 tics por segundo y el móvil.
