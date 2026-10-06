#include <string>

#include "avatar.h"
#include "cata_catch.h"
#include "calendar.h"
#include "coordinates.h"
#include "game.h"
#include "interfaz.h"
#include "item.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "map_helpers.h"
#include "npc.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "player_helpers.h"
#include "type_id.h"

// La interfaz web (src/interfaz.h, Encargo 7 B): el estado que se le da a la página, en JSON que se pueda leer, y
// las órdenes que llegan de ella.

static const itype_id itype_apple( "apple" );
static const itype_id itype_backpack( "backpack" );
static const itype_id itype_test_rock( "test_rock" );
static const ter_str_id ter_t_door_c( "t_door_c" );
static const ter_str_id ter_t_door_o( "t_door_o" );

TEST_CASE( "interfaz_estado_es_json_con_lo_del_hud", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    const std::string s = interfaz::estado_json();
    CAPTURE( s );
    JsonObject o = json_loader::from_string( s ).get_object();
    o.allow_omitted_members();
    CHECK( o.has_string( "hora" ) );
    CHECK( o.has_string( "tiempo" ) );
    CHECK( o.has_string( "enMano" ) );
    JsonArray necesidades = o.get_array( "necesidades" );
    CHECK( necesidades.size() == 7 );
    for( JsonObject n : necesidades ) {
        n.allow_omitted_members();
        CHECK( n.get_float( "barra" ) >= 0.0 );
        CHECK( n.get_float( "barra" ) <= 1.0 );
    }
    JsonArray cuerpo = o.get_array( "cuerpo" );
    CHECK( cuerpo.size() >= 6 );
}

TEST_CASE( "interfaz_inventario_y_ordenes_sobre_objetos", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    avatar &u = get_avatar();
    u.i_add( item( itype_apple ) );
    const std::string s = interfaz::inventario_json();
    CAPTURE( s );
    std::string id_manzana;
    bool se_come = false;
    for( JsonObject it : json_loader::from_string( s ).get_array() ) {
        it.allow_omitted_members();
        if( it.get_string( "nombre" ).find( "apple" ) != std::string::npos ) {
            id_manzana = it.get_string( "id" );
            for( const std::string a : it.get_array( "acciones" ) ) {
                se_come = se_come || a == "comer";
            }
        }
    }
    REQUIRE_FALSE( id_manzana.empty() );
    CHECK( se_come );
    // soltarla: la orden se guarda y se hace al empezar el tic (no antes)
    interfaz::orden( "{\"a\":\"objeto\",\"id\":\"" + id_manzana + "\",\"que\":\"soltar\"}" );
    CHECK( u.has_amount( itype_apple, 1 ) );
    interfaz::turno();
    // (soltar es una actividad corta: se hace en los tics siguientes)
    for( int i = 0; i < 5 && u.has_amount( itype_apple, 1 ); i++ ) {
        u.set_moves( u.get_speed() );
        u.process_activity();
    }
    CHECK_FALSE( u.has_amount( itype_apple, 1 ) );
}

TEST_CASE( "interfaz_casilla_con_las_acciones_que_tienen_sentido", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    map &here = get_map();
    avatar &u = get_avatar();
    const tripoint_bub_ms puerta = u.pos_bub() + tripoint_rel_ms::east;
    here.ter_set( puerta, ter_t_door_c );
    here.add_item( u.pos_bub() + tripoint_rel_ms::west, item( itype_test_rock ) );
    const auto acciones = []( int dx, int dy ) {
        std::string ids;
        JsonObject o = json_loader::from_string( interfaz::casilla_json( dx, dy ) ).get_object();
        o.allow_omitted_members();
        for( JsonObject a : o.get_array( "acciones" ) ) {
            a.allow_omitted_members();
            ids += a.get_string( "id" ) + " ";
        }
        return ids;
    };
    // una puerta cerrada: abrir (y no cerrar, ni beber, ni pescar)
    const std::string en_puerta = acciones( 1, 0 );
    CAPTURE( en_puerta );
    CHECK( en_puerta.find( "abrir" ) != std::string::npos );
    CHECK( en_puerta.find( "cerrar" ) == std::string::npos );
    CHECK( en_puerta.find( "pescar" ) == std::string::npos );
    // una piedra en el suelo: coger
    const std::string en_piedra = acciones( -1, 0 );
    CAPTURE( en_piedra );
    CHECK( en_piedra.find( "coger" ) != std::string::npos );
    // abrir la puerta con una orden (es una actividad corta: se hace en los tics siguientes)
    interfaz::orden( "{\"a\":\"abrir\",\"dx\":1,\"dy\":0}" );
    interfaz::turno();
    for( int i = 0; i < 5 && here.ter( puerta ) != ter_t_door_o; i++ ) {
        u.set_moves( u.get_speed() );
        u.process_activity();
    }
    CHECK( here.ter( puerta ) == ter_t_door_o );
}

TEST_CASE( "interfaz_recetas_personaje_y_mensajes", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    JsonObject r = json_loader::from_string( interfaz::recetas_json() ).get_object();
    r.allow_omitted_members();
    CHECK( r.has_array( "conocidas" ) );
    CHECK( r.has_array( "porAprender" ) );
    JsonObject p = json_loader::from_string( interfaz::personaje_json() ).get_object();
    p.allow_omitted_members();
    CHECK( p.get_int( "fuerza" ) > 0 );
    CHECK( p.get_array( "habilidades" ).size() > 10 );
    JsonObject m = json_loader::from_string( interfaz::mensajes_json( 20 ) ).get_object();
    m.allow_omitted_members();
    CHECK( m.has_array( "mensajes" ) );
    CHECK( json_loader::from_string( interfaz::construcciones_json() ).test_array() );
}

TEST_CASE( "interfaz_menus_equipo_receta_suelo_y_mapa", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    avatar &u = get_avatar();
    map &here = get_map();
    // el equipo: objetos con su tipo (para el icono) y lo que se puede hacer
    u.i_add( item( itype_apple ) );
    JsonObject eq = json_loader::from_string( interfaz::equipo_json() ).get_object();
    eq.allow_omitted_members();
    CHECK( eq.get_array( "objetos" ).size() >= 1 );
    // una receta cualquiera: componentes, herramientas y tiempo
    const recipe &r = recipe_dict.begin()->second;
    JsonObject re = json_loader::from_string( interfaz::receta_json( r.ident().str() ) ).get_object();
    re.allow_omitted_members();
    CHECK( re.has_string( "tiempo" ) );
    CHECK( re.has_array( "componentes" ) );
    // el suelo: una piedra a los pies, y cogerla con una orden (con una mochila donde meterla)
    clear_avatar();
    u.wear_item( item( itype_backpack ), false );
    here.add_item( u.pos_bub(), item( itype_test_rock ) );
    JsonObject su = json_loader::from_string( interfaz::suelo_json( 0, 0 ) ).get_object();
    su.allow_omitted_members();
    REQUIRE( su.get_array( "objetos" ).size() == 1 );
    interfaz::orden( "{\"a\":\"coger_objetos\",\"dx\":0,\"dy\":0,\"objetos\":[{\"indice\":0,\"cantidad\":0}]}" );
    interfaz::turno();
    REQUIRE( u.activity );
    for( int i = 0; i < 10 && u.activity; i++ ) {
        u.set_moves( u.get_speed() );
        u.activity.do_turn( u );
    }
    CAPTURE( here.i_at( u.pos_bub() ).size() );
    CAPTURE( u.activity.id().str() );
    CAPTURE( u.get_wielded_item() ? u.get_wielded_item()->tname() : std::string( "nada" ) );
    CHECK( u.has_amount( itype_test_rock, 1 ) );
    // el mapa del mundo: (2r+1)^2 casillas
    JsonObject ma = json_loader::from_string( interfaz::mapa_json( 5 ) ).get_object();
    ma.allow_omitted_members();
    CHECK( ma.get_array( "casillas" ).size() == 121 );
}

TEST_CASE( "interfaz_dialogo_sin_parar_el_mundo", "[interfaz]" )
{
    clear_map();
    clear_avatar();
    avatar &u = get_avatar();
    spawn_npc( u.pos_bub().xy() + point_rel_ms::east, "test_talker" );
    CHECK( interfaz::dialogo_json() == "null" );
    interfaz::orden( "{\"a\":\"hablar\",\"dx\":1,\"dy\":0}" );
    interfaz::turno();
    const std::string s = interfaz::dialogo_json();
    CAPTURE( s );
    JsonObject d = json_loader::from_string( s ).get_object();
    d.allow_omitted_members();
    CHECK( !d.get_string( "linea" ).empty() );
    CHECK( d.get_array( "respuestas" ).size() >= 1 );
    // cerrarla
    interfaz::orden( "{\"a\":\"cerrar_dialogo\"}" );
    interfaz::turno();
    CHECK( interfaz::dialogo_json() == "null" );
}

// la traducción española trae «<punt.!>» y «<punc...!>» en vez de «<punc.!>» y «<punc…!>»: se entienden igual, sin el
// aviso «Bad tag» al hablar
TEST_CASE( "etiquetas_de_dialogo_mal_traducidas", "[interfaz]" )
{
    clear_avatar();
    avatar &u = get_avatar();
    for( std::string frase : {
             "Adios<punt.!>", "Mierda<punt...!>", "Condenado infierno<punc...!>", "Vaya<punc....>"
         } ) {
        parse_tags( frase, u, u, itype_id::NULL_ID() );
        CAPTURE( frase );
        CHECK( frase.find( '<' ) == std::string::npos );
        CHECK( frase.find( "????" ) == std::string::npos );
    }
}
