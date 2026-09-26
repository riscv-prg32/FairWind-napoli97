#!/usr/bin/env python3
"""Generate the PRG32 cup sprite, Bay of Naples panoramas and store artwork.

Yachts, sails and buoys are drawn procedurally by the race engine, so no
pre-rotated yacht sprites are generated any more."""
from pathlib import Path
from math import cos, sin, pi
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'src'/'assets_bitplanes.h'
GEN=ROOT/'assets'/'generated'
PALETTE=[(0,0,0),(255,247,218),(2,38,99),(4,96,174),(54,174,217),(248,196,55),(207,39,45),(43,116,70),(64,82,110),(131,171,210),(232,125,50),(70,44,31),(218,190,139),(13,25,42),(111,181,235),(177,77,42)]

def rgb565(c): return ((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3)
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

def cube_level(v): return min(5,(v*5+127)//255)
def cube_rgb(l): return tuple(x*51 for x in l)
def logo(size=96,colors=16):
    """The FairWind logo quantised onto the display's 6x6x6 colour cube.

    Keeping the most used cube colours means the ILI9341 palette lookup and the
    QEMU RGB565 display show exactly the same pixels."""
    from collections import Counter
    src=Image.open(ROOT/'assets'/'source'/'fairwind-logo.png').convert('RGB').resize((size,size),Image.Resampling.LANCZOS)
    levels=[tuple(cube_level(c) for c in p) for p in src.get_flattened_data()]
    keep=[c for c,_ in Counter(levels).most_common(colors)]
    near=lambda l:min(range(len(keep)),key=lambda i:sum((a-b)**2 for a,b in zip(l,keep[i])))
    im=Image.new('P',(size,size));im.putpalette([v for l in keep for v in cube_rgb(l)]);im.putdata([near(l) for l in levels])
    return im,keep
def cube565(l):
    # Round up so the firmware's rgb565 -> cube conversion returns level l.
    return (((l[0]*31+4)//5)<<11)|(((l[1]*63+4)//5)<<5)|((l[2]*31+4)//5)
c=cup(); logo_image,logo_levels=logo(); backgrounds=[authored_panorama(i) for i in range(5)]; GEN.mkdir(parents=True,exist_ok=True)
runtime_backgrounds=[background.resize((320,24),Image.Resampling.NEAREST) for background in backgrounds]
def rle8(frames):
    data=[];offsets=[0]
    for image in frames:
        pixels=list(image.get_flattened_data())
        for y in range(image.height):
            row=pixels[y*image.width:(y+1)*image.width];value=row[0];count=1
            for next_value in row[1:]:
                if next_value==value and count<255: count+=1
                else: data.extend((count,value));value=next_value;count=1
            data.extend((count,value))
        offsets.append(len(data))
    return data,offsets
background_rle,background_offsets=rle8(runtime_backgrounds)
c.convert('RGB').resize((128,160),Image.Resampling.NEAREST).save(GEN/'cup.png')
logo_image.convert('RGB').resize((192,192),Image.Resampling.NEAREST).save(GEN/'logo_sprite.png')
bay=Image.new('RGB',(320,48*5));
for i,bg in enumerate(backgrounds): bay.paste(bg.convert('RGB'),(0,i*48))
bay.save(GEN/'bay_of_naples_backgrounds.png')
pal=', '.join(f'0x{rgb565(x):04x}' for x in PALETTE)
text='#ifndef FAIRWIND_ASSETS_BITPLANES_H\n#define FAIRWIND_ASSETS_BITPLANES_H\n#include <stdint.h>\n'
text+=f'static const uint16_t fairwind_palette[16]={{{pal}}};\n'
text+=f'static const uint16_t fairwind_background_palette[16]={{{pal}}};\n'
text+='static const uint16_t fairwind_logo_palette[16]={'+', '.join(f'0x{cube565(l):04x}' for l in logo_levels)+'};\n'
text+=arr('fairwind_logo_planes',planar([logo_image],96,96))+arr('fairwind_cup_planes',planar([c],32,40))+arr('fairwind_background_rle8',background_rle)
text+='static const uint16_t fairwind_background_offsets[6]={'+','.join(str(x) for x in background_offsets)+'};\n#endif\n'
OUT.write_text(text)
print(f'wrote {OUT} ({OUT.stat().st_size} bytes)')
