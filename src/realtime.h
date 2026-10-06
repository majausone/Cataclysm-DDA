#pragma once
#ifndef CATA_SRC_REALTIME_H
#define CATA_SRC_REALTIME_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

// Tiempo real (rama tiempo-real): el reloj hace pasar los turnos solo, al ritmo de la velocidad elegida.
//  - Cada turno empieza cuando le toca (1 turno por segundo real a x1), sin esperar al jugador: si no ha
//    pulsado nada cuando se le acaba el turno, espera ese turno y el mundo sigue.
//  - Lo que pulsa mientras espera a que le toque se guarda y se hace en cuanto le toca: no se pierde.
//  - Las actividades largas y el sueño van también al ritmo del reloj.
//  - En pausa el juego es el de siempre: espera al jugador, y las actividades y el sueño se paran.
//  - Los menús paran el reloj solos: son modales y el turno no avanza mientras están abiertos.
//  - Si el ordenador no llega, va lo más rápido que puede y lo dice («va a xN»), sin acumular retraso.
namespace realtime
{

// Un tic es un turno del juego (1 segundo del mundo). Lo que se elige es cuánto dura un día: con el día normal van
// 24 tics por segundo real (un día, una hora). Andar, pelear y lo demás de la acción va a paso real con cualquier
// duración del día (ver factor_accion y TIEMPO-24.md); «máxima» (acelerar todo) lo acelera todo.
enum class velocidad : int { pausa = 0, lento, normal, rapido, maxima };
constexpr int num_velocidades = 5;

// tics por segundo real: 0 en pausa, -1 a la máxima (lento 12, normal 24, rápido 48)
int multiplicador( velocidad v );
// lo que se pinta: "PAUSE", "Day 2h", "Day 1h", "Day 30m", "FAST"
std::string nombre( velocidad v );
// de una cadena de las opciones ("pausa", "lento", "normal", "rapido", "max") a la velocidad; normal si no se conoce
velocidad de_texto( const std::string &s );

// --- los dos relojes (TIEMPO-24.md)
// cuántos tics hay en un segundo real de la acción: los puntos de movimiento de cada criatura se reparten entre
// ellos. 12, 24 o 48 según la duración del día (24 en pausa y a la máxima); 1 si el tiempo real no está activo (y en
// las pruebas): entonces todo es como siempre
int factor_accion();
// los puntos de movimiento de este tic para quien tiene esa velocidad: su parte del reparto entre factor tics (sin
// perder restos: en factor tics consecutivos, exactamente la velocidad). Al ritmo del mundo (en una actividad
// larga, durmiendo), la velocidad entera
int puntos_por_tic( int velocidad_criatura, bool al_ritmo_del_mundo );
int reparto( int velocidad_criatura, int64_t tic, int factor );
// ¿le toca en este tic a lo que va a velocidad real y se hace «una vez por turno» (fuego, humo, olor, vehículos,
// efectos de combate...)? Una vez cada factor tics; fase, para repartirlos y que no caigan todos en el mismo tic
bool tic_de_accion( int fase );
bool es_tic_de_accion( int64_t tic, int fase, int factor );
// de un intervalo de turnos, cuántos son de acción (para lo que se actualiza con lo que ha pasado: el aguante)
int turnos_de_accion( int turnos, int fase );
// ¿este efecto va a velocidad real? (aturdido, derribado, sangrado, agarrado, ardiendo...)
bool efecto_real( const std::string &id );

// El reloj, con el tiempo inyectable (para las pruebas). plazo: cuándo puede empezar el siguiente turno.
class reloj
{
    public:
        using instante = std::chrono::steady_clock::time_point;

        explicit reloj( std::function<instante()> ahora = []() {
            return std::chrono::steady_clock::now();
        } );

        velocidad vel() const {
            return vel_;
        }
        void poner( velocidad v );
        // ¿puede empezar ya el siguiente turno?  (a la máxima, siempre; en pausa, nunca)
        bool toca() const;
        // ms que faltan para que pueda empezar el siguiente turno (0 si ya puede; -1 en pausa: sin límite)
        int64_t ms_hasta_turno() const;
        // empieza un turno: apunta el plazo del siguiente (sin acumular retraso si va tarde)
        void empezar();
        // el turno ha terminado de calcularse (sin contar lo que se ha esperado al jugador)
        void fin_de_calculo( double ms_esperando_al_jugador );

        // medidas
        double ms_ultimo_turno = 0.0;      // lo que tardó en calcularse el último turno
        double ms_turno_medio = 0.0;       // media móvil de lo anterior
        double turnos_por_segundo = 0.0;   // turnos por segundo real, media móvil
        bool retrasado = false;            // ¿no llega a la velocidad pedida?
        int64_t turnos = 0;                // turnos empezados

    private:
        std::function<instante()> ahora_;
        velocidad vel_ = velocidad::normal;
        bool hay_plazo_ = false;
        instante plazo_;
        instante ultimo_inicio_;
        bool hay_inicio_ = false;
        int a_tiempo_ = 0;                 // turnos seguidos a tiempo (para quitar «retrasado»)
};

// el reloj del juego
reloj &el_reloj();
// ¿está activo el tiempo real?  (opción REALTIME; nunca en las pruebas)
bool activo();
void poner( velocidad v );
void subir();
void bajar();
void alternar_pausa();
// un peligro nuevo a la vista (el «safe mode»): baja a x1 o pausa, según la opción REALTIME_DANGER
void peligro();

// En game::do_turn, al empezar cada turno: espera a que le toque, leyendo el teclado (velocidad, pausa,
// interrumpir la actividad; y lo que pulse el jugador lo guarda para su turno).
void esperar_turno();
// En game::get_player_input: ¿se le ha acabado al jugador el tiempo de este turno?  Si no está activo,
// devuelve lo de antes (la opción TURN_DURATION del juego).
bool plazo_vencido( bool vencido_sin_tiempo_real );
// cuántos ms esperar el teclado como mucho en cada vuelta
int ms_espera_teclado();
// lo que se ha esperado al jugador (para no contarlo en lo que tarda un turno)
void empieza_espera_jugador();
void acaba_espera_jugador();

// una acción del jugador pulsada mientras esperaba su turno: se hace en cuanto le toca
bool hay_accion_pendiente();
std::string tomar_accion_pendiente();
// si es una de las acciones del tiempo real (velocidad, pausa, piloto automático), la hace y devuelve true
bool manejar_accion( const std::string &action );

// ¿repintar ahora?  Hasta x10, siempre; más rápido, como mucho cada 40 ms (25 por segundo, de sobra para verlo):
// repintar la pantalla entera en cada turno es lo que más frena a las velocidades altas
bool conviene_repintar();
// ¿pasar el turno del jugador sin mirar el teclado?  A más de x10, el teclado se mira como mucho cada 30 ms: cada vez
// que se mira, el juego le cede el control al navegador (o al sistema) y eso pone un tope de unos 30 turnos por
// segundo. Lo pulsado entretanto se lee en la siguiente mirada (30 ms después como mucho).
bool saltar_espera();
// lo que se pinta en pantalla: «x1», «PAUSA», «x72 (va a x41)»...
std::string texto_estado();
// la ventanita de la velocidad, siempre a la vista (se crea la primera vez)
void mostrar_estado();

} // namespace realtime

#endif // CATA_SRC_REALTIME_H
