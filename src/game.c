#include "platform.h"
#include "game.h"
#include "assets_bitplanes.h"
#include "fixmath.h"
#include <stddef.h>
#include <stdint.h>
#define W 320
#define H 200
#define BLACK 0x0000
#define WHITE 0xffff
#define SEA 0x0452
#define SKY 0x65df
#define NAVY 0x0010
#define GOLD 0xfda0
#define RED 0xf800
#define GREEN 0x07e0
#define CYAN 0x07ff
#define GREY 0x8410
#define LAND 0x39e7
/* Race-view colours sit exactly on the display's 6x6x6 colour cube, so the
   indexed ILI9341 panel shows the same shades as the RGB565 QEMU display. */
#define C6(r,g,b) ((uint16_t)((((r)*31+4)/5)<<11|(((g)*63+4)/5)<<5|(((b)*31+4)/5)))
#define SKY1 C6(1,3,5)
#define SKY2 C6(2,4,5)
#define SKY3 C6(3,4,5)
#define HILL C6(2,3,4)
#define SEA1 C6(1,2,3)
#define SEA2 C6(0,2,3)
#define SEA3 C6(0,1,3)
#define SEA4 C6(0,1,2)
#define RIPPLE C6(1,3,4)
#define FOAM C6(4,5,5)
#define HULL_RED C6(3,0,0)
#define DECK C6(5,5,4)
#define CLOTH C6(5,5,5)
#define CLOTH_LUFF C6(3,3,4)
#define SPAR C6(1,1,1)
#define BUOY C6(5,3,0)
#define SAND C6(5,4,3)
#define LAND_GROUND C6(2,3,1)
/* Race text sits on black and uses the firmware's named colours: the ILI9341
   backend converts every text pixel, and only named colours skip its three
   divisions (measured: 2,378 instructions per white-on-black character
   against 7,053 on navy). */
#define PANEL BLACK
#define YELLOW 0xffe0
#define RACES 5
#define START_COUNTDOWN_SECONDS 600
#define RACE_LIMIT_SECONDS 2700
/* Simulated seconds per real second: fast-forward until the box opens, then
   a 4x race clock so a 12-Metre's 8 knots reads as 8 knots on screen. */
#define FAST_TIME_SCALE 20
#define RACE_TIME_SCALE 4
/* PRG32 paces cartridges at one frame per 33 ms; the simulation integrates
   the measured frame time, so speeds stay true at any sustained frame rate. */
#define FRAME_MS 33
/* World +y points into the mean south-westerly; all distances in metres. */
#define PREVAILING_SW_HEADING 0
#define LINE_HALF 110
#define BOX_HALF 150
#define BOX_DEPTH 200
#define FINISH_Y (-80)
#define FINISH_HALF 90
#define FIELD_X 900
#define FIELD_Y0 (-450)
#define FIELD_Y1 1250
#define MARK_CAPTURE 40
#define MARK_ZONE 63
#define TOP_VIEW_ENTER 70
#define TOP_VIEW_EXIT 100
#define COURSE_COUNT 3
#define COURSE_MARKS_MAX 6
/* Yacht rig in decimetres, forward = +y from the hull centre. */
#define MAST_Y 20
#define MAST_TOP 250
#define BOOM_Z 25
#define BOOM_LEN 85
/* Chase camera in decimetres and pixels. */
#define HOR 76
#define FOCAL 150
#define CAM_BACK 300
#define CAM_H 65
#define NEAR_DM 20
#define CLIP_TOP 18
#define CLIP_BOT 180
#define TOP_S 15
#define TOP_TILT 3
#define TOP_CY 140
typedef enum {ST_TITLE,ST_MODE,ST_TEAM,ST_MANAGER,ST_BRIEF,ST_LOBBY,ST_RACE,ST_RESULT,ST_SEASON} screen_t;
typedef enum {RULE_NONE,RULE_PORT,RULE_WINDWARD,RULE_ASTERN,RULE_TACKING,RULE_CONTACT,RULE_MARK_ROOM,RULE_MARK_TOUCH} rule_t;
enum {KITE_NONE,KITE_SPIN,KITE_GENN};
typedef struct {char name[18],code[4];uint16_t color,kite;uint8_t speed,turn,crew;} team_t;
typedef struct {
    int32_t x,y,v,aws,tack_start,ai_last_tack;   /* mm, mm, mm/s<<8, mm/s, sim s */
    uint32_t turned;                              /* penalty-turn progress, bam */
    uint16_t heading,kite_prog,finish_time,ai_clock;
    int16_t rudder,boom,twa,awa,hacc;             /* boom in degrees, + = starboard */
    int8_t tack,heel,serve_dir,ai_tack;
    uint8_t team,leg,finished,penalty,serving,kite,kite_want,sheet,started,start_ready,entered_box,tacking,rule,mark_touch,touching,aground,ai_above;
} boat_t;
typedef struct {char name[18];int fee,win_bonus;uint8_t min_wins;} sponsor_t;
typedef struct {int32_t x,h,z;} v3;
/* GCC may lower small descriptor assignments to memcpy/memset even under
   -ffreestanding.  Portable cartridges do not link a C library. */
void *memcpy(void *dst,const void *src,size_t size){volatile uint8_t *d=(volatile uint8_t *)dst;const volatile uint8_t *s=(const volatile uint8_t *)src;size_t i;for(i=0;i<size;i++)d[i]=s[i];return dst;}
void *memset(void *dst,int value,size_t size){volatile uint8_t *d=(volatile uint8_t *)dst;size_t i;for(i=0;i<size;i++)d[i]=(uint8_t)value;return dst;}
static const team_t teams[4]={{"PARTENOPE 12","ITA",CYAN,C6(0,4,5),6,7,7},{"LIBECCIO CORSE","ITA",RED,C6(5,1,1),7,6,6},{"ATLANTIC UNION","USA",WHITE,C6(4,2,5),7,7,5},{"SOUTHERN CROSS","AUS",0xffe0,C6(5,4,0),6,6,8}};
static const sponsor_t sponsors[4]={{"GULF MARINE",18,7,0},{"VESUVIO TECH",26,11,1},{"CAPRI LUX",38,16,2},{"PARTENOPE GLOBAL",55,24,3}};
static const char race_names[RACES][16]={"SANTA LUCIA","VESUVIUS CUP","CAPRI PASSAGE","SORRENTO RACE","NAPLES FINAL"};
static const char course_names[COURSE_COUNT][20]={"WINDWARD / RUN","OLYMPIC TRIANGLE","1992 IACC Z"};
static const char rule_names[8][20]={"CLEAR","PORT / STARBOARD","WINDWARD / LEEWARD","CLEAR ASTERN","WHILE TACKING","AVOID CONTACT","MARK ROOM","TOUCHED MARK"};
static const uint8_t course_len[COURSE_COUNT]={3,5,6};
/* Mark positions in metres: x across the course, y upwind of the start. */
static const int16_t course_x[COURSE_COUNT][COURSE_MARKS_MAX]={{0,0,0,0,0,0},{0,-360,0,0,0,0},{0,360,0,0,-360,0}};
static const int16_t course_y[COURSE_COUNT][COURSE_MARKS_MAX]={{800,120,800,0,0,0},{800,450,120,800,120,0},{800,450,120,800,450,800}};
static fairwind_sprite_t cup_sprite,logo_sprite;static boat_t boats[4],prop;static screen_t screen;
static uint32_t last_input,frame,rng=0x97ca1997u,wind_seed;static int team_sel,menu,points[4],race_no,money,hull,sails,crew,strategy,sponsor,wins,last_prize,tws10,race_clock,start_clock,sim_acc,signal_sound_ms,course_sel,frame_ms=FRAME_MS,sheet_acc;static uint32_t last_ticks;static uint8_t race_header,hud_force,ui_dirty=1;static int drawn_screen=-1;static uint8_t bg_idx[16];static int32_t wind_t;static uint16_t twd,cam_h;static uint8_t multiplayer,peer_count,player_rank,local_ready,start_line_active,top_view_mode,ab_combo,result_order[4],net_live[4];static uint32_t net_ids[4];
/* PRG32's ILI9341 backend fills palette-indexed spans with memset but
   converts RGB565 colours pixel by pixel, so every fill goes through the
   palette index the firmware would pick for that colour. */
static uint8_t ci(uint16_t c){static const uint16_t named[8]={0x0000,0xffff,0xf800,0x07e0,0x001f,0xffe0,0x07ff,0xf81f};uint8_t i;for(i=0;i<8;i++)if(c==named[i])return i;return (uint8_t)(16u+((c>>11)&31u)*5u/31u*36u+((c>>5)&63u)*5u/63u*6u+(c&31u)*5u/31u);}
static void fill(int x,int y,int w,int h,uint16_t c){prg32_gfx_rect_indexed(x,y,w,h,ci(c));}
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}static int absi(int n){return n<0?-n:n;}static int slen(const char *t){int n=0;while(t[n])n++;return n;}static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static void number(int x,int y,int n,uint16_t fg,uint16_t bg){char b[8];int i=6;b[7]=0;if(n<0)n=0;do{b[i--]=(char)('0'+n%10);n/=10;}while(n&&i>=0);while(i>=0)b[i--]=' ';prg32_gfx_text8(x,y,b,fg,bg);}static void digit(int x,int y,int n,uint16_t fg,uint16_t bg){char b[2];b[0]=(char)('0'+clamp(n,0,9));b[1]=0;prg32_gfx_text8(x,y,b,fg,bg);}static void clock_mmss(int x,int y,int n,uint16_t fg,uint16_t bg){char b[6];int m;if(n<0)n=0;m=clamp(n/60,0,99);b[0]=(char)('0'+m/10);b[1]=(char)('0'+m%10);b[2]=':';b[3]=(char)('0'+(n%60)/10);b[4]=(char)('0'+n%10);b[5]=0;prg32_gfx_text8(x,y,b,fg,bg);}static void box(int x,int y,int w,int h,uint16_t e,uint16_t f){fill(x,y,w,h,e);fill(x+2,y+2,w-4,h-4,f);}
/* Left-aligned integer, optionally with one decimal (n in tenths). */
static void small_num(int x,int y,int n,int tenths,uint16_t fg,uint16_t bg){char b[8];int i=0,d=1;if(n<0)n=0;if(n>9999)n=9999;while(n/d>=10)d*=10;if(tenths&&d<10)d=10;for(;d;d/=10){if(tenths&&d==1)b[i++]='.';b[i++]=(char)('0'+n/d%10);}b[i]=0;prg32_gfx_text8(x,y,b,fg,bg);}
static void sprite(fairwind_sprite_t *s,const uint8_t *pixels,const uint16_t *palette,uint16_t w,uint16_t h,uint16_t frames,uint8_t bpp,int16_t transparent){s->planes=pixels;s->palette=palette;s->width=w;s->height=h;s->frame_count=frames;s->plane_count=bpp;s->palette_count=(uint16_t)(1u<<bpp);s->transparent_index=transparent;}
static void setup_sprites(void){sprite(&cup_sprite,fairwind_cup_planes,fairwind_palette,32,40,1,4,0);sprite(&logo_sprite,fairwind_logo_planes,fairwind_logo_palette,96,96,1,4,-1);}
static void new_campaign(void){int i;for(i=0;i<4;i++)points[i]=0;race_no=wins=course_sel=0;money=70;hull=sails=crew=strategy=1;sponsor=0;last_prize=0;menu=0;screen=ST_TEAM;}
static void play_start_signal(int long_signal){prg32_audio_play_track((uint16_t)(long_signal?4:3));signal_sound_ms=long_signal?700:400;}
/* Humans keep their chosen syndicate; AI slots take the teams nobody picked. */
static void assign_ai_teams(void){int i,t,n=fairwind_net_peer_count();unsigned used=1u<<(team_sel&3);prg32_multiplayer_peer_t p;for(i=0;i<n;i++)if(fairwind_net_peer(i,&p))used|=1u<<((p.flags>>3)&3);for(t=0,i=n+1;i<4&&t<4;t++)if(!((used>>t)&1))boats[i++].team=(uint8_t)t;}

/* ---------------------------------------------------------------- wind -- */
static int time_scale(void){return start_clock>120?FAST_TIME_SCALE:RACE_TIME_SCALE;}
/* Simulated milliseconds that pass during this frame. */
static int32_t sim_ms(void){return (int32_t)time_scale()*frame_ms;}
static int32_t tws_mms(void){return (int32_t)tws10*5144/100;}
/* Deterministic in the course and elapsed simulated time, so every console
   in a multiplayer room sails the same breeze: two oscillating shifts, a
   slow persistent veer or back, and pressure pulses around the day's base. */
static void update_wind(void){
    uint32_t s=wind_seed;int32_t t=wind_t,trend=(int32_t)((s>>19)%11u)-5;
    int32_t shift=((DEG(7)*fsin((uint16_t)(t*156+(int32_t)(s>>3))))>>14)+((DEG(4)*fsin((uint16_t)(t*345+(int32_t)(s>>11))))>>14)+trend*DEG(1)*t/3300;
    twd=(uint16_t)(PREVAILING_SW_HEADING+shift);
    tws10=80+20*(int)(s%5u)+(int)((15*fsin((uint16_t)(t*504+(int32_t)(s>>5))))>>14)+(int)((8*fsin((uint16_t)(t*1191+(int32_t)(s>>13))))>>14);
}
static int wind_shift_deg(void){return BAM2DEG((int16_t)(twd-PREVAILING_SW_HEADING));}

/* ------------------------------------------------------ polar and trim -- */
static const uint8_t polar_angle[13]={0,28,34,40,52,60,75,90,110,120,135,150,180};
static const uint8_t polar8[13]={0,0,40,63,71,74,76,76,76,74,68,57,50};
static const uint8_t polar12[13]={0,0,46,72,81,84,87,88,89,89,86,79,69};
static const uint8_t polar16[13]={0,0,48,75,84,88,91,93,94,96,94,91,82};
/* Sail-plan efficiency (%) per polar row: the ORC rows above 90 degrees
   assume a kite, so the jib alone falls off downwind; the symmetric
   spinnaker collapses when reaching; the gennaker owns the reaches. */
static const uint8_t plan_jib[13]={100,100,100,100,100,100,100,100,90,84,78,74,72};
static const uint8_t plan_spin[13]={40,40,40,40,40,45,60,88,100,100,100,100,100};
static const uint8_t plan_genn[13]={50,50,50,55,65,80,102,106,104,100,95,90,82};
static int row_at(const uint8_t *p,int a){int i;for(i=1;i<13&&a>polar_angle[i];i++){}if(i>=13)return p[12];return p[i-1]+(p[i]-p[i-1])*(a-polar_angle[i-1])/(polar_angle[i]-polar_angle[i-1]);}
static int polar_speed10(int twa,int tws){int lo,hi;if(tws<=80)return row_at(polar8,twa)*tws/80;if(tws>=160)return row_at(polar16,twa);if(tws<=120){lo=row_at(polar8,twa);hi=row_at(polar12,twa);return lo+(hi-lo)*(tws-80)/40;}lo=row_at(polar12,twa);hi=row_at(polar16,twa);return lo+(hi-lo)*(tws-120)/40;}
static int sail_plan(const boat_t *b,int twa){int j=row_at(plan_jib,twa),k;if(b->kite==KITE_NONE)return j;k=row_at(b->kite==KITE_SPIN?plan_spin:plan_genn,twa);return j+(int)((k-j)*(int32_t)b->kite_prog/65535);}
static int awa_deg(const boat_t *b){return absi(BAM2DEG(b->awa));}
/* Best boom angle keeps ~18 degrees angle of attack to the apparent wind. */
static int trim_opt(int awa){return clamp(awa-18,2,85);}
/* Eased past the optimum the luff lifts and drive dies; over-trimmed the
   flow stalls and drive falls off more gently. */
static int trim_eff(const boat_t *b){int err=absi(b->boom)-trim_opt(awa_deg(b));if(err>0)return clamp(100-err*err*100/324,0,100);return clamp(100-err*err*100/2025,30,100);}
static int crew_of(const boat_t *b){return b==&boats[0]?crew:teams[b->team].crew;}

/* ------------------------------------------------------------- physics -- */
static void begin_penalty_turn(boat_t *b){if(b->penalty&&!b->serving){b->serving=1;b->turned=0;b->serve_dir=(int8_t)(b->tack>0?1:-1);}}
static void award_penalty(boat_t *b,rule_t rule){if(!b->penalty){b->penalty=1;b->serving=0;b->turned=0;b->rule=(uint8_t)rule;}}
static int clamp_field(boat_t *b){int32_t x=b->x,y=b->y,lim=(FIELD_X-12)*1000;b->x=clamp(b->x,-lim,lim);b->y=clamp(b->y,(FIELD_Y0+12)*1000,(FIELD_Y1-12)*1000);return x!=b->x||y!=b->y;}
/* Apparent wind, tack state, boom swing, kite hoist/drop and heel. Remote
   yachts run this too so their rigs agree with the local breeze. */
static void update_rig(boat_t *b,int local){
    int awa,side,lim,step,target,cr=crew_of(b),heel;int32_t sm=sim_ms(),tws=tws_mms(),wf,ws;int8_t nt;
    b->twa=(int16_t)(twd-b->heading);
    wf=((tws*fcos((uint16_t)b->twa))>>14)+(b->v>>8);ws=(tws*fsin((uint16_t)b->twa))>>14;
    b->awa=(int16_t)bearing(ws,wf);b->aws=isqrt((uint32_t)(wf*wf+ws*ws));
    nt=(int8_t)(b->twa>=0?1:-1);
    if(nt!=b->tack&&absi(b->twa)<DEG(90)){b->tacking=1;b->tack_start=wind_t;}
    b->tack=nt;
    if(b->tacking&&(absi(b->twa)>=DEG(38)||wind_t-b->tack_start>20))b->tacking=0;
    awa=awa_deg(b);side=b->awa>=0?-1:1;lim=b->sheet<awa?b->sheet:awa;target=side*lim;
    /* The wind carries the boom to leeward until the sheet stops it; a gybe
       slams it across much faster than a tack walks it over. */
    step=(int)(((b->boom<0)!=(side<0)&&absi(b->boom)>8?180:45)*sm/1000);if(step<1)step=1;
    if(b->boom<target)b->boom=(int16_t)(b->boom+step>target?target:b->boom+step);else if(b->boom>target)b->boom=(int16_t)(b->boom-step<target?target:b->boom-step);
    if(local){
        int32_t up=65535L*sm/(1000*(32-2*cr)),down=65535L*sm/(1000*(20-cr));
        if(b->kite!=b->kite_want){if(b->kite_prog>down)b->kite_prog=(uint16_t)(b->kite_prog-down);else{b->kite_prog=0;b->kite=b->kite_want;}}
        else if(b->kite!=KITE_NONE&&b->kite_prog<65535)b->kite_prog=(uint16_t)(b->kite_prog+up>65535?65535:b->kite_prog+up);
    }
    heel=awa<100?(int)(b->aws/514)*3*(100-awa)/200*trim_eff(b)/100:0;b->heel=(int8_t)(side*clamp(heel,0,24));
}
/* Polar target speed x sail plan x trim x syndicate, reached through the
   momentum of a 26-tonne keelboat; rudder angle both turns and brakes. */
static void sail(boat_t *b){
    int twa=absi(BAM2DEG(b->twa)),cr=crew_of(b),perf,tau;int32_t sm=sim_ms(),v=b->v>>8,target,old_y=b->y,q,step,drift,vv;
    if(b->finished)return;
    target=(int32_t)polar_speed10(twa,tws10)*5144/100;
    perf=b==&boats[0]?97+hull+sails+strategy/2:96+teams[b->team].speed;
    target=target*sail_plan(b,twa)/100*trim_eff(b)/100*perf/100;
    tau=target>v?11-cr/2:14;
    b->v+=((target<<8)-b->v)/(10*tau)*sm/100;
    b->v-=(b->v/1000)*absi(b->rudder)*sm/5000;if(b->v<0)b->v=0;
    v=b->v>>8;vv=v<4100?v:4100;
    q=(int32_t)b->rudder*(300+900*vv/4100)*182/100;
    q=q*sm/100*(92+2*cr)/100*256/1000+b->hacc;
    b->heading=(uint16_t)(b->heading+(q>>8));b->hacc=(int16_t)(q&255);
    if(b->serving){b->turned+=(uint32_t)absi((int)(q>>8));if(b->turned>=65536u){b->penalty=b->serving=0;b->turned=0;b->rule=RULE_NONE;}}
    step=v*sm/1000;b->x+=(step*fsin(b->heading))>>14;b->y+=(step*fcos(b->heading))>>14;
    drift=tws_mms()*15/1000*sm/1000;b->x-=(drift*fsin(twd))>>14;b->y-=(drift*fcos(twd))>>14;
    /* The shore and the race-area limit are hard: a yacht driven into them
       loses all way and has to steer off. */
    b->aground=(uint8_t)clamp_field(b);if(b->aground)b->v/=2;
    if(start_clock>120&&b->y<15000){b->y=15000;b->v-=b->v/8;}
    if(start_clock<=120&&!b->entered_box&&old_y>=0&&b->y<0&&((!(b->team&1)&&b->x>=-BOX_HALF*1000&&b->x<0)||((b->team&1)&&b->x>0&&b->x<=BOX_HALF*1000)))b->entered_box=1;
    if(start_clock>0)b->start_ready=(uint8_t)(b->entered_box&&b->y<0);
    else if(!b->started){if(b->entered_box&&b->y<0)b->start_ready=1;if(b->entered_box&&b->start_ready&&old_y<0&&b->y>=0&&b->x>=-LINE_HALF*1000&&b->x<=LINE_HALF*1000)b->started=1;}
    if(b->started&&b->leg<course_len[course_sel]){int32_t dx=b->x/1000-course_x[course_sel][b->leg],dy=b->y/1000-course_y[course_sel][b->leg];if(dx*dx+dy*dy<MARK_CAPTURE*MARK_CAPTURE)b->leg++;}
    else if(b->started&&b->leg>=course_len[course_sel]&&old_y>=FINISH_Y*1000&&b->y<FINISH_Y*1000&&b->x>=-FINISH_HALF*1000&&b->x<=FINISH_HALF*1000){b->finished=1;b->finish_time=(uint16_t)(race_clock+b->penalty*180);}
}
static void slew_rudder(boat_t *b,int target,int rate){if(b->rudder<target)b->rudder=(int16_t)(b->rudder+rate>target?target:b->rudder+rate);else if(b->rudder>target)b->rudder=(int16_t)(b->rudder-rate<target?target:b->rudder-rate);}
/* Left helms to port, right to starboard; up eases and down trims sheets. */
static void helm_player(boat_t *b,uint32_t in){
    int target=0,step;
    if(b->serving)target=b->serve_dir*100;else{if(in&PRG32_BTN_LEFT)target-=100;if(in&PRG32_BTN_RIGHT)target+=100;}
    slew_rudder(b,target,12*frame_ms/16);
    /* Sheets run at (14 + 2 x crew) degrees per simulated second. */
    if(in&(PRG32_BTN_UP|PRG32_BTN_DOWN))sheet_acc+=(14+2*crew)*(int)sim_ms();else sheet_acc=0;
    step=sheet_acc/1000;sheet_acc%=1000;
    if(in&PRG32_BTN_UP)b->sheet=(uint8_t)clamp(b->sheet+step,0,90);
    if(in&PRG32_BTN_DOWN)b->sheet=(uint8_t)clamp(b->sheet-step,0,90);
}

/* ------------------------------------------------------------------ AI -- */
static const int16_t layline_cos[4]={16322,16262,16182,16083};
static void leg_target(int leg,int32_t *x,int32_t *y){if(leg<course_len[course_sel]){*x=course_x[course_sel][leg]*1000;*y=course_y[course_sel][leg]*1000;}else{*x=0;*y=(FINISH_Y-40)*1000;}}
static uint8_t kite_for(int rel){return (uint8_t)(rel>=118?KITE_SPIN:rel>=68?KITE_GENN:KITE_NONE);}
static int near_edge(const boat_t *b,uint16_t h){int32_t x=b->x/1000+((120*fsin(h))>>14),y=b->y/1000+((120*fcos(h))>>14);return x<-FIELD_X+30||x>FIELD_X-30||y<FIELD_Y0+30||y>FIELD_Y1-30;}
static int clear_water(const boat_t *b,int r){int i;for(i=0;i<4;i++){int32_t dx=(boats[i].x-b->x)/1000,dy=(boats[i].y-b->y)/1000;if(&boats[i]!=b&&!boats[i].finished&&dx*dx+dy*dy<r*r)return 0;}return 1;}
static int32_t windward_score(const boat_t *b){return ((b->x/100)*fsin(twd)+(b->y/100)*fcos(twd))>>14;}
/* Beat or run on the tack that gains: go at the (team-specific) layline, when
   the current tack points well away, on a 5-degree header, or off an edge. */
static uint16_t vmg_heading(boat_t *b,uint16_t brg,int angle){
    uint16_t hs=(uint16_t)(twd-DEG(angle)),hp=(uint16_t)(twd+DEG(angle)),cur,oth;int32_t cc,co;int headed;
    if(!b->ai_tack)b->ai_tack=b->tack;
    cur=b->ai_tack>0?hs:hp;oth=b->ai_tack>0?hp:hs;cc=fcos((uint16_t)(cur-brg));co=fcos((uint16_t)(oth-brg));
    headed=angle<90&&wind_shift_deg()*b->ai_tack<=-5;
    if(wind_t-b->ai_last_tack>=25&&(co>=layline_cos[b->team&3]||co>cc+4000||(headed&&co>0)||near_edge(b,cur))){b->ai_tack=(int8_t)-b->ai_tack;b->ai_last_tack=wind_t;cur=oth;}
    return cur;
}
/* Rule 10/11/12 keep-clear duties as the AI understands them. */
static int gives_way(const boat_t *b,const boat_t *o,int *head_up){
    int32_t dx=(o->x-b->x)/100,dy=(o->y-b->y)/100,along=(dx*fsin(b->heading)+dy*fcos(b->heading))>>14;
    *head_up=0;if(b->tack!=o->tack)return b->tack<0;
    if(along>150)return 1;if(along<-150)return 0;
    *head_up=1;return windward_score(b)>windward_score(o);
}
static uint16_t avoid_traffic(const boat_t *b,uint16_t want){
    int i,head_up;
    for(i=0;i<4;i++){
        const boat_t *o=&boats[i];int32_t rx,ry,wx,wy,w2,t,cx,cy;
        if(o==b||o->finished)continue;
        rx=(o->x-b->x)/100;ry=(o->y-b->y)/100;if(rx>1500||rx<-1500||ry>1500||ry<-1500)continue;
        wx=((((o->v>>8)*fsin(o->heading))>>14)-(((b->v>>8)*fsin(b->heading))>>14))/100;wy=((((o->v>>8)*fcos(o->heading))>>14)-(((b->v>>8)*fcos(b->heading))>>14))/100;
        w2=wx*wx+wy*wy;if(w2<4)continue;t=-(rx*wx+ry*wy)/w2;if(t<=0||t>25)continue;
        cx=rx+wx*t;cy=ry+wy*t;if(cx*cx+cy*cy>250*250)continue;
        /* Rule 14: even the right-of-way yacht avoids contact when it is imminent. */
        if(!gives_way(b,o,&head_up)&&(cx*cx+cy*cy>120*120||t>8))continue;
        return (uint16_t)(want+(head_up?DEG(20):-DEG(30))*b->tack);
    }
    return want;
}
/* Timed start from the box, port-rounding offsets, VMG beats and runs with
   shift and layline tacking, give-way manoeuvres and crewed kite work. */
static void helm_ai(boat_t *b){
    int side=(b->team&1)?1:-1,rel,luff=0,cr=teams[b->team].crew;int32_t tx,ty,dx,dy,dist;uint16_t brg,want;
    b->ai_clock++;
    if(b->serving){b->rudder=(int16_t)(b->serve_dir*100);b->sheet=(uint8_t)trim_opt(awa_deg(b));return;}
    if(!b->started){
        int hold=0,slot=b->team>>1,ease=0;
        if(b->y>20000)b->ai_above=1;else if(b->y<0&&!b->entered_box)b->ai_above=0;
        if(!b->entered_box&&start_clock>120){tx=side*(LINE_HALF+40+25*slot)*1000;ty=(45+25*slot)*1000;hold=1;}
        else if(!b->entered_box&&!b->ai_above){tx=side*(LINE_HALF-30)*1000;ty=30000;}
        else if(!b->entered_box){tx=side*(LINE_HALF-30)*1000;ty=-40000;}
        else if(start_clock>0){
            /* Time-distance: kill time deep in the box, leave for the line
               when a close-hauled run just makes the gun, and meter speed on
               the way in by easing sheets. */
            int32_t lx=side*(20+25*slot)*1000,vcl=(int32_t)polar_speed10(45,tws10)*5144/100*9/10,d,vreq;
            dx=(lx-b->x)/1000;dy=(5000-b->y)/1000;d=isqrt((uint32_t)(dx*dx+dy*dy));
            if(start_clock>d*1000/vcl+((b->v>>8)<vcl/2?14:4)+2*slot+6){tx=side*(40+20*slot)*1000;ty=-140000;hold=1;}
            else{tx=lx;ty=40000;vreq=d*1000/(start_clock>2?start_clock-1:1);ease=vreq<vcl?18*(int)(vcl-vreq)/(int)vcl+2:0;if(b->y>-12000&&start_clock>3)luff=1;}
        }
        else if(!b->start_ready){tx=b->x;ty=-40000;}
        else{tx=side*30000;ty=60000;}
        dx=(tx-b->x)/1000;dy=(ty-b->y)/1000;if(hold&&dx*dx+dy*dy<30*30)luff=1;
        b->ai_above=(uint8_t)(b->ai_above|(uint8_t)(ease<<1));
        b->kite_want=KITE_NONE;
    }else{
        int32_t mx,my,nx,ny;int rel_leg,rel_next;
        leg_target(b->leg,&mx,&my);brg=bearing(mx-b->x,my-b->y);
        if(b->leg<course_len[course_sel]){tx=mx+((18000*fcos(brg))>>14);ty=my-((18000*fsin(brg))>>14);}else{tx=mx;ty=my;}
        dx=(mx-b->x)/1000;dy=(my-b->y)/1000;dist=isqrt((uint32_t)(dx*dx+dy*dy));
        rel_leg=absi(BAM2DEG((int16_t)(brg-twd)));
        if(b->leg<course_len[course_sel]){leg_target(b->leg+1,&nx,&ny);rel_next=absi(BAM2DEG((int16_t)(bearing(nx-mx,ny-my)-twd)));}else rel_next=rel_leg;
        if((b->ai_clock&15)==0){
            uint8_t want_kite=kite_for(rel_leg);int twa=absi(BAM2DEG(b->twa));
            if(dist<110&&kite_for(rel_next)==KITE_NONE)want_kite=KITE_NONE;
            if(want_kite==KITE_NONE||(want_kite==KITE_SPIN&&twa>=100)||(want_kite==KITE_GENN&&twa>=60))b->kite_want=want_kite;
        }
    }
    brg=bearing(tx-b->x,ty-b->y);rel=absi(BAM2DEG((int16_t)(brg-twd)));
    if(luff)want=(uint16_t)(twd-b->tack*DEG(55));
    else if(rel<46)want=vmg_heading(b,brg,42);
    else if(rel>150&&b->started)want=vmg_heading(b,brg,158);
    else want=brg;
    if(!luff)want=avoid_traffic(b,want);
    slew_rudder(b,clamp(BAM2DEG((int16_t)(want-b->heading))*5+(int)(rnd()%3)-1,-100,100),10*frame_ms/16);
    b->sheet=(uint8_t)(luff?90:clamp(trim_opt(awa_deg(b))+(b->started?0:b->ai_above>>1)+((int)(((b->ai_clock>>6)+b->team*3u)%5u)-2)*(9-cr)/2,0,90));
    b->ai_above&=1;
}

/* --------------------------------------------------------------- rules -- */
static void hull_points(const boat_t *b,int32_t *px,int32_t *py){int32_t s=fsin(b->heading),c=fcos(b->heading),l;int k;for(k=0;k<5;k++){l=(k-2)*47;px[k]=b->x/100+((l*s)>>14);py[k]=b->y/100+((l*c)>>14);}}
static int32_t mark_dist2(const boat_t *b){int32_t dx,dy;if(b->leg>=course_len[course_sel])return 99999999;dx=b->x/1000-course_x[course_sel][b->leg];dy=b->y/1000-course_y[course_sel][b->leg];return dx*dx+dy*dy;}
/* Closest approach (dm^2) of a yacht's hull to any buoy on the water. */
static int32_t buoy_clearance(const boat_t *b){
    int32_t px[5],py[5],best=99999999,d,mx,my;int i,k,n=course_len[course_sel]+1;
    hull_points(b,px,py);
    for(i=0;i<n;i++){
        if(i<course_len[course_sel]){mx=course_x[course_sel][i]*10;my=course_y[course_sel][i]*10;}else if(start_line_active){mx=-LINE_HALF*10;my=0;}else{mx=-FINISH_HALF*10;my=FINISH_Y*10;}
        for(k=0;k<5;k++){d=(px[k]-mx)*(px[k]-mx)+(py[k]-my)*(py[k]-my);if(d<best)best=d;}
    }
    return best;
}
static void enforce_rules(void){
    int i,j,k,m;
    if(start_clock>240)return;
    for(i=0;i<4;i++){int32_t d=buoy_clearance(&boats[i]);if(d<25*25&&!boats[i].touching){boats[i].touching=1;boats[i].mark_touch=1;}else if(d>60*60)boats[i].touching=0;if(boats[i].mark_touch){award_penalty(&boats[i],RULE_MARK_TOUCH);boats[i].mark_touch=0;}}
    for(i=0;i<4;i++)for(j=i+1;j<4;j++){
        boat_t *a=&boats[i],*b=&boats[j],*o;rule_t r;int32_t ax[5],ay[5],bx[5],by[5],dx,dy,along,da,db,best=99999999;uint16_t away;
        if(a->finished||b->finished)continue;
        hull_points(a,ax,ay);hull_points(b,bx,by);
        for(k=0;k<5;k++)for(m=0;m<5;m++){int32_t d=(ax[k]-bx[m])*(ax[k]-bx[m])+(ay[k]-by[m])*(ay[k]-by[m]);if(d<best)best=d;}
        if(best>=40*40)continue;
        /* Rule 14 contact: both yachts lose way and bounce apart. */
        dx=(b->x-a->x)/100;dy=(b->y-a->y)/100;away=bearing(dx,dy);
        if(a->v>(1000<<8))a->v/=2;if(b->v>(1000<<8))b->v/=2;a->x-=(2500*fsin(away))>>14;a->y-=(2500*fcos(away))>>14;b->x+=(2500*fsin(away))>>14;b->y+=(2500*fcos(away))>>14;clamp_field(a);clamp_field(b);
        if(a->penalty||b->penalty)continue;
        da=mark_dist2(a);db=mark_dist2(b);
        if(a->tacking!=b->tacking){o=a->tacking?a:b;r=RULE_TACKING;}
        else if(a->leg==b->leg&&(da<MARK_ZONE*MARK_ZONE||db<MARK_ZONE*MARK_ZONE)){o=da<=db?b:a;r=RULE_MARK_ROOM;}
        else if(a->tack!=b->tack){o=a->tack<0?a:b;r=RULE_PORT;}
        else{along=(dx*fsin(a->heading)+dy*fcos(a->heading))>>14;if(absi((int)along)<210){o=windward_score(a)>=windward_score(b)?a:b;r=RULE_WINDWARD;}else{o=along>0?a:b;r=RULE_ASTERN;}}
        award_penalty(o,r);
    }
}

/* ---------------------------------------------------------- race state -- */
static void start_race(void){
    int i;uint32_t s=0x9e3779b9u*(uint32_t)(course_sel+1)^(multiplayer?0u:(uint32_t)race_no*0x85ebca6bu);
    s^=s>>15;s*=0x2c1b3c6du;s^=s>>12;wind_seed=s;wind_t=0;update_wind();
    race_clock=0;start_clock=START_COUNTDOWN_SECONDS;sim_acc=0;start_line_active=1;top_view_mode=0;signal_sound_ms=0;ab_combo=1;
    for(i=0;i<4;i++){net_live[i]=0;boats[i].team=(uint8_t)((team_sel+i)%4);}
    if(multiplayer)assign_ai_teams();
    for(i=0;i<4;i++){
        boat_t *b=&boats[i];uint8_t team=b->team;int side=(team&1)?1:-1;
        memset(b,0,sizeof *b);b->team=team;
        b->x=side*(LINE_HALF+60+20*(team>>1))*1000;b->y=(90+30*(team>>1))*1000;
        b->heading=(uint16_t)(side>0?DEG(300):DEG(60));b->sheet=90;b->tack=(int8_t)((int16_t)(twd-b->heading)>=0?1:-1);b->ai_last_tack=-100;
        update_rig(b,1);b->boom=(int16_t)(b->awa>=0?-awa_deg(b):awa_deg(b));
    }
    cam_h=boats[0].heading;screen=ST_RACE;race_header=0;hud_force=1;play_start_signal(0);
}
static void score_race(void){int i,j;for(i=0;i<4;i++)result_order[i]=(uint8_t)i;for(i=0;i<4;i++)for(j=i+1;j<4;j++)if(boats[result_order[j]].finish_time<boats[result_order[i]].finish_time){uint8_t t=result_order[i];result_order[i]=result_order[j];result_order[j]=t;}for(i=0;i<4;i++){points[boats[result_order[i]].team]+=4-i;if(result_order[i]==0)player_rank=(uint8_t)(i+1);}last_prize=30-(player_rank-1)*7;if(player_rank==1){wins++;last_prize+=sponsors[sponsor].win_bonus;}money+=last_prize+sponsors[sponsor].fee;if(sponsor<3&&wins>=sponsors[sponsor+1].min_wins)sponsor++;}
static int upgrade_cost(int l){return 8+l*7;}static void update_manager(uint32_t p){int *v=0;if(p&PRG32_BTN_UP)menu--;if(p&PRG32_BTN_DOWN)menu++;menu=clamp(menu,0,5);if(menu==1)v=&hull;else if(menu==2)v=&sails;else if(menu==3)v=&crew;else if(menu==4)v=&strategy;if((p&PRG32_BTN_A)&&menu==0&&sponsor<3&&wins>=sponsors[sponsor+1].min_wins)sponsor++;else if((p&PRG32_BTN_A)&&v&&*v<8&&money>=upgrade_cost(*v)){money-=upgrade_cost(*v);(*v)++;}else if((p&PRG32_BTN_A)&&menu==5)screen=ST_BRIEF;if((p&PRG32_BTN_B)&&v&&*v>1){(*v)--;money+=upgrade_cost(*v)/2;}}
/* Peers are bound to fleet slots by player id, so a disconnect hands only
   that yacht to the AI instead of shifting every later peer down a slot.
   Each console owns its own penalties; remote ones are never kept here.
   Snapshot: x/y in decimetres; sprite = heading(9) | sheet(7); flags =
   leg(3) team(2) spin started finished racing gennaker kite-hoist(3)
   penalty; the 7-bit input field carries speed in 0.2-knot steps. */
static int net_slot(uint32_t id,const uint8_t *seen){int i;for(i=1;i<4;i++)if(net_live[i]&&net_ids[i]==id)return i;for(i=1;i<4;i++)if(!net_live[i]&&!seen[i])return i;return 0;}
static void net_update(void){
    int i,n,slot;uint8_t seen[4]={0,0,0,0};prg32_multiplayer_peer_t p;const boat_t *me=&boats[0];
    uint16_t flags=(uint16_t)((me->leg&7)|((team_sel&3)<<3)|(me->kite==KITE_SPIN?0x20:0)|(me->started?0x40:0)|(me->finished?0x80:0)|0x100|(me->kite==KITE_GENN?0x200:0)|((me->kite_prog>>13)<<10)|(me->penalty?0x2000:0));
    fairwind_net_publish((int)(me->x/100),(int)(me->y/100),(uint16_t)((me->heading>>7)|((unsigned)me->sheet<<9)),(uint32_t)clamp((int)((me->v>>8)/103),0,127),flags);
    n=fairwind_net_peer_count();
    for(i=0;i<n;i++)if(fairwind_net_peer(i,&p)&&(slot=net_slot(p.player_id,seen))>0){
        boat_t *b=&boats[slot];uint8_t was_finished=b->finished;
        seen[slot]=1;net_ids[slot]=p.player_id;
        if(!(p.flags&0x100))continue;
        b->x=(int32_t)p.x*100;b->y=(int32_t)p.y*100;b->heading=(uint16_t)((p.sprite&511u)<<7);b->sheet=(uint8_t)(p.sprite>>9);b->v=(int32_t)(p.input&127u)*103*256;
        b->leg=p.flags&7;b->team=(p.flags>>3)&3;b->kite=b->kite_want=(uint8_t)((p.flags&0x20)?KITE_SPIN:(p.flags&0x200)?KITE_GENN:KITE_NONE);b->kite_prog=(uint16_t)(((p.flags>>10)&7u)*9362u);
        b->started=(p.flags&0x40)!=0;b->finished=(p.flags&0x80)!=0;if(b->started||(start_clock<=120&&b->y<0))b->entered_box=1;
        b->penalty=b->serving=b->mark_touch=0;b->rule=RULE_NONE;
        if(!was_finished&&b->finished)b->finish_time=(uint16_t)race_clock;
    }
    peer_count=0;for(i=1;i<4;i++){net_live[i]=seen[i];peer_count=(uint8_t)(peer_count+seen[i]);}
}
/* Lobby snapshots carry only team and readiness; a racing snapshot sets 0x100,
   so a peer already racing has seen every player ready and counts as ready. */
static void update_lobby(uint32_t p){int i,all_ready;prg32_multiplayer_peer_t peer;if(p&PRG32_BTN_A)local_ready=1;if(p&PRG32_BTN_B){prg32_multiplayer_leave();multiplayer=0;peer_count=0;local_ready=0;start_race();return;}fairwind_net_publish(0,0,(uint16_t)team_sel,0,(uint16_t)(((team_sel&3)<<3)|(local_ready?0x40:0)));peer_count=(uint8_t)fairwind_net_peer_count();all_ready=local_ready&&peer_count>0;for(i=0;i<peer_count;i++)if(!fairwind_net_peer(i,&peer)||!(peer.flags&0x140))all_ready=0;if(all_ready)start_race();}
static int close_to_rival_or_buoy(int limit){
    int i;int32_t dx,dy,l2=(int32_t)limit*limit,bx=boats[0].x/1000,by=boats[0].y/1000;
    for(i=1;i<4;i++){dx=bx-boats[i].x/1000;dy=by-boats[i].y/1000;if(dx*dx+dy*dy<=l2)return 1;}
    for(i=0;i<course_len[course_sel];i++){dx=bx-course_x[course_sel][i];dy=by-course_y[course_sel][i];if(dx*dx+dy*dy<=l2)return 1;}
    if(start_line_active){dx=absi((int)bx)-LINE_HALF;dy=by;return dx*dx+dy*dy<=l2;}
    dx=absi((int)bx)-FINISH_HALF;dy=by-FINISH_Y;return dx*dx+dy*dy<=l2;
}
/* A toggles the spinnaker and B the gennaker on release, so pressing both
   (a penalty turn) never flips a kite. Changing kite drops the other first. */
static void update_race(uint32_t in,uint32_t released){
    int i,done=1,all_started=1;boat_t *me=&boats[0];
    if(signal_sound_ms>0&&(signal_sound_ms-=frame_ms)<=0){signal_sound_ms=0;prg32_audio_play_track(1);}
    for(sim_acc+=sim_ms();sim_acc>=1000;){sim_acc-=1000;wind_t++;update_wind();if(start_clock>0){start_clock--;if(start_clock==300||start_clock==240||start_clock==60)play_start_signal(0);else if(start_clock==0)play_start_signal(1);}else race_clock++;}
    if((in&(PRG32_BTN_A|PRG32_BTN_B))==(PRG32_BTN_A|PRG32_BTN_B)){ab_combo=1;begin_penalty_turn(me);}
    else{if((released&PRG32_BTN_A)&&!ab_combo)me->kite_want=(uint8_t)(me->kite_want==KITE_SPIN?KITE_NONE:KITE_SPIN);if((released&PRG32_BTN_B)&&!ab_combo)me->kite_want=(uint8_t)(me->kite_want==KITE_GENN?KITE_NONE:KITE_GENN);}
    if(!(in&(PRG32_BTN_A|PRG32_BTN_B)))ab_combo=0;
    helm_player(me,in);update_rig(me,1);sail(me);
    if(multiplayer)net_update();
    for(i=1;i<4;i++){boat_t *b=&boats[i];if(net_live[i]){update_rig(b,0);continue;}if(b->penalty&&clear_water(b,45))begin_penalty_turn(b);helm_ai(b);update_rig(b,1);sail(b);}
    enforce_rules();
    top_view_mode=(uint8_t)close_to_rival_or_buoy(top_view_mode?TOP_VIEW_EXIT:TOP_VIEW_ENTER);
    cam_h=(uint16_t)(cam_h+(int16_t)(me->heading-cam_h)*frame_ms/100);
    for(i=0;i<4;i++){if(!boats[i].started)all_started=0;if(!boats[i].finished)done=0;}
    if(all_started)start_line_active=0;
    if(race_clock>RACE_LIMIT_SECONDS)for(i=0;i<4;i++)if(!boats[i].finished){boats[i].finished=1;boats[i].finish_time=(uint16_t)(race_clock+600+(COURSE_MARKS_MAX+1-(boats[i].started?boats[i].leg+1:0))*30);}
    if((done&&start_clock==0)||race_clock>RACE_LIMIT_SECONDS){score_race();screen=ST_RESULT;menu=0;prg32_audio_play_track(2);}
}
void fairwind_init(void){int i;for(i=0;i<16;i++)bg_idx[i]=ci(fairwind_background_palette[i]);prg32_gfx_clear(SEA);setup_sprites();prg32_band_set_game_info("FairWind-napoli97 | portable 64 KiB | multiplayer");prg32_audio_play_track(0);screen=ST_TITLE;}
void fairwind_update(void){uint32_t in=prg32_input_read(),p=in&~last_input,r=last_input&~in,now=prg32_ticks_ms();frame++;frame_ms=last_ticks?clamp((int)(now-last_ticks),10,66):FRAME_MS;last_ticks=now;if(p)ui_dirty=1;if(screen==ST_TITLE&&(p&PRG32_BTN_A))screen=ST_MODE;else if(screen==ST_MODE){if(p&(PRG32_BTN_UP|PRG32_BTN_DOWN))multiplayer=!multiplayer;if(p&PRG32_BTN_A)new_campaign();}else if(screen==ST_TEAM){if(p&PRG32_BTN_LEFT)team_sel=(team_sel+3)%4;if(p&PRG32_BTN_RIGHT)team_sel=(team_sel+1)%4;if(p&PRG32_BTN_A){menu=0;screen=ST_MANAGER;}}else if(screen==ST_MANAGER)update_manager(p);else if(screen==ST_BRIEF){if(p&(PRG32_BTN_LEFT|PRG32_BTN_UP))course_sel=(course_sel+COURSE_COUNT-1)%COURSE_COUNT;if(p&(PRG32_BTN_RIGHT|PRG32_BTN_DOWN))course_sel=(course_sel+1)%COURSE_COUNT;if(p&PRG32_BTN_A){if(multiplayer){peer_count=local_ready=0;if(fairwind_net_join(course_sel))screen=ST_LOBBY;else multiplayer=0;}if(!multiplayer)start_race();}}else if(screen==ST_LOBBY)update_lobby(p);else if(screen==ST_RACE)update_race(in,r);else if(screen==ST_RESULT&&(p&PRG32_BTN_A)){if(multiplayer)prg32_multiplayer_leave();race_no++;if(race_no>=RACES)screen=ST_SEASON;else{menu=0;screen=ST_MANAGER;prg32_audio_play_track(0);}}else if(screen==ST_SEASON&&(p&PRG32_BTN_A)){screen=ST_TITLE;prg32_audio_play_track(0);}last_input=in;}

/* ------------------------------------------------------------- menus -- */
static void header(const char *t){fill(0,0,W,18,NAVY);prg32_gfx_text8(6,5,t,WHITE,NAVY);prg32_gfx_text8(238,5,"NAPOLI 1997",GOLD,NAVY);}
static void landscape(void){unsigned scene=(unsigned)(race_no%RACES),i=fairwind_background_offsets[scene],end=fairwind_background_offsets[scene+1];int x=0,y=27;fill(0,18,W,33,SKY);while(i<end){int count=fairwind_background_rle8[i++];uint8_t color=bg_idx[fairwind_background_rle8[i++]];while(count){int width=count<320-x?count:320-x;prg32_gfx_rect_indexed(x,y,width,1,color);x+=width;count-=width;if(x==320){x=0;y++;}}}fill(0,51,W,15,SEA);}
/* The FairWind logo on a white badge over the venue skyline. */
static void draw_title(void){header("FairWind");landscape();fill(106,56,108,108,WHITE);fairwind_draw_sprite(112,62,&logo_sprite,0);prg32_gfx_text8(49,170,"12-METRE AMERICA'S CUP",WHITE,SEA);prg32_gfx_text8(76,186,"PRESS A TO SET SAIL",GOLD,SEA);}
static void draw_mode(void){header("CHAMPIONSHIP MODE");box(48,48,224,78,GREY,NAVY);prg32_gfx_text8(77,67,"SINGLE PLAYER",!multiplayer?GOLD:WHITE,NAVY);prg32_gfx_text8(77,94,"NETWORK MULTIPLAYER",multiplayer?GOLD:WHITE,NAVY);prg32_gfx_text8(74,151,"UP/DOWN  A CONFIRM",WHITE,SEA);}
static void draw_team(void){int i;header("CHOOSE YOUR SYNDICATE");for(i=0;i<4;i++){int y=28+i*36;box(24,y,272,29,i==team_sel?GOLD:GREY,NAVY);fill(34,y+7,20,15,teams[i].color);prg32_gfx_text8(64,y+6,teams[i].name,WHITE,NAVY);prg32_gfx_text8(64,y+17,teams[i].code,GOLD,NAVY);}prg32_gfx_text8(55,178,"LEFT/RIGHT  A CONFIRM",WHITE,SEA);}
static void statrow(int y,const char *n,int v,int s){int i;prg32_gfx_text8(30,y,n,s?GOLD:WHITE,NAVY);for(i=0;i<8;i++)fill(132+i*13,y,10,7,i<v?CYAN:GREY);}
static void draw_manager(void){header("TEAM HQ");prg32_gfx_text8(12,24,teams[team_sel].name,GOLD,SEA);prg32_gfx_text8(164,24,"CASH K",WHITE,SEA);number(217,24,money,WHITE,SEA);box(16,38,288,133,GREY,NAVY);prg32_gfx_text8(30,49,sponsors[sponsor].name,menu==0?GOLD:WHITE,NAVY);number(222,49,sponsors[sponsor].fee,WHITE,NAVY);statrow(70,"HULL",hull,menu==1);statrow(88,"SAILS",sails,menu==2);statrow(106,"CREW",crew,menu==3);statrow(124,"STRATEGY",strategy,menu==4);prg32_gfx_text8(30,148,"RACE WEEKEND",menu==5?GOLD:WHITE,NAVY);prg32_gfx_text8(20,180,"A SIGN/INVEST/GO  B SELL",WHITE,SEA);}
static void draw_brief(void){header(race_names[race_no]);landscape();box(26,63,268,103,GREY,NAVY);prg32_gfx_text8(42,68,"SELECT RACE PATH",WHITE,NAVY);prg32_gfx_text8(42,81,course_names[course_sel],GOLD,NAVY);prg32_gfx_text8(42,94,"RRS 10-14, 18, 31 ACTIVE",WHITE,NAVY);prg32_gfx_text8(42,107,"SW 8-16 KT, SHIFTING",WHITE,NAVY);prg32_gfx_text8(42,120,"L/R HELM  UP EASE DN TRIM",CYAN,NAVY);prg32_gfx_text8(42,133,"A SPI  B GEN  A+B PENALTY",CYAN,NAVY);prg32_gfx_text8(42,149,"LEFT/RIGHT CHOOSE  A GO",GOLD,NAVY);prg32_gfx_text8(83,177,"A - LEAVE DOCK",WHITE,SEA);}
static void draw_lobby(void){header("PRG32 MULTIPLAYER");box(45,38,230,108,GREY,NAVY);prg32_gfx_text8(72,49,"ROOM FAIRWIND-NAPOLI97",GOLD,NAVY);prg32_gfx_text8(72,67,course_names[course_sel],CYAN,NAVY);prg32_gfx_text8(72,84,"PLAYERS",WHITE,NAVY);digit(133,84,peer_count+1,peer_count?GREEN:WHITE,NAVY);prg32_gfx_text8(145,84,"/ 4",WHITE,NAVY);prg32_gfx_text8(72,105,local_ready?"YOU ARE READY":"A - MARK READY",local_ready?GREEN:WHITE,NAVY);prg32_gfx_text8(72,126,peer_count?"WAITING FOR READY...":"WAITING FOR PEERS...",WHITE,NAVY);prg32_gfx_text8(69,157,"B - SOLO WITH AI",WHITE,SEA);}

/* ------------------------------------------------------ 3D rasteriser -- */
/* Camera space is decimetres: x to the right, z forward, h height above the
   eye. The chase camera rides CAM_BACK behind and CAM_H above the yacht; the
   close-quarters view is an overhead orthographic camera with a slight tilt
   so masts and sails keep their shape. Both views clip in camera space. */
static int32_t cam_wx,cam_wy,cam_s,cam_c;
static void camera_setup(void){cam_s=fsin(cam_h);cam_c=fcos(cam_h);cam_wx=boats[0].x/100;cam_wy=boats[0].y/100;if(!top_view_mode){cam_wx-=(CAM_BACK*cam_s)>>14;cam_wy-=(CAM_BACK*cam_c)>>14;}}
static v3 cam_pt(int32_t wx,int32_t wy,int32_t z){v3 r;int32_t dx=wx-cam_wx,dy=wy-cam_wy;r.x=(dx*cam_c-dy*cam_s)>>14;r.z=(dx*cam_s+dy*cam_c)>>14;r.h=top_view_mode?z:z-CAM_H;return r;}
static int plane_count(void){return top_view_mode?4:3;}
static int32_t plane_d(const v3 *p,int i){
    if(top_view_mode)return i==0?p->x+1150:i==1?1150-p->x:i==2?p->z+400:900-p->z;
    return i==0?p->z-NEAR_DM:i==1?p->z*16+p->x*15:p->z*16-p->x*15;
}
static v3 lerp3(const v3 *a,const v3 *b,int32_t da,int32_t db){
    int32_t den=da-db,t;v3 r;
    while(da>(1<<18)||da<-(1<<18)||den>(1<<18)||den<-(1<<18)){da/=2;den/=2;}
    t=den?(da*4096)/den:0;
    r.x=a->x+(((b->x-a->x)*t)>>12);r.h=a->h+(((b->h-a->h)*t)>>12);r.z=a->z+(((b->z-a->z)*t)>>12);return r;
}
static void proj(const v3 *p,int *sx,int *sy){
    if(top_view_mode){*sx=160+(int)(p->x*TOP_S/100);*sy=TOP_CY-(int)((p->z*TOP_S+p->h*TOP_TILT)/100);}
    else{*sx=160+(int)(p->x*FOCAL/p->z);*sy=HOR-(int)(p->h*FOCAL/p->z);}
}
static void rect_clip(int x,int y,int w,int h,uint8_t c){if(x<0){w+=x;x=0;}if(x+w>W)w=W-x;if(y<CLIP_TOP){h-=CLIP_TOP-y;y=CLIP_TOP;}if(y+h>CLIP_BOT)h=CLIP_BOT-y;if(w>0&&h>0)prg32_gfx_rect_indexed(x,y,w,h,c);}
/* Even-odd scanline fill, so concave sail outlines render correctly. Each
   edge gets a 16.16 slope once, so a scanline costs multiplies, not divides. */
static void fill_poly(const int *xs,const int *ys,int n,uint8_t c){
    int y,y0=9999,y1=-9999,i,j,k,m,ey0[16],ey1[16];int32_t ex[16],es[16];
    for(i=m=0;i<n;i++){
        j=i+1==n?0:i+1;if(ys[i]<y0)y0=ys[i];if(ys[i]>y1)y1=ys[i];if(ys[i]==ys[j])continue;
        k=ys[i]<ys[j]?i:j;ey0[m]=ys[k];ey1[m]=ys[k==i?j:i];ex[m]=(int32_t)xs[k]*65536+32768;es[m]=((int32_t)xs[k==i?j:i]-xs[k])*65536/(ey1[m]-ey0[m]);m++;
    }
    if(y0<CLIP_TOP)y0=CLIP_TOP;if(y1>CLIP_BOT)y1=CLIP_BOT;
    for(y=y0;y<y1;y++){
        int cx[16],nx=0;
        for(i=0;i<m;i++)if(y>=ey0[i]&&y<ey1[i])cx[nx++]=(int)((ex[i]+es[i]*(y-ey0[i]))>>16);
        for(i=1;i<nx;i++){int v=cx[i];for(k=i;k>0&&cx[k-1]>v;k--)cx[k]=cx[k-1];cx[k]=v;}
        for(k=0;k+1<nx;k+=2)rect_clip(cx[k],y,cx[k+1]-cx[k]+1,1,c);
    }
}
static void poly3(const v3 *in,int n,uint16_t c){
    v3 a[16],b[16];int p,i,m,xs[16],ys[16];
    for(i=0;i<n;i++)a[i]=in[i];
    for(p=0;p<plane_count()&&n>=3;p++){
        for(i=m=0;i<n;i++){const v3 *u=&a[i],*v=&a[i+1==n?0:i+1];int32_t du=plane_d(u,p),dv=plane_d(v,p);if(du>=0)b[m++]=*u;if((du>=0)!=(dv>=0))b[m++]=lerp3(u,v,du,dv);}
        for(i=0;i<m;i++)a[i]=b[i];n=m;
    }
    if(n<3)return;
    for(i=0;i<n;i++)proj(&a[i],&xs[i],&ys[i]);
    fill_poly(xs,ys,n,ci(c));
}
static void line2(int x0,int y0,int x1,int y1,uint16_t c){
    int dx=x1-x0,dy=y1-y0,n=absi(dx)>absi(dy)?absi(dx):absi(dy),i;uint8_t ic=ci(c);int32_t x=(int32_t)x0*65536+32768,y=(int32_t)y0*65536+32768,sx,sy;
    if(n>1200)return;if(!n)n=1;sx=(int32_t)dx*65536/n;sy=(int32_t)dy*65536/n;
    for(i=0;i<=n;i++){int px=(int)(x>>16),py=(int)(y>>16);if(px>=0&&px<W&&py>=CLIP_TOP&&py<CLIP_BOT)prg32_gfx_pixel_indexed(px,py,ic);x+=sx;y+=sy;}
}
static void line3(v3 a,v3 b,uint16_t c){
    int p,x0,y0,x1,y1;
    for(p=0;p<plane_count();p++){int32_t da=plane_d(&a,p),db=plane_d(&b,p);if(da<0&&db<0)return;if(da<0)a=lerp3(&a,&b,da,db);else if(db<0)b=lerp3(&b,&a,db,da);}
    proj(&a,&x0,&y0);proj(&b,&x1,&y1);line2(x0,y0,x1,y1,c);
}
static void wline(int x0,int y0,int x1,int y1,uint16_t c){line3(cam_pt(x0*10,y0*10,0),cam_pt(x1*10,y1*10,0),c);}

/* ------------------------------------------------------------- scenery -- */
static void panorama(int top,int off){
    unsigned scene=(unsigned)(race_no%RACES),i=fairwind_background_offsets[scene],end=fairwind_background_offsets[scene+1];int x=0,y=top;
    while(i<end){int count=fairwind_background_rle8[i++],a=x+off,b=x+off+count;uint8_t color=bg_idx[fairwind_background_rle8[i++]];if(a<0)a=0;if(b>W)b=W;if(b>a)prg32_gfx_rect_indexed(a,y,b-a,1,color);x+=count;if(x>=320){x=0;y++;}}
}
/* The Gulf opens to the SW: open horizon upwind, the venue's panorama dead
   downwind (1:1 at 150 px per radian) and low coastal hills in between. */
static void draw_horizon(void){
    int sx,off=(int)((int16_t)(32768u-cam_h))*FOCAL/10430;
    fill(0,18,W,20,SKY1);fill(0,38,W,20,SKY2);fill(0,58,W,HOR-58,SKY3);
    for(sx=0;sx<W;sx+=4){uint16_t wb=(uint16_t)(cam_h+(sx+2-160)*10430/FOCAL);int d=absi((int16_t)(wb-32768u)),hh;if(d<DEG(61)||d>DEG(128))continue;hh=6+(int)((fsin((uint16_t)(wb*3u))+fsin((uint16_t)(wb*7u+9000u))/2)>>12);fill(sx,HOR-hh,4,hh,HILL);}
    if(off>-W&&off<W)panorama(HOR-24,off);
    for(sx=HOR;sx<CLIP_BOT;sx++){int f=(sx-HOR)*48/(CLIP_BOT-HOR),d=(sx*5)&15;fill(0,sx,W,1,f<4?SEA1:f<16?(d<f-4?SEA3:SEA2):(d<(f-16)/2?SEA4:SEA3));}
}
static const int16_t shore_h[9]={200,260,180,320,280,220,300,240,210};
static void shore_wall(int x0,int y0,int x1,int y1,uint16_t c,int seed){
    int k;for(k=0;k<8;k++){
        int32_t ax=(x0+(x1-x0)*k/8)*10,ay=(y0+(y1-y0)*k/8)*10,bx=(x0+(x1-x0)*(k+1)/8)*10,by=(y0+(y1-y0)*(k+1)/8)*10;v3 q[4];
        q[0]=cam_pt(ax,ay,0);q[1]=cam_pt(bx,by,0);q[2]=cam_pt(bx,by,shore_h[(k+seed+1)%9]);q[3]=cam_pt(ax,ay,shore_h[(k+seed)%9]);
        poly3(q,4,(k&1)?c:(uint16_t)(c&0xe79c));line3(q[0],q[1],SAND);
    }
}
static void ground(int x0,int y0,int x1,int y1){v3 q[4];q[0]=cam_pt(x0*10,y0*10,0);q[1]=cam_pt(x1*10,y0*10,0);q[2]=cam_pt(x1*10,y1*10,0);q[3]=cam_pt(x0*10,y1*10,0);poly3(q,4,LAND_GROUND);}
/* Naples to leeward, Sorrento's tufa to port and Posillipo's green to
   starboard of the upwind course; the yachts cannot sail onto any of it. */
static void draw_land(void){
    if(top_view_mode){ground(-2500,-2500,-FIELD_X,FIELD_Y1);ground(FIELD_X,-2500,2500,FIELD_Y1);ground(-FIELD_X,-2500,FIELD_X,FIELD_Y0);wline(-FIELD_X,FIELD_Y1,FIELD_X,FIELD_Y1,GOLD);return;}
    shore_wall(-FIELD_X,FIELD_Y1,-FIELD_X,FIELD_Y0,C6(4,3,2),0);shore_wall(-FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y0,C6(4,4,3),3);shore_wall(FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y1,C6(1,3,1),5);
}
/* World-anchored ripples drift downwind and give the eye speed and depth. */
static void draw_ripples(void){
    int i,j,sx,sy,w;uint8_t ir=ci(RIPPLE),iff=ci(FOAM);int32_t cx,cy,drift=((int32_t)wind_t*6+(int32_t)(frame/4))%50000,dx=-(drift*fsin(twd))/16384,dy=-(drift*fcos(twd))/16384,gx,gy;
    cx=cam_wx;cy=cam_wy;if(!top_view_mode){cx+=(1500*cam_s)>>14;cy+=(1500*cam_c)>>14;}
    gx=(cx-dx)/250;gy=(cy-dy)/250;
    for(i=-7;i<=7;i++)for(j=-7;j<=7;j++){
        uint32_t h=(uint32_t)(gx+i)*73856093u^(uint32_t)(gy+j)*19349663u;int32_t wx=(gx+i)*250+dx+(int32_t)(h>>8)%200,wy=(gy+j)*250+dy+(int32_t)(h>>16)%200;v3 p=cam_pt(wx,wy,0);
        if(plane_d(&p,0)<30||plane_d(&p,1)<0||plane_d(&p,2)<0)continue;
        if(!top_view_mode&&p.z>4500)continue;
        proj(&p,&sx,&sy);w=top_view_mode?6:2+(int)(1200/p.z);rect_clip(sx-w/2,sy,w,1,(h&3)?ir:iff);
    }
}

/* -------------------------------------------------------------- yachts -- */
static const boat_t *bb_;static int32_t bs_,bc_;
static v3 bpt(int lx,int ly,int z){int32_t wx,wy;lx+=z*bb_->heel/57;wx=bb_->x/100+((lx*bc_+ly*bs_)>>14);wy=bb_->y/100+((ly*bc_-lx*bs_)>>14);return cam_pt(wx,wy,z);}
static void use_boat(const boat_t *b){bb_=b;bs_=fsin(b->heading);bc_=fcos(b->heading);}
static const int8_t hull_lx[7]={0,17,18,13,-13,-18,-17};static const int16_t hull_ly[7]={106,55,-30,-98,-98,-30,55};
static void hull_poly(int z,int pct,uint16_t c){v3 p[7];int i;for(i=0;i<7;i++)p[i]=bpt(hull_lx[i]*pct/100,hull_ly[i]*pct/100,z);poly3(p,7,c);}
static void seg(int ax,int ay,int az,int bx,int by,int bz,uint16_t c){line3(bpt(ax,ay,az),bpt(bx,by,bz),c);}
/* A point's offset from a stowed kite at the bow grows with hoist progress. */
static v3 kpt(int x,int y,int z,int k){return bpt(x*k/255,90+(y-90)*k/255,14+(z-14)*k/255);}
static void draw_main(void){
    int b=bb_->boom,side=b>=0?1:-1,cx,cy,belly=12,ex,ey,luff=trim_eff(bb_)<45&&absi(b)>trim_opt(awa_deg(bb_));int32_t s=fsin((uint16_t)DEG(b)),c=fcos((uint16_t)DEG(b));v3 p[5];
    if(luff)belly=(frame&4)?10:-6;
    cx=(int)((BOOM_LEN*s)>>14);cy=MAST_Y-(int)((BOOM_LEN*c)>>14);ex=(int)((side*belly*c)>>14);ey=(int)((side*belly*s)>>14);
    p[0]=bpt(0,MAST_Y,BOOM_Z);p[1]=bpt(0,MAST_Y,MAST_TOP);p[2]=bpt(cx/2+ex,(MAST_Y+cy)/2+ey,(MAST_TOP+BOOM_Z)/2-10);p[3]=bpt(cx,cy,BOOM_Z);p[4]=bpt(cx/2+ex*2/3,(MAST_Y+cy)/2+ey*2/3,BOOM_Z);
    poly3(p,5,luff&&(frame&2)?CLOTH_LUFF:DECK);
    seg(0,MAST_Y,12,0,MAST_Y,MAST_TOP+8,SPAR);seg(0,MAST_Y,BOOM_Z,cx,cy,BOOM_Z,SPAR);
}
static void draw_fore(void){
    int kp=bb_->kite==KITE_NONE?0:bb_->kite_prog>>8,sd=bb_->boom>=0?1:-1,awa=awa_deg(bb_),i;v3 p[8],q[8];
    if(kp<255){
        int jf=255-kp,j=clamp(bb_->boom*55/100,-32,32),hx=104+(MAST_Y+4-104)*jf/255,hz=14+191*jf/255,cx,cy;int32_t s=fsin((uint16_t)DEG(j)),c=fcos((uint16_t)DEG(j));
        cx=(int)((70*s)>>14)*jf/255;cy=104-(int)((70*c)>>14)*jf/255;
        p[0]=bpt(0,104,14);p[1]=bpt(0,hx,hz);p[2]=bpt(cx/2+sd*8,(hx+cy)/2,hz/2+8);p[3]=bpt(cx,cy,16);
        poly3(p,4,CLOTH);
    }
    if(kp>0){
        uint16_t col=teams[bb_->team&3].kite;
        if(bb_->kite==KITE_SPIN){
            int pa=clamp(awa-80,12,88),ca=clamp(awa-50,25,100),tx=-sd*(int)((55*fsin((uint16_t)DEG(pa)))>>14),ty=MAST_Y+(int)((55*fcos((uint16_t)DEG(pa)))>>14),cx=sd*(int)((62*fsin((uint16_t)DEG(ca)))>>14),cy=MAST_Y+(int)((62*fcos((uint16_t)DEG(ca)))>>14);
            /* A symmetric kite: rounded head, full shoulders, pole to windward. */
            p[0]=kpt(0,MAST_Y+12,250,kp);p[1]=kpt(cx*3/4,cy+26,228,kp);p[2]=kpt(cx*118/100,cy+36,165,kp);p[3]=kpt(cx,cy,30,kp);p[4]=kpt((tx+cx)/2,(ty>cy?ty:cy)+42,45,kp);p[5]=kpt(tx,ty,28,kp);p[6]=kpt(tx*118/100,ty+36,165,kp);p[7]=kpt(tx*3/4,ty+26,228,kp);
        }else{
            int ga=clamp(150-awa,60,110),cx=sd*(int)((55*fsin((uint16_t)DEG(ga)))>>14),cy=MAST_Y+(int)((55*fcos((uint16_t)DEG(ga)))>>14);
            /* Asymmetric: tacked at the bow, luff curling to leeward. */
            p[0]=kpt(0,MAST_Y+10,240,kp);p[1]=kpt(sd*16,70,200,kp);p[2]=kpt(sd*28,90,130,kp);p[3]=kpt(0,112,16,kp);p[4]=kpt(sd*30,(112+cy)/2+10,28,kp);p[5]=kpt(cx,cy,30,kp);p[6]=kpt(cx*3/4+sd*14,(MAST_Y+cy)/2+8,120,kp);p[7]=kpt(cx/3+sd*10,MAST_Y+14,200,kp);
        }
        poly3(p,8,col);
        for(i=0;i<8;i++){q[i].x=(p[i].x+(p[0].x+p[4].x)*2)/5;q[i].h=(p[i].h+(p[0].h+p[4].h)*2)/5;q[i].z=(p[i].z+(p[0].z+p[4].z)*2)/5;}
        poly3(q,8,CLOTH);
    }
}
static void draw_boat(const boat_t *b){
    int32_t v=b->v>>8;int len=(int)(v/40);
    use_boat(b);
    if(len>8){v3 w[4];w[0]=bpt(-11,-94,0);w[1]=bpt(11,-94,0);w[2]=bpt(len/10,-96-len,0);w[3]=bpt(-len/10,-96-len,0);poly3(w,4,SEA1);seg(-8,-96,0,-len/10,-96-len,0,RIPPLE);seg(8,-96,0,len/10,-96-len,0,RIPPLE);}
    hull_poly(0,100,HULL_RED);hull_poly(12,100,teams[b->team&3].kite);hull_poly(13,72,DECK);
    /* Paint the farther of mainsail and headsail first. */
    if(bpt(0,MAST_Y-40,120).z>bpt(0,80,120).z){draw_main();draw_fore();}else{draw_fore();draw_main();}
}
static void draw_buoy(int x,int y,int next){
    v3 base=cam_pt(x*10,y*10,0),top=cam_pt(x*10,y*10,26);int sx,sy,tx,ty,w;
    if(plane_d(&base,0)<0)return;
    proj(&base,&sx,&sy);proj(&top,&tx,&ty);w=top_view_mode?3:1+(int)(10*FOCAL/base.z);if(sx<-20||sx>W+20)return;
    if(ty>sy-2)ty=sy-2;rect_clip(sx-w,ty,2*w,sy-ty+1,ci(BUOY));rect_clip(sx-w,(ty+sy)/2,2*w,1,1);
    if(next&&(frame&16)){uint8_t g=ci(GOLD);rect_clip(sx-w-3,ty-3,2*w+6,1,g);rect_clip(sx-w-3,sy+2,2*w+6,1,g);rect_clip(sx-w-3,ty-3,1,sy-ty+6,g);rect_clip(sx+w+2,ty-3,1,sy-ty+6,g);}
    if(next&&ty-12>CLIP_TOP&&sx>16&&sx<W-16)prg32_gfx_text8(sx-15,ty-12,"NEXT",YELLOW,PANEL);
    if(next&&top_view_mode){int i;for(i=0;i<32;i++){v3 z=cam_pt(x*10+((MARK_ZONE*10*fsin((uint16_t)(i*2048)))>>14),y*10+((MARK_ZONE*10*fcos((uint16_t)(i*2048)))>>14),0);int zx,zy;if(plane_d(&z,0)<0||plane_d(&z,1)<0||plane_d(&z,2)<0||plane_d(&z,3)<0)continue;proj(&z,&zx,&zy);rect_clip(zx,zy,1,1,ci(GOLD));}}
}
/* Committee boat with flag hoist: orange line flag, class "12" to the start,
   then International Code P between the preparatory signal and one minute. */
static void draw_committee(int x,int y,int finish){
    v3 q[4];int i,n=0;uint16_t flag[3];
    memset(&prop,0,sizeof prop);prop.x=x*1000;prop.y=y*1000;use_boat(&prop);
    hull_poly(0,65,WHITE);hull_poly(10,65,C6(4,4,4));hull_poly(30,30,WHITE);seg(0,-10,30,0,-10,110,SPAR);
    flag[n++]=finish?C6(0,0,5):BUOY;if(!finish&&start_clock>0)flag[n++]=WHITE;if(!finish&&start_clock<=240&&start_clock>60)flag[n++]=C6(0,1,4);
    for(i=0;i<n;i++){int z=105-i*22;q[0]=bpt(0,-10,z);q[1]=bpt(24,-10,z);q[2]=bpt(24,-10,z-16);q[3]=bpt(0,-10,z-16);poly3(q,4,flag[i]);}
}
static void draw_lines(void){
    uint16_t fin=boats[0].leg>=course_len[course_sel]?GOLD:WHITE;
    if(start_line_active){
        wline(-LINE_HALF,0,LINE_HALF,0,WHITE);wline(-BOX_HALF,0,-LINE_HALF,0,RED);wline(LINE_HALF,0,BOX_HALF,0,GREEN);
        wline(-BOX_HALF,0,-BOX_HALF,-BOX_DEPTH,GREY);wline(BOX_HALF,0,BOX_HALF,-BOX_DEPTH,GREY);wline(-BOX_HALF,-BOX_DEPTH,BOX_HALF,-BOX_DEPTH,GREY);
    }else wline(-FINISH_HALF,FINISH_Y,FINISH_HALF,FINISH_Y,fin);
}
typedef struct {int32_t z;int8_t kind,idx;} item_t;
static void draw_objects(void){
    item_t it[12],t;int n=0,i,j,len=course_len[course_sel];
    for(i=0;i<4;i++){it[n].z=cam_pt(boats[i].x/100,boats[i].y/100,0).z;it[n].kind=0;it[n++].idx=(int8_t)i;}
    for(i=0;i<len;i++){it[n].z=cam_pt(course_x[course_sel][i]*10,course_y[course_sel][i]*10,0).z;it[n].kind=1;it[n++].idx=(int8_t)i;}
    it[n].z=cam_pt((start_line_active?LINE_HALF+8:FINISH_HALF+8)*10,(start_line_active?0:FINISH_Y)*10,0).z;it[n].kind=2;it[n++].idx=0;
    it[n].z=cam_pt((start_line_active?-LINE_HALF:-FINISH_HALF)*10,(start_line_active?0:FINISH_Y)*10,0).z;it[n].kind=3;it[n++].idx=0;
    for(i=1;i<n;i++){t=it[i];for(j=i;j>0&&it[j-1].z<t.z;j--)it[j]=it[j-1];it[j]=t;}
    for(i=0;i<n;i++){
        if(!top_view_mode&&it[i].z<-300)continue;
        if(it[i].kind==0)draw_boat(&boats[it[i].idx]);
        else if(it[i].kind==1)draw_buoy(course_x[course_sel][it[i].idx],course_y[course_sel][it[i].idx],boats[0].started&&boats[0].leg==it[i].idx);
        else if(it[i].kind==2)draw_committee(start_line_active?LINE_HALF+8:FINISH_HALF+8,start_line_active?0:FINISH_Y,!start_line_active);
        else draw_buoy(start_line_active?-LINE_HALF:-FINISH_HALF,start_line_active?0:FINISH_Y,!start_line_active&&boats[0].leg>=len);
    }
}

/* ----------------------------------------------------------------- HUD -- */
/* Wind instrument: true wind (gold) and apparent wind (cyan) relative to the
   bow, true wind speed, and the current shift as a lift or header. */
static void draw_wind_gauge(void){
    const boat_t *b=&boats[0];int i,shift=wind_shift_deg(),upwind=absi(b->twa)<DEG(90);uint16_t rel=(uint16_t)(twd-b->heading);
    fill(2,20,58,62,PANEL);
    for(i=0;i<16;i++)fill(31+(int)((22*fsin((uint16_t)(i*4096)))>>14),46-(int)((22*fcos((uint16_t)(i*4096)))>>14),1,1,GREY);
    fill(30,40,3,13,WHITE);fill(31,38,1,2,WHITE);
    line2(31+(int)((22*fsin(rel))>>14),46-(int)((22*fcos(rel))>>14),31+(int)((8*fsin(rel))>>14),46-(int)((8*fcos(rel))>>14),GOLD);
    fill(30+(int)((8*fsin(rel))>>14),45-(int)((8*fcos(rel))>>14),3,3,GOLD);
    fill(30+(int)((18*fsin((uint16_t)b->awa))>>14),45-(int)((18*fcos((uint16_t)b->awa))>>14),3,3,CYAN);
    small_num(4,72,tws10,1,WHITE,PANEL);
    if(upwind&&absi(shift)>=3){int lift=shift*b->tack>0;prg32_gfx_text8(38,72,lift?"L":"H",lift?GREEN:RED,PANEL);digit(46,72,absi(shift),lift?GREEN:RED,PANEL);}
}
/* North is not up: the map, like the course, puts the SW breeze at the top. */
static int map_x(int32_t x){return 257+(int)((x+FIELD_X)/30);}
static int map_y(int32_t y){return 24+(int)((FIELD_Y1-y)/30);}
static void draw_minimap(void){
    int i,len=course_len[course_sel],nx,ny,nl=boats[0].leg;
    fill(254,20,66,66,PANEL);fill(257,24,60,57,SEA2);fill(254,20,3,66,C6(4,3,2));fill(317,20,3,66,C6(1,3,1));fill(257,81,60,5,C6(4,4,3));
    for(i=258;i<316;i+=4)fill(i,23,2,1,GOLD);
    if(start_line_active)fill(map_x(-LINE_HALF),map_y(0),map_x(LINE_HALF)-map_x(-LINE_HALF),1,WHITE);else fill(map_x(-FINISH_HALF),map_y(FINISH_Y),map_x(FINISH_HALF)-map_x(-FINISH_HALF),1,nl>=len?GOLD:WHITE);
    for(i=0;i<len;i++)fill(map_x(course_x[course_sel][i])-1,map_y(course_y[course_sel][i])-1,2,2,BUOY);
    if(nl<len){
        nx=map_x(course_x[course_sel][nl]);ny=map_y(course_y[course_sel][nl]);
        if(frame&16)fill(nx-2,ny-2,4,4,GOLD);
        /* Strategy upgrades plot laylines to a windward mark. */
        if(strategy>=2&&absi(BAM2DEG((int16_t)(bearing(course_x[course_sel][nl]*1000-boats[0].x,course_y[course_sel][nl]*1000-boats[0].y)-twd)))<60)
            for(i=-1;i<=1;i+=2){uint16_t a=(uint16_t)(twd+32768u+i*DEG(42));line2(nx,ny,nx+(int)((22*fsin(a))>>14),ny-(int)((22*fcos(a))>>14),C6(3,4,5));}
    }
    for(i=3;i>=0;i--){int bx=map_x(boats[i].x/1000),by=map_y(boats[i].y/1000);if(i){fill(bx-1,by-1,2,2,teams[boats[i].team&3].kite);continue;}line2(bx,by,bx+(int)((5*fsin(boats[0].heading))>>14),by-(int)((5*fcos(boats[0].heading))>>14),WHITE);fill(bx-1,by-1,3,3,WHITE);}
    line2(262+(int)((5*fsin(twd))>>14),76-(int)((5*fcos(twd))>>14),262,76,GOLD);fill(261,75,3,3,GOLD);
}
/* Short signal banners: the HUD already counts down, and every text pixel is
   converted by the firmware, so the banner names only the flag in force. */
static void draw_start_signal(void){const char *msg;if(start_clock>300)msg="WARNING";else if(start_clock>240)msg="CLASS FLAG";else if(start_clock>120)msg="P FLAG UP";else if(start_clock>60)msg="ENTER BOX";else if(start_clock>0)msg="P FLAG DOWN";else msg="START";fill(112,20,96,12,PANEL);prg32_gfx_text8(160-4*slen(msg),22,msg,start_clock?WHITE:YELLOW,PANEL);if(time_scale()>RACE_TIME_SCALE)prg32_gfx_text8(148,34,"x20",YELLOW,PANEL);}
static void draw_hud(void){
    const boat_t *b=&boats[0];int len=course_len[course_sel],opt=trim_opt(awa_deg(b)),err=absi(b->boom)-opt,kp=b->kite_prog*100/65535;const char *msg=0;uint16_t mc=YELLOW;
    fill(0,CLIP_BOT,W,H-CLIP_BOT,PANEL);
    prg32_gfx_text8(4,182,start_clock?"T-":"R",start_clock?YELLOW:WHITE,PANEL);clock_mmss(20,182,start_clock?start_clock:race_clock,start_clock?YELLOW:WHITE,PANEL);
    small_num(68,182,(int)((b->v>>8)*100/5144),1,WHITE,PANEL);prg32_gfx_text8(100,182,"KT",CYAN,PANEL);
    prg32_gfx_text8(122,182,"TWA",CYAN,PANEL);small_num(148,182,absi(BAM2DEG(b->twa)),0,WHITE,PANEL);
    prg32_gfx_text8(180,182,b->kite==KITE_SPIN?"SPI":b->kite==KITE_GENN?"GEN":"JIB",b->kite?teams[b->team&3].kite:WHITE,PANEL);
    if(b->kite!=b->kite_want||(b->kite&&kp<100)){small_num(206,182,kp,0,YELLOW,PANEL);prg32_gfx_text8(230,182,b->kite!=b->kite_want?"v":"^",YELLOW,PANEL);}
    if(b->started&&b->leg<len){prg32_gfx_text8(250,182,"NEXT",YELLOW,PANEL);digit(290,182,b->leg+1,YELLOW,PANEL);}else if(b->started)prg32_gfx_text8(250,182,"FINISH",YELLOW,PANEL);
    fill(4,192,128,6,C6(1,1,2));fill(4+opt*14/10-4,192,9,6,C6(0,3,1));fill(4+b->sheet*14/10,197,1,2,GOLD);fill(4+clamp(absi(b->boom),0,90)*14/10,191,2,8,WHITE);
    prg32_gfx_text8(136,191,err>8?"LUFF":err<-12?"STALL":"GOOD",err>8?RED:err<-12?YELLOW:GREEN,PANEL);
    if(b->serving)msg="PENALTY TURN";
    else if(b->penalty){msg=(frame&32)?rule_names[b->rule]:"A+B PENALTY";mc=RED;}
    else if(b->aground){msg=b->y>(FIELD_Y1-20)*1000?"AREA LIMIT":"AGROUND";mc=RED;}
    else if(start_clock>120&&b->y<=15000)msg="WAIT FOR 2 MIN";
    else if(start_clock<=120&&!b->entered_box)msg=(b->team&1)?"ENTER STBD":"ENTER PORT";
    else if(!b->started&&start_clock==0)msg="CROSS LINE";
    else if(multiplayer){prg32_gfx_text8(184,191,"NET",GREEN,PANEL);digit(210,191,peer_count+1,GREEN,PANEL);prg32_gfx_text8(218,191,"/4",WHITE,PANEL);}
    if(msg)prg32_gfx_text8(184,191,msg,mc,PANEL);
}
static void draw_race(void){
    camera_setup();
    if(top_view_mode)fill(0,CLIP_TOP,W,CLIP_BOT-CLIP_TOP,SEA2);else draw_horizon();
    draw_land();draw_ripples();draw_lines();draw_objects();
    if(!race_header){header(race_names[race_no]);race_header=1;}
    draw_wind_gauge();draw_minimap();
    if(start_line_active)draw_start_signal();
    if(top_view_mode)prg32_gfx_text8(148,170,"TOP",YELLOW,PANEL);
    /* The header is static and the HUD refreshes at 7.5 Hz, so most frames
       push only rows 18-179 over SPI. */
    if(!(frame&3)||hud_force){draw_hud();hud_force=0;}
}
static void draw_result(void){int i;header("RACE RESULT");prg32_gfx_text8(20,28,"PLACE",WHITE,SEA);number(67,28,player_rank,GOLD,SEA);prg32_gfx_text8(126,28,"PRIZE K",WHITE,SEA);number(182,28,last_prize,GOLD,SEA);for(i=0;i<4;i++){const boat_t *b=&boats[result_order[i]];number(20,52+i*25,i+1,WHITE,SEA);prg32_gfx_text8(42,52+i*25,teams[b->team].name,b->team==team_sel?GOLD:WHITE,SEA);number(205,52+i*25,b->finish_time,WHITE,SEA);}prg32_gfx_text8(48,160,"SPONSOR PAYMENT RECEIVED",GREEN,SEA);prg32_gfx_text8(75,180,"A - NEXT RACE",WHITE,SEA);}
static void draw_season(void){int i,best=0;header("FINAL CLASSIFICATION");for(i=1;i<4;i++)if(points[i]>points[best])best=i;fairwind_draw_sprite(144,27,&cup_sprite,0);prg32_gfx_text8(67,70,best==team_sel?"YOU WON THE AULD MUG":"THE CUP HAS A WINNER",GOLD,SEA);for(i=0;i<4;i++){prg32_gfx_text8(62,94+i*20,teams[i].name,WHITE,SEA);number(220,94+i*20,points[i],WHITE,SEA);}prg32_gfx_text8(63,180,"A - RETURN TO TITLE",WHITE,SEA);}
/* Menus redraw only after input, a screen change, or (lobby) twice a second;
   with nothing dirty the firmware's present sends nothing over SPI. */
void fairwind_draw(void){if(screen!=ST_RACE){if(!ui_dirty&&(int)screen==drawn_screen&&!(screen==ST_LOBBY&&frame%15==0))return;ui_dirty=0;prg32_gfx_clear_indexed(ci(SEA));}else if(drawn_screen!=ST_RACE){race_header=0;hud_force=1;}drawn_screen=(int)screen;if(screen==ST_TITLE)draw_title();else if(screen==ST_MODE)draw_mode();else if(screen==ST_TEAM)draw_team();else if(screen==ST_MANAGER)draw_manager();else if(screen==ST_BRIEF)draw_brief();else if(screen==ST_LOBBY)draw_lobby();else if(screen==ST_RACE)draw_race();else if(screen==ST_RESULT)draw_result();else draw_season();prg32_gfx_present();}
