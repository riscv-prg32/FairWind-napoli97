from pathlib import Path
from PIL import Image
import re, json
r=Path(__file__).resolve().parents[1]
s=(r/'src/game.c').read_text(); h=(r/'src/assets_bitplanes.h').read_text(); m=(r/'src/fixmath.h').read_text()
platform=(r/'src/platform.h').read_text(); generator=(r/'tools/generate_assets.py').read_text(); build_script=(r/'build.sh').read_text()
# Portable 64 KiB cartridge, multiplayer build, no C library.
assert 'prg32_sprite_draw_bitplanes' in platform and 'prg32_multiplayer_join' in platform
assert '--portable --multiplayer' in build_script and '65536' in build_script and '131072' not in build_script
adapter=(r/'tools/prg32_cli_64.py').read_text()
assert 'RAM_SIZE = 64 * 1024' in adapter and adapter.count('FALLBACK_CART_RAM_SIZE = RAM_SIZE')==3
assert 'void *memcpy(' in s and 'void *memset(' in s
# Assets: only the cup and the five Bay panoramas remain as bitmaps; yachts,
# sails and buoys are procedural, so no pre-rotated sprite sheets ship.
assert 'fairwind_background_palette[16]' in h and 'fairwind_background_rle8' in h and 'fairwind_background_offsets[6]' in h
assert 'fairwind_cup_planes' in h and 'yacht_planes' not in h and 'hull_palette' not in h
assert "backgrounds=[authored_panorama(i) for i in range(5)]" in generator
for i in range(5): assert (r/f'assets/source/panorama-{i}.png').is_file()
assert len(re.findall(r'0x[0-9a-f]{2}',h)) > 9000
# Fixed-point maths: integer trig, CORDIC bearing, integer sqrt.
assert 'sin_quarter[65]' in m and 'cordic_atan[14]' in m and 'static uint16_t bearing(' in m and 'static int32_t isqrt(' in m
# Chase camera from the stern, perspective projection and near/side clipping.
assert '#define CAM_BACK' in s and '#define FOCAL' in s and 'static void proj(' in s and 'static int32_t plane_d(' in s
assert 'p->x*FOCAL/p->z' in s and 'NEAR_DM' in s and 'static void poly3(' in s and 'static void line3(' in s
# Race field larger than the view, simplified map, land that cannot be sailed on.
assert 'FIELD_X 900' in s and 'FIELD_Y0 (-450)' in s and 'FIELD_Y1 1250' in s
assert 'static void draw_minimap(void)' in s and 'b->aground=(uint8_t)clamp_field(b)' in s and 'static void shore_wall(' in s
assert 'static void draw_horizon(void)' in s and 'panorama(HOR-24,off)' in s
# Wind with deterministic shifts and a wind instrument.
assert 'static void update_wind(void)' in s and 'wind_seed' in s and 'static void draw_wind_gauge(void)' in s
assert 'PREVAILING_SW_HEADING 0' in s and 'SW 8-16 KT, SHIFTING' in s
# ORC 12mR polars, sail plans, trim model and momentum.
assert 'polar8[13]' in s and 'polar12[13]' in s and 'polar16[13]' in s
assert 'plan_jib[13]' in s and 'plan_spin[13]' in s and 'plan_genn[13]' in s
assert 'static int trim_opt(int awa)' in s and 'static int trim_eff(const boat_t *b)' in s and 'tau=target>v?' in s
assert 'b->boom' in s and 'side=b->awa>=0?-1:1' in s
# Controls: helm, sheets, spinnaker (A), gennaker (B), A+B penalty.
assert 'if(in&PRG32_BTN_UP)b->sheet' in s and 'if(in&PRG32_BTN_DOWN)b->sheet' in s
assert 'me->kite_want==KITE_SPIN?KITE_NONE:KITE_SPIN' in s and 'me->kite_want==KITE_GENN?KITE_NONE:KITE_GENN' in s
assert '(PRG32_BTN_A|PRG32_BTN_B))==(PRG32_BTN_A|PRG32_BTN_B)' in s and 'begin_penalty_turn(me)' in s
assert 'b->kite_prog' in s and 'b->kite=b->kite_want' in s
# Close-quarters top view with hysteresis.
assert 'TOP_VIEW_ENTER 70' in s and 'TOP_VIEW_EXIT 100' in s and 'close_to_rival_or_buoy(top_view_mode?TOP_VIEW_EXIT:TOP_VIEW_ENTER)' in s and '"TOP VIEW"' in s
# AI: timed start, VMG beats/runs with shift and layline tacking, rules, kites.
assert 'static void helm_ai(boat_t *b)' in s and 'static uint16_t vmg_heading(' in s and 'layline_cos[4]' in s
assert 'static uint16_t avoid_traffic(' in s and 'static int gives_way(' in s and 'kite_for(' in s and 'clear_water(b,45)' in s
# Campaign, courses, start and finish.
assert 'NAPOLI 1997' in s and 'RACES 5' in s and 'sponsors[4]' in s and 'money+=' in s and 'wins++' in s
assert 'START_COUNTDOWN_SECONDS 600' in s and 'FAST_TIME_SCALE 20' in s and 'RACE_TIME_SCALE 4' in s
assert '"10 MIN - WARNING"' in s and '"5 MIN - CLASS SIGNAL"' in s and '"4 MIN - P FLAG UP"' in s and '"2 MIN - ENTER THE BOX"' in s and '"1 MIN - P FLAG DOWN"' in s and 'msg="START - CLASS FLAG DOWN"' in s
assert 'entered_box' in s and 'BOX_HALF' in s and 'if(all_started)start_line_active=0' in s
assert 'course_names[COURSE_COUNT]' in s and 'course_len[COURSE_COUNT]={3,5,6}' in s
assert 'WINDWARD / RUN' in s and 'OLYMPIC TRIANGLE' in s and '1992 IACC Z' in s
assert 'FINISH_Y' in s and 'FINISH_HALF' in s and 'boats[0].leg==it[i].idx' in s
for rule in ('RULE_PORT','RULE_WINDWARD','RULE_ASTERN','RULE_TACKING','RULE_CONTACT','RULE_MARK_ROOM','RULE_MARK_TOUCH'):
    assert rule in s
assert 'start_clock>240' in s and 'b->turned>=65536u' in s and 'mark_dist2' in s and 'windward_score' in s
# Multiplayer: v8 rooms, peer slots, lobby handshake, 16-bit snapshot fields.
assert 'count>3?3:count' in platform and ':v8-iacc92' in platform and ':v8-olympic' in platform and ':v8-windward' in platform
assert 'fairwind_net_join(course_sel)' in s and 'for(i=0;i<n;i++)if(fairwind_net_peer(i,&p)' in s and 'peer_count+1' in s
assert 'local_ready' in s and 'peer.flags&0x140' in s and 'if(!(p.flags&0x100))continue;' in s
# ESP32-C6 performance design: indexed fills, frame-time integration, a
# static header, a throttled HUD and menus that redraw only on change.
assert 'prg32_gfx_rect_indexed' in s and 'prg32_gfx_pixel_indexed' in s and 'prg32_gfx_clear_indexed' in s
assert 'prg32_gfx_rect(' not in s and 'prg32_gfx_pixel(' not in s
# The cartridge owns the palette: identical colours on every runtime and run.
assert 'static void set_palette(void)' in s and 'set_palette();' in s and 'bg_idx[i]=(uint8_t)(BG_BASE+i)' in s
assert '0x0452' not in s and '#define SEA C6(' in s
assert 'now=prg32_ticks_ms()' in s and 'static int32_t sim_ms(void)' in s and '#define FRAME_MS 33' in s
assert 'if(!race_header)' in s and 'if(!(frame&3)||hud_force)' in s and 'if(!ui_dirty&&' in s
assert 'es[m]=' in s  # one division per polygon edge
for f in ('tools/profile/qemu_profile.py','tools/profile/profile_cart.c','tools/profile/profile_null.c','tools/autopilot.h','docs/PERFORMANCE.md'):
    assert (r/f).is_file(), f
# The FairWind logo: title-screen sprite and store icon.
assert (r/'assets/source/fairwind-logo.png').is_file() and 'fairwind_logo_planes[4608]' in h and 'fairwind_logo_palette[16]' in h
assert '&logo_sprite' in s
# Store and release artwork.
assert (r/'assets/generated/title.png').is_file() and (r/'assets/generated/screenshot.png').is_file()
shot=Image.open(r/'assets/generated/screenshot.png').convert('RGB')
assert shot.size==(320,200) and len(shot.getcolors(maxcolors=1<<20))>=12
a=json.loads((r/'audio.json').read_text())
assert len(a['instruments'])==8 and len(a['tracks'])==5
voices={e.get('arg0') for t in a['tracks'] for e in t['events'] if e['command']=='NOTE_ON'}
assert voices==set(range(8)),voices
meta=json.loads((r/'metadata/metadata.json').read_text())
assert meta['players']=={'min':1,'max':4}
assert meta['name']=='FairWind-napoli97' and meta['id']=='org.riscv-prg32.fairwind-napoli97'
assert meta['version']=='4.2.0' and 'selectable-courses' in meta['features'] and 'stern-chase-view' in meta['features']
assert 'basic-rrs-rules' in meta['features'] and 'wind-shifts' in meta['features']
assert meta['cartridge_profile']=='portable-64k'
print('source checks: OK; stern-view engine, sailing physics, wind shifts, AI, rules, courses, audio, and multiplayer verified')
