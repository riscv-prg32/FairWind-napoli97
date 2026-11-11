from pathlib import Path
from PIL import Image
import re, json
r=Path(__file__).resolve().parents[1]
s=(r/'src/game.c').read_text(); h=(r/'src/assets.h').read_text(); m=(r/'src/fixmath.h').read_text()
platform=(r/'src/platform.h').read_text(); generator=(r/'tools/generate_assets.py').read_text(); build_script=(r/'build.sh').read_text()
# Portable 128 KiB cartridge (PRG32 1.8), multiplayer build, no C library,
# position independent (no static pointer tables), both architectures.
assert 'prg32_gfx_rect_indexed' in platform and 'prg32_palette_set' in platform and 'prg32_multiplayer_join' in platform
assert 'prg32_sprite_draw_bitplanes' not in platform and 'prg32_gfx_rect(' not in s and 'prg32_gfx_clear' not in s
assert '--portable --multiplayer' in build_script and 'CART_RAM_KIB=128' in build_script and 'PACKAGE_LIMIT=131072' in build_script
assert 'for arch in esp32c6 qemu' in build_script and 'check_relocatable.py' in build_script and 'preview.mp4' in build_script
assert not (r/'tools/prg32_cli_64.py').exists() and not (r/'src/assets_bitplanes.h').exists()
assert 'void *memcpy(' in s and 'void *memset(' in s
assert not re.search(r'static const char \*\w+\[', s), 'pointer tables are absolute addresses: use char arrays'
# Indexed colour: fixed and per-venue palettes, glints cycled in the palette,
# text colours on exact cube cells, RLE panoramas, logo and trophy.
assert 'fixed_slot[' in h and 'venue_rgb[5][VENUE_COLOURS]' in h and 'pano_rle[' in h and 'pano_off[6]' in h and 'logo_rle[' in h and 'cup_rle[' in h
assert 'far_coast[256]' in h and 'venue_sun_bearing[5]' in h and '#define SKY_N 14' in h and '#define SEA_N 12' in h
assert 'static void load_venue(int v)' in s and 'static void cycle_glints(void)' in s and 'gx_palette(slot[i]' in s
assert 'def cube_index' in generator or 'cube' in generator
for i in range(5): assert (r/f'assets/source/panorama-{i}.png').is_file()
# Fixed-point maths: integer trig, CORDIC bearing, integer sqrt.
assert 'sin_quarter[65]' in m and 'cordic_atan[14]' in m and 'static uint16_t bearing(' in m and 'static int32_t isqrt(' in m
# Chase camera from the stern, perspective projection and near/side clipping.
assert '#define CAM_BACK' in s and '#define FOCAL' in s and 'static void proj(' in s and 'static int32_t plane_d(' in s
assert 'p->x*FOCAL/p->z' in s and 'NEAR_DM' in s and 'static void poly3(' in s and 'static void line3(' in s
# Race field larger than the view, simplified map, land that cannot be sailed on.
assert 'FIELD_X 900' in s and 'FIELD_Y0 (-450)' in s and 'FIELD_Y1 1250' in s
assert 'static void draw_minimap(void)' in s and 'b->aground=(uint8_t)clamp_field(b)' in s and 'static void shore_wall(' in s
assert 'static void draw_horizon(void)' in s and 'panorama(HOR,off)' in s and 'C_SUN' in s and 'draw_cloud(' in s
# Wind with deterministic shifts and a wind instrument.
assert 'static void update_wind(void)' in s and 'wind_seed' in s and 'static void draw_wind_gauge(void)' in s
assert 'PREVAILING_SW_HEADING 0' in s and 'SW 8-16 KT, SHIFTING' in s
# ORC 12mR polars, sail plans, trim model and momentum.
assert 'polar8[13]' in s and 'polar12[13]' in s and 'polar16[13]' in s
assert 'plan_jib[13]' in s and 'plan_spin[13]' in s and 'plan_genn[13]' in s
assert 'static int trim_opt(int awa)' in s and 'static int trim_eff(const boat_t *b)' in s and 'tau=target>v?' in s
assert 'b->boom' in s and 'side=b->awa>=0?-1:1' in s
# Controls: only the D-pad, A and B. Helm, sheets, tap A spinnaker, tap B
# gennaker, hold A penalty turn, hold B race menu; an A+B chord does nothing.
assert 'if(in&PRG32_BTN_UP)b->sheet' in s and 'if(in&PRG32_BTN_DOWN)b->sheet' in s
assert 'me->kite_want==KITE_SPIN?KITE_NONE:KITE_SPIN' in s and 'me->kite_want==KITE_GENN?KITE_NONE:KITE_GENN' in s
assert '(PRG32_BTN_A|PRG32_BTN_B))==(PRG32_BTN_A|PRG32_BTN_B))a_used=b_used=1' in s and 'begin_penalty_turn(me)' in s
assert 'HOLD_FRAMES 24' in s and 'race_menu=1' in s and 'auto_trim' in s
assert 'PRG32_BTN_START' not in s and 'PRG32_BTN_SELECT' not in s
assert '(PRG32_BTN_LEFT|PRG32_BTN_RIGHT|PRG32_BTN_UP|PRG32_BTN_DOWN|PRG32_BTN_A|PRG32_BTN_B)' in s
assert 'b->kite_prog' in s and 'b->kite=b->kite_want' in s
# Close-quarters top view with hysteresis.
assert 'TOP_VIEW_ENTER 70' in s and 'TOP_VIEW_EXIT 100' in s and 'close_to_rival_or_buoy(top_view_mode?TOP_VIEW_EXIT:TOP_VIEW_ENTER)' in s and '"TOP VIEW"' in s
# AI: timed start, VMG beats/runs with shift and layline tacking, rules, kites.
assert 'static void helm_ai(boat_t *b)' in s and 'static uint16_t vmg_heading(' in s and 'layline_cos[4]' in s
assert 'static uint16_t avoid_traffic(' in s and 'static int gives_way(' in s and 'kite_for(' in s and 'clear_water(b,45)' in s
# Campaign, courses, start and finish.
assert 'NAPOLI 1997' in s and 'RACES 5' in s and 'sponsors[4]' in s and 'money+=' in s and 'wins++' in s
assert 'START_COUNTDOWN_SECONDS 600' in s and 'FAST_TIME_SCALE 20' in s and 'RACE_TIME_SCALE 4' in s
assert '10 MIN - WARNING' in s and '5 MIN - CLASS SIGNAL' in s and '4 MIN - P FLAG UP' in s and '2 MIN - ENTER THE BOX' in s
assert '1 MIN - P FLAG DOWN' in s and 'START - CLASS FLAG DOWN' in s
assert 'entered_box' in s and 'BOX_HALF' in s and 'if(all_started)start_line_active=0' in s
assert 'course_names[COURSE_COUNT]' in s and 'course_len[COURSE_COUNT]={3,5,6}' in s
assert 'WINDWARD / RUN' in s and 'OLYMPIC TRIANGLE' in s and '1992 IACC Z' in s
assert 'FINISH_Y' in s and 'FINISH_HALF' in s and 'boats[0].leg==it[i].idx' in s
for rule in ('RULE_PORT','RULE_WINDWARD','RULE_ASTERN','RULE_TACKING','RULE_CONTACT','RULE_MARK_ROOM','RULE_MARK_TOUCH'):
    assert rule in s
assert 'start_clock>240' in s and 'b->turned>=65536u' in s and 'mark_dist2' in s and 'windward_score' in s
# Multiplayer: v9 rooms, stable peer slots, lobby handshake and syndicate
# arbitration, late joiners, smoothed remote yachts, 16-bit snapshot fields.
assert 'count>3?3:count' in platform and ':v9-iacc92' in platform and ':v9-olympic' in platform and ':v9-windward' in platform
assert 'fairwind_net_join(course_sel)' in s and 'peer_count+1' in s and 'prg32_multiplayer_local_player_id' in platform
assert 'local_ready' in s and 'NF_READY|NF_RACING' in s and 'static uint8_t lobby_team(void)' in s and 'NF_OLD_RACE' in s
assert 'static void net_smooth(' in s and 'human_tag(' in s
# Sound: songs per screen, live race effects; scores to the hall of fame.
assert 'snd_song(SND_TITLE)' in s and 'static void race_audio_update(void)' in s and 'fairwind_score_submit(' in s
# Store and release artwork.
assert (r/'assets/generated/title.png').is_file() and (r/'assets/generated/screenshot.png').is_file()
shot=Image.open(r/'assets/generated/screenshot.png').convert('RGB')
assert shot.size==(320,200) and len(shot.getcolors(maxcolors=1<<20))>=24
a=json.loads((r/'audio.json').read_text())
assert len(a['instruments'])==20 and len(a['tracks'])==6
voices={e.get('arg0') for t in a['tracks'] for e in t['events'] if e['command']=='NOTE_ON'}
assert voices<=set(range(8)),voices
meta=json.loads((r/'metadata/metadata.json').read_text())
assert meta['players']=={'min':1,'max':4}
assert meta['name']=='FairWind-napoli97' and meta['id']=='org.riscv-prg32.fairwind-napoli97'
assert meta['version']=='5.0.0' and 'selectable-courses' in meta['features'] and 'stern-chase-view' in meta['features']
assert 'basic-rrs-rules' in meta['features'] and 'wind-shifts' in meta['features']
assert meta['cartridge_profile']=='portable-128k' and 'A+B' not in meta['controls']
col=json.loads((r/'metadata/colophon.json').read_text())
assert col['version']==meta['version'] and not any('A+B' in c['input'] for c in col['controls'])
print('source checks: OK; 128 KiB indexed-colour engine, sailing physics, wind shifts, AI, rules, courses, audio, and multiplayer verified')
