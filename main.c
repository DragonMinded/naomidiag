#include <stdint.h>
#include <naomi/video.h>
#include <naomi/ta.h>
#include <naomi/eeprom.h>
#include <naomi/timer.h>
#include <naomi/audio.h>
#include <naomi/font.h>
#include "common.h"
#include "state.h"
#include "screens.h"

// Sounds compiled in from Makefile.
extern uint8_t *scroll_raw_data;
extern unsigned int scroll_raw_len;
extern uint8_t *scale_raw_data;
extern unsigned int scale_raw_len;

// Sprites, compiled in from Makefile.
extern unsigned int up_png_width;
extern unsigned int up_png_height;
extern void *up_png_data;
extern unsigned int dn_png_width;
extern unsigned int dn_png_height;
extern void *dn_png_data;
extern unsigned int cursor_png_width;
extern unsigned int cursor_png_height;
extern void *cursor_png_data;
extern unsigned int pswoff_png_width;
extern unsigned int pswoff_png_height;
extern void *pswoff_png_data;
extern unsigned int pswon_png_width;
extern unsigned int pswon_png_height;
extern void *pswon_png_data;
extern unsigned int buttonmask_png_width;
extern unsigned int buttonmask_png_height;
extern void *buttonmask_png_data;

void main()
{
    // Grab the system configuration for monitor rotation/etc.
    eeprom_t settings;
    eeprom_read(&settings);

    // Init the screen for full range of color for video test subsystem.
    video_init(VIDEO_COLOR_8888);
    ta_set_background_color(rgb(0, 0, 0));

    // Init audio system for audio test subsystem.
    audio_init();

    // Create global state for the menu.
    state_t state;
    state.settings = &settings;

    // Initialize some system sounds.
    state.sounds.scroll = audio_register_sound(AUDIO_FORMAT_16BIT, 44100, scroll_raw_data, scroll_raw_len / 2);
    state.sounds.scale = audio_register_sound(AUDIO_FORMAT_16BIT, 11025, scale_raw_data, scale_raw_len / 2);
    audio_set_registered_sound_loop(state.sounds.scale, 0);

    // Attach our menu/system fonts
    extern uint8_t *dejavusans_ttf_data;
    extern unsigned int dejavusans_ttf_len;
    state.font_18pt = font_add(dejavusans_ttf_data, dejavusans_ttf_len);
    font_set_size(state.font_18pt, 18);
    state.font_12pt = font_add(dejavusans_ttf_data, dejavusans_ttf_len);
    font_set_size(state.font_12pt, 12);

    // Attach our input test font
    extern uint8_t *dejavumono_ttf_data;
    extern unsigned int dejavumono_ttf_len;
    state.font_mono = font_add(dejavumono_ttf_data, dejavumono_ttf_len);
    font_set_size(state.font_mono, 12);

    // Attach our sprites
    state.sprites.up = ta_texture_desc_malloc_direct(up_png_width, up_png_data, TA_TEXTUREMODE_ARGB1555);
    state.sprites.down = ta_texture_desc_malloc_direct(dn_png_width, dn_png_data, TA_TEXTUREMODE_ARGB1555);
    state.sprites.cursor = ta_texture_desc_malloc_direct(cursor_png_width, cursor_png_data, TA_TEXTUREMODE_ARGB1555);
    state.sprites.pswoff = ta_texture_desc_malloc_direct(pswoff_png_width, pswoff_png_data, TA_TEXTUREMODE_ARGB1555);
    state.sprites.pswon = ta_texture_desc_malloc_direct(pswon_png_width, pswon_png_data, TA_TEXTUREMODE_ARGB1555);
    state.sprites.buttonmask = ta_texture_desc_malloc_direct(buttonmask_png_width, buttonmask_png_data, TA_TEXTUREMODE_ARGB1555);

    // FPS calculation for debugging.
    double fps_value = 60.0;

    // Simple animations for the screen.
    double animation_counter = 0.0;

    while ( 1 )
    {
        // Get FPS measurements.
        int fps = profile_start();

        // Set up the global state for any draw screen.
        state.fps = fps_value;
        state.animation_counter = animation_counter;

        // Now, draw the current screen.
        int profile = profile_start();
        ta_commit_begin();
        draw_screen(&state);
        ta_commit_end();
        ta_render();
        uint32_t draw_time = profile_end(profile);

        // Display some debugging info.
        if (DEBUG_ENABLED)
        {
            video_draw_debug_text((video_width() / 2) - (18 * 4), video_height() - 40, rgb(0, 200, 255), "uS full draw: %d", draw_time);
            video_draw_debug_text((video_width() / 2) - (18 * 4), video_height() - 32, rgb(0, 200, 255), "FPS: %.01f, %dx%d", fps_value, video_width(), video_height());
            video_draw_debug_text((video_width() / 2) - (28 * 4), video_height() - 24, rgb(0, 200, 255), "CPU draw time consume: %.01f%%", (draw_time * fps_value) / 10000.00);
        }

        // Actually draw the buffer.
        video_display_on_vblank();

        // Calculate instantaneous FPS, adjust animation counters.
        uint32_t uspf = profile_end(fps);
        fps_value = (1000000.0 / (double)uspf) + 0.01;
        animation_counter += (double)uspf / 1000000.0;
    }
}

void test()
{
    // Our ROM *IS* a diagnostics/test ROM, so just make the test mode
    // identical to the main executable.
    main();
}
