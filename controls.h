#ifndef __CONTROLS_H
#define __CONTROLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
    // The following controls only ever need a pressed event.
    uint8_t up_pressed;
    uint8_t down_pressed;
    uint8_t left_pressed;
    uint8_t right_pressed;
    uint8_t test_pressed;
    uint8_t start_pressed;
    uint8_t service_pressed;

    // The following controls are only for the front panel test.
    uint8_t psw1;
    uint8_t psw2;
    uint8_t dipswitches;

    // The following controlls need raw analog values for calibration.
    uint8_t joy1_h;
    uint8_t joy1_v;
    uint8_t joy1_a3;
    uint8_t joy1_a4;
    uint8_t joy2_h;
    uint8_t joy2_v;
    uint8_t joy2_a3;
    uint8_t joy2_a4;
} controls_t;

#define COMBINED_CONTROLS 0
#define SEPARATE_CONTROLS 1

controls_t get_controls(state_t *state, int reinit, int full_separate);

#ifdef __cplusplus
}
#endif

#endif
