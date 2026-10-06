#include <string>

#include "avatar.h"
#include "cata_catch.h"
#include "character.h"
#include "coordinates.h"
#include "game.h"
#include "map_helpers.h"
#include "npc.h"
#include "piloto.h"
#include "player_helpers.h"
#include "type_id.h"

// El piloto automático (src/piloto.h): el registro de lo que decide la IA, el estado para los paneles y el cambio
// de cuerpo (el personaje pasa a la IA de NPC y el jugador se queda de observador invisible; y vuelta).

static const trait_id trait_DEBUG_CLOAK( "DEBUG_CLOAK" );

TEST_CASE( "piloto_registro_y_estado", "[piloto]" )
{
    clear_map();
    clear_avatar();
    npc &guy = spawn_npc( get_avatar().pos_bub().xy() + point_rel_ms::east * 3, "test_talker" );
    // la misma decisión dos veces seguidas se apunta una vez
    piloto::apuntar( guy, "npc_pause", "idle", "descansar" );
    piloto::apuntar( guy, "npc_pause", "idle", "descansar" );
    piloto::apuntar( guy, "npc_eat", "needs", "comer" );
    piloto::seleccionar( guy );
    const std::string j = piloto::estado_json();
    CAPTURE( j );
    CHECK( j.find( "\"nombre\":\"" + guy.get_name() + "\"" ) != std::string::npos );
    CHECK( j.find( "\"accion\":\"npc_eat\"" ) != std::string::npos );
    CHECK( j.find( "\"necesidades\":{" ) != std::string::npos );
    // en el registro, de lo más nuevo a lo más viejo, y sin repetir
    const size_t comer = j.find( "\"meta\":\"comer\"" ), descansar = j.find( "\"meta\":\"descansar\"" );
    CHECK( comer < descansar );
    CHECK( j.find( "\"meta\":\"descansar\"", descansar + 1 ) == std::string::npos );
    CHECK( piloto::lista_json().find( guy.get_name() ) != std::string::npos );
}

TEST_CASE( "piloto_cambia_de_cuerpo_y_vuelve", "[piloto]" )
{
    clear_map();
    clear_avatar();
    avatar &u = get_avatar();
    const std::string nombre = u.get_name();
    const character_id id = u.getID();
    REQUIRE_FALSE( piloto::activo() );

    piloto::activar();
    REQUIRE( piloto::activo() );
    // el jugador es ahora el observador (otro, invisible) y el personaje, un NPC por su cuenta
    CHECK( get_avatar().getID() != id );
    CHECK( get_avatar().has_trait( trait_DEBUG_CLOAK ) );
    npc *personaje = g->find_npc( id );
    REQUIRE( personaje != nullptr );
    CHECK( personaje->get_name() == nombre );
    CHECK_FALSE( personaje->is_player_ally() );
    piloto::turno();
    CHECK( get_avatar().get_moves() == 0 );

    piloto::desactivar();
    CHECK_FALSE( piloto::activo() );
    CHECK( get_avatar().getID() == id );
    CHECK( get_avatar().get_name() == nombre );
    CHECK_FALSE( get_avatar().has_trait( trait_DEBUG_CLOAK ) );
}
