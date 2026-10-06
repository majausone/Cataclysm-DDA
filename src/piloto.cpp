#include "piloto.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "character.h"
#include "character_id.h"
#include "coordinates.h"
#include "creature_tracker.h"
#include "game.h"
#include "line.h"
#include "map.h"
#include "messages.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static const faction_id faction_your_followers( "your_followers" );
static const faction_id faction_no_faction( "no_faction" );
static const string_id<npc_template> npc_observador( "survivor_camper" );
// lo que hace del observador un fantasma: invisible, intocable, sin necesidades, sin ruido ni olor
static const std::array<trait_id, 7> rasgos_observador = { {
        trait_id( "DEBUG_CLOAK" ), trait_id( "DEBUG_NODMG" ), trait_id( "DEBUG_PREVENT_DEATH" ), trait_id( "DEBUG_LS" ),
        trait_id( "DEBUG_NOTEMP" ), trait_id( "DEBUG_NOSCENT" ), trait_id( "DEBUG_SILENT" )
    }
};

namespace piloto
{
namespace
{
struct entrada {
    std::string hora, accion, categoria, meta;
};
// lo que va decidiendo cada uno (por su id), las últimas 40 decisiones distintas
std::map<int, std::deque<entrada>> registro;
bool encendido = false;
character_id personaje;   // el personaje del jugador, llevado por la IA mientras el piloto está puesto
character_id observador;  // el cuerpo del observador (con el piloto puesto, es el jugador)
int elegido = 0;          // a quién enseñan los paneles (0: al personaje)
std::optional<bool> pedido; // encender o apagar en el siguiente turno
std::string motivo;          // por qué se apagó solo
std::atomic<bool> npc_pedido{ false };
std::atomic<int> carga_pedida{ 0 };
const mtype_id mon_zombie_carga( "mon_zombie" );

std::string escapar( const std::string &s )
{
    std::string r;
    for( const char c : s ) {
        if( c == '"' || c == '\\' ) {
            r += '\\';
            r += c;
        } else if( c == '\n' ) {
            r += "\\n";
        } else if( static_cast<unsigned char>( c ) >= 0x20 ) {
            r += c;
        }
    }
    return r;
}

const char *nombre_mision( int m )
{
    switch( m ) {
        case NPC_MISSION_NULL:
            return "nada en particular";
        case NPC_MISSION_SHELTER:
            return "quedarse en el refugio";
        case NPC_MISSION_SHOPKEEP:
            return "tener la tienda";
        case NPC_MISSION_GUARD_ALLY:
        case NPC_MISSION_GUARD:
            return "vigilar";
        case NPC_MISSION_GUARD_PATROL:
            return "patrullar";
        case NPC_MISSION_ACTIVITY:
            return "una tarea";
        case NPC_MISSION_TRAVELLING:
            return "viajar";
        case NPC_MISSION_CAMP_RESIDENT:
            return "vivir en el campamento";
        default:
            return "?";
    }
}

npc *buscar_npc( const character_id &id )
{
    return id.is_valid() ? g->find_npc( id ) : nullptr;
}

// una casilla libre cerca de p (para el observador): andable y sin nadie
std::optional<tripoint_bub_ms> libre_cerca( const tripoint_bub_ms &p )
{
    map &here = get_map();
    creature_tracker &criaturas = get_creature_tracker();
    for( int r = 1; r <= 4; r++ ) {
        for( const tripoint_bub_ms &q : here.points_in_radius( p, r ) ) {
            if( rl_dist( q, p ) == r && here.passable( q ) && criaturas.creature_at( q ) == nullptr ) {
                return q;
            }
        }
    }
    return std::nullopt;
}
} // namespace

bool activo()
{
    return encendido;
}

void activar()
{
    if( encendido ) {
        return;
    }
    avatar &u = get_avatar();
    const std::optional<tripoint_bub_ms> donde = libre_cerca( u.pos_bub() );
    if( !donde ) {
        add_msg( m_bad, _( "Autopilot: no room nearby for the observer." ) );
        return;
    }
    // el cuerpo del observador: un NPC aliado al lado (el cambio de cuerpo del juego solo vale con aliados)
    map &here = get_map();
    const character_id id = here.place_npc( donde->xy(), npc_observador );
    g->load_npcs();
    npc *ob = g->find_npc( id );
    if( ob == nullptr ) {
        add_msg( m_bad, _( "Autopilot: could not create the observer." ) );
        return;
    }
    ob->set_fac( faction_your_followers );
    ob->set_attitude( NPCATT_FOLLOW );
    g->add_npc_follower( ob->getID() );
    personaje = u.getID();
    // el jugador pasa al cuerpo del observador; el personaje queda en el NPC
    u.control_npc( *ob );
    observador = u.getID();
    for( const trait_id &t : rasgos_observador ) {
        if( !u.has_trait( t ) ) {
            u.set_mutation( t );
        }
    }
    // el personaje, por su cuenta: ni sigue a nadie ni es de nadie (la IA de NPC de siempre decide qué hacer)
    if( npc *p = buscar_npc( personaje ) ) {
        g->remove_npc_follower( p->getID() );
        p->set_fac( faction_no_faction );
        p->set_attitude( NPCATT_NULL );
        p->set_mission( NPC_MISSION_NULL );
    }
    encendido = true;
    elegido = personaje.get_value();
    add_msg( m_info, _( "Autopilot on: the game's NPC AI is driving your character." ) );
}

void desactivar()
{
    if( !encendido ) {
        return;
    }
    encendido = false;
    npc *p = buscar_npc( personaje );
    avatar &u = get_avatar();
    if( p == nullptr || p->is_dead() ) {
        add_msg( m_bad, _( "Autopilot: your character is gone; you stay as the observer." ) );
        return;
    }
    // vuelve a ser aliado (para el cambio de cuerpo) y el jugador vuelve a él
    p->set_fac( faction_your_followers );
    p->set_attitude( NPCATT_FOLLOW );
    g->add_npc_follower( p->getID() );
    u.control_npc( *p );
    // el observador sobra: fuera, sin dejar nada
    if( npc *ob = buscar_npc( observador ) ) {
        const character_id id = ob->getID();
        g->remove_npc_follower( id );
        g->remove_npc( id );
        overmap_buffer.remove_npc( id );
    }
    observador = character_id();
    elegido = 0;
    add_msg( m_info, _( "Autopilot off: you have your character back." ) );
}

void alternar()
{
    if( encendido ) {
        desactivar();
    } else {
        activar();
    }
}

void pedir_npc_al_lado()
{
    npc_pedido = true;
}

void pedir_carga( int n )
{
    carga_pedida = n;
}

std::string motivo_apagado()
{
    return motivo;
}

void pedir( bool encender )
{
    pedido = encender;
}

static void poner_npc_al_lado()
{
    avatar &u = get_avatar();
    // (como mucho dos cerca: con más, el juego se pasa el rato en diálogos)
    int cerca = 0;
    for( const npc &n : g->all_npcs() ) {
        if( !n.is_dead() && rl_dist( n.pos_bub(), u.pos_bub() ) <= 3 ) {
            cerca++;
        }
    }
    if( cerca >= 2 ) {
        return;
    }
    // (mejor en una casilla de al lado: con un solo NPC al lado, «hablar» no pregunta con quién)
    const std::optional<tripoint_bub_ms> donde = libre_cerca( u.pos_bub() );
    if( !donde ) {
        return;
    }
    // uno al azar, sin plantilla, como los que salen por el mundo (como el «spawn NPC» del menú de depuración)
    shared_ptr_fast<npc> p = make_shared_fast<npc>();
    p->normalize();
    p->randomize();
    p->spawn_at_precise( get_map().get_abs( *donde ) );
    overmap_buffer.insert_npc( p );
    p->form_opinion( u );
    // (neutral: al azar, algunos salen atracadores y no dejan hacer otra cosa que contestarles)
    p->set_attitude( NPCATT_NULL );
    p->mission = NPC_MISSION_NULL;
    g->load_npcs();
}

void turno()
{
    if( npc_pedido.exchange( false ) ) {
        poner_npc_al_lado();
    }
    if( const int n = carga_pedida.exchange( 0 ) ) {
        // (para medir: el jugador, intocable, que si no le matan en seguida y no da tiempo)
        get_avatar().set_mutation( trait_id( "DEBUG_NODMG" ) );
        for( int i = 0; i < n; i++ ) {
            g->place_critter_around( mon_zombie_carga, get_avatar().pos_bub(), 30 );
        }
    }
    if( pedido ) {
        const bool encender = *pedido;
        pedido.reset();
        if( encender ) {
            activar();
        } else {
            desactivar();
        }
    }
    if( !encendido ) {
        return;
    }
    npc *p = buscar_npc( personaje );
    if( p == nullptr || p->is_dead() ) {
        motivo = p == nullptr ? "su personaje ya no está en el mapa" : "su personaje ha muerto";
        if( p != nullptr && p->get_killer() != nullptr ) {
            motivo += " (lo ha matado " + p->get_killer()->disp_name() + ")";
        }
        motivo += ", " + to_string_time_of_day( calendar::turn ) + "; últimos mensajes:";
        // (si ya no está, lo más probable es que haya muerto y el juego lo haya quitado: lo dicen los mensajes)
        for( const std::pair<std::string, std::string> &m : Messages::recent_messages( 8 ) ) {
            motivo += " [" + m.first + " " + m.second + "]";
        }
        desactivar();
        return;
    }
    avatar &u = get_avatar();
    // el observador, siempre cerca del personaje (si se aleja, el juego deja de simularlo)
    if( u.posz() != p->posz() || rl_dist( u.pos_bub(), p->pos_bub() ) > 4 ) {
        if( const std::optional<tripoint_bub_ms> q = libre_cerca( p->pos_bub() ) ) {
            g->place_player( *q, true );
        }
    }
    // y no hace nada: su turno pasa solo
    u.set_moves( 0 );
}

void apuntar( const npc &quien, const std::string &accion, const std::string &categoria,
              const std::string &meta )
{
    std::deque<entrada> &l = registro[quien.getID().get_value()];
    if( !l.empty() && l.back().accion == accion && l.back().meta == meta ) {
        return;
    }
    l.push_back( { to_string_time_of_day( calendar::turn ), accion, categoria, meta } );
    while( l.size() > 40 ) {
        l.pop_front();
    }
}

void seleccionar( const Character &quien )
{
    elegido = quien.getID().get_value();
}

void seleccionar_id( int id )
{
    elegido = id;
}

std::string estado_json()
{
    const int id = elegido != 0 ? elegido : ( encendido ? personaje.get_value() : get_avatar().getID().get_value() );
    const Character *c = nullptr;
    if( id == get_avatar().getID().get_value() ) {
        c = &get_avatar();
    } else if( npc *n = g->find_npc( character_id( id ) ) ) {
        c = n;
    }
    std::ostringstream o;
    o << "{\"piloto\":" << ( encendido ? "true" : "false" );
    o << ",\"hora\":\"" << escapar( to_string_time_of_day( calendar::turn ) ) << "\"";
    if( c == nullptr ) {
        o << ",\"elegido\":null}";
        return o.str();
    }
    const npc *n = dynamic_cast<const npc *>( c );
    o << ",\"elegido\":{\"id\":" << id << ",\"nombre\":\"" << escapar( c->get_name() ) << "\"";
    o << ",\"esPersonaje\":" << ( encendido && id == personaje.get_value() ? "true" : "false" );
    o << ",\"esJugador\":" << ( c->is_avatar() ? "true" : "false" );
    // ahora: la actividad en marcha, y la última decisión de su IA
    o << ",\"actividad\":\"" << escapar( c->activity ? c->activity.id().str() : std::string() ) << "\"";
    const auto it = registro.find( id );
    if( it != registro.end() && !it->second.empty() ) {
        const entrada &e = it->second.back();
        o << ",\"accion\":\"" << escapar( e.accion ) << "\",\"categoria\":\"" << escapar( e.categoria ) <<
          "\",\"meta\":\"" << escapar( e.meta ) << "\"";
    }
    // el plan: su misión y a dónde va
    if( n != nullptr ) {
        o << ",\"mision\":\"" << nombre_mision( static_cast<int>( n->mission ) ) << "\"";
        const tripoint_abs_omt aqui = n->pos_abs_omt();
        if( n->goal != tripoint_abs_omt::invalid ) {
            o << ",\"destino\":{\"dx\":" << ( n->goal.x() - aqui.x() ) << ",\"dy\":" << ( n->goal.y() - aqui.y() ) <<
              ",\"lugar\":\"" << escapar( overmap_buffer.ter( n->goal )->get_name( om_vision_level::full ) ) << "\"}";
        }
    }
    // las necesidades
    o << ",\"necesidades\":{\"hambre\":" << c->get_hunger() << ",\"sed\":" << c->get_thirst() << ",\"sueno\":" <<
      c->get_sleepiness() << ",\"aguante\":" << c->get_stamina() << ",\"dolor\":" << c->get_pain() <<
      ",\"animo\":" << c->get_morale_level() << ",\"vida\":" << c->get_hp() << ",\"vidaMax\":" << c->get_hp_max() << "}";
    // el registro, de lo más nuevo a lo más viejo
    o << ",\"registro\":[";
    if( it != registro.end() ) {
        bool primero = true;
        for( auto e = it->second.rbegin(); e != it->second.rend(); ++e ) {
            o << ( primero ? "" : "," ) << "{\"hora\":\"" << escapar( e->hora ) << "\",\"accion\":\"" << escapar(
                  e->accion ) << "\",\"categoria\":\"" << escapar( e->categoria ) << "\",\"meta\":\"" << escapar( e->meta ) <<
              "\"}";
            primero = false;
        }
    }
    o << "]}}";
    return o.str();
}

std::string lista_json()
{
    const avatar &u = get_avatar();
    std::ostringstream o;
    o << "[";
    bool primero = true;
    for( const npc &n : g->all_npcs() ) {
        if( observador.is_valid() && n.getID() == observador ) {
            continue;
        }
        o << ( primero ? "" : "," ) << "{\"id\":" << n.getID().get_value() << ",\"nombre\":\"" << escapar(
              n.get_name() ) << "\",\"distancia\":" << rl_dist( u.pos_bub(), n.pos_bub() ) << ",\"personaje\":" <<
          ( encendido && n.getID() == personaje ? "true" : "false" ) << "}";
        primero = false;
    }
    o << "]";
    return o.str();
}

} // namespace piloto

#if defined(__EMSCRIPTEN__)
extern "C" {
    EMSCRIPTEN_KEEPALIVE int cdda_piloto_activo()
    {
        return piloto::activo() ? 1 : 0;
    }
    EMSCRIPTEN_KEEPALIVE void cdda_piloto( int on )
    {
        // (se hace en el siguiente turno: desde JS el juego puede estar a mitad de algo)
        piloto::pedir( on != 0 );
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ia_estado()
    {
        static std::string s;
        s = piloto::estado_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ia_lista()
    {
        static std::string s;
        s = piloto::lista_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE void cdda_ia_seleccionar( int id )
    {
        piloto::seleccionar_id( id );
    }
    // un superviviente al lado del jugador (para probar el diálogo)
    EMSCRIPTEN_KEEPALIVE void cdda_sim_npc_al_lado()
    {
        piloto::pedir_npc_al_lado();
    }
    // n zombis alrededor (para medir en una zona cargada)
    EMSCRIPTEN_KEEPALIVE void cdda_sim_carga( int n )
    {
        piloto::pedir_carga( n );
    }
}
#endif
