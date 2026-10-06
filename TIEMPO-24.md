# 24 tics por segundo: qué va a x24 y qué a velocidad real

Encargo 7 (A). El juego pasa a **24 tics por segundo real**, y cada tic es **1 segundo del mundo**: un día
de juego dura **1 hora real**. Pero no todo puede ir 24 veces más deprisa: si andar, pelear o el fuego
fueran a x24, no se podría jugar. Así que hay dos relojes:

- **El reloj del mundo (x24):** lo que pasa «solo», sin que lo veas ni reacciones: la hora, el hambre, el
  sueño, que se pudra la comida, que crezcan los cultivos, las actividades largas.
- **El reloj de la acción (velocidad real):** lo que ves y a lo que reaccionas: andar, pelear, disparar,
  los monstruos y los NPC, el fuego y el humo, conducir, sangrar, el aguante. Va como si fuera un turno por
  segundo real, como en la fase 1 a x1, repartido entre los 24 tics.

Es lo que hace Project Zomboid: el día dura una hora (por defecto), pero andar y pelear van a su paso; y
en las dudas se ha hecho como allí.

## Cómo funciona

- **Un tic es un turno del juego de siempre** (`game::do_turn`, 1 segundo de `calendar::turn`). Todo lo
  que el juego hace por turno sigue haciéndose por tic, y así el reloj del mundo va solo a x24 sin tocar
  nada: hambre, sed, sueño, temperatura, pudrirse, cultivos, clima... ya van por segundos de juego.
- **Los puntos de movimiento se reparten entre los 24 tics.** En el juego, cada turno una criatura recibe
  tantos puntos como su velocidad (100 una persona normal) y andar una casilla cuesta unos 100: una casilla
  por turno. Ahora recibe la 24.ª parte en cada tic (sin perder restos: en 24 tics, exactamente su
  velocidad). Andar una casilla sigue costando lo mismo, así que se anda una casilla por segundo real, y lo
  mismo pelear, disparar, recargar... Igual para el jugador, los monstruos y los NPC.
- **Al pulsar, la acción se hace en el siguiente tic** (como mucho 42 ms), si el personaje tiene puntos
  para ella; si acaba de hacer otra, cuando los recupere, como siempre.
- **Las actividades largas van al reloj del mundo.** Fabricar, leer, construir, desmontar, esperar,
  dormir... cuestan su tiempo en segundos del mundo: fabricar algo de 10 minutos tarda 10 minutos de juego,
  25 segundos reales. Para eso, quien está en una actividad larga (el jugador o un NPC) recibe en cada tic
  la velocidad entera, como antes.
- **Lo de la acción que el juego hace «por turno» se hace una vez cada 24 tics** (repartido: no todo en el
  mismo tic), o con sus probabilidades divididas entre 24. Es lo de la tabla de abajo marcado «real».

## La tabla

**Mundo** = va a x24 (como está: por tic). **Real** = va a velocidad real (frenado x24).

### El personaje

| Proceso | Reloj | Cómo |
|---|---|---|
| Andar, correr, nadar, trepar | real | puntos de movimiento repartidos entre los 24 tics |
| Pelear cuerpo a cuerpo, disparar, lanzar, recargar | real | lo mismo (cuestan puntos) |
| Coger, soltar, ponerse, comer y beber (lo que es una acción corta) | real | lo mismo |
| Actividades largas: fabricar, leer, construir, desmontar, esperar, rebuscar, cavar... | mundo | velocidad entera por tic mientras dura la actividad |
| Dormir | mundo | velocidad entera por tic; el sueño, por tic |
| Hambre, sed, cansancio, sueño acumulado | mundo | por tic (como estaba) |
| Temperatura del cuerpo, mojarse, secarse | mundo | por tic |
| Curarse (heridas, huesos), regenerar | mundo | por tic |
| Ánimo, adicciones, radiación, enfermedades | mundo | por tic |
| Aguante (gastar y recuperar) | real | una vez cada 24 tics |
| Sangrar (el efecto de sangrado) | real | sus efectos, una vez cada 24 tics |
| Efectos de combate: aturdido, derribado, mareado, agarrado, ardiendo, cegado por un fogonazo, empujado | real | su duración baja una vez cada 24 tics |
| Efectos largos: drogas, comida, enfermedad, gripe, infección... | mundo | por tic |

### Monstruos y NPC

| Proceso | Reloj | Cómo |
|---|---|---|
| Moverse, atacar, huir | real | su velocidad entera una vez por segundo real, cada uno en su tic (repartidos entre los 24 para que no se muevan todos a la vez). No 1/24 en cada tic: lo que en el juego acaba el turno (hablar, esperar) les costaría solo 1/24 de segundo |
| Ataques especiales (sus esperas) | real | la espera baja una vez cada 24 tics |
| NPC en una actividad larga (fabricar, construir, dormir) | mundo | velocidad entera por tic |
| NPC viajando por el mapa grande (fuera de la vista) | mundo | por tic (como estaba) |
| Hordas que se mueven por el mapa grande | mundo | por tic (como estaba) |
| Necesidades de los NPC (hambre, sueño) | mundo | por tic |
| Que los monstruos crezcan, críen, se reproduzcan | mundo | por tic |

### El mapa

| Proceso | Reloj | Cómo |
|---|---|---|
| Fuego, humo, gases, ácido, sangre que se seca... (los «campos») | real | se procesan una vez cada 24 tics |
| Lo que emiten muebles y terreno (humo de una hoguera, vapor) | real | una vez cada 24 tics |
| Explosiones, proyectiles, caídas | real | son instantáneas: igual |
| Olor (rastro que siguen los monstruos) | real | una vez cada 24 tics (si no, se borraría 24 veces más deprisa de lo que andan) |
| Ruidos | real | por tic (son instantáneos) |
| Vehículos en marcha (moverse, girar, chocar) | real | una vez cada 24 tics |
| Combustible y batería de un vehículo en marcha | real | va con el movimiento |
| Comida que se pudre, objetos que se secan o se enfrían | mundo | por tic |
| Cultivos, plantas, árboles | mundo | por tic |
| Clima, lluvia, temperatura del aire, día y noche | mundo | por tic |
| Hogueras y hornos que consumen combustible | mundo | por tic (el fuego como «campo» va con los campos) |
| Misiones, eventos programados, guardado automático | mundo | por tic |

## Dudas y cómo se han resuelto

- **¿El fuego, real o mundo?** Real, como en Zomboid: lo ves crecer y reaccionas. Pero que una hoguera
  gaste leña va con el mundo: una noche de hoguera gasta lo de una noche.
- **¿Comer?** Coger y llevarse algo a la boca es una acción (real); lo que tarda en hacer efecto, mundo.
- **¿Curarse?** Mundo (como Zomboid: las heridas tardan días de juego). Vendarse es una actividad.
- **¿Sangrar?** Real: una herida que sangra en una pelea tiene que dar tiempo a reaccionar.
- **¿Conducir?** Real: el coche va a su velocidad de verdad. Lo que se gasta por kilómetro, igual.
- **¿Dormir?** Mundo: una noche dura 8 horas de juego, 20 minutos reales.

## Velocidad: una sola, fija (Encargo 8)

El tiempo va **siempre** a 24 tics por segundo: un día de juego dura una hora real. No hay pausa ni se elige
la duración del día (ni F6/F7/F8 ni opción). Andar y pelear van a paso real: los puntos de movimiento del
jugador se reparten entre los tics de un segundo; NPC y monstruos reciben su velocidad entera una vez por
segundo, cada uno en su tic.

**Nada para el mundo**: con la interfaz web, el inventario, fabricar, construir, el mapa, hablar con un NPC,
las opciones... son ventanas de la página y el reloj sigue. Lo único que espera respuesta son las preguntas
cortas del juego (sus listas y «¿seguro?»), que salen en nuestra ventana.

Para probar, el modo simulación (sin ventana, src/simulacion.h) puede ir a la máxima, lo más rápido que pueda
el ordenador, para jugar partidas enteras en minutos y cazar fallos. Es una herramienta de pruebas, no una
opción del juego.

## Fuego, humo y gases

El juego los procesa por turno con sus probabilidades (extenderse, consumirse, disiparse). Se procesan una
vez por segundo real (cada 24 tics a día normal), repartidos para no caer todos en el mismo tic: en un
segundo real se extienden lo mismo que antes en un turno, que es lo equivalente a ajustar la probabilidad
por tic (p' = 1-(1-p)^(1/24)), pero sin tocar cada fórmula.
