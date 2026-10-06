#pragma once
#ifndef CATA_SRC_SIMULACION_H
#define CATA_SRC_SIMULACION_H

// Modo simulación (rama tiempo-real): el juego juega solo para cazar fallos, sin nadie delante. Solo en escritorio.
// Con CDDA_SIM=fichero en el entorno:
//  - arranca una partida («Play Now!»), pone la máxima velocidad y juega:
//      CDDA_SIM_MODO=piloto (por defecto): lo lleva la IA de NPC del juego (src/piloto.h);
//      CDDA_SIM_MODO=mono: teclas al azar (andar, inventario, comer, hablar, fabricar, mirar...), con los menús
//      que abran, y Escape o Enter de vez en cuando; con CDDA_SIM_SEMILLA se repite la misma tanda;
//  - un vigilante (otro hilo) distingue un CUELGUE (el juego deja de mirar el teclado: un bucle sin fin o algo
//    que no acaba) de un ATASCO (mira el teclado pero el turno no avanza ni con Escape: un menú que no se cierra);
//  - apunta en el fichero, una línea por cosa: las teclas con su turno (para reproducirlo), el estado cada
//    10 s, y al final FIN, CUELGUE, ATASCO o MUERTO;
//  - con CDDA_SIM_NPC, pone un superviviente al lado del jugador cada 30 s (para probar el diálogo);
//  - y se cierra solo a los CDDA_SIM_SEG segundos (300 por defecto). Código de salida: 0 bien, 3 cuelgue, 4 atasco.
#include <string>

namespace simulacion
{

// el juego mira el teclado (input_context::handle_input): sigue vivo
void latido();
// al empezar cada turno (game::do_turn)
void turno();
// un aviso del juego (debugmsg): se apunta (en la web, cdda_avisos y cdda_ultimo_aviso)
void aviso( const std::string &texto );

} // namespace simulacion

#endif // CATA_SRC_SIMULACION_H
