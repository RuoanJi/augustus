#include "start_year.h"

#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "scenario/editor.h"
#include "scenario/property.h"
#include "window/editor/starting_conditions.h"
#include "window/numeric_input.h"

static void button_era(int param1, int param2);
static void button_year(int param1, int param2);

static generic_button buttons[] = {
    {158, 100, 258, 130, GB_IMMEDIATE, button_era, button_none},
    {278, 100, 398, 130, GB_IMMEDIATE, button_year, button_none},
};

static int focus_button_id;

static void draw_background(void)
{
    // TODO draw city map
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    outer_panel_draw(128, 44, 20, 10);

    lang_text_draw_centered(44, 13, 138, 56, 320, FONT_LARGE_BLACK);
    lang_text_draw_centered(13, 3, 128, 178, 320, FONT_NORMAL_BLACK);

    int start_year = scenario_property_start_year();
    button_border_draw(158, 100, 100, 30, focus_button_id == 1);
    lang_text_draw_centered(20, start_year >= 0 ? 1 : 0, 158, 110, 100, FONT_NORMAL_BLACK);

    button_border_draw(278, 100, 120, 30, focus_button_id == 2);
    text_draw_number_centered(start_year >= 0 ? start_year : -start_year, 278, 110, 120, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void handle_mouse(const mouse *m)
{
    if (m->right.went_down) {
        window_editor_starting_conditions_show();
        return;
    }

    generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 2, &focus_button_id);
}

static void button_era(int param1, int param2)
{
    scenario_editor_set_start_year(-scenario_property_start_year());
}

static void set_year(int value)
{
    if (scenario_property_start_year() < 0) {
        value = -value;
    }
    scenario_editor_set_start_year(value);
}
static void button_year(int param1, int param2)
{
    window_numeric_input_show(screen_dialog_offset_x() + 100, screen_dialog_offset_y() + 100,
                              4, 9999, set_year);
}

void window_editor_start_year_show(void)
{
    window_type window = {
        WINDOW_EDITOR_START_YEAR,
        draw_background,
        draw_foreground,
        handle_mouse
    };
    window_show(&window);
}
