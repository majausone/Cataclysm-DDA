#pragma once
#ifndef CATA_SRC_INTERFAZ_H
#define CATA_SRC_INTERFAZ_H

#include <string>

class JsonObject;

// La interfaz de la versión web (Encargo 7, B): el enchufe entre el juego y la página. La interfaz va en HTML y
// JavaScript alrededor del juego (build-data/web/, rama tiempo-real-web), y se cambia sin recompilar.
//  - Hacia fuera, el estado en JSON: el del personaje para el HUD (hora, tiempo, necesidades, salud por partes, lo
//    que lleva en la mano), los mensajes con su tipo, el inventario con lo que se puede hacer con cada cosa, las
//    recetas y construcciones con si se pueden hacer y por qué no, el personaje (atributos, habilidades,
//    competencias) y lo que hay en una casilla con las acciones que tienen sentido ahí.
//  - Hacia dentro, órdenes en JSON («abre esa puerta», «come esto», «fabrica tal receta»...): se guardan y se hacen al
//    empezar el siguiente tic, en el hilo del juego y nunca a mitad de un menú.
// En la web, las funciones cdda_ui_* (EMSCRIPTEN_KEEPALIVE, al final de interfaz.cpp).
namespace interfaz
{

std::string estado_json();
// los mensajes: cuántos hay en total (para saber si hay nuevos) y los últimos n con su tipo
std::string mensajes_json( int n );
std::string inventario_json();
std::string recetas_json();
std::string construcciones_json();
std::string personaje_json();
// lo que hay en la casilla (dx, dy) respecto del jugador y qué se puede hacer ahí
std::string casilla_json( int dx, int dy );
// la casilla bajo un píxel del canvas (el de la ventana del juego): {"dx", "dy"} o null
std::string casilla_en_pixel_json( int px, int py );
// una orden (JSON): se hace al empezar el siguiente tic
void orden( const std::string &json );
// al empezar cada tic (game::do_turn): las órdenes pendientes
void turno();
// ¿hay interfaz web? (entonces el juego no pinta su barra lateral)
bool activa();
// el idioma de todo, juego e interfaz: "es" (español) o "en" (inglés); se guarda en las opciones
void poner_idioma( const std::string &idioma );
std::string idioma();

// --- inicio y muerte (Encargo 8)
// lo que pide la página en el menú principal ({"a":"nueva","nombre":...,"hombre":true} o
// {"a":"cargar","mundo":...,"partida":...}); el menú principal lo mira cada 100 ms
void pedir_menu_principal( const std::string &json );
bool tomar_pedido_menu_principal( std::string &json );
// los mundos y sus partidas guardadas
std::string partidas_json();
// al morir: se guarda un resumen para la pantalla de muerte de la página
void al_morir();
std::string muerte_json();
void olvidar_muerte();

// --- segunda parte (interfaz_menus.cpp, Encargo 8): los menús como en un videojuego
// las imágenes del tileset (ruta en el sistema de ficheros del juego, tamaño de sprite, desde qué índice, cuántos)
std::string atlas_json();
// los sprites de una lista de cosas: petición [[id, categoría (item, monster, terrain, furniture, overmap,
// vpart), variante?], ...] -> {"id|cat|variante": [delante, detrás]}
std::string iconos_json( const std::string &peticion );
// lo que se lleva, con lo puesto en cada parte del cuerpo y lo que hay dentro de cada bolsa
std::string equipo_json();
// una receta: componentes y herramientas (con cuántos tienes), tiempo, habilidad y lo que sale
std::string receta_json( const std::string &id );
// lo que hay en el suelo de la casilla (dx, dy), para coger lo que se elija
std::string suelo_json( int dx, int dy );
// lo que se le puede robar al NPC de la casilla (dx, dy): lo que lleva, salvo lo puesto y lo que empuña
std::string robo_json( int dx, int dy );
// la conversación en curso (null si no hay)
std::string dialogo_json();
// el mapa del mundo alrededor (radio en casillas del mapa grande)
std::string mapa_json( int radio );
bool hacer_menus( const std::string &a, JsonObject &o );
// las listas del juego (uilist: «¿qué quieres hacer?»...): con la interfaz web las pinta la página. La que está
// abierta (JSON, o null), y la elección (índice; -1 cancelar), que la página deja y la lista recoge mientras espera
void abrir_lista( const std::string &json );
void cerrar_lista();
std::string lista_json();
void elegir( int i );
bool tomar_eleccion( int &i );
void turno_menus();

} // namespace interfaz

#endif // CATA_SRC_INTERFAZ_H
