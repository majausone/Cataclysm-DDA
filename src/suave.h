#pragma once
#ifndef CATA_SRC_SUAVE_H
#define CATA_SRC_SUAVE_H

#include "point.h"

class Creature;

// Movimiento suave (Encargo 8, B): las criaturas no saltan de casilla en casilla, se deslizan durante lo que dura su
// paso, y la cámara se desliza con el jugador. El juego sigue moviendo por casillas; esto es solo el dibujo:
//  - de cada criatura se recuerda de dónde venía y cuándo empezó el paso, y se calcula cuánto dura (según su
//    velocidad y lo que cuesta la casilla: corriendo, menos; en terreno difícil o en diagonal, lo que cueste);
//  - al pintarla, se le suma el desfase en píxeles de donde va ahora en su paso (si cambia de dirección a mitad de
//    paso, sale desde donde se la ve);
//  - y el mapa entero se desplaza con el desfase del jugador, para que la cámara le siga píxel a píxel.
// Mientras algo se mueve, el juego repinta a 60 imágenes por segundo (realtime::esperar_turno).
namespace suave
{

// ¿está activo?  (con el tiempo real y con gráficos; nunca en las pruebas)
bool activo();
// el desfase en píxeles con el que hay que pintar a la criatura ahora (0 si está quieta)
point desfase( const Creature &c, int ancho_casilla, int alto_casilla );
// el desfase de todo el mapa (la cámara sigue al jugador): el contrario del del jugador
point camara( int ancho_casilla, int alto_casilla );
// ¿se está moviendo algo que se vea?  (entonces se repinta a 60 imágenes por segundo)
bool hay_movimiento();
// al empezar cada imagen: olvida a las criaturas que ya no se ven
void nueva_imagen();

} // namespace suave

#endif // CATA_SRC_SUAVE_H
