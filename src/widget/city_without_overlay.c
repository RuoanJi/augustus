#include "city_without_overlay.h"

#include "assets/assets.h"
#include "building/animation.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/dock.h"
#include "building/granary.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/model.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "building/type.h"
#include "city/buildings.h"
#include "city/entertainment.h"
#include "city/festival.h"
#include "city/labor.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/view.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/time.h"
#include "figure/formation_legion.h"
#include "figure/roamer_preview.h"
#include "game/resource.h"
#include "game/state.h"
#include "graphics/clouds.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/weather.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "sound/city.h"
#include "widget/city_bridge.h"
#include "widget/city_building_ghost.h"
#include "widget/city_figure.h"
#include "widget/city_draw_highway.h"

#define OFFSET(x,y) (x + GRID_SIZE * y)

#define WAREHOUSE_FLAG_FRAMES 9
#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE

static const int ADJACENT_OFFSETS[2][4][7] = {
    {
        {OFFSET(-1, 0), OFFSET(-1, -1),  OFFSET(-1, -2), OFFSET(0, -2), OFFSET(1, -2)},
        {OFFSET(0, -1), OFFSET(1, -1),  OFFSET(2, -1), OFFSET(2, 0), OFFSET(2, 1)},
        {OFFSET(1, 0), OFFSET(1, 1),  OFFSET(1, 2), OFFSET(0, 2), OFFSET(-1, 2)},
        {OFFSET(0, 1), OFFSET(-1, 1),  OFFSET(-2, 1), OFFSET(-2, 0), OFFSET(-2, -1)}
    },
    {
        {OFFSET(-1, 0), OFFSET(-1, -1),  OFFSET(-1, -2), OFFSET(-1, -3), OFFSET(0, -3),  OFFSET(1, -3), OFFSET(2, -3)},
        {OFFSET(0, -1), OFFSET(1, -1),  OFFSET(2, -1), OFFSET(3, -1), OFFSET(3, 0),  OFFSET(3, 1), OFFSET(3, 2)},
        {OFFSET(1, 0), OFFSET(1, 1),  OFFSET(1, 2), OFFSET(1, 3), OFFSET(0, 3),  OFFSET(-1, 3), OFFSET(-2, 3)},
        {OFFSET(0, 1), OFFSET(-1, 1),  OFFSET(-2, 1), OFFSET(-3, 1), OFFSET(-3, 0),  OFFSET(-3, -1), OFFSET(-3, -2)}
    }
};

static struct {
    time_millis last_water_animation_time;
    int advance_water_animation;

    int image_id_water_first;
    int image_id_water_last;
    int selected_figure_id;
    int highlighted_formation;
    unsigned int selected_building_id;
    pixel_coordinate *selected_figure_coord;

    float scale;
} draw_context;

static void init_draw_context(int selected_figure_id, pixel_coordinate *figure_coord, int highlighted_formation)
{
    draw_context.advance_water_animation = 0;
    if (!selected_figure_id) {
        time_millis now = time_get_millis();
        if (now - draw_context.last_water_animation_time > 60) {
            draw_context.last_water_animation_time = now;
            draw_context.advance_water_animation = 1;
        }
    }
    draw_context.image_id_water_first = image_group(GROUP_TERRAIN_WATER);
    draw_context.image_id_water_last = 5 + draw_context.image_id_water_first;
    draw_context.selected_figure_id = selected_figure_id;
    draw_context.selected_figure_coord = figure_coord;
    draw_context.highlighted_formation = highlighted_formation;
    draw_context.scale = city_view_get_scale() / 100.0f;
}

static int draw_building_as_deleted(building *b)
{
    b = building_main(b);
    return (b->id && (b->is_deleted || map_property_is_deleted(b->grid_offset)));
}

static int is_multi_tile_terrain(int grid_offset)
{
    return (!map_building_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1);
}

static int has_adjacent_deletion(int grid_offset)
{
    int size = map_property_multi_tile_size(grid_offset);
    int total_adjacent_offsets = size * 2 + 1;
    const int *adjacent_offset = ADJACENT_OFFSETS[size - 2][city_view_orientation() / 2];
    for (int i = 0; i < total_adjacent_offsets; ++i) {
        if (map_property_is_deleted(grid_offset + adjacent_offset[i]) ||
            draw_building_as_deleted(building_get(map_building_at(grid_offset + adjacent_offset[i])))) {
            return 1;
        }
    }
    return 0;
}

static void draw_roamer_frequency(int x, int y, int grid_offset)
{
    int travel_frequency = figure_roamer_preview_get_frequency(grid_offset);
    if (travel_frequency > 0 && travel_frequency <= FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
        static const color_t frequency_colors[] = {
            0x663377ff, 0x662266ee, 0x661155dd, 0x660044cc, 0x660033c4, 0x660022bb, 0x660011a4, 0x66000088
        };
        image_draw(image_group(GROUP_TERRAIN_FLAT_TILE), x, y,
            frequency_colors[travel_frequency - 1], draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_TILE) {
        image_blend_footprint_color(x, y, COLOR_MASK_RED, draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_EXIT_TILE) {
        image_blend_footprint_color(x, y, COLOR_MASK_GREEN, draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE) {
        image_draw_isometric_footprint(image_group(GROUP_TERRAIN_FLAT_TILE),
            x, y, COLOR_MASK_PINK, draw_context.scale);
    }
}

static color_t get_building_color_mask(const building *b)
{
    color_t color_mask = COLOR_MASK_NONE;
    const model_building *model = model_get_building(b->type);
    int labor_needed = model->laborers;
    if (!labor_needed && b->type != BUILDING_WAREHOUSE_SPACE) {
        // account for warehouse case
        color_mask = COLOR_MASK_NONE;
    } else {
        switch (b->type) {
            //buildings that have labor but no walkers
            case BUILDING_LATRINES:
            case BUILDING_WELL:
                color_mask = COLOR_MASK_NONE;
                //all other buildings
            default:
                color_mask = SELECTED_BUILDING_COLOR_MASK;
        }
    }
    return color_mask;
}

static int is_building_selected(const building *b)
{
    if (!config_get(CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING)) {
        return 0;
    }
    const building *main_building = building_main(b);
    unsigned int main_part_id = main_building->id;
    if (b->id == draw_context.selected_building_id || main_part_id == draw_context.selected_building_id) {
        return 1;
    } else {
        return 0;
    }

}

static void draw_footprint(int x, int y, int grid_offset)
{
    sound_city_progress_ambient();
    building_construction_record_view_position(x, y, grid_offset);
    if (grid_offset < 0 || !map_property_is_draw_tile(grid_offset)) {
        return;
    }
    // Valid grid_offset and leftmost tile -> draw
    int building_id = map_building_at(grid_offset);
    color_t color_mask = 0;
    if (building_id) {
        building *b = building_get(building_id);
        if (draw_building_as_deleted(b)) {
            color_mask = COLOR_MASK_RED;
        } else if (is_building_selected(b)) {
            color_mask = get_building_color_mask(b);
        }
        int view_x, view_y, view_width, view_height;
        city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);

        if (b->state == BUILDING_STATE_IN_USE) {
            int direction;
            if (x < view_x + 100) {
                direction = SOUND_DIRECTION_LEFT;
            } else if (x > view_x + view_width - 100) {
                direction = SOUND_DIRECTION_RIGHT;
            } else {
                direction = SOUND_DIRECTION_CENTER;
            }
            if (building_monument_is_unfinished_monument(b)) {
                sound_city_mark_construction_site_view(direction);
            } else {
                sound_city_mark_building_view(b->type, b->num_workers, direction);
            }
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        sound_city_mark_building_view(BUILDING_GARDENS, 0, SOUND_DIRECTION_CENTER);
    }
    int image_id = map_image_at(grid_offset);
    if (map_property_is_constructing(grid_offset)) { //&&
        //  !building_is_connectable(building_construction_type())) {
        image_id = image_group(GROUP_TERRAIN_OVERLAY);
    }
    if (draw_context.advance_water_animation &&
        image_id >= draw_context.image_id_water_first &&
        image_id <= draw_context.image_id_water_last) {
        image_id++;
        if (image_id > draw_context.image_id_water_last) {
            image_id = draw_context.image_id_water_first;
        }
        map_image_set(grid_offset, image_id);
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY) && !map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        city_draw_highway_footprint(x, y, draw_context.scale, grid_offset);
    } else {
        image_draw_isometric_footprint_from_draw_tile(image_id, x, y, color_mask, draw_context.scale);
    }
    if (!building_id && config_get(CONFIG_UI_SHOW_GRID) && draw_context.scale <= 2.0f) {
        //grid is drawn by the renderer directly at zoom > 200%
        static int grid_id = 0;
        if (!grid_id) {
            grid_id = assets_get_image_id("UI", "Grid_Full");
        }
        image_draw(grid_id, x, y, COLOR_GRID, draw_context.scale);
    }
    draw_roamer_frequency(x, y, grid_offset);
}

static void draw_hippodrome_spectators(const building *b, int x, int y, color_t color_mask)
{
    // get which part of the hippodrome is getting checked
    int building_part;
    if (b->prev_part_building_id == 0) {
        building_part = 0; // part 1, no previous building
    } else if (b->next_part_building_id == 0) {
        building_part = 2; // part 3, no next building
    } else {
        building_part = 1; // part 2
    }
    int orientation = building_rotation_get_building_orientation(b->subtype.orientation);
    int population = city_population();
    if ((building_part == 0) && population > 2000) {
        // first building part
        switch (orientation) {
            case DIR_0_TOP:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_2) + 6, x + 147, y - 72,
                    color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_1) + 8, x + 58, y - 79,
                    color_mask, draw_context.scale);
                break;
            case DIR_4_BOTTOM:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_2) + 8, x + 119, y - 80,
                    color_mask, draw_context.scale);
                break;
            case DIR_6_LEFT:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_1) + 6, x, y - 72,
                    color_mask, draw_context.scale);
        }
    } else if ((building_part == 1) && population > 100) {
        // middle building part
        switch (orientation) {
            case DIR_0_TOP:
            case DIR_4_BOTTOM:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_2) + 7, x + 122, y - 79,
                    color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
            case DIR_6_LEFT:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_1) + 7, x, y - 80, color_mask, draw_context.scale);
        }
    } else if ((building_part == 2) && population > 1000) {
        // last building part
        switch (orientation) {
            case DIR_0_TOP:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_2) + 8, x + 119, y - 80,
                    color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_1) + 6, x, y - 72, color_mask, draw_context.scale);
                break;
            case DIR_4_BOTTOM:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_2) + 6, x + 147, y - 72, color_mask,
                    draw_context.scale);
                break;
            case DIR_6_LEFT:
                image_draw(image_group(GROUP_BUILDING_HIPPODROME_1) + 8, x + 58, y - 79, color_mask,
                    draw_context.scale);
                break;
        }
    }
}

static void draw_entertainment_spectators(building *b, int x, int y, color_t color_mask)
{
    if (b->type == BUILDING_HIPPODROME && building_main(b)->num_workers > 0
        && city_entertainment_hippodrome_has_race()) {
        draw_hippodrome_spectators(b, x, y, color_mask);
    }
}

static void draw_workshop_raw_material_storage(const building *b, int x, int y, color_t color_mask)
{
    if (b->type == BUILDING_WINE_WORKSHOP) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL), x + 45, y + 23,
                color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_OIL_WORKSHOP) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 1, x + 35, y + 15,
                color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_WEAPONS_WORKSHOP) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 3, x + 46, y + 24,
                color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_FURNITURE_WORKSHOP) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 2, x + 48, y + 19,
                color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_POTTERY_WORKSHOP) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 4, x + 47, y + 24,
                color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_BRICKWORKS) {
        if (b->data.industry.has_raw_materials ||
            b->resources[RESOURCE_CLAY] >= building_get_required_raw_amount_for_production(b->type, RESOURCE_CLAY)) {
            image_draw(image_group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 4, x + 47, y + 24,
                color_mask, draw_context.scale);
        }
        if (b->data.industry.has_raw_materials ||
            b->resources[RESOURCE_SAND] >= building_get_required_raw_amount_for_production(b->type, RESOURCE_SAND)) {
            int image_id = assets_get_image_id("Industry", "Sand_Supplied_Workshop");
            image_draw(image_id, x + 67, y + 12, color_mask, draw_context.scale);
        }
    }
    if (b->type == BUILDING_CONCRETE_MAKER) {
        if (building_loads_stored(b) >= 2 * RESOURCE_ONE_LOAD || b->data.industry.has_raw_materials) {
            int image_id = assets_get_image_id("Industry", "Sand_Supplied_Workshop");
            image_draw(image_id, x + 47, y + 24, color_mask, draw_context.scale);
        }
    }
}

static void get_mothball_icon_position(const building *b, int *x, int *y)
{
    const image *img = image_get(building_image_get(b));
    int icon_id = assets_get_image_id("UI", "Mothball_Sprite");

    switch (b->type) {
        case BUILDING_WAREHOUSE:
            *x += 21;
            *y -= 60;
            break;
        case BUILDING_GRANARY:
            *x += 83;
            *y -= 120;
            break;
        case BUILDING_SAND_PIT:
        case BUILDING_STONE_QUARRY:
            *x += 50;
            *y -= 30;
            break;
        case BUILDING_FOUNTAIN:
            *x += 20;
            *y -= 15;
            break;
        case BUILDING_WHEAT_FARM:
        case BUILDING_VEGETABLE_FARM:
        case BUILDING_FRUIT_FARM:
        case BUILDING_OLIVE_FARM:
        case BUILDING_VINES_FARM:
        case BUILDING_PIG_FARM:
            *x += 50;
            *y -= 50;
            break;
        default:
            *x = (img->width - image_get(icon_id)->width) / 2;
            *y = (-image_get(icon_id)->height / 2) + 10;
            break;
    }
    if (img->top) {
        *y -= img->top->original.height;
    }
}

static void draw_mothball_icon(const building *b, int x, int y, color_t color_mask, int grid_offset)
{
    if (!b || (!b->data.industry.is_stockpiling && b->state != BUILDING_STATE_MOTHBALLED)) {
        return;
    }
    //farms have individual top drawings, but they have one building ID, unlike warehouses
    if (b->prev_part_building_id) {
        return; //do not draw mothball icon for non-main
    }
    if (building_is_farm(b->type)) {
        if (map_property_multi_tile_size(grid_offset) == 1) {
            return; //crop tile
        }
    }

    int mothball_x = 0;
    int mothball_y = 0;
    get_mothball_icon_position(b, &mothball_x, &mothball_y);
    x += mothball_x;
    y += mothball_y;

    if (b->state == BUILDING_STATE_MOTHBALLED) {
        image_draw(assets_get_image_id("UI", "Mothball_Sprite"), x, y, COLOR_MASK_NONE, draw_context.scale);
    } else if (b->data.industry.is_stockpiling) {
        image_draw(assets_get_image_id("UI", "Stockpile_Sprite"), x, y, COLOR_MASK_NONE, draw_context.scale);
    }
}

static void draw_senate_rating_flags(const building *b, int x, int y, color_t color_mask)
{
    if (b->type == BUILDING_SENATE) {
        // rating flags
        int image_id = image_group(GROUP_BUILDING_SENATE);
        image_draw(image_id + 1, x + 138, y + 44 - city_rating_culture() / 2, color_mask, draw_context.scale);
        image_draw(image_id + 2, x + 168, y + 36 - city_rating_prosperity() / 2, color_mask, draw_context.scale);
        image_draw(image_id + 3, x + 198, y + 27 - city_rating_peace() / 2, color_mask, draw_context.scale);
        image_draw(image_id + 4, x + 228, y + 19 - city_rating_favor() / 2, color_mask, draw_context.scale);
        // unemployed
        image_id = image_group(GROUP_FIGURE_HOMELESS);
        int unemployment_pct = city_labor_unemployment_percentage_for_senate();
        if (unemployment_pct > 0) {
            image_draw(image_id + 108, x + 80, y, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 5) {
            image_draw(image_id + 104, x + 230, y - 30, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 10) {
            image_draw(image_id + 107, x + 100, y + 20, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 15) {
            image_draw(image_id + 106, x + 235, y - 10, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 20) {
            image_draw(image_id + 106, x + 66, y + 20, color_mask, draw_context.scale);
        }
    }
}

static void draw_top(int x, int y, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    building *b = building_get(map_building_at(grid_offset));
    int image_id = map_image_at(grid_offset);
    color_t color_mask = 0;
    if (draw_building_as_deleted(b) || (map_property_is_deleted(grid_offset) && !is_multi_tile_terrain(grid_offset))) {
        color_mask = COLOR_MASK_RED;
    } else if (is_building_selected(b)) {
        color_mask = get_building_color_mask(b);
    }

    image_draw_isometric_top_from_draw_tile(image_id, x, y, color_mask, draw_context.scale);
    // specific buildings
    if (b->id > 0) { //dont draw or calculate for non-buildings
        draw_senate_rating_flags(b, x, y, color_mask);
        draw_mothball_icon(b, x, y, color_mask, grid_offset);
        draw_entertainment_spectators(b, x, y, color_mask);
        draw_workshop_raw_material_storage(b, x, y, color_mask);
    }

}

static void draw_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id) {
        figure *f = figure_get(figure_id);
        if (figure_id == draw_context.selected_figure_id) {
            if (!f->is_ghost || f->height_adjusted_ticks) {
                city_draw_selected_figure(f, x, y, draw_context.scale, draw_context.selected_figure_coord);
            }
        } else if (!f->is_ghost) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            city_draw_figure(f, x, y, draw_context.scale, highlight);
        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_fumigation(building *b, int x, int y, color_t color_mask)
{
    int image_id = image_group(GROUP_FIGURE_EXPLOSION); // smoke image_id
    image_id += b->fumigation_frame;
    image_draw(image_id, x, y, color_mask, draw_context.scale);
    if (image_id == image_group(GROUP_FIGURE_EXPLOSION) + 3) {
        b->fumigation_direction = 0;
    }
    if (image_id == image_group(GROUP_FIGURE_EXPLOSION)) {
        b->fumigation_direction = 1;
    }
    building_animation_advance_fumigation(b);
}

static void get_plague_icon_position_for_house(building *b, int *x, int *y, int is_fumigating)
{
    const image *img = image_get(building_image_get(b));
    int icon_id = is_fumigating ? image_group(GROUP_FIGURE_EXPLOSION) + 3 : image_group(GROUP_PLAGUE_SKULL);

    *x = (img->width - image_get(icon_id)->width) / 2;
    *y = -image_get(icon_id)->height / 2;

    if (img->top) {
        *y -= img->top->original.height;
    }
}

static void draw_plague(building *b, int x, int y, color_t color_mask)
{
    int x_pos = 0;
    int y_pos = 0;
    int is_fumigating = b->sickness_doctor_cure == 99;

    if (building_is_house(b->type)) {
        get_plague_icon_position_for_house(b, &x_pos, &y_pos, is_fumigating);
        if (x_pos || y_pos) {
            x_pos += x;
            y_pos += y;
        }
    } else if (b->type == BUILDING_DOCK) {
        if (is_fumigating) {
            x_pos = x + 68;
            y_pos = y - 38;
        } else {
            x_pos = x + 88;
            y_pos = y - 84;
        }
    } else if (b->type == BUILDING_WAREHOUSE) {
        if (is_fumigating) {
            x_pos = x + 10;
            y_pos = y - 64;
        } else {
            x_pos = x + 12;
            y_pos = y - 84;
        }
    } else if (b->type == BUILDING_GRANARY) {
        if (is_fumigating) {
            x_pos = x + 70;
            y_pos = y - 114;
        } else {
            x_pos = x + 80;
            y_pos = y - 124;
        }
    }

    if (x_pos && y_pos) {
        if (is_fumigating) {
            draw_fumigation(b, x_pos, y_pos, color_mask);
        } else {
            b->fumigation_direction = 1;
            image_draw(image_group(GROUP_PLAGUE_SKULL), x_pos, y_pos, color_mask, draw_context.scale);
        }
    }
}

static void draw_depot_resource(building *b, int x, int y, color_t color_mask)
{
    int img_id;

    if (b->num_workers > 0) {
        switch (b->data.depot.current_order.resource_type) {
            case RESOURCE_VEGETABLES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Vegetables");
                break;
            case RESOURCE_FRUIT:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Fruit");
                break;
            case RESOURCE_MEAT:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Meat");
                break;
            case RESOURCE_FISH:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Fish");
                break;
            case RESOURCE_VINES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Grapes");
                break;
            case RESOURCE_POTTERY:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Pottery");
                break;
            case RESOURCE_FURNITURE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Furniture");
                break;
            case RESOURCE_OIL:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Oil");
                break;
            case RESOURCE_WINE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Wine");
                break;
            case RESOURCE_MARBLE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Marble");
                break;
            case RESOURCE_WEAPONS:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Weapons");
                break;
            case RESOURCE_CLAY:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Clay");
                break;
            case RESOURCE_TIMBER:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Timber");
                break;
            case RESOURCE_OLIVES:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Olives");
                break;
            case RESOURCE_IRON:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Iron");
                break;
            case RESOURCE_GOLD:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Gold");
                break;
            case RESOURCE_SAND:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Sand");
                break;
            case RESOURCE_STONE:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Stone");
                break;
            case RESOURCE_BRICKS:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Bricks");
                break;
            case RESOURCE_WHEAT:
            default:
                img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Wheat");
                break;
        }
    } else {
        img_id = assets_get_image_id("Admin_Logistics", "Cart_Depot_Cat");
    }
    image_draw(img_id, x + 11, y, COLOR_MASK_NONE, draw_context.scale);
}

static void draw_dock_workers(const building *b, int x, int y, color_t color_mask)
{
    if (!b->has_plague) {
        int num_dockers = building_dock_count_idle_dockers(b);
        if (num_dockers > 0) {
            int image_dock = map_image_at(b->grid_offset);
            int image_dockers = image_group(GROUP_BUILDING_DOCK_DOCKERS);
            if (image_dock == image_group(GROUP_BUILDING_DOCK_1)) {
                image_dockers += 0;
            } else if (image_dock == image_group(GROUP_BUILDING_DOCK_2)) {
                image_dockers += 3;
            } else if (image_dock == image_group(GROUP_BUILDING_DOCK_3)) {
                image_dockers += 6;
            } else {
                image_dockers += 9;
            }
            if (num_dockers == 2) {
                image_dockers += 1;
            } else if (num_dockers == 3) {
                image_dockers += 2;
            }
            const image *img = image_get(image_dockers);
            if (img->animation) {
                image_draw(image_dockers, x + img->animation->sprite_offset_x, y + img->animation->sprite_offset_y,
                    color_mask, draw_context.scale);
            }
        }
    }
}

static void draw_permissions_flag(building *b, int x, int y, color_t color_mask)
{
    if (b->has_plague) {
        return;
    }
    static int base_permission_image[8];
    if (!base_permission_image[0]) {
        base_permission_image[0] = 0xdeadbeef; // Invalid image ID, just to confirm the other values have been set
        base_permission_image[1] = assets_get_image_id("UI", "Warehouse_Flag_Market");
        base_permission_image[2] = assets_get_image_id("UI", "Warehouse_Flag_Land");
        base_permission_image[3] = assets_get_image_id("UI", "Warehouse_Flag_Market_Land");
        base_permission_image[4] = assets_get_image_id("UI", "Warehouse_Flag_Sea");
        base_permission_image[5] = assets_get_image_id("UI", "Warehouse_Flag_Market_Sea");
        base_permission_image[6] = assets_get_image_id("UI", "Warehouse_Flag_Land_Sea");
        base_permission_image[7] = assets_get_image_id("UI", "Warehouse_Flag_All");
    }
    const building_storage *storage = building_storage_get(b->storage_id);
    int flag_permission_mask = 0x7;
    int permissions = (~storage->permissions) & flag_permission_mask;
    if (!permissions) {
        return;
    }
    image_draw(base_permission_image[permissions] + b->data.warehouse.flag_frame, x, y, color_mask, draw_context.scale);

    building_animation_advance_storage_flag(b, base_permission_image[permissions]);
}

static void draw_warehouse_ornaments(int x, int y, color_t color_mask)
{
    image_draw(image_group(GROUP_BUILDING_WAREHOUSE) + 17, x - 4, y - 42, color_mask, draw_context.scale);
}

static void draw_granary_stores(const image *img, const building *b, int x, int y, color_t color_mask)
{
    if (img->animation) {
        image_draw(image_group(GROUP_BUILDING_GRANARY) + 1,
            x + img->animation->sprite_offset_x,
            y + 60 + img->animation->sprite_offset_y - img->height,
            color_mask, draw_context.scale);
    }
    if (b->resources[RESOURCE_NONE] < FULL_GRANARY) {
        image_draw(image_group(GROUP_BUILDING_GRANARY) + 2, x + 33, y - 60, color_mask, draw_context.scale);
    }
    if (b->resources[RESOURCE_NONE] < THREEQUARTERS_GRANARY) {
        image_draw(image_group(GROUP_BUILDING_GRANARY) + 3, x + 56, y - 50, color_mask, draw_context.scale);
    }
    if (b->resources[RESOURCE_NONE] < HALF_GRANARY) {
        image_draw(image_group(GROUP_BUILDING_GRANARY) + 4, x + 91, y - 50, color_mask, draw_context.scale);
    }
    if (b->resources[RESOURCE_NONE] < QUARTER_GRANARY) {
        image_draw(image_group(GROUP_BUILDING_GRANARY) + 5, x + 117, y - 62, color_mask, draw_context.scale);
    }
}

static void draw_ceres_module_crops(int x, int y, int image_offset, color_t color_mask)
{
    int image_id = assets_get_image_id("Monuments", "Ceres Module 1 Crop");
    image_draw(image_id + image_offset, x, y, color_mask, draw_context.scale);
}

static void draw_neptune_fountain(int x, int y, int image_offset, color_t color_mask)
{
    int image_id = assets_get_image_id("Monuments", "Neptune Module 2 Fountain");
    image_draw(image_id + image_offset, x, y, color_mask, draw_context.scale);
}

static void draw_animation(int x, int y, int grid_offset)
{
    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    int building_id = map_building_at(grid_offset);
    building *b = building_get(building_id);
    color_t color_mask = 0;
    if (draw_building_as_deleted(b) || map_property_is_deleted(grid_offset)) {
        color_mask = COLOR_MASK_RED;
    } else if (is_building_selected(b)) {
        color_mask = get_building_color_mask(b);
    }
    if (img->animation) {
        if (map_property_is_draw_tile(grid_offset)) {
            if (b->type == BUILDING_DOCK) {
                draw_dock_workers(b, x, y, color_mask);
            } else if (b->type == BUILDING_WAREHOUSE) {
                draw_warehouse_ornaments(x, y, color_mask);
                draw_permissions_flag(b, x + 19, y - 56, color_mask);
            } else if (b->type == BUILDING_GRANARY) {
                draw_granary_stores(img, b, x, y, color_mask);
                draw_permissions_flag(b, x + 81, y - 101, color_mask);
            } else if (b->type == BUILDING_BURNING_RUIN && b->has_plague) {
                image_draw(image_group(GROUP_PLAGUE_SKULL), x + 18, y - 32, color_mask, draw_context.scale);
            }
            int animation_offset = building_animation_offset(b, image_id, grid_offset);
            if (b->type != BUILDING_HIPPODROME && animation_offset > 0) {
                int y_offset = img->top ? img->top->original.height - FOOTPRINT_HALF_HEIGHT : 0;
                if (animation_offset > img->animation->num_sprites) {
                    animation_offset = img->animation->num_sprites;
                }
                if (b->type == BUILDING_GRAND_TEMPLE_CERES && b->monument.upgrades == 1) {
                    draw_ceres_module_crops(x + 190, y + 95 - y_offset, b->monument.secondary_frame, color_mask);
                }
                if (b->type == BUILDING_GRAND_TEMPLE_NEPTUNE && b->monument.upgrades == 2) {
                    draw_neptune_fountain(x + 98, y + 87 - y_offset, (animation_offset - 1) % 5, color_mask);
                }
                if (b->type == BUILDING_GRANARY) {
                    image_draw(image_id + img->animation->start_offset + animation_offset + 5, x + 77, y - 49,
                        color_mask, draw_context.scale);
                } else {
                    image_draw(image_id + img->animation->start_offset + animation_offset,
                        x + img->animation->sprite_offset_x,
                        y + img->animation->sprite_offset_y - y_offset,
                        color_mask, draw_context.scale);
                }
                if (b->type == BUILDING_COLOSSEUM) {
                    int festival_id = calc_bound(city_festival_games_active(), 0, 4);
                    int extra_x = festival_id ? 57 : 127;
                    int extra_y = festival_id ? 12 : 93;
                    int overlay_id = assets_get_image_id("Monuments", "Col Base Overlay") + festival_id;
                    image_draw(overlay_id, x + extra_x, y + extra_y - y_offset, color_mask, draw_context.scale);
                }
            }
            if (b->has_plague) {
                draw_plague(b, x, y, color_mask);
            }
            if (b->type == BUILDING_DEPOT) {
                draw_depot_resource(b, x, y, color_mask);
            }
        }
    } else if (map_property_is_draw_tile(grid_offset) && building_id && b->has_plague) {
        draw_plague(b, x, y, color_mask);
    } else if (map_sprite_bridge_at(grid_offset)) {
        city_draw_bridge(x, y, draw_context.scale, grid_offset);
    } else if (b->type == BUILDING_FORT) {
        if (map_property_is_draw_tile(grid_offset)) {
            building *fort = building_get(map_building_at(grid_offset));
            image_id = assets_get_image_id("Military", "Fort_Jav_Flag_Central");
            switch (fort->subtype.fort_figure_type) {
                case FIGURE_FORT_LEGIONARY: image_id += 2; break;
                case FIGURE_FORT_MOUNTED: image_id += 1; break;
                case FIGURE_FORT_JAVELIN: break;
            }
            switch (scenario_property_climate()) {
                case CLIMATE_DESERT: image_id += 3; break;
                case CLIMATE_NORTHERN: image_id += 6; break;
                default: break;
            }
            if (fort->subtype.fort_figure_type == FIGURE_FORT_INFANTRY) {
                image_id = assets_get_image_id("Military", "fort_aux_inf_flag_central");
                switch (scenario_property_climate()) {
                    case CLIMATE_DESERT: image_id += 2; break;
                    case CLIMATE_NORTHERN: image_id += 1; break;
                    default: break;
                }
            }
            if (fort->subtype.fort_figure_type == FIGURE_FORT_ARCHER) {
                image_id = assets_get_image_id("Military", "fort_aux_arch_flag_central");
                switch (scenario_property_climate()) {
                    case CLIMATE_DESERT: image_id += 2; break;
                    case CLIMATE_NORTHERN: image_id += 1; break;
                    default: break;
                }
            }
            image_draw(image_id, x + 81, y + 5,
                draw_building_as_deleted(fort) ? COLOR_MASK_RED : COLOR_MASK_NONE, draw_context.scale);
        }
    } else if (b->type == BUILDING_GATEHOUSE) {
        int xy = map_property_multi_tile_xy(grid_offset);
        int orientation = city_view_orientation();
        if ((orientation == DIR_0_TOP && xy == EDGE_X1Y1) ||
            (orientation == DIR_2_RIGHT && xy == EDGE_X0Y1) ||
            (orientation == DIR_4_BOTTOM && xy == EDGE_X0Y0) ||
            (orientation == DIR_6_LEFT && xy == EDGE_X1Y0)) {
            building *gate = building_get(map_building_at(grid_offset));
            image_id = image_group(GROUP_BUILDING_GATEHOUSE);
            color_mask = draw_building_as_deleted(gate) ? COLOR_MASK_RED : 0;
            if (gate->subtype.orientation == 1) {
                if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
                    image_draw(image_id, x - 22, y - 80, color_mask, draw_context.scale);
                } else {
                    image_draw(image_id + 1, x - 18, y - 81, color_mask, draw_context.scale);
                }
            } else if (gate->subtype.orientation == 2) {
                if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
                    image_draw(image_id + 1, x - 18, y - 81, color_mask, draw_context.scale);
                } else {
                    image_draw(image_id, x - 22, y - 80, color_mask, draw_context.scale);
                }
            }
        }
    }
}

static void draw_elevated_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);


    while (figure_id > 0) {
        figure *f = figure_get(figure_id);

        if ((f->use_cross_country && !f->is_ghost && !f->dont_draw_elevated) || f->height_adjusted_ticks) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            city_draw_figure(f, x, y, draw_context.scale, highlight);
        } else if (f->building_id == draw_context.selected_building_id) { //figure originates from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, draw_context.scale, highlight);
            }

        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_hippodrome_ornaments(int x, int y, int grid_offset)
{
    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    building *b = building_get(map_building_at(grid_offset));
    if (img->animation && map_property_is_draw_tile(grid_offset) && b->type == BUILDING_HIPPODROME) {
        int top_height = img->top ? img->top->original.height : 0;
        image_draw(image_id + 1,
            x + img->animation->sprite_offset_x,
            y + img->animation->sprite_offset_y - top_height + FOOTPRINT_HALF_HEIGHT,
            draw_building_as_deleted(b) ? COLOR_MASK_RED : COLOR_MASK_NONE, draw_context.scale
        );
    }
}

static int should_draw_top_before_deletion(int grid_offset)
{
    return is_multi_tile_terrain(grid_offset) && has_adjacent_deletion(grid_offset);
}

static void deletion_draw_terrain_top(int x, int y, int grid_offset)
{
    if (map_property_is_draw_tile(grid_offset) && should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
}

static void deletion_draw_figures_animations(int x, int y, int grid_offset)
{
    if (map_property_is_deleted(grid_offset) || draw_building_as_deleted(building_get(map_building_at(grid_offset)))) {
        image_blend_footprint_color(x, y, COLOR_MASK_RED, draw_context.scale);
    }
    if (map_property_is_draw_tile(grid_offset) && !should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
    draw_figures(x, y, grid_offset);
    draw_animation(x, y, grid_offset);
}

static void deletion_draw_remaining(int x, int y, int grid_offset)
{
    draw_elevated_figures(x, y, grid_offset);
    draw_hippodrome_ornaments(x, y, grid_offset);
}

static void draw_connectable_construction_ghost(int x, int y, int grid_offset)
{
    if (!map_property_is_constructing(grid_offset)) {
        return;
    }
    static building b;
    b.type = building_construction_type();
    if (building_connectable_gate_type(b.type) && map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        b.type = building_connectable_gate_type(b.type);
    }
    b.grid_offset = grid_offset;
    if (building_properties_for_type(b.type)->rotation_offset) {
        b.subtype.orientation = building_rotation_get_rotation();
    }
    int image_id = building_image_get(&b);
    image_draw_isometric_footprint_from_draw_tile(image_id, x, y, COLOR_MASK_BUILDING_GHOST, draw_context.scale);
    image_draw_isometric_top_from_draw_tile(image_id, x, y, COLOR_MASK_BUILDING_GHOST, draw_context.scale);
}

static int get_highlighted_formation_id(const map_tile *tile)
{
    if (!config_get(CONFIG_UI_HIGHLIGHT_LEGIONS)) {
        return 0;
    }
    int highlighted_formation_id = formation_legion_or_herd_at_grid_offset(tile->grid_offset);
    if (highlighted_formation_id <= 0) {
        return 0;
    }
    formation *highlighted_formation = formation_get(highlighted_formation_id);
    if (highlighted_formation->in_distant_battle) {
        return 0;
    }
    int selected_formation_id = formation_get_selected();
    // don't highlight friendly legions if we've already selected one
    if (selected_formation_id && highlighted_formation_id != selected_formation_id && !highlighted_formation->is_herd) {
        return 0;
    }
    // don't highlight herds unless we have a formation selected
    if (!selected_formation_id && highlighted_formation->is_herd) {
        return 0;
    }
    if (config_get(CONFIG_GP_CH_AUTO_KILL_ANIMALS) && highlighted_formation->is_herd) {
        return 0;
    }
    return highlighted_formation_id;
}

static void update_clouds(void)
{
    int camera_x, camera_y;
    if (game_state_is_paused() || (!window_is(WINDOW_CITY) && !window_is(WINDOW_CITY_MILITARY))) {
        clouds_pause();
    }
    city_view_get_camera_in_pixels(&camera_x, &camera_y);
    clouds_draw(camera_x, camera_y, GRID_SIZE * 60, GRID_SIZE * 30, draw_context.scale);
}

/***
 * TODO:
 *
 * The following code should be executed, depending on a new "console window" output
 *
 * For now, we'll leave it commented out so it's added in the future.
 */

 /**
 static void draw_routing(int x, int y, int grid_offset)
 {
     int tx = map_grid_offset_to_x(grid_offset);
     int ty = map_grid_offset_to_y(grid_offset);
     map_routing_distance_grid *distance = map_routing_get_distance_grid();
     int16_t dist = distance->determined.items[grid_offset];
     if (!dist) {
         return;
     }
     if (tx == distance->dst_x && ty == distance->dst_y) {
         int dst_image_id = assets_get_image_id("UI", "Happy God Icon");
         image_draw(dst_image_id, x + 29 - 10, y + 15 - 10, 0, 1);
     }
     uint8_t text[20];
     string_from_int(text, dist, 0);
     text_draw_centered(text, x, y, 58, FONT_NORMAL_BLACK, COLOR_WHITE);
 }

 static void draw_highway_terrain(int x, int y, int grid_offset)
 {
     int offset = -8;
     int terrain = map_terrain_get(grid_offset);
     if (terrain & TERRAIN_HIGHWAY_TOP_LEFT) {
         text_draw_centered("TL", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_TOP_RIGHT) {
         text_draw_centered("TR", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_BOTTOM_LEFT) {
         text_draw_centered("BL", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_BOTTOM_RIGHT) {
         text_draw_centered("BR", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
 }

 static void draw_tile_coords(int x, int y, int grid_offset)
 {
     int tx = map_grid_offset_to_x(grid_offset);
     int ty = map_grid_offset_to_y(grid_offset);
     uint8_t text[20];
     string_from_int(text, tx, 0);
     int len = string_length(text);
     string_copy(",", text + len, 2);
     len++;
     //string_from_int(text + len, ty, 0);
     //text_draw_centered(text, x, y + 4, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
     string_from_int(text, grid_offset, 0);
     text_draw_centered(text, x, y + 10, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
 }

 static void draw_road_network_id(int x, int y, int grid_offset)
 {
     int road_network_id = map_road_network_get(grid_offset);
     uint8_t text[20];
     string_from_int(text, road_network_id, 0);
     text_draw_centered(text, x, y + 4, 58, FONT_NORMAL_BLACK, COLOR_WHITE);
 }
 ***/

void city_without_overlay_draw(int selected_figure_id, pixel_coordinate *figure_coord, const map_tile *tile, unsigned int roamer_preview_building_id)
{
    int highlighted_formation_id = get_highlighted_formation_id(tile);
    init_draw_context(selected_figure_id, figure_coord, highlighted_formation_id);
    if (roamer_preview_building_id) {
        draw_context.selected_building_id = roamer_preview_building_id;//store the clicked building id
    }
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    graphics_fill_rect(x, y, width, height, COLOR_BLACK);
    int should_mark_deleting = city_building_ghost_mark_deleting(tile);
    city_view_foreach_valid_map_tile(draw_footprint);
    if (!should_mark_deleting) {
        city_view_foreach_valid_map_tile_row(
            draw_top,
            draw_figures,
            draw_animation
        );
        if (!selected_figure_id) {
            if (building_is_connectable(building_construction_type())) {
                city_view_foreach_valid_map_tile(draw_connectable_construction_ghost);
            }
            city_building_ghost_draw(tile);
        }
        city_view_foreach_valid_map_tile_row(
            draw_elevated_figures,
            draw_hippodrome_ornaments,
            0
        );
    } else {
        city_view_foreach_valid_map_tile(deletion_draw_terrain_top);
        city_view_foreach_valid_map_tile(deletion_draw_figures_animations);
        city_view_foreach_valid_map_tile(deletion_draw_remaining);
    }
    update_clouds();
    update_weather();
}
