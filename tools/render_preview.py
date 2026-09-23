#!/usr/bin/env python3
"""Render the store icon and title art. In-race screenshots are real frames
captured by tools/host_capture.py --store."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from pngutil import save_compact
r=Path(__file__).resolve().parents[1]; o=r/'assets/generated';o.mkdir(parents=True,exist_ok=True)
font=ImageFont.load_default()
bay=Image.open(o/'bay_of_naples_backgrounds.png').convert('RGB')
icon=Image.new('RGB',(256,256),(3,65,145));q=ImageDraw.Draw(icon);q.ellipse((22,22,234,234),outline=(255,205,20),width=10);q.polygon([(128,38),(91,191),(128,219),(165,191)],fill='white');q.polygon([(128,55),(128,175),(203,167)],fill=(220,225,230));label='FairWind';bb=q.textbbox((0,0),label,font=font);q.text(((256-(bb[2]-bb[0]))//2,222),label,font=font,fill=(255,205,20));save_compact(icon,o/'icon.png')

title=Image.new('RGB',(320,200),(3,65,145));t=ImageDraw.Draw(title)
t.rectangle((0,0,319,18),fill=(4,10,42));t.text((6,5),'FairWind',font=font,fill='white');t.text((238,5),'NAPOLI 1997',font=font,fill=(255,205,20))
title.paste(bay.crop((0,0,320,48)),(0,18))
cup=Image.open(o/'cup.png').convert('RGB').resize((32,40),Image.Resampling.NEAREST);cup_mask=cup.convert('L').point(lambda p:255 if p else 0);title.paste(cup,(144,38),cup_mask)
t=ImageDraw.Draw(title);t.text((49,91),"12-METRE AMERICA'S CUP",font=font,fill='white');t.text((91,108),'NAPLES 1997',font=font,fill=(255,205,20));t.text((76,172),'PRESS A TO SET SAIL',font=font,fill='white')
title.save(o/'title.png')

print('rendered title.png and icon.png')
