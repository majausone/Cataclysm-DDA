#include <string>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "cata_catch.h"
#include "debug.h"
#include "item.h"
#include "item_location.h"
#include "map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "type_id.h"

// Fallos que ha cazado el modo simulación (src/simulacion.h) jugando en tiempo real, para que no vuelvan.

static const itype_id itype_apple( "apple" );

TEST_CASE( "comer_algo_que_desaparece_a_mitad_no_es_un_error", "[tiempo_real]" )
{
    // Se empieza a comer una manzana del suelo y, mientras se come, desaparece (en el juego: un NPC la coge; en tiempo
    // real el mundo sigue mientras comes). Antes salía el aviso «Item location/name to be consumed should not be
    // null.» (activity_actor.cpp), que en las pruebas hace fallar; ahora solo se dice que ya no está.
    clear_map();
    clear_avatar();
    avatar &u = get_avatar();
    map &here = get_map();
    item &manzana = here.add_item( u.pos_bub(), item( itype_apple ) );
    u.assign_activity( consume_activity_actor( item_location( map_cursor( u.pos_bub() ), &manzana ) ) );
    REQUIRE( u.activity );
    here.i_clear( u.pos_bub() );
    const std::string avisos = capture_debugmsg_during( [&u]() {
        for( int i = 0; i < 2000 && u.activity; i++ ) {
            u.set_moves( u.get_speed() );
            u.activity.do_turn( u );
        }
    } );
    CHECK( avisos.empty() );
    CHECK_FALSE( u.activity );
}
