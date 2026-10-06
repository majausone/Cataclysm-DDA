#include "suave.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "avatar.h"
#include "avatar_action.h"
#include "cached_options.h"
#include "character.h"
#include "coordinates.h"
#include "creature.h"
#include "creature_tracker.h"
#include "map.h"
#include "realtime.h"
#include "trap.h"
#include "vpart_position.h"

namespace suave
{
namespace
{
using reloj_t = std::chrono::steady_clock;

struct paso_t {
    tripoint_abs_ms casilla;        // donde está ahora (para el juego)
    double desde_x = 0.0;           // de dónde sale el dibujo (en casillas, relativo a casilla)
    double desde_y = 0.0;
    reloj_t::time_point inicio;     // cuándo empezó el paso
    double duracion = 1.0;          // cuánto dura, en segundos reales
    reloj_t::time_point visto;      // la última vez que se pintó
};

std::unordered_map<const Creature *, paso_t> &pasos()
{
    static std::unordered_map<const Creature *, paso_t> p;
    return p;
}

double segundos( reloj_t::time_point desde, reloj_t::time_point hasta )
{
    return std::chrono::duration<double>( hasta - desde ).count();
}

// lo que dura un paso a la casilla p: su coste en puntos de movimiento entre su velocidad, en segundos reales (con
// el día normal, 100 puntos son un segundo: lo que una persona normal tarda en andar una casilla)
double duracion_paso( const Creature &c, const tripoint_abs_ms &de, const tripoint_abs_ms &a )
{
    const map &here = get_map();
    const tripoint_bub_ms p = here.get_bub( a );
    int coste = 100;
    if( here.inbounds( p ) ) {
        coste = std::max( 50, here.move_cost( p ) * 50 );
    }
    // (en diagonal, lo que cueste: el juego cuenta igual o algo más según la opción; aquí, raíz de 2 si se ve en
    // diagonal, para que no parezca que va más deprisa)
    if( de.x() != a.x() && de.y() != a.y() ) {
        coste = static_cast<int>( coste * 1.2 );
    }
    if( const Character *ch = c.as_character() ) {
        coste = static_cast<int>( coste * ch->run_cost( 100, false ) / 100.0 );
    }
    const int velocidad = std::max( 1, c.get_speed() );
    const double factor = realtime::factor_accion();
    // (a más tics por segundo que el día normal (solo en las pruebas), más deprisa)
    const double escala = factor > 0 ? 24.0 / factor : 1.0;
    return std::clamp( static_cast<double>( coste ) / velocidad * escala, 0.08, 2.0 );
}

// cuánto lleva del paso (de 0 a 1), con una curva suave al llegar
double avance( const paso_t &p, reloj_t::time_point ahora )
{
    if( p.duracion <= 0.0 ) {
        return 1.0;
    }
    return std::clamp( segundos( p.inicio, ahora ) / p.duracion, 0.0, 1.0 );
}
} // namespace

bool activo()
{
    return use_tiles && realtime::activo();
}

point desfase( const Creature &c, int ancho_casilla, int alto_casilla )
{
    if( !activo() ) {
        return point::zero;
    }
    const reloj_t::time_point ahora = reloj_t::now();
    const tripoint_abs_ms aqui = c.pos_abs();
    auto it = pasos().find( &c );
    if( it == pasos().end() ) {
        paso_t p;
        p.casilla = aqui;
        p.inicio = ahora - std::chrono::seconds( 10 );
        p.visto = ahora;
        pasos().emplace( &c, p );
        return point::zero;
    }
    paso_t &p = it->second;
    p.visto = ahora;
    if( aqui != p.casilla ) {
        const int dx = aqui.x() - p.casilla.x();
        const int dy = aqui.y() - p.casilla.y();
        if( aqui.z() != p.casilla.z() || std::abs( dx ) > 2 || std::abs( dy ) > 2 ) {
            // (un salto: escaleras, teletransporte... sin deslizarse)
            p.desde_x = 0.0;
            p.desde_y = 0.0;
            p.duracion = 0.0;
        } else {
            // sale desde donde se le ve ahora (si estaba a mitad de otro paso, desde ahí)
            const double t = avance( p, ahora );
            const double visto_x = p.desde_x * ( 1.0 - t );
            const double visto_y = p.desde_y * ( 1.0 - t );
            p.desde_x = visto_x - dx;
            p.desde_y = visto_y - dy;
            p.duracion = duracion_paso( c, p.casilla, aqui );
        }
        p.casilla = aqui;
        p.inicio = ahora;
    }
    const double t = avance( p, ahora );
    if( t >= 1.0 ) {
        return point::zero;
    }
    return point( static_cast<int>( std::lround( p.desde_x * ( 1.0 - t ) * ancho_casilla ) ),
                  static_cast<int>( std::lround( p.desde_y * ( 1.0 - t ) * alto_casilla ) ) );
}

point camara( int ancho_casilla, int alto_casilla )
{
    if( !activo() ) {
        return point::zero;
    }
    const point d = desfase( get_avatar(), ancho_casilla, alto_casilla );
    return point( -d.x, -d.y );
}

bool hay_movimiento()
{
    if( !activo() ) {
        return false;
    }
    const reloj_t::time_point ahora = reloj_t::now();
    return std::any_of( pasos().begin(), pasos().end(), [ahora]( const auto & e ) {
        return segundos( e.second.visto, ahora ) < 1.0 && avance( e.second, ahora ) < 1.0;
    } );
}

namespace
{
int imagenes_pintadas = 0;
} // namespace

int imagenes()
{
    return imagenes_pintadas;
}

void nueva_imagen()
{
    imagenes_pintadas++;
    const reloj_t::time_point ahora = reloj_t::now();
    for( auto it = pasos().begin(); it != pasos().end(); ) {
        if( segundos( it->second.visto, ahora ) > 5.0 ) {
            it = pasos().erase( it );
        } else {
            ++it;
        }
    }
}

namespace
{
struct paso_jugador_t {
    bool hay = false;
    tripoint_abs_ms desde;
    int moves_antes = 0;
    reloj_t::time_point cuando;
};
paso_jugador_t paso_jugador;
} // namespace

void anotar_paso_jugador( const tripoint_abs_ms &desde, int moves_antes )
{
    paso_jugador.hay = true;
    paso_jugador.desde = desde;
    paso_jugador.moves_antes = moves_antes;
    paso_jugador.cuando = reloj_t::now();
}

bool girar_jugador( int dx, int dy )
{
    if( !paso_jugador.hay || ( dx == 0 && dy == 0 ) || !realtime::activo() ) {
        return false;
    }
    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_abs_ms a = paso_jugador.desde;
    const tripoint_abs_ms b = u.pos_abs();
    const tripoint_abs_ms c = a + tripoint_rel_ms( dx, dy, 0 );
    // (solo en el primer tercio del paso: más tarde, ya casi ha llegado y el giro sale del paso siguiente)
    const auto it = pasos().find( &u );
    const double duracion = it != pasos().end() ? std::max( 0.1, it->second.duracion ) : 1.0;
    if( segundos( paso_jugador.cuando, reloj_t::now() ) > duracion / 3.0 ) {
        paso_jugador.hay = false;
        return false;
    }
    if( b == a || b.z() != a.z() || std::abs( b.x() - a.x() ) > 1 || std::abs( b.y() - a.y() ) > 1 ||
        ( b.x() - a.x() == dx && b.y() - a.y() == dy ) || c == b ) {
        return false;
    }
    const tripoint_bub_ms ab = here.get_bub( a );
    const tripoint_bub_ms bb = here.get_bub( b );
    const tripoint_bub_ms cb = here.get_bub( c );
    if( !here.inbounds( ab ) || !here.inbounds( cb ) || here.impassable( cb ) ||
        get_creature_tracker().creature_at( cb ) != nullptr || here.veh_at( cb ) || here.veh_at( bb ) ||
        here.veh_at( ab ) || !here.tr_at( bb ).is_null() || here.has_field_at( bb ) || u.is_mounted() ||
        u.in_vehicle ) {
        return false;
    }
    paso_jugador.hay = false;
    const int moves_ahora = u.get_moves();
    u.setpos( here, ab, false );
    u.set_moves( paso_jugador.moves_antes );
    if( !avatar_action::move( u, here, tripoint_rel_ms( dx, dy, 0 ) ) ) {
        // (no ha podido: se queda donde iba, con lo que le costó)
        u.setpos( here, bb, false );
        u.set_moves( moves_ahora );
        return false;
    }
    return true;
}

} // namespace suave

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
extern "C" {
    // cuántas imágenes del mapa se han pintado (para medir las imágenes por segundo desde la página)
    EMSCRIPTEN_KEEPALIVE int cdda_imagenes()
    {
        return suave::imagenes();
    }
}
#endif
