#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
r=Path(__file__).resolve().parents[1]; o=r/'assets/generated';o.mkdir(parents=True,exist_ok=True)
im=Image.new('RGB',(320,200),(3,65,145));d=ImageDraw.Draw(im);font=ImageFont.load_default()
d.rectangle((0,0,319,18),fill=(4,10,42));d.text((6,5),'VESUVIUS CUP',font=font,fill='white');d.text((238,5),'NAPOLI 1997',font=font,fill=(255,205,20))
bay=Image.open(o/'bay_of_naples_backgrounds.png').convert('RGB')
im.paste(bay.crop((0,48,320,96)),(0,18));d=ImageDraw.Draw(im)
for i in range(18):
 x=(i*53+19)%320;y=62+(i*37)%116;d.line((x,y,min(319,x+10+(i&7)),y),fill=(54,174,217) if i&1 else (111,181,235))
def mark(x,y):d.ellipse((x-5,y-7,x+5,y+7),fill=(250,150,20));d.rectangle((x-2,y-4,x+2,y+4),fill=(255,205,20))
for p in [(160,53),(288,112),(160,159)]:mark(*p)
d.line((126,98,190,98),fill='white',width=2)
# Playable pre-start: committee boat, pin buoy, orange/class/P signals.
d.line((94,146,226,146),fill='white',width=1)
d.rectangle((74,148,101,151),fill='white');d.rectangle((78,152,97,154),fill=(4,10,42))
d.rectangle((87,116,88,148),fill='white');d.rectangle((89,118,99,124),fill=(250,150,20))
d.rectangle((89,128,104,136),fill='white');d.text((92,129),'12',font=font,fill=(4,10,42))
d.rectangle((89,139,104,147),fill=(4,10,42));d.rectangle((93,141,100,145),fill='white')
mark(226,146)
d.rectangle((70,146,70,178),fill=(120,120,130));d.rectangle((250,146,250,178),fill=(120,120,130));d.line((70,178,250,178),fill=(120,120,130))
d.rectangle((70,144,94,146),fill=(230,25,25));d.rectangle((226,144,250,146),fill=(30,220,80))
d.text((31,158),'PORT',font=font,fill=(255,80,80));d.text((258,158),'STBD',font=font,fill=(30,220,80))
d.rectangle((5,21,225,34),fill=(4,10,42));d.text((9,24),'2 MIN - ENTER THE BOX',font=font,fill='white')
boat_sheet=Image.open(o/'boat_bitplane_sheet.png').convert('RGB')
def boat(x,y,frame,col):
 tile=boat_sheet.crop(((frame%8)*64,(frame//8)*64,(frame%8+1)*64,(frame//8+1)*64)).resize((32,32),Image.Resampling.NEAREST)
 im.paste(tile,(x-16,y-16));d.line((x-5,y+14,x+5,y+14),fill=col,width=2)
boat(82,126,8,(0,210,230));boat(238,130,8,(230,25,25));boat(106,153,0,(255,205,20));boat(215,156,0,'white')
d.rectangle((0,181,319,199),fill=(4,10,42));d.text((5,187),'T- 02:00   KT 07   JIB   ENTER PORT',font=font,fill='white')
im.save(o/'start_sequence.png')

# In-race proof: 1992 IACC Z path, highlighted target, and buoy-to-committee finish.
im=Image.new('RGB',(320,200),(3,65,145));d=ImageDraw.Draw(im)
d.rectangle((0,0,319,18),fill=(4,10,42));d.text((6,5),'1992 IACC Z',font=font,fill='white');d.text((238,5),'NAPOLI 1997',font=font,fill=(255,205,20))
im.paste(bay.crop((0,48,320,96)),(0,18));d=ImageDraw.Draw(im)
for i in range(18):
 x=(i*53+19)%320;y=62+(i*37)%116;d.line((x,y,min(319,x+10+(i&7)),y),fill=(54,174,217) if i&1 else (111,181,235))
for p in [(160,45),(280,88),(160,132),(40,88)]:mark(*p)
d.rectangle((268,76,292,78),fill=(255,205,20));d.rectangle((268,98,292,100),fill=(255,205,20));d.rectangle((268,76,270,100),fill=(255,205,20));d.rectangle((290,76,292,100),fill=(255,205,20));d.text((265,64),'NEXT',font=font,fill=(255,205,20))
# Blue-flag committee boat and finishing buoy.
d.rectangle((74,158,101,161),fill='white');d.rectangle((78,162,97,165),fill=(4,10,42));d.rectangle((87,133,88,158),fill='white');d.rectangle((89,135,100,142),fill=(0,55,190))
d.line((101,158,218,158),fill='white',width=2);mark(226,158)
boat(207,110,1,(0,210,230));boat(172,124,0,(230,25,25));boat(132,93,15,(255,205,20));boat(93,119,14,'white')
d.rectangle((0,181,319,199),fill=(4,10,42));d.text((5,187),'R 08:42   KT 09   SW   JIB   NEXT 2   NET 4/4',font=font,fill='white')
im.save(o/'screenshot.png');im.save(o/'course_paths.png')
icon=Image.new('RGB',(256,256),(3,65,145));q=ImageDraw.Draw(icon);q.ellipse((22,22,234,234),outline=(255,205,20),width=10);q.polygon([(128,38),(91,191),(128,219),(165,191)],fill='white');q.polygon([(128,55),(128,175),(203,167)],fill=(220,225,230));label='NaCup';bb=q.textbbox((0,0),label,font=font);q.text(((256-(bb[2]-bb[0]))//2,222),label,font=font,fill=(255,205,20));icon.save(o/'icon.png')

title=Image.new('RGB',(320,200),(3,65,145));t=ImageDraw.Draw(title)
t.rectangle((0,0,319,18),fill=(4,10,42));t.text((6,5),'NaCup',font=font,fill='white');t.text((238,5),'NAPOLI 1997',font=font,fill=(255,205,20))
title.paste(bay.crop((0,0,320,48)),(0,18))
cup=Image.open(o/'cup.png').convert('RGB').resize((32,40),Image.Resampling.NEAREST);cup_mask=cup.convert('L').point(lambda p:255 if p else 0);title.paste(cup,(144,38),cup_mask)
t=ImageDraw.Draw(title);t.text((49,91),"12-METRE AMERICA'S CUP",font=font,fill='white');t.text((91,108),'NAPLES 1997',font=font,fill=(255,205,20));t.text((76,172),'PRESS A TO SET SAIL',font=font,fill='white')
title.save(o/'title.png')
print('rendered title.png, icon.png, start_sequence.png, course_paths.png, and screenshot.png')
