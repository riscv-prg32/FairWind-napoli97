#ifndef PRG32_H
#define PRG32_H
/* Host stub of the PRG32 1.8 cartridge API: just the calls FairWind makes,
   with the firmware's own button bits. */
#include <stdint.h>
#include <stddef.h>
#define PRG32_BTN_LEFT (1u<<0)
#define PRG32_BTN_RIGHT (1u<<1)
#define PRG32_BTN_UP (1u<<2)
#define PRG32_BTN_DOWN (1u<<3)
#define PRG32_BTN_A (1u<<4)
#define PRG32_BTN_B (1u<<5)
#define PRG32_BTN_START (1u<<6)
#define PRG32_BAND_TOP 0
#define PRG32_BAND_BOTTOM 1
typedef struct {uint32_t player_id;int16_t x,y;uint16_t sprite,flags;uint32_t input,frame,last_seen_ms;} prg32_player_state_t;
typedef struct {char game[24];char player[24];uint32_t score;} prg32_score_t;
uint32_t prg32_input_read(void);
void prg32_band_set_game_info(const char*);void prg32_band_set_colors(uint8_t,uint16_t,uint16_t);
void prg32_gfx_present(void);void prg32_gfx_text8(int,int,const char*,uint16_t,uint16_t);
void prg32_gfx_rect_indexed(int,int,int,int,uint8_t);void prg32_gfx_pixel_indexed(int,int,uint8_t);void prg32_palette_set(uint8_t,uint16_t);
void prg32_audio_note_on_pan(uint8_t,uint8_t,uint8_t,uint8_t,int8_t);void prg32_audio_note_off(uint8_t);
void prg32_audio_set_channel_volume(uint8_t,uint8_t);void prg32_audio_set_channel_pan(uint8_t,int8_t);
void prg32_audio_play_track(uint16_t);void prg32_audio_stop_track(void);void prg32_audio_stop_all(void);
void prg32_multiplayer_init(void);int prg32_multiplayer_available(void);int prg32_multiplayer_join(const char*,uint32_t);int prg32_multiplayer_leave(void);void prg32_multiplayer_tick(void);int prg32_multiplayer_set_local_state(int16_t,int16_t,uint16_t,uint16_t);int prg32_multiplayer_set_input(uint32_t);int prg32_multiplayer_get_peer_count(void);int prg32_multiplayer_get_peer(int,prg32_player_state_t*);uint32_t prg32_multiplayer_local_player_id(void);
int prg32_score_submit_current_player(const char*,uint32_t);int prg32_score_count(const char*);int prg32_score_get(const char*,int,prg32_score_t*);
#endif
