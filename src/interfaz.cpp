#include "interfaz.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "activity_actor_definitions.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bodypart.h"
#include "calendar.h"
#include "cached_options.h"
#include "character.h"
#include "color.h"
#include "construction.h"
#include "construction_group.h"
#include "creature.h"
#include "creature_tracker.h"
#include "crafting_gui_helpers.h"
#include "display.h"
#include "game.h"
#include "iexamine.h"
#include "item.h"
#include "item_category.h"
#include "item_location.h"
#include "itype.h"
#include "json.h"
#include "json_loader.h"
#include "map.h"
#include "mapdata.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "worldfactory.h"
#include "output.h"
#include "system_locale.h"
#include "translations.h"
#include "overmap_ui.h"
#include "panels.h"
#include "pathfinding.h"
#include "proficiency.h"
#include "realtime.h"
#include "suave.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "skill.h"
#include "units.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "weather.h"
#include "weather_type.h"

#if defined(TILES)
#include "cata_tiles.h"
#include "cursesport.h"
#include "sdltiles.h"
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_sleep( "sleep" );

namespace interfaz
{
namespace
{
std::deque<std::string> &ordenes()
{
    static std::deque<std::string> o;
    return o;
}

// el nombre de un color del juego ("c_light_green"), que la página convierte en un color de verdad
std::string color( const nc_color &c )
{
    return get_all_colors().get_name( c );
}

// de 0 a 1
double entre( double v, double desde, double hasta )
{
    if( hasta == desde ) {
        return 0.0;
    }
    return std::clamp( ( v - desde ) / ( hasta - desde ), 0.0, 1.0 );
}

void necesidad( JsonOut &j, const std::string &id, const std::string &nombre,
                const std::pair<std::string, nc_color> &texto, double barra )
{
    j.start_object();
    j.member( "id", id );
    j.member( "nombre", nombre );
    j.member( "texto", remove_color_tags( texto.first ) );
    j.member( "color", color( texto.second ) );
    j.member( "barra", barra );
    j.end_object();
}

std::string id_de( const item &it )
{
    return std::to_string( reinterpret_cast<std::uintptr_t>( &it ) );
}

std::optional<item_location> objeto_por_id( avatar &u, const std::string &id )
{
    for( item_location &loc : u.all_items_loc() ) {
        if( loc && id_de( *loc ) == id ) {
            return loc;
        }
    }
    return std::nullopt;
}

tripoint_bub_ms casilla( const avatar &u, int dx, int dy )
{
    return u.pos_bub() + tripoint_rel_ms( dx, dy, 0 );
}

// lo que tiene sentido hacer en una casilla: id de la acción y lo que se pinta
std::vector<std::pair<std::string, std::string>> acciones_en( avatar &u, const tripoint_bub_ms &p )
{
    map &here = get_map();
    std::vector<std::pair<std::string, std::string>> a;
    const bool es_el_jugador = p == u.pos_bub();
    if( Creature *c = get_creature_tracker().creature_at( p ) ) {
        if( c != &u ) {
            if( c->is_npc() ) {
                a.emplace_back( "hablar", "Hablar" );
                if( c->attitude_to( u ) != Creature::Attitude::HOSTILE ) {
                    a.emplace_back( "robar", "Robar" );
                }
            }
            a.emplace_back( "atacar", "Atacar" );
            a.emplace_back( "mirar", "Mirar" );
            return a;
        }
    }
    const ter_t &ter = here.ter( p ).obj();
    const furn_t &furn = here.furn( p ).obj();
    const bool hay_mueble = here.has_furn( p );
    if( ( hay_mueble && furn.open ) || ( !hay_mueble && ter.open ) ) {
        a.emplace_back( "abrir", "Abrir" );
    }
    if( ( hay_mueble && furn.close ) || ( !hay_mueble && ter.close ) ) {
        a.emplace_back( "cerrar", "Cerrar" );
    }
    if( !here.i_at( p ).empty() ) {
        a.emplace_back( "coger", "Coger" );
    }
    if( here.has_flag( ter_furn_flag::TFLAG_LIQUID, p ) ) {
        a.emplace_back( "beber", "Beber o llenar" );
    }
    if( here.has_flag( ter_furn_flag::TFLAG_FISHABLE, p ) ) {
        a.emplace_back( "pescar", "Pescar" );
    }
    const bool examinable = ( hay_mueble && !furn.has_examine( iexamine::none ) ) ||
                            !ter.has_examine( iexamine::none );
    if( examinable && !here.has_flag( ter_furn_flag::TFLAG_LIQUID, p ) ) {
        a.emplace_back( "examinar", "Examinar" );
    }
    if( here.veh_at( p ) ) {
        a.emplace_back( "vehiculo", "Vehículo" );
    }
    if( !es_el_jugador && here.passable( p ) ) {
        a.emplace_back( "ir", "Ir aquí" );
    }
    a.emplace_back( "mirar", "Mirar" );
    return a;
}
} // namespace

void poner_idioma( const std::string &idioma )
{
    // (antes de que el juego cargue sus opciones, la página ya pregunta: entonces no hay nada que tocar)
    if( !get_options().has_option( "USE_LANG" ) ) {
        return;
    }
    get_options().get_option( "USE_LANG" ).setValue( idioma == "es" ? "es_ES" : "en" );
    get_options().save();
    set_language_from_options();
}

std::string idioma()
{
    if( !get_options().has_option( "USE_LANG" ) ) {
        return "es";
    }
    const std::string l = get_option<std::string>( "USE_LANG" );
    if( l != "es_ES" && l != "en" ) {
        // (sin elegir, el juego va en el idioma del sistema, que puede ser cualquiera: aquí solo español o inglés,
        // y el mismo para el juego y la página. Español si el sistema lo está, si no inglés; y se guarda)
        const std::string sistema = l.empty() ? SystemLocale::Language().value_or( "en" ) : l;
        const std::string elegido = sistema.rfind( "es", 0 ) == 0 ? "es" : "en";
        poner_idioma( elegido );
        return elegido;
    }
    return l == "es_ES" ? "es" : "en";
}

namespace
{
std::string &pedido_menu()
{
    static std::string p;
    return p;
}
std::string &resumen_muerte()
{
    static std::string r;
    return r;
}
} // namespace

void pedir_menu_principal( const std::string &json )
{
    pedido_menu() = json;
}

bool tomar_pedido_menu_principal( std::string &json )
{
    if( pedido_menu().empty() ) {
        return false;
    }
    json = pedido_menu();
    pedido_menu().clear();
    return true;
}

std::string partidas_json()
{
    std::ostringstream s;
    JsonOut j( s );
    j.start_array();
    if( world_generator ) {
        for( const std::string &nombre : world_generator->all_worldnames() ) {
            WORLD *w = world_generator->get_world( nombre );
            if( w == nullptr ) {
                continue;
            }
            j.start_object();
            j.member( "mundo", nombre );
            j.member( "partidas" );
            j.start_array();
            for( const save_t &p : w->world_saves ) {
                j.write( p.decoded_name() );
            }
            j.end_array();
            j.end_object();
        }
    }
    j.end_array();
    return s.str();
}

void al_morir()
{
    avatar &u = get_avatar();
    std::ostringstream s;
    JsonOut j( s );
    j.start_object();
    j.member( "nombre", u.get_name() );
    j.member( "dias", to_days<int>( calendar::turn - calendar::start_of_game ) );
    j.member( "horas", to_hours<int>( calendar::turn - calendar::start_of_game ) % 24 );
    j.member( "hora", to_string_time_of_day( calendar::turn ) );
    j.member( "mensajes" );
    j.start_array();
    for( const Messages::mensaje_tipado &m : Messages::recent_messages_typed( 8 ) ) {
        j.write( remove_color_tags( m.texto ) );
    }
    j.end_array();
    j.end_object();
    resumen_muerte() = s.str();
}

std::string muerte_json()
{
    return resumen_muerte().empty() ? std::string( "null" ) : resumen_muerte();
}

void olvidar_muerte()
{
    resumen_muerte().clear();
}

bool activa()
{
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}

std::string estado_json()
{
    std::ostringstream s;
    JsonOut j( s );
    if( g == nullptr || !get_avatar().getID().is_valid() ) {
        j.write_null();
        return s.str();
    }
    avatar &u = get_avatar();
    j.start_object();
    j.member( "nombre", u.get_name() );
    j.member( "idioma", idioma() );
    j.member( "hora", to_string_time_of_day( calendar::turn ) );
    j.member( "dia", day_of_season<int>( calendar::turn ) + 1 );
    j.member( "estacion", calendar::name_season( season_of_year( calendar::turn ) ) );
    j.member( "año", calendar::years_since_cataclysm( calendar::turn ) + 1 );
    j.member( "velocidad", realtime::nombre( realtime::el_reloj().vel() ) );
    j.member( "vel", static_cast<int>( realtime::el_reloj().vel() ) );
    j.member( "tiempo", remove_color_tags( display::weather_text_color( u ).first ) );
    j.member( "tiempoColor", color( display::weather_text_color( u ).second ) );
    j.member( "temperatura", print_temperature( get_weather().temperature ) );
    j.member( "temperaturaC", units::to_celsius( get_weather().temperature ) );
    j.member( "exterior", get_map().is_outside( u.pos_bub() ) );
    // (dónde está, en casillas absolutas, y si va camino de algún sitio: para las pruebas de andar)
    j.member( "pos", std::vector<int> { u.pos_abs().x(), u.pos_abs().y(), u.pos_abs().z() } );
    j.member( "enCamino", u.has_destination() );
    const item_location en_mano = u.get_wielded_item();
    j.member( "enMano", en_mano ? remove_color_tags( en_mano->tname( 1, false ) ) : std::string() );
    j.member( "enManoTipo", en_mano ? en_mano->typeId().str() : std::string() );
    // (la actividad en curso: fabricar, leer, construir...; con su progreso, para la barra, y si se puede cancelar)
    if( u.activity && !u.activity.is_null() ) {
        j.member( "actividad" );
        j.start_object();
        j.member( "id", u.activity.id().str() );
        j.member( "nombre", remove_color_tags( u.activity.get_verb().translated() ) );
        const int total = u.activity.moves_total;
        double progreso = total > 0 ? std::clamp( 1.0 - static_cast<double>( u.activity.moves_left ) / total, 0.0,
                          1.0 ) : -1.0;
        // (lo que el juego enseñaba en su ventanita: «Fabricando: 23 %»...; de ahí el progreso si no hay otro)
        const std::optional<std::string> texto = u.activity.get_progress_message( u );
        if( texto ) {
            j.member( "texto", remove_color_tags( *texto ) );
            const size_t pc = texto->find( '%' );
            if( progreso < 0 && pc != std::string::npos ) {
                size_t i = pc;
                while( i > 0 && ( std::isdigit( static_cast<unsigned char>( ( *texto )[i - 1] ) ) ||
                                  ( *texto )[i - 1] == '.' || ( *texto )[i - 1] == ' ' ) ) {
                    i--;
                }
                try {
                    progreso = std::clamp( std::stod( texto->substr( i, pc - i ) ) / 100.0, 0.0, 1.0 );
                } catch( const std::exception & ) {
                    // (sin número)
                }
            }
        }
        j.member( "progreso", progreso );
        j.member( "cancelable", u.activity.is_interruptible() );
        j.end_object();
    } else if( u.has_effect( effect_sleep ) ) {
        j.member( "actividad" );
        j.start_object();
        j.member( "id", "dormir" );
        j.member( "nombre", _( "Sleeping" ) );
        j.member( "progreso", -1.0 );
        j.member( "cancelable", false );
        j.end_object();
    }
    j.member( "necesidades" );
    j.start_array();
    necesidad( j, "hambre", "Hambre", display::hunger_text_color( u ),
               entre( u.get_stored_kcal(), 0, u.get_healthy_kcal() ) * ( 1.0 - entre( u.get_hunger(), 0, 300 ) * 0.5 ) );
    necesidad( j, "sed", "Sed", display::thirst_text_color( u ), 1.0 - entre( u.get_thirst(), 0, 600 ) );
    necesidad( j, "sueno", "Sueño", display::sleepiness_text_color( u ), 1.0 - entre( u.get_sleepiness(), 0, 575 ) );
    necesidad( j, "temperatura", "Temperatura", display::temp_text_color( u, bodypart_str_id( "torso" ) ),
               1.0 - entre( std::abs( units::to_legacy_bodypart_temp( u.get_part_temp_cur( bodypart_id( "torso" ) ) ) - 5000 ),
                            0, 4000 ) );
    necesidad( j, "animo", "Ánimo", display::morale_face_color( u ), entre( u.get_morale_level(), -100, 100 ) );
    necesidad( j, "aguante", "Aguante", std::make_pair( std::to_string( u.get_stamina() * 100 /
               std::max( 1, u.get_stamina_max() ) ) + " %", c_light_blue ), entre( u.get_stamina(), 0, u.get_stamina_max() ) );
    necesidad( j, "dolor", "Dolor", display::pain_text_color( u ), 1.0 - entre( u.get_perceived_pain(), 0, 80 ) );
    j.end_array();
    j.member( "cuerpo" );
    j.start_array();
    for( const bodypart_id &bp : u.get_all_body_parts( get_body_part_flags::only_main ) ) {
        j.start_object();
        j.member( "id", bp.id().str() );
        j.member( "nombre", body_part_name_as_heading( bp, 1 ) );
        j.member( "vida", u.get_part_hp_cur( bp ) );
        j.member( "max", u.get_part_hp_max( bp ) );
        j.member( "sangra", u.get_effect_int( effect_bleed, bp ) );
        j.member( "rota", u.is_limb_broken( bp ) );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

std::string mensajes_json( int n )
{
    std::ostringstream s;
    JsonOut j( s );
    j.start_object();
    j.member( "total", static_cast<int>( Messages::size() ) );
    j.member( "mensajes" );
    j.start_array();
    for( const Messages::mensaje_tipado &m : Messages::recent_messages_typed( std::max( 0, n ) ) ) {
        // (sin los de depuración)
        if( m.tipo == m_debug ) {
            continue;
        }
        j.start_object();
        j.member( "hora", m.hora );
        j.member( "texto", remove_color_tags( m.texto ) );
        j.member( "tipo", m.tipo );
        j.member( "turno", m.turno );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

std::string inventario_json()
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    const item_location en_mano = u.get_wielded_item();
    j.start_array();
    for( item_location &loc : u.all_items_loc() ) {
        if( !loc ) {
            continue;
        }
        const item &it = *loc;
        const bool puesto = u.is_worn( it );
        const bool empunado = en_mano && &*en_mano == &it;
        j.start_object();
        j.member( "id", id_de( it ) );
        j.member( "nombre", remove_color_tags( it.tname( it.count_by_charges() ? it.charges : 1 ) ) );
        j.member( "cantidad", it.count_by_charges() ? 1 : static_cast<int>( it.count() ) );
        j.member( "categoria", it.get_category_shallow().name_header() );
        j.member( "peso", units::to_gram( it.weight() ) );
        j.member( "volumen", units::to_milliliter( it.volume() ) );
        j.member( "puesto", puesto );
        j.member( "enMano", empunado );
        j.member( "dentro", loc.where() == item_location::type::container );
        j.member( "acciones" );
        j.start_array();
        if( it.is_comestible() ) {
            j.write( "comer" );
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
    return s.str();
}

std::string recetas_json()
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    const temp_crafting_inventory &inv = u.crafting_inventory();
    j.start_object();
    j.member( "conocidas" );
    j.start_array();
    for( const recipe *r : u.get_learned_recipes() ) {
        if( r == nullptr || r->is_blacklisted() || r->obsolete ) {
            continue;
        }
        const availability av( u, r );
        std::string motivo;
        if( !av.has_all_skills ) {
            motivo = string_format( "Necesitas %s %d", r->skill_used ? r->skill_used->name() : std::string( "?" ),
                                    r->get_difficulty( u ) );
        } else if( !av.has_proficiencies ) {
            motivo = "Te falta una competencia";
        } else if( !av.can_craft_recipe ) {
            r->simple_requirements().can_make_with_inventory( &u, inv, r->get_component_filter() );
            motivo = remove_color_tags( r->simple_requirements().list_missing() );
        }
        j.start_object();
        j.member( "id", r->ident().str() );
        j.member( "resultado", r->result().str() );
        j.member( "nombre", r->result_name() );
        j.member( "categoria", r->category.str() );
        j.member( "subcategoria", r->subcategory );
        j.member( "puede", av.can_craft_recipe );
        j.member( "motivo", motivo );
        j.end_object();
    }
    j.end_array();
    // las que se aprenden solas con más habilidad: las más cercanas
    std::vector<std::pair<int, const recipe *>> por_aprender;
    for( const auto &e : recipe_dict ) {
        const recipe &r = e.second;
        if( r.autolearn_requirements.empty() || r.is_blacklisted() || r.obsolete || u.knows_recipe( &r ) ||
            r.is_practice() || r.is_nested() ) {
            continue;
        }
        int falta = 0;
        for( const auto &req : r.autolearn_requirements ) {
            falta += std::max( 0, req.second - static_cast<int>( u.get_skill_level( req.first ) ) );
        }
        por_aprender.emplace_back( falta, &r );
    }
    std::sort( por_aprender.begin(), por_aprender.end(), []( const auto & a, const auto & b ) {
        return a.first < b.first;
    } );
    if( por_aprender.size() > 80 ) {
        por_aprender.resize( 80 );
    }
    j.member( "porAprender" );
    j.start_array();
    for( const auto &e : por_aprender ) {
        const recipe &r = *e.second;
        std::string requisito;
        for( const auto &req : r.autolearn_requirements ) {
            requisito += ( requisito.empty() ? "" : ", " ) + req.first->name() + " " + std::to_string( req.second );
        }
        j.start_object();
        j.member( "id", r.ident().str() );
        j.member( "resultado", r.result().str() );
        j.member( "nombre", r.result_name() );
        j.member( "categoria", r.category.str() );
        j.member( "requisito", requisito );
        j.end_object();
    }
    j.end_array();
    j.end_object();
    return s.str();
}

std::string construcciones_json()
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    const temp_crafting_inventory &inv = u.crafting_inventory();
    std::map<std::string, std::pair<bool, std::string>> grupos;
    std::map<std::string, std::string> categoria;
    std::map<std::string, std::string> grupos_id;
    for( const construction &c : get_constructions() ) {
        if( !c.on_display ) {
            continue;
        }
        const std::string nombre = c.group->name();
        grupos_id[nombre] = c.group.str();
        bool habilidad = true;
        std::string falta;
        for( const auto &sk : c.required_skills ) {
            if( u.get_skill_level( sk.first ) < sk.second ) {
                habilidad = false;
                falta = string_format( "Necesitas %s %d", sk.first->name(), sk.second );
            }
        }
        bool materiales = false;
        if( habilidad ) {
            materiales = c.requirements->can_make_with_inventory( &u, inv, is_crafting_component );
            if( !materiales ) {
                falta = remove_color_tags( c.requirements->list_missing() );
            }
        }
        const bool puede = habilidad && materiales;
        auto it = grupos.find( nombre );
        if( it == grupos.end() || ( puede && !it->second.first ) ) {
            grupos[nombre] = { puede, puede ? std::string() : falta };
            categoria[nombre] = c.category.str();
        }
    }
    j.start_array();
    for( const auto &g : grupos ) {
        j.start_object();
        j.member( "nombre", g.first );
        j.member( "grupo", grupos_id[g.first] );
        j.member( "categoria", categoria[g.first] );
        // (dónde se puede, de las casillas de al lado: dx, dy)
        if( g.second.first ) {
            j.member( "donde" );
            j.start_array();
            for( const tripoint_bub_ms &p : casillas_para_construir( construction_group_str_id( grupos_id[g.first] ) ) ) {
                j.start_array();
                j.write( p.x() - u.pos_bub().x() );
                j.write( p.y() - u.pos_bub().y() );
                j.end_array();
            }
            j.end_array();
        }
        j.member( "puede", g.second.first );
        j.member( "motivo", g.second.second );
        j.end_object();
    }
    j.end_array();
    return s.str();
}

std::string personaje_json()
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    j.start_object();
    j.member( "nombre", u.get_name() );
    j.member( "fuerza", u.get_str() );
    j.member( "destreza", u.get_dex() );
    j.member( "inteligencia", u.get_int() );
    j.member( "percepcion", u.get_per() );
    j.member( "habilidades" );
    j.start_array();
    for( const Skill &sk : Skill::skills ) {
        if( sk.obsolete() ) {
            continue;
        }
        j.start_object();
        j.member( "nombre", sk.name() );
        j.member( "nivel", static_cast<int>( u.get_skill_level( sk.ident() ) ) );
        j.end_object();
    }
    j.end_array();
    j.member( "competencias" );
    j.start_array();
    for( const proficiency_id &p : u.known_proficiencies() ) {
        j.write( p->name() );
    }
    j.end_array();
    j.end_object();
    return s.str();
}

std::string casilla_json( int dx, int dy )
{
    std::ostringstream s;
    JsonOut j( s );
    avatar &u = get_avatar();
    map &here = get_map();
    const tripoint_bub_ms p = casilla( u, dx, dy );
    if( !here.inbounds( p ) ) {
        j.write_null();
        return s.str();
    }
    j.start_object();
    j.member( "dx", dx );
    j.member( "dy", dy );
    const bool visible = u.sees( here, p );
    j.member( "visible", visible );
    if( visible ) {
        j.member( "terreno", here.ter( p ).obj().name() );
        // (para «Mirar», en la página: lo que dice el juego de cada cosa)
        j.member( "descTerreno", here.ter( p ).obj().description.translated() );
        j.member( "idTerreno", here.ter( p ).id().str() );
        if( here.has_furn( p ) ) {
            j.member( "mueble", here.furn( p ).obj().name() );
            j.member( "descMueble", here.furn( p ).obj().description.translated() );
            j.member( "idMueble", here.furn( p ).id().str() );
        }
        j.member( "objetos" );
        j.start_array();
        int n = 0;
        for( const item &it : here.i_at( p ) ) {
            if( ++n > 6 ) {
                break;
            }
            j.write( remove_color_tags( it.tname() ) );
        }
        j.end_array();
        j.member( "numObjetos", static_cast<int>( here.i_at( p ).size() ) );
        if( Creature *c = get_creature_tracker().creature_at( p ) ) {
            if( c != &u ) {
                j.member( "criatura", c->disp_name() );
                j.member( "hostil", c->attitude_to( u ) == Creature::Attitude::HOSTILE );
                j.member( "descCriatura" );
                j.start_array();
                for( const std::string &l : c->extended_description() ) {
                    j.write( remove_color_tags( l ) );
                }
                j.end_array();
                if( const monster *m = c->as_monster() ) {
                    j.member( "tipoCriatura", m->type->id.str() );
                }
            }
        }
        if( const optional_vpart_position vp = here.veh_at( p ) ) {
            j.member( "vehiculo", vp->vehicle().name );
        }
    }
    j.member( "acciones" );
    j.start_array();
    if( visible ) {
        for( const auto &a : acciones_en( u, p ) ) {
            j.start_object();
            j.member( "id", a.first );
            j.member( "nombre", a.second );
            j.end_object();
        }
    }
    j.end_array();
    j.end_object();
    return s.str();
}

std::string casilla_en_pixel_json( int px, int py )
{
    std::ostringstream s;
    JsonOut j( s );
#if defined(TILES)
    if( g != nullptr && g->w_terrain && use_tiles && tilecontext ) {
        const window_dimensions dim = get_window_dimensions( g->w_terrain );
        const int escala = std::max( 1, get_scaling_factor() );
        const point logico( px / escala, py / escala );
        const point desde = dim.window_pos_pixel;
        const point hasta = desde + dim.window_size_pixel;
        if( logico.x >= desde.x && logico.y >= desde.y && logico.x < hasta.x && logico.y < hasta.y ) {
            const point tile( tilecontext->get_tile_width(), tilecontext->get_tile_height() );
            const point_bub_ms c = cata_tiles::screen_to_player( logico - desde - tilecontext->desplazamiento_vista, tile,
                                   dim.window_size_pixel,
                                   g->ter_view_p.xy(), g->is_tileset_isometric() );
            const point_bub_ms yo = get_avatar().pos_bub().xy();
            j.start_object();
            j.member( "dx", c.x() - yo.x() );
            j.member( "dy", c.y() - yo.y() );
            j.end_object();
            return s.str();
        }
    }
#else
    static_cast<void>( px );
    static_cast<void>( py );
#endif
    j.write_null();
    return s.str();
}

void orden( const std::string &json )
{
    // (la tecla mantenida se apunta al momento: es solo un estado que se mira cuando le toca al jugador)
    if( json.find( "\"a\":\"mantener\"" ) != std::string::npos ) {
        try {
            JsonObject o = json_loader::from_string( json ).get_object();
            o.allow_omitted_members();
            realtime::mantener_direccion( o.get_int( "dx", 0 ), o.get_int( "dy", 0 ) );
        } catch( const std::exception & ) {
            realtime::mantener_direccion( 0, 0 );
        }
        return;
    }
    ordenes().push_back( json );
}

static void hacer( const std::string &json )
{
    avatar &u = get_avatar();
    map &here = get_map();
    JsonValue v = json_loader::from_string( json );
    JsonObject o = v.get_object();
    o.allow_omitted_members();
    const std::string a = o.get_string( "a", "" );
    // (las de los menús nuevos: coger lo elegido, el diálogo sin parar el mundo...)
    if( hacer_menus( a, o ) ) {
        return;
    }
    if( a == "velocidad" ) {
        // (el jugador: la pausa o la duración del día; la máxima es solo para las pruebas)
        realtime::poner( static_cast<realtime::velocidad>( std::clamp( o.get_int( "v", 2 ), 0,
                         static_cast<int>( realtime::velocidad::rapido ) ) ) );
        return;
    }
    if( a == "objeto" ) {
        std::optional<item_location> loc = objeto_por_id( u, o.get_string( "id", "" ) );
        if( !loc ) {
            add_msg( m_info, _( "That item is no longer there." ) );
            return;
        }
        const std::string que = o.get_string( "que", "" );
        if( que == "comer" ) {
            avatar_action::eat( u, *loc );
        } else if( que == "ponerse" ) {
            u.wear( *loc );
        } else if( que == "quitarse" ) {
            u.takeoff( *loc );
        } else if( que == "empunar" ) {
            u.wield( *loc );
        } else if( que == "leer" ) {
            u.read( *loc );
        } else if( que == "usar" ) {
            avatar_action::use_item( u, *loc );
        } else if( que == "soltar" ) {
            u.drop( *loc, u.pos_bub() );
        }
        return;
    }
    if( a == "fabricar" ) {
        u.make_craft( recipe_id( o.get_string( "receta", "" ) ), std::max( 1, o.get_int( "cantidad", 1 ) ) );
        return;
    }
    if( a == "construir" ) {
        // (sin el menú del juego: qué grupo y en qué casilla de al lado; sin grupo, el menú de siempre)
        if( o.has_string( "grupo" ) ) {
            const tripoint_bub_ms donde = u.pos_bub() + tripoint_rel_ms( o.get_int( "dx", 0 ), o.get_int( "dy", 0 ), 0 );
            if( !construir_en( construction_group_str_id( o.get_string( "grupo" ) ), donde ) ) {
                add_msg( m_info, _( "You can't build that there." ) );
            }
        } else {
            construction_menu( false );
        }
        return;
    }
    if( a == "idioma" ) {
        poner_idioma( o.get_string( "v", "es" ) );
        return;
    }
    // guardar la partida; y guardar y salir al menú principal (la página enseña su pantalla de inicio)
    if( a == "guardar" ) {
        g->quicksave();
        return;
    }
    if( a == "salir" ) {
        if( g->save() ) {
            g->uquit = QUIT_SAVED;
        }
        return;
    }
    if( a == "cancelar_actividad" ) {
        if( u.activity ) {
            u.cancel_activity();
        }
        return;
    }
    if( a == "mapa" ) {
        ui::omap::display();
        return;
    }
    const tripoint_bub_ms p = casilla( u, o.get_int( "dx", 0 ), o.get_int( "dy", 0 ) );
    if( !here.inbounds( p ) ) {
        return;
    }
    if( a == "ir" ) {
        const std::vector<tripoint_bub_ms> ruta = here.route( u, pathfinding_target::point( p ) );
        if( !ruta.empty() ) {
            u.set_destination( ruta );
        }
    } else if( a == "abrir" ) {
        u.assign_activity( open_tile_activity_actor( p ) );
    } else if( a == "cerrar" ) {
        u.assign_activity( close_tile_activity_actor( p ) );
    } else if( a == "coger" ) {
        g->pickup( p );
    } else if( a == "examinar" || a == "beber" || a == "pescar" || a == "vehiculo" ) {
        // (lo de examinar del mueble, o si no del terreno: abre lo que abra el juego, su menú de beber o pescar...)
        if( here.has_furn( p ) && !here.furn( p ).obj().has_examine( iexamine::none ) ) {
            here.furn( p ).obj().examine( u, p );
        } else {
            here.ter( p ).obj().examine( u, p );
        }
    } else if( a == "hablar" ) {
        if( npc *guy = get_creature_tracker().creature_at<npc>( p ) ) {
            u.talk_to( get_talker_for( *guy ) );
        }
    } else if( a == "atacar" ) {
        if( Creature *c = get_creature_tracker().creature_at( p ) ) {
            if( rl_dist( u.pos_bub(), p ) <= 1 ) {
                u.melee_attack( *c, true );
            } else {
                const std::vector<tripoint_bub_ms> ruta = here.route( u, pathfinding_target::adjacent( p ) );
                if( !ruta.empty() ) {
                    u.set_destination( ruta );
                }
            }
        }
    }
}

void turno()
{
    turno_menus();
    // (la tecla mantenida cambia a mitad de paso: se gira al momento)
    suave::girar_jugador( realtime::direccion_x(), realtime::direccion_y() );
    while( !ordenes().empty() ) {
        const std::string json = ordenes().front();
        ordenes().pop_front();
        try {
            hacer( json );
        } catch( const std::exception &e ) {
            debugmsg( "interfaz: orden mal formada %s: %s", json, e.what() );
        }
    }
}

} // namespace interfaz

#if defined(__EMSCRIPTEN__)
extern "C" {
    // (las cadenas que se devuelven valen hasta la siguiente llamada)
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_estado()
    {
        static std::string s;
        s = interfaz::estado_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_mensajes( int n )
    {
        static std::string s;
        s = interfaz::mensajes_json( n );
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_inventario()
    {
        static std::string s;
        s = interfaz::inventario_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_recetas()
    {
        static std::string s;
        s = interfaz::recetas_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_construcciones()
    {
        static std::string s;
        s = interfaz::construcciones_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_personaje()
    {
        static std::string s;
        s = interfaz::personaje_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_casilla( int dx, int dy )
    {
        static std::string s;
        s = interfaz::casilla_json( dx, dy );
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_casilla_en_pixel( int px, int py )
    {
        static std::string s;
        s = interfaz::casilla_en_pixel_json( px, py );
        return s.c_str();
    }
    // las órdenes: la página escribe el JSON (UTF-8) en este búfer y llama a cdda_ui_orden con su longitud
    static char bufer_ordenes[65536];
    EMSCRIPTEN_KEEPALIVE char *cdda_ui_bufer()
    {
        return bufer_ordenes;
    }
    EMSCRIPTEN_KEEPALIVE int cdda_ui_bufer_tam()
    {
        return static_cast<int>( sizeof( bufer_ordenes ) );
    }
    EMSCRIPTEN_KEEPALIVE void cdda_ui_orden( int n )
    {
        n = std::clamp( n, 0, static_cast<int>( sizeof( bufer_ordenes ) ) );
        interfaz::orden( std::string( bufer_ordenes, static_cast<size_t>( n ) ) );
    }
    // (Encargo 8) las de los menús nuevos; las que reciben texto lo leen del búfer (n: su longitud)
    static std::string bufer_texto( int n )
    {
        n = std::clamp( n, 0, static_cast<int>( sizeof( bufer_ordenes ) ) );
        return std::string( bufer_ordenes, static_cast<size_t>( n ) );
    }
    // el idioma (también en el menú principal, antes de la partida): 0 inglés, 1 español; y cuál hay
    EMSCRIPTEN_KEEPALIVE void cdda_ui_poner_idioma( int es )
    {
        interfaz::poner_idioma( es != 0 ? "es" : "en" );
    }
    EMSCRIPTEN_KEEPALIVE int cdda_ui_idioma()
    {
        return interfaz::idioma() == "es" ? 1 : 0;
    }
    // el menú principal: lo que pide la página (en el búfer) y los mundos con sus partidas; y la última muerte
    EMSCRIPTEN_KEEPALIVE void cdda_ui_menu_principal( int n )
    {
        interfaz::pedir_menu_principal( bufer_texto( n ) );
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_partidas()
    {
        static std::string s;
        s = interfaz::partidas_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_muerte()
    {
        static std::string s;
        s = interfaz::muerte_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE void cdda_ui_olvidar_muerte()
    {
        interfaz::olvidar_muerte();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_atlas()
    {
        static std::string s;
        s = interfaz::atlas_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_iconos( int n )
    {
        static std::string s;
        s = interfaz::iconos_json( bufer_texto( n ) );
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_equipo()
    {
        static std::string s;
        s = interfaz::equipo_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_receta( int n )
    {
        static std::string s;
        s = interfaz::receta_json( bufer_texto( n ) );
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_suelo( int dx, int dy )
    {
        static std::string s;
        s = interfaz::suelo_json( dx, dy );
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_robo( int dx, int dy )
    {
        static std::string s;
        s = interfaz::robo_json( dx, dy );
        return s.c_str();
    }
    // la lista del juego abierta (null si no hay), y elegir en ella (índice; -1 cancelar)
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_lista()
    {
        static std::string s;
        s = interfaz::lista_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE void cdda_ui_elegir( int i )
    {
        interfaz::elegir( i );
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_dialogo()
    {
        static std::string s;
        s = interfaz::dialogo_json();
        return s.c_str();
    }
    EMSCRIPTEN_KEEPALIVE const char *cdda_ui_mapa( int radio )
    {
        static std::string s;
        s = interfaz::mapa_json( radio );
        return s.c_str();
    }
}
#endif
