#ifndef NACUP_PLATFORM_H
#define NACUP_PLATFORM_H
#include "prg32.h"

/* Keep the game source readable across the current indexed-sprite and player
   snapshot names while retaining its bitplane-specific terminology. */
typedef prg32_indexed_sprite_t prg32_bitplane_sprite_t;
typedef prg32_player_state_t prg32_multiplayer_peer_t;
#define planes pixels
#define plane_count bits_per_pixel

/* Kept here so an ABI rename only touches one file. Current PRG32 main uses
   planar pixels ordered frame -> plane -> row, MSB first. */
typedef prg32_bitplane_sprite_t nacup_sprite_t;
static inline void nacup_draw_sprite(int x,int y,const nacup_sprite_t *s,unsigned frame){
    prg32_sprite_draw_bitplanes(x,y,s,(uint16_t)frame);
}

/* PRG32's multiplayer service transports compact player snapshots. */
static inline int nacup_net_join(int course){
    const char *room=course==1?"nacup-napoli97:v7-olympic":course==2?"nacup-napoli97:v7-iacc92":"nacup-napoli97:v7-windward";
    prg32_multiplayer_init();
    return prg32_multiplayer_available() &&
           prg32_multiplayer_join(room,0)>=0;
}
static inline void nacup_net_publish(int x,int y,int heading,uint32_t input,uint8_t flags){
    prg32_multiplayer_set_local_state(x,y,(uint16_t)heading,flags);
    prg32_multiplayer_set_input(input);
    prg32_multiplayer_tick();
}
static inline int nacup_net_peer_count(void){
    int count=prg32_multiplayer_get_peer_count();
    return count<0?0:count>3?3:count;
}
static inline int nacup_net_peer(int index,prg32_multiplayer_peer_t *peer){
    return index>=0&&index<3&&prg32_multiplayer_get_peer(index,peer)>=0;
}
#endif
