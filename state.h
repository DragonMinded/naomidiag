#ifndef __STATE_H
#define __STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <naomi/eeprom.h>
#include <naomi/video.h>
#include <naomi/ta.h>

typedef struct
{
    int scroll;
} sounds_t;

typedef struct
{
    eeprom_t *settings;
    double fps;
    double animation_counter;
    font_t *font_18pt;
    font_t *font_12pt;
    texture_description_t *sprite_up;
    texture_description_t *sprite_down;
    texture_description_t *sprite_cursor;
    sounds_t sounds;
} state_t;

#ifdef __cplusplus
}
#endif

#endif
