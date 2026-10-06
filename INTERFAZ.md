# La interfaz de la versión web

Encargo 7 (B). En el navegador, el juego solo dibuja el mapa; todo lo demás es HTML y JavaScript alrededor
(rama `tiempo-real-web`: `build-data/web/interfaz.js` y `interfaz.css`), y se cambia sin recompilar el juego.

## Qué hay

- **HUD** (arriba a la izquierda, plegable): la hora y el día de la estación; el tiempo y la temperatura;
  la velocidad (pausa y cuánto dura el día: 2 h, 1 h o 30 min); las necesidades con icono, texto y barra
  (hambre, sed, sueño, temperatura, ánimo, aguante, dolor); la salud de cada parte del cuerpo, con lo que
  sangra o está roto; y lo que llevas en la mano.
- **Mensajes** (abajo a la izquierda, recogible): con icono y color según el tipo (bueno, malo, aviso,
  información) y filtros de combate, salud y ambiente. Sin los de depuración.
- **Menú** (el botón redondo o Tab): un panel con pestañas. Mientras está abierto, el juego está en pausa,
  como con sus propios menús; al cerrarlo, sigue a su velocidad.
  - **Inventario:** lo que llevas en la mano, lo que llevas puesto y lo demás, con buscador y, en cada cosa,
    solo lo que se puede hacer con ella (comer, ponerse, quitarse, empuñar, leer, usar, soltar).
  - **Fabricar:** lo que puedes hacer ya; lo que no, en gris y con el motivo (la habilidad, la competencia o
    lo que te falta); y lo que se aprende solo con más habilidad, con candado y qué hace falta. Por categorías y
    con buscador.
  - **Construir:** lo mismo con las construcciones. Al elegir una se abre el menú del juego para escoger dónde.
  - **Salud**, **Personaje** (atributos, habilidades, competencias) y **Mapa** (abre el mapa del mundo).
- **Clic en una casilla del mapa:** un menú con lo que hay ahí y solo las acciones que tienen sentido: en una
  puerta, abrir o cerrar; con cosas en el suelo, coger; en el agua, beber o llenar, y pescar si se puede;
  en un mueble que se puede examinar, examinar; en un NPC, hablar o atacar; en un monstruo, atacar; en un
  sitio por el que se pasa, ir. Con un menú del juego abierto, el clic es del juego.
- La barra lateral y el registro de mensajes del juego no se pintan.
- En el móvil, el HUD se estrecha y el panel ocupa la pantalla.

## El enchufe (src/interfaz.h)

**Hacia fuera** (JSON, en la web `cdda_ui_*`): `estado` (lo del HUD), `mensajes(n)`, `inventario`, `recetas`,
`construcciones`, `personaje`, `casilla(dx, dy)` (lo que hay y sus acciones) y `casilla_en_pixel(x, y)` (qué
casilla hay bajo un píxel del canvas, con la misma cuenta que usa el juego para el ratón).

**Hacia dentro**, órdenes en JSON. Se guardan y se hacen en el hilo del juego al empezar el siguiente tic (o
mientras espera tu tecla, que en pausa no pasa ninguno), nunca a mitad de un menú. La página escribe la orden en
un búfer (`cdda_ui_bufer`) y llama a `cdda_ui_orden` con su longitud.

| Orden | Qué hace |
|---|---|
| `{"a":"ir","dx":3,"dy":-1}` | andar hasta esa casilla (por el camino que encuentre) |
| `{"a":"abrir"/"cerrar","dx":1,"dy":0}` | abrir o cerrar la puerta (o lo que sea) |
| `{"a":"coger","dx":..}` | coger lo que hay (el menú de coger del juego) |
| `{"a":"examinar"/"beber"/"pescar"/"vehiculo","dx":..}` | lo de examinar del mueble o del terreno |
| `{"a":"hablar"/"atacar","dx":..}` | hablar con el NPC; atacar (si está lejos, ir a su lado) |
| `{"a":"mirar"}` | mirar alrededor |
| `{"a":"objeto","id":"...","que":"comer"}` | comer, ponerse, quitarse, empuñar, leer, usar o soltar algo del inventario |
| `{"a":"fabricar","receta":"...","cantidad":1}` | fabricar |
| `{"a":"construir"}`, `{"a":"mapa"}` | el menú de construir; el mapa del mundo |
| `{"a":"velocidad","v":2}` | pausa (0) o la duración del día (1, 2, 3) |

## Pruebas

- `./tests/cata_test.exe "[interfaz]"`: el estado es JSON con lo del HUD; el inventario y una orden sobre un
  objeto; las acciones de una casilla (una puerta cerrada se abre; una piedra se coge) y abrir una puerta con
  una orden; recetas, personaje, mensajes y construcciones.
- `node tools/tiempo-real/prueba-interfaz.mjs` (rama web, Playwright): el HUD y los mensajes, el menú de una
  casilla, hablar con un NPC y salir sin que se cuelgue, las seis pestañas con el juego en pausa, 100 zombis
  alrededor y el móvil, con fotos de cada paso.
