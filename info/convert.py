from PIL import Image

img = Image.open("/home/gridhead/.claude/image-cache/adbb7709-1132-48be-9aeb-41c87d2f21ba/1.png")
img = img.convert("RGBA")
img = img.resize((72, 72), Image.LANCZOS)

pixels = []
for y in range(72):
    for x in range(72):
        r, g, b, a = img.getpixel((x, y))
        if a < 128:
            rgb565 = 0x0000
        else:
            rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        pixels.append(rgb565)

with open("logo.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write("#define LOGO_W 72\n")
    f.write("#define LOGO_H 72\n\n")
    f.write("const uint16_t logo_bitmap[LOGO_W * LOGO_H] PROGMEM = {\n")
    for i in range(0, len(pixels), 12):
        chunk = pixels[i:i+12]
        f.write("    " + ", ".join(f"0x{p:04X}" for p in chunk) + ",\n")
    f.write("};\n")

print(f"Generated logo.h: {len(pixels)} pixels, {len(pixels)*2} bytes")