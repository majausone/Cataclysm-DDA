#include "realtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "avatar.h"
#include "calendar.h"
#include "cached_options.h"
#include "game.h"
#include "input_context.h"
#include "options.h"
#include "output.h"
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
        case velocidad::x1:
            return 1;
        case velocidad::x3:
            return 3;
        case velocidad::x10:
            return 10;
        case velocidad::x30:
            return 30;
        case velocidad::x72:
            return 72;
        case velocidad::maxima:
            return -1;
    }
    return 1;
}

std::string nombre( velocidad v )
{
    switch( v ) {
        case velocidad::pausa:
            return _( "PAUSE" );
        case velocidad::maxima:
            return _( "MAX" );
        default:
            return "x" + std::to_string( multiplicador( v ) );
    }
}

velocidad de_texto( const std::string &s )
{
    if( s == "pausa" || s == "pause" ) {
        return velocidad::pausa;
    }
    if( s == "max" ) {
        return velocidad::maxima;
    }
    for( int i = 1; i < num_velocidades - 1; i++ ) {
        const velocidad v = static_cast<velocidad>( i );
        if( s == std::to_string( multiplicador( v ) ) ) {
            return v;
        }
    }
    return velocidad::x1;
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
    if( hay_plazo_ && ahora - plazo_ < periodo ) {
        // a tiempo (o con menos de un turno de retraso): el siguiente, un periodo después del plazo de este
        plazo_ += periodo;
        if( ++a_tiempo_ > m ) {
            retrasado = false;
        }
    } else {
        // va tarde más de un turno (o es el primero): sin acumular retraso, desde ahora
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
    ms_turno_medio = ms_turno_medio <= 0.0 ? ms_ultimo_turno : ms_turno_medio * 0.95 + ms_ultimo_turno *
                     0.05;
}

// ------------------------------------------------------------------ el del juego
namespace
{
std::optional<std::string> accion_pendiente;
double ms_espera_jugador = 0.0;
std::chrono::steady_clock::time_point inicio_espera_jugador;
bool esperando_jugador = false;
bool velocidad_de_opciones = false;

// ¿le toca al jugador decidir en este turno?  (si no, el turno pasa solo: actividad, sueño, moviéndose...)
bool jugador_decide()
{
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

void repintar()
{
    if( test_mode ) {
        return;
    }
    g->invalidate_main_ui_adaptor();
    ui_manager::redraw();
    refresh_display();
}
} // namespace

reloj &el_reloj()
{
    static reloj r;
    // la velocidad de partida, la de las opciones (una vez)
    if( !velocidad_de_opciones && !test_mode ) {
        velocidad_de_opciones = true;
        r.poner( de_texto( get_option<std::string>( "REALTIME_SPEED" ) ) );
    }
    return r;
}

bool activo()
{
    return !test_mode && get_option<bool>( "REALTIME" );
}

void poner( velocidad v )
{
    el_reloj().poner( v );
}

void subir()
{
    const int i = static_cast<int>( el_reloj().vel() );
    poner( static_cast<velocidad>( std::min( i + 1, num_velocidades - 1 ) ) );
}

void bajar()
{
    const int i = static_cast<int>( el_reloj().vel() );
    poner( static_cast<velocidad>( std::max( i - 1, 0 ) ) );
}

void alternar_pausa()
{
    static velocidad antes = velocidad::x1;
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
    if( !activo() ) {
        return;
    }
    const std::string que = get_option<std::string>( "REALTIME_DANGER" );
    reloj &r = el_reloj();
    if( que == "pause" ) {
        if( r.vel() != velocidad::pausa ) {
            alternar_pausa();
        }
    } else if( que == "x1" ) {
        if( static_cast<int>( r.vel() ) > static_cast<int>( velocidad::x1 ) ) {
            poner( velocidad::x1 );
        }
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
    auto ultimo_repintado = std::chrono::steady_clock::now();
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
        const int t = falta < 0 ? 100 : static_cast<int>( std::clamp<int64_t>( falta, 1, 50 ) );
        input_context ctxt = get_default_mode_input_context();
        const std::string action = ctxt.handle_input( t );
        if( action == "TIMEOUT" ) {
            // (que se vea la velocidad y lo que pasa, de vez en cuando, mientras espera)
            const auto ahora = std::chrono::steady_clock::now();
            if( ahora - ultimo_repintado > std::chrono::milliseconds( 250 ) ) {
                ultimo_repintado = ahora;
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
    if( action == "REALTIME_FASTER" ) {
        subir();
    } else if( action == "REALTIME_SLOWER" ) {
        bajar();
    } else if( action == "REALTIME_PAUSE" ) {
        alternar_pausa();
    } else {
        return false;
    }
    return true;
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
