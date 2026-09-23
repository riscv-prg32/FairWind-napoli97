from pathlib import Path
from PIL import Image
import re, json
r=Path(__file__).resolve().parents[1]
s=(r/'src/game.c').read_text(); h=(r/'src/assets_bitplanes.h').read_text()
assert 'prg32_sprite_draw_bitplanes' in (r/'src/platform.h').read_text()
assert 'sprite(&jib_yacht_sprite,nacup_jib_yacht_planes,nacup_yacht_palette0,32,32,16,4,0)' in s
assert 'sprite(&spin_yacht_sprite,nacup_spin_yacht_planes,nacup_yacht_palette0,32,32,16,4,0)' in s
assert 'nacup_hull_palette[256]' in h
assert 'nacup_sail_palette[4]' in h
assert 'nacup_background_palette[16]' in h and 'nacup_background_rle8' in h
for component in ('jib_yacht','spin_yacht'):
    assert f'nacup_{component}_planes' in h
for i in range(4): assert f'nacup_spin_palette{i}[4]' in h
assert 'nacup_spin_palettes' not in h and 'yacht_palette(b->team)' in s
generator=(r/'tools/generate_assets.py').read_text()
assert 'Il Moro di Venezia V-inspired IACC form' in generator
assert '(98,18,24)' in generator and '(207,39,45)' in generator
assert 'nacup_background_offsets[6]' in h and 'nacup_background_rle8[i++]' in s
assert 'NAPOLI 1997' in s and 'RACES 5' in s
assert 'prg32_multiplayer_join' in (r/'src/platform.h').read_text()
assert '--portable --multiplayer' in (r/'build.sh').read_text()
build_script=(r/'build.sh').read_text()
assert '65536' in build_script and '131072' not in build_script
adapter=(r/'tools/prg32_cli_64.py').read_text()
assert 'RAM_SIZE = 64 * 1024' in adapter and adapter.count('FALLBACK_CART_RAM_SIZE = RAM_SIZE')==3
assert 'sponsors[4]' in s and 'money+=' in s and 'wins++' in s
assert 'count>3?3:count' in (r/'src/platform.h').read_text()
assert 'for(i=0;i<peer_count;i++)' in s and 'peer_count+1' in s
assert 'local_ready' in s and '0x40' in s
assert 'polar8[11]' in s and 'polar12[11]' in s and 'polar16[11]' in s
assert 'PRG32_BTN_A)boats[0].spinnaker' in s and '0x20' in s
assert 'START_COUNTDOWN_SECONDS 600' in s and 'SIM_FRAMES_PER_SECOND 3' in s
assert '10 MIN - WARNING' in s and '5 MIN - CLASS SIGNAL' in s
assert '4 MIN - P FLAG UP' in s and '2 MIN - ENTER THE BOX' in s
assert '1 MIN - P FLAG DOWN' in s and 'START - CLASS FLAG DOWN' in s
assert 'entered_box' in s and 'START_BOX_LEFT' in s and 'if(all_started)start_line_active=0' in s
assert 'boats[0].started?0x40:0' in s and 'b->started=(p.flags&0x40)!=0' in s
assert 'course_names[COURSE_COUNT]' in s and 'course_len[COURSE_COUNT]={3,5,6}' in s
assert 'WINDWARD / RUN' in s and 'OLYMPIC TRIANGLE' in s and '1992 IACC Z' in s
assert 'FINISH_BOAT_X' in s and 'FINISH_BUOY_X' in s and 'draw_finish_line' in s
assert 'target_box' in s and 'boats[0].leg==i' in s
assert 'nacup_net_join(course_sel)' in s and ':v7-iacc92' in (r/'src/platform.h').read_text()
for rule in ('RULE_PORT','RULE_WINDWARD','RULE_ASTERN','RULE_TACKING','RULE_CONTACT','RULE_MARK_ROOM','RULE_MARK_TOUCH'):
    assert rule in s
assert 'start_clock>240' in s and 'penalty_turn>=32' in s and 'serve_penalty' in s
assert 'mark_dist2' in s and 'windward_score' in s and 'tack_timer=24' in s
assert 'PREVAILING_SW_HEADING 0' in s and 'SW BREEZE 8-16 KT' in s
assert 'TOP_VIEW_ENTER 65' in s and 'top_view_mode=(uint8_t)close_to_rival_or_buoy(TOP_VIEW_ENTER)' in s
assert 'close_to_rival_or_buoy' in s and '"TOP VIEW"' in s
assert "backgrounds=[authored_panorama(i) for i in range(5)]" in generator
for i in range(5): assert (r/f'assets/source/panorama-{i}.png').is_file()
sheet=Image.open(r/'assets/generated/boat_bitplane_sheet.png').convert('RGB')
assert len(sheet.getcolors(maxcolors=1<<20))>=12
assert (r/'assets/generated/title.png').is_file()
for preview in ('main_sail_sprite_sheet.png','jib_sprite_sheet.png'):
    assert len(Image.open(r/'assets/generated'/preview).convert('RGB').getcolors(maxcolors=1<<20))>1
assert len(re.findall(r'0x[0-9a-f]{2}',h)) > 9000
a=json.loads((r/'audio.json').read_text())
assert len(a['instruments'])==8
assert len(a['tracks'])==5
voices={e.get('arg0') for t in a['tracks'] for e in t['events'] if e['command']=='NOTE_ON'}
assert voices==set(range(8)),voices
meta=json.loads((r/'metadata/metadata.json').read_text())
assert meta['players']=={'min':1,'max':4}
assert meta['name']=='NaCup-napoli97' and meta['id']=='org.riscv-prg32.nacup-napoli97'
assert meta['version']=='3.1.0' and 'selectable-courses' in meta['features']
assert 'basic-rrs-rules' in meta['features']
assert meta['cartridge_profile']=='portable-64k'
print('source checks: OK; basic RRS engine, refined yachts, courses, start/finish, polars, audio, and multiplayer verified')
