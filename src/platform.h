#ifndef FAIRWIND_PLATFORM_H
#define FAIRWIND_PLATFORM_H
#include "prg32.h"

/* Keep the game source readable across the current indexed-sprite and player
   snapshot names while retaining its bitplane-specific terminology. */
typedef prg32_indexed_sprite_t prg32_bitplane_sprite_t;
typedef prg32_player_state_t prg32_multiplayer_peer_t;
#define planes pixels
#define plane_count bits_per_pixel

/* Kept here so an ABI rename only touches one file. Current PRG32 main uses
   planar pixels ordered frame -> plane -> row, MSB first. */
typedef prg32_bitplane_sprite_t fairwind_sprite_t;
static inline void fairwind_draw_sprite(int x,int y,const fairwind_sprite_t *s,unsigned frame){
    prg32_sprite_draw_bitplanes(x,y,s,(uint16_t)frame);
}

/* PRG32's multiplayer service transports compact player snapshots. */
static inline int fairwind_net_join(int course){
    const char *room=course==1?"fairwind-napoli97:v8-olympic":course==2?"fairwind-napoli97:v8-iacc92":"fairwind-napoli97:v8-windward";
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
#endif
