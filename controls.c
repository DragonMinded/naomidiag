#include <string.h>
#include <naomi/timer.h>
#include <naomi/maple.h>
#include "common.h"
#include "state.h"
#include "controls.h"

#define REPEAT_INITIAL_DELAY 500000
#define REPEAT_SUBSEQUENT_DELAY 50000

unsigned int repeat(unsigned int cur_state, int *repeat_count)
{
    // A held button will "repeat" itself 20x a second
    // after a 0.5 second hold delay.
    if (*repeat_count < 0)
    {
        // If we have never pushed this button, don't try repeating
        // if it happened to be held.
        return 0;
    }

    if (cur_state == 0)
    {
        // Button isn't held, no repeats.
        timer_stop(*repeat_count);
        *repeat_count = -1;
        return 0;
    }

    if (timer_left(*repeat_count) == 0)
    {
        // We should restart this timer with a shorter delay
        // because we're in a repeat zone.
        timer_stop(*repeat_count);
        *repeat_count = timer_start(REPEAT_SUBSEQUENT_DELAY);
        return 1;
    }

    // Not currently being repeated.
    return 0;
}

void repeat_init(unsigned int pushed_state, int *repeat_count)
{
    if (pushed_state == 0)
    {
        // Haven't pushed the button yet.
        return;
    }

    // Clear out old timer if needed.
    if (*repeat_count >= 0)
    {
        timer_stop(*repeat_count);
    }

    // Set up a half-second timer for our first repeat.
    *repeat_count = timer_start(REPEAT_INITIAL_DELAY);
}

#define ANALOG_DEAD_ZONE 8

controls_t get_controls(state_t *state, int reinit, int full_separate)
{
    static unsigned int oldaup[2] = { 0 };
    static unsigned int oldadown[2] = { 0 };
    static unsigned int aup[2] = { 0 };
    static unsigned int adown[2] = { 0 };
    static unsigned int oldaleft[2] = { 0 };
    static unsigned int oldaright[2] = { 0 };
    static unsigned int aleft[2] = { 0 };
    static unsigned int aright[2] = { 0 };
    static int repeats[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

    if (reinit)
    {
        memset(oldaup, 0, sizeof(unsigned int) * 2);
        memset(aup, 0, sizeof(unsigned int) * 2);
        memset(oldadown, 0, sizeof(unsigned int) * 2);
        memset(adown, 0, sizeof(unsigned int) * 2);
        memset(oldaleft, 0, sizeof(unsigned int) * 2);
        memset(aleft, 0, sizeof(unsigned int) * 2);
        memset(oldaright, 0, sizeof(unsigned int) * 2);
        memset(aright, 0, sizeof(unsigned int) * 2);

        for (unsigned int i = 0; i < sizeof(repeats) / sizeof(repeats[0]); i++)
        {
            repeats[i] = -1;
        }
    }

    // First, poll the buttons and act accordingly.
    maple_poll_buttons();
    jvs_buttons_t pressed = maple_buttons_pressed();
    jvs_buttons_t held = maple_buttons_held();

    // Copy over joystick config.
    controls_t controls;
    controls.joy1_v = held.player1.analog1;
    controls.joy1_h = held.player1.analog2;
    if (state->settings->system.players >= 2)
    {
        controls.joy2_v = held.player2.analog1;
        controls.joy2_h = held.player2.analog2;
    }
    else
    {
        controls.joy2_v = 80;
        controls.joy2_h = 80;
    }

    // Process buttons and repeats.
    controls.up_pressed = 0;
    controls.down_pressed = 0;
    controls.left_pressed = 0;
    controls.right_pressed = 0;
    controls.start_pressed = 0;
    controls.test_pressed = 0;
    controls.service_pressed = 0;

    // Process separate controls.
    controls.psw1 = 0;
    controls.psw2 = 0;
    controls.dipswitches = (held.dip1 ? 0x1 : 0x0) | (held.dip2 ? 0x2 : 0x0) | (held.dip3 ? 0x4 : 0x0) | (held.dip4 ? 0x8 : 0x0);

    if (full_separate && held.psw1)
    {
        controls.psw1 = 1;
    }
    if (full_separate && held.psw2)
    {
        controls.psw2 = 1;
    }

    if (pressed.test || ((!full_separate) && pressed.psw1))
    {
        controls.test_pressed = 1;
    }
    else if (pressed.player1.service || ((!full_separate) && pressed.psw2) || (state->settings->system.players >= 2 && pressed.player2.service))
    {
        controls.service_pressed = 1;
    }
    else
    {
        if (pressed.player1.start || (state->settings->system.players >= 2 && pressed.player2.start))
        {
            controls.start_pressed = 1;
        }
        else
        {
            if (pressed.player1.up || (state->settings->system.players >= 2 && pressed.player2.up))
            {
                controls.up_pressed = 1;

                repeat_init(pressed.player1.up, &repeats[0]);
                repeat_init(pressed.player2.up, &repeats[1]);
            }
            else if (pressed.player1.down || (state->settings->system.players >= 2 && pressed.player2.down))
            {
                controls.down_pressed = 1;

                repeat_init(pressed.player1.down, &repeats[2]);
                repeat_init(pressed.player2.down, &repeats[3]);
            }
            if (repeat(held.player1.up, &repeats[0]) || (state->settings->system.players >= 2 && repeat(held.player2.up, &repeats[1])))
            {
                controls.up_pressed = 1;
            }
            else if (repeat(held.player1.down, &repeats[2]) || (state->settings->system.players >= 2 && repeat(held.player2.down, &repeats[3])))
            {
                controls.down_pressed = 1;
            }
            if (pressed.player1.left || (state->settings->system.players >= 2 && pressed.player2.left))
            {
                controls.left_pressed = 1;

                repeat_init(pressed.player1.left, &repeats[4]);
                repeat_init(pressed.player2.left, &repeats[5]);
            }
            else if (pressed.player1.right || (state->settings->system.players >= 2 && pressed.player2.right))
            {
                controls.right_pressed = 1;

                repeat_init(pressed.player1.right, &repeats[6]);
                repeat_init(pressed.player2.right, &repeats[7]);
            }
            if (repeat(held.player1.left, &repeats[4]) || (state->settings->system.players >= 2 && repeat(held.player2.left, &repeats[5])))
            {
                controls.left_pressed = 1;
            }
            else if (repeat(held.player1.right, &repeats[6]) || (state->settings->system.players >= 2 && repeat(held.player2.right, &repeats[7])))
            {
                controls.right_pressed = 1;
            }
        }
    }

    return controls;
}
