# Cataclysm: Dark Days Ahead en tiempo real

Rama `tiempo-real` del fork [majausone/Cataclysm-DDA](https://github.com/majausone/Cataclysm-DDA) (y
`tiempo-real-web`, la misma con lo necesario para el navegador). Es el mismo juego, pero **el reloj corre solo**:
los turnos pasan al ritmo de la velocidad elegida sin esperar a que pulses. Sin animaciones nuevas ni efectos, y
sin tocar los sprites.

El código del juego es de CleverRaven y sus colaboradores, con licencia CC BY-SA 3.0; los cambios de esta rama
también. La versión web parte de los parches de [nornagon/play-cdda](https://github.com/nornagon/play-cdda).

## Cómo funciona

- **Cada turno es un segundo de juego.** A x1 pasa un turno por segundo real.
- **Si no pulsas nada** cuando se te acaba el turno, tu personaje espera ese turno y el mundo sigue.
- **Si pulsas**, la acción se hace en cuanto te toca. Lo que pulses mientras esperas a que te toque se guarda y
  se hace al empezar tu turno: no se pierden teclas.
- **Las actividades largas** (fabricar, leer, construir, esperar...) y **el sueño** van también al ritmo del
  reloj, no a toda velocidad.
- **En pausa** el juego es el de siempre: espera a que pulses, y cada acción gasta su tiempo. Las actividades y
  el sueño se quedan parados hasta que quites la pausa.
- **Los menús e inventarios paran el reloj** mientras están abiertos (son ventanas modales: el turno no avanza).
- **Peligro:** cuando aparece un peligro nuevo a la vista (lo mismo que detecta el «safe mode»), el reloj baja a
  x1. Se puede cambiar a «pausa» o «nada» en las opciones.
- **Si el ordenador no llega** a la velocidad pedida, va lo más rápido que puede, sin acumular retraso (no hay
  ráfagas para «recuperar»), y lo dice en pantalla: `x72>41` es «pedido x72, va a 41 turnos por segundo».
- En este juego el tiempo y el movimiento van juntos: a x72 tu personaje anda 72 casillas por segundo. No se
  separan, porque eso rompería la simulación (hambre, sed, cansancio...).

## Velocidades y teclas

| Velocidad | Turnos por segundo real | Para qué |
|---|---|---|
| Pausa | 0 | el juego de siempre, por turnos |
| x1 | 1 | tiempo real (por defecto) |
| x3 | 3 | |
| x10 | 10 | |
| x30 | 30 | |
| x72 | 72 | un día de juego en 20 minutos |
| Máx | lo que dé el ordenador | |

| Tecla | Qué hace |
|---|---|
| `F7` o `}` | más rápido |
| `F6` | más despacio |
| `F8` | pausa / seguir |

Se pueden cambiar en el menú de teclas del juego (`?`, «Real time: ...»). La velocidad se ve siempre en la barra
lateral, al lado de la hora (`10:24:31  x1`).

**Opciones** (Opciones → General): `Real time` (activado), `Real time: starting speed` (x1) y `Real time: on
danger` (bajar a x1).

## Compilar

### Escritorio (Windows, MSYS2 UCRT64)

Para cambiar el C++ deprisa y pasar las pruebas. Paquetes (`pacman -S --needed`):
`mingw-w64-ucrt-x86_64-{gcc,ccache,cmake,freetype,glslang,libwebp,pkgconf,sdl3,sdl3-image,sdl3-ttf,libzip,libavif,zlib}`.

```bash
export MSYSTEM=UCRT64; source /etc/profile
make -j24 CCACHE=1 RELEASE=1 MSYS2=1 DYNAMIC_LINKING=1 TILES=1 SOUND=0 LOCALIZE=0 LINTJSON=0 ASTYLE=0 TESTS=1 BACKTRACE=0
```

(Con `USERPROFILE` puesto, que ccache lo pide.) Se lanza con `./cataclysm-tiles` (con `C:\msys64\ucrt64\bin` en
el PATH).

### Web (Emscripten 6.0.8)

La que se mira y se prueba en el navegador. Rama `tiempo-real-web`: master de CleverRaven con los parches de
play-cdda (JSPI y SDL3) y, encima, los mismos cambios de `tiempo-real`. Se compila con `build-scripts/build-emscripten.sh`
(en Linux o WSL; `-Os`, que se puede cambiar con `EMSCRIPTEN_OPTLEVEL`) y se empaqueta con `build-scripts/prepare-web.sh`.
Necesita un Chrome con JSPI (de serie desde la 137). Los tilesets que haya en `gfx/` se empaquetan aparte y se
bajan al elegirlos; UltiCa (`UltimateCataclysm`, el de por defecto) no viene en el repositorio: se saca de `gfx/` del
paquete «linux-with-graphics» de una release de CleverRaven, como hace play-cdda.

La prueba en el navegador está en esa rama, `tools/tiempo-real/prueba-web.mjs` (Playwright, Chromium sin ventana):
arranca una partida y lee el estado del juego desde JS (`cdda_turno`, `cdda_hora`, `cdda_rt_velocidad`,
`cdda_rt_turnos_por_segundo`, `cdda_rt_ms_turno`, `cdda_rt_retrasado`, `cdda_ventanas`, exportadas por
`src/realtime.cpp`).

## Pruebas

- `./tests/cata_test.exe "[realtime]"`: el reloj (N turnos por segundo a xN, pausa, máx, ir con retraso sin deuda,
  recuperar las cesiones de fotograma del navegador, cambiar de velocidad...). 8 casos, 43 comprobaciones: pasan.
- `./tests/cata_test.exe "[realtime_bench]"`: cuánto tarda un turno en un mapa vacío (0,16 ms en escritorio).
- Todas las de Catch2: pasan todas salvo 6 casos que fallan igual en master sin estos cambios (o fallan a veces,
  según el orden: p. ej. la de la autopista, 1 de cada 5 en master).
- `node tools/tiempo-real/prueba-web.mjs --url http://localhost:8095/` (rama web): velocidades, el inventario para
  el reloj, la pausa con F8, andar a x1 sin que se pare el reloj y 3 minutos a x72 moviéndose.
- Modo de medida en escritorio: con `CDDA_RT_BANCO=fichero` en el entorno, el juego se maneja solo («Play Now!» y
  F7 cada 25 s: x1 → x3 → x10 → x30 → x72 → máx) y apunta cada 10 s en ese fichero a cuántos turnos por segundo va
  y cuánto tarda cada turno.

## Medidas

Ordenador: Windows 11, 24 hilos. Partida rápida («Play Now!»), en el refugio del principio.

### Turnos por segundo real a cada velocidad

| Velocidad | Escritorio | Navegador (Chromium sin ventana) |
|---|---|---|
| x1 | 1,00 | 1,00 |
| x3 | 3,00 | 3,00 |
| x10 | 10,0 | 10,0 |
| x30 | 30,0 | 30,0 |
| x72 | 72,0 | 60,4 (dice «x72>60»)* |
| Máx | ~2.500 (2.130-2.660) | 247 (108-247 según la pasada) |

* Medido con el margen de recuperación a 50 ms y el ordenador libre. Con el margen a 100 ms (lo que queda) no
se pudo repetir sin carga: el humano estaba jugando en otra pestaña y todo iba a unos 14-30 por segundo.

Lo que tarda en calcularse un turno (sin contar las esperas): en escritorio, 0,25-1,6 ms a la máxima (15 ms a x1-x10,
porque a esas velocidades se repinta la pantalla en cada turno); en el navegador, unos 23 ms a x1-x10 (se repinta en cada turno), 9-12 ms a x30-x72 y 3-4 ms a la máxima (se repinta como mucho cada 40 ms).

En el navegador, x1 a x30 van exactas y x72 se queda en unos 60. Lo que frena no es calcular el turno (a la máxima van
más de 100 por segundo) sino que el juego tiene que cederle el control al navegador para que pinte y para leer el teclado, y
cada cesión se lleva al menos un fotograma (unos 16 ms). Lo que se ha hecho para eso: a más de x10 se repinta como mucho
cada 40 ms y el teclado se mira como mucho cada 30 ms; lo poco que falta hasta el plazo de un turno se espera sin ceder; y
el reloj tiene un margen de 100 ms para recuperar: los turnos que tocaban mientras el navegador pintaba se hacen después,
seguidos (sin ese margen x72 iba a 37 por segundo). Si no llega, lo dice en pantalla (`x72>60`) y va lo más rápido que
puede, sin acumular retraso.

Las medidas del navegador varían mucho con lo que haga el ordenador a la vez: con otra pestaña del juego abierta o
compilando, todo baja a unos 14 turnos por segundo (32 ms por turno) y lo dice en pantalla igual.

La prueba web pasa entera: velocidades, el inventario para el reloj, la pausa con F8, andando a x1 va a 1,0 turnos por
segundo y 3 minutos a x72 moviéndose sin pararse ni errores.

### Pendiente

- Al hablar con un NPC («talk», la ventana de diálogo de ImGui) el juego se cuelga en la versión web. Hay que
  reproducirlo, arreglarlo, añadirlo a la prueba web y mirar las demás ventanas ImGui.

### Compilar

| | Primera vez | Un cambio pequeño |
|---|---|---|
| Escritorio (MSYS2, -j24, ccache; juego y pruebas) | 780 s | 7-57 s |
| Web (objetos en WSL y enlace en Windows) | 810 s (con parte en caché) | ~570-610 s (objetos ~210 s, enlace ~200 s) |

El enlace de la web es lento y se hace entero aunque cambie un solo fichero.
