#include "suave.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "avatar.h"
#include "cached_options.h"
#include "character.h"
#include "coordinates.h"
#include "creature.h"
#include "map.h"
#include "realtime.h"

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

void nueva_imagen()
{
    const reloj_t::time_point ahora = reloj_t::now();
    for( auto it = pasos().begin(); it != pasos().end(); ) {
        if( segundos( it->second.visto, ahora ) > 5.0 ) {
            it = pasos().erase( it );
        } else {
            ++it;
        }
    }
}

} // namespace suave
