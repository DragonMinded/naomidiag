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
    texture_description_t *up;
    texture_description_t *down;
    texture_description_t *cursor;
    texture_description_t *pswoff;
    texture_description_t *pswon;
} sprites_t;

typedef struct
{
    eeprom_t *settings;
    double fps;
    double animation_counter;
    font_t *font_18pt;
    font_t *font_12pt;
    sprites_t sprites;
    sounds_t sounds;
} state_t;

#ifdef __cplusplus
}
#endif

#endif
