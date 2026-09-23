"""Store images count toward the 64 KiB package ceiling, so they are saved
losslessly as small as possible: an exact palette PNG when they fit one."""
from PIL import Image


def save_compact(img, path):
    img = img.convert('RGB')
    colors = img.getcolors(256)
    if colors:
        palette = [c for _, c in colors]
        index = {c: i for i, c in enumerate(palette)}
        pal = Image.new('P', img.size)
        pal.putpalette([v for c in palette for v in c])
        pal.putdata([index[c] for c in img.get_flattened_data()])
        assert pal.convert('RGB').tobytes() == img.tobytes(), path
        img = pal
    img.save(path, optimize=True)
