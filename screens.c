#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/audio.h>
#include <naomi/maple.h>
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
#define SCREEN_ANALOG_TESTS 7

// These aren't really screens, but its easiest if we just add the
// action functionality into screens themselves.
#define SCREEN_SYSTEM_MENU 1001
#define SCREEN_REBOOT_SYSTEM 1002

// The offset down onto each screen past where we might display instructions.
#define CONTENT_HOFFSET 48
#define CONTENT_VOFFSET 92

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
unsigned int analog_tests(state_t *state, int reinit);
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
        "JVS Digital Input Tests",
        SCREEN_INPUT_TESTS,
        input_tests,
    },
    {
        "JVS Analog Input Tests",
        SCREEN_ANALOG_TESTS,
        analog_tests,
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
        "System Test Menu",
        SCREEN_SYSTEM_MENU,
        system_menu,
    },
    {
        "Reboot Naomi",
        SCREEN_REBOOT_SYSTEM,
        reboot_system,
    }
};

pthread_t spawn_background_task(void *(*task)(void * param), void *param)
{
    pthread_t thread;
    pthread_create(&thread, NULL, task, param);
    return thread;
}

void despawn_background_task(pthread_t thread)
{
    pthread_cancel(thread);
    pthread_join(thread, NULL);
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
#define MONITOR_TEST_SCREENS 8

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
                "Use digital joystick left/right to move between pages.",
                "Press start button to exit back to main menu.",
                "",
                "Alternatively, use service to move between pages and test to exit.",
                "",
                "Page 1 is a pure white screen for white balance adjustments.",
                "Page 2-4 are pure red/green/blue for purity adjustments.",
                "Page 5 is a gradient for individual gain/bias adjustments.",
                "Page 6 is a white cross hatch for focus and green/magenta convergence adjustments.",
                "Page 7 is a magenta cross hatch for red/blue convergence adjustments.",
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

            sprite_draw_box(0, 0, video_width(), video_height(), colors[screen - 1]);
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
                    sprite_draw_box(left, top, right, bottom, actual);
                }
            }

            break;
        }
        case 6:
        case 7:
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
            color_t rgbcolor;

            if (screen == 6)
            {
                rgbcolor = rgb(255, 255, 255);
            }
            else if (screen == 7)
            {
                rgbcolor = rgb(255, 0, 255);
            }

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

                sprite_draw_box(left + bump, top, right + bump, bottom, rgbcolor);
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

                sprite_draw_box(left, top + bump, right, bottom + bump, rgbcolor);
            }

            break;
        }
    }

    return new_screen;
}

#define AUDIO_MAX_SCREENS 4

unsigned int audio_tests(state_t *state, int reinit)
{
    static int screen = 0;

    if (reinit)
    {
        // Start out with silence.
        screen = 0;
    }

    // If we need to switch screens.
    int new_screen = SCREEN_AUDIO_TESTS;

    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);

    int start_please = 0;
    if (controls.test_pressed || controls.start_pressed)
    {
        // Exit out of the audio test screen.
        audio_stop_registered_sound(state->sounds.scale);
        new_screen = SCREEN_MAIN_MENU;
    }
    else if (controls.right_pressed || controls.service_pressed)
    {
        // Play the next sound.
        audio_stop_registered_sound(state->sounds.scale);

        screen++;
        if (screen >= AUDIO_MAX_SCREENS) { screen = 0; }
        start_please = 1;
    }
    else if (controls.left_pressed)
    {
        // Play the previous sound.
        audio_stop_registered_sound(state->sounds.scale);

        screen--;
        if (screen < 0) { screen = (AUDIO_MAX_SCREENS - 1); }
        start_please = 1;
    }

    if (start_please)
    {
        switch(screen)
        {
            case 1:
            {
                audio_play_registered_sound(state->sounds.scale, SPEAKER_LEFT, 1.0);
                break;
            }
            case 2:
            {
                audio_play_registered_sound(state->sounds.scale, SPEAKER_RIGHT, 1.0);
                break;
            }
            case 3:
            {
                audio_play_registered_sound(state->sounds.scale, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);
                break;
            }
        }
    }

    // Instructions page.
    char *instructions[] = {
        "Use digital joystick left/right to start/stop sound.",
        "Press start button to exit back to main menu.",
        "",
        "Alternatively, use service to start/stop sound and test to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    switch(screen)
    {
        case 0:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "No sound playing.");
            break;
        }
        case 1:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "Left speaker only.");
            break;
        }
        case 2:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "Right speaker only.");
            break;
        }
        case 3:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "Both speakers.");
            break;
        }
    }

    return new_screen;
}

color_t char2rgb(char c)
{
    switch(c)
    {
        // Color pairs that should not be possible to hit
        // simultaneously.
        case 'U':
        case 'D':
            return rgb(255, 0, 0);
        case 'L':
        case 'R':
            return rgb(0, 255, 0);
        case 'S':
            return rgb(255, 255, 0);
        case '1':
            return rgb(0, 0, 255);
        case '2':
            return rgb(255, 0, 255);
        case '3':
            return rgb(0, 255, 255);
        case '4':
            return rgb(255, 127, 40);
        case '5':
            return rgb(255, 128, 192);
        case '6':
            return rgb(180, 255, 30);
    }

    return rgb(255, 255, 255);
}

void ta_draw_button(state_t *state, int x, int y, float scale, color_t color)
{
    // First, draw the backing graphic.
    int diameter = (int)(48.0 * scale);
    sprite_draw_box(x, y, x + diameter, y + diameter, color);

    // Now, draw the mask in front of it.
    sprite_draw_scaled(x, y, scale, scale, state->sprites.buttonmask);
}

#define MAX_HIST_POSITIONS 60

unsigned int input_tests(state_t *state, int reinit)
{
    // The position in the histogram.
    static unsigned int hist_pos = 0;

    // Each position represented as a character.
    static unsigned char hist_val[2][MAX_HIST_POSITIONS];

    if (reinit)
    {
        // Reset the histogram.
        memset(hist_val, '-', sizeof(hist_val[0][0]) * 2 * MAX_HIST_POSITIONS);
        hist_pos = 0;
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_INPUT_TESTS;

    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);
    if ((controls.test && (controls.joy1_svc || controls.joy2_svc)) || (controls.psw1 && controls.psw2))
    {
        // Exit out of the digital input test screen.
        new_screen = SCREEN_MAIN_MENU;
    }

    // Calculate what each histogram should be displaying.
    char controlvals[11] = {'U', 'D', 'L', 'R', 'S', '1', '2', '3', '4', '5', '6' };
    uint8_t heldcontrols[2][11] = {
        {
            controls.joy1_u,
            controls.joy1_d,
            controls.joy1_l,
            controls.joy1_r,
            controls.joy1_s,
            controls.joy1_1,
            controls.joy1_2,
            controls.joy1_3,
            controls.joy1_4,
            controls.joy1_5,
            controls.joy1_6
        },
        {
            controls.joy2_u,
            controls.joy2_d,
            controls.joy2_l,
            controls.joy2_r,
            controls.joy2_s,
            controls.joy2_1,
            controls.joy2_2,
            controls.joy2_3,
            controls.joy2_4,
            controls.joy2_5,
            controls.joy2_6
        },
    };

    for (int player = 0; player < 2; player++)
    {
        hist_val[player][hist_pos] = '-';
        for (int control = 0; control < (sizeof(controlvals) / sizeof(controlvals[0])); control++)
        {
            if (heldcontrols[player][control])
            {
                hist_val[player][hist_pos] = controlvals[control];
                break;
            }
        }
    }

    char *instructions[] = {
        "Press test and service simultaneously to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    // Move the 2P below the 1P histogram if it is vertical.
    int vstride = 0;
    int hstride = 300;
    if (video_is_vertical())
    {
        vstride = 300;
        hstride = 0;
    }

    // Display the control panel.
    for (int player = 0; player < 2; player++)
    {
        // Draw joystick as a crude D-pad.
        ta_draw_button(state, CONTENT_HOFFSET + (hstride * player), CONTENT_VOFFSET + 24 + (vstride * player), 0.5, heldcontrols[player][2] ? char2rgb('L') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 48 + (hstride * player), CONTENT_VOFFSET + 24 + (vstride * player), 0.5, heldcontrols[player][3] ? char2rgb('R') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 24 + (hstride * player), CONTENT_VOFFSET + (vstride * player), 0.5, heldcontrols[player][0] ? char2rgb('U') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 24 + (hstride * player), CONTENT_VOFFSET + 48 + (vstride * player), 0.5, heldcontrols[player][1] ? char2rgb('D') : rgb(128, 128, 128));

        // Draw buttons.
        ta_draw_button(state, CONTENT_HOFFSET + 90 + (hstride * player), CONTENT_VOFFSET + 18 + (vstride * player), 0.5, heldcontrols[player][5] ? char2rgb('1') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 118 + (hstride * player), CONTENT_VOFFSET + 10 + (vstride * player), 0.5, heldcontrols[player][6] ? char2rgb('2') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 146 + (hstride * player), CONTENT_VOFFSET + 10 + (vstride * player), 0.5, heldcontrols[player][7] ? char2rgb('3') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 90 + (hstride * player), CONTENT_VOFFSET + 48 + (vstride * player), 0.5, heldcontrols[player][8] ? char2rgb('4') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 118 + (hstride * player), CONTENT_VOFFSET + 40 + (vstride * player), 0.5, heldcontrols[player][9] ? char2rgb('5') : rgb(128, 128, 128));
        ta_draw_button(state, CONTENT_HOFFSET + 146 + (hstride * player), CONTENT_VOFFSET + 40 + (vstride * player), 0.5, heldcontrols[player][10] ? char2rgb('6') : rgb(128, 128, 128));
    }

    // Display the start buttons separately, since they go in "the middle".
    ta_draw_button(state, CONTENT_HOFFSET + 210, CONTENT_VOFFSET, 0.4, heldcontrols[0][4] ? char2rgb('S') : rgb(128, 128, 128));
    ta_draw_button(state, CONTENT_HOFFSET + 210 + 30, CONTENT_VOFFSET, 0.4, heldcontrols[1][4] ? char2rgb('S') : rgb(128, 128, 128));

    // Display test/service switches special case.
    int tleft = CONTENT_HOFFSET + 190;
    int ttop = CONTENT_VOFFSET + 96;
    sprite_draw_box(tleft, ttop, tleft + 24, ttop + 24, controls.test ? rgb(255, 255, 255) : rgb(128, 128, 128));
    font_metrics_t metrics = font_get_text_metrics(state->font_12pt, "test");
    ta_draw_text(tleft + (24 - (int)metrics.width) / 2, ttop + 26, state->font_12pt, rgb(255, 255, 255), "test");

    tleft += 32;
    sprite_draw_box(tleft, ttop, tleft + 24, ttop + 24, controls.joy1_svc ? rgb(255, 255, 255) : rgb(128, 128, 128));
    metrics = font_get_text_metrics(state->font_12pt, "svc1");
    ta_draw_text(tleft + (24 - (int)metrics.width) / 2, ttop + 26, state->font_12pt, rgb(255, 255, 255), "svc1");

    tleft += 32;
    sprite_draw_box(tleft, ttop, tleft + 24, ttop + 24, controls.joy2_svc ? rgb(255, 255, 255) : rgb(128, 128, 128));
    metrics = font_get_text_metrics(state->font_12pt, "svc2");
    ta_draw_text(tleft + (24 - (int)metrics.width) / 2, ttop + 26, state->font_12pt, rgb(255, 255, 255), "svc2");

    // Now, display the histogram.
    int hist_top = CONTENT_VOFFSET + 160;
    int hist_left = CONTENT_HOFFSET;

    // Its pretty difficult to fit this screen on a vertical setup, so the
    // visuals will have to suffer a bit.
    int stride = video_is_vertical() ? 7 : 8;
    int bump = video_is_vertical() ? 256 : 64;

    for (int player = 0; player < 2; player++)
    {
        // Draw which player this is for.
        ta_draw_text(hist_left, hist_top + (player * bump), state->font_18pt, rgb(255, 255, 255), "Player %d History", player + 1);

        for (int i = 0; i < MAX_HIST_POSITIONS; i++)
        {
            int left = hist_left + (stride * i);
            int top = hist_top + (player * bump) + 30;
            int right = left + stride;
            int bottom = top + 8;

            // First, if this is where the current histogram position is, display a box.
            if (i == hist_pos)
            {
                sprite_draw_box(left, top - 2, right, bottom + 10, rgb(96, 0, 0));
            }

            // Now, draw the character.
            ta_draw_text(left, top, state->font_mono, char2rgb(hist_val[player][i]), "%c", hist_val[player][i]);
        }
    }

    // Move to the next slot.
    hist_pos ++;
    if (hist_pos >= MAX_HIST_POSITIONS) { hist_pos = 0; }

    return new_screen;
}

#define ANALOG_MAX_SCREENS 2

unsigned int analog_tests(state_t *state, int reinit)
{
    // List of ranges, indexed by player, then by control, then by min/max.
    static uint8_t ranges[2][4][2];
    static int screen = 0;

    // Analog input tests. Show current, track full range for each control.
    if (reinit)
    {
        // Make sure the ranges are such that any input will change it on first pass.
        for (int player = 0; player < 2; player++)
        {
            for (int control = 0; control < 4; control++)
            {
                ranges[player][control][0] = 0xFF;
                ranges[player][control][1] = 0x00;
            }
        }

        // Start on the joystick screen.
        screen = 0;
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_ANALOG_TESTS;

    // Grab the current values for each.
    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);
    uint8_t values[2][4] = {
        { controls.joy1_v, controls.joy1_h, controls.joy1_a3, controls.joy1_a4 },
        { controls.joy2_v, controls.joy2_h, controls.joy2_a3, controls.joy2_a4 },
    };

    for (int player = 0; player < 2; player++)
    {
        for (int control = 0; control < 4; control++)
        {
            if (values[player][control] < ranges[player][control][0])
            {
                ranges[player][control][0] = values[player][control];
            }
            if (values[player][control] > ranges[player][control][1])
            {
                ranges[player][control][1] = values[player][control];
            }
        }
    }

    if (controls.test_pressed || controls.start_pressed)
    {
        // Exit out of the analog test screen.
        new_screen = SCREEN_MAIN_MENU;
    }
    else if (controls.right_pressed || controls.service_pressed)
    {
        audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);

        screen ++;
        if (screen >= ANALOG_MAX_SCREENS) { screen = 0; }
    }
    else if (controls.left_pressed)
    {
        audio_play_registered_sound(state->sounds.scroll, SPEAKER_LEFT | SPEAKER_RIGHT, 1.0);

        screen --;
        if (screen < 0) { screen = (ANALOG_MAX_SCREENS - 1); }
    }

    // Display instructions.
    char *instructions[] = {
        "Use digital joystick left/right or service to change screen.",
        "",
        "Press either start or test to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    switch (screen)
    {
        case 0:
        {
            // Joystic view, displaying only current values for X/Y.
            // Draw 1P and 2P joysticks as a box representation.
            int joy1left = CONTENT_HOFFSET;
            int joy2left = joy1left + 270;
            int joy1top = CONTENT_VOFFSET + 24;
            int joy2top = joy1top;

            if (video_is_vertical())
            {
                joy2left = joy1left;
                joy1top -= 24;
                joy2top = joy1top + 270;

                // Draw labels.
                ta_draw_text(joy1left + 260, joy1top, state->font_18pt, rgb(255, 255, 255), "1P Joystick");
                ta_draw_text(joy2left + 260, joy2top, state->font_18pt, rgb(255, 255, 255), "1P Joystick");
            }
            else
            {
                // Draw labels.
                font_metrics_t metrics = font_get_text_metrics(state->font_18pt, "1P Joystick");
                ta_draw_text(joy1left + (257 - metrics.width) / 2, joy1top - 24, state->font_18pt, rgb(255, 255, 255), "1P Joystick");
                metrics = font_get_text_metrics(state->font_18pt, "2P Joystick");
                ta_draw_text(joy2left + (257 - metrics.width) / 2, joy2top - 24, state->font_18pt, rgb(255, 255, 255), "2P Joystick");
            }

            // First draw the outline and inner motion section.
            sprite_draw_box(joy1left, joy1top, joy1left + 255 + 2, joy1top + 255 + 2, rgb(255, 255, 255));
            sprite_draw_box(joy2left, joy2top, joy2left + 255 + 2, joy2top + 255 + 2, rgb(255, 255, 255));
            sprite_draw_box(joy1left + 1, joy1top + 1, joy1left + 255 + 1, joy1top + 255 + 1, rgb(64, 64, 64));
            sprite_draw_box(joy2left + 1, joy2top + 1, joy2left + 255 + 1, joy2top + 255 + 1, rgb(64, 64, 64));

            // Now draw an outline for the min/max of each axis.
            sprite_draw_box(
                joy1left + 1 + ranges[0][1][0],
                joy1top + 1 + ranges[0][0][0],
                joy1left + 1 + ranges[0][1][1],
                joy1top + 1 + ranges[0][0][1],
                rgb(64, 192, 64)
            );
            sprite_draw_box(
                joy2left + 1 + ranges[1][1][0],
                joy2top + 1 + ranges[1][0][0],
                joy2left + 1 + ranges[1][1][1],
                joy2top + 1 + ranges[1][0][1],
                rgb(64, 192, 64)
            );

            // Now draw a square for the current location of the joystick.
            sprite_draw_box(
                joy1left + 1 + values[0][1] - 15,
                joy1top + 1 + values[0][0] - 15,
                joy1left + 1 + values[0][1] + 15,
                joy1top + 1 + values[0][0] + 15,
                rgb(255, 255, 255)
            );
            sprite_draw_box(
                joy2left + 1 + values[1][1] - 15,
                joy2top + 1 + values[1][0] - 15,
                joy2left + 1 + values[1][1] + 15,
                joy2top + 1 + values[1][0] + 15,
                rgb(255, 255, 255)
            );

            if (video_is_vertical())
            {
                // Draw current values.
                ta_draw_text(joy1left + 260, joy1top + 24, state->font_18pt, rgb(255, 255, 255), "H: %02X, V: %02X", values[0][1], values[0][0]);
                ta_draw_text(joy2left + 260, joy2top + 24, state->font_18pt, rgb(255, 255, 255), "H: %02X, V: %02X", values[1][1], values[1][0]);
            }
            else
            {
                // Draw current values.
                font_metrics_t metrics = font_get_text_metrics(state->font_18pt, "H: %02X, V: %02X", values[0][1], values[0][0]);
                ta_draw_text(joy1left + (257 - metrics.width) / 2, joy1top + 260, state->font_18pt, rgb(255, 255, 255), "H: %02X, V: %02X", values[0][1], values[0][0]);
                metrics = font_get_text_metrics(state->font_18pt, "H: %02X, V: %02X", values[1][1], values[1][0]);
                ta_draw_text(joy2left + (257 - metrics.width) / 2, joy2top + 260, state->font_18pt, rgb(255, 255, 255), "H: %02X, V: %02X", values[1][1], values[1][0]);
            }

            break;
        }
        case 1:
        {
            // Analog range view, for pedals/steering/etc.
            // Draw 1P and 2P joysticks as a box representation.
            // Unfortunately this doesn't fit on a vertical screen even with
            // rearranging controls, so we have two draw paths.
            if (video_is_vertical())
            {
                int joyleft[2] = { CONTENT_HOFFSET };
                joyleft[1] = joyleft[0];
                int joytop[2] = { CONTENT_VOFFSET + 24 };
                joytop[1] = joytop[0] + 270;

                for (int player = 0; player < 2; player++)
                {
                    // Draw labels.
                    font_metrics_t metrics = font_get_text_metrics(state->font_18pt, "%dP Analog", player + 1);
                    ta_draw_text(joyleft[player] + (257 - metrics.width) / 2, joytop[player] - 24, state->font_18pt, rgb(255, 255, 255), "%dP Analog", player + 1);

                    for (int control = 0; control < 4; control++)
                    {
                        int left = joyleft[player];
                        int right = left + 257;
                        int top = joytop[player] + (58 * control);
                        int bottom = top + 50;

                        // First draw the control itself.
                        sprite_draw_box(left, top, right, bottom, rgb(255, 255, 255));
                        sprite_draw_box(left + 1, top + 1, right - 1, bottom - 1, rgb(64, 64, 64));

                        // Now, draw the outline of min/max range.
                        sprite_draw_box(
                            left + 1 + ranges[player][control][0],
                            top + 1,
                            left + 1 + ranges[player][control][1],
                            bottom - 1,
                            rgb(64, 192, 64)
                        );

                        // Now, draw a slider displaying where the control is.
                        sprite_draw_box(
                            left + values[player][control],
                            top + 1,
                            left + 2 + values[player][control],
                            bottom - 1,
                            rgb(255, 255, 255)
                        );

                        // Draw current value.
                        metrics = font_get_text_metrics(state->font_18pt, "%02X", values[player][control]);
                        ta_draw_text(right + 2, (top + bottom - metrics.height) / 2, state->font_18pt, rgb(255, 255, 255), "%02X", values[player][control]);
                    }
                }
            }
            else
            {
                int joyleft[2] = { CONTENT_HOFFSET };
                joyleft[1] = joyleft[0] + 270;
                int joytop[2] = { CONTENT_VOFFSET + 24 };
                joytop[1] = joytop[0];

                for (int player = 0; player < 2; player++)
                {
                    // Draw labels.
                    font_metrics_t metrics = font_get_text_metrics(state->font_18pt, "%dP Analog", player + 1);
                    ta_draw_text(joyleft[player] + (257 - metrics.width) / 2, joytop[player] - 24, state->font_18pt, rgb(255, 255, 255), "%dP Analog", player + 1);

                    for (int control = 0; control < 4; control++)
                    {
                        int left = joyleft[player] + (64 * control);
                        int right = left + 56;
                        int top = joytop[player];
                        int bottom = top + 257;

                        // First draw the control itself.
                        sprite_draw_box(left, top, right, bottom, rgb(255, 255, 255));
                        sprite_draw_box(left + 1, top + 1, right - 1, bottom - 1, rgb(64, 64, 64));

                        // Now, draw the outline of min/max range.
                        sprite_draw_box(
                            left + 1,
                            top + 1 + ranges[player][control][0],
                            right - 1,
                            top + 1 + ranges[player][control][1],
                            rgb(64, 192, 64)
                        );

                        // Now, draw a slider displaying where the control is.
                        sprite_draw_box(
                            left + 1,
                            top + values[player][control],
                            right - 1,
                            top + 2 + values[player][control],
                            rgb(255, 255, 255)
                        );

                        // Draw current value.
                        metrics = font_get_text_metrics(state->font_18pt, "%02X", values[player][control]);
                        ta_draw_text((left + right - metrics.width) / 2, bottom + 2, state->font_18pt, rgb(255, 255, 255), "%02X", values[player][control]);
                    }
                }
            }

            break;
        }
    }

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
    ta_draw_text(CONTENT_HOFFSET + ((64 - metrics.width) / 2), CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "PSW2");
    sprite_draw_simple(CONTENT_HOFFSET, CONTENT_VOFFSET + 32, controls.psw2 ? state->sprites.pswon : state->sprites.pswoff);

    metrics = font_get_text_metrics(state->font_18pt, "PSW1");
    ta_draw_text(CONTENT_HOFFSET + 128 + ((64 - metrics.width) / 2), CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "PSW1");
    sprite_draw_simple(CONTENT_HOFFSET + 128, CONTENT_VOFFSET + 32, controls.psw1 ? state->sprites.pswon : state->sprites.pswoff);

    // Draw state of the current front panel DIP switches.
    metrics = font_get_text_metrics(state->font_18pt, "DIPSW");
    ta_draw_text(
        CONTENT_HOFFSET + 256 + ((((4 * DIP_WIDTH) + (5 * DIP_SPACING) + (2 * DIP_BORDER)) - metrics.width) / 2),
        CONTENT_VOFFSET,
        state->font_18pt,
        rgb(255, 255, 255),
        "DIPSW"
    );
    sprite_draw_box(
        CONTENT_HOFFSET + 256,
        CONTENT_VOFFSET + 32,
        CONTENT_HOFFSET + 256 + (4 * DIP_WIDTH) + (5 * DIP_SPACING) + (2 * DIP_BORDER),
        CONTENT_VOFFSET + 32 + (2 * DIP_BORDER) + (2 * DIP_SPACING) + DIP_HEIGHT,
        rgb(0, 0, 128)
    );
    sprite_draw_box(
        CONTENT_HOFFSET + 256 + DIP_BORDER,
        CONTENT_VOFFSET + 32 + DIP_BORDER,
        CONTENT_HOFFSET + 256 + (4 * DIP_WIDTH) + (5 * DIP_SPACING) + (DIP_BORDER),
        CONTENT_VOFFSET + 32 + (DIP_BORDER) + (2 * DIP_SPACING) + DIP_HEIGHT,
        rgb(200, 200, 200)
    );

    for (int i = 0; i < 4; i++)
    {
        int left = CONTENT_HOFFSET + 256 + DIP_BORDER + DIP_SPACING + (i * (DIP_SPACING + DIP_WIDTH));
        int right = left + DIP_WIDTH;
        int top = CONTENT_VOFFSET + 32 + DIP_BORDER + DIP_SPACING;
        int bottom = top + DIP_HEIGHT;

        sprite_draw_box(left, top, right, bottom, rgb(32, 32, 32));

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

        sprite_draw_box(left, top, right, bottom, color);
    }

    return new_screen;
}

typedef struct
{
    unsigned int state;
    unsigned int exit;
    state_t *sysstate;

    pthread_t thread;
    pthread_mutex_t mutex;
} eeprom_test_t;

#define EEPROM_TEST_STATE_INITIAL_READ 0
#define EEPROM_TEST_STATE_INITIAL_WRITEBACK 1
#define EEPROM_TEST_STATE_SECOND_READ 2
#define EEPROM_TEST_STATE_SECOND_WRITEBACK 3
#define EEPROM_TEST_STATE_FINISHED 4

#define EEPROM_TEST_STATE_FAILED_INITIAL_READ 1000
#define EEPROM_TEST_STATE_FAILED_INITIAL_WRITEBACK 1001
#define EEPROM_TEST_STATE_FAILED_SECOND_READ 1002
#define EEPROM_TEST_STATE_FAILED_SECOND_WRITEBACK 1003

void *eeprom_test_thread(void *param)
{
    eeprom_test_t *eeprom_test = (eeprom_test_t *)param;

    // We manually interleave control checks here since we cannot
    // have an outstanding EEPROM read/write request and also try
    // to read controls. The maple bus handles both and cannot do
    // simultaneous outstanding requests.
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&eeprom_test->mutex);
    controls_t controls = get_controls(eeprom_test->sysstate, 1, COMBINED_CONTROLS);
    if (controls.test_pressed || controls.start_pressed)
    {
        eeprom_test->exit = 1;
    }
    pthread_mutex_unlock(&eeprom_test->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    // First, try to read, bail out of it fails.
    uint8_t eeprom[128];
    if(maple_request_eeprom_read(eeprom) != 0)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&eeprom_test->mutex);
        eeprom_test->state = EEPROM_TEST_STATE_FAILED_INITIAL_READ;
        controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
        if (controls.test_pressed || controls.start_pressed)
        {
            eeprom_test->exit = 1;
        }
        pthread_mutex_unlock(&eeprom_test->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return NULL;
    }

    // Now, invert the whole thing and write it back.
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&eeprom_test->mutex);
    eeprom_test->state = EEPROM_TEST_STATE_INITIAL_WRITEBACK;
    controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
    if (controls.test_pressed || controls.start_pressed)
    {
        eeprom_test->exit = 1;
    }
    pthread_mutex_unlock(&eeprom_test->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    uint8_t inveeprom[128];
    for (int i = 0; i < 128; i++)
    {
        inveeprom[i] = ~eeprom[i];
    }

    if(maple_request_eeprom_write(inveeprom) != 0)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&eeprom_test->mutex);
        eeprom_test->state = EEPROM_TEST_STATE_FAILED_INITIAL_WRITEBACK;
        controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
        if (controls.test_pressed || controls.start_pressed)
        {
            eeprom_test->exit = 1;
        }
        pthread_mutex_unlock(&eeprom_test->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return NULL;
    }

    // Now, try to read back that just written eeprom.
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&eeprom_test->mutex);
    eeprom_test->state = EEPROM_TEST_STATE_SECOND_READ;
    controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
    if (controls.test_pressed || controls.start_pressed)
    {
        eeprom_test->exit = 1;
    }
    pthread_mutex_unlock(&eeprom_test->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    uint8_t neweeprom[128];
    if(maple_request_eeprom_read(neweeprom) != 0 || memcmp(inveeprom, neweeprom, 128) != 0)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&eeprom_test->mutex);
        eeprom_test->state = EEPROM_TEST_STATE_FAILED_SECOND_READ;
        controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
        if (controls.test_pressed || controls.start_pressed)
        {
            eeprom_test->exit = 1;
        }
        pthread_mutex_unlock(&eeprom_test->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return NULL;
    }

    // Now, try to write back the inverse of the inverse.
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&eeprom_test->mutex);
    eeprom_test->state = EEPROM_TEST_STATE_SECOND_WRITEBACK;
    controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
    if (controls.test_pressed || controls.start_pressed)
    {
        eeprom_test->exit = 1;
    }
    pthread_mutex_unlock(&eeprom_test->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    for (int i = 0; i < 128; i++)
    {
        inveeprom[i] = ~inveeprom[i];
    }

    if(maple_request_eeprom_write(inveeprom) != 0)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&eeprom_test->mutex);
        eeprom_test->state = EEPROM_TEST_STATE_FAILED_SECOND_WRITEBACK;
        controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
        if (controls.test_pressed || controls.start_pressed)
        {
            eeprom_test->exit = 1;
        }
        pthread_mutex_unlock(&eeprom_test->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return NULL;
    }

    // Include a second verify (even though we know we read properly) just for kicks.
    if(maple_request_eeprom_read(neweeprom) != 0 || memcmp(eeprom, neweeprom, 128) != 0)
    {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&eeprom_test->mutex);
        eeprom_test->state = EEPROM_TEST_STATE_FINISHED;
        controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
        if (controls.test_pressed || controls.start_pressed)
        {
            eeprom_test->exit = 1;
        }
        pthread_mutex_unlock(&eeprom_test->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return NULL;
    };

    // We passed!
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&eeprom_test->mutex);
    eeprom_test->state = EEPROM_TEST_STATE_FINISHED;
    controls = get_controls(eeprom_test->sysstate, 0, COMBINED_CONTROLS);
    if (controls.test_pressed || controls.start_pressed)
    {
        eeprom_test->exit = 1;
    }
    pthread_mutex_unlock(&eeprom_test->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return NULL;
}

eeprom_test_t *start_eeprom_test(state_t *state)
{
    eeprom_test_t *eeprom_test = malloc(sizeof(eeprom_test_t));
    eeprom_test->state = EEPROM_TEST_STATE_INITIAL_READ;
    eeprom_test->exit = 0;
    eeprom_test->sysstate = state;
    pthread_mutex_init(&eeprom_test->mutex, NULL);
    eeprom_test->thread = spawn_background_task(eeprom_test_thread, eeprom_test);
    return eeprom_test;
}

void end_eeprom_test(eeprom_test_t *eeprom_test)
{
    despawn_background_task(eeprom_test->thread);
    pthread_mutex_destroy(&eeprom_test->mutex);
    free(eeprom_test);
}

unsigned int eeprom_tests(state_t *state, int reinit)
{
    // The test we are currently running.
    static eeprom_test_t *test = NULL;

    // Re-initialize the test;
    if (reinit)
    {
        if (test != NULL)
        {
            end_eeprom_test(test);
            test = 0;
        }

        test = start_eeprom_test(state);
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_EEPROM_TESTS;

    // Display instructions.
    char *instructions[] = {
        "Press either start or test to exit.",
    };

    for (int i = 0; i < sizeof(instructions) / sizeof(instructions[0]); i++)
    {
        font_metrics_t metrics = font_get_text_metrics(state->font_12pt, instructions[i]);
        ta_draw_text((video_width() - metrics.width) / 2, 22 + (14 * i), state->font_12pt, rgb(255, 255, 255), instructions[i]);
    }

    pthread_mutex_lock(&test->mutex);
    unsigned int eepromstate = test->state;
    unsigned int exitstate = test->exit;
    pthread_mutex_unlock(&test->mutex);

    if (
        eepromstate == EEPROM_TEST_STATE_FINISHED ||
        eepromstate == EEPROM_TEST_STATE_FAILED_SECOND_WRITEBACK ||
        eepromstate == EEPROM_TEST_STATE_FAILED_SECOND_READ ||
        eepromstate == EEPROM_TEST_STATE_FAILED_INITIAL_WRITEBACK ||
        eepromstate == EEPROM_TEST_STATE_FAILED_INITIAL_READ
    ) {
        controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);

        if (controls.test_pressed || controls.start_pressed)
        {
            // Exit out of the EEPROM test screen.
            new_screen = SCREEN_MAIN_MENU;
        }
    }
    else if (exitstate)
    {
        // Exit out of the EEPROM test screen.
        new_screen = SCREEN_MAIN_MENU;
    }

    switch(eepromstate)
    {
        case EEPROM_TEST_STATE_FINISHED:
        {
            /* This is just a fallthrough to the next chunk of states. */
        }
        case EEPROM_TEST_STATE_SECOND_WRITEBACK:
        case EEPROM_TEST_STATE_FAILED_SECOND_WRITEBACK:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET + 72, state->font_18pt, rgb(255, 255, 255), "Performing final writeback...");
            if (eepromstate == EEPROM_TEST_STATE_FAILED_SECOND_WRITEBACK)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 72, state->font_18pt, rgb(255, 0, 0), "FAILED");
            }
            else if (eepromstate != EEPROM_TEST_STATE_SECOND_WRITEBACK)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 72, state->font_18pt, rgb(0, 255, 0), "PASSED");
            }

            // Fallthrough to display the previous state.
        }
        case EEPROM_TEST_STATE_SECOND_READ:
        case EEPROM_TEST_STATE_FAILED_SECOND_READ:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET + 48, state->font_18pt, rgb(255, 255, 255), "Performing second read...");
            if (eepromstate == EEPROM_TEST_STATE_FAILED_SECOND_READ)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 48, state->font_18pt, rgb(255, 0, 0), "FAILED");
            }
            else if (eepromstate != EEPROM_TEST_STATE_SECOND_READ)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 48, state->font_18pt, rgb(0, 255, 0), "PASSED");
            }

            // Fallthrough to display the previous state.
        }
        case EEPROM_TEST_STATE_INITIAL_WRITEBACK:
        case EEPROM_TEST_STATE_FAILED_INITIAL_WRITEBACK:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET + 24, state->font_18pt, rgb(255, 255, 255), "Performing inverted writeback...");
            if (eepromstate == EEPROM_TEST_STATE_FAILED_INITIAL_WRITEBACK)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 24, state->font_18pt, rgb(255, 0, 0), "FAILED");
            }
            else if (eepromstate != EEPROM_TEST_STATE_INITIAL_WRITEBACK)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET + 24, state->font_18pt, rgb(0, 255, 0), "PASSED");
            }

            // Fallthrough to display the previous state.
        }
        case EEPROM_TEST_STATE_INITIAL_READ:
        case EEPROM_TEST_STATE_FAILED_INITIAL_READ:
        {
            ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET, state->font_18pt, rgb(255, 255, 255), "Performing initial read...");
            if (eepromstate == EEPROM_TEST_STATE_FAILED_INITIAL_READ)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET, state->font_18pt, rgb(255, 0, 0), "FAILED");
            }
            else if (eepromstate != EEPROM_TEST_STATE_INITIAL_READ)
            {
                ta_draw_text(CONTENT_HOFFSET + 315, CONTENT_VOFFSET, state->font_18pt, rgb(0, 255, 0), "PASSED");
            }

            // No more states to display.
            break;
        }
    }

    if (new_screen != SCREEN_EEPROM_TESTS)
    {
        end_eeprom_test(test);
        test = 0;
    }

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

typedef struct
{
    unsigned int startaddr;
    unsigned int size;

    pthread_t thread;
    pthread_mutex_t mutex;

    unsigned int w1saddr;
    unsigned int w0saddr;
    unsigned int addraddr;
    unsigned int dataaddr;
} memory_test_t;

void *memtest_thread(void *param)
{
    memory_test_t *memtest = (memory_test_t *)param;
    unsigned int result;

    /* First, grab our range. */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&memtest->mutex);
    unsigned int startaddr = memtest->startaddr;
    unsigned int size = memtest->size;
    pthread_mutex_unlock(&memtest->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    /* Now, run the four memory tests. */
    result = walking_1s(startaddr, size);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&memtest->mutex);
    memtest->w1saddr = result;
    pthread_mutex_unlock(&memtest->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    result = walking_0s(startaddr, size);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&memtest->mutex);
    memtest->w0saddr = result;
    pthread_mutex_unlock(&memtest->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    result = address_test(startaddr, size);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&memtest->mutex);
    memtest->addraddr = result;
    pthread_mutex_unlock(&memtest->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    result = device_test(startaddr, size);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&memtest->mutex);
    memtest->dataaddr = result;
    pthread_mutex_unlock(&memtest->mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return NULL;
}

memory_test_t *start_memory_test(unsigned int startaddr, unsigned int size)
{
    memory_test_t *memtest = malloc(sizeof(memory_test_t));
    memtest->startaddr = startaddr;
    memtest->size = size;
    memtest->w1saddr = 0xFFFFFFFF;
    memtest->w0saddr = 0xFFFFFFFF;
    memtest->addraddr = 0xFFFFFFFF;
    memtest->dataaddr = 0xFFFFFFFF;
    pthread_mutex_init(&memtest->mutex, NULL);
    memtest->thread = spawn_background_task(memtest_thread, memtest);
    return memtest;
}

void end_memory_test(memory_test_t *memtest)
{
    despawn_background_task(memtest->thread);
    pthread_mutex_destroy(&memtest->mutex);
    free(memtest);
}

unsigned int sram_tests(state_t *state, int reinit)
{
    // The test we are currently running.
    static memory_test_t *test = NULL;

    // Re-initialize the test;
    if (reinit)
    {
        if (test != NULL)
        {
            end_memory_test(test);
        }

        test = start_memory_test(SRAM_BASE, SRAM_SIZE);
    }

    // If we need to switch screens.
    unsigned int new_screen = SCREEN_SRAM_TESTS;

    controls_t controls = get_controls(state, reinit, COMBINED_CONTROLS);

    if (controls.test_pressed || controls.start_pressed)
    {
        // Exit out of the SRAM test screen.
        new_screen = SCREEN_MAIN_MENU;
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

    pthread_mutex_lock(&test->mutex);
    unsigned int results[4] = {test->w1saddr, test->w0saddr, test->addraddr, test->dataaddr};
    char *titles[4] = {"Walking 1s", "Walking 0s", "Address Bus", "Device"};
    pthread_mutex_unlock(&test->mutex);

    for (int i = 0; i < (sizeof(results) / sizeof(results[0])); i++)
    {
        ta_draw_text(CONTENT_HOFFSET, CONTENT_VOFFSET + (24 * i), state->font_18pt, rgb(255, 255, 255), "%s Test...", titles[i]);

        switch(results[i])
        {
            case 0x0:
            {
                ta_draw_text(CONTENT_HOFFSET + 240, CONTENT_VOFFSET + (24 * i), state->font_18pt, rgb(0, 255, 0), "PASSED");
                break;
            }
            case 0xFFFFFFFF:
            {
                ta_draw_text(CONTENT_HOFFSET + 240, CONTENT_VOFFSET + (24 * i), state->font_18pt, rgb(255, 255, 0), "RUNNING");
                break;
            }
            default:
            {
                ta_draw_text(CONTENT_HOFFSET + 240, CONTENT_VOFFSET + (24 * i), state->font_18pt, rgb(255, 0, 0), "FAILED AT 0x%08X", results[i]);
                break;
            }
        }
    }

    if (new_screen != SCREEN_SRAM_TESTS)
    {
        end_memory_test(test);
        test = 0;
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
    // This doesn't seem to work on Demul, but it works on real hardware.
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
