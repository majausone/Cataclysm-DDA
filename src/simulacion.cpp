#include "simulacion.h"

#if defined(TILES) && !defined(__EMSCRIPTEN__)

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "avatar.h"
#include "calendar.h"
#include "piloto.h"
#include "realtime.h"
#include "ui_manager.h"

namespace
{
using reloj_t = std::chrono::steady_clock;

std::atomic<int64_t> latidos{ 0 };
std::atomic<int64_t> turnos{ 0 };
std::atomic<bool> muerto{ false };
std::atomic<int> ventanas{ 0 };
std::mutex cerrojo;
std::string estado;                 // el último estado (lo escribe el juego, lo lee el vigilante)
std::string imgui_activas;          // las ventanas de ImGui abiertas (lo mismo)
std::deque<std::string> ultimas;    // las últimas teclas, para el informe
std::string fichero;
std::string modo = "piloto";

void escribir( const std::string &linea )
{
    static std::mutex m;
    std::lock_guard<std::mutex> g( m );
    std::ofstream( fichero, std::ios::app ) << linea << "\n";
}

double segundos_desde( reloj_t::time_point t0 )
{
    return std::chrono::duration<double>( reloj_t::now() - t0 ).count();
}

// una tecla en la cola de SDL (SDL_PushEvent vale desde otro hilo). Las de texto, como texto (así las lee el juego en
// los menús y en el mapa); las demás, como tecla
void tecla( SDL_Keycode k, SDL_Scancode sc, const char *texto )
{
    if( texto != nullptr ) {
        SDL_Event t;
        SDL_zero( t );
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.text = texto;
        SDL_PushEvent( &t );
        return;
    }
    SDL_Event e;
    SDL_zero( e );
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = sc;
    e.key.down = true;
    SDL_PushEvent( &e );
    SDL_Event u;
    SDL_zero( u );
    u.type = SDL_EVENT_KEY_UP;
    u.key.key = k;
    u.key.scancode = sc;
    SDL_PushEvent( &u );
}

struct tecla_t {
    const char *nombre;
    SDL_Keycode k;
    SDL_Scancode sc;
    const char *texto;
    int peso;
};

// lo que pulsa el mono: sobre todo andar; y las acciones con menú (inventario, comer, ponerse, coger, hablar, fabricar,
// mirar...), con lo que haya en el menú (letras, números, Enter, Escape)
const std::vector<tecla_t> &teclas()
{
    static const std::vector<tecla_t> t = {
        { "arriba", SDLK_UP, SDL_SCANCODE_UP, nullptr, 8 },
        { "abajo", SDLK_DOWN, SDL_SCANCODE_DOWN, nullptr, 8 },
        { "izquierda", SDLK_LEFT, SDL_SCANCODE_LEFT, nullptr, 8 },
        { "derecha", SDLK_RIGHT, SDL_SCANCODE_RIGHT, nullptr, 8 },
        { "y", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "y", 3 },
        { "u", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "u", 3 },
        { "b", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "b", 3 },
        { "n", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "n", 3 },
        { "esperar .", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, ".", 3 },
        { "inventario i", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "i", 2 },
        { "comer E", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "E", 2 },
        { "ponerse W", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "W", 1 },
        { "coger g", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "g", 2 },
        { "hablar C", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "C", 2 },
        { "fabricar &", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "&", 1 },
        { "mirar x", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "x", 1 },
        { "examinar e", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "e", 2 },
        { "abrir o", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "o", 1 },
        { "cerrar c", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "c", 1 },
        { "empuñar w", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "w", 1 },
        { "personaje @", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "@", 1 },
        { "mapa m", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "m", 1 },
        { "a", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "a", 2 },
        { "1", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "1", 2 },
        { "2", SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "2", 1 },
        { "Enter", SDLK_RETURN, SDL_SCANCODE_RETURN, nullptr, 4 },
        { "Escape", SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, nullptr, 5 },
    };
    return t;
}

void apuntar_tecla( const std::string &n )
{
    std::ostringstream l;
    l << "TECLA turno " << turnos.load() << " " << n;
    escribir( l.str() );
    std::lock_guard<std::mutex> g( cerrojo );
    ultimas.push_back( n );
    while( ultimas.size() > 20 ) {
        ultimas.pop_front();
    }
}

std::string informe()
{
    std::lock_guard<std::mutex> g( cerrojo );
    std::ostringstream l;
    l << "turno " << turnos.load() << ", ventanas " << ventanas.load() << ( muerto ? ", muerto" : "" ) <<
      ", últimas teclas:";
    for( const std::string &t : ultimas ) {
        l << " [" << t << "]";
    }
    l << "; ImGui: [" << imgui_activas << "]; estado: " << estado;
    return l.str();
}

void jugar()
{
    const char *seg = std::getenv( "CDDA_SIM_SEG" );
    const double duracion = seg != nullptr ? std::atof( seg ) : 300.0;
    const char *sem = std::getenv( "CDDA_SIM_SEMILLA" );
    const unsigned semilla = sem != nullptr ? static_cast<unsigned>( std::strtoul( sem, nullptr, 10 ) ) : 1u;
    std::mt19937 azar( semilla );
    const auto t0 = reloj_t::now();
    escribir( "EMPIEZA modo " + modo + ", semilla " + std::to_string( semilla ) + ", " + std::to_string(
                  static_cast<int>( duracion ) ) + " s" );
    // hasta que hay partida: «d» (Play Now! Default Scenario) cada 5 s
    while( turnos.load() == 0 ) {
        if( segundos_desde( t0 ) > 240 ) {
            escribir( "ATASCO no empieza la partida: " + informe() );
            std::_Exit( 4 );
        }
        std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
        if( turnos.load() == 0 ) {
            tecla( SDLK_D, SDL_SCANCODE_D, nullptr );
            tecla( SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, "d" );
        }
    }
    escribir( "PARTIDA en marcha a los " + std::to_string( static_cast<int>( segundos_desde( t0 ) ) ) + " s" );

    std::vector<int> pesos;
    for( const tecla_t &t : teclas() ) {
        pesos.push_back( t.peso );
    }
    std::discrete_distribution<int> elegir( pesos.begin(), pesos.end() );
    int64_t latido_visto = latidos.load();
    int64_t turno_visto = turnos.load();
    auto t_latido = reloj_t::now();
    auto t_turno = reloj_t::now();
    auto t_estado = reloj_t::now();
    auto t_tecla = reloj_t::now();
    int escapes = 0;
    const bool con_npc = std::getenv( "CDDA_SIM_NPC" ) != nullptr;
    auto t_npc = reloj_t::now() - std::chrono::seconds( 25 );
    while( segundos_desde( t0 ) < duracion ) {
        if( con_npc && segundos_desde( t_npc ) > 30 ) {
            t_npc = reloj_t::now();
            escribir( "NPC al lado, turno " + std::to_string( turnos.load() ) );
            piloto::pedir_npc_al_lado();
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        const int64_t l = latidos.load();
        const int64_t tu = turnos.load();
        if( l != latido_visto ) {
            latido_visto = l;
            t_latido = reloj_t::now();
        }
        if( tu != turno_visto ) {
            turno_visto = tu;
            t_turno = reloj_t::now();
            escapes = 0;
        }
        // CUELGUE: 20 s sin mirar el teclado
        if( segundos_desde( t_latido ) > 20 ) {
            escribir( "CUELGUE 20 s sin mirar el teclado: " + informe() );
            std::_Exit( 3 );
        }
        if( muerto ) {
            escribir( "MUERTO a los " + std::to_string( static_cast<int>( segundos_desde( t0 ) ) ) + " s: " +
                      informe() );
            std::_Exit( 0 );
        }
        // el turno no avanza: Escape cada 2 s; si con 10 sigue igual, ATASCO
        if( segundos_desde( t_turno ) > 10 + 2.0 * escapes ) {
            if( escapes >= 10 ) {
                escribir( "ATASCO el turno no avanza ni con 10 Escape: " + informe() );
                std::_Exit( 4 );
            }
            escapes++;
            apuntar_tecla( "Escape (no avanza)" );
            tecla( SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, nullptr );
        }
        // el mono: una tecla cada 150 ms
        if( modo == "mono" && segundos_desde( t_tecla ) > 0.15 ) {
            t_tecla = reloj_t::now();
            const tecla_t &t = teclas()[elegir( azar )];
            apuntar_tecla( t.nombre );
            tecla( t.k, t.sc, t.texto );
        }
        if( segundos_desde( t_estado ) > 10 ) {
            t_estado = reloj_t::now();
            std::lock_guard<std::mutex> g( cerrojo );
            escribir( "ESTADO turno " + std::to_string( tu ) + ", " + estado );
        }
    }
    escribir( "FIN " + informe() );
    std::_Exit( 0 );
}

struct arranque_t {
    arranque_t() {
        const char *f = std::getenv( "CDDA_SIM" );
        if( f == nullptr ) {
            return;
        }
        fichero = f;
        if( const char *m = std::getenv( "CDDA_SIM_MODO" ) ) {
            modo = m;
        }
        std::thread( jugar ).detach();
    }
};
const arranque_t arranque;

bool en_marcha()
{
    return !fichero.empty();
}
} // namespace

namespace simulacion
{

void latido()
{
    if( en_marcha() ) {
        const int64_t n = latidos.fetch_add( 1, std::memory_order_relaxed );
        ventanas.store( static_cast<int>( ui_adaptor::ui_stack_size() ), std::memory_order_relaxed );
        // (cada 20 latidos: qué ventanas de ImGui hay abiertas, para el informe de un atasco)
        if( n % 20 == 0 && ImGui::GetCurrentContext() != nullptr ) {
            std::string v;
            for( const ImGuiWindow *w : ImGui::GetCurrentContext()->Windows ) {
                if( w->WasActive && !w->Hidden ) {
                    v += ( v.empty() ? "" : ", " ) + std::string( w->Name );
                }
            }
            std::lock_guard<std::mutex> g( cerrojo );
            imgui_activas = v;
        }
    }
}

void turno()
{
    if( !en_marcha() ) {
        return;
    }
    latido();
    turnos.fetch_add( 1, std::memory_order_relaxed );
    // siempre a la máxima (el peligro la baja a x1)
    if( realtime::el_reloj().vel() != realtime::velocidad::maxima ) {
        realtime::poner( realtime::velocidad::maxima );
    }
    if( modo == "piloto" ) {
        static bool pedido = false;
        static bool estuvo = false;
        if( !pedido ) {
            pedido = true;
            piloto::pedir( true );
        } else if( piloto::activo() ) {
            estuvo = true;
        } else if( estuvo && !muerto ) {
            // el piloto se ha apagado solo: su personaje ha muerto o se ha perdido
            std::lock_guard<std::mutex> g( cerrojo );
            estado = "piloto apagado: " + piloto::motivo_apagado() + "; " + piloto::estado_json();
            muerto = true;
        }
    }
    avatar &u = get_avatar();
    if( u.is_dead_state() ) {
        muerto = true;
    }
    // el estado, cada 100 turnos (lo apunta el vigilante cada 10 s)
    if( turnos.load() % 100 == 1 ) {
        std::ostringstream e;
        e << to_string( calendar::turn ) << ", " << ( modo == "piloto" ? piloto::estado_json() : u.get_name() +
                " en " + u.pos_abs().to_string() );
        std::lock_guard<std::mutex> g( cerrojo );
        estado = e.str();
    }
}

} // namespace simulacion

#elif defined(__EMSCRIPTEN__)

// en la web el que juega y vigila es la página (tools/tiempo-real/cazafallos-web.mjs): aquí solo se cuentan los latidos
#include <emscripten.h>

namespace
{
int latidos_web = 0;
} // namespace

namespace simulacion
{
void latido()
{
    latidos_web++;
}
void turno() {}
} // namespace simulacion

extern "C" {
    // cuántas veces ha mirado el teclado el juego (si deja de subir con la página viva, algo no acaba)
    EMSCRIPTEN_KEEPALIVE int cdda_latidos()
    {
        return latidos_web;
    }
}

#else

namespace simulacion
{
void latido() {}
void turno() {}
} // namespace simulacion

#endif
