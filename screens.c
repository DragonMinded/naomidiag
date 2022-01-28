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
        "JVS Input Tests",
        SCREEN_INPUT_TESTS,
        input_tests,
    },
    {
        "Filter Board Input Tests",
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

void ta_draw_rectangle(int left, int top, int right, int bottom, color_t color)
{
    vertex_t vertexes[] = {
        { left, bottom, 1 },
        { left, top, 1 },
        { right, top, 1 },
        { right, bottom, 1 },
    };

    ta_fill_box(TA_CMD_POLYGON_TYPE_TRANSPARENT, vertexes, color);
}

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
    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);

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
        sprite_draw_simple(video_width() / 2 - 10, 10 - scroll_offset, state->sprites.up);
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
            sprite_draw_simple(24, 24 + ((entry - top) * 21), state->sprites.cursor);
        }

        // Draw game, highlighted if it is selected.
        ta_draw_text(48, 22 + ((entry - top) * 21), state->font_18pt, entry == cursor ? rgb(255, 255, 20) : rgb(255, 255, 255), entries[entry].name);
    }

    if ((top + maxentries) < menuentries)
    {
        sprite_draw_simple(video_width() / 2 - 10, 24 + (maxentries * 21) + scroll_offset, state->sprites.down);
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
    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);

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
            // Change these colors if you need to change what color is displayed on the screen.
            color_t colors[] = {
                rgb(255, 255, 255),
                rgb(255, 0, 0),
                rgb(0, 255, 0),
                rgb(0, 0, 255),
            };

            ta_draw_rectangle(0, 0, video_width(), video_height(), colors[screen - 1]);
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

                    /* Calculate the box color, based on where it is on the screen. */
                    float percent = (float)(bar + 1) / (float)GRADIENT_STEPS;
                    color_t actual = {
                        (int)((float)colors[color].r * percent),
                        (int)((float)colors[color].g * percent),
                        (int)((float)colors[color].b * percent),
                        colors[color].a
                    };

                    /* Draw it! */
                    ta_draw_rectangle(left, top, right, bottom, actual);
                }
            }

            break;
        }
        case 6:
        {
            // Cross hatch pattern, for convergence and focus adjustments.
            int chors = video_is_vertical() ? CROSS_VERTICAL_STEPS : CROSS_HORIZONTAL_STEPS;
            int cvers = video_is_vertical() ? CROSS_HORIZONTAL_STEPS : CROSS_VERTICAL_STEPS;

            int hjump = (video_width() - CROSS_WEIGHT) / chors;
            int vjump = (video_height() - CROSS_WEIGHT) / cvers;

            // Because the above might not divide evenly, we need to bump random lines to make sure
            // we line up the last line on the far right of the screen.
            int herror = video_width() - ((hjump * chors) + CROSS_WEIGHT);
            int verror = video_height() - ((vjump * cvers) + CROSS_WEIGHT);

            int accum = 0;
            int bump = 0;

            for (int hloc = 0; hloc < (chors + 1); hloc++)
            {
                int left = hloc * hjump;
                int right = left + CROSS_WEIGHT;
                int top = 0;
                int bottom = video_height();

                accum += herror;
                while (accum >= chors)
                {
                    bump++;
                    accum -= chors;
                }

                ta_draw_rectangle(left + bump, top, right + bump, bottom, rgb(255, 255, 255));
            }

            accum = 0;
            bump = 0;

            for (int vloc = 0; vloc < (cvers + 1); vloc++)
            {
                int left = 0;
                int right = video_width();
                int top = vloc * vjump;
                int bottom = top + CROSS_WEIGHT;

                accum += verror;
                while (accum >= cvers)
                {
                    bump++;
                    accum -= cvers;
                }

                ta_draw_rectangle(left, top + bump, right, bottom + bump, rgb(255, 255, 255));
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

#define DIP_WIDTH 16
#define DIP_HEIGHT 44
#define DIP_SPACING 6
#define DIP_BORDER 4
#define DIP_NUB 16

unsigned int dip_tests(state_t *state, int reinit)
{
    // If we need to switch screens.
    unsigned int new_screen = SCREEN_DIP_TESTS;

    // Get our controls, in raw mode since we are testing filter board inputs.
    controls_t controls = get_controls(state, reinit, SEPARATE_CONTROLS);

    if ((controls.psw1 && controls.psw2) || controls.start_pressed || controls.test_pressed)
    {
        // Exit out of the dip switch test screen.
        new_screen = SCREEN_MAIN_MENU;
    }

    char *instructions[] = {
        "Press PSW1 and PSW2 simultaneously to exit.",
        "",
        "Alternatively, press either start or test to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    // Draw state of the current front panel switches.
    font_metrics_t metrics = font_get_text_metrics(state->font_18pt, "PSW2");
    ta_draw_text(64 + ((64 - metrics.width) / 2), 128, state->font_18pt, rgb(255, 255, 255), "PSW2");
    sprite_draw_simple(64, 160, controls.psw2 ? state->sprites.pswon : state->sprites.pswoff);

    metrics = font_get_text_metrics(state->font_18pt, "PSW1");
    ta_draw_text(192 + ((64 - metrics.width) / 2), 128, state->font_18pt, rgb(255, 255, 255), "PSW1");
    sprite_draw_simple(192, 160, controls.psw1 ? state->sprites.pswon : state->sprites.pswoff);

    // Draw state of the current front panel DIP switches.
    metrics = font_get_text_metrics(state->font_18pt, "DIPSW");
    ta_draw_text(320 + ((((4 * DIP_WIDTH) + (5 * DIP_SPACING) + (2 * DIP_BORDER)) - metrics.width) / 2), 128, state->font_18pt, rgb(255, 255, 255), "DIPSW");
    ta_draw_rectangle(320, 160, 320 + (4 * DIP_WIDTH) + (5 * DIP_SPACING) + (2 * DIP_BORDER), 160 + (2 * DIP_BORDER) + (2 * DIP_SPACING) + DIP_HEIGHT, rgb(0, 0, 128));
    ta_draw_rectangle(320 + DIP_BORDER, 160 + DIP_BORDER, 320 + (4 * DIP_WIDTH) + (5 * DIP_SPACING) + (DIP_BORDER), 160 + (DIP_BORDER) + (2 * DIP_SPACING) + DIP_HEIGHT, rgb(200, 200, 200));

    for (int i = 0; i < 4; i++)
    {
        int left = 320 + DIP_BORDER + DIP_SPACING + (i * (DIP_SPACING + DIP_WIDTH));
        int right = left + DIP_WIDTH;
        int top = 160 + DIP_BORDER + DIP_SPACING;
        int bottom = top + DIP_HEIGHT;

        ta_draw_rectangle(left, top, right, bottom, rgb(32, 32, 32));

        color_t color;
        if ((1 << i) & controls.dipswitches)
        {
            bottom = top + DIP_NUB;
            color = rgb(0, 0, 255);
        }
        else
        {
            top = bottom - DIP_NUB;
            color = rgb(0, 0, 128);
        }

        ta_draw_rectangle(left, top, right, bottom, color);
    }

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

unsigned int walking_0s(unsigned int startaddr, unsigned int size)
{
    // Check for stuck data bits.
    uint8_t patterns[8] = { 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

    for (unsigned int addr = startaddr; addr < startaddr + size; addr++)
    {
        volatile uint8_t *loc = (volatile uint8_t *)addr;

        for (int i = 0; i < 8; i++)
        {
            *loc = patterns[i];
            if ((*loc) != patterns[i])
            {
                return addr;
            }
        }
    }

    return 0;
}

unsigned int walking_1s(unsigned int startaddr, unsigned int size)
{
    // Check for stuck data bits.
    uint8_t patterns[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

    for (unsigned int addr = startaddr; addr < startaddr + size; addr++)
    {
        volatile uint8_t *loc = (volatile uint8_t *)addr;

        for (int i = 0; i < 8; i++)
        {
            *loc = patterns[i];
            if ((*loc) != patterns[i])
            {
                return addr;
            }
        }
    }

    return 0;
}

unsigned int device_test(unsigned int startaddr, unsigned int size)
{
    // Check to make sure something can be stored in each byte.
    uint8_t pattern = 5;
    for (unsigned int addr = startaddr; addr < startaddr + size; addr++)
    {
        volatile uint8_t *loc = (volatile uint8_t *)addr;
        *loc = pattern;
        pattern++;
    }

    pattern = 5;
    for (unsigned int addr = startaddr; addr < startaddr + size; addr++)
    {
        volatile uint8_t *loc = (volatile uint8_t *)addr;
        if (*loc != pattern)
        {
            return addr;
        }
        pattern++;
    }

    return 0;
}

unsigned int address_test(unsigned int startaddr, unsigned int size)
{
    // Check for address bits stuck low.
    for (unsigned int offset = 1; offset < size; offset <<= 1)
    {
        volatile uint8_t *loc = (volatile uint8_t *)(startaddr + offset);
        *loc = 0xAA;
    }

    // Set the low address to a sentinel, so we can walk up and set values
    // at each address line high to another and compare against this value.
    volatile uint8_t *lowloc = (volatile uint8_t *)startaddr;
    *lowloc = 0xAA;

    for (unsigned int offset = 1; offset < size; offset <<= 1)
    {
        volatile uint8_t *loc = (volatile uint8_t *)(startaddr + offset);
        *loc = 0x55;

        if (*lowloc != 0xAA)
        {
            return startaddr + offset;
        }
    }

    // Check for address bits stuck high.
    for (unsigned int offset = 1; offset < size; offset <<= 1)
    {
        volatile uint8_t *loc = (volatile uint8_t *)(startaddr + offset);
        *loc = 0xAA;
    }

    // Set the low address to a sentinel, so we can walk up and get values
    // at each address line high to another and compare against this value.
    *lowloc = 0x55;

    for (unsigned int offset = 1; offset < size; offset <<= 1)
    {
        volatile uint8_t *loc = (volatile uint8_t *)(startaddr + offset);
        if (*loc != 0xAA)
        {
            return startaddr + offset;
        }
    }

    return 0;
}

unsigned int sram_tests(state_t *state, int reinit)
{
    // The test we are currently running. This should really be a thread but
    // I don't think it's going to take long enough to test. Maybe if we ever
    // have a system RAM test this can be made threaded?
    static int whichtest = -1;

    // The result of each test. All F's means not run, All 0's means it passed
    // and any other value is the memory address it failed on.
    static unsigned int w1saddr = 0xFFFFFFFF;
    static unsigned int w0saddr = 0xFFFFFFFF;
    static unsigned int addraddr = 0xFFFFFFFF;
    static unsigned int dataaddr = 0xFFFFFFFF;

    // Re-initialize the test;
    if (reinit)
    {
        whichtest = -1;
        w1saddr = 0xFFFFFFFF;
        w0saddr = 0xFFFFFFFF;
        addraddr = 0xFFFFFFFF;
        dataaddr = 0xFFFFFFFF;
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_SRAM_TESTS;

    controls_t controls = get_controls(state, reinit, SEPARATE_CONTROLS);

    if (controls.test_pressed || controls.start_pressed)
    {
        // Exit out of the monitor test screen.
        new_screen = SCREEN_MAIN_MENU;
    }

    switch(whichtest)
    {
        case -1:
        {
            // Just display something on the screen.
            whichtest++;
            break;
        }
        case 0:
        {
            whichtest++;
            w1saddr = walking_1s(SRAM_BASE, SRAM_SIZE);
            break;
        }
        case 1:
        {
            whichtest++;
            w0saddr = walking_0s(SRAM_BASE, SRAM_SIZE);
            break;
        }
        case 2:
        {
            whichtest++;
            addraddr = address_test(SRAM_BASE, SRAM_SIZE);
            break;
        }
        case 3:
        {
            whichtest++;
            dataaddr = device_test(SRAM_BASE, SRAM_SIZE);
            break;
        }
    }

    // Display instructions.
    char *instructions[] = {
        "Press either start or test to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    unsigned int results[4] = {w1saddr, w0saddr, addraddr, dataaddr};
    char *titles[4] = {"Walking 1s", "Walking 0s", "Address Bus", "Device"};

    for (int i = 0; i < (sizeof(results) / sizeof(results[0])); i++)
    {
        ta_draw_text(64, 128 + (24 * i), state->font_18pt, rgb(255, 255, 255), "%s Test...", titles[i]);

        switch(results[i])
        {
            case 0x0:
            {
                ta_draw_text(300, 128 + (24 * i), state->font_18pt, rgb(0, 255, 0), "PASSED");
                break;
            }
            case 0xFFFFFFFF:
            {
                ta_draw_text(300, 128 + (24 * i), state->font_18pt, rgb(255, 255, 0), "RUNNING");
                break;
            }
            default:
            {
                ta_draw_text(300, 128 + (24 * i), state->font_18pt, rgb(255, 0, 0), "FAILED AT 0x%08X", results[i]);
                break;
            }
        }
    }

    return new_screen;
}

unsigned int system_menu(state_t *state, int reinit)
{
    enter_test_mode();
    return SCREEN_SYSTEM_MENU;
}

unsigned int reboot_system(state_t *state, int reinit)
{
    call_unmanaged((void (*)())0xA0000000);
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
