#include "formation_enemy.h"

#include "building/building.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/figures.h"
#include "city/gods.h"
#include "city/message.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/random.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/formation_layout.h"
#include "figure/route.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/routing.h"
#include "map/routing_path.h"
#include "map/soldier_strength.h"
#include "map/terrain.h"

#define INFINITE 10000

static const int ENEMY_ATTACK_PRIORITY[4][100] = {
    {
        BUILDING_GRANARY, BUILDING_WAREHOUSE, BUILDING_MARKET,
        BUILDING_WHEAT_FARM, BUILDING_VEGETABLE_FARM, BUILDING_FRUIT_FARM,
        BUILDING_OLIVE_FARM, BUILDING_VINES_FARM, BUILDING_PIG_FARM, 0
    },
    {
        BUILDING_SENATE, BUILDING_SENATE_1_UNUSED,
        BUILDING_FORUM_2_UNUSED, BUILDING_FORUM, 0
    },
    {
        BUILDING_GOVERNORS_PALACE, BUILDING_GOVERNORS_VILLA, BUILDING_GOVERNORS_HOUSE,
        BUILDING_HOUSE_LUXURY_PALACE, BUILDING_HOUSE_LARGE_PALACE,
        BUILDING_HOUSE_MEDIUM_PALACE, BUILDING_HOUSE_SMALL_PALACE,
        BUILDING_HOUSE_GRAND_VILLA, BUILDING_HOUSE_LARGE_VILLA,
        BUILDING_HOUSE_MEDIUM_VILLA, BUILDING_HOUSE_SMALL_VILLA,
        BUILDING_HOUSE_GRAND_INSULA, BUILDING_HOUSE_LARGE_INSULA,
        BUILDING_HOUSE_MEDIUM_INSULA, BUILDING_HOUSE_SMALL_INSULA,
        BUILDING_HOUSE_LARGE_CASA, BUILDING_HOUSE_SMALL_CASA,
        BUILDING_HOUSE_LARGE_HOVEL, BUILDING_HOUSE_SMALL_HOVEL,
        BUILDING_HOUSE_LARGE_SHACK, BUILDING_HOUSE_SMALL_SHACK,
        BUILDING_HOUSE_LARGE_TENT, BUILDING_HOUSE_SMALL_TENT, 0
    },
    {
        BUILDING_MILITARY_ACADEMY, BUILDING_PREFECTURE, 0
    }
};

static const int RIOTER_ATTACK_PRIORITY[29] = {
    BUILDING_GOVERNORS_PALACE,
    BUILDING_GOVERNORS_VILLA,
    BUILDING_GOVERNORS_HOUSE,
    BUILDING_AMPHITHEATER,
    BUILDING_THEATER,
    BUILDING_HOSPITAL,
    BUILDING_ACADEMY,
    BUILDING_BATHHOUSE,
    BUILDING_LIBRARY,
    BUILDING_SCHOOL,
    BUILDING_DOCTOR,
    BUILDING_GLADIATOR_SCHOOL,
    BUILDING_ACTOR_COLONY,
    BUILDING_CHARIOT_MAKER,
    BUILDING_LION_HOUSE,
    BUILDING_BARBER,
    BUILDING_TAVERN,
    BUILDING_ARENA,
    BUILDING_HORSE_STATUE,
    BUILDING_LEGION_STATUE,
    BUILDING_LARGE_STATUE,
    BUILDING_MEDIUM_STATUE,
    BUILDING_OBELISK,
    BUILDING_HOUSE_LUXURY_PALACE,
    BUILDING_HOUSE_LARGE_PALACE,
    BUILDING_HOUSE_MEDIUM_PALACE,
    BUILDING_HOUSE_SMALL_PALACE,
    BUILDING_HOUSE_GRAND_VILLA,
    BUILDING_HOUSE_LARGE_VILLA,
};

#define NUM_LAYOUT_FORMATIONS 40
static const int LAYOUT_ORIENTATION_OFFSETS[13][4][NUM_LAYOUT_FORMATIONS] = {
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -4, 0, 4, 0, -12, 0, 12, 0, -4, 12, 4, 12, 0},
        {0, 0, 0, -4, 0, 4, 0, -12, 0, 12, 12, -4, 12, 4, 0},
        {0, 0, -4, 0, 4, 0, -12, 0, 12, 0, -4, -12, 4, -12, 0},
        {0, 0, 0, -4, 0, 4, 0, -12, 0, 12, -12, -4, -12, 4, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    }
};

static building *get_best_building(const int *priority_order, int max)
{
    for (int i = 0; i < max; i++) {
        building_type type = priority_order[i];
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE) {
                return b;
            }
        }
    }
    return 0;
}

static building *get_best_and_closest_building(int x, int y, const int *priority_order, int max)
{
    int min_distance = INFINITE;
    for (int i = 0; i < max; i++) {
        building_type type = priority_order[i];
        building *best_building = 0;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            int distance = calc_maximum_distance(x, y, b->x, b->y);
            if (distance < min_distance) {
                best_building = b;
                min_distance = distance;
            }
        }
        if (best_building) {
            return best_building;
        }
    }
    return 0;
}

int formation_rioter_get_target_building(int *x_tile, int *y_tile)
{
    building *best_building = get_best_building(RIOTER_ATTACK_PRIORITY, 29);
    if (!best_building) {
        return 0;
    }
    *x_tile = best_building->x;
    *y_tile = best_building->y;
    return best_building->id;
}

int formation_rioter_get_target_building_for_robbery(int x, int y, int *x_tile, int *y_tile)
{
    building *best_building = 0;
    int closest = INFINITE;

    static const building_type building_targets[] = { BUILDING_SENATE, BUILDING_FORUM };
    for (int i = 0; i < 2; i++) {
        building_type type = building_targets[i];
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            int distance = calc_maximum_distance(x, y, b->x, b->y);
            if (distance >= 150) {
                continue;
            }
            if (distance < closest) {
                closest = distance;
                best_building = b;
            }
        }
    }
    if (!best_building) {
        return 0;
    }

    *x_tile = best_building->road_access_x;
    *y_tile = best_building->road_access_y;

    return best_building->id;
}

static int set_enemy_target_building(formation *m)
{
    int attack = m->attack_type;
    if (attack == FORMATION_ATTACK_RANDOM) {
        attack = random_byte() & 3;
    }
    building *best_building = get_best_and_closest_building(m->x_home, m->y_home, ENEMY_ATTACK_PRIORITY[attack], 100);

    if (!best_building) {
        // no target buildings left: take rioter attack priority
        best_building = get_best_and_closest_building(m->x_home, m->y_home, RIOTER_ATTACK_PRIORITY, 29);
    }
    if (best_building) {
        if (best_building->type == BUILDING_WAREHOUSE) {
            formation_set_destination_building(m, best_building->x + 1, best_building->y, best_building->id + 1);
        } else {
            formation_set_destination_building(m, best_building->x, best_building->y, best_building->id);
        }
    }
    return best_building != 0;
}


static int get_structures_on_native_land(int *dst_x, int *dst_y)
{
    int meeting_x, meeting_y;
    city_buildings_main_native_meeting_center(&meeting_x, &meeting_y);

    building_type native_buildings[] = { BUILDING_NATIVE_MEETING, BUILDING_NATIVE_WATCHTOWER, 
        BUILDING_NATIVE_HUT, BUILDING_NATIVE_HUT_ALT };
    int min_distance = INFINITE;

    for (int i = 0; i < sizeof(native_buildings) / sizeof(native_buildings[0]) && min_distance == INFINITE; i++) {
        building_type type = native_buildings[i];
        int size = building_properties_for_type(type)->size;
        int radius = size * 2;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            int x_min, y_min, x_max, y_max;
            map_grid_get_area(b->x, b->y, size, radius, &x_min, &y_min, &x_max, &y_max);
            for (int yy = y_min; yy <= y_max; yy++) {
                for (int xx = x_min; xx <= x_max; xx++) {
                    if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GARDEN)) {
                        int distance = calc_maximum_distance(meeting_x, meeting_y, xx, yy);
                        if (distance < min_distance) {
                            min_distance = distance;
                            *dst_x = xx;
                            *dst_y = yy;
                        }
                    }
                }
            }
        }
    }
    return min_distance < INFINITE;
}

static void set_native_target_building(formation *m)
{
    int meeting_x, meeting_y;
    city_buildings_main_native_meeting_center(&meeting_x, &meeting_y);
    building *min_building = 0;
    int min_distance = INFINITE;
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        switch (b->type) {
            case BUILDING_MISSION_POST:
            case BUILDING_NATIVE_HUT:
            case BUILDING_NATIVE_HUT_ALT:
            case BUILDING_NATIVE_CROPS:
            case BUILDING_NATIVE_MEETING:
            case BUILDING_NATIVE_MONUMENT:
            case BUILDING_NATIVE_WATCHTOWER:
            case BUILDING_NATIVE_DECORATION:
            case BUILDING_WAREHOUSE:
            case BUILDING_FORT:
            case BUILDING_FORT_GROUND:
            case BUILDING_ROADBLOCK:
            case BUILDING_ROOFED_GARDEN_WALL_GATE:
            case BUILDING_PANELLED_GARDEN_GATE:
            case BUILDING_LOOPED_GARDEN_GATE:
            case BUILDING_HEDGE_GATE_DARK:
            case BUILDING_HEDGE_GATE_LIGHT:
                break;
            default:
                {
                    int distance = calc_maximum_distance(meeting_x, meeting_y, b->x, b->y);
                    if (distance < min_distance) {
                        min_building = b;
                        min_distance = distance;
                    }
                }
                break;
        }
    }
    if (min_building) {
        formation_set_destination_building(m, min_building->x, min_building->y, min_building->id);
    } else {
        int dst_x = 0;
        int dst_y = 0;
        int has_target = get_structures_on_native_land(&dst_x, &dst_y);
        if (has_target) {
            formation_set_destination_building(m, dst_x, dst_y, 0);
        } else {
            formation_retreat(m);
        }
    }
}

static void set_figures_to_initial(const formation *m)
{
    for (int i = 0; i < MAX_FORMATION_FIGURES; i++) {
        if (m->figures[i] > 0) {
            figure *f = figure_get(m->figures[i]);
            if (f->action_state != FIGURE_ACTION_149_CORPSE &&
                f->action_state != FIGURE_ACTION_150_ATTACK) {
                f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
                f->wait_ticks = 0;
            }
        }
    }
}

int formation_enemy_next_formation_move(const formation *m, int* figure_offsets, int from_x, int from_y, int to_x, int to_y, int check_depth, int *x_tile, int *y_tile) {
    for (int r = 0; r <= 10; r++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(to_x, to_y, 1, r, &x_min, &y_min, &x_max, &y_max);
        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                if (from_x == xx && from_y == yy) {
                    // Do not check place where we are coming from
                    continue;
                }
                int can_move = 1;
                for (int fig = 0; fig < m->num_figures; fig++) {
                    int grid_offset = map_grid_offset(xx, yy) + figure_offsets[fig];
                    if (!map_grid_is_valid_offset(grid_offset)) {
                        can_move = 0;
                        break;
                    }
                    if (map_terrain_is(grid_offset, TERRAIN_IMPASSABLE_ENEMY)) {
                        can_move = 0;
                        break;
                    }
                    if (map_routing_distance(grid_offset) <= 0) {
                        can_move = 0;
                        break;
                    }
                    if (map_has_figure_at(grid_offset) &&
                        figure_get(map_figure_at(grid_offset))->formation_id != m->id) {
                        can_move = 0;
                        break;
                    }
                }
                if (can_move) {
                    int x_next, y_next;
                    if (check_depth > 0) {
                        // Check if next move is possible
                        if (!formation_enemy_next_formation_move(m, figure_offsets, xx, yy, to_x, to_y, check_depth - 1, &x_next, &y_next)) {
                            continue;
                        }
                        // Do not allow to return on previous position
                        if (x_next == from_x && y_next == from_y) {
                            continue;
                        }
                    }
                    *x_tile = xx;
                    *y_tile = yy;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int formation_enemy_move_formation_to(const formation *m, int x, int y, int *x_tile, int *y_tile)
{
    int base_offset = map_grid_offset(
        formation_layout_position_x(m->layout, 0),
        formation_layout_position_y(m->layout, 0));
    int figure_offsets[50];
    figure_offsets[0] = 0;
    for (int i = 1; i < m->num_figures; i++) {
        figure_offsets[i] = map_grid_offset(
            formation_layout_position_x(m->layout, i),
            formation_layout_position_y(m->layout, i)) - base_offset;
    }
    map_routing_noncitizen_can_travel_over_land(x, y, -1, -1, 8, 0, 600);

    // Find next move position and check if we will not stay
    // on the same place or return to previous position afterwards
    return formation_enemy_next_formation_move(m, figure_offsets, m->x_home, m->y_home, x, y, 1, x_tile, y_tile);
}

static void mars_kill_enemies(void)
{
    int to_kill = city_god_spirit_of_mars_power();
    if (to_kill <= 0) {
        return;
    }
    int grid_offset = 0;
    for (int i = 1; i < figure_count() && to_kill > 0; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (figure_is_enemy(f) && f->type != FIGURE_ENEMY54_GLADIATOR) {
            f->action_state = FIGURE_ACTION_149_CORPSE;
            to_kill--;
            if (!grid_offset) {
                grid_offset = f->grid_offset;
            }
        }
    }
    city_god_spirit_of_mars_mark_used();
    city_message_post(1, MESSAGE_SPIRIT_OF_MARS, 0, grid_offset);
}

static void get_layout_orientation_offset(const enemy_army *army, const formation *m, int *x_offset, int *y_offset)
{
    int layout = army->layout;
    int legion_index_offset = (2 * m->enemy_legion_index) % NUM_LAYOUT_FORMATIONS;
    *x_offset = LAYOUT_ORIENTATION_OFFSETS[layout][m->orientation / 2][legion_index_offset];
    *y_offset = LAYOUT_ORIENTATION_OFFSETS[layout][m->orientation / 2][legion_index_offset + 1];
}

static void update_enemy_movement(formation *m, int roman_distance)
{
    const enemy_army *army = enemy_army_get(m->invasion_id);
    formation_state *state = &m->enemy_state;
    int regroup = 0;
    int halt = 0;
    int pursue_target = 0;
    int advance = 0;
    int target_formation_id = 0;
    if (m->missile_fired) {
        halt = 1;
    } else if (m->missile_attack_timeout) {
        pursue_target = 1;
        target_formation_id = m->missile_attack_formation_id;
    } else if (m->wait_ticks < 32) {
        regroup = 1;
        state->duration_advance = 4;
    } else if (army->ignore_roman_soldiers) {
        halt = 0;
        regroup = 0;
        advance = 1;
    } else {
        int halt_duration, advance_duration, regroup_duration;
        if (army->layout == FORMATION_ENEMY_MOB || army->layout == FORMATION_ENEMY12) {
            switch (m->enemy_legion_index) {
                case 0:
                case 1:
                    regroup_duration = 2;
                    advance_duration = 4;
                    halt_duration = 2;
                    break;
                case 2:
                case 3:
                    regroup_duration = 2;
                    advance_duration = 5;
                    halt_duration = 3;
                    break;
                default:
                    regroup_duration = 2;
                    advance_duration = 6;
                    halt_duration = 4;
                    break;
            }
            if (!roman_distance) {
                advance_duration += 6;
                halt_duration--;
                regroup_duration--;
            }
        } else {
            if (roman_distance) {
                regroup_duration = 6;
                advance_duration = 4;
                halt_duration = 2;
            } else {
                regroup_duration = 1;
                advance_duration = 12;
                halt_duration = 1;
            }
        }
        if (state->duration_halt) {
            state->duration_advance = 0;
            state->duration_regroup = 0;
            halt = 1;
            state->duration_halt--;
            if (state->duration_halt <= 0) {
                state->duration_regroup = regroup_duration;
                set_figures_to_initial(m);
                regroup = 0;
                halt = 1;
            }
        } else if (state->duration_regroup) {
            state->duration_advance = 0;
            state->duration_halt = 0;
            regroup = 1;
            state->duration_regroup--;
            if (state->duration_regroup <= 0) {
                state->duration_advance = advance_duration;
                set_figures_to_initial(m);
                advance = 1;
                regroup = 0;
            }
        } else {
            state->duration_regroup = 0;
            state->duration_halt = 0;
            advance = 1;
            state->duration_advance--;
            if (state->duration_advance <= 0) {
                state->duration_halt = halt_duration;
                set_figures_to_initial(m);
                halt = 1;
                advance = 0;
            }
        }
    }

    if (m->wait_ticks > 32) {
        mars_kill_enemies();
    }
    if (halt) {
        formation_set_destination(m, m->x_home, m->y_home);
    } else if (pursue_target) {
        if (target_formation_id > 0) {
            const formation *target = formation_get(target_formation_id);
            if (target->num_figures > 0) {
                formation_set_destination(m, target->x_home, target->y_home);
            }
        } else {
            formation_set_destination(m, army->destination_x, army->destination_y);
        }
    } else if (regroup) {
        int x_offset, y_offset;
        get_layout_orientation_offset(army, m, &x_offset, &y_offset);
        x_offset += army->home_x;
        y_offset += army->home_y;
        map_grid_bound(&x_offset, &y_offset);
        int x_tile, y_tile;
        if (formation_enemy_move_formation_to(m, x_offset, y_offset, &x_tile, &y_tile)) {
            formation_set_destination(m, x_tile, y_tile);
        }
    } else if (advance) {
        int x_offset, y_offset;
        get_layout_orientation_offset(army, m, &x_offset, &y_offset);
        x_offset += army->destination_x;
        y_offset += army->destination_y;
        map_grid_bound(&x_offset, &y_offset);
        int x_tile, y_tile;
        if (formation_enemy_move_formation_to(m, x_offset, y_offset, &x_tile, &y_tile)) {
            formation_set_destination(m, x_tile, y_tile);
        }
    }
}

static int formation_fully_in_city(const formation *m)
{
    for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
        figure *f = figure_get(m->figures[n]);
        if (f->state != FIGURE_STATE_DEAD && f->is_ghost) {
            return 0;
        }
    }
    return 1;
}

static void update_enemy_formation(formation *m, int *roman_distance)
{
    enemy_army *army = enemy_army_get_editable(m->invasion_id);
    if (enemy_army_is_stronger_than_legions()) {
        if (m->figure_type != FIGURE_FORT_JAVELIN) {
            army->ignore_roman_soldiers = 1;
        }
    }
    formation_decrease_monthly_counters(m);
    if (city_figures_soldiers() <= 0) {
        formation_clear_monthly_counters(m);
    }
    for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
        figure *f = figure_get(m->figures[n]);
        if (f->action_state == FIGURE_ACTION_150_ATTACK) {
            figure *opponent = figure_get(f->opponent_id);
            if (!figure_is_dead(opponent) && figure_is_legion(opponent)) {
                formation_record_fight(m);
            }
        }
    }
    if (formation_has_low_morale(m)) {
        for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
            figure *f = figure_get(m->figures[n]);
            if (f->action_state != FIGURE_ACTION_150_ATTACK &&
                f->action_state != FIGURE_ACTION_149_CORPSE &&
                f->action_state != FIGURE_ACTION_148_FLEEING) {
                f->action_state = FIGURE_ACTION_148_FLEEING;
                figure_route_remove(f);
            }
        }
        return;
    }
    if (m->figures[0]) {
        figure *f = figure_get(m->figures[0]);
        if (f->state == FIGURE_STATE_ALIVE) {
            formation_set_home(m, f->x, f->y);
        }
    }
    if (!army->formation_id) {
        army->formation_id = m->id;
        army->home_x = m->x_home;
        army->home_y = m->y_home;
        army->layout = m->layout;
        *roman_distance = 0;
        map_routing_noncitizen_can_travel_over_land(m->x_home, m->y_home, -1, -1, 8, 100000, 300);
        int x_tile, y_tile;
        if (map_soldier_strength_get_max(m->x_home, m->y_home, 16, &x_tile, &y_tile)) {
            *roman_distance = 1;
        } else if (map_soldier_strength_get_max(m->x_home, m->y_home, 32, &x_tile, &y_tile)) {
            *roman_distance = 2;
        }
        if (army->ignore_roman_soldiers) {
            *roman_distance = 0;
        }
        if (*roman_distance == 1) {
            // attack roman legion
            army->destination_x = x_tile;
            army->destination_y = y_tile;
            army->destination_building_id = 0;
        } else {
            if (!set_enemy_target_building(m) && !army->started_retreating && formation_fully_in_city(m)) {
                city_message_post(1, MESSAGE_ENEMIES_LEAVING, 0, 0);
                army->started_retreating = 1;
            }
            army->destination_x = m->destination_x;
            army->destination_y = m->destination_y;
            army->destination_building_id = m->destination_building_id;
        }
    }
    m->enemy_legion_index = army->num_legions++;
    m->wait_ticks++;
    if (!army->started_retreating) {
        formation_set_destination_building(m, army->destination_x, army->destination_y, army->destination_building_id);
    } else {
        formation_retreat(m);
    }

    update_enemy_movement(m, *roman_distance);
}

void formation_enemy_update(void)
{
    if (enemy_army_total_enemy_formations() <= 0) {
        enemy_armies_clear_ignore_roman_soldiers();
    } else {
        enemy_army_calculate_roman_influence();
        enemy_armies_clear_formations();
        int roman_distance = 0;
        for (int i = 1; i < formation_count(); i++) {
            formation *m = formation_get(i);
            if (m->in_use && !m->is_herd && !m->is_legion) {
                update_enemy_formation(m, &roman_distance);
            }
        }
    }
    if (city_military_is_native_attack_active()) {
        set_native_target_building(formation_get(NATIVE_FORMATION));
    }
}
