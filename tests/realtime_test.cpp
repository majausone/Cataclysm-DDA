#include <chrono>
#include <string>

#include "cata_catch.h"
#include "realtime.h"

// El reloj del tiempo real (src/realtime.h) con un tiempo de mentira: cada prueba avanza el reloj a mano.
namespace
{
using instante = realtime::reloj::instante;

struct tiempo_falso {
    instante t = instante() + std::chrono::hours( 1 );
    instante ahora() const {
        return t;
    }
    void pasar_ms( double ms ) {
        t += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                 std::chrono::duration<double, std::milli>( ms ) );
    }
};

// cuántos turnos empiezan en «segundos» de tiempo, si cada turno tarda «ms_calculo» en calcularse
int turnos_en( realtime::reloj &r, tiempo_falso &tf, double segundos, double ms_calculo = 0.0 )
{
    const instante fin = tf.t + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                             std::chrono::duration<double>( segundos ) );
    int n = 0;
    while( tf.t < fin ) {
        if( r.toca() ) {
            r.empezar();
            n++;
            tf.pasar_ms( ms_calculo );
        } else {
            tf.pasar_ms( 0.5 );
        }
    }
    return n;
}
} // namespace

TEST_CASE( "realtime_speeds_give_N_turns_per_real_second", "[realtime]" )
{
    for( const realtime::velocidad v : {
             realtime::velocidad::x1, realtime::velocidad::x3, realtime::velocidad::x10,
             realtime::velocidad::x30, realtime::velocidad::x72
         } ) {
        tiempo_falso tf;
        realtime::reloj r( [&tf]() {
            return tf.ahora();
        } );
        r.poner( v );
        const int n = realtime::multiplicador( v );
        CAPTURE( realtime::nombre( v ) );
        // en 10 segundos de reloj, 10·N turnos (más el primero, que empieza al momento)
        const int turnos = turnos_en( r, tf, 10.0 );
        CHECK( turnos >= 10 * n );
        CHECK( turnos <= 10 * n + 1 );
        CHECK_FALSE( r.retrasado );
    }
}

TEST_CASE( "realtime_pause_never_advances_and_max_always_does", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::pausa );
    CHECK_FALSE( r.toca() );
    CHECK( r.ms_hasta_turno() == -1 );
    CHECK( turnos_en( r, tf, 5.0 ) == 0 );

    r.poner( realtime::velocidad::maxima );
    CHECK( r.toca() );
    CHECK( r.ms_hasta_turno() == 0 );
    r.empezar();
    CHECK( r.toca() );
}

TEST_CASE( "realtime_waits_for_the_deadline_between_turns", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::x1 );
    REQUIRE( r.toca() );
    r.empezar();
    // a x1, el siguiente turno, un segundo después
    CHECK_FALSE( r.toca() );
    CHECK( r.ms_hasta_turno() > 990 );
    tf.pasar_ms( 999.0 );
    CHECK_FALSE( r.toca() );
    tf.pasar_ms( 1.0 );
    CHECK( r.toca() );
}

TEST_CASE( "realtime_falls_behind_without_debt_and_says_so", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::x72 );
    // cada turno tarda 50 ms (no llega a x72, que pide 13,9 ms): va lo más rápido que puede, unos 20 por segundo
    const int lentos = turnos_en( r, tf, 10.0, 50.0 );
    CHECK( lentos >= 190 );
    CHECK( lentos <= 201 );
    CHECK( r.retrasado );
    // y cuando vuelve a ir ligero, a x72 sin ráfaga para «recuperar» lo perdido
    const int ligeros = turnos_en( r, tf, 2.0, 1.0 );
    CHECK( ligeros <= 2 * 72 + 2 );
    CHECK( ligeros >= 2 * 72 - 2 );
    CHECK_FALSE( r.retrasado );
}

TEST_CASE( "realtime_changing_speed_starts_counting_from_now", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::x1 );
    r.empezar();
    tf.pasar_ms( 100.0 );
    // a x72 en mitad de un turno de x1: el siguiente ya puede empezar (sin esperar al plazo de x1)
    r.poner( realtime::velocidad::x72 );
    CHECK( r.toca() );
}

TEST_CASE( "realtime_speed_names_and_options", "[realtime]" )
{
    CHECK( realtime::de_texto( "pausa" ) == realtime::velocidad::pausa );
    CHECK( realtime::de_texto( "1" ) == realtime::velocidad::x1 );
    CHECK( realtime::de_texto( "72" ) == realtime::velocidad::x72 );
    CHECK( realtime::de_texto( "max" ) == realtime::velocidad::maxima );
    CHECK( realtime::de_texto( "lo que sea" ) == realtime::velocidad::x1 );
    CHECK( realtime::multiplicador( realtime::velocidad::x30 ) == 30 );
    CHECK( realtime::nombre( realtime::velocidad::x10 ) == "x10" );
}

TEST_CASE( "realtime_is_off_in_tests", "[realtime]" )
{
    // en las pruebas no hay tiempo real: las demás pruebas del juego van como siempre
    CHECK_FALSE( realtime::activo() );
}
