#include "simulacion.h"

#if defined(TILES) && !defined(__EMSCRIPTEN__)

#include <array>
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
#if defined(_WIN32)
#include <windows.h>
#endif
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "avatar.h"
#include "calendar.h"
#include "game.h"
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
std::atomic<int> pos_x{ 0 };                 // dónde está el jugador (lo escribe el juego en cada turno)
std::atomic<int> pos_y{ 0 };
std::atomic<bool> guardado_pedido{ false };  // un guardado rápido (lo hace el juego al empezar el turno)
std::atomic<int> foto_pedida{ 0 };     // una foto de la pantalla (la hace el juego en su siguiente latido): su número
// (lo que comparten el juego y el vigilante no se destruye nunca: si el juego sale por su cuenta, el vigilante sigue
// un momento y no puede encontrárselo destruido)
std::mutex &cerrojo = *new std::mutex;
std::string &estado = *new std::string;                 // el último estado (lo escribe el juego, lo lee el vigilante)
std::string &imgui_activas = *new std::string;          // las ventanas de ImGui abiertas (lo mismo)
std::deque<std::string> &ultimas = *new std::deque<std::string>; // las últimas teclas, para el informe
std::string &fichero = *new std::string;
std::string &modo = *new std::string( "piloto" );

void escribir( const std::string &linea )
{
    static std::mutex &m = *new std::mutex;
    std::lock_guard<std::mutex> g( m );
    std::ofstream( fichero, std::ios::app ) << linea << "\n";
}

double segundos_desde( reloj_t::time_point t0 )
{
    return std::chrono::duration<double>( reloj_t::now() - t0 ).count();
}

// una tecla en la cola de SDL (SDL_PushEvent vale desde otro hilo), como la manda un teclado de verdad: la tecla y,
// si es de texto, también el texto (el mapa y los menús de siempre leen el texto; las ventanas de ImGui, la tecla)
// la ventana del juego (los eventos sin ella los descarta ImGui)
SDL_WindowID ventana_juego()
{
    int n = 0;
    SDL_Window **v = SDL_GetWindows( &n );
    const SDL_WindowID id = v != nullptr && n > 0 ? SDL_GetWindowID( v[0] ) : 0;
    SDL_free( v );
    return id;
}

void tecla( SDL_Keycode k, SDL_Scancode sc, const char *texto )
{
    SDL_Keymod mod = SDL_KMOD_NONE;
    const SDL_WindowID vj = ventana_juego();
    if( texto != nullptr && texto[0] != '\0' && texto[1] == '\0' ) {
        const char c = texto[0];
        if( c >= 'A' && c <= 'Z' ) {
            k = static_cast<SDL_Keycode>( c - 'A' + 'a' );
            mod = SDL_KMOD_LSHIFT;
        } else if( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '.' || c == ' ' ) {
            k = static_cast<SDL_Keycode>( c );
        }
        if( k != SDLK_UNKNOWN ) {
            sc = SDL_GetScancodeFromKey( k, nullptr );
        }
    }
    SDL_Event e;
    SDL_zero( e );
    if( k != SDLK_UNKNOWN ) {
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.windowID = vj;
        e.key.key = k;
        e.key.scancode = sc;
        e.key.mod = mod;
        e.key.down = true;
        SDL_PushEvent( &e );
    }
    if( texto != nullptr ) {
        SDL_Event t;
        SDL_zero( t );
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.windowID = vj;
        t.text.text = texto;
        SDL_PushEvent( &t );
    }
    if( k != SDLK_UNKNOWN ) {
        SDL_Event u;
        SDL_zero( u );
        u.type = SDL_EVENT_KEY_UP;
        u.key.windowID = vj;
        u.key.key = k;
        u.key.scancode = sc;
        u.key.mod = mod;
        SDL_PushEvent( &u );
    }
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
    const bool con_guardado = std::getenv( "CDDA_SIM_GUARDAR" ) != nullptr;
    auto t_guardado = reloj_t::now();
    auto t_npc = reloj_t::now() - std::chrono::seconds( 25 );
    while( segundos_desde( t0 ) < duracion ) {
        if( con_guardado && segundos_desde( t_guardado ) > 20 ) {
            t_guardado = reloj_t::now();
            escribir( "GUARDAR turno " + std::to_string( turnos.load() ) );
            guardado_pedido = true;
        }
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
#if defined(_WIN32)
            // (con gdb debajo (CDDA_SIM_GDB), se para aquí y gdb saca la pila de todos los hilos: la del juego dice
            // dónde se ha quedado)
            if( IsDebuggerPresent() ) {
                DebugBreak();
            }
#endif
            std::_Exit( 3 );
        }
        if( muerto ) {
            escribir( "MUERTO a los " + std::to_string( static_cast<int>( segundos_desde( t0 ) ) ) + " s: " +
                      informe() );
            std::_Exit( 0 );
        }
        // el turno no avanza: un intento de salir cada 2 s; si con 30 sigue igual, ATASCO. Los diálogos de un atraco
        // tienen varios pasos, cada uno con su pregunta («You may be attacked! Proceed?»): hacen falta varios
        if( segundos_desde( t_turno ) > 10 + 2.0 * escapes ) {
            if( escapes >= 30 ) {
                foto_pedida = 99;
                std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
                escribir( "ATASCO el turno no avanza ni con 30 intentos (Escape, a/b/c y Enter, y, espacio): " +
                          informe() );
                std::_Exit( 4 );
            }
            // (una foto antes de cada intento: fichero-1.png, fichero-2.png...; y al final, fichero-99.png)
            foto_pedida = escapes + 1;
            std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
            // (Escape, y si no, contestar como en un diálogo obligatorio: a, c o b y Enter, «y» si pregunta si seguir,
            // o espacio)
            static const std::array<const char *, 15> salidas = { {
                    "Escape", "Escape", "a", "Enter", "y", "c", "Enter", "y", "b", "Enter", "y", "espacio",
                    "a", "Enter", "y"
                }
            };
            const std::string s = salidas[escapes % salidas.size()];
            const char *texto = salidas[escapes % salidas.size()];
            escapes++;
            apuntar_tecla( s + " (no avanza)" );
            if( s == "Escape" ) {
                tecla( SDLK_ESCAPE, SDL_SCANCODE_ESCAPE, nullptr );
            } else if( s == "Enter" ) {
                tecla( SDLK_RETURN, SDL_SCANCODE_RETURN, nullptr );
            } else if( s == "espacio" ) {
                tecla( SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, " " );
            } else {
                tecla( SDLK_UNKNOWN, SDL_SCANCODE_UNKNOWN, texto );
            }
        }
        // andar: una dirección cada 100 ms; si en 2 s no se ha movido (una pared), la siguiente. Cada segundo, cuántas
        // casillas ha avanzado (ANDAR)
        if( modo == "andar" && escapes == 0 ) {
            static const std::array<std::pair<SDL_Keycode, SDL_Scancode>, 4> dirs = { {
                    { SDLK_RIGHT, SDL_SCANCODE_RIGHT }, { SDLK_DOWN, SDL_SCANCODE_DOWN },
                    { SDLK_LEFT, SDL_SCANCODE_LEFT }, { SDLK_UP, SDL_SCANCODE_UP }
                }
            };
            static int dir = 0;
            static auto t_mov = reloj_t::now();
            static auto t_seg = reloj_t::now();
            static int ux = pos_x.load();
            static int uy = pos_y.load();
            static int sx = pos_x.load();
            static int sy = pos_y.load();
            if( segundos_desde( t_tecla ) > 0.1 ) {
                t_tecla = reloj_t::now();
                tecla( dirs[dir].first, dirs[dir].second, nullptr );
            }
            if( pos_x.load() != ux || pos_y.load() != uy ) {
                ux = pos_x.load();
                uy = pos_y.load();
                t_mov = reloj_t::now();
            } else if( segundos_desde( t_mov ) > 2 ) {
                dir = ( dir + 1 ) % 4;
                t_mov = reloj_t::now();
            }
            if( segundos_desde( t_seg ) >= 1.0 ) {
                const int casillas = std::max( std::abs( pos_x.load() - sx ), std::abs( pos_y.load() - sy ) );
                escribir( "ANDAR " + std::to_string( casillas ) + " casillas en " +
                          std::to_string( segundos_desde( t_seg ) ).substr( 0, 4 ) + " s, turno " +
                          std::to_string( turnos.load() ) );
                sx = pos_x.load();
                sy = pos_y.load();
                t_seg = reloj_t::now();
            }
        }
        // el mono: una tecla cada 150 ms (callado mientras se intenta salir de un atasco)
        if( modo == "mono" && escapes == 0 && segundos_desde( t_tecla ) > 0.15 ) {
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
        // (muerto: el juego lo marca al morir, y lo que viene después, la puntuación y el menú principal, no son
        // atascos)
        if( g != nullptr && g->uquit == QUIT_DIED ) {
            muerto = true;
        }
        // (la foto pedida: fichero-N.png antes de cada intento de salir de un atasco, fichero-11.png al darlo por bueno)
        if( const int f = foto_pedida.exchange( 0 ) ) {
            const std::string ruta = fichero + "-" + std::to_string( f ) + ".png";
            escribir( std::string( "FOTO " ) + ( g->take_screenshot( ruta ) ? "" : "(no se pudo) " ) + ruta );
        }
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

void aviso( const std::string &texto )
{
    if( en_marcha() ) {
        escribir( "AVISO turno " + std::to_string( turnos.load() ) + " " + texto );
        // (el aviso se queda en pantalla hasta la barra espaciadora: se pulsa, para seguir buscando)
        tecla( SDLK_SPACE, SDL_SCANCODE_SPACE, " " );
    }
}

// el juego sale por su cuenta (lo normal: ha muerto y desde el menú principal el mono ha salido): se apunta y se sale
// ya, sin los destructores del final
[[noreturn]] static void al_salir()
{
    escribir( "SALE el juego por su cuenta: " + informe() );
    std::_Exit( 0 );
}

void turno()
{
    if( !en_marcha() ) {
        return;
    }
    // (al salir, lo primero; se registra aquí, tarde, para que vaya antes que lo demás del final)
    static const bool registrado = std::atexit( al_salir ) == 0;
    static_cast<void>( registrado );
    latido();
    turnos.fetch_add( 1, std::memory_order_relaxed );
    if( guardado_pedido.exchange( false ) ) {
        g->quicksave();
    }
    // siempre a la velocidad de CDDA_SIM_VEL (por defecto, la máxima; el peligro la baja)
    static const realtime::velocidad vel_sim = std::getenv( "CDDA_SIM_VEL" ) != nullptr ?
            realtime::de_texto( std::getenv( "CDDA_SIM_VEL" ) ) : realtime::velocidad::maxima;
    if( realtime::el_reloj().vel() != vel_sim ) {
        realtime::poner( vel_sim );
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
    pos_x = u.pos_abs().x();
    pos_y = u.pos_abs().y();
    if( turnos.load() % 100 == 1 ) {
        std::ostringstream e;
        e << to_string( calendar::turn ) << ", pos " << u.pos_abs().to_string() << ", " << ( modo == "piloto" ? piloto::estado_json() : u.get_name() +
                " en " + u.pos_abs().to_string() );
        std::lock_guard<std::mutex> g( cerrojo );
        estado = e.str();
    }
}

} // namespace simulacion

#elif defined(__EMSCRIPTEN__)

// en la web el que juega y vigila es la página (tools/tiempo-real/cazafallos-web.mjs): aquí solo se cuentan los latidos
#include <emscripten.h>

#include <string>

namespace
{
int latidos_web = 0;
int avisos_web = 0;
std::string ultimo_aviso_web;
} // namespace

namespace simulacion
{
void latido()
{
    latidos_web++;
}
void turno() {}
void aviso( const std::string &texto )
{
    avisos_web++;
    ultimo_aviso_web = texto;
}
} // namespace simulacion

extern "C" {
    // cuántas veces ha mirado el teclado el juego (si deja de subir con la página viva, algo no acaba)
    EMSCRIPTEN_KEEPALIVE int cdda_latidos()
    {
        return latidos_web;
    }
    // cuántos avisos del juego (debugmsg) ha habido, y el último
    EMSCRIPTEN_KEEPALIVE int cdda_avisos()
    {
        return avisos_web;
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ultimo_aviso()
    {
        return ultimo_aviso_web.c_str();
    }
}

#else

namespace simulacion
{
void latido() {}
void turno() {}
void aviso( const std::string & ) {}
} // namespace simulacion

#endif
