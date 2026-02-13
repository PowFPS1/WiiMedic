"""Generate icon.png (128x48) for the Homebrew Channel."""
from PIL import Image, ImageDraw, ImageFont
import os

W, H = 128, 48
img = Image.new('RGBA', (W, H), (15, 15, 15, 255))
draw = ImageDraw.Draw(img)

# Draw green border (2px)
draw.rectangle([0, 0, W-1, H-1], outline=(0, 190, 80, 255), width=2)

# Draw medical cross
cx, cy = 24, 24
cs = 8  # half-size of cross arm width
cl = 14  # half-length of cross arm
cross_col = (0, 210, 90, 255)
# Vertical bar
draw.rectangle([cx-cs, cy-cl, cx+cs, cy+cl], fill=cross_col)
# Horizontal bar
draw.rectangle([cx-cl, cy-cs, cx+cl, cy+cs], fill=cross_col)

# Draw "WiiMedic" text
try:
    font = ImageFont.truetype("arial.ttf", 16)
    small = ImageFont.truetype("arial.ttf", 9)
except:
    font = ImageFont.load_default()
    small = font

draw.text((46, 8), "WiiMedic", fill=(255, 255, 255, 255), font=font)
draw.text((46, 28), "System Diagnostics", fill=(140, 200, 140, 255), font=small)

out = os.path.join(os.path.dirname(__file__), "icon.png")
img.save(out)
print(f"Icon saved: {out} ({W}x{H})")
