#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/audio.h>
#include <naomi/system.h>
#include <naomi/font.h>
#include <naomi/sprite/sprite.h>
#include "common.h"
#include "state.h"
#include "screens.h"
#include "controls.h"

// The possible screens that we can have in this diagnostics rom.
#define SCREEN_MAIN_MENU 0
#define SCREEN_MONITOR_TESTS 1
#define SCREEN_AUDIO_TESTS 2
#define SCREEN_INPUT_TESTS 3
#define SCREEN_EEPROM_TESTS 4
#define SCREEN_SRAM_TESTS 5
#define SCREEN_DIP_TESTS 6

// These aren't really screens, but its easiest if we just add the
// action functionality into screens themselves.
#define SCREEN_SYSTEM_MENU 1001
#define SCREEN_REBOOT_SYSTEM 1002

// The main menu selected entry, used for making sure the cursor
// is preserved when exiting a diagnostic screen.
static unsigned int main_selected_entry = 0;

typedef struct
{
    char *name;
    unsigned int screen;
    unsigned int (*func)(state_t *state, int reinit);
} entry_t;

// Forward definitions for various displays.
unsigned int monitor_tests(state_t *state, int reinit);
unsigned int audio_tests(state_t *state, int reinit);
unsigned int input_tests(state_t *state, int reinit);
unsigned int dip_tests(state_t *state, int reinit);
unsigned int eeprom_tests(state_t *state, int reinit);
unsigned int sram_tests(state_t *state, int reinit);
unsigned int system_menu(state_t *state, int reinit);
unsigned int reboot_system(state_t *state, int reinit);

// The main configuration for what screens exist and where to find them.
entry_t entries[] = {
    {
        "Monitor Tests",
        SCREEN_MONITOR_TESTS,
        monitor_tests,
    },
    {
        "Audio Tests",
        SCREEN_AUDIO_TESTS,
        audio_tests,
    },
    {
        "Input Tests",
        SCREEN_INPUT_TESTS,
        input_tests,
    },
    {
        "DIP Switch Tests",
        SCREEN_DIP_TESTS,
        dip_tests,
    },
    {
        "EEPROM Tests",
        SCREEN_EEPROM_TESTS,
        eeprom_tests,
    },
    {
        "SRAM Tests",
        SCREEN_SRAM_TESTS,
        sram_tests,
    },
    /* An empty entry. */
    {
        "",
        0,
        NULL,
    },
    {
        "BIOS Test Menu",
        SCREEN_SYSTEM_MENU,
        system_menu,
    },
    {
        "Reboot Naomi",
        SCREEN_REBOOT_SYSTEM,
        reboot_system,
    }
};

unsigned int main_menu(state_t *state, int reinit)
{
    // Grab our configuration.
    static unsigned int menuentries = sizeof(entries) / sizeof(entries[0]);

    // Leave 24 pixels of padding on top and bottom of the menu.
    // Space out entries 16 pixels across.
    static unsigned int maxentries = 0;

    // Where we are on the screen for both our cursor and scroll position.
    static unsigned int cursor = 0;
    static unsigned int top = 0;

    // Initialize our state if we're being loaded from another screen.
    if (reinit)
    {
        maxentries = (video_height() - (24 + 16)) / 21;
        cursor = main_selected_entry;
        top = 0;
        if (cursor >= (top + maxentries))
        {
            top = cursor - (maxentries - 1);
        }
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    // Get our controls, including repeats.
    controls_t controls = get_controls(state, reinit);

    if (controls.test_pressed || controls.start_pressed)
    {
        // Enter this menu option.
        main_selected_entry = cursor;
        new_screen = entries[cursor].screen;
    }
    else if (controls.service_pressed)
    {
        // Cycled cursor to next entry, wrapping around to the top.
        if (cursor < (menuentries - 1))
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);

            /* We don't have to worry about the last or first entry being empty,
             * because we promise never to do that in the config above! */
            cursor ++;
            while (strlen(entries[cursor].name) == 0)
            {
                cursor ++;
            }

            if (cursor >= (top + maxentries))
            {
                top = cursor - (maxentries - 1);
            }
        }
        else
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
            cursor = 0;

            if (cursor < top)
            {
                top = cursor;
            }
        }
    }
    else if (controls.up_pressed)
    {
        // Moved cursor up.
        if (cursor > 0)
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);

            /* We don't have to worry about the last or first entry being empty,
             * because we promise never to do that in the config above! */
            cursor --;
            while (strlen(entries[cursor].name) == 0)
            {
                cursor --;
            }

        }
        if (cursor < top)
        {
            top = cursor;
        }
    }
    else if (controls.down_pressed)
    {
        // Moved cursor down.
        if (cursor < (menuentries - 1))
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);

            /* We don't have to worry about the last or first entry being empty,
             * because we promise never to do that in the config above! */
            cursor ++;
            while (strlen(entries[cursor].name) == 0)
            {
                cursor ++;
            }

        }
        if (cursor >= (top + maxentries))
        {
            top = cursor - (maxentries - 1);
        }
    }

    // Now, render the actual list of screens.
    unsigned int scroll_indicator_move_amount[4] = { 1, 2, 1, 0 };
    int scroll_offset = scroll_indicator_move_amount[((int)(state->animation_counter * 4.0)) & 0x3];

    if (top > 0)
    {
        sprite_draw_simple(video_width() / 2 - 10, 10 - scroll_offset, state->sprite_up);
    }

    for (unsigned int entry = top; entry < top + maxentries; entry++)
    {
        if (entry >= menuentries)
        {
            // Ran out of entries to display.
            break;
        }

        // Draw cursor itself.
        if (entry == cursor)
        {
            sprite_draw_simple(24, 24 + ((entry - top) * 21), state->sprite_cursor);
        }

        // Draw game, highlighted if it is selected.
        ta_draw_text(48, 22 + ((entry - top) * 21), state->font_18pt, entry == cursor ? rgb(255, 255, 20) : rgb(255, 255, 255), entries[entry].name);
    }

    if ((top + maxentries) < menuentries)
    {
        sprite_draw_simple(video_width() / 2 - 10, 24 + (maxentries * 21) + scroll_offset, state->sprite_down);
    }

    return new_screen;
}

// Number of different test screens, the first being the instructions.
#define MONITOR_TEST_SCREENS 7

// The number of steps (individual color areas on each gradient).
#define GRADIENT_STEPS 24

// The safe area of black around the gradient area itself.
#define GRADIENT_SAFE_AREA 32

// The number of steps (individual boxes) for the cross hatch.
#define CROSS_HORIZONTAL_STEPS 16
#define CROSS_VERTICAL_STEPS 12

// The line width for the cross hatch.
#define CROSS_WEIGHT 3

unsigned int monitor_tests(state_t *state, int reinit)
{
    static unsigned int screen = 0;

    if (reinit)
    {
        screen = 0;
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MONITOR_TESTS;

    // Get our controls, including repeats.
    controls_t controls = get_controls(state, reinit);

    if (controls.test_pressed || controls.start_pressed)
    {
        // Exit out of the monitor test screen.
        new_screen = SCREEN_MAIN_MENU;
    }
    else if (controls.service_pressed || controls.right_pressed)
    {
        // Cycled screen to next entry, wrapping around to the second screen.
        if (screen < (MONITOR_TEST_SCREENS - 1))
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
            screen ++;
        }
        else
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
            screen = 1;
        }
    }
    else if (controls.left_pressed)
    {
        // Moved cursor up.
        if (screen > 1)
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
            screen --;
        }
        else
        {
            audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
            screen = MONITOR_TEST_SCREENS - 1;
        }
    }

    // Now, draw the screen.
    switch (screen)
    {
        case 0:
        {
            // Instructions page.
            char *instructions[] = {
                "Use joystick left/right to move between pages.",
                "Press start button to exit back to main menu.",
                "",
                "Alternatively, use service to move between pages and test to exit.",
                "",
                "Page 1 is a pure white screen for white balance adjustments.",
                "Page 2-4 are pure red/green/blue for purity adjustments.",
                "Page 5 is a gradient for individual gain/bias adjustments.",
                "Page 6 is a cross hatch for focus and convergence adjustments.",
            };

            for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
            {
                font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
                ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
            }

            break;
        }
        case 1:
        case 2:
        case 3:
        case 4:
        {
            // Pure color screens for purity/white balance adjustments.
            vertex_t vertexes[] = {
                { 0, video_height(), 1 },
                { 0, 0, 1 },
                { video_width(), 0, 1 },
                { video_width(), video_height(), 1 },
            };

            // Change these colors if you need to change what color is displayed on the screen.
            color_t colors[] = {
                rgb(255, 255, 255),
                rgb(255, 0, 0),
                rgb(0, 255, 0),
                rgb(0, 0, 255),
            };

            ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, vertexes, colors[screen - 1]);
            break;
        }
        case 5:
        {
            // Individual gain gradients and grayscale gradient for individual gun bias/gain adjustments.
            // These will be modulated for a full gradient, so only change these if you want to change
            // what color is used on the gradient itself.
            color_t colors[] = {
                rgb(255, 0, 0),
                rgb(255, 255, 0),
                rgb(0, 255, 0),
                rgb(0, 255, 255),
                rgb(0, 0, 255),
                rgb(255, 0, 255),
                rgb(255, 255, 255),
            };

            unsigned int step = (video_width() - (GRADIENT_SAFE_AREA * 2)) / GRADIENT_STEPS;
            unsigned int height = (video_height() - (GRADIENT_SAFE_AREA * 2) - 24) / (sizeof(colors) / sizeof(colors[0]));

            for (int bar = 0; bar < GRADIENT_STEPS; bar++)
            {
                // Calculate left/right of bars, as well as the label itself.
                int left = GRADIENT_SAFE_AREA + (bar * step);
                int right = left + step;

                char idbuf[16];
                sprintf(idbuf, "%d", bar + 1);
                font_metrics_t metrics = font_get_text_metrics(state->font_12pt, idbuf);
                ta_draw_text((left + right - metrics.width) / 2, GRADIENT_SAFE_AREA, state->font_12pt, rgb(255, 255, 255), idbuf);

                for (int color = 0; color < (sizeof(colors) / sizeof(colors[0])); color++)
                {
                    /* Calculate where the box goes, leaving room for the text labels. */
                    int top = GRADIENT_SAFE_AREA + 24 + (color * height);
                    int bottom = top + height;

                    vertex_t vertexes[] = {
                        { left, bottom, 1 },
                        { left, top, 1 },
                        { right, top, 1 },
                        { right, bottom, 1 },
                    };

                    /* Calculate the box color, based on where it is on the screen. */
                    float percent = (float)(bar + 1) / (float)GRADIENT_STEPS;
                    color_t actual = {
                        (int)((float)colors[color].r * percent),
                        (int)((float)colors[color].g * percent),
                        (int)((float)colors[color].b * percent),
                        colors[color].a
                    };

                    /* Draw it! */
                    ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, vertexes, actual);
                }
            }

            break;
        }
        case 6:
        {
            // Cross hatch pattern, for convergence and focus adjustments.
            int hjump = (video_width() - CROSS_WEIGHT) / CROSS_HORIZONTAL_STEPS;
            int vjump = (video_height() - CROSS_WEIGHT) / CROSS_VERTICAL_STEPS;

            // Because the above might not divide evenly, we need to bump random lines to make sure
            // we line up the last line on the far right of the screen.
            int herror = video_width() - ((hjump * CROSS_HORIZONTAL_STEPS) + CROSS_WEIGHT);
            int verror = video_height() - ((vjump * CROSS_VERTICAL_STEPS) + CROSS_WEIGHT);

            int accum = 0;
            int bump = 0;

            for (int hloc = 0; hloc < (CROSS_HORIZONTAL_STEPS + 1); hloc++)
            {
                int left = hloc * hjump;
                int right = left + CROSS_WEIGHT;
                int top = 0;
                int bottom = video_height();

                accum += herror;
                while (accum >= CROSS_HORIZONTAL_STEPS)
                {
                    bump++;
                    accum -= CROSS_HORIZONTAL_STEPS;
                }

                vertex_t vertexes[] = {
                    { left + bump, bottom, 1 },
                    { left + bump, top, 1 },
                    { right + bump, top, 1 },
                    { right + bump, bottom, 1 },
                };

                ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, vertexes, rgb(255, 255, 255));
            }

            accum = 0;
            bump = 0;

            for (int vloc = 0; vloc < (CROSS_VERTICAL_STEPS + 1); vloc++)
            {
                int left = 0;
                int right = video_width();
                int top = vloc * vjump;
                int bottom = top + CROSS_WEIGHT;

                accum += verror;
                while (accum >= CROSS_VERTICAL_STEPS)
                {
                    bump++;
                    accum -= CROSS_VERTICAL_STEPS;
                }

                vertex_t vertexes[] = {
                    { left, bottom + bump, 1 },
                    { left, top + bump, 1 },
                    { right, top + bump, 1 },
                    { right, bottom + bump, 1 },
                };

                ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, vertexes, rgb(255, 255, 255));
            }

            break;
        }
    }

    return new_screen;
}

unsigned int audio_tests(state_t *state, int reinit)
{
    // TODO: Audio subsystem test.
    if (reinit)
    {
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    return new_screen;
}

unsigned int input_tests(state_t *state, int reinit)
{
    // TODO: Input tests
    if (reinit)
    {
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    return new_screen;
}

unsigned int dip_tests(state_t *state, int reinit)
{
    // TODO: Figure out how to read DIP switches, display them.
    if (reinit)
    {
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    return new_screen;
}

unsigned int eeprom_tests(state_t *state, int reinit)
{
    // TODO: Read EEPROM, flip bits, write back, read again, verify
    // flipped, flip again and write back original.
    if (reinit)
    {
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    return new_screen;
}

unsigned int sram_tests(state_t *state, int reinit)
{
    // TODO: Walking 1s, walking 0s, address tests.
    if (reinit)
    {
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_MAIN_MENU;

    return new_screen;
}

unsigned int system_menu(state_t *state, int reinit)
{
    enter_test_mode();
    return SCREEN_SYSTEM_MENU;
}

unsigned int reboot_system(state_t *state, int reinit)
{
    // TODO: Figure out how BIOS reboots system, do that here.
    return SCREEN_REBOOT_SYSTEM;
}

void draw_screen(state_t *state)
{
    // What screen we're on right now.
    static unsigned int curscreen = SCREEN_MAIN_MENU;
    static unsigned int oldscreen = -1;

    // The screen we are requested to go to next.
    unsigned int newscreen;

    switch(curscreen)
    {
        case SCREEN_MAIN_MENU:
        {
            newscreen = main_menu(state, curscreen != oldscreen);
            break;
        }
        default:
        {
            int found = 0;
            for (unsigned int e = 0; e < sizeof(entries) / sizeof(entries[0]); e++)
            {
                if (entries[e].screen == curscreen)
                {
                    newscreen = entries[e].func(state, curscreen != oldscreen);
                    found = 1;
                    break;
                }
            }
            
            if (!found)
            {
                // Should never happen, but still, whatever.
                newscreen = curscreen;
            }

            break;
        }
    }

    // Track what screen we are versus what we were so we know when we
    // switch screens.
    oldscreen = curscreen;
    curscreen = newscreen;
}
