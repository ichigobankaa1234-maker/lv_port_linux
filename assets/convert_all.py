import os
from PIL import Image
import argparse

# ------------------------------------------------------------
# CONFIG (editable)
# ------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(description="Convert PNGs to LVGL RGB565 C assets")
    parser.add_argument(
        "--input",
        required=True,
        help="C:\HondDash\assets_cropped"
    )
    parser.add_argument(
        "--output",
        required=True,
        help="C:\HondDash\assets_cropped\assets"
    )
    return parser.parse_args()


# ------------------------------------------------------------
# RGB565 conversion (CORRECT for FBDEV)
# ------------------------------------------------------------

def rgba_to_rgb565_bytes(img: Image.Image) -> bytes:
    """
    Convert RGBA image to RGB565 byte stream (little endian).
    Alpha is ignored (FBDEV has no alpha).
    """
    img = img.convert("RGBA")
    pixels = img.load()
    w, h = img.size

    out = bytearray()

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]

            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F

            rgb565 = (r5 << 11) | (g6 << 5) | b5

            # little-endian: LOW byte first
            out.append(rgb565 & 0xFF)
            out.append((rgb565 >> 8) & 0xFF)

    return bytes(out)


# ------------------------------------------------------------
# C file writer
# ------------------------------------------------------------

def write_c_file(c_path, symbol, data, width, height):
    with open(c_path, "w", encoding="utf-8") as f:
        f.write('#include "lvgl.h"\n\n')

        f.write("LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("LV_ATTRIBUTE_LARGE_CONST\n")
        f.write(f"static const uint8_t {symbol}_map[] = {{\n")

        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            line = ", ".join(f"0x{b:02X}" for b in chunk)
            f.write(f"    {line},\n")

        f.write("};\n\n")

        f.write(f"const lv_image_dsc_t img_{symbol} = {{\n")
        f.write("    .header.magic = LV_IMAGE_HEADER_MAGIC,\n")
        f.write("    .header.cf = LV_COLOR_FORMAT_RGB565,\n")
        f.write(f"    .header.w = {width},\n")
        f.write(f"    .header.h = {height},\n")
        f.write(f"    .data_size = sizeof({symbol}_map),\n")
        f.write(f"    .data = {symbol}_map,\n")
        f.write("};\n")


# ------------------------------------------------------------
# Main conversion walk
# ------------------------------------------------------------

def sanitize(name: str) -> str:
    return (
        name.replace("-", "_")
            .replace(" ", "_")
            .replace(".", "_")
    )


def main():
    args = parse_args()

    in_root = os.path.abspath(args.input)
    out_root = os.path.abspath(args.output)

    if not os.path.isdir(in_root):
        raise RuntimeError(f"Input directory not found: {in_root}")

    for root, _, files in os.walk(in_root):
        rel = os.path.relpath(root, in_root)
        out_dir = os.path.join(out_root, rel)
        os.makedirs(out_dir, exist_ok=True)

        for file in files:
            if not file.lower().endswith(".png"):
                continue

            in_png = os.path.join(root, file)
            base = os.path.splitext(file)[0]
            symbol = sanitize(base)
            out_c = os.path.join(out_dir, f"{base}.c")

            img = Image.open(in_png)
            w, h = img.size
            rgb565_data = rgba_to_rgb565_bytes(img)

            write_c_file(out_c, symbol, rgb565_data, w, h)

            print(f"[OK] {in_png} -> {out_c}")


if __name__ == "__main__":
    main()
