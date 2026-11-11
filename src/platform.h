#ifndef FAIRWIND_PLATFORM_H
#define FAIRWIND_PLATFORM_H
#include "prg32.h"

/* Every firmware call the cartridge makes goes through this file, so an ABI
   rename only touches one place. PRG32 1.8, portable 128 KiB profile. */
typedef prg32_player_state_t prg32_multiplayer_peer_t;

/* ------------------------------------------------------------ graphics -- */
/* Fills use palette indices: the ESP32-C6 framebuffer stores one index per
   pixel, QEMU expands it through the same palette. Text takes RGB565 (T_*),
   which the firmware maps to the cube cell holding that exact colour. */
static inline void gx_rect(int x,int y,int w,int h,uint8_t c){prg32_gfx_rect_indexed(x,y,w,h,c);}
static inline void gx_pixel(int x,int y,uint8_t c){prg32_gfx_pixel_indexed(x,y,c);}
static inline void gx_text(int x,int y,const char *s,uint16_t fg,uint16_t bg){prg32_gfx_text8(x,y,s,fg,bg);}
static inline void gx_palette(uint8_t i,uint16_t rgb){prg32_palette_set(i,rgb);}

/* ---------------------------------------------------------- multiplayer -- */
/* Rooms are versioned: v9 adds lobby team arbitration and the late-joiner
   flag, so v8 and v9 cartridges never meet in one room. */
static inline int fairwind_net_join(int course){
    const char *room=course==1?"fairwind-napoli97:v9-olympic":course==2?"fairwind-napoli97:v9-iacc92":"fairwind-napoli97:v9-windward";
    prg32_multiplayer_init();
    return prg32_multiplayer_available() &&
           prg32_multiplayer_join(room,0)>=0;
}
/* The firmware relays x, y, sprite and flags as 16-bit fields but keeps only
   the low seven bits of input. */
static inline void fairwind_net_publish(int x,int y,uint16_t sprite,uint32_t input,uint16_t flags){
    prg32_multiplayer_set_local_state((int16_t)x,(int16_t)y,sprite,flags);
    prg32_multiplayer_set_input(input);
    prg32_multiplayer_tick();
}
static inline int fairwind_net_peer_count(void){
    int count=prg32_multiplayer_get_peer_count();
    return count<0?0:count>3?3:count;
}
static inline int fairwind_net_peer(int index,prg32_multiplayer_peer_t *peer){
    return index>=0&&index<3&&prg32_multiplayer_get_peer(index,peer)>=0;
}
/* 0 until the server confirms this console; treated as the highest id. */
static inline uint32_t fairwind_net_local_id(void){
    uint32_t id=prg32_multiplayer_local_player_id();
    return id?id:0xffffffffu;
}

/* ---------------------------------------------------------------- audio -- */
static inline void snd_note(uint8_t voice,uint8_t inst,uint8_t note,uint8_t vol,int8_t pan){prg32_audio_note_on_pan(voice,inst,note,vol,pan);}
static inline void snd_off(uint8_t voice){prg32_audio_note_off(voice);}
static inline void snd_volume(uint8_t voice,uint8_t vol){prg32_audio_set_channel_volume(voice,vol);}
static inline void snd_pan(uint8_t voice,int8_t pan){prg32_audio_set_channel_pan(voice,pan);}
/* A song always starts from silence, with every channel back on its
   instrument's own volume. */
static inline void snd_song(uint16_t track){
    int c;prg32_audio_stop_track();prg32_audio_stop_all();
    for(c=0;c<8;c++)prg32_audio_set_channel_volume((uint8_t)c,0);
    prg32_audio_play_track(track);
}
static inline void snd_silence(void){prg32_audio_stop_track();prg32_audio_stop_all();}

/* --------------------------------------------------------------- scores -- */
#define FAIRWIND_SCORE_GAME "fairwind-napoli97"
static inline void fairwind_score_submit(uint32_t score){prg32_score_submit_current_player(FAIRWIND_SCORE_GAME,score);}
static inline int fairwind_score_count(void){int n=prg32_score_count(FAIRWIND_SCORE_GAME);return n<0?0:n;}
static inline int fairwind_score_get(int i,prg32_score_t *s){return prg32_score_get(FAIRWIND_SCORE_GAME,i,s)==0;}
#endif
