#include "platform.h"
#include "game.h"
#include "assets.h"
#include "fixmath.h"
#include <stddef.h>
#include <stdint.h>
#define W 320
#define H 200
#define RACES 5
#define START_COUNTDOWN_SECONDS 600
#define RACE_LIMIT_SECONDS 2700
/* Simulated seconds per real second: fast-forward until the box opens, then
   a 4x race clock so a 12-Metre's 8 knots reads as 8 knots on screen. */
#define FAST_TIME_SCALE 20
#define RACE_TIME_SCALE 4
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
#define SHEER_Z 14
/* Chase camera in decimetres and pixels. */
#define FOCAL 150
#define CAM_BACK 300
#define CAM_H 65
#define NEAR_DM 20
#define TOP_S 15
#define TOP_TILT 3
#define TOP_CY 140
/* Buttons: only the digital joystick, A and B. A or B held this long is a
   "hold" (penalty turn, race menu); shorter is a tap (spinnaker, gennaker).
   A chord of A and B means nothing: the firmware restarts on A+B+DOWN. */
#define HOLD_FRAMES 24
#define TOAST_FRAMES 110
/* Sound: songs own voices 0..7 through the tracker; a race is silent of
   music and the game plays these voices itself (instruments 8 and up). */
enum {SND_TITLE,SND_HQ,SND_PODIUM,SND_RESULT,SND_SEASON,SND_LOBBY};
enum {V_WIND,V_WATER,V_SURF,V_FLAP,V_WINCH,V_THUMP,V_HORN,V_BELL};
enum {I_WIND=8,I_WATER,I_SURF,I_HORN,I_WINCH,I_FLAP,I_THUMP,I_BUZZ,I_CHIME,I_CLICK,I_GUN,I_WHOOSH};
/* Events a yacht raises for the presentation layer (sound, toasts). */
#define EV_MARK 1
#define EV_FINISH 2
#define EV_PENALTY 4
#define EV_CONTACT 8
#define EV_GYBE 16
#define EV_TACK 32
#define EV_CLEARED 64
typedef enum {ST_TITLE,ST_MODE,ST_TEAM,ST_MANAGER,ST_BRIEF,ST_LOBBY,ST_RACE,ST_RESULT,ST_SEASON} screen_t;
typedef enum {RULE_NONE,RULE_PORT,RULE_WINDWARD,RULE_ASTERN,RULE_TACKING,RULE_CONTACT,RULE_MARK_ROOM,RULE_MARK_TOUCH} rule_t;
enum {KITE_NONE,KITE_SPIN,KITE_GENN};
typedef struct {char name[18],code[4];uint8_t speed,turn,crew;} team_t;
typedef struct {
    int32_t x,y,v,aws,tack_start,ai_last_tack;   /* mm, mm, mm/s<<8, mm/s, sim s */
    uint32_t turned;                              /* penalty-turn progress, bam */
    uint16_t heading,kite_prog,finish_time,ai_clock;
    int16_t rudder,boom,twa,awa,hacc;             /* boom in degrees, + = starboard */
    int8_t tack,heel,serve_dir,ai_tack;
    uint8_t team,leg,finished,penalty,serving,kite,kite_want,sheet,started,start_ready,entered_box,tacking,rule,mark_touch,touching,aground,ai_above,evt,retired;
} boat_t;
typedef struct {char name[18];int fee,win_bonus;uint8_t min_wins;} sponsor_t;
typedef struct {int32_t x,h,z;} v3;
/* The view the 3D renderer draws into: projection centre, horizon row and
   clip box. Races use the whole game area; menus use small windows. */
typedef struct {int cx,hor,x0,y0,x1,y1;} view_t;
/* GCC may lower small descriptor assignments to memcpy/memset even under
   -ffreestanding.  Portable cartridges do not link a C library. */
void *memcpy(void *dst,const void *src,size_t size){volatile uint8_t *d=(volatile uint8_t *)dst;const volatile uint8_t *s=(const volatile uint8_t *)src;size_t i;for(i=0;i<size;i++)d[i]=s[i];return dst;}
void *memset(void *dst,int value,size_t size){volatile uint8_t *d=(volatile uint8_t *)dst;size_t i;for(i=0;i<size;i++)d[i]=(uint8_t)value;return dst;}
static const team_t teams[4]={{"PARTENOPE 12","ITA",6,7,7},{"LIBECCIO CORSE","ITA",7,6,6},{"ATLANTIC UNION","USA",7,7,5},{"SOUTHERN CROSS","AUS",6,6,8}};
static const uint8_t team_c[4]={C_TEAM0,C_TEAM1,C_TEAM2,C_TEAM3},team_d[4]={C_TEAMD0,C_TEAMD1,C_TEAMD2,C_TEAMD3};
static const uint16_t team_tx[4]={T_TEAM0,T_TEAM1,T_TEAM2,T_TEAM3};
static const sponsor_t sponsors[4]={{"GULF MARINE",18,7,0},{"VESUVIO TECH",26,11,1},{"CAPRI LUX",38,16,2},{"PARTENOPE GLOBAL",55,24,3}};
static const char race_names[RACES][16]={"SANTA LUCIA","VESUVIUS CUP","CAPRI PASSAGE","SORRENTO RACE","NAPLES FINAL"};
static const char race_light[RACES][13]={"MORNING","LATE MORNING","MIDDAY","AFTERNOON","SUNSET"};
static const char course_names[COURSE_COUNT][20]={"WINDWARD / RUN","OLYMPIC TRIANGLE","1992 IACC Z"};
static const char rule_names[8][20]={"CLEAR","PORT / STARBOARD","WINDWARD / LEEWARD","CLEAR ASTERN","WHILE TACKING","AVOID CONTACT","MARK ROOM","TOUCHED MARK"};
static const uint8_t course_len[COURSE_COUNT]={3,5,6};
/* Mark positions in metres: x across the course, y upwind of the start. */
static const int16_t course_x[COURSE_COUNT][COURSE_MARKS_MAX]={{0,0,0,0,0,0},{0,-360,0,0,0,0},{0,360,0,0,-360,0}};
static const int16_t course_y[COURSE_COUNT][COURSE_MARKS_MAX]={{800,120,800,0,0,0},{800,450,120,800,120,0},{800,450,120,800,450,800}};
static boat_t boats[4],prop,show;static screen_t screen;
static view_t vw={160,76,0,18,W,180};
static uint32_t cur_input,last_input,frame,rng=0x97ca1997u,wind_seed;
static int team_sel,menu,points[4],race_no,money,hull,sails,crew,strategy,sponsor,wins,last_prize,tws10,race_clock,start_clock,race_frames,course_sel,race_ticks;
static int32_t wind_t;static uint16_t twd,cam_h;
static uint8_t multiplayer,peer_count,player_rank,local_ready,start_line_active,top_view_mode,result_order[4],net_live[4],race_team,auto_trim=1;
static uint32_t net_ids[4];
/* Button holds, the race menu and the presentation layer. */
static uint8_t hold_a,hold_b,a_used,b_used,race_menu,race_menu_sel,manual_trim,horn_frames,buzz_frames,chime_frames,flap_frames,last_sheet;
static int toast_frames;static const char *toast_msg;static uint16_t toast_fg;
static int surf_timer;
/* Remote yachts: last snapshot (decimetres), its age in frames, P-tags. */
static int16_t net_rx[4],net_ry[4];static uint16_t net_age[4];static uint8_t net_tag[4],my_tag;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}static int absi(int n){return n<0?-n:n;}static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
#define HOR (vw.hor)
#define CLIP_TOP (vw.y0)
#define CLIP_BOT (vw.y1)

/* ---------------------------------------------------------------- text -- */
static void number(int x,int y,int n,uint16_t fg,uint16_t bg){char b[8];int i=6;b[7]=0;if(n<0)n=0;do{b[i--]=(char)('0'+n%10);n/=10;}while(n&&i>=0);while(i>=0)b[i--]=' ';gx_text(x,y,b,fg,bg);}
static void digit(int x,int y,int n,uint16_t fg,uint16_t bg){char b[2];b[0]=(char)('0'+clamp(n,0,9));b[1]=0;gx_text(x,y,b,fg,bg);}
static void clock_mmss(int x,int y,int n,uint16_t fg,uint16_t bg){char b[6];int m;if(n<0)n=0;m=clamp(n/60,0,99);b[0]=(char)('0'+m/10);b[1]=(char)('0'+m%10);b[2]=':';b[3]=(char)('0'+(n%60)/10);b[4]=(char)('0'+n%10);b[5]=0;gx_text(x,y,b,fg,bg);}
/* Left-aligned integer, optionally with one decimal (n in tenths). */
static void small_num(int x,int y,int n,int tenths,uint16_t fg,uint16_t bg){char b[8];int i=0,d=1;if(n<0)n=0;if(n>9999)n=9999;while(n/d>=10)d*=10;if(tenths&&d<10)d=10;for(;d;d/=10){if(tenths&&d==1)b[i++]='.';b[i++]=(char)('0'+n/d%10);}b[i]=0;gx_text(x,y,b,fg,bg);}
static int text_len(const char *s){int n=0;while(s[n])n++;return n;}
static void text_c(int y,const char *s,uint16_t fg,uint16_t bg){gx_text(160-text_len(s)*4,y,s,fg,bg);}

/* --------------------------------------------------------------- palette -- */
static uint8_t loaded_venue=255,glint_phase=255;
static void load_fixed_palette(void){int i;for(i=0;i<FIXED_COLOURS;i++)gx_palette(fixed_slot[i],fixed_rgb[i]);}
/* A venue is a whole lighting setup: sky, sea, haze, sun, glints and its
   panorama colours, graded to the time of day of that race. */
static void load_venue(int v){int i;v=clamp(v,0,RACES-1);if(loaded_venue==v)return;loaded_venue=(uint8_t)v;glint_phase=255;for(i=0;i<VENUE_COLOURS;i++)gx_palette(venue_slot[i],venue_rgb[v][i]);}
/* Indexed-colour shimmer: the four glint entries rotate, so every glint on
   the water sparkles without being redrawn. */
static void cycle_glints(void){
    static const uint8_t slot[4]={C_GL0,C_GL1,C_GL2,C_GL3};uint8_t ph=(uint8_t)((frame>>2)&3);int i;
    if(ph==glint_phase||loaded_venue>=RACES)return;glint_phase=ph;
    for(i=0;i<4;i++)gx_palette(slot[i],venue_rgb[loaded_venue][VK_GL0+((i+ph)&3)]);
}
static int venue(void){return race_no%RACES;}
static int lit(void){return loaded_venue<RACES?loaded_venue:0;}

/* ---------------------------------------------------------------- sound -- */
static void click(void){snd_note(V_BELL,I_CLICK,84,0,0);}
static void toast(const char *msg,uint16_t fg){toast_msg=msg;toast_fg=fg;toast_frames=TOAST_FRAMES;}
static void horn(int long_signal){snd_note(V_HORN,I_HORN,long_signal?46:50,0,0);horn_frames=(uint8_t)(long_signal?54:24);}
/* The sea, the wind in the rig and the water along the hull, as two held
   noise voices whose level and pan follow the apparent wind and the speed. */
static int trim_eff_luffing(void);
static void race_audio_start(void){
    snd_silence();snd_note(V_WIND,I_WIND,60,255,0);snd_note(V_WATER,I_WATER,48,255,0);snd_volume(V_WIND,0);snd_volume(V_WATER,0);
    horn_frames=buzz_frames=chime_frames=flap_frames=0;surf_timer=40;last_sheet=boats[0].sheet;
}
static void race_audio_update(void){
    const boat_t *b=&boats[0];int kn=(int)((b->v>>8)/514),aws=(int)(b->aws/514),pan=b->awa>=0?36:-36;
    if((frame&3)==0){
        snd_volume(V_WIND,(uint8_t)clamp(40+aws*9,40,230));snd_pan(V_WIND,(int8_t)(top_view_mode?0:pan));
        snd_volume(V_WATER,(uint8_t)clamp(24+kn*22,24,220));
    }
    if(--surf_timer<=0){surf_timer=50+(int)(rnd()%70);snd_note(V_SURF,I_SURF,(uint8_t)(40+rnd()%10),(uint8_t)(90+tws10/2),(int8_t)((int)(rnd()%100)-50));}
    if(b->sheet!=last_sheet&&(frame&3)==0){snd_note(V_WINCH,I_WINCH,(uint8_t)(b->sheet<last_sheet?88:80),0,b->boom>=0?20:-20);last_sheet=b->sheet;}
    if(trim_eff_luffing()&&++flap_frames>=7){flap_frames=0;snd_note(V_FLAP,I_FLAP,(uint8_t)(70+rnd()%6),(uint8_t)clamp(60+aws*6,60,200),(int8_t)(b->boom>=0?-30:30));}
    if(horn_frames&&--horn_frames==0)snd_off(V_HORN);
    if(buzz_frames&&--buzz_frames==0)snd_off(V_BELL);
    if(chime_frames&&--chime_frames==8)snd_note(V_BELL,I_CHIME,91,0,0);
}

/* ---------------------------------------------------------------- wind -- */
static int time_scale(void){return start_clock>120?FAST_TIME_SCALE:RACE_TIME_SCALE;}
static int frames_per_sim_second(void){return 60/time_scale();}
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
static int trim_eff_luffing(void){const boat_t *b=&boats[0];return trim_eff(b)<45&&absi(b->boom)>trim_opt(awa_deg(b))&&b->aws>1500;}
static int crew_of(const boat_t *b){return b==&boats[0]?crew:teams[b->team].crew;}

/* ------------------------------------------------------------- physics -- */
static void begin_penalty_turn(boat_t *b){if(b->penalty&&!b->serving){b->serving=1;b->turned=0;b->serve_dir=(int8_t)(b->tack>0?1:-1);}}
static void award_penalty(boat_t *b,rule_t rule){if(!b->penalty){b->penalty=1;b->serving=0;b->turned=0;b->rule=(uint8_t)rule;b->evt|=EV_PENALTY;}}
static int clamp_field(boat_t *b){int32_t x=b->x,y=b->y,lim=(FIELD_X-12)*1000;b->x=clamp(b->x,-lim,lim);b->y=clamp(b->y,(FIELD_Y0+12)*1000,(FIELD_Y1-12)*1000);return x!=b->x||y!=b->y;}
/* Apparent wind, tack state, boom swing, kite hoist/drop and heel. Remote
   yachts run this too so their rigs agree with the local breeze. */
static void update_rig(boat_t *b,int local){
    int scale=time_scale(),awa,side,lim,step,target,cr=crew_of(b),heel;int32_t tws=tws_mms(),wf,ws;int8_t nt;
    b->twa=(int16_t)(twd-b->heading);
    wf=((tws*fcos((uint16_t)b->twa))>>14)+(b->v>>8);ws=(tws*fsin((uint16_t)b->twa))>>14;
    b->awa=(int16_t)bearing(ws,wf);b->aws=isqrt((uint32_t)(wf*wf+ws*ws));
    nt=(int8_t)(b->twa>=0?1:-1);
    if(nt!=b->tack&&absi(b->twa)<DEG(90)){b->tacking=1;b->tack_start=wind_t;b->evt|=EV_TACK;}
    else if(nt!=b->tack)b->evt|=EV_GYBE;
    b->tack=nt;
    if(b->tacking&&(absi(b->twa)>=DEG(38)||wind_t-b->tack_start>20))b->tacking=0;
    awa=awa_deg(b);side=b->awa>=0?-1:1;lim=b->sheet<awa?b->sheet:awa;target=side*lim;
    /* The wind carries the boom to leeward until the sheet stops it; a gybe
       slams it across much faster than a tack walks it over. */
    step=((b->boom<0)!=(side<0)&&absi(b->boom)>8?12:3)*scale/4;if(step<1)step=1;
    if(b->boom<target)b->boom=(int16_t)(b->boom+step>target?target:b->boom+step);else if(b->boom>target)b->boom=(int16_t)(b->boom-step<target?target:b->boom-step);
    if(local){
        int32_t up=65535L*scale/(60*(32-2*cr)),down=65535L*scale/(60*(20-cr));
        if(b->kite!=b->kite_want){if(b->kite_prog>down)b->kite_prog=(uint16_t)(b->kite_prog-down);else{b->kite_prog=0;b->kite=b->kite_want;}}
        else if(b->kite!=KITE_NONE&&b->kite_prog<65535)b->kite_prog=(uint16_t)(b->kite_prog+up>65535?65535:b->kite_prog+up);
    }
    heel=awa<100?(int)(b->aws/514)*3*(100-awa)/200*trim_eff(b)/100:0;b->heel=(int8_t)(side*clamp(heel,0,24));
}
/* Polar target speed x sail plan x trim x syndicate, reached through the
   momentum of a 26-tonne keelboat; rudder angle both turns and brakes. */
static void sail(boat_t *b){
    int scale=time_scale(),twa=absi(BAM2DEG(b->twa)),cr=crew_of(b),perf,tau;int32_t v=b->v>>8,target,old_y=b->y,q,step,drift,vv;
    if(b->finished)return;
    target=(int32_t)polar_speed10(twa,tws10)*5144/100;
    perf=b==&boats[0]?97+hull+sails+strategy/2:96+teams[b->team].speed;
    target=target*sail_plan(b,twa)/100*trim_eff(b)/100*perf/100;
    tau=target>v?11-cr/2:14;
    b->v+=((target<<8)-b->v)*scale/(60*tau);
    b->v-=(b->v/100)*absi(b->rudder)*scale/3000;if(b->v<0)b->v=0;
    v=b->v>>8;vv=v<4100?v:4100;
    /* Near a standstill the crew backs the jib, so a yacht in irons can still
       be steered off the wind rather than sitting head to wind. */
    q=(int32_t)b->rudder*(300+900*vv/4100+(vv<1500?(1500-vv)*400/1500:0))*182/100;
    q=q*scale*(92+2*cr)/100*256/6000+b->hacc;
    b->heading=(uint16_t)(b->heading+(q>>8));b->hacc=(int16_t)(q&255);
    if(b->serving){b->turned+=(uint32_t)absi((int)(q>>8));if(b->turned>=65536u){b->penalty=b->serving=0;b->turned=0;b->rule=RULE_NONE;b->evt|=EV_CLEARED;}}
    step=v*scale/60;b->x+=(step*fsin(b->heading))>>14;b->y+=(step*fcos(b->heading))>>14;
    drift=tws_mms()*15/1000*scale/60;b->x-=(drift*fsin(twd))>>14;b->y-=(drift*fcos(twd))>>14;
    /* The shore and the race-area limit are hard: a yacht driven into them
       loses all way and has to steer off. */
    b->aground=(uint8_t)clamp_field(b);if(b->aground)b->v/=2;
    if(start_clock>120&&b->y<15000){b->y=15000;b->v-=b->v/8;}
    if(start_clock<=120&&!b->entered_box&&old_y>=0&&b->y<0&&((!(b->team&1)&&b->x>=-BOX_HALF*1000&&b->x<0)||((b->team&1)&&b->x>0&&b->x<=BOX_HALF*1000)))b->entered_box=1;
    if(start_clock>0)b->start_ready=(uint8_t)(b->entered_box&&b->y<0);
    else if(!b->started){if(b->entered_box&&b->y<0)b->start_ready=1;if(b->entered_box&&b->start_ready&&old_y<0&&b->y>=0&&b->x>=-LINE_HALF*1000&&b->x<=LINE_HALF*1000)b->started=1;}
    if(b->started&&b->leg<course_len[course_sel]){int32_t dx=b->x/1000-course_x[course_sel][b->leg],dy=b->y/1000-course_y[course_sel][b->leg];if(dx*dx+dy*dy<MARK_CAPTURE*MARK_CAPTURE){b->leg++;b->evt|=EV_MARK;}}
    else if(b->started&&b->leg>=course_len[course_sel]&&old_y>=FINISH_Y*1000&&b->y<FINISH_Y*1000&&b->x>=-FINISH_HALF*1000&&b->x<=FINISH_HALF*1000){b->finished=1;b->finish_time=(uint16_t)(race_clock+b->penalty*180);b->evt|=EV_FINISH;}
}
static void slew_rudder(boat_t *b,int target,int rate){if(b->rudder<target)b->rudder=(int16_t)(b->rudder+rate>target?target:b->rudder+rate);else if(b->rudder>target)b->rudder=(int16_t)(b->rudder-rate<target?target:b->rudder-rate);}
/* Left helms to port, right to starboard; up eases and down trims sheets.
   With AUTO TRIM the crew trims to the optimum unless the player has
   touched the sheets in the last one and a half seconds. */
static void helm_player(boat_t *b,uint32_t in){
    int target=0,step=time_scale()*(6+crew)/32;
    if(b->serving)target=b->serve_dir*100;else{if(in&PRG32_BTN_LEFT)target-=100;if(in&PRG32_BTN_RIGHT)target+=100;}
    slew_rudder(b,target,12);if(step<1)step=1;
    if(in&(PRG32_BTN_UP|PRG32_BTN_DOWN))manual_trim=90;else if(manual_trim)manual_trim--;
    if(in&PRG32_BTN_UP)b->sheet=(uint8_t)clamp(b->sheet+step,0,90);
    if(in&PRG32_BTN_DOWN)b->sheet=(uint8_t)clamp(b->sheet-step,0,90);
    if((auto_trim||b->serving)&&!manual_trim){int opt=trim_opt(awa_deg(b));if(b->sheet+step<=opt)b->sheet=(uint8_t)(b->sheet+step);else if(b->sheet>=opt+step)b->sheet=(uint8_t)(b->sheet-step);}
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
    slew_rudder(b,clamp(BAM2DEG((int16_t)(want-b->heading))*5+(int)(rnd()%3)-1,-100,100),10);
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
        dx=(b->x-a->x)/100;dy=(b->y-a->y)/100;away=bearing(dx,dy);a->evt|=EV_CONTACT;b->evt|=EV_CONTACT;
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
/* Humans keep their syndicate; AI slots take the teams nobody picked. */
static void assign_ai_teams(void){int i,t,n=fairwind_net_peer_count();unsigned used=1u<<(race_team&3);prg32_multiplayer_peer_t p;for(i=0;i<n;i++)if(fairwind_net_peer(i,&p))used|=1u<<((p.flags>>3)&3);for(t=0,i=n+1;i<4&&t<4;t++)if(!((used>>t)&1))boats[i++].team=(uint8_t)t;}
static void start_race(void){
    int i;uint32_t s=0x9e3779b9u*(uint32_t)(course_sel+1)^(multiplayer?0u:(uint32_t)race_no*0x85ebca6bu);
    s^=s>>15;s*=0x2c1b3c6du;s^=s>>12;wind_seed=s;wind_t=0;update_wind();
    race_clock=0;start_clock=START_COUNTDOWN_SECONDS;race_frames=0;race_ticks=0;start_line_active=1;top_view_mode=0;
    hold_a=hold_b=0;a_used=(uint8_t)((cur_input&PRG32_BTN_A)!=0);b_used=(uint8_t)((cur_input&PRG32_BTN_B)!=0);race_menu=0;manual_trim=0;toast_frames=0;
    if(!multiplayer)race_team=(uint8_t)team_sel;
    for(i=0;i<4;i++){net_live[i]=0;net_tag[i]=0;boats[i].team=(uint8_t)((race_team+i)%4);}
    if(multiplayer)assign_ai_teams();
    for(i=0;i<4;i++){
        boat_t *b=&boats[i];uint8_t team=b->team;int side=(team&1)?1:-1;
        memset(b,0,sizeof *b);b->team=team;
        b->x=side*(LINE_HALF+60+20*(team>>1))*1000;b->y=(90+30*(team>>1))*1000;
        b->heading=(uint16_t)(side>0?DEG(300):DEG(60));b->sheet=90;b->tack=(int8_t)((int16_t)(twd-b->heading)>=0?1:-1);b->ai_last_tack=-100;
        update_rig(b,1);b->boom=(int16_t)(b->awa>=0?-awa_deg(b):awa_deg(b));b->evt=0;
    }
    cam_h=boats[0].heading;screen=ST_RACE;race_audio_start();horn(0);toast("10 MINUTES - WARNING",T_GOLD);
}
/* Points and prizes. A human who was moved to another syndicate in the lobby
   still scores for the one chosen in this campaign. */
static void score_race(void){
    int i,j;for(i=0;i<4;i++)result_order[i]=(uint8_t)i;
    for(i=0;i<4;i++)for(j=i+1;j<4;j++)if(boats[result_order[j]].finish_time<boats[result_order[i]].finish_time){uint8_t t=result_order[i];result_order[i]=result_order[j];result_order[j]=t;}
    for(i=0;i<4;i++){int t=result_order[i]==0?team_sel:boats[result_order[i]].team;if(result_order[i]!=0&&t==team_sel)continue;points[t]+=4-i;if(result_order[i]==0)player_rank=(uint8_t)(i+1);}
    last_prize=30-(player_rank-1)*7;if(player_rank==1){wins++;last_prize+=sponsors[sponsor].win_bonus;}money+=last_prize+sponsors[sponsor].fee;if(sponsor<3&&wins>=sponsors[sponsor+1].min_wins)sponsor++;
}
static int upgrade_cost(int l){return 8+l*7;}
static void update_manager(uint32_t p){
    int *v=0,old=menu;if(p&PRG32_BTN_UP)menu--;if(p&PRG32_BTN_DOWN)menu++;menu=clamp(menu,0,5);if(menu!=old)click();
    if(menu==1)v=&hull;else if(menu==2)v=&sails;else if(menu==3)v=&crew;else if(menu==4)v=&strategy;
    if((p&PRG32_BTN_A)&&menu==0&&sponsor<3&&wins>=sponsors[sponsor+1].min_wins){sponsor++;click();}
    else if((p&PRG32_BTN_A)&&v&&*v<8&&money>=upgrade_cost(*v)){money-=upgrade_cost(*v);(*v)++;snd_note(V_BELL,I_CHIME,84,0,0);}
    else if((p&PRG32_BTN_A)&&menu==5){screen=ST_BRIEF;click();}
    if((p&PRG32_BTN_B)&&v&&*v>1){(*v)--;money+=upgrade_cost(*v)/2;click();}
}

/* ----------------------------------------------------------- multiplayer -- */
/* Snapshot: x/y in decimetres; sprite = heading(9) | sheet(7); flags =
   leg(3) team(2) spin started finished racing gennaker kite-hoist(3) penalty
   retired(0x4000) old-race(0x8000); the 7-bit input carries speed in 0.2-knot
   steps. Lobby snapshots have x=y=0, flags = team(2)<<3 | ready(0x40). */
#define NF_READY 0x40
#define NF_RACING 0x100
#define NF_RETIRED 0x4000
#define NF_OLD_RACE 0x8000
/* Peers are bound to fleet slots by player id, so a disconnect hands only
   that yacht to the AI instead of shifting every later peer down a slot.
   Each console owns its own penalties; remote ones are never kept here. */
static int net_slot(uint32_t id,const uint8_t *seen){int i;for(i=1;i<4;i++)if(net_live[i]&&net_ids[i]==id)return i;for(i=1;i<4;i++)if(!net_live[i]&&!seen[i])return i;return 0;}
/* P1..P4 by player id, the same on every console. */
static uint8_t human_tag(uint32_t id){int i,n=fairwind_net_peer_count(),t=1;prg32_multiplayer_peer_t p;if(fairwind_net_local_id()<id)t++;for(i=0;i<n;i++)if(fairwind_net_peer(i,&p)&&p.player_id<id)t++;return (uint8_t)t;}
static void net_update(void){
    int i,n,slot;uint8_t seen[4]={0,0,0,0};prg32_multiplayer_peer_t p;const boat_t *me=&boats[0];
    uint16_t flags=(uint16_t)((me->leg&7)|((race_team&3)<<3)|(me->kite==KITE_SPIN?0x20:0)|(me->started?0x40:0)|(me->finished?0x80:0)|NF_RACING|(me->kite==KITE_GENN?0x200:0)|((me->kite_prog>>13)<<10)|(me->penalty?0x2000:0)|(me->retired?NF_RETIRED:0)|(race_ticks>300?NF_OLD_RACE:0));
    fairwind_net_publish((int)(me->x/100),(int)(me->y/100),(uint16_t)((me->heading>>7)|((unsigned)me->sheet<<9)),(uint32_t)clamp((int)((me->v>>8)/103),0,127),flags);
    n=fairwind_net_peer_count();my_tag=human_tag(fairwind_net_local_id());
    for(i=0;i<n;i++){
        boat_t *b;uint8_t was_finished,first;
        /* Only peers on this race's water take a yacht: a console still in
           the lobby (or one that left for the result screen) does not. */
        if(!fairwind_net_peer(i,&p))continue;
        /* A yacht whose console is back in the lobby holds its slot, still. */
        if(!(p.flags&NF_RACING)){for(slot=1;slot<4;slot++)if(net_live[slot]&&net_ids[slot]==p.player_id)seen[slot]=1;continue;}
        if((slot=net_slot(p.player_id,seen))<=0)continue;
        b=&boats[slot];was_finished=b->finished;first=!net_live[slot];
        seen[slot]=1;net_ids[slot]=p.player_id;net_tag[slot]=human_tag(p.player_id);
        if(first||p.x!=net_rx[slot]||p.y!=net_ry[slot]){net_rx[slot]=p.x;net_ry[slot]=p.y;net_age[slot]=0;}else if(net_age[slot]<600)net_age[slot]++;
        if(first){b->x=(int32_t)p.x*100;b->y=(int32_t)p.y*100;}
        b->heading=(uint16_t)((p.sprite&511u)<<7);b->sheet=(uint8_t)(p.sprite>>9);b->v=(int32_t)(p.input&127u)*103*256;
        b->leg=p.flags&7;b->team=(p.flags>>3)&3;b->kite=b->kite_want=(uint8_t)((p.flags&0x20)?KITE_SPIN:(p.flags&0x200)?KITE_GENN:KITE_NONE);b->kite_prog=(uint16_t)(((p.flags>>10)&7u)*9362u);
        b->started=(p.flags&0x40)!=0;b->finished=(p.flags&0x80)!=0;b->retired=(p.flags&NF_RETIRED)!=0;if(b->started||(start_clock<=120&&b->y<0))b->entered_box=1;
        b->penalty=b->serving=b->mark_touch=0;b->rule=RULE_NONE;
        if(!was_finished&&b->finished)b->finish_time=(uint16_t)(b->retired?9000:race_clock);
    }
    peer_count=0;for(i=1;i<4;i++){net_live[i]=seen[i];peer_count=(uint8_t)(peer_count+seen[i]);}
}
/* Snapshots arrive slower than frames: dead-reckon each remote yacht from
   its last snapshot along its heading and ease towards that estimate, so
   rivals glide instead of stepping. A big error snaps (a reconnection). */
static void net_smooth(boat_t *b,int slot){
    int32_t step=(b->v>>8)*time_scale()/60*net_age[slot],ex=(int32_t)net_rx[slot]*100+((step*fsin(b->heading))>>14),ey=(int32_t)net_ry[slot]*100+((step*fcos(b->heading))>>14),dx=ex-b->x,dy=ey-b->y;
    if(dx>60000||dx<-60000||dy>60000||dy<-60000){b->x=ex;b->y=ey;}else{b->x+=dx/4;b->y+=dy/4;}
    clamp_field(b);
}
/* The lobby gives every human a distinct syndicate: a console yields its
   choice to any peer with a lower player id and takes the next free one. */
static uint8_t lobby_team(void){
    int i,n=fairwind_net_peer_count(),k;unsigned taken=0;uint32_t me=fairwind_net_local_id();uint8_t t=(uint8_t)(team_sel&3);prg32_multiplayer_peer_t p;
    for(i=0;i<n;i++)if(fairwind_net_peer(i,&p)&&p.player_id<me)taken|=1u<<((p.flags>>3)&3);
    for(k=0;k<4&&((taken>>t)&1);k++)t=(uint8_t)((t+1)&3);
    return t;
}
/* A peer racing for more than a few seconds is a race already under way:
   this console waits for it to end rather than start out of step. */
static int lobby_race_running(void){int i,n=fairwind_net_peer_count();prg32_multiplayer_peer_t p;for(i=0;i<n;i++)if(fairwind_net_peer(i,&p)&&(p.flags&NF_RACING)&&(p.flags&NF_OLD_RACE)&&!(p.flags&0x80))return 1;return 0;}
/* Lobby snapshots carry only team and readiness; a racing snapshot sets
   NF_RACING, so a peer that has just started has seen every player ready
   and counts as ready. */
static void update_lobby(uint32_t p){
    int i,all_ready;prg32_multiplayer_peer_t peer;
    if(p&PRG32_BTN_A){if(!local_ready)click();local_ready=1;}
    if(p&PRG32_BTN_B){prg32_multiplayer_leave();multiplayer=0;peer_count=0;local_ready=0;start_race();return;}
    race_team=lobby_team();
    fairwind_net_publish(0,0,(uint16_t)team_sel,0,(uint16_t)(((race_team&3)<<3)|(local_ready?NF_READY:0)));
    peer_count=(uint8_t)fairwind_net_peer_count();all_ready=local_ready&&peer_count>0&&!lobby_race_running();
    for(i=0;i<peer_count;i++)if(!fairwind_net_peer(i,&peer)||!(peer.flags&(NF_READY|NF_RACING)))all_ready=0;
    if(all_ready)start_race();
}

/* ------------------------------------------------------------ the race -- */
static int close_to_rival_or_buoy(int limit){
    int i;int32_t dx,dy,l2=(int32_t)limit*limit,bx=boats[0].x/1000,by=boats[0].y/1000;
    for(i=1;i<4;i++){dx=bx-boats[i].x/1000;dy=by-boats[i].y/1000;if(dx*dx+dy*dy<=l2)return 1;}
    for(i=0;i<course_len[course_sel];i++){dx=bx-course_x[course_sel][i];dy=by-course_y[course_sel][i];if(dx*dx+dy*dy<=l2)return 1;}
    if(start_line_active){dx=absi((int)bx)-LINE_HALF;dy=by;return dx*dx+dy*dy<=l2;}
    dx=absi((int)bx)-FINISH_HALF;dy=by-FINISH_Y;return dx*dx+dy*dy<=l2;
}
/* Course progress for the live position: finished yachts by time, then legs
   rounded, then distance to the next mark. */
static int32_t progress(const boat_t *b){int32_t mx,my,dx,dy;if(b->finished)return 100000000-(int32_t)b->finish_time;if(!b->started)return -100000+b->y/1000;leg_target(b->leg,&mx,&my);dx=(mx-b->x)/1000;dy=(my-b->y)/1000;return (int32_t)b->leg*10000-isqrt((uint32_t)(dx*dx+dy*dy));}
static int live_rank(void){int i,r=1;int32_t me=progress(&boats[0]);for(i=1;i<4;i++)if(progress(&boats[i])>me)r++;return r;}
static void retire(void){boat_t *me=&boats[0];me->finished=me->retired=1;me->finish_time=9000;me->serving=me->penalty=0;if(!multiplayer)race_clock=RACE_LIMIT_SECONDS+1;toast("RETIRED",T_RED);}
/* Taps act on release, so a hold or a chord never flips a kite. Hold A to
   take a penalty turn; hold B for the race menu. */
static void race_buttons(boat_t *me,uint32_t in,uint32_t released){
    if((in&(PRG32_BTN_A|PRG32_BTN_B))==(PRG32_BTN_A|PRG32_BTN_B))a_used=b_used=1;
    if(in&PRG32_BTN_A){if(hold_a<255)hold_a++;if(hold_a==HOLD_FRAMES&&!a_used){a_used=1;if(me->penalty&&!me->serving){begin_penalty_turn(me);toast("PENALTY TURN",T_GOLD);}}}
    if(in&PRG32_BTN_B){if(hold_b<255)hold_b++;if(hold_b==HOLD_FRAMES&&!b_used){b_used=1;race_menu=1;race_menu_sel=0;click();}}
    if(released&PRG32_BTN_A){if(!a_used&&!me->finished)me->kite_want=(uint8_t)(me->kite_want==KITE_SPIN?KITE_NONE:KITE_SPIN);hold_a=0;a_used=0;}
    if(released&PRG32_BTN_B){if(!b_used&&!me->finished)me->kite_want=(uint8_t)(me->kite_want==KITE_GENN?KITE_NONE:KITE_GENN);hold_b=0;b_used=0;}
}
/* Turn the local yacht's events into sound and messages. */
static void race_events(void){
    boat_t *me=&boats[0];uint8_t e=me->evt;int i;
    if(e&EV_MARK){static char msg[]="MARK 0 ROUNDED";msg[5]=(char)('0'+me->leg);toast(me->leg>=course_len[course_sel]?"LAST MARK - TO THE FINISH":msg,T_GOLD);snd_note(V_BELL,I_CHIME,84,0,0);chime_frames=16;}
    if(e&EV_FINISH){static char fin[]="FINISHED - POSITION 0";fin[20]=(char)('0'+live_rank());toast(fin,T_GOLD);snd_note(V_THUMP,I_GUN,40,0,0);horn(1);}
    if(e&EV_PENALTY){toast(rule_names[me->rule],T_RED);snd_note(V_BELL,I_BUZZ,45,0,0);buzz_frames=22;}
    if(e&EV_CLEARED){toast("PENALTY CLEARED",T_GREEN);snd_note(V_BELL,I_CHIME,79,0,0);chime_frames=16;}
    if(e&EV_CONTACT)snd_note(V_THUMP,I_THUMP,30,0,0);
    else if(e&EV_GYBE)snd_note(V_THUMP,I_THUMP,38,200,(int8_t)(me->boom>=0?40:-40));
    else if(e&EV_TACK)snd_note(V_FLAP,I_FLAP,64,220,0);
    if(me->kite_want!=KITE_NONE&&me->kite==me->kite_want&&me->kite_prog>0&&me->kite_prog<3000)snd_note(V_FLAP,I_WHOOSH,(uint8_t)(52+me->kite_prog/400),0,0);
    for(i=0;i<4;i++)boats[i].evt=0;
}
static void update_race(uint32_t in,uint32_t released){
    int i,done=1,all_started=1;boat_t *me=&boats[0];
    race_ticks++;if(toast_frames>0)toast_frames--;
    race_frames++;if(race_frames>=frames_per_sim_second()){race_frames=0;wind_t++;update_wind();if(start_clock>0){start_clock--;
        if(start_clock==300){horn(0);toast("5 MINUTES - CLASS FLAG",T_GOLD);}else if(start_clock==240){horn(0);toast("4 MINUTES - P FLAG UP",T_GOLD);}
        else if(start_clock==120){toast("2 MINUTES - ENTER THE BOX",T_CYAN);}else if(start_clock==60){horn(1);toast("1 MINUTE - P FLAG DOWN",T_GOLD);}
        else if(start_clock==0){horn(1);snd_note(V_THUMP,I_GUN,40,0,0);toast("START! CLASS FLAG DOWN",T_GREEN);}}else race_clock++;}
    race_buttons(me,in,released);
    helm_player(me,in);update_rig(me,1);sail(me);
    if(multiplayer)net_update();
    for(i=1;i<4;i++){boat_t *b=&boats[i];if(net_live[i]){update_rig(b,0);net_smooth(b,i);b->evt=0;continue;}if(b->penalty&&clear_water(b,45))begin_penalty_turn(b);helm_ai(b);update_rig(b,1);sail(b);}
    enforce_rules();race_events();race_audio_update();
    top_view_mode=(uint8_t)close_to_rival_or_buoy(top_view_mode?TOP_VIEW_EXIT:TOP_VIEW_ENTER);
    cam_h=(uint16_t)(cam_h+(int16_t)(me->heading-cam_h)/6);
    for(i=0;i<4;i++){if(!boats[i].started)all_started=0;if(!boats[i].finished)done=0;}
    if(all_started)start_line_active=0;
    if(race_clock>RACE_LIMIT_SECONDS)for(i=0;i<4;i++)if(!boats[i].finished){boats[i].finished=1;boats[i].finish_time=(uint16_t)(race_clock+600+(COURSE_MARKS_MAX+1-(boats[i].started?boats[i].leg+1:0))*30);}
    if((done&&start_clock==0)||race_clock>RACE_LIMIT_SECONDS){score_race();screen=ST_RESULT;menu=0;race_menu=0;snd_song(player_rank<=3?SND_PODIUM:SND_RESULT);}
}
/* The race menu: single-player races pause behind it; a network race goes
   on with the helm centred. */
static void update_race_menu(uint32_t p){
    int items=multiplayer?3:4;
    if(p&PRG32_BTN_UP)race_menu_sel=(uint8_t)((race_menu_sel+items-1)%items);
    if(p&PRG32_BTN_DOWN)race_menu_sel=(uint8_t)((race_menu_sel+1)%items);
    if(p&(PRG32_BTN_UP|PRG32_BTN_DOWN))click();
    if((race_menu_sel==1)&&(p&(PRG32_BTN_LEFT|PRG32_BTN_RIGHT))){auto_trim=!auto_trim;click();}
    if(p&PRG32_BTN_B){race_menu=0;return;}
    if(!(p&PRG32_BTN_A))return;
    click();
    if(race_menu_sel==0)race_menu=0;
    else if(race_menu_sel==1)auto_trim=!auto_trim;
    else if(race_menu_sel==2){race_menu=0;if(!boats[0].finished)retire();}
    else{race_menu=0;start_race();}
}
static void enter_screen(screen_t s){screen=s;menu=0;click();}
void fairwind_init(void){
    load_fixed_palette();loaded_venue=255;load_venue(0);
    prg32_band_set_colors(PRG32_BAND_TOP,T_GOLD,T_HDR);prg32_band_set_colors(PRG32_BAND_BOTTOM,T_GOLD,T_HDR);
    prg32_band_set_game_info("FairWind-napoli97 | 128 KiB | multiplayer");
    screen=ST_TITLE;menu=0;snd_song(SND_TITLE);
}
void fairwind_update(void){
    uint32_t in=prg32_input_read()&(PRG32_BTN_LEFT|PRG32_BTN_RIGHT|PRG32_BTN_UP|PRG32_BTN_DOWN|PRG32_BTN_A|PRG32_BTN_B),p=in&~last_input,r=last_input&~in;
    cur_input=in;frame++;
    if(screen==ST_TITLE){if(p&PRG32_BTN_A)enter_screen(ST_MODE);}
    else if(screen==ST_MODE){
        if(p&PRG32_BTN_UP){menu=(menu+2)%3;click();}if(p&PRG32_BTN_DOWN){menu=(menu+1)%3;click();}
        if(menu==2&&(p&(PRG32_BTN_LEFT|PRG32_BTN_RIGHT|PRG32_BTN_A))){auto_trim=!auto_trim;click();}
        else if(p&PRG32_BTN_A){multiplayer=(uint8_t)(menu==1);menu=0;points[0]=points[1]=points[2]=points[3]=0;race_no=wins=course_sel=0;money=70;hull=sails=crew=strategy=1;sponsor=0;last_prize=0;enter_screen(ST_TEAM);snd_song(SND_HQ);}
        if(p&PRG32_BTN_B)enter_screen(ST_TITLE);
    }
    else if(screen==ST_TEAM){if(p&(PRG32_BTN_LEFT|PRG32_BTN_UP)){team_sel=(team_sel+3)%4;click();}if(p&(PRG32_BTN_RIGHT|PRG32_BTN_DOWN)){team_sel=(team_sel+1)%4;click();}if(p&PRG32_BTN_A)enter_screen(ST_MANAGER);if(p&PRG32_BTN_B){enter_screen(ST_MODE);menu=multiplayer;}}
    else if(screen==ST_MANAGER)update_manager(p);
    else if(screen==ST_BRIEF){
        if(p&(PRG32_BTN_LEFT|PRG32_BTN_UP)){course_sel=(course_sel+COURSE_COUNT-1)%COURSE_COUNT;click();}
        if(p&(PRG32_BTN_RIGHT|PRG32_BTN_DOWN)){course_sel=(course_sel+1)%COURSE_COUNT;click();}
        if(p&PRG32_BTN_B)enter_screen(ST_MANAGER);
        else if(p&PRG32_BTN_A){if(multiplayer){peer_count=local_ready=0;if(fairwind_net_join(course_sel)){screen=ST_LOBBY;snd_song(SND_LOBBY);}else multiplayer=0;if(!multiplayer){start_race();toast("NO SERVER - RACING THE AI",T_RED);}}else start_race();}
    }
    else if(screen==ST_LOBBY)update_lobby(p);
    else if(screen==ST_RACE){
        if(race_menu){update_race_menu(p);if(multiplayer&&screen==ST_RACE&&race_menu)update_race(0,0);if(!race_menu){hold_a=hold_b=0;a_used=(uint8_t)((in&PRG32_BTN_A)!=0);b_used=(uint8_t)((in&PRG32_BTN_B)!=0);}}
        else update_race(in,r);
    }
    else if(screen==ST_RESULT&&(p&PRG32_BTN_A)){if(multiplayer)prg32_multiplayer_leave();race_no++;if(race_no>=RACES){screen=ST_SEASON;fairwind_score_submit((uint32_t)(points[team_sel]*1000+clamp(money,0,999)));snd_song(SND_SEASON);}else{enter_screen(ST_MANAGER);snd_song(SND_HQ);}}
    else if(screen==ST_SEASON&&(p&PRG32_BTN_A)){enter_screen(ST_TITLE);snd_song(SND_TITLE);}
    last_input=in;
}

/* ------------------------------------------------------ 3D rasteriser -- */
/* Camera space is decimetres: x to the right, z forward, h height above the
   eye. The chase camera rides CAM_BACK behind and CAM_H above the yacht; the
   close-quarters view is an overhead orthographic camera with a slight tilt
   so masts and sails keep their shape. Both views clip in camera space. */
static int32_t cam_wx,cam_wy,cam_s,cam_c;
static void camera_at(int32_t wx,int32_t wy,uint16_t h,int back){cam_h=h;cam_s=fsin(cam_h);cam_c=fcos(cam_h);cam_wx=wx;cam_wy=wy;if(back){cam_wx-=(back*cam_s)>>14;cam_wy-=(back*cam_c)>>14;}}
static void camera_setup(void){camera_at(boats[0].x/100,boats[0].y/100,cam_h,top_view_mode?0:CAM_BACK);}
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
    if(top_view_mode){*sx=vw.cx+(int)(p->x*TOP_S/100);*sy=TOP_CY-(int)((p->z*TOP_S+p->h*TOP_TILT)/100);}
    else{*sx=vw.cx+(int)(p->x*FOCAL/p->z);*sy=HOR-(int)(p->h*FOCAL/p->z);}
}
static void rect_clip(int x,int y,int w,int h,uint8_t c){if(x<vw.x0){w-=vw.x0-x;x=vw.x0;}if(x+w>vw.x1)w=vw.x1-x;if(y<vw.y0){h-=vw.y0-y;y=vw.y0;}if(y+h>vw.y1)h=vw.y1-y;if(w>0&&h>0)gx_rect(x,y,w,h,c);}
/* Even-odd scanline fill, so concave sail outlines render correctly. */
static void fill_poly(const int *xs,const int *ys,int n,uint8_t c){
    int y,y0=9999,y1=-9999,i,j,k,m;
    for(i=0;i<n;i++){if(ys[i]<y0)y0=ys[i];if(ys[i]>y1)y1=ys[i];}
    if(y0<CLIP_TOP)y0=CLIP_TOP;if(y1>CLIP_BOT)y1=CLIP_BOT;
    for(y=y0;y<y1;y++){
        int cx[16];m=0;
        for(i=0;i<n;i++){j=i+1==n?0:i+1;if((y>=ys[i]&&y<ys[j])||(y>=ys[j]&&y<ys[i]))cx[m++]=xs[i]+(y-ys[i])*(xs[j]-xs[i])/(ys[j]-ys[i]);}
        for(i=1;i<m;i++){int v=cx[i];for(k=i;k>0&&cx[k-1]>v;k--)cx[k]=cx[k-1];cx[k]=v;}
        for(k=0;k+1<m;k+=2)rect_clip(cx[k],y,cx[k+1]-cx[k]+1,1,c);
    }
}
/* Clip, project and fill. The projected winding tells which face the camera
   sees: `front` fills a face seen from outside, `back` the other one (255
   culls it), so hull sides cull and sails shade by the side in view. */
#define CULL 255
static int poly3s(const v3 *in,int n,uint8_t front,uint8_t back){
    v3 a[16],b[16];int p,i,m,xs[16],ys[16];int32_t area=0;
    for(i=0;i<n;i++)a[i]=in[i];
    for(p=0;p<plane_count()&&n>=3;p++){
        for(i=m=0;i<n;i++){const v3 *u=&a[i],*v=&a[i+1==n?0:i+1];int32_t du=plane_d(u,p),dv=plane_d(v,p);if(du>=0)b[m++]=*u;if((du>=0)!=(dv>=0))b[m++]=lerp3(u,v,du,dv);}
        for(i=0;i<m;i++)a[i]=b[i];n=m;
    }
    if(n<3)return 0;
    for(i=0;i<n;i++)proj(&a[i],&xs[i],&ys[i]);
    for(i=0;i<n;i++){int j=i+1==n?0:i+1;area+=(int32_t)xs[i]*ys[j]-(int32_t)xs[j]*ys[i];}
    if(area>=0){if(back==CULL)return -1;fill_poly(xs,ys,n,back);return -1;}
    fill_poly(xs,ys,n,front);return 1;
}
static void poly3(const v3 *in,int n,uint8_t c){poly3s(in,n,c,c);}
static void line2(int x0,int y0,int x1,int y1,uint8_t c){
    int dx=x1-x0,dy=y1-y0,n=absi(dx)>absi(dy)?absi(dx):absi(dy),i;int32_t x=(int32_t)x0*65536+32768,y=(int32_t)y0*65536+32768,sx,sy;
    if(n>1200)return;if(!n)n=1;sx=(int32_t)dx*65536/n;sy=(int32_t)dy*65536/n;
    for(i=0;i<=n;i++){int px=(int)(x>>16),py=(int)(y>>16);if(px>=vw.x0&&px<vw.x1&&py>=CLIP_TOP&&py<CLIP_BOT)gx_pixel(px,py,c);x+=sx;y+=sy;}
}
static void line3(v3 a,v3 b,uint8_t c){
    int p,x0,y0,x1,y1;
    for(p=0;p<plane_count();p++){int32_t da=plane_d(&a,p),db=plane_d(&b,p);if(da<0&&db<0)return;if(da<0)a=lerp3(&a,&b,da,db);else if(db<0)b=lerp3(&b,&a,db,da);}
    proj(&a,&x0,&y0);proj(&b,&x1,&y1);line2(x0,y0,x1,y1,c);
}
static void wline(int x0,int y0,int x1,int y1,uint8_t c){line3(cam_pt(x0*10,y0*10,0),cam_pt(x1*10,y1*10,0),c);}
/* Row-compressed indexed bitmap: (count, index) runs, 255 transparent. */
static void blit_rle(int x,int y,int w,const uint8_t *d,int n,int xoff){
    int i=0,cx=0;
    while(i<n){int c=d[i++],v=d[i++],a=x+cx+xoff,b=a+c;if(v!=255){if(a<vw.x0)a=vw.x0;if(b>vw.x1)b=vw.x1;if(b>a&&y>=vw.y0&&y<vw.y1)gx_rect(a,y,b-a,1,(uint8_t)v);}cx+=c;if(cx>=w){cx=0;y++;}}
}

/* ------------------------------------------------------------- scenery -- */
/* Screen x of a world bearing seen from the camera, or -9999 off to the side. */
static int bearing_x(uint16_t wb,int halfwidth){int rel=(int16_t)(wb-cam_h);if(rel<-DEG(70)||rel>DEG(70))return -9999;(void)halfwidth;return vw.cx+rel*FOCAL/10430;}
/* Clouds are infinitely far: they turn with the view and drift with the
   day's breeze, bearing in degrees from upwind, height above the horizon. */
static const int16_t cloud_b[7]={20,75,128,170,214,262,318};
static const uint8_t cloud_h[7]={40,28,46,34,24,42,30},cloud_w[7]={44,30,52,36,28,40,34};
static void draw_cloud(int x,int y,int w){
    int r,hh=w/5+3;
    for(r=0;r<hh;r++){int t=r*2-hh,half=w/2-(t*t*w/2)/(hh*hh*2);uint8_t c=r<hh/3?C_CLOUD0:r<hh*2/3?C_CLOUD1:C_CLOUD2;rect_clip(x-half,y+r,half*2,1,c);}
    for(r=0;r<hh/2+2;r++){int half=w/5-r*2/3;rect_clip(x-w/6-half,y-r,half*2,1,C_CLOUD0);rect_clip(x+w/8-half*3/4,y-r+1,half*3/2,1,r<2?C_CLOUD1:C_CLOUD0);}
}
static void panorama(int bottom,int off){int v=lit();blit_rle(0,bottom-pano_h[v],320,pano_rle+pano_off[v],pano_off[v+1]-pano_off[v],off);}
/* The Gulf opens to the SW: open horizon upwind, the venue's panorama dead
   downwind (1:1 at 150 px per radian), the far coast of Ischia, Procida,
   Posillipo, Monte Faito and Capri all round, a graded sky with the sun and
   its glitter path on the water. */
static void draw_horizon(void){
    int y,sx,i,v=lit(),off=(int)((int16_t)(32768u-cam_h))*FOCAL/10430,top=vw.y0,sky_rows=HOR-top,sea_rows=vw.y1-HOR,last=-1,start=HOR;
    for(i=0;i<SKY_N;i++){int a=top+sky_rows*i/SKY_N,b=top+sky_rows*(i+1)/SKY_N;rect_clip(vw.x0,a,vw.x1-vw.x0,b-a,venue_slot[VK_SKY0+i]);}
    sx=bearing_x(venue_sun_bearing[v],0);
    if(sx>-9999){int sy=HOR-venue_sun_elev[v]*sky_rows/72-1,r;for(r=-9;r<=9;r++){int hw=9-absi(r)*absi(r)/9;rect_clip(sx-hw,sy+r,hw*2+1,1,C_HALO);}for(r=-4;r<=4;r++){int hw=4-absi(r)*absi(r)/4;rect_clip(sx-hw,sy+r,hw*2+1,1,C_SUN);}}
    for(i=0;i<7;i++){int cx=bearing_x((uint16_t)(DEG(cloud_b[i])+wind_t*3+frame/40),cloud_w[i]);if(cx>-9999)draw_cloud(cx,HOR-cloud_h[i]*sky_rows/58,cloud_w[i]);}
    for(sx=vw.x0;sx<vw.x1;sx+=2){uint16_t wb=(uint16_t)(cam_h+(sx+1-vw.cx)*10430/FOCAL);int d=absi((int16_t)(wb-32768u)),f;if(d<DEG(61)&&off>-W&&off<W)continue;f=far_coast[wb>>8];if(f&31)rect_clip(sx,HOR-(f&31),2,f&31,(uint8_t)(f>>5==0?C_FAR0:f>>5==1?C_FAR1:C_FAR2));}
    if(off>-W&&off<W)panorama(HOR,off);
    /* Sea: graded from the hazy horizon to deep water under the camera, with
       perspective (bands crowd towards the horizon). */
    for(y=HOR;y<vw.y1;y++){int k=isqrt((uint32_t)((y-HOR)*SEA_N*SEA_N/sea_rows));if(k>=SEA_N)k=SEA_N-1;if(k!=last){if(last>=0)rect_clip(vw.x0,start,vw.x1-vw.x0,y-start,venue_slot[VK_SEA0+last]);last=k;start=y;}}
    rect_clip(vw.x0,start,vw.x1-vw.x0,vw.y1-start,venue_slot[VK_SEA0+last]);
    sx=bearing_x(venue_sun_bearing[v],0);
    if(sx>-9999){int r;uint32_t h=(uint32_t)(cam_h>>9)*2654435761u;for(r=1;r<sea_rows*2/3;r+=2){int spread=2+r*(venue_sun_elev[v]<30?3:2)/5,k;for(k=0;k<1+r/16;k++){int gx;h=h*1103515245u+12345u;gx=sx+(int)((h>>8)%(uint32_t)(2*spread+1))-spread;rect_clip(gx,HOR+r,2+r/14,1,(uint8_t)(h>>30==0?C_GL0:h>>30==1?C_GL1:h>>30==2?C_GL2:C_GL3));}}}
}
static const int16_t shore_h[9]={200,260,180,320,280,220,300,240,210};
static void shore_wall(int x0,int y0,int x1,int y1,uint8_t lit,uint8_t dark,int seed){
    int k;for(k=0;k<8;k++){
        int32_t ax=(x0+(x1-x0)*k/8)*10,ay=(y0+(y1-y0)*k/8)*10,bx=(x0+(x1-x0)*(k+1)/8)*10,by=(y0+(y1-y0)*(k+1)/8)*10;v3 q[4];
        q[0]=cam_pt(ax,ay,0);q[1]=cam_pt(bx,by,0);q[2]=cam_pt(bx,by,shore_h[(k+seed+1)%9]);q[3]=cam_pt(ax,ay,shore_h[(k+seed)%9]);
        poly3(q,4,(k&1)?lit:dark);line3(q[0],q[1],C_FOAM);
    }
}
static void ground(int x0,int y0,int x1,int y1,uint8_t c){v3 q[4];q[0]=cam_pt(x0*10,y0*10,0);q[1]=cam_pt(x1*10,y0*10,0);q[2]=cam_pt(x1*10,y1*10,0);q[3]=cam_pt(x0*10,y1*10,0);poly3(q,4,c);}
/* Naples to leeward, Sorrento's tufa to port and Posillipo's green to
   starboard of the upwind course; the yachts cannot sail onto any of it. */
static void draw_land(void){
    if(top_view_mode){ground(-2500,-2500,-FIELD_X,FIELD_Y1,C_GROUND);ground(FIELD_X,-2500,2500,FIELD_Y1,C_GROUND_D);ground(-FIELD_X,-2500,FIELD_X,FIELD_Y0,C_GROUND);wline(-FIELD_X,FIELD_Y1,FIELD_X,FIELD_Y1,C_GOLD);
        wline(-FIELD_X,FIELD_Y0,-FIELD_X,FIELD_Y1,C_FOAM);wline(FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y1,C_FOAM);wline(-FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y0,C_FOAM);return;}
    shore_wall(-FIELD_X,FIELD_Y1,-FIELD_X,FIELD_Y0,C_TUFA_L,C_TUFA_D,0);shore_wall(-FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y0,C_CITY_L,C_CITY_D,3);shore_wall(FIELD_X,FIELD_Y0,FIELD_X,FIELD_Y1,C_GREEN_L,C_GREEN_D,5);
}
/* World-anchored ripples drift downwind and give the eye speed and depth;
   their colours are the cycling glint entries, so they sparkle. */
static void draw_ripples(void){
    int i,j,sx,sy,w;int32_t cx,cy,drift=((int32_t)wind_t*6+(int32_t)(frame/4))%50000,dx=-(drift*fsin(twd))/16384,dy=-(drift*fcos(twd))/16384,gx,gy;
    static const uint8_t col[4]={C_RIPPLE,C_GL1,C_RIPPLE,C_GL3};
    cx=cam_wx;cy=cam_wy;if(!top_view_mode){cx+=(1500*cam_s)>>14;cy+=(1500*cam_c)>>14;}
    gx=(cx-dx)/250;gy=(cy-dy)/250;
    for(i=-7;i<=7;i++)for(j=-7;j<=7;j++){
        uint32_t h=(uint32_t)(gx+i)*73856093u^(uint32_t)(gy+j)*19349663u;int32_t wx=(gx+i)*250+dx+(int32_t)(h>>8)%200,wy=(gy+j)*250+dy+(int32_t)(h>>16)%200;v3 p=cam_pt(wx,wy,0);
        if(plane_d(&p,0)<30||plane_d(&p,1)<0||plane_d(&p,2)<0)continue;
        if(!top_view_mode&&p.z>4500)continue;
        proj(&p,&sx,&sy);w=top_view_mode?6:2+(int)(1200/p.z);rect_clip(sx-w/2,sy,w,1,col[h&3]);
    }
}

/* -------------------------------------------------------------- yachts -- */
static const boat_t *bb_;static int32_t bs_,bc_;static uint16_t sun_rel_;
static v3 bpt(int lx,int ly,int z){int32_t wx,wy;lx+=z*bb_->heel/57;wx=bb_->x/100+((lx*bc_+ly*bs_)>>14);wy=bb_->y/100+((ly*bc_-lx*bs_)>>14);return cam_pt(wx,wy,z);}
static void use_boat(const boat_t *b){bb_=b;bs_=fsin(b->heading);bc_=fcos(b->heading);sun_rel_=(uint16_t)(venue_sun_bearing[lit()]-b->heading);}
static const int8_t hull_lx[7]={0,17,18,13,-13,-18,-17};static const int16_t hull_ly[7]={106,55,-30,-98,-98,-30,55};
/* Outward normal of each hull side (from bow round to starboard), relative
   to the bow: a side facing the sun is lit, one in shadow darker. */
static const uint16_t hull_nrm[7]={DEG(71),DEG(91),DEG(140),DEG(180),DEG(220),DEG(269),DEG(289)};
static void hull_ring(int z,int pct,v3 *p){int i;for(i=0;i<7;i++)p[i]=bpt(hull_lx[i]*pct/100,hull_ly[i]*pct/100,z);}
static void draw_hull(int pct,uint8_t lit,uint8_t dark,uint8_t deck,uint8_t stripe){
    v3 lo[7],hi[7],q[4];int i;
    hull_ring(0,pct,lo);hull_ring(SHEER_Z*pct/100,pct,hi);
    for(i=0;i<7;i++){int j=i+1==7?0:i+1;q[0]=lo[j];q[1]=lo[i];q[2]=hi[i];q[3]=hi[j];poly3s(q,4,fcos((uint16_t)(hull_nrm[i]-sun_rel_))>0?lit:dark,CULL);}
    poly3(hi,7,deck);
    if(stripe!=CULL)for(i=0;i<7;i++)line3(hi[i],hi[i+1==7?0:i+1],stripe);
}
static void seg(int ax,int ay,int az,int bx,int by,int bz,uint8_t c){line3(bpt(ax,ay,az),bpt(bx,by,bz),c);}
/* A point's offset from a stowed kite at the bow grows with hoist progress. */
static v3 kpt(int x,int y,int z,int k){return bpt(x*k/255,90+(y-90)*k/255,14+(z-14)*k/255);}
static void draw_main(void){
    int b=bb_->boom,side=b>=0?1:-1,cx,cy,belly=12,ex,ey,luff=trim_eff(bb_)<45&&absi(b)>trim_opt(awa_deg(bb_));int32_t s=fsin((uint16_t)DEG(b)),c=fcos((uint16_t)DEG(b));v3 p[5];
    if(luff)belly=(frame&4)?10:-6;
    cx=(int)((BOOM_LEN*s)>>14);cy=MAST_Y-(int)((BOOM_LEN*c)>>14);ex=(int)((side*belly*c)>>14);ey=(int)((side*belly*s)>>14);
    p[0]=bpt(0,MAST_Y,BOOM_Z);p[1]=bpt(0,MAST_Y,MAST_TOP);p[2]=bpt(cx/2+ex,(MAST_Y+cy)/2+ey,(MAST_TOP+BOOM_Z)/2-10);p[3]=bpt(cx,cy,BOOM_Z);p[4]=bpt(cx/2+ex*2/3,(MAST_Y+cy)/2+ey*2/3,BOOM_Z);
    if(luff&&(frame&2))poly3(p,5,C_CLOTH_LUFF);else poly3s(p,5,C_CLOTH_L,C_CLOTH_D);
    /* Battens and the boom. */
    line3(bpt(cx*3/10+ex/2,MAST_Y+(cy-MAST_Y)*3/10+ey/2,BOOM_Z+150),bpt(0,MAST_Y,BOOM_Z+150),C_BATTEN);
    line3(bpt(cx*6/10+ex*2/3,MAST_Y+(cy-MAST_Y)*6/10+ey*2/3,BOOM_Z+80),bpt(0,MAST_Y,BOOM_Z+80),C_BATTEN);
    seg(0,MAST_Y,12,0,MAST_Y,MAST_TOP+8,C_SPAR);seg(0,MAST_Y,BOOM_Z,cx,cy,BOOM_Z,C_SPAR);
}
static void draw_fore(void){
    int kp=bb_->kite==KITE_NONE?0:bb_->kite_prog>>8,sd=bb_->boom>=0?1:-1,awa=awa_deg(bb_),i,t=bb_->team&3;v3 p[8],q[8];
    if(kp<255){
        int jf=255-kp,j=clamp(bb_->boom*55/100,-32,32),hx=104+(MAST_Y+4-104)*jf/255,hz=14+191*jf/255,cx,cy;int32_t s=fsin((uint16_t)DEG(j)),c=fcos((uint16_t)DEG(j));
        cx=(int)((70*s)>>14)*jf/255;cy=104-(int)((70*c)>>14)*jf/255;
        p[0]=bpt(0,104,14);p[1]=bpt(0,hx,hz);p[2]=bpt(cx/2+sd*8,(hx+cy)/2,hz/2+8);p[3]=bpt(cx,cy,16);
        poly3s(p,4,C_CLOTH_L,C_CLOTH_D);
    }
    if(kp>0){
        if(bb_->kite==KITE_SPIN){
            int pa=clamp(awa-80,12,88),ca=clamp(awa-50,25,100),tx=-sd*(int)((55*fsin((uint16_t)DEG(pa)))>>14),ty=MAST_Y+(int)((55*fcos((uint16_t)DEG(pa)))>>14),cx=sd*(int)((62*fsin((uint16_t)DEG(ca)))>>14),cy=MAST_Y+(int)((62*fcos((uint16_t)DEG(ca)))>>14);
            /* A symmetric kite: rounded head, full shoulders, pole to windward. */
            p[0]=kpt(0,MAST_Y+12,250,kp);p[1]=kpt(cx*3/4,cy+26,228,kp);p[2]=kpt(cx*118/100,cy+36,165,kp);p[3]=kpt(cx,cy,30,kp);p[4]=kpt((tx+cx)/2,(ty>cy?ty:cy)+42,45,kp);p[5]=kpt(tx,ty,28,kp);p[6]=kpt(tx*118/100,ty+36,165,kp);p[7]=kpt(tx*3/4,ty+26,228,kp);
        }else{
            int ga=clamp(150-awa,60,110),cx=sd*(int)((55*fsin((uint16_t)DEG(ga)))>>14),cy=MAST_Y+(int)((55*fcos((uint16_t)DEG(ga)))>>14);
            /* Asymmetric: tacked at the bow, luff curling to leeward. */
            p[0]=kpt(0,MAST_Y+10,240,kp);p[1]=kpt(sd*16,70,200,kp);p[2]=kpt(sd*28,90,130,kp);p[3]=kpt(0,112,16,kp);p[4]=kpt(sd*30,(112+cy)/2+10,28,kp);p[5]=kpt(cx,cy,30,kp);p[6]=kpt(cx*3/4+sd*14,(MAST_Y+cy)/2+8,120,kp);p[7]=kpt(cx/3+sd*10,MAST_Y+14,200,kp);
        }
        poly3s(p,8,team_c[t],team_d[t]);
        for(i=0;i<8;i++){q[i].x=(p[i].x+(p[0].x+p[4].x)*2)/5;q[i].h=(p[i].h+(p[0].h+p[4].h)*2)/5;q[i].z=(p[i].z+(p[0].z+p[4].z)*2)/5;}
        poly3s(q,8,C_CLOTH_L,C_CLOTH_D);
    }
}
/* Wake: a widening V of foam and lighter water astern, a bow wave at speed. */
static void draw_wake(const boat_t *b){
    int32_t v=b->v>>8;int len=(int)(v/30);v3 w[4];
    if(len<8)return;
    w[0]=bpt(-12,-94,0);w[1]=bpt(12,-94,0);w[2]=bpt(len/5,-96-len,0);w[3]=bpt(-len/5,-96-len,0);poly3(w,4,C_WAKE);
    seg(-12,-96,0,-len/4,-96-len,0,C_FOAM);seg(12,-96,0,len/4,-96-len,0,C_FOAM);
    if(v>1500){w[0]=bpt(0,108,0);w[1]=bpt(18,60,0);w[2]=bpt(22+(int)(v/400),40,0);w[3]=bpt(6,96,0);poly3(w,4,C_FOAM);w[1]=bpt(-18,60,0);w[2]=bpt(-22-(int)(v/400),40,0);w[3]=bpt(-6,96,0);poly3(w,4,C_FOAM);}
}
static void draw_boat(const boat_t *b){
    use_boat(b);draw_wake(b);
    draw_hull(100,C_HULL_L,C_HULL_D,C_DECK_L,team_c[b->team&3]);
    {v3 ck[4];ck[0]=bpt(-7,-40,SHEER_Z+1);ck[1]=bpt(7,-40,SHEER_Z+1);ck[2]=bpt(6,-80,SHEER_Z+1);ck[3]=bpt(-6,-80,SHEER_Z+1);poly3(ck,4,C_COCKPIT);}
    /* Paint the farther of mainsail and headsail first. */
    if(bpt(0,MAST_Y-40,120).z>bpt(0,80,120).z){draw_main();draw_fore();}else{draw_fore();draw_main();}
}
static void draw_buoy(int x,int y,int next){
    v3 base=cam_pt(x*10,y*10,0),top=cam_pt(x*10,y*10,30);int sx,sy,tx,ty,w;
    if(plane_d(&base,0)<0)return;
    proj(&base,&sx,&sy);proj(&top,&tx,&ty);w=top_view_mode?3:1+(int)(12*FOCAL/base.z);if(sx<-20||sx>W+20)return;
    if(ty>sy-2)ty=sy-2;
    rect_clip(sx-w,ty+(sy-ty)/4,w,sy-ty-(sy-ty)/4+1,C_BUOY_L);rect_clip(sx,ty+(sy-ty)/4,w,sy-ty-(sy-ty)/4+1,C_BUOY_D);
    rect_clip(sx-w/2,ty,w,(sy-ty)/4+1,C_BUOY_L);rect_clip(sx-w-1,sy,2*w+2,1,C_FOAM);rect_clip(sx-w,(ty+sy)/2,2*w,1,C_WHITE);
    if(next&&(frame&16)){rect_clip(sx-w-3,ty-3,2*w+6,1,C_GOLD);rect_clip(sx-w-3,sy+2,2*w+6,1,C_GOLD);rect_clip(sx-w-3,ty-3,1,sy-ty+6,C_GOLD);rect_clip(sx+w+2,ty-3,1,sy-ty+6,C_GOLD);}
    if(next&&ty-12>CLIP_TOP&&sx>16&&sx<W-16)gx_text(sx-15,ty-12,"NEXT",T_GOLD,T_NAVY);
    if(next&&top_view_mode){int i;for(i=0;i<32;i++){v3 z=cam_pt(x*10+((MARK_ZONE*10*fsin((uint16_t)(i*2048)))>>14),y*10+((MARK_ZONE*10*fcos((uint16_t)(i*2048)))>>14),0);int zx,zy;if(plane_d(&z,0)<0||plane_d(&z,1)<0||plane_d(&z,2)<0||plane_d(&z,3)<0)continue;proj(&z,&zx,&zy);rect_clip(zx,zy,1,1,C_GOLD);}}
}
/* Committee boat with flag hoist: orange line flag, class "12" to the start,
   then International Code P between the preparatory signal and one minute. */
static void draw_committee(int x,int y,int finish){
    v3 q[4];int i,n=0;uint8_t flag[3];
    memset(&prop,0,sizeof prop);prop.x=x*1000;prop.y=y*1000;use_boat(&prop);
    draw_hull(65,C_WHITE,C_CB_GREY,C_CB_GREY,C_CB_DARK);
    {v3 cab[4];cab[0]=bpt(-8,10,10);cab[1]=bpt(8,10,10);cab[2]=bpt(8,-30,10);cab[3]=bpt(-8,-30,10);poly3(cab,4,C_WHITE);cab[0]=bpt(-8,10,24);cab[1]=bpt(8,10,24);cab[2]=bpt(8,-30,24);cab[3]=bpt(-8,-30,24);poly3(cab,4,C_CB_GREY);}
    seg(0,-10,24,0,-10,110,C_SPAR);
    flag[n++]=finish?C_FLAG_BLUE:C_BUOY_L;if(!finish&&start_clock>0)flag[n++]=C_WHITE;if(!finish&&start_clock<=240&&start_clock>60)flag[n++]=C_FLAG_BLUE;
    for(i=0;i<n;i++){int z=105-i*22;q[0]=bpt(0,-10,z);q[1]=bpt(24,-10,z);q[2]=bpt(24,-10,z-16);q[3]=bpt(0,-10,z-16);poly3(q,4,flag[i]);if(flag[i]==C_FLAG_BLUE&&!finish){q[0]=bpt(8,-10,z-5);q[1]=bpt(16,-10,z-5);q[2]=bpt(16,-10,z-11);q[3]=bpt(8,-10,z-11);poly3(q,4,C_WHITE);}}
}
static void draw_lines(void){
    uint8_t fin=boats[0].leg>=course_len[course_sel]?C_GOLD:C_WHITE;
    if(start_line_active){
        wline(-LINE_HALF,0,LINE_HALF,0,C_WHITE);wline(-BOX_HALF,0,-LINE_HALF,0,C_RED);wline(LINE_HALF,0,BOX_HALF,0,C_GREEN);
        wline(-BOX_HALF,0,-BOX_HALF,-BOX_DEPTH,C_GREY);wline(BOX_HALF,0,BOX_HALF,-BOX_DEPTH,C_GREY);wline(-BOX_HALF,-BOX_DEPTH,BOX_HALF,-BOX_DEPTH,C_GREY);
    }else wline(-FINISH_HALF,FINISH_Y,FINISH_HALF,FINISH_Y,fin);
}
/* Name tags above rivals: P2..P4 for humans in their team colour, the
   syndicate code for AI yachts. */
static void draw_tag(const boat_t *b,int slot){
    char t[4];v3 p;int sx,sy;
    use_boat(b);p=bpt(0,MAST_Y,MAST_TOP+40);if(plane_d(&p,0)<200&&!top_view_mode)return;proj(&p,&sx,&sy);
    if(sx<vw.x0+12||sx>vw.x1-28||sy<CLIP_TOP+2||sy>CLIP_BOT-10)return;
    if(multiplayer&&net_live[slot]){t[0]='P';t[1]=(char)('0'+net_tag[slot]);t[2]=0;gx_text(sx-8,sy-8,t,T_WHITE,team_tx[b->team&3]);}
    else gx_text(sx-12,sy-8,teams[b->team&3].code,team_tx[b->team&3],T_NAVY);
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
    for(i=1;i<4;i++)if(!boats[i].finished||boats[i].retired)draw_tag(&boats[i],i);
}

/* ------------------------------------------------------------- panels -- */
static void panel(int x,int y,int w,int h){gx_rect(x,y,w,h,C_PANEL_LO);gx_rect(x+1,y+1,w-2,h-2,C_NAVY);gx_rect(x+1,y+1,w-2,1,C_PANEL_HI);gx_rect(x+1,y+1,1,h-2,C_PANEL_HI);}
static void header(const char *t){gx_rect(0,0,W,18,C_HDR);gx_rect(0,17,W,1,C_GOLD_D);gx_text(6,5,t,T_WHITE,T_HDR);gx_text(230,5,"NAPOLI 1997",T_GOLD,T_HDR);}
static void menu_backdrop(void){int i;for(i=0;i<12;i++){static const uint8_t m[12]={C_MENU0,C_MENU1,C_MENU2,C_MENU3,C_MENU4,C_MENU5,C_MENU6,C_MENU7,C_MENU8,C_MENU9,C_MENU10,C_MENU11};gx_rect(0,18+i*182/12,W,(i+1)*182/12-i*182/12,m[i]);}}
static void hint(const char *s){gx_rect(0,186,W,14,C_HDR);text_c(189,s,T_GREY,T_HDR);}
/* A scenic window: the 3D horizon of the current venue in a sub-view. */
static void scenic(int x0,int y0,int x1,int y1,int hor,uint16_t heading){
    vw.x0=x0;vw.y0=y0;vw.x1=x1;vw.y1=y1;vw.hor=hor;vw.cx=(x0+x1)/2;top_view_mode=0;
    camera_at(0,-3000,heading,0);draw_horizon();
}
static void restore_view(void){vw.cx=160;vw.hor=76;vw.x0=0;vw.y0=18;vw.x1=W;vw.y1=180;}
/* A showcase yacht sailing across a scenic window, in a team's colours. */
static void showcase_yacht(int team,uint16_t heading,int dist_dm,int kite){
    memset(&show,0,sizeof show);show.team=(uint8_t)team;show.heading=heading;show.x=(cam_wx+((dist_dm*cam_s)>>14))*100;show.y=(cam_wy+((dist_dm*cam_c)>>14))*100;
    show.v=2800<<8;show.sheet=30;show.kite=show.kite_want=(uint8_t)kite;show.kite_prog=kite?65535:0;
    twd=(uint16_t)(cam_h+DEG(150));tws10=120;update_rig(&show,0);show.boom=(int16_t)(show.awa>=0?-trim_opt(awa_deg(&show)):trim_opt(awa_deg(&show)));
    draw_boat(&show);
}

/* ------------------------------------------------------------- menus -- */
static void draw_title(void){
    int v=(int)((frame/600)%RACES);
    load_venue(v);cycle_glints();
    scenic(0,0,W,H,118,(uint16_t)(32768u+(int)(fsin((uint16_t)(frame*40))>>4)));
    showcase_yacht((int)((frame/600)%4),(uint16_t)(cam_h+DEG(80)+(int)(fsin((uint16_t)(frame*90))>>5)),520,KITE_GENN);
    restore_view();
    blit_rle(160-LOGO_W/2,12,LOGO_W,logo_rle,(int)sizeof logo_rle,0);
    panel(96,48,128,14);text_c(51,"NAPOLI 1997",T_GOLD,T_NAVY);
    text_c(64,race_names[v],T_WHITE,T_HDR);
    if(frame&32){panel(76,172,168,14);text_c(175,"PRESS A TO SET SAIL",T_WHITE,T_NAVY);}
}
static void draw_mode(void){
    static const char items[3][20]={"SINGLE PLAYER","NETWORK MULTIPLAYER","AUTO TRIM"};int i;
    menu_backdrop();header("CHAMPIONSHIP MODE");
    blit_rle(144,26,32,cup_rle,(int)sizeof cup_rle,0);
    for(i=0;i<3;i++){int y=76+i*28;panel(40,y,240,22);if(menu==i)gx_rect(42,y+2,236,18,C_SEL);gx_text(56,y+7,items[i],menu==i?T_GOLD:T_WHITE,menu==i?T_SEL:T_NAVY);if(i==2)gx_text(224,y+7,auto_trim?"ON ":"OFF",auto_trim?T_GREEN:T_GREY,menu==i?T_SEL:T_NAVY);}
    gx_text(48,164,menu==2?"AUTO TRIM: THE CREW TRIMS FOR YOU":menu==1?"UP TO 4 CONSOLES, SAME COURSE":"5 RACES IN THE BAY OF NAPLES",T_CYAN,T_NAVY);
    hint("UP/DOWN  A CONFIRM  B BACK");
}
static void draw_team(void){
    int i;menu_backdrop();header("CHOOSE YOUR SYNDICATE");
    for(i=0;i<4;i++){int y=24+i*38,s=i==team_sel;panel(8,y,148,34);if(s)gx_rect(10,y+2,144,30,C_SEL);gx_rect(14,y+6,6,22,team_c[i]);gx_rect(20,y+6,2,22,team_d[i]);gx_text(28,y+7,teams[i].name,s?T_GOLD:T_WHITE,s?T_SEL:T_NAVY);gx_text(28,y+19,teams[i].code,team_tx[i],s?T_SEL:T_NAVY);}
    /* The chosen yacht, sailing in a window onto the Bay. */
    panel(162,24,152,150);load_venue(2);cycle_glints();
    scenic(164,26,312,172,92,(uint16_t)(32768u+(frame*60)));
    showcase_yacht(team_sel,(uint16_t)(cam_h+DEG(80)),420,KITE_GENN);restore_view();
    {int y=136;gx_rect(166,y,144,34,C_HDR);gx_text(170,y+3,"SPEED",T_GREY,T_HDR);gx_text(170,y+13,"TURN",T_GREY,T_HDR);gx_text(170,y+23,"CREW",T_GREY,T_HDR);
     for(i=0;i<8;i++){gx_rect(214+i*11,y+3,9,6,i<teams[team_sel].speed?C_CYAN:C_PANEL);gx_rect(214+i*11,y+13,9,6,i<teams[team_sel].turn?C_CYAN:C_PANEL);gx_rect(214+i*11,y+23,9,6,i<teams[team_sel].crew?C_CYAN:C_PANEL);}}
    hint("D-PAD CHOOSE  A CONFIRM  B BACK");
}
static void statrow(int y,const char *n,int v,int s){int i;if(s)gx_rect(18,y-3,284,14,C_SEL);gx_text(30,y,n,s?T_GOLD:T_WHITE,s?T_SEL:T_NAVY);for(i=0;i<8;i++)gx_rect(124+i*12,y,10,7,i<v?C_CYAN:C_PANEL);if(v<8){gx_text(226,y,"COST",T_GREY,s?T_SEL:T_NAVY);small_num(262,y,upgrade_cost(v),0,money>=upgrade_cost(v)?T_GREEN:T_RED,s?T_SEL:T_NAVY);}else gx_text(226,y,"MAX",T_GOLD,s?T_SEL:T_NAVY);}
static void draw_manager(void){
    int next=sponsor<3?sponsors[sponsor+1].min_wins:0;
    menu_backdrop();header("TEAM HQ");
    panel(8,22,304,26);gx_rect(14,28,6,14,team_c[team_sel]);gx_text(26,25,teams[team_sel].name,T_GOLD,T_NAVY);gx_text(26,36,"RACE",T_GREY,T_NAVY);digit(62,36,race_no+1,T_WHITE,T_NAVY);gx_text(70,36,"/5",T_WHITE,T_NAVY);
    gx_text(184,25,"CASH K",T_WHITE,T_NAVY);number(232,25,money,T_GOLD,T_NAVY);gx_text(184,36,"WINS",T_GREY,T_NAVY);digit(232,36,wins,T_WHITE,T_NAVY);gx_text(250,36,"PTS",T_GREY,T_NAVY);small_num(282,36,points[team_sel],0,T_WHITE,T_NAVY);
    panel(8,52,304,128);
    if(menu==0)gx_rect(18,57,284,14,C_SEL);
    gx_text(30,60,sponsors[sponsor].name,menu==0?T_GOLD:T_WHITE,menu==0?T_SEL:T_NAVY);gx_text(200,60,"FEE",T_GREY,menu==0?T_SEL:T_NAVY);small_num(232,60,sponsors[sponsor].fee,0,T_WHITE,menu==0?T_SEL:T_NAVY);
    if(sponsor<3)gx_text(30,73,wins>=next?"A: SIGN A RICHER SPONSOR":"MORE WINS UNLOCK SPONSORS",wins>=next?T_GREEN:T_GREY,T_NAVY);
    statrow(90,"HULL",hull,menu==1);statrow(106,"SAILS",sails,menu==2);statrow(122,"CREW",crew,menu==3);statrow(138,"STRATEGY",strategy,menu==4);
    if(menu==5)gx_rect(18,155,284,16,C_SEL);gx_text(30,159,"RACE WEEKEND >",menu==5?T_GOLD:T_WHITE,menu==5?T_SEL:T_NAVY);gx_text(158,159,race_names[race_no],T_CYAN,menu==5?T_SEL:T_NAVY);
    hint(menu>=1&&menu<=4?"A INVEST  B SELL HALF PRICE":"UP/DOWN  A SELECT");
}
/* Course diagram for the briefing: marks, legs, line and the breeze. */
static void course_map(int x,int y,int c){
    int i,px=x+60,py=y+92,lx=px,ly=py;
    gx_rect(x,y,120,104,C_MAP_DEEP);gx_rect(x+2,y+2,116,100,C_MAP_SEA);
    for(i=0;i<course_len[c];i++){int mx=px+course_x[c][i]*50/900,my=py-course_y[c][i]*80/900;line2(lx,ly,mx,my,C_LAYLINE);lx=mx;ly=my;}
    line2(lx,ly,px,py+FINISH_Y*80/900,C_LAYLINE);
    for(i=0;i<course_len[c];i++){int mx=px+course_x[c][i]*50/900,my=py-course_y[c][i]*80/900;gx_rect(mx-2,my-2,5,5,C_BUOY_L);digit(mx+4,my-3,i+1,T_WHITE,T_NAVY);}
    gx_rect(px-LINE_HALF*50/900,py,LINE_HALF*100/900,1,C_WHITE);gx_rect(px-FINISH_HALF*50/900,py-FINISH_Y*80/900,FINISH_HALF*100/900,1,C_GOLD);
    line2(x+10,y+8,x+10,y+20,C_GOLD);gx_rect(x+9,y+20,3,3,C_GOLD);
}
static void draw_brief(void){
    load_venue(venue());cycle_glints();
    scenic(0,18,W,76,62,32768u);restore_view();header(race_names[race_no]);
    panel(96,22,128,12);text_c(24,race_light[venue()],T_GOLD,T_NAVY);
    panel(4,76,312,110);
    course_map(8,79,course_sel);
    gx_text(136,82,"RACE PATH",T_GREY,T_NAVY);gx_text(136,93,course_names[course_sel],T_GOLD,T_NAVY);
    gx_text(136,108,"SW 8-16 KT, SHIFTING",T_WHITE,T_NAVY);gx_text(136,118,"RRS 10-14, 18, 31",T_WHITE,T_NAVY);
    gx_text(136,134,"TAP A  SPINNAKER",T_CYAN,T_NAVY);gx_text(136,144,"TAP B  GENNAKER",T_CYAN,T_NAVY);gx_text(136,154,"HOLD A PENALTY TURN",T_CYAN,T_NAVY);gx_text(136,164,"HOLD B RACE MENU",T_CYAN,T_NAVY);gx_text(136,174,"UP/DOWN SHEETS",T_CYAN,T_NAVY);
    hint("L/R COURSE  A LEAVE DOCK  B HQ");
}
static void draw_lobby(void){
    int i,n=fairwind_net_peer_count(),row=0,late=lobby_race_running();prg32_multiplayer_peer_t p;
    menu_backdrop();header("MULTIPLAYER LOBBY");
    panel(16,24,288,30);gx_text(26,29,"ROOM",T_GREY,T_NAVY);gx_text(66,29,course_names[course_sel],T_GOLD,T_NAVY);gx_text(26,41,"PLAYERS",T_GREY,T_NAVY);digit(90,41,n+1,n?T_GREEN:T_WHITE,T_NAVY);gx_text(98,41,"/4",T_WHITE,T_NAVY);
    panel(16,58,288,100);
    for(i=-1;i<n&&row<4;i++){
        int y=64+row*22,t,ready,tag;const char *st;
        if(i<0){t=race_team;ready=local_ready;tag=human_tag(fairwind_net_local_id());st=ready?"READY":"PRESS A";}
        else{if(!fairwind_net_peer(i,&p))continue;t=(p.flags>>3)&3;ready=(p.flags&(NF_READY|NF_RACING))!=0;tag=human_tag(p.player_id);st=p.flags&NF_RACING?"RACING":ready?"READY":"WAITING";}
        gx_rect(24,y,6,16,team_c[t]);
        {char tg[3]={'P',(char)('0'+tag),0};gx_text(36,y+4,tg,T_WHITE,T_NAVY);}
        gx_text(60,y+4,teams[t].name,i<0?T_GOLD:T_WHITE,T_NAVY);gx_text(236,y+4,st,ready?T_GREEN:T_GREY,T_NAVY);
        if(i<0)gx_text(196,y+4,"YOU",T_CYAN,T_NAVY);
        row++;
    }
    if(race_team!=team_sel)gx_text(24,148,"SYNDICATE TAKEN: COLOURS CHANGED",T_GOLD,T_NAVY);
    panel(16,162,288,20);text_c(168,late?"RACE IN PROGRESS - PLEASE WAIT":n?(local_ready?"WAITING FOR EVERYONE...":"PRESS A WHEN READY"):"WAITING FOR OTHER CONSOLES...",late?T_GOLD:T_WHITE,T_NAVY);
    hint("A READY  B RACE AI INSTEAD");
}

/* ----------------------------------------------------------------- HUD -- */
/* Wind instrument: true wind (gold) and apparent wind (cyan) relative to the
   bow, true wind speed, and the current shift as a lift or header. */
static void draw_wind_gauge(void){
    const boat_t *b=&boats[0];int i,shift=wind_shift_deg(),upwind=absi(b->twa)<DEG(90);uint16_t rel=(uint16_t)(twd-b->heading);
    panel(2,20,58,62);
    for(i=0;i<16;i++)gx_rect(31+(int)((22*fsin((uint16_t)(i*4096)))>>14),46-(int)((22*fcos((uint16_t)(i*4096)))>>14),1,1,i&3?C_GREY:C_WHITE);
    gx_rect(30,40,3,13,C_HULL_L);gx_rect(31,38,1,2,C_HULL_L);
    line2(31+(int)((22*fsin(rel))>>14),46-(int)((22*fcos(rel))>>14),31+(int)((8*fsin(rel))>>14),46-(int)((8*fcos(rel))>>14),C_GOLD);
    gx_rect(30+(int)((8*fsin(rel))>>14),45-(int)((8*fcos(rel))>>14),3,3,C_GOLD);
    gx_rect(30+(int)((18*fsin((uint16_t)b->awa))>>14),45-(int)((18*fcos((uint16_t)b->awa))>>14),3,3,C_CYAN);
    small_num(4,72,tws10,1,T_WHITE,T_NAVY);
    if(upwind&&absi(shift)>=3){int lift=shift*b->tack>0;gx_text(38,72,lift?"L":"H",lift?T_GREEN:T_RED,T_NAVY);digit(46,72,absi(shift),lift?T_GREEN:T_RED,T_NAVY);}
}
/* North is not up: the map, like the course, puts the SW breeze at the top. */
static int map_x(int32_t x){return 257+(int)((x+FIELD_X)/30);}
static int map_y(int32_t y){return 24+(int)((FIELD_Y1-y)/30);}
static void draw_minimap(void){
    int i,len=course_len[course_sel],nx,ny,nl=boats[0].leg;
    gx_rect(254,20,66,66,C_PANEL_LO);gx_rect(257,24,60,57,C_MAP_SEA);gx_rect(254,20,3,66,C_TUFA_L);gx_rect(317,20,3,66,C_GREEN_L);gx_rect(257,81,60,5,C_CITY_L);
    for(i=258;i<316;i+=4)gx_rect(i,23,2,1,C_GOLD);
    if(start_line_active)gx_rect(map_x(-LINE_HALF),map_y(0),map_x(LINE_HALF)-map_x(-LINE_HALF),1,C_WHITE);else gx_rect(map_x(-FINISH_HALF),map_y(FINISH_Y),map_x(FINISH_HALF)-map_x(-FINISH_HALF),1,nl>=len?C_GOLD:C_WHITE);
    for(i=0;i<len;i++)gx_rect(map_x(course_x[course_sel][i])-1,map_y(course_y[course_sel][i])-1,2,2,C_BUOY_L);
    if(nl<len){
        nx=map_x(course_x[course_sel][nl]);ny=map_y(course_y[course_sel][nl]);
        if(frame&16)gx_rect(nx-2,ny-2,4,4,C_GOLD);
        /* Strategy upgrades plot laylines to a windward mark. */
        if(strategy>=2&&absi(BAM2DEG((int16_t)(bearing(course_x[course_sel][nl]*1000-boats[0].x,course_y[course_sel][nl]*1000-boats[0].y)-twd)))<60)
            for(i=-1;i<=1;i+=2){uint16_t a=(uint16_t)(twd+32768u+i*DEG(42));line2(nx,ny,nx+(int)((22*fsin(a))>>14),ny-(int)((22*fcos(a))>>14),C_LAYLINE);}
    }
    for(i=3;i>=0;i--){int bx=map_x(boats[i].x/1000),by=map_y(boats[i].y/1000);if(i){gx_rect(bx-1,by-1,2,2,team_c[boats[i].team&3]);continue;}line2(bx,by,bx+(int)((5*fsin(boats[0].heading))>>14),by-(int)((5*fcos(boats[0].heading))>>14),C_WHITE);gx_rect(bx-1,by-1,3,3,C_WHITE);}
    line2(262+(int)((5*fsin(twd))>>14),76-(int)((5*fcos(twd))>>14),262,76,C_GOLD);gx_rect(261,75,3,3,C_GOLD);
}
static void draw_start_signal(void){
    const char *msg;if(start_clock>300)msg="10 MIN - WARNING";else if(start_clock>240)msg="5 MIN - CLASS SIGNAL";else if(start_clock>120)msg="4 MIN - P FLAG UP";else if(start_clock>60)msg="2 MIN - ENTER THE BOX";else if(start_clock>0)msg="1 MIN - P FLAG DOWN";else msg="START - CLASS FLAG DOWN";
    panel(64,20,188,time_scale()>RACE_TIME_SCALE?24:14);gx_text(68,23,msg,start_clock?T_WHITE:T_GOLD,T_NAVY);if(time_scale()>RACE_TIME_SCALE)gx_text(68,33,">> x20  TIME FLIES",T_GOLD,T_NAVY);
}
/* Where to: an arrow to the next mark (or the finish) relative to the bow,
   its distance and the live position in the fleet. */
static void draw_guidance(void){
    const boat_t *b=&boats[0];int32_t mx,my,dx,dy;int d,ax=112,ay=26,rk=live_rank();uint16_t rel;char mk[4]={'M','K','0',0},pos[5]={'P','0','/','4',0};
    if(b->finished)return;
    leg_target(b->leg,&mx,&my);dx=(mx-b->x)/1000;dy=(my-b->y)/1000;d=isqrt((uint32_t)(dx*dx+dy*dy));rel=(uint16_t)(bearing(mx-b->x,my-b->y)-b->heading);
    panel(100,20,120,14);
    line2(ax-(int)((5*fsin(rel))>>14),ay+(int)((5*fcos(rel))>>14),ax+(int)((5*fsin(rel))>>14),ay-(int)((5*fcos(rel))>>14),C_GOLD);
    gx_rect(ax+(int)((5*fsin(rel))>>14)-1,ay-(int)((5*fcos(rel))>>14)-1,3,3,C_GOLD);
    if(b->leg<course_len[course_sel]){mk[2]=(char)('1'+b->leg);gx_text(122,23,mk,T_WHITE,T_NAVY);}else gx_text(122,23,"FIN",T_GOLD,T_NAVY);
    small_num(148,23,d,0,T_WHITE,T_NAVY);gx_text(148+(d>=1000?32:d>=100?24:d>=10?16:8),23,"M",T_GREY,T_NAVY);
    pos[1]=(char)('0'+rk);gx_text(184,23,pos,rk==1?T_GOLD:T_CYAN,T_NAVY);
}
static void draw_hud(void){
    const boat_t *b=&boats[0];int opt=trim_opt(awa_deg(b)),err=absi(b->boom)-opt,kp=b->kite_prog*100/65535,twa=absi(BAM2DEG(b->twa));const char *msg=0;uint16_t mc=T_GOLD;
    gx_rect(0,CLIP_BOT,W,H-CLIP_BOT,C_HDR);gx_rect(0,CLIP_BOT,W,1,C_GOLD_D);
    gx_text(4,182,start_clock?"T-":"R ",start_clock?T_GOLD:T_WHITE,T_HDR);clock_mmss(20,182,start_clock?start_clock:race_clock,start_clock?T_GOLD:T_WHITE,T_HDR);
    small_num(68,182,(int)((b->v>>8)*100/5144),1,T_WHITE,T_HDR);gx_text(100,182,"KT",T_GREY,T_HDR);
    gx_text(122,182,"TWA",T_GREY,T_HDR);small_num(148,182,twa,0,T_WHITE,T_HDR);
    gx_text(180,182,b->kite==KITE_SPIN?"SPI":b->kite==KITE_GENN?"GEN":"JIB",b->kite?team_tx[b->team&3]:T_WHITE,T_HDR);
    if(b->kite!=b->kite_want||(b->kite&&kp<100)){small_num(206,182,kp,0,T_GOLD,T_HDR);gx_text(230,182,b->kite!=b->kite_want?"v":"^",T_GOLD,T_HDR);}
    gx_text(250,182,auto_trim?"AUTO":"MAN ",auto_trim?T_CYAN:T_GREY,T_HDR);
    if(multiplayer){gx_text(286,182,"N",T_GREEN,T_HDR);digit(294,182,peer_count+1,T_GREEN,T_HDR);gx_text(302,182,"/4",T_WHITE,T_HDR);}
    gx_text(4,191,"TRIM",T_GREY,T_HDR);gx_rect(40,192,92,6,C_TRIM_BG);gx_rect(40+opt-4,192,9,6,C_TRIM_OK);gx_rect(40+b->sheet,197,1,2,C_GOLD);gx_rect(40+clamp(absi(b->boom),0,90),191,2,8,C_WHITE);
    gx_text(136,191,err>8?"LUFF ":err<-12?"STALL":"GOOD ",err>8?T_RED:err<-12?T_GOLD:T_GREEN,T_HDR);
    if(b->retired)msg="RETIRED";
    else if(b->finished)msg="FINISHED";
    else if(b->serving)msg="PENALTY TURN";
    else if(b->penalty){msg=(frame&64)?rule_names[b->rule]:"HOLD A: PENALTY";mc=T_RED;}
    else if(b->aground){msg=b->y>(FIELD_Y1-20)*1000?"AREA LIMIT":"AGROUND";mc=T_RED;}
    else if(start_clock>120&&b->y<=15000)msg="WAIT FOR 2 MIN";
    else if(start_clock>480)msg="HOLD B: RACE MENU";
    else if(start_clock<=120&&!b->entered_box)msg=(b->team&1)?"ENTER STBD SIDE":"ENTER PORT SIDE";
    else if(!b->started&&start_clock==0)msg="CROSS THE LINE";
    else if(b->started&&b->kite==KITE_NONE&&b->kite_want==KITE_NONE&&twa>=125)msg="TAP A: SPINNAKER";
    else if(b->started&&b->kite==KITE_NONE&&b->kite_want==KITE_NONE&&twa>=75&&twa<125)msg="TAP B: GENNAKER";
    else if(b->kite!=KITE_NONE&&b->kite_want==b->kite&&twa<(b->kite==KITE_SPIN?95:60)){msg=b->kite==KITE_SPIN?"TAP A: DROP KITE":"TAP B: DROP KITE";mc=T_RED;}
    if(msg)gx_text(176,191,msg,mc,T_HDR);
}
static void draw_toast(void){int w;if(toast_frames<=0||!toast_msg)return;w=text_len(toast_msg)*8+12;panel(160-w/2,toast_frames>TOAST_FRAMES-6?52:48,w,14);text_c(toast_frames>TOAST_FRAMES-6?55:51,toast_msg,toast_fg,T_NAVY);}
static void draw_race_menu(void){
    static const char items[4][13]={"RESUME","AUTO TRIM","RETIRE","RESTART RACE"};int i,n=multiplayer?3:4;
    panel(80,58,160,24+n*16);text_c(62,multiplayer?"RACE MENU - LIVE":"PAUSED",T_GOLD,T_NAVY);
    for(i=0;i<n;i++){int y=76+i*16,s=i==race_menu_sel;if(s)gx_rect(84,y-3,152,14,C_SEL);gx_text(96,y,items[i],s?T_GOLD:T_WHITE,s?T_SEL:T_NAVY);if(i==1)gx_text(200,y,auto_trim?"ON ":"OFF",auto_trim?T_GREEN:T_GREY,s?T_SEL:T_NAVY);}
}
static void draw_race(void){
    load_venue(venue());cycle_glints();restore_view();camera_setup();
    if(top_view_mode){int y;for(y=CLIP_TOP;y<CLIP_BOT;y+=8)gx_rect(0,y,W,8,(y>>3)&1?C_TOPSEA0:C_TOPSEA1);}else draw_horizon();
    draw_land();draw_ripples();draw_lines();draw_objects();
    header(race_names[race_no]);draw_wind_gauge();draw_minimap();
    if(start_line_active&&start_clock>0)draw_start_signal();else draw_guidance();
    if(top_view_mode){panel(124,166,72,13);gx_text(128,169,"TOP VIEW",T_GOLD,T_NAVY);}
    draw_toast();draw_hud();
    if(race_menu)draw_race_menu();
}
static void draw_result(void){
    int i;menu_backdrop();header("RACE RESULT");
    panel(8,22,304,26);gx_text(18,26,"PLACE",T_WHITE,T_NAVY);digit(66,26,player_rank,player_rank==1?T_GOLD:T_WHITE,T_NAVY);gx_text(150,26,"PRIZE K",T_WHITE,T_NAVY);small_num(210,26,last_prize,0,T_GOLD,T_NAVY);
    gx_text(18,37,course_names[course_sel],T_CYAN,T_NAVY);gx_text(150,37,"FEE K",T_GREY,T_NAVY);small_num(210,37,sponsors[sponsor].fee,0,T_WHITE,T_NAVY);
    panel(8,52,304,110);
    for(i=0;i<4;i++){
        const boat_t *b=&boats[result_order[i]];int y=58+i*26,me=result_order[i]==0,bar=(4-i)*24;
        if(me)gx_rect(10,y-2,300,24,C_SEL);
        gx_rect(18,y+18-bar/6,10,bar/6,i==0?C_GOLD:i==1?C_CB_GREY:i==2?C_TUFA_D:C_PANEL);digit(20,y+2,i+1,T_WHITE,me?T_SEL:T_NAVY);
        gx_rect(36,y,6,18,team_c[b->team&3]);gx_text(50,y+2,teams[b->team&3].name,me?T_GOLD:T_WHITE,me?T_SEL:T_NAVY);
        if(multiplayer&&(me||net_live[result_order[i]])){char t[3]={'P',(char)('0'+(me?my_tag:net_tag[result_order[i]])),0};gx_text(50,y+12,t,T_CYAN,me?T_SEL:T_NAVY);}
        if(b->retired)gx_text(220,y+2,"RET",T_RED,me?T_SEL:T_NAVY);else if(b->finish_time>=RACE_LIMIT_SECONDS)gx_text(220,y+2,"DNF",T_GREY,me?T_SEL:T_NAVY);else clock_mmss(220,y+2,b->finish_time,T_WHITE,me?T_SEL:T_NAVY);
        small_num(270,y+2,4-i,0,T_GREEN,me?T_SEL:T_NAVY);gx_text(282,y+2,"PT",T_GREY,me?T_SEL:T_NAVY);
    }
    panel(8,166,304,16);text_c(170,player_rank==1?"VICTORY! WIN BONUS PAID":"SPONSOR PAYMENT RECEIVED",player_rank==1?T_GOLD:T_GREEN,T_NAVY);
    hint("A - CONTINUE");
}
static void draw_season(void){
    int i,best=0,n;prg32_score_t s;menu_backdrop();header("FINAL CLASSIFICATION");
    for(i=1;i<4;i++)if(points[i]>points[best])best=i;
    blit_rle(144,22,32,cup_rle,(int)sizeof cup_rle,0);
    panel(40,64,240,14);text_c(67,best==team_sel?"YOU WON THE AULD MUG!":"THE CUP HAS A WINNER",T_GOLD,T_NAVY);
    panel(8,82,150,82);for(i=0;i<4;i++){int y=88+i*18;gx_rect(14,y,6,12,team_c[i]);gx_text(24,y+2,teams[i].code,i==team_sel?T_GOLD:T_WHITE,T_NAVY);small_num(60,y+2,points[i],0,T_WHITE,T_NAVY);if(i==best)gx_text(94,y+2,"CUP",T_GOLD,T_NAVY);}
    panel(162,82,150,82);gx_text(170,86,"HALL OF FAME",T_CYAN,T_NAVY);n=fairwind_score_count();
    for(i=0;i<n&&i<5;i++)if(fairwind_score_get(i,&s)){s.player[8]=0;gx_text(170,98+i*12,s.player,T_WHITE,T_NAVY);small_num(244,98+i*12,(int)(s.score/1000),0,T_GOLD,T_NAVY);gx_text(276,98+i*12,"PT",T_GREY,T_NAVY);}
    if(!n)gx_text(170,100,"NO ENTRIES YET",T_GREY,T_NAVY);
    hint("A - RETURN TO TITLE");
}
void fairwind_draw(void){
    if(screen!=ST_RACE)restore_view();
    if(screen==ST_TITLE)draw_title();else if(screen==ST_MODE)draw_mode();else if(screen==ST_TEAM)draw_team();else if(screen==ST_MANAGER)draw_manager();else if(screen==ST_BRIEF)draw_brief();else if(screen==ST_LOBBY)draw_lobby();else if(screen==ST_RACE)draw_race();else if(screen==ST_RESULT)draw_result();else draw_season();
    if(screen!=ST_RACE&&toast_frames>0){toast_frames--;draw_toast();}
    prg32_gfx_present();
}
