#include "realtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <thread>

#include "avatar.h"
#include "calendar.h"
#include "debug.h"
#include "cached_options.h"
#include "game.h"
#include "input_context.h"
#include "interfaz.h"
#include "options.h"
#include "output.h"
#include "piloto.h"
#include "suave.h"
#include "player_activity.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static const efftype_id effect_sleep( "sleep" );

namespace realtime
{

int multiplicador( velocidad v )
{
    switch( v ) {
        case velocidad::pausa:
            return 0;
        case velocidad::lento:
            return 12;
        case velocidad::normal:
            return 24;
        case velocidad::rapido:
            return 48;
        case velocidad::maxima:
            return -1;
    }
    return 24;
}

std::string nombre( velocidad v )
{
    switch( v ) {
        case velocidad::pausa:
            return _( "PAUSE" );
        case velocidad::lento:
            return _( "Day 2h" );
        case velocidad::normal:
            return _( "Day 1h" );
        case velocidad::rapido:
            return _( "Day 30m" );
        case velocidad::maxima:
            return _( "FAST" );
    }
    return "";
}

velocidad de_texto( const std::string &s )
{
    if( s == "pausa" || s == "pause" ) {
        return velocidad::pausa;
    }
    if( s == "lento" ) {
        return velocidad::lento;
    }
    if( s == "rapido" ) {
        return velocidad::rapido;
    }
    if( s == "max" ) {
        return velocidad::maxima;
    }
    return velocidad::normal;
}

// ------------------------------------------------------------------ los dos relojes
int reparto( int velocidad_criatura, int64_t tic, int factor )
{
    if( factor <= 1 ) {
        return velocidad_criatura;
    }
    const int64_t k = ( ( tic % factor ) + factor ) % factor;
    const int64_t v = velocidad_criatura;
    return static_cast<int>( v * ( k + 1 ) / factor - v * k / factor );
}

bool es_tic_de_accion( int64_t tic, int fase, int factor )
{
    if( factor <= 1 ) {
        return true;
    }
    return ( ( ( tic + fase ) % factor ) + factor ) % factor == 0;
}

static int64_t tic_actual()
{
    return to_turn<int64_t>( calendar::turn );
}

int puntos_por_tic( int velocidad_criatura, bool al_ritmo_del_mundo )
{
    if( al_ritmo_del_mundo ) {
        return velocidad_criatura;
    }
    return reparto( velocidad_criatura, tic_actual(), factor_accion() );
}

int turno_entero( int velocidad_criatura, int64_t tic, int64_t fase, int factor )
{
    if( factor <= 1 ) {
        return velocidad_criatura;
    }
    return ( ( ( tic + fase ) % factor ) + factor ) % factor == 0 ? velocidad_criatura : 0;
}

int puntos_por_turno( int velocidad_criatura, int64_t fase, bool al_ritmo_del_mundo )
{
    if( al_ritmo_del_mundo ) {
        return velocidad_criatura;
    }
    return turno_entero( velocidad_criatura, tic_actual(), fase, factor_accion() );
}

bool tic_de_accion( int fase )
{
    return es_tic_de_accion( tic_actual(), fase, factor_accion() );
}

int turnos_de_accion( int turnos, int fase )
{
    const int f = factor_accion();
    if( f <= 1 ) {
        return turnos;
    }
    if( turnos == 1 ) {
        return tic_de_accion( fase ) ? 1 : 0;
    }
    return turnos / f;
}

bool efecto_real( const std::string &id )
{
    static const std::array<const char *, 12> reales = { {
            "stunned", "downed", "dazed", "bleed", "grabbed", "grabbing", "onfire", "staggered", "winded",
            "flash_blinded", "hit_by_player", "pushed"
        }
    };
    return std::find( reales.begin(), reales.end(), id ) != reales.end();
}

// ------------------------------------------------------------------ el reloj
reloj::reloj( std::function<instante()> ahora ) : ahora_( std::move( ahora ) )
{
    plazo_ = ahora_();
    ultimo_inicio_ = plazo_;
}

void reloj::poner( velocidad v )
{
    if( v == vel_ ) {
        return;
    }
    vel_ = v;
    // al cambiar de velocidad se empieza a contar desde ahora (ni deuda ni adelanto de la anterior)
    hay_plazo_ = false;
    plazo_ = ahora_();
    retrasado = false;
    a_tiempo_ = 0;
}

bool reloj::toca() const
{
    if( vel_ == velocidad::maxima ) {
        return true;
    }
    if( vel_ == velocidad::pausa ) {
        return false;
    }
    return !hay_plazo_ || ahora_() >= plazo_;
}

int64_t reloj::ms_hasta_turno() const
{
    if( vel_ == velocidad::pausa ) {
        return -1;
    }
    if( vel_ == velocidad::maxima || !hay_plazo_ ) {
        return 0;
    }
    const auto falta = std::chrono::duration_cast<std::chrono::milliseconds>( plazo_ - ahora_() );
    return std::max<int64_t>( 0, falta.count() );
}

void reloj::empezar()
{
    const instante ahora = ahora_();
    turnos++;
    if( hay_inicio_ ) {
        const double ms = std::chrono::duration<double, std::milli>( ahora - ultimo_inicio_ ).count();
        if( ms > 0.0 ) {
            const double tps = 1000.0 / ms;
            turnos_por_segundo = turnos_por_segundo <= 0.0 ? tps : turnos_por_segundo * 0.9 + tps * 0.1;
        }
    }
    hay_inicio_ = true;
    ultimo_inicio_ = ahora;
    const int m = multiplicador( vel_ );
    if( m <= 0 ) {
        // a la máxima o en pausa no hay plazo: el siguiente turno va cuando pueda o cuando quiera el jugador
        hay_plazo_ = false;
        plazo_ = ahora;
        return;
    }
    const std::chrono::nanoseconds periodo( 1000000000LL / m );
    // (margen: lo que se puede ir tarde y aún recuperar, haciendo seguidos los turnos que tocaban. Un turno, o 100 ms si
    // es más: en el navegador, cada vez que el juego cede el control (al repintar, al mirar el teclado) se le va al
    // menos un fotograma, y a x72 (turnos de 14 ms) eso solo se recupera haciendo después los que tocaban, por
    // fotogramas. Más allá no se recupera nada: sin deuda ni ráfagas largas)
    const std::chrono::nanoseconds margen = std::max<std::chrono::nanoseconds>( periodo,
                                            std::chrono::milliseconds( 100 ) );
    if( hay_plazo_ && ahora - plazo_ < margen ) {
        // a tiempo (o dentro del margen): el siguiente, un periodo después del plazo de este
        plazo_ += periodo;
        if( ++a_tiempo_ > m ) {
            retrasado = false;
        }
    } else {
        // va tarde más que el margen (o es el primero): sin acumular retraso, desde ahora
        if( hay_plazo_ ) {
            retrasado = true;
            a_tiempo_ = 0;
        }
        plazo_ = ahora + periodo;
    }
    hay_plazo_ = true;
}

void reloj::fin_de_calculo( double ms_esperando_al_jugador )
{
    const double ms = std::chrono::duration<double, std::milli>( ahora_() - ultimo_inicio_ ).count() -
                      ms_esperando_al_jugador;
    ms_ultimo_turno = std::max( 0.0, ms );
    // (los primeros turnos no cuentan para la media: el primero lleva dentro generar el mundo)
    if( turnos <= 5 ) {
        ms_turno_medio = ms_ultimo_turno;
    } else {
        ms_turno_medio = ms_turno_medio * 0.95 + ms_ultimo_turno * 0.05;
    }
}

// ------------------------------------------------------------------ el del juego
namespace
{
std::optional<std::string> accion_pendiente;
double ms_espera_jugador = 0.0;
std::chrono::steady_clock::time_point inicio_espera_jugador;
bool esperando_jugador = false;
// (el juego está parado esperando el siguiente tic, en esperar_turno: un sitio seguro para repintar desde fuera)
bool en_espera_de_tic = false;
bool velocidad_de_opciones = false;

// ¿le toca al jugador decidir en este turno?  (si no, el turno pasa solo: actividad, sueño, moviéndose...)
bool jugador_decide()
{
    // (con el piloto automático, decide la IA: el jugador solo mira)
    if( piloto::activo() ) {
        return false;
    }
    const avatar &u = get_avatar();
    return !u.has_effect( effect_sleep ) && u.get_moves() > 0 && !u.activity && !u.has_destination();
}

// las acciones de ratón y las internas no se guardan para luego
bool se_puede_guardar( const std::string &a )
{
    static const std::array<const char *, 9> no = { {
            "TIMEOUT", "ERROR", "ANY_INPUT", "MOUSE_MOVE", "SELECT", "SEC_SELECT", "COORDINATE",
            "HELP_KEYBINDINGS", ""
        }
    };
    return std::find_if( no.begin(), no.end(), [&]( const char *x ) {
        return a == x;
    } ) == no.end();
}

// espera hasta que le toque al siguiente turno sin ceder el control (como mucho unos ms)
void esperar_sin_ceder( const reloj &r )
{
    while( !r.toca() && r.vel() != velocidad::pausa ) {
#if defined(__EMSCRIPTEN__)
        // (en el navegador no hay hilos que dormir: se espera mirando la hora)
#else
        std::this_thread::sleep_for( std::chrono::microseconds( 500 ) );
#endif
    }
}

// lo que tarda en pintarse una imagen (media móvil, ms): para medir si da para 60 por segundo
double ms_imagen_medio = 0.0;

void repintar()
{
    if( test_mode ) {
        return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    g->invalidate_main_ui_adaptor();
    ui_manager::redraw();
    refresh_display();
    const double ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
    ms_imagen_medio = ms_imagen_medio * 0.9 + ms * 0.1;
}
} // namespace

reloj &el_reloj()
{
    static reloj r;
    // la velocidad de partida, la de las opciones (una vez)
    if( !velocidad_de_opciones && !test_mode ) {
        velocidad_de_opciones = true;
        // (siempre el día de una hora: 24 tics por segundo. El jugador no elige velocidad ni pausa; las otras
        // velocidades son para el modo simulación y las pruebas)
        r.poner( velocidad::normal );
    }
    return r;
}

bool activo()
{
    return !test_mode && get_option<bool>( "REALTIME" );
}

int factor_accion()
{
    if( !activo() ) {
        return 1;
    }
    const int m = multiplicador( el_reloj().vel() );
    return m > 0 ? m : 24;
}

void poner( velocidad v )
{
    el_reloj().poner( v );
}

void subir()
{
    // (el jugador elige la duración del día; la máxima es solo para el modo simulación y las pruebas)
    const int i = static_cast<int>( el_reloj().vel() );
    poner( static_cast<velocidad>( std::min( i + 1, static_cast<int>( velocidad::rapido ) ) ) );
}

void bajar()
{
    const int i = static_cast<int>( el_reloj().vel() );
    poner( static_cast<velocidad>( std::max( i - 1, 0 ) ) );
}

void alternar_pausa()
{
    static velocidad antes = velocidad::normal;
    reloj &r = el_reloj();
    if( r.vel() == velocidad::pausa ) {
        poner( antes );
    } else {
        antes = r.vel();
        poner( velocidad::pausa );
    }
}

void peligro()
{
    // (el juego no se para nunca y el jugador no elige velocidad: un peligro a la vista no cambia el reloj. Lo que
    // sí: si está a la máxima (solo en el modo simulación y las pruebas), vuelve al día normal)
    if( activo() && el_reloj().vel() == velocidad::maxima ) {
        poner( velocidad::normal );
    }
}

void esperar_turno()
{
    if( !activo() ) {
        return;
    }
    reloj &r = el_reloj();
    r.fin_de_calculo( ms_espera_jugador );
    ms_espera_jugador = 0.0;
    for( ;; ) {
        const bool decide = jugador_decide();
        // en pausa, si decide el jugador, es el juego de siempre: espera su tecla en su turno
        if( r.vel() == velocidad::pausa && decide ) {
            break;
        }
        if( r.vel() != velocidad::pausa && r.toca() ) {
            break;
        }
        const int64_t falta = r.ms_hasta_turno();
        // (a más de x10, lo que falta hasta el plazo, si es poco, se espera sin ceder el control: cada espera del
        // teclado se lleva al menos un fotograma del navegador y con turnos de 14 ms se perdía uno de cada dos;
        // el teclado se sigue mirando cada 30 ms, en realtime::saltar_espera)
        // (solo con tics de menos de 33 ms, más de 30 por segundo: a 24 por segundo, esperar así la mitad de cada tic
        // dejaba al navegador sin pintar ni leer el teclado, y salían 25 imágenes por segundo en vez de 60)
        if( falta >= 0 && falta <= 20 && multiplicador( r.vel() ) > 30 ) {
            esperar_sin_ceder( r );
            continue;
        }
        // (mientras algo se mueve, se espera solo hasta la siguiente imagen: 16 ms desde la anterior, descontando lo
        // que tardó en pintarse; si se esperaran 16 ms enteros y luego se pintara, saldrían unas 30 por segundo)
        const bool moviendo = suave::hay_movimiento();
        // (desde la última imagen de verdad, la pintara el tic o esta espera: si se contara desde que empezó la
        // espera, tras cada tic se perdían 15 ms y salía una imagen por tic)
        const int64_t hasta_imagen = std::clamp<int64_t>( 16 - suave::ms_desde_imagen(), 1, 16 );
        const int t = falta < 0 ? 100 : static_cast<int>( std::clamp<int64_t>( falta, 1,
                      moviendo ? hasta_imagen : 50 ) );
        input_context ctxt = get_default_mode_input_context();
        en_espera_de_tic = true;
        const std::string action = ctxt.handle_input( t );
        en_espera_de_tic = false;
        if( action == "TIMEOUT" ) {
            // (las órdenes de la interfaz web, también mientras espera)
            interfaz::turno();
            // (que se vea la velocidad y lo que pasa, de vez en cuando, mientras espera)
            // (el tic manda: solo se pinta si da tiempo antes del siguiente; si no, se pinta con el tic)
            const int64_t queda = r.ms_hasta_turno();
            if( suave::ms_desde_imagen() >= ( suave::hay_movimiento() ? 15 : 250 ) &&
                ( queda < 0 ? false : static_cast<double>( queda ) > ms_imagen_medio + 2.0 ) ) {
                repintar();
            }
            continue;
        }
        if( manejar_accion( action ) ) {
            repintar();
            continue;
        }
        avatar &u = get_avatar();
        if( !decide && u.activity ) {
            // con una actividad en marcha, como siempre: «pause» la interrumpe (preguntando)
            if( action == "pause" && u.activity.is_interruptible_with_kb() ) {
                g->cancel_activity_query( _( "Confirm:" ) );
                repintar();
            }
            continue;
        }
        // lo que pulsa mientras espera a que le toque: se hace en cuanto le toque
        if( se_puede_guardar( action ) && !accion_pendiente ) {
            accion_pendiente = action;
        }
    }
    r.empezar();
    // (cada 10 s, al registro del juego: a qué va de verdad y cuánto tarda cada turno)
    // (con CDDA_RT_BANCO=fichero, también a ese fichero: es el modo de medida de abajo, y el juego se cierra a la fuerza)
    static auto ultimo_registro = std::chrono::steady_clock::now();
    static int64_t turnos_registro = r.turnos;
    const auto ahora = std::chrono::steady_clock::now();
    if( ahora - ultimo_registro >= std::chrono::seconds( 10 ) ) {
        const double s = std::chrono::duration<double>( ahora - ultimo_registro ).count();
        const double tps = ( r.turnos - turnos_registro ) / s;
        ultimo_registro = ahora;
        turnos_registro = r.turnos;
        DebugLog( D_INFO, D_GAME ) << "tiempo real: velocidad " << nombre( r.vel() ) << ", " << tps <<
                                   " turnos/s, " << r.ms_turno_medio << " ms por turno" << ( r.retrasado ? " (no llega)" : "" );
        const char *banco = std::getenv( "CDDA_RT_BANCO" );
        if( banco != nullptr && std::string( banco ).size() > 1 ) {
            std::ofstream( banco, std::ios::app ) << "velocidad " << nombre( r.vel() ) << ", " << tps << " turnos/s, " <<
                                                  r.ms_turno_medio << " ms por turno" << ( r.retrasado ? " (no llega)" : "" ) << "\n";
        }
    }
}

bool plazo_vencido( bool vencido_sin_tiempo_real )
{
    if( !activo() ) {
        return vencido_sin_tiempo_real;
    }
    const reloj &r = el_reloj();
    if( r.vel() == velocidad::pausa ) {
        return false;
    }
    // (a más de x10, el turno del jugador mira el teclado una vez y lo pasa: lo que falta hasta el plazo se espera
    // en esperar_turno, sin ceder el control; esperarlo aquí, mirando el teclado, se llevaba un fotograma del
    // navegador por vuelta y a x72 (turnos de 14 ms) se perdía uno de cada dos plazos)
    if( multiplicador( r.vel() ) > 10 ) {
        return true;
    }
    return r.toca();
}

int ms_espera_teclado()
{
    if( !activo() ) {
        return 125;
    }
    const int64_t falta = el_reloj().ms_hasta_turno();
    if( falta < 0 ) {
        return 125;
    }
    if( multiplicador( el_reloj().vel() ) > 10 ) {
        return 1;
    }
    return static_cast<int>( std::clamp<int64_t>( falta, 1, 125 ) );
}

void empieza_espera_jugador()
{
    esperando_jugador = true;
    inicio_espera_jugador = std::chrono::steady_clock::now();
}

void acaba_espera_jugador()
{
    if( esperando_jugador ) {
        ms_espera_jugador += std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() -
                             inicio_espera_jugador ).count();
        esperando_jugador = false;
    }
}

namespace
{
int dir_x = 0;
int dir_y = 0;
} // namespace

void mantener_direccion( int dx, int dy )
{
    dir_x = std::clamp( dx, -1, 1 );
    dir_y = std::clamp( dy, -1, 1 );
}

int pintar_si_toca()
{
    // (solo parado en una espera, la del tic o la de la tecla del jugador, nunca a mitad de un tic ni de un menú)
    // devuelve 1 si ha pintado; si no, por qué: 2 no está esperando, 3 nada se mueve, 4 hace poco de la última
    if( !activo() || test_mode ) {
        return 0;
    }
    if( !en_espera_de_tic && !esperando_jugador ) {
        return 2;
    }
    if( !suave::hay_movimiento() ) {
        return 3;
    }
    if( suave::ms_desde_imagen() < 8 ) {
        return 4;
    }
    repintar();
    return 1;
}

double ms_imagen()
{
    return ms_imagen_medio;
}

int direccion_x()
{
    return dir_x;
}

int direccion_y()
{
    return dir_y;
}

std::string accion_direccion()
{
    if( !activo() ) {
        return "";
    }
    static const std::array<std::array<const char *, 3>, 3> nombres = { {
            { { "LEFTUP", "UP", "RIGHTUP" } },
            { { "LEFT", "", "RIGHT" } },
            { { "LEFTDOWN", "DOWN", "RIGHTDOWN" } }
        }
    };
    return nombres[dir_y + 1][dir_x + 1];
}

bool hay_accion_pendiente()
{
    return activo() && accion_pendiente.has_value();
}

std::string tomar_accion_pendiente()
{
    std::string a = accion_pendiente.value_or( std::string() );
    accion_pendiente.reset();
    return a;
}

bool manejar_accion( const std::string &action )
{
    // (sin teclas de velocidad ni de pausa: el juego no se para nunca)
    if( action == "AUTOPILOT" ) {
        piloto::pedir( !piloto::activo() );
    } else {
        return false;
    }
    return true;
}

bool saltar_espera()
{
    if( !activo() ) {
        return false;
    }
    const int m = multiplicador( el_reloj().vel() );
    if( m >= 0 && m <= 10 ) {
        return false;
    }
    static std::chrono::steady_clock::time_point ultima;
    const auto ahora = std::chrono::steady_clock::now();
    if( ahora - ultima >= std::chrono::milliseconds( 30 ) ) {
        ultima = ahora;
        return false;
    }
    return true;
}

bool conviene_repintar()
{
    if( !activo() ) {
        return true;
    }
    const int m = multiplicador( el_reloj().vel() );
    if( m >= 0 && m <= 10 ) {
        return true;
    }
    static std::chrono::steady_clock::time_point ultimo;
    const auto ahora = std::chrono::steady_clock::now();
    // (mientras algo se mueve, a 60 imágenes por segundo; si no, a 25)
    if( ahora - ultimo >= std::chrono::milliseconds( suave::hay_movimiento() ? 15 : 40 ) ) {
        ultimo = ahora;
        return true;
    }
    return false;
}

std::string texto_estado()
{
    const reloj &r = el_reloj();
    std::string s = nombre( r.vel() );
    if( r.vel() == velocidad::maxima ) {
        if( r.turnos_por_segundo > 0.0 ) {
            s += " " + std::to_string( static_cast<int>( std::lround( r.turnos_por_segundo ) ) ) + "/s";
        }
    } else if( r.retrasado && r.turnos_por_segundo > 0.0 ) {
        // no llega: a cuánto va de verdad
        s += ">" + std::to_string( static_cast<int>( std::lround( r.turnos_por_segundo ) ) );
    }
    return s;
}

void mostrar_estado()
{
    repintar();
}

} // namespace realtime

// ------------------------------------------------------------------ para la versión web (JS)
#if defined(__EMSCRIPTEN__)
extern "C" {
    EMSCRIPTEN_KEEPALIVE int cdda_rt_velocidad()
    {
        return static_cast<int>( realtime::el_reloj().vel() );
    }
    EMSCRIPTEN_KEEPALIVE void cdda_rt_poner_velocidad( int v )
    {
        realtime::poner( static_cast<realtime::velocidad>( std::clamp( v, 0, realtime::num_velocidades - 1 ) ) );
    }
    EMSCRIPTEN_KEEPALIVE double cdda_rt_turnos_por_segundo()
    {
        return realtime::el_reloj().turnos_por_segundo;
    }
    EMSCRIPTEN_KEEPALIVE double cdda_rt_ms_turno()
    {
        return realtime::el_reloj().ms_turno_medio;
    }
    // en la web, la página lo llama en cada fotograma del navegador (requestAnimationFrame): si el juego está parado
    // esperando y algo se mueve, repinta. Así el movimiento suave va al ritmo del navegador, y no al de las esperas
    // del juego, que en el navegador duran más de lo pedido (salía una imagen por tic)
    EMSCRIPTEN_KEEPALIVE int cdda_rt_pintar()
    {
        return realtime::pintar_si_toca();
    }
    // lo que tarda en pintarse una imagen del mapa (ms, media)
    EMSCRIPTEN_KEEPALIVE double cdda_rt_ms_imagen()
    {
        return realtime::ms_imagen();
    }
    EMSCRIPTEN_KEEPALIVE int cdda_rt_retrasado()
    {
        return realtime::el_reloj().retrasado ? 1 : 0;
    }
    // el turno del juego (desde el principio del calendario) y la hora, para leerlos desde la página
    EMSCRIPTEN_KEEPALIVE int cdda_turno()
    {
        return to_turns<int>( calendar::turn - calendar::turn_zero );
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_hora()
    {
        static std::string s;
        s = to_string_time_of_day( calendar::turn );
        return s.c_str();
    }
    // cuántas ventanas de interfaz hay abiertas (la del juego y los menús encima)
    EMSCRIPTEN_KEEPALIVE int cdda_ventanas()
    {
        return static_cast<int>( ui_adaptor::ui_stack_size() );
    }
}
#endif

// ------------------------------------------------------------------ modo de medida (escritorio)
// Con CDDA_RT_BANCO en el entorno, el juego se maneja solo para medir el tiempo real sin nadie delante: un hilo mete
// en la cola de SDL (SDL_PushEvent vale desde otro hilo) «d» (Play Now! Default Scenario: sin traducciones no sale
// la pantalla del idioma y «New Game» ya está abierto) y luego F7
// cada 25 s (x1 -> x3 -> x10 -> x30 -> x72 -> máx). Las cifras salen en el registro (debug.log, «tiempo real: ...») y,
// si CDDA_RT_BANCO es un fichero, en él (una línea cada 10 s).
#if defined(TILES) && !defined(__EMSCRIPTEN__)
#include <SDL3/SDL.h>
namespace
{
void tecla_sdl( SDL_Keycode k, SDL_Scancode sc, const char *texto )
{
    SDL_Event e;
    SDL_zero( e );
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = sc;
    e.key.down = true;
    SDL_PushEvent( &e );
    if( texto != nullptr ) {
        SDL_Event t;
        SDL_zero( t );
        t.type = SDL_EVENT_TEXT_INPUT;
        t.text.text = texto;
        SDL_PushEvent( &t );
    }
    SDL_Event u;
    SDL_zero( u );
    u.type = SDL_EVENT_KEY_UP;
    u.key.key = k;
    u.key.scancode = sc;
    SDL_PushEvent( &u );
}
struct banco_t {
    banco_t() {
        if( std::getenv( "CDDA_RT_BANCO" ) == nullptr ) {
            return;
        }
        std::thread( []() {
            const auto dormir = []( int s ) {
                std::this_thread::sleep_for( std::chrono::seconds( s ) );
            };
            dormir( 40 );
            tecla_sdl( SDLK_D, SDL_SCANCODE_D, "d" );
            dormir( 70 );
            for( int i = 0; i < 5; i++ ) {
                dormir( 25 );
                tecla_sdl( SDLK_F7, SDL_SCANCODE_F7, nullptr );
            }
        } ).detach();
    }
} banco;
} // namespace
#endif
