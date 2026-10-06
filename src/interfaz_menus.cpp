// La interfaz web, segunda parte (Encargo 8): lo que necesitan los menús rehechos como en un videojuego.
//  - los iconos: con qué sprite del tileset se pinta cada cosa, y las imágenes del tileset para recortarlo;
//  - el inventario con lo que se lleva puesto en cada parte del cuerpo y lo que hay dentro de cada bolsa;
//  - el detalle de una receta: componentes y herramientas con cuántos tienes y cuántos hacen falta, el tiempo, la
//    habilidad y lo que sale;
//  - lo que hay en el suelo de una casilla, para coger lo que se elija;
//  - el diálogo con un NPC sin parar el mundo (la conversación avanza con órdenes; mientras, el mundo sigue);
//  - y el mapa del mundo alrededor.
#include "interfaz.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_tiles.h"
#include "character.h"
#include "color.h"
#include "creature_tracker.h"
#include "crafting_gui_helpers.h"
#include "dialogue.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "item_category.h"
#include "item_location.h"
#include "item_pocket.h"
#include "itype.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "messages.h"
#include "mission.h"
#include "npc.h"
#include "omdata.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "proficiency.h"
#include "rng.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "skill.h"
#include "talker.h"
#include "units.h"

#if defined(TILES)
#include "sdltiles.h"
#endif

namespace interfaz
{
namespace
{
std::string color_de( const nc_color &c )
{
    return get_all_colors().get_name( c );
}

std::string id_objeto( const item &it )
{
    return std::to_string( reinterpret_cast<std::uintptr_t>( &it ) );
}

TILE_CATEGORY categoria_de( const std::string &c )
{
    if( c == "item" ) {
        return TILE_CATEGORY::ITEM;
    } else if( c == "monster" ) {
        return TILE_CATEGORY::MONSTER;
    } else if( c == "terrain" ) {
        return TILE_CATEGORY::TERRAIN;
    } else if( c == "furniture" ) {
        return TILE_CATEGORY::FURNITURE;
    } else if( c == "overmap" ) {
        return TILE_CATEGORY::OVERMAP_TERRAIN;
    } else if( c == "vpart" ) {
        return TILE_CATEGORY::VEHICLE_PART;
    }
    return TILE_CATEGORY::NONE;
}

// un objeto, lo común en todas las listas
void objeto( JsonOut &j, const item &it, bool con_id = true )
{
    if( con_id ) {
        j.member( "id", id_objeto( it ) );
    }
    j.member( "tipo", it.typeId().str() );
    j.member( "variante", it.has_itype_variant() ? it.itype_variant().id : std::string() );
    j.member( "nombre", remove_color_tags( it.tname( it.count_by_charges() ? it.charges : 1, false ) ) );
    j.member( "cantidad", it.count_by_charges() ? 1 : static_cast<int>( it.count() ) );
    j.member( "cargas", it.count_by_charges() ? it.charges : 0 );
    j.member( "categoria", it.get_category_shallow().name_header() );
    j.member( "peso", units::to_gram( it.weight() ) );
    j.member( "volumen", units::to_milliliter( it.volume() ) );
    j.member( "color", color_de( it.color_in_inventory() ) );
}

// --- el diálogo sin parar el mundo
struct dialogo_web {
    std::unique_ptr<dialogue> d;
    std::vector<std::pair<std::string, std::string>> historia;   // quién, qué
    std::string linea;
    std::vector<std::string> respuestas;
    std::vector<bool> respuestas_validas;
    std::string npc;
    bool preparado = false;
};
std::unique_ptr<dialogo_web> &dialogo()
{
    static std::unique_ptr<dialogo_web> d;
    return d;
}

// lo de opt_imgui hasta las respuestas: la frase del NPC y lo que se puede contestar
void preparar_tema( dialogo_web &dw )
{
    dialogue &d = *dw.d;
    const talk_topic topic = d.topic_stack.back();
    std::string frase = d.dynamic_line( topic );
    d.gen_responses( topic );
    if( frase.empty() ) {
        frase = "...";
    }
    if( frase[0] != '*' && frase[0] != '&' ) {
        frase = string_format( _( "\"%s\"" ), frase );
    }
    if( d.actor( true )->get_npc() ) {
        parse_tags( frase, *d.actor( false )->get_character(), *d.actor( true )->get_npc(), d, topic.item_type );
    }
    frase = uppercase_first_letter( frase );
    if( frase[0] == '&' ) {
        frase = frase.substr( 1 );
        dw.historia.emplace_back( "", frase );
    } else if( frase[0] == '*' ) {
        frase = string_format( pgettext( "npc does something", "%s %s" ), dw.npc, frase.substr( 1 ) );
        dw.historia.emplace_back( "", frase );
    } else {
        dw.historia.emplace_back( dw.npc, frase );
    }
    dw.linea = frase;
    d.apply_speaker_effects( topic );
    dw.respuestas.clear();
    dw.respuestas_validas.clear();
    const input_event sin_tecla;
    for( size_t i = 0; i < d.responses.size(); i++ ) {
        const talk_data td = d.responses[i].create_option_line( d, sin_tecla, false );
        dw.respuestas.push_back( remove_color_tags( td.text ) );
        const bool valida = i >= d.response_condition_exists.size() || !d.response_condition_exists[i] ||
                            d.response_condition_eval[i];
        dw.respuestas_validas.push_back( valida );
    }
    dw.preparado = true;
}

void cerrar_dialogo()
{
    if( dialogo() ) {
        dialogue &d = *dialogo()->d;
        if( d.actor( true ) != nullptr ) {
            d.actor( true )->say( dialog_helper::bye_message( d.actor( true )->get_npc() ) );
        }
    }
    dialogo().reset();
}

void avanzar_dialogo( size_t i )
{
    if( !dialogo() || i >= dialogo()->d->responses.size() || !dialogo()->respuestas_validas[i] ) {
        return;
    }
    dialogo_web &dw = *dialogo();
    dialogue &d = *dw.d;
    dw.historia.emplace_back( "Tú", dw.respuestas[i] );
    talk_response elegida = d.responses[i];
    if( elegida.mission_selected != nullptr ) {
        d.actor( true )->select_mission( elegida.mission_selected );
    }
    d.actor( true )->store_chosen_training( elegida.skill, elegida.style, elegida.dialogue_spell,
                                            elegida.proficiency );
    const bool exito = elegida.trial.roll( d );
    talk_effect_t const &efectos = exito ? elegida.success : elegida.failure;
    const talk_topic siguiente = efectos.apply( d );
    talk_effect_t::update_missions( d );
    // (como draw_dialogue_imgui: TALK_NONE vuelve al tema de antes; TALK_DONE acaba)
    if( siguiente.id == "TALK_NONE" ) {
        d.topic_stack.pop_back();
    }
    if( siguiente.id == "TALK_DONE" || d.topic_stack.empty() || d.done ) {
        cerrar_dialogo();
        return;
    }
    if( siguiente.id != "TALK_NONE" ) {
        d.add_topic( siguiente );
    }
    preparar_tema( dw );
}
} // namespace

// --- iconos
std::string atlas_json()
{
    std::ostringstream s;
    JsonOut j( s );
    j.start_object();
#if defined(TILES)
    if( tilecontext ) {
        j.member( "ancho", tilecontext->ancho_sprite_interfaz() );
        j.member( "alto", tilecontext->alto_sprite_interfaz() );
        j.member( "imagenes" );
        j.start_array();
        for( const atlas_replay_descriptor &a : tilecontext->atlas_interfaz() ) {
            j.start_object();
            j.member( "ruta", a.image_path_u8 );
            j.member( "ancho", a.sprite_width );
            j.member( "alto", a.sprite_height );
            j.member( "desde", a.atlas_offset );
            j.member( "cuantos", a.expected_tilecount );
            j.end_object();
        }
        j.end_array();
    }
#endif
    j.end_object();
    return s.str();
}

std::string iconos_json( const std::string &peticion )
{
    std::ostringstream s;
    JsonOut j( s );
    j.start_object();
#if defined(TILES)
    if( tilecontext ) {
        try {
            for( JsonArray par : json_loader::from_string( peticion ).get_array() ) {
                const std::string id = par.get_string( 0 );
                const std::string cat = par.get_string( 1 );
                const std::string variante = par.size() > 2 ? par.get_string( 2 ) : std::string();
                const std::pair<int, int> sp = tilecontext->sprite_interfaz( id, categoria_de( cat ), variante );
                j.member( id + "|" + cat + "|" + variante );
                j.start_array();
                j.write( sp.first );
                j.write( sp.second );
                j.end_array();
            }
        } catch( const std::exception & ) {
        }
    }
#else
    static_cast<void>( peticion );
#endif
    j.end_object();
    return s.str();
}

// --- inventario con el muñequito y las bolsas
std::string equipo_json()
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    const item_location en_mano = u.get_wielded_item();
    j.start_object();
    j.member( "objetos" );
    j.start_array();
    for( item_location &loc : u.all_items_loc() ) {
        if( !loc || loc->is_null() ) {
            continue;
        }
        const item &it = *loc;
        const bool puesto = u.is_worn( it );
        const bool empunado = en_mano && &*en_mano == &it;
        j.start_object();
        objeto( j, it );
        j.member( "puesto", puesto );
        j.member( "enMano", empunado );
        j.member( "dentroDe", loc.has_parent() ? id_objeto( *loc.parent_item() ) : std::string() );
        if( puesto ) {
            j.member( "cubre" );
            j.start_array();
            for( const bodypart_str_id &bp : it.get_covered_body_parts() ) {
                j.write( bp.str() );
            }
            j.end_array();
        }
        const bool contenedor = it.is_container() && !it.is_comestible();
        j.member( "contenedor", contenedor );
        if( contenedor ) {
            j.member( "lleno", units::to_milliliter( it.get_contents_volume() ) );
            j.member( "capacidad", units::to_milliliter( it.get_volume_capacity() ) );
        }
        j.member( "comestible", it.is_comestible() );
        j.member( "bebida", it.is_comestible() && it.get_comestible()->comesttype == "DRINK" );
        j.member( "acciones" );
        j.start_array();
        if( it.is_comestible() ) {
            j.write( it.get_comestible()->comesttype == "DRINK" ? "beber" : "comer" );
        }
        if( it.is_armor() && !puesto ) {
            j.write( "ponerse" );
        }
        if( puesto ) {
            j.write( "quitarse" );
        }
        if( !empunado && !puesto ) {
            j.write( "empunar" );
        }
        if( it.is_book() ) {
            j.write( "leer" );
        }
        if( it.type->has_use() ) {
            j.write( "usar" );
        }
        j.write( "soltar" );
        j.end_array();
        j.end_object();
    }
    j.end_array();
    j.member( "peso", units::to_gram( u.weight_carried() ) );
    j.member( "pesoMax", units::to_gram( u.weight_capacity() ) );
    j.member( "volumen", units::to_milliliter( u.volume_carried() ) );
    j.member( "volumenMax", units::to_milliliter( u.volume_capacity() ) );
    j.end_object();
    return s.str();
}

// --- el detalle de una receta
std::string receta_json( const std::string &id )
{
    std::ostringstream s;
    JsonOut j( s );
    const recipe_id rid( id );
    if( !rid.is_valid() ) {
        j.write_null();
        return s.str();
    }
    avatar &u = get_avatar();
    const recipe &r = rid.obj();
    const temp_crafting_inventory &inv = u.crafting_inventory();
    const availability av( u, &r );
    j.start_object();
    j.member( "id", id );
    j.member( "nombre", r.result_name() );
    j.member( "resultado", r.result().str() );
    j.member( "cantidad", r.makes_amount() );
    j.member( "tiempo", to_string( r.time_to_craft( u, crafting_cost_context::for_recipe( u, r ) ) ) );
    if( r.skill_used ) {
        j.member( "habilidad", r.skill_used->name() );
        j.member( "dificultad", r.get_difficulty( u ) );
        j.member( "tuNivel", static_cast<int>( u.get_skill_level( r.skill_used ) ) );
    }
    j.member( "competencias", remove_color_tags( r.required_proficiencies_string( &u ) ) );
    j.member( "puede", av.can_craft_recipe );
    j.member( "tieneHabilidad", av.has_all_skills );
    // (las prácticas no hacen ningún objeto: su descripción es la de la receta)
    j.member( "descripcion", remove_color_tags( r.is_practice() || !r.description.empty() ?
                                                r.description.translated() : r.result()->description.translated() ) );
    j.member( "practica", r.is_practice() );
    const requirement_data &req = r.simple_requirements();
    // herramientas: cada grupo, sus alternativas (vale una)
    j.member( "herramientas" );
    j.start_array();
    for( const std::vector<tool_comp> &grupo : req.get_tools() ) {
        j.start_array();
        for( const tool_comp &t : grupo ) {
            j.start_object();
            j.member( "tipo", t.type.str() );
            j.member( "nombre", item::nname( t.type ) );
            j.member( "tiene", inv.has_tools( t.type, 1 ) );
            j.end_object();
        }
        j.end_array();
    }
    j.end_array();
    j.member( "cualidades" );
    j.start_array();
    for( const std::vector<quality_requirement> &grupo : req.get_qualities() ) {
        j.start_array();
        for( const quality_requirement &q : grupo ) {
            j.start_object();
            j.member( "nombre", q.type->name.translated() );
            j.member( "nivel", q.level );
            j.member( "tiene", inv.has_quality( q.type, q.level, q.count ) );
            j.end_object();
        }
        j.end_array();
    }
    j.end_array();
    j.member( "componentes" );
    j.start_array();
    for( const std::vector<item_comp> &grupo : req.get_components() ) {
        j.start_array();
        for( const item_comp &c : grupo ) {
            const bool por_cargas = item::count_by_charges( c.type );
            const int tiene = por_cargas ? inv.charges_of( c.type, INT_MAX ) : inv.amount_of( c.type, false,
                              INT_MAX );
            j.start_object();
            j.member( "tipo", c.type.str() );
            j.member( "nombre", item::nname( c.type, c.count ) );
            j.member( "necesita", c.count );
            j.member( "tiene", tiene );
            j.end_object();
        }
        j.end_array();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

// --- lo que hay en el suelo de una casilla
std::string suelo_json( int dx, int dy )
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms p = u.pos_bub() + tripoint_rel_ms( dx, dy, 0 );
    j.start_object();
    j.member( "dx", dx );
    j.member( "dy", dy );
    j.member( "lugar", here.has_furn( p ) ? here.furn( p ).obj().name() : here.ter( p ).obj().name() );
    j.member( "objetos" );
    j.start_array();
    int i = 0;
    for( const item &it : here.i_at( p ) ) {
        j.start_object();
        j.member( "indice", i++ );
        objeto( j, it, false );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

// --- las listas del juego
namespace
{
// (una lista puede abrir otra: se apilan)
std::vector<std::string> &listas()
{
    static std::vector<std::string> l;
    return l;
}
std::optional<int> &eleccion()
{
    static std::optional<int> e;
    return e;
}
} // namespace

void abrir_lista( const std::string &json )
{
    listas().push_back( json );
    eleccion().reset();
}

void cerrar_lista()
{
    if( !listas().empty() ) {
        listas().pop_back();
    }
    eleccion().reset();
}

std::string lista_json()
{
    return listas().empty() ? std::string( "null" ) : listas().back();
}

void elegir( int i )
{
    if( !listas().empty() ) {
        eleccion() = i;
    }
}

bool tomar_eleccion( int &i )
{
    if( !eleccion() ) {
        return false;
    }
    i = *eleccion();
    eleccion().reset();
    return true;
}

// --- robar
std::string robo_json( int dx, int dy )
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    npc *guy = get_creature_tracker().creature_at<npc>( u.pos_bub() + tripoint_rel_ms( dx, dy, 0 ) );
    if( guy == nullptr ) {
        j.write_null();
        return s.str();
    }
    j.start_object();
    j.member( "npc", guy->disp_name() );
    j.member( "hostil", guy->is_enemy() );
    j.member( "objetos" );
    j.start_array();
    const item_location en_mano = guy->get_wielded_item();
    for( item_location &loc : guy->all_items_loc() ) {
        if( !loc || guy->is_worn( *loc ) || ( en_mano && &*en_mano == &*loc ) ) {
            continue;
        }
        j.start_object();
        objeto( j, *loc );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

// --- el diálogo
std::string dialogo_json()
{
    std::ostringstream s;
    JsonOut j( s );
    if( !dialogo() ) {
        j.write_null();
        return s.str();
    }
    dialogo_web &dw = *dialogo();
    j.start_object();
    j.member( "npc", dw.npc );
    j.member( "linea", dw.linea );
    j.member( "historia" );
    j.start_array();
    const size_t desde = dw.historia.size() > 30 ? dw.historia.size() - 30 : 0;
    for( size_t i = desde; i < dw.historia.size(); i++ ) {
        j.start_array();
        j.write( dw.historia[i].first );
        j.write( dw.historia[i].second );
        j.end_array();
    }
    j.end_array();
    j.member( "respuestas" );
    j.start_array();
    for( size_t i = 0; i < dw.respuestas.size(); i++ ) {
        j.start_object();
        j.member( "texto", dw.respuestas[i] );
        j.member( "vale", static_cast<bool>( dw.respuestas_validas[i] ) );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

// --- el mapa del mundo alrededor (casillas del mapa grande)
std::string mapa_json( int radio )
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    radio = std::clamp( radio, 5, 60 );
    const tripoint_abs_omt centro = u.pos_abs_omt();
    j.start_object();
    j.member( "radio", radio );
    j.member( "x", centro.x() );
    j.member( "y", centro.y() );
    j.member( "z", centro.z() );
    j.member( "casillas" );
    j.start_array();
    for( int dy = -radio; dy <= radio; dy++ ) {
        for( int dx = -radio; dx <= radio; dx++ ) {
            const tripoint_abs_omt p = centro + tripoint_rel_omt( dx, dy, 0 );
            const om_vision_level visto = overmap_buffer.seen( p );
            if( visto == om_vision_level::unseen ) {
                j.write_null();
                continue;
            }
            const oter_id &t = overmap_buffer.ter( p );
            j.start_array();
            j.write( t->get_type_id().str() );
            j.write( t->get_name( visto ) );
            j.write( t->get_symbol( visto ) );
            j.write( color_de( t->get_color( visto ) ) );
            j.end_array();
        }
    }
    j.end_array();
    j.end_object();
    return s.str();
}

// las órdenes de esta parte (devuelve true si era suya)
bool hacer_menus( const std::string &a, JsonObject &o )
{
    avatar &u = get_avatar();
    map &here = get_map();
    if( a == "coger_objetos" ) {
        const tripoint_bub_ms p = u.pos_bub() + tripoint_rel_ms( o.get_int( "dx", 0 ), o.get_int( "dy", 0 ), 0 );
        std::vector<item_location> objetos;
        std::vector<int> cuantos;
        std::vector<item *> en_suelo;
        for( item &it : here.i_at( p ) ) {
            en_suelo.push_back( &it );
        }
        for( JsonObject e : o.get_array( "objetos" ) ) {
            e.allow_omitted_members();
            const int i = e.get_int( "indice", -1 );
            if( i >= 0 && i < static_cast<int>( en_suelo.size() ) ) {
                objetos.emplace_back( map_cursor( p ), en_suelo[i] );
                cuantos.push_back( e.get_int( "cantidad", 0 ) );
            }
        }
        if( !objetos.empty() ) {
            u.assign_activity( pickup_activity_actor( objetos, cuantos, u.pos_bub(), false ) );
        }
        return true;
    }
    if( a == "hablar" ) {
        const tripoint_bub_ms p = u.pos_bub() + tripoint_rel_ms( o.get_int( "dx", 0 ), o.get_int( "dy", 0 ), 0 );
        npc *guy = get_creature_tracker().creature_at<npc>( p );
        if( guy == nullptr ) {
            return true;
        }
        std::unique_ptr<talker> con = get_talker_for( *guy );
        if( !con->will_talk_to_u( u, false ) ) {
            return true;
        }
        auto dw = std::make_unique<dialogo_web>();
        dw->npc = guy->disp_name();
        dw->d = std::make_unique<dialogue>( get_talker_for( u ), std::move( con ),
                std::unordered_map<std::string, std::function<bool( const_dialogue const & )>> {} );
        dw->d->actor( true )->check_missions();
        for( mission *&m : dw->d->actor( true )->assigned_missions() ) {
            if( m->get_assigned_player_id() == u.getID() ) {
                dw->d->missions_assigned.push_back( m );
            }
        }
        for( const std::string &t : dw->d->actor( true )->get_topics( false ) ) {
            dw->d->add_topic( t );
        }
        dialogo() = std::move( dw );
        preparar_tema( *dialogo() );
        return true;
    }
    if( a == "robar" ) {
        // (lo de avatar::steal después de elegir qué: las mismas tiradas)
        npc *guy = get_creature_tracker().creature_at<npc>( u.pos_bub() + tripoint_rel_ms( o.get_int( "dx", 0 ),
                   o.get_int( "dy", 0 ), 0 ) );
        if( guy == nullptr ) {
            return true;
        }
        if( guy->is_enemy() ) {
            add_msg( _( "%s is hostile!" ), guy->get_name() );
            return true;
        }
        item *it = nullptr;
        for( item_location &loc : guy->all_items_loc() ) {
            if( loc && id_objeto( *loc ) == o.get_string( "id", "" ) ) {
                it = loc.get_item();
                break;
            }
        }
        if( it == nullptr ) {
            return true;
        }
        int mi_tirada = dice( 3, u.get_dex() );
        if( !u.is_armed() ) {
            mi_tirada += dice( 4, 3 );
        }
        if( u.has_trait( trait_id( "DEFT" ) ) ) {
            mi_tirada += dice( 2, 6 );
        }
        if( u.has_trait( trait_id( "CLUMSY" ) ) ) {
            mi_tirada -= dice( 4, 6 );
        }
        const int su_tirada = dice( 5, guy->get_per() );
        const std::string nombre = it->tname();
        if( mi_tirada >= su_tirada && !guy->is_hallucination() ) {
            add_msg( _( "You sneakily steal %1$s from %2$s!" ), nombre, guy->get_name() );
            u.i_add( guy->i_rem( it ) );
        } else if( mi_tirada >= su_tirada / 2 ) {
            add_msg( _( "You failed to steal %1$s from %2$s, but did not attract attention." ), nombre,
                     guy->get_name() );
        } else {
            add_msg( _( "You failed to steal %1$s from %2$s." ), nombre, guy->get_name() );
            guy->on_attacked( u );
        }
        u.mod_moves( -200 );
        return true;
    }
    if( a == "responder" ) {
        avanzar_dialogo( static_cast<size_t>( std::max( 0, o.get_int( "i", -1 ) ) ) );
        return true;
    }
    if( a == "cerrar_dialogo" ) {
        cerrar_dialogo();
        return true;
    }
    return false;
}

// cada tic: si el NPC del diálogo ya no está (muerto, lejos), se acaba
void turno_menus()
{
    if( dialogo() ) {
        dialogue &d = *dialogo()->d;
        const npc *guy = d.actor( true ) != nullptr ? d.actor( true )->get_npc() : nullptr;
        if( guy == nullptr || guy->is_dead() || rl_dist( guy->pos_bub(), get_avatar().pos_bub() ) > 12 ) {
            dialogo().reset();
        }
    }
}

} // namespace interfaz
