#ifndef PRG32_H
#define PRG32_H
#include <stdint.h>
#define PRG32_BTN_A (1u<<0)
#define PRG32_BTN_B (1u<<1)
#define PRG32_BTN_UP (1u<<2)
#define PRG32_BTN_DOWN (1u<<3)
#define PRG32_BTN_LEFT (1u<<4)
#define PRG32_BTN_RIGHT (1u<<5)
#define PRG32_BTN_SELECT (1u<<6)
typedef struct {const uint8_t *pixels;const uint16_t *palette;uint16_t width,height,frame_count,palette_count;uint8_t bits_per_pixel;int16_t transparent_index;} prg32_indexed_sprite_t;
typedef struct {uint32_t player_id;int16_t x,y;uint16_t sprite,flags;uint32_t input,frame,last_seen_ms;} prg32_player_state_t;
uint32_t prg32_input_read(void);void prg32_band_set_game_info(const char*);void prg32_audio_play_track(uint16_t);
void prg32_gfx_clear(uint16_t);void prg32_gfx_present(void);void prg32_gfx_rect(int,int,int,int,uint16_t);void prg32_gfx_text8(int,int,const char*,uint16_t,uint16_t);
void prg32_sprite_draw_bitplanes(int,int,const prg32_indexed_sprite_t*,uint32_t);
void prg32_multiplayer_init(void);int prg32_multiplayer_available(void);int prg32_multiplayer_join(const char*,uint32_t);int prg32_multiplayer_leave(void);void prg32_multiplayer_tick(void);int prg32_multiplayer_set_local_state(int16_t,int16_t,uint16_t,uint16_t);int prg32_multiplayer_set_input(uint32_t);int prg32_multiplayer_get_peer_count(void);int prg32_multiplayer_get_peer(int,prg32_player_state_t*);
#endif
