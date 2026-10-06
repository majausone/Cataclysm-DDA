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
             realtime::velocidad::lento, realtime::velocidad::normal, realtime::velocidad::rapido
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
    r.poner( realtime::velocidad::lento );
    REQUIRE( r.toca() );
    r.empezar();
    // con el día lento (12 tics por segundo), el siguiente tic 83,3 ms después
    CHECK_FALSE( r.toca() );
    CHECK( r.ms_hasta_turno() > 80 );
    tf.pasar_ms( 83.0 );
    CHECK_FALSE( r.toca() );
    tf.pasar_ms( 0.5 );
    CHECK( r.toca() );
}

TEST_CASE( "realtime_falls_behind_without_debt_and_says_so", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::rapido );
    // cada turno tarda 50 ms (no llega al día rápido, que pide 20,8 ms): va lo más rápido que puede, unos 20 por segundo
    const int lentos = turnos_en( r, tf, 10.0, 50.0 );
    CHECK( lentos >= 190 );
    CHECK( lentos <= 201 );
    CHECK( r.retrasado );
    // y cuando vuelve a ir ligero, a 48 por segundo sin ráfaga para «recuperar» lo perdido (como mucho los 100 ms
    // del margen)
    const int ligeros = turnos_en( r, tf, 2.0, 1.0 );
    CHECK( ligeros <= 2 * 48 + 6 );
    CHECK( ligeros >= 2 * 48 - 2 );
    CHECK_FALSE( r.retrasado );
}

TEST_CASE( "realtime_frame_yields_are_made_up_within_the_margin", "[realtime]" )
{
    // como en el navegador: turnos de 4 ms, y cada 30 ms el juego cede el control y se le van 17 ms (un fotograma).
    // Los turnos que tocaban mientras se hacen seguidos después, y con el día rápido pasan 48 por segundo
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::rapido );
    const instante fin = tf.t + std::chrono::seconds( 10 );
    instante ultima_cesion = tf.t;
    int n = 0;
    while( tf.t < fin ) {
        if( tf.t - ultima_cesion >= std::chrono::milliseconds( 30 ) ) {
            ultima_cesion = tf.t;
            tf.pasar_ms( 17.0 );
        }
        if( r.toca() ) {
            r.empezar();
            n++;
            tf.pasar_ms( 4.0 );
        } else {
            tf.pasar_ms( 0.5 );
        }
    }
    CHECK( n >= 10 * 48 - 5 );
    CHECK( n <= 10 * 48 + 5 );
}

TEST_CASE( "realtime_changing_speed_starts_counting_from_now", "[realtime]" )
{
    tiempo_falso tf;
    realtime::reloj r( [&tf]() {
        return tf.ahora();
    } );
    r.poner( realtime::velocidad::lento );
    r.empezar();
    tf.pasar_ms( 50.0 );
    // al día rápido en mitad de un tic del lento: el siguiente ya puede empezar (sin esperar al plazo del lento)
    r.poner( realtime::velocidad::rapido );
    CHECK( r.toca() );
}

TEST_CASE( "realtime_speed_names_and_options", "[realtime]" )
{
    CHECK( realtime::de_texto( "pausa" ) == realtime::velocidad::pausa );
    CHECK( realtime::de_texto( "lento" ) == realtime::velocidad::lento );
    CHECK( realtime::de_texto( "normal" ) == realtime::velocidad::normal );
    CHECK( realtime::de_texto( "rapido" ) == realtime::velocidad::rapido );
    CHECK( realtime::de_texto( "max" ) == realtime::velocidad::maxima );
    CHECK( realtime::de_texto( "lo que sea" ) == realtime::velocidad::normal );
    CHECK( realtime::multiplicador( realtime::velocidad::normal ) == 24 );
    CHECK( realtime::nombre( realtime::velocidad::normal ) == "Day 1h" );
}

TEST_CASE( "realtime_movement_points_are_shared_out_among_the_tics", "[realtime]" )
{
    // en factor tics seguidos, exactamente la velocidad (sin perder restos), y en cada uno casi lo mismo
    for( const int factor : { 12, 24, 48 } ) {
        for( const int velocidad : { 100, 87, 7, 250 } ) {
            CAPTURE( factor, velocidad );
            for( const int64_t desde : { 0, 5, 1000003 } ) {
                int suma = 0;
                int minimo = velocidad;
                int maximo = 0;
                for( int64_t t = desde; t < desde + factor; t++ ) {
                    const int p = realtime::reparto( velocidad, t, factor );
                    suma += p;
                    minimo = std::min( minimo, p );
                    maximo = std::max( maximo, p );
                }
                CHECK( suma == velocidad );
                CHECK( maximo - minimo <= 1 );
            }
        }
    }
    // sin tiempo real (factor 1), la velocidad entera en cada tic: como siempre
    CHECK( realtime::reparto( 100, 7, 1 ) == 100 );
    // y en las pruebas no hay reparto: el resto de pruebas del juego van como siempre
    CHECK( realtime::factor_accion() == 1 );
    CHECK( realtime::puntos_por_tic( 100, false ) == 100 );
}

TEST_CASE( "realtime_npcs_and_monsters_get_a_whole_turn_once_per_real_second", "[realtime]" )
{
    // en factor tics seguidos, una vez la velocidad entera (en el tic de su fase) y nada en los demás; fases
    // distintas, tics distintos
    for( const int factor : { 12, 24, 48 } ) {
        for( const int64_t fase : { 0, 7, 1000003 } ) {
            int suma = 0;
            int veces = 0;
            for( int64_t t = 50; t < 50 + factor; t++ ) {
                const int p = realtime::turno_entero( 100, t, fase, factor );
                suma += p;
                veces += p > 0 ? 1 : 0;
            }
            CHECK( suma == 100 );
            CHECK( veces == 1 );
        }
    }
    CHECK( realtime::turno_entero( 100, 3, 5, 1 ) == 100 );
    // en las pruebas, como siempre
    CHECK( realtime::puntos_por_turno( 100, 5, false ) == 100 );
}

TEST_CASE( "realtime_action_tics_come_once_per_real_second", "[realtime]" )
{
    // una vez cada factor tics, en el tic que le toca a su fase
    for( const int factor : { 12, 24, 48 } ) {
        for( const int fase : { 0, 5, 20 } ) {
            int veces = 0;
            for( int64_t t = 100; t < 100 + 10 * factor; t++ ) {
                veces += realtime::es_tic_de_accion( t, fase, factor ) ? 1 : 0;
            }
            CHECK( veces == 10 );
        }
    }
    // fases distintas, tics distintos (para repartirlos)
    int coinciden = 0;
    for( int64_t t = 0; t < 24; t++ ) {
        coinciden += realtime::es_tic_de_accion( t, 0, 24 ) && realtime::es_tic_de_accion( t, 10, 24 ) ? 1 : 0;
    }
    CHECK( coinciden == 0 );
    CHECK( realtime::efecto_real( "stunned" ) );
    CHECK( realtime::efecto_real( "bleed" ) );
    CHECK_FALSE( realtime::efecto_real( "hunger" ) );
}

TEST_CASE( "realtime_is_off_in_tests", "[realtime]" )
{
    // en las pruebas no hay tiempo real: las demás pruebas del juego van como siempre
    CHECK_FALSE( realtime::activo() );
}
