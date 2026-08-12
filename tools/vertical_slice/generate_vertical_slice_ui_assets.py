from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
UI_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice" / "ui"


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def alpha_canvas(size: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def upscale(image: Image.Image, scale: int = 4) -> Image.Image:
    return image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)


def panel(size: tuple[int, int], body: tuple[int, int, int], border: tuple[int, int, int], glow: tuple[int, int, int]) -> Image.Image:
    image, d = alpha_canvas(size)
    w, h = size
    d.rounded_rectangle((2, 2, w - 3, h - 3), radius=4, fill=(*body, 220), outline=(*border, 245), width=2)
    d.rectangle((5, 5, w - 6, 7), fill=(*glow, 210))
    d.rectangle((5, h - 8, w - 6, h - 6), fill=(0, 0, 0, 80))
    for y in range(13, h - 12, 10):
        d.line((8, y, w - 9, y), fill=(*glow, 24), width=1)
    return upscale(image, 3)


def icon_armor() -> Image.Image:
    image, d = alpha_canvas((64, 64))
    d.polygon([(17, 12), (32, 6), (47, 12), (51, 30), (43, 55), (32, 60), (21, 55), (13, 30)], fill=(86, 109, 122, 255), outline=(205, 223, 220, 245))
    d.polygon([(24, 16), (32, 12), (40, 16), (39, 49), (32, 54), (25, 49)], fill=(47, 177, 181, 230))
    d.line((18, 29, 46, 29), fill=(222, 233, 217, 200), width=2)
    d.line((32, 13, 32, 54), fill=(30, 52, 66, 170), width=2)
    return upscale(image, 3)


def icon_dungeon() -> Image.Image:
    image, d = alpha_canvas((64, 64))
    d.rectangle((7, 42, 57, 52), fill=(45, 57, 55, 255))
    d.polygon([(10, 42), (20, 20), (29, 42)], fill=(38, 82, 70, 255))
    d.polygon([(25, 42), (38, 10), (51, 42)], fill=(30, 67, 73, 255))
    d.ellipse((27, 18, 41, 32), fill=(94, 232, 255, 210))
    d.line((31, 24, 37, 24), fill=(255, 255, 255, 220), width=2)
    d.rectangle((15, 52, 49, 57), fill=(170, 158, 80, 230))
    return upscale(image, 3)


def icon_skill_tree() -> Image.Image:
    image, d = alpha_canvas((64, 64))
    centers = [(32, 12), (17, 32), (47, 32), (32, 52)]
    for a, b in [((32, 12), (17, 32)), ((32, 12), (47, 32)), ((17, 32), (32, 52)), ((47, 32), (32, 52))]:
        d.line((*a, *b), fill=(98, 235, 230, 200), width=3)
    for i, (x, y) in enumerate(centers):
        color = (74, 224, 205) if i != 3 else (235, 190, 88)
        d.ellipse((x - 8, y - 8, x + 8, y + 8), fill=(*color, 245), outline=(255, 255, 255, 210), width=2)
    return upscale(image, 3)


def icon_result() -> Image.Image:
    image, d = alpha_canvas((64, 64))
    d.polygon([(32, 4), (40, 24), (61, 24), (44, 37), (51, 58), (32, 45), (13, 58), (20, 37), (3, 24), (24, 24)], fill=(237, 198, 80, 245), outline=(255, 255, 230, 230))
    d.polygon([(32, 14), (37, 28), (52, 28), (40, 37), (44, 50), (32, 42), (20, 50), (24, 37), (12, 28), (27, 28)], fill=(84, 220, 210, 230))
    return upscale(image, 3)


def icon_circle(base: tuple[int, int, int], accent: tuple[int, int, int], mark: str) -> Image.Image:
    image, d = alpha_canvas((48, 48))
    d.ellipse((5, 5, 43, 43), fill=(*base, 245), outline=(242, 246, 226, 225), width=2)
    d.ellipse((12, 12, 36, 36), fill=(*accent, 218))
    if mark == "heart":
        d.polygon([(24, 36), (12, 22), (15, 14), (22, 15), (24, 19), (26, 15), (33, 14), (36, 22)], fill=(255, 216, 225, 245))
    elif mark == "support":
        d.rectangle((14, 19, 34, 25), fill=(238, 245, 220, 245))
        d.rectangle((20, 13, 28, 35), fill=(238, 245, 220, 245))
    elif mark == "settings":
        for angle in range(0, 360, 45):
            x = 24 + int(math.cos(math.radians(angle)) * 12)
            y = 24 + int(math.sin(math.radians(angle)) * 12)
            d.rectangle((x - 2, y - 2, x + 2, y + 2), fill=(238, 245, 220, 235))
        d.ellipse((16, 16, 32, 32), fill=(40, 50, 58, 255), outline=(238, 245, 220, 235), width=2)
    elif mark == "save":
        d.rounded_rectangle((12, 10, 36, 38), radius=3, fill=(238, 245, 220, 245))
        d.rectangle((16, 13, 32, 20), fill=(*base, 245))
        d.rectangle((17, 26, 31, 34), fill=(*accent, 220))
    return upscale(image, 3)


def main() -> None:
    save(panel((96, 40), (20, 26, 34), (80, 219, 213), (98, 235, 230)), UI_ROOT / "panels" / "panel_dark.png")
    save(panel((128, 64), (24, 42, 40), (80, 219, 160), (128, 248, 210)), UI_ROOT / "panels" / "panel_hub.png")
    save(panel((128, 64), (35, 29, 38), (236, 196, 86), (255, 231, 132)), UI_ROOT / "panels" / "panel_result.png")
    save(panel((128, 64), (26, 34, 46), (92, 180, 244), (118, 218, 255)), UI_ROOT / "panels" / "panel_dungeon.png")
    save(panel((128, 64), (42, 30, 42), (226, 138, 162), (255, 190, 205)), UI_ROOT / "panels" / "panel_relationship.png")
    save(panel((128, 64), (34, 36, 48), (156, 142, 238), (190, 184, 255)), UI_ROOT / "panels" / "panel_support.png")
    save(panel((128, 64), (28, 34, 36), (128, 220, 178), (176, 250, 210)), UI_ROOT / "panels" / "panel_settings.png")
    save(icon_armor(), UI_ROOT / "icons" / "icon_armor.png")
    save(icon_dungeon(), UI_ROOT / "icons" / "icon_dungeon.png")
    save(icon_skill_tree(), UI_ROOT / "icons" / "icon_skill_tree.png")
    save(icon_result(), UI_ROOT / "icons" / "icon_result.png")
    save(icon_circle((172, 66, 98), (92, 42, 70), "heart"), UI_ROOT / "icons" / "icon_relationship.png")
    save(icon_circle((83, 78, 177), (48, 52, 114), "support"), UI_ROOT / "icons" / "icon_support.png")
    save(icon_circle((74, 136, 108), (34, 72, 65), "settings"), UI_ROOT / "icons" / "icon_settings.png")
    save(icon_circle((188, 139, 62), (82, 65, 42), "save"), UI_ROOT / "icons" / "icon_save.png")


if __name__ == "__main__":
    main()
