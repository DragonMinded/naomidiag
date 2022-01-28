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

    // The following controlls need raw analog values for calibration.
    uint8_t joy1_h;
    uint8_t joy1_v;
    uint8_t joy2_h;
    uint8_t joy2_v;
} controls_t;

controls_t get_controls(state_t *state, int reinit);

#ifdef __cplusplus
}
#endif

#endif
