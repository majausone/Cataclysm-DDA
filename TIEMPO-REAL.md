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

### Web (Emscripten 6.0.8, en WSL)

La que se mira y se prueba en el navegador. Ver `tools/tiempo-real/` para los guiones.

## Pruebas

- `./tests/cata_test.exe "[realtime]"`: el reloj (N turnos por segundo a xN, pausa, máx, ir con retraso sin deuda,
  cambiar de velocidad...).
- `./tests/cata_test.exe "[realtime_bench]"`: cuánto tarda un turno con más o menos carga.

## Medidas

(Pendiente.)
