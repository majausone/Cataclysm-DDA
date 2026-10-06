#include <chrono>
#include <memory>
#include <cstdio>
#include <string>

#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "player_helpers.h"
#include "type_id.h"
#include "units.h"

// Cuánto tarda un turno (game::do_turn) en el mapa de pruebas, sin bichos. Dice hasta qué velocidad del tiempo real aguanta (1000 / ms por turno, en turnos por segundo).
// Oculta: ./tests/cata_test.exe "[realtime_bench]"
// (el jugador, con los movimientos muy en negativo: así el turno no le pide nada y solo se calcula el mundo)


namespace
{
double ms_por_turno( int turnos )
{
    get_avatar().set_moves( -1000000 );
    // (unos turnos para que se asiente)
    for( int i = 0; i < 10; i++ ) {
        g->do_turn();
    }
    const auto t0 = std::chrono::steady_clock::now();
    for( int i = 0; i < turnos; i++ ) {
        g->do_turn();
    }
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>( t1 - t0 ).count() / turnos;
}

void informar( const std::string &que, double ms )
{
    printf( "[realtime_bench] %-34s %8.3f ms/turno  -> aguanta hasta %7.0f turnos/s\n", que.c_str(), ms,
            ms > 0 ? 1000.0 / ms : 0.0 );
    fflush( stdout );
}

} // namespace

TEST_CASE( "realtime_turn_cost", "[.][realtime_bench]" )
{
    clear_map();
    clear_avatar();
    set_time_to_day();
    informar( "mapa vacío", ms_por_turno( 300 ) );
    // (con bichos a la vista, el juego le pide teclas al jugador y en las pruebas no se puede: los sitios cargados
    // se miden en la partida de verdad, en la versión web; ver TIEMPO-REAL.md)
    CHECK( true );
}
