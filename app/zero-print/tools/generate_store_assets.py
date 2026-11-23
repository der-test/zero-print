from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "assets" / "images"
SCREENSHOT_DIR = ROOT / "screenshot"

FONT_PATHS = (
    "/System/Library/Fonts/Supplemental/Avenir Next.ttc",
    "/System/Library/Fonts/Supplemental/Helvetica Neue.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
)


def load_font(size, bold=False):
    for path in FONT_PATHS:
        if Path(path).exists():
            try:
                return ImageFont.truetype(path, size=size, index=1 if bold else 0)
            except OSError:
                continue
    return ImageFont.load_default(size=size)


def make_icon():
    image = Image.new("RGBA", (256, 256), "#112622")
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((24, 24, 232, 232), radius=48, fill="#145d4f")
    draw.rounded_rectangle((53, 65, 204, 169), radius=20, fill="#f4f5ef")
    draw.rounded_rectangle((81, 49, 126, 73), radius=8, fill="#f4f5ef")
    draw.ellipse((101, 83, 158, 140), fill="#343b42")
    draw.ellipse((112, 94, 147, 129), fill="#f0a629")
    draw.rectangle((83, 158, 177, 220), fill="#f4f5ef")
    draw.line((97, 174, 163, 174), fill="#343b42", width=5)
    draw.line((97, 187, 156, 187), fill="#343b42", width=5)
    draw.line((97, 200, 145, 200), fill="#343b42", width=5)
    image.save(IMAGE_DIR / "zero-print.png")
    image.resize((100, 100), Image.Resampling.LANCZOS).save(IMAGE_DIR / "zero-print_100.png")
    image.resize((80, 80), Image.Resampling.LANCZOS).save(IMAGE_DIR / "zero-print_80.png")


def make_header(draw):
    draw.rectangle((0, 0, 319, 25), fill="#f1f3f2")
    draw.line((0, 25, 319, 25), fill="#d8dce0", width=1)
    draw.text((8, 5), "ZERO PRINT", font=load_font(13, True), fill="#1f2825")
    draw.text((250, 7), "14:28", font=load_font(10), fill="#424a47")
    draw.rounded_rectangle((293, 8, 312, 17), radius=2, outline="#424a47", width=1)
    draw.rectangle((295, 10, 308, 15), fill="#145d4f")
    draw.rectangle((313, 11, 315, 14), fill="#424a47")


def make_home_screenshot():
    image = Image.new("RGB", (320, 170), "#fafbfa")
    draw = ImageDraw.Draw(image)
    make_header(draw)
    cards = (
        ((8, 42, 152, 126), "#145d4f", "CAMERA", "SNAP + PRINT"),
        ((168, 42, 312, 126), "#343b42", "SETTINGS", "BLE / USB"),
    )
    for index, (bounds, color, title, subtitle) in enumerate(cards):
        draw.rounded_rectangle(bounds, radius=5, fill=color)
        border = "#f0a629" if index == 0 else color
        draw.rounded_rectangle(bounds, radius=5, outline=border, width=2)
        center_x = (bounds[0] + bounds[2]) // 2
        draw.text((center_x, 72), title, font=load_font(17, True), fill="#ffffff", anchor="mm")
        draw.text((center_x, 99), subtitle, font=load_font(10), fill="#e4e8e8", anchor="mm")
    draw.text((160, 146), "Up/Down Select | Enter/OK Open | Hold Esc Quit",
              font=load_font(8), fill="#65706b", anchor="mm")
    image.save(SCREENSHOT_DIR / "zero-print-home.png")


def make_settings_screenshot():
    image = Image.new("RGB", (320, 170), "#fafbfa")
    draw = ImageDraw.Draw(image)
    make_header(draw)
    rows = (
        (29, "CONNECTION", "MX06 - BLE"),
        (66, "PRINTER", "Scan for MX06"),
        (103, "DITHER", "Floyd-Steinberg"),
    )
    for index, (top, title, value) in enumerate(rows):
        bounds = (16, top, 304, top + 34)
        draw.rounded_rectangle(bounds, radius=4, fill="#f3f4f4")
        draw.rounded_rectangle(bounds,
                               radius=4,
                               outline="#e49b25" if index == 0 else "#d8dce0",
                               width=2 if index == 0 else 1)
        draw.text((25, top + 10), title, font=load_font(12), fill="#252c29")
        draw.text((295, top + 10), value, font=load_font(11, True), fill="#252c29", anchor="ra")
    draw.text((16, 148), "Up/Down: row  Left/Right: mode  Enter: scan  Esc: back",
              font=load_font(8), fill="#65706b")
    image.save(SCREENSHOT_DIR / "zero-print-settings.png")


def main():
    IMAGE_DIR.mkdir(parents=True, exist_ok=True)
    SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
    make_icon()
    make_home_screenshot()
    make_settings_screenshot()


if __name__ == "__main__":
    main()