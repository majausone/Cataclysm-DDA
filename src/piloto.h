#pragma once
#ifndef CATA_SRC_PILOTO_H
#define CATA_SRC_PILOTO_H

#include <string>

class Character;
class npc;

// Piloto automático (rama tiempo-real, fase 2): el personaje lo lleva la propia IA de NPC del juego.
//  - Al activarlo, el personaje pasa a ser un NPC con la IA de siempre (anda, come, bebe, lucha, coge cosas) y el
//    jugador se queda como un observador invisible e intocable que lo sigue de cerca (para que el personaje no se
//    salga de la zona que el juego simula alrededor del jugador). Se usa lo mismo que el «tomar el control de un
//    NPC» del juego (avatar::control_npc), al revés.
//  - Al desactivarlo, el jugador vuelve a su personaje y el observador desaparece.
//  - De todos los NPC (y del personaje con el piloto) se apunta qué hacen y por qué: la acción que elige su IA y la
//    meta y categoría de su árbol de decisiones, con la hora; y su plan y sus necesidades. Es lo que enseñan los
//    paneles de la página (versión web) y el que se haya elegido con un clic.
namespace piloto
{

bool activo();
void activar();
void desactivar();
void alternar();
// pedir encenderlo o apagarlo: se hace al empezar el siguiente turno (desde una tecla o desde la página, el juego
// puede estar a mitad de algo)
void pedir( bool encender );
// al empezar cada turno (game::do_turn): el observador sigue al personaje y no hace nada
void turno();
// por qué se apagó solo la última vez ("" si no se ha apagado solo)
std::string motivo_apagado();
// para las pruebas y la simulación: un superviviente neutral al lado del jugador, para hablar con él. Se pide (desde la
// página o desde otro hilo) y se hace al empezar el siguiente turno
void pedir_npc_al_lado();

// lo que ha decidido la IA de un NPC en este turno (npc::move)
void apuntar( const npc &quien, const std::string &accion, const std::string &categoria,
              const std::string &meta );
// elegir a quién enseñan los paneles (un clic en él, o desde la lista)
void seleccionar( const Character &quien );
void seleccionar_id( int id );

// el estado del elegido (por defecto, el personaje con el piloto) y la lista de NPC cerca, en JSON
std::string estado_json();
std::string lista_json();

} // namespace piloto

#endif // CATA_SRC_PILOTO_H
