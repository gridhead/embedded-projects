from PIL import Image
import struct

img = Image.open("data.gif")
target_w, target_h = 320, 240

frames = []
for i in range(img.n_frames):
    img.seek(i)
    frame = img.convert("RGB")
    frame = frame.resize((target_w, target_h), Image.NEAREST)
    frames.append(frame)

with open("data.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write(f"#define FRAME_W {target_w}\n")
    f.write(f"#define FRAME_H {target_h}\n")
    f.write(f"#define FRAME_COUNT {len(frames)}\n\n")

    for idx, frame in enumerate(frames):
        pixels = []
        for y in range(target_h):
            for x in range(target_w):
                r, g, b = frame.getpixel((x, y))
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                pixels.append(f"0x{rgb565:04X}")

        f.write(f"const uint16_t frame_{idx}[{target_w * target_h}] PROGMEM = {{\n")
        for row in range(target_h):
            start = row * target_w
            line = ", ".join(pixels[start:start + target_w])
            f.write(f"  {line},\n")
        f.write(f"}};\n\n")

    f.write(f"const uint16_t* const frames[FRAME_COUNT] PROGMEM = {{\n")
    for i in range(len(frames)):
        f.write(f"  frame_{i},\n")
    f.write("};\n")

print(f"Generated data.h: {len(frames)} frames, {target_w}x{target_h}, "
      f"{target_w * target_h * 2 * len(frames)} bytes total")