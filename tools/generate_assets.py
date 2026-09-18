#!/usr/bin/env python3
"""Generate deterministic 4-bitplane PRG32 sprites and store artwork."""
from pathlib import Path
from math import cos, sin, pi
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'src'/'assets_bitplanes.h'
GEN=ROOT/'assets'/'generated'
PALETTE=[(0,0,0),(255,247,218),(2,38,99),(4,96,174),(54,174,217),(248,196,55),(207,39,45),(43,116,70),(64,82,110),(131,171,210),(232,125,50),(70,44,31),(218,190,139),(13,25,42),(111,181,235),(177,77,42)]
BOAT_BASE=[(0,0,0),(255,255,255),(250,247,224),(224,231,232),(174,190,196),(36,47,61),(242,245,241),(121,143,153),(116,67,35),(239,185,38),(78,174,209),(28,37,48),(177,235,247),(207,39,45),(0,196,220),(245,205,42),(98,18,24),(0,91,119),(117,130,135),(208,216,211),(255,241,184),(68,87,95),(149,85,44),(236,104,48),(202,247,255),(73,125,155),(151,205,222),(231,230,208),(92,101,100),(255,156,72),(45,59,76),(13,25,42)]
BOAT_PALETTE=BOAT_BASE+[(r,g,b) for r in (0,51,102,153,204,255) for g in (0,51,102,153,204,255) for b in (0,51,102,153,204,255)]+[(v,v,v) for v in (24,56,88,120,152,184,216,248)]
SPIN_COLORS=[((0,210,230),(0,91,119)),((240,38,48),(125,16,25)),((181,85,232),(82,35,124)),((255,205,20),(190,118,0))]
SAIL_PALETTE=[(0,0,0),(250,247,224),(185,205,210),(255,255,255)]

def rgb565(c): return ((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3)
def poly(img,points,c): ImageDraw.Draw(img).polygon(points,fill=c)
def boat(angle):
    # Draw at 4x resolution, rotate there, then quantize to a dedicated
    # 256-entry palette. This keeps rigging and sail edges legible at 32 px.
    hi=Image.new('RGBA',(128,128),(0,0,0,0));d=ImageDraw.Draw(hi)
    # Foaming wake, hull shadow, hull and deck.
    d.polygon([(50,91),(39,123),(58,100)],fill=(177,235,247,150));d.polygon([(78,91),(89,123),(70,100)],fill=(177,235,247,150))
    d.polygon([(64,6),(42,91),(64,121),(86,91)],fill=(36,47,61,255))
    d.polygon([(64,9),(47,89),(64,115),(81,89)],fill=(242,245,241,255))
    d.polygon([(64,16),(52,86),(64,105),(76,86)],fill=(174,190,196,255))
    d.polygon([(64,22),(57,84),(64,96),(71,84)],fill=(116,67,35,255))
    d.ellipse((57,79,71,94),fill=(28,37,48,255));d.ellipse((60,82,68,89),fill=(78,174,209,255))
    # Main and jib, with separate shaded panels and seams.
    d.polygon([(67,17),(67,83),(112,73)],fill=(250,247,224,255));d.polygon([(70,25),(70,76),(101,70)],fill=(224,231,232,255))
    d.polygon([(61,24),(61,75),(25,67)],fill=(255,255,255,255));d.polygon([(58,34),(58,68),(34,64)],fill=(202,247,255,255))
    d.line((64,12,64,94),fill=(28,37,48,255),width=3);d.line((64,18,111,73),fill=(121,143,153,255),width=1);d.line((64,24,25,67),fill=(121,143,153,255),width=1)
    d.line((69,37,96,69),fill=(174,190,196,255),width=1);d.line((58,42,37,65),fill=(174,190,196,255),width=1)
    d.line((64,82,98,75),fill=(116,67,35,255),width=3);d.ellipse((60,9,68,17),fill=(239,185,38,255))
    turned=hi.rotate(-angle*360/32,resample=Image.Resampling.BICUBIC,center=(64,64))
    small=turned.resize((32,32),Image.Resampling.LANCZOS)
    palette_image=Image.new('P',(1,1));palette_image.putpalette(sum((list(c) for c in BOAT_PALETTE),[]))
    rgb=Image.new('RGB',(32,32),(0,0,0));rgb.paste(small.convert('RGB'),mask=small.getchannel('A'))
    out=rgb.quantize(palette=palette_image,dither=Image.Dither.NONE)
    alpha=list(small.getchannel('A').get_flattened_data());pixels=list(out.get_flattened_data())
    out.putdata([0 if a<72 else p for p,a in zip(pixels,alpha)])
    return out
def component(heading,kind,side=0):
    hi=Image.new('RGBA',(128,128),(0,0,0,0));d=ImageDraw.Draw(hi)
    if kind=='hull':
        # Layered wake, deep hull shadow, fine waterline and tapered deck.
        d.polygon([(50,88),(38,125),(58,102)],fill=(177,235,247,145));d.polygon([(78,88),(90,125),(70,102)],fill=(177,235,247,145));d.line((43,112,57,98),fill=(202,247,255,180),width=3);d.line((85,112,71,98),fill=(202,247,255,180),width=3)
        # Il Moro di Venezia V-inspired IACC form: long narrow red hull,
        # fine bow, broad working stern and a pale inset deck.
        d.polygon([(64,3),(43,88),(47,104),(56,122),(72,122),(81,104),(85,88)],fill=(98,18,24,255));d.polygon([(64,6),(47,87),(51,101),(59,116),(69,116),(77,101),(81,87)],fill=(207,39,45,255))
        d.polygon([(64,13),(52,84),(56,98),(64,109),(72,98),(76,84)],fill=(242,245,241,255));d.line((48,88,57,103,64,114,71,103,80,88),fill=(255,241,184,255),width=2);d.polygon([(64,27),(59,78),(64,90),(69,78)],fill=(231,230,208,255))
        # Cockpit, crew, winches, mast step and bow fitting survive at 32 px.
        d.ellipse((54,76,74,99),fill=(28,37,48,255));d.ellipse((58,80,70,93),fill=(78,174,209,255));d.ellipse((55,83,60,89),fill=(250,247,224,255));d.ellipse((68,83,73,89),fill=(250,247,224,255));d.ellipse((55,91,60,97),fill=(207,39,45,255));d.ellipse((68,91,73,97),fill=(207,39,45,255))
        d.ellipse((52,69,58,75),fill=(121,143,153,255));d.ellipse((70,69,76,75),fill=(121,143,153,255));d.line((64,9,64,98),fill=(13,25,42,255),width=4);d.line((50,87,78,87),fill=(68,87,95,255),width=2)
        d.line((55,34,50,82),fill=(121,143,153,255),width=2);d.line((73,34,78,82),fill=(121,143,153,255),width=2);d.ellipse((59,6,69,16),fill=(239,185,38,255));d.ellipse((61,8,67,14),fill=(255,241,184,255))
        palette=PALETTE;planes=4
    elif kind=='main':
        if side==0:
            d.polygon([(67,43),(67,105),(111,93),(104,66),(91,50)],fill=(250,247,224,255));d.polygon([(72,53),(72,97),(101,90),(97,69),(88,57)],fill=(185,205,210,255));d.polygon([(73,57),(88,58),(96,70),(74,72)],fill=(255,255,255,255));d.line((69,70,104,74),fill=(185,205,210,255),width=3);d.line((69,86,108,88),fill=(185,205,210,255),width=3);d.line((68,102,107,93),fill=(185,205,210,255),width=3);d.ellipse((81,76,90,85),fill=(255,255,255,255))
        else:
            d.polygon([(61,43),(61,105),(17,93),(24,66),(37,50)],fill=(250,247,224,255));d.polygon([(56,53),(56,97),(27,90),(31,69),(40,57)],fill=(185,205,210,255));d.polygon([(55,57),(40,58),(32,70),(54,72)],fill=(255,255,255,255));d.line((59,70,24,74),fill=(185,205,210,255),width=3);d.line((59,86,20,88),fill=(185,205,210,255),width=3);d.line((60,102,21,93),fill=(185,205,210,255),width=3);d.ellipse((38,76,47,85),fill=(255,255,255,255))
        palette=SAIL_PALETTE;planes=2
    elif kind=='jib':
        if side==0:
            d.polygon([(67,18),(67,62),(104,37),(91,25)],fill=(250,247,224,255));d.polygon([(72,27),(72,54),(94,38),(87,30)],fill=(255,255,255,255));d.line((69,39,97,36),fill=(185,205,210,255),width=3);d.line((69,53,83,44),fill=(185,205,210,255),width=3)
        else:
            d.polygon([(61,18),(61,62),(24,37),(37,25)],fill=(250,247,224,255));d.polygon([(56,27),(56,54),(34,38),(41,30)],fill=(255,255,255,255));d.line((59,39,31,36),fill=(185,205,210,255),width=3);d.line((59,53,45,44),fill=(185,205,210,255),width=3)
        palette=SAIL_PALETTE;planes=2
    else:
        # Indices 1/2 are recoloured at runtime through the team palette.
        if side==0:
            d.ellipse((57,22,121,96),fill=(250,247,224,255));d.polygon([(64,22),(113,41),(106,86),(64,93)],fill=(185,205,210,255));d.polygon([(65,25),(89,32),(85,91),(65,93)],fill=(255,255,255,255));d.line((65,58,117,59),fill=(250,247,224,255),width=3);d.line((65,25,106,86),fill=(185,205,210,255),width=3);d.ellipse((83,51,96,64),fill=(250,247,224,255))
        else:
            d.ellipse((7,22,71,96),fill=(250,247,224,255));d.polygon([(64,22),(15,41),(22,86),(64,93)],fill=(185,205,210,255));d.polygon([(63,25),(39,32),(43,91),(63,93)],fill=(255,255,255,255));d.line((63,58,11,59),fill=(250,247,224,255),width=3);d.line((63,25,22,86),fill=(185,205,210,255),width=3);d.ellipse((32,51,45,64),fill=(250,247,224,255))
        palette=SAIL_PALETTE;planes=2
    turned=hi.rotate(-heading*360/16,resample=Image.Resampling.BICUBIC,center=(64,64));small=turned.resize((32,32),Image.Resampling.LANCZOS)
    pal=Image.new('P',(1,1));flat=sum((list(c) for c in palette),[]);pal.putpalette(flat+[0]*(768-len(flat)))
    rgb=Image.new('RGB',(32,32),(0,0,0));rgb.paste(small.convert('RGB'),mask=small.getchannel('A'));out=rgb.quantize(palette=pal,dither=Image.Dither.NONE)
    alpha=list(small.getchannel('A').get_flattened_data());pixels=list(out.get_flattened_data());out.putdata([0 if a<72 else p for p,a in zip(pixels,alpha)])
    return out,planes
def mark():
    im=Image.new('P',(16,16),0);im.putpalette(sum((list(c) for c in PALETTE),[])+[0]*(768-48));d=ImageDraw.Draw(im)
    d.ellipse((3,2,12,14),fill=10);d.rectangle((5,4,10,11),fill=5);d.line((3,14,12,14),fill=1);return im
def cup():
    im=Image.new('P',(32,40),0);im.putpalette(sum((list(c) for c in PALETTE),[])+[0]*(768-48));d=ImageDraw.Draw(im)
    d.polygon([(8,5),(24,5),(21,22),(17,27),(17,33),(25,35),(25,38),(7,38),(7,35),(15,33),(15,27),(11,22)],fill=5)
    d.arc((1,7,13,25),80,280,fill=5,width=3);d.arc((19,7,31,25),260,100,fill=5,width=3);d.rectangle((10,7,22,10),fill=10);return im
def panorama(scene):
    """Hand-authored 320x48 Bay of Naples panorama in the shared palette."""
    im=Image.new('P',(320,48),14);im.putpalette(sum((list(c) for c in PALETTE),[])+[0]*(768-48));d=ImageDraw.Draw(im)
    # Atmospheric dither and horizon haze.
    d.rectangle((0,35,319,47),fill=3);d.line((0,34,319,34),fill=9)
    for x in range(scene%3,320,9): d.point((x,8+(x*7+scene*11)%18),fill=1)
    if scene==0: # Santa Lucia and Castel dell'Ovo
        d.polygon([(0,34),(55,29),(108,33),(150,34)],fill=8);d.polygon([(165,34),(211,21),(258,28),(319,32),(319,35)],fill=9)
        d.polygon([(18,21),(31,18),(77,19),(91,23),(91,35),(13,35)],fill=11)
        d.rectangle((25,13,39,34),fill=12);d.rectangle((67,15,83,34),fill=12);d.rectangle((39,18,67,34),fill=9)
        for x in (29,34,72,77): d.rectangle((x,18,x+2,22),fill=13)
        for x in range(44,66,7): d.rectangle((x,24,x+2,28),fill=13)
        d.arc((47,26,59,38),180,360,fill=11,width=2);d.line((7,35,112,35),fill=5)
    elif scene==1: # Vesuvius across the gulf
        d.polygon([(0,35),(55,32),(101,24),(139,10),(158,16),(176,12),(220,27),(270,33),(319,35)],fill=8)
        d.polygon([(58,34),(111,25),(139,12),(158,18),(176,14),(220,29),(270,34)],fill=11)
        d.line((139,11,158,17,176,13),fill=9,width=2);d.arc((154,0,183,16),190,330,fill=12,width=2)
        for x,h in [(7,8),(20,12),(31,7),(45,10),(263,9),(278,13),(295,8),(309,11)]: d.rectangle((x,35-h,x+5,35),fill=12);d.rectangle((x+1,37-h,x+2,38-h),fill=5)
    elif scene==2: # Capri limestone and Faraglioni
        d.polygon([(0,35),(31,31),(65,18),(104,14),(140,22),(177,29),(205,35)],fill=8)
        d.polygon([(0,35),(49,29),(68,20),(104,17),(138,24),(176,31),(205,35)],fill=11)
        d.polygon([(213,35),(219,15),(227,9),(234,18),(238,35)],fill=9);d.polygon([(250,35),(255,8),(263,3),(270,15),(273,35)],fill=12);d.polygon([(285,35),(290,17),(298,11),(306,21),(308,35)],fill=9)
        for x in range(18,170,13): d.rectangle((x,25+(x%9),x+4,28+(x%9)),fill=7)
        d.arc((256,19,267,34),80,280,fill=8,width=2)
    elif scene==3: # Sorrento tufa cliffs and terraces
        d.polygon([(0,35),(41,31),(92,25),(143,21),(197,17),(246,14),(319,10),(319,35)],fill=11)
        d.polygon([(0,35),(78,32),(138,29),(203,25),(264,22),(319,20),(319,35)],fill=8)
        for x,y,w in [(25,23,18),(52,20,21),(83,17,17),(115,14,23),(151,11,18),(185,9,25),(225,7,21),(260,5,28)]:
            d.rectangle((x,y,x+w,y+9),fill=12);d.line((x,y,x+w,y),fill=10);d.rectangle((x+4,y+3,x+6,y+5),fill=13);d.rectangle((x+w-7,y+3,x+w-5,y+5),fill=13)
        for x in range(4,315,17): d.rectangle((x,28-(x//50),x+8,32-(x//50)),fill=7)
    else: # Naples waterfront, Castel Nuovo and dense city
        d.polygon([(0,35),(66,31),(118,30),(170,27),(221,30),(273,27),(319,30),(319,35)],fill=8)
        for x,h in [(2,9),(12,13),(25,8),(38,16),(54,11),(72,14),(91,10),(107,17),(126,12),(145,9),(166,15),(185,11),(207,17),(229,10),(250,14),(275,11),(298,16),(312,9)]:
            d.rectangle((x,35-h,x+8,35),fill=12);d.rectangle((x+2,38-h,x+3,39-h),fill=5)
        d.rectangle((130,17,181,35),fill=11);d.rectangle((125,12,141,35),fill=9);d.rectangle((171,11,187,35),fill=9);d.polygon([(125,12),(133,7),(141,12)],fill=8);d.polygon([(171,11),(179,6),(187,11)],fill=8)
        d.arc((147,24,164,38),180,360,fill=13,width=2);d.line((0,36,319,36),fill=5)
    # Glints tie every skyline to the gulf.
    for x in range(7+scene*3,320,23): d.line((x,40,x+9,40),fill=4)
    return im

def authored_panorama(scene):
    """Load the late-1990s pixel-art Bay panorama and normalize its palette."""
    src=Image.open(ROOT/'assets'/'source'/f'panorama-{scene}.png').convert('RGB')
    pal=Image.new('P',(1,1));pal.putpalette(sum((list(c) for c in PALETTE),[])+[0]*(768-48))
    return src.resize((320,48),Image.Resampling.NEAREST).quantize(palette=pal,dither=Image.Dither.NONE)
def planar(frames,w,h,planes=4):
    data=[]; rowbytes=(w+7)//8
    for im in frames:
      pix=list(im.get_flattened_data())
      for p in range(planes):
       for y in range(h):
        for bx in range(rowbytes):
         v=0
         for bit in range(8):
          x=bx*8+bit
          if x<w and ((pix[y*w+x]>>p)&1): v|=1<<(7-bit)
         data.append(v)
    return data
def arr(name,data):
    lines=[]
    for i in range(0,len(data),16): lines.append('  '+','.join(f'0x{x:02x}' for x in data[i:i+16])+',')
    return f'static const uint8_t {name}[{len(data)}]={{\n'+"\n".join(lines)+'\n};\n'

hulls=[component(a,'hull')[0] for a in range(16)]
mains=[component(a,'main',0)[0] for a in range(16)]
jibs=[component(a,'jib',side)[0] for side in range(2) for a in range(16)]
spins=[component(a,'spin',0)[0] for a in range(16)]
m=mark(); c=cup(); backgrounds=[authored_panorama(i) for i in range(5)]; GEN.mkdir(parents=True,exist_ok=True)
def paste_indexed(dst,b,pos):
    art=b.convert('RGB').resize((64,64),Image.Resampling.NEAREST)
    # Build the transparency mask from palette indices directly. Converting a
    # mode-P point image through its colour palette can turn index 255 black
    # and make otherwise valid sail layers disappear from preview sheets.
    mask=b.point(lambda p:255 if p else 0,mode='L').resize((64,64),Image.Resampling.NEAREST)
    dst.paste(art,pos,mask)
hull_sheet=Image.new('RGB',(8*64,2*64),(3,65,145));composite=Image.new('RGB',(8*64,2*64),(3,65,145))
for i,b in enumerate(hulls):
    pos=((i%8)*64,(i//8)*64);paste_indexed(hull_sheet,b,pos);paste_indexed(composite,b,pos);paste_indexed(composite,mains[i],pos);paste_indexed(composite,jibs[i],pos)
hull_sheet.save(GEN/'hull_sprite_sheet.png');composite.save(GEN/'boat_bitplane_sheet.png')
for name,frames in [('main_sail',mains),('jib',jibs),('spinnaker',spins)]:
    sh=Image.new('RGB',(8*64,4*64),(3,65,145))
    for i,b in enumerate(frames):paste_indexed(sh,b,((i%8)*64,(i//8)*64))
    sh.save(GEN/f'{name}_sprite_sheet.png')
m.convert('RGB').resize((128,128),Image.Resampling.NEAREST).save(GEN/'mark.png');c.convert('RGB').resize((128,160),Image.Resampling.NEAREST).save(GEN/'cup.png')
bay=Image.new('RGB',(320,48*5));
for i,bg in enumerate(backgrounds): bay.paste(bg.convert('RGB'),(0,i*48))
bay.save(GEN/'bay_of_naples_backgrounds.png')
pal=', '.join(f'0x{rgb565(x):04x}' for x in PALETTE)
boat_pal=', '.join(f'0x{rgb565(x):04x}' for x in BOAT_PALETTE)
sail_pal=', '.join(f'0x{rgb565(x):04x}' for x in SAIL_PALETTE)
text='#ifndef NACUP_ASSETS_BITPLANES_H\n#define NACUP_ASSETS_BITPLANES_H\n#include <stdint.h>\n'
text+=f'static const uint16_t nacup_palette[16]={{{pal}}};\n'
text+=f'static const uint16_t nacup_hull_palette[256]={{{boat_pal}}};\n'
text+=f'static const uint16_t nacup_sail_palette[4]={{{sail_pal}}};\n'
for i,(bright,dark) in enumerate(SPIN_COLORS):
    sp=[(0,0,0),bright,dark,(255,255,255)]
    text+=f'static const uint16_t nacup_spin_palette{i}[4]={{'+', '.join(f'0x{rgb565(x):04x}' for x in sp)+'};\n'
text+='static const uint16_t *const nacup_spin_palettes[4]={nacup_spin_palette0,nacup_spin_palette1,nacup_spin_palette2,nacup_spin_palette3};\n'
text+=arr('nacup_hull_planes',planar(hulls,32,32,4))+arr('nacup_main_planes',planar(mains,32,32,2))+arr('nacup_spin_planes',planar(spins,32,32,2))+arr('nacup_mark_planes',planar([m],16,16))+arr('nacup_cup_planes',planar([c],32,40))+'#endif\n'
OUT.write_text(text)
print(f'wrote {OUT} ({OUT.stat().st_size} bytes)')
