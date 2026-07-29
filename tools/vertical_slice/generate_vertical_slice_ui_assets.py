from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


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


def small_badge(color: tuple[int, int, int], accent: tuple[int, int, int]) -> Image.Image:
    image, d = alpha_canvas((48, 48))
    d.polygon([(24, 3), (40, 10), (45, 27), (34, 43), (14, 43), (3, 27), (8, 10)], fill=(*color, 245), outline=(255, 255, 255, 210))
    d.polygon([(24, 9), (36, 15), (39, 28), (31, 38), (17, 38), (9, 28), (12, 15)], fill=(*accent, 230))
    d.line((15, 27, 23, 35, 35, 17), fill=(255, 255, 255, 235), width=3)
    return upscale(image, 3)


def icon_magic_sword() -> Image.Image:
    image, d = alpha_canvas((64, 64))
    d.ellipse((18, 42, 48, 52), fill=(0, 0, 0, 70))
    d.polygon([(34, 3), (42, 12), (33, 49), (26, 49), (25, 12)], fill=(92, 241, 255, 245))
    d.polygon([(31, 5), (36, 13), (30, 43), (27, 44), (27, 13)], fill=(236, 255, 255, 255))
    d.rectangle((19, 44, 42, 49), fill=(43, 67, 99, 255))
    d.rectangle((27, 49, 35, 59), fill=(81, 58, 102, 255))
    d.line((39, 16, 52, 10), fill=(156, 255, 232, 185), width=2)
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


def material_core() -> Image.Image:
    image, d = alpha_canvas((48, 48))
    d.polygon([(25, 4), (39, 15), (35, 34), (19, 44), (6, 30), (9, 12)], fill=(126, 81, 200, 245), outline=(236, 220, 255, 210))
    d.polygon([(24, 12), (32, 19), (29, 30), (19, 35), (13, 27), (15, 17)], fill=(69, 232, 255, 230))
    return upscale(image, 3)


def material_sinew() -> Image.Image:
    image, d = alpha_canvas((48, 48))
    for offset, color in [(0, (197, 128, 74)), (6, (232, 171, 111)), (12, (159, 93, 70))]:
        d.arc((6 + offset, 9, 34 + offset, 39), 80, 288, fill=(*color, 245), width=5)
    d.line((14, 14, 37, 34), fill=(255, 222, 164, 220), width=2)
    return upscale(image, 3)


def material_claw() -> Image.Image:
    image, d = alpha_canvas((48, 48))
    for x in (14, 24, 34):
        d.polygon([(x, 5), (x + 7, 35), (x - 4, 43)], fill=(232, 224, 186, 245), outline=(94, 75, 55, 180))
    d.rectangle((8, 34, 40, 42), fill=(112, 70, 48, 230))
    return upscale(image, 3)


def icon_history() -> Image.Image:
    image, d = alpha_canvas((48, 48))
    d.rounded_rectangle((8, 5, 39, 43), radius=3, fill=(46, 62, 75, 245), outline=(168, 225, 226, 220), width=2)
    for y in (14, 22, 30, 38):
        d.line((14, y, 33, y), fill=(224, 238, 222, 230), width=2)
    d.rectangle((10, 5, 16, 43), fill=(79, 177, 171, 190))
    return upscale(image, 3)


def icon_tutorial() -> Image.Image:
    image, d = alpha_canvas((48, 48))
    d.ellipse((7, 7, 41, 41), fill=(47, 113, 119, 245), outline=(225, 248, 236, 225), width=2)
    d.rectangle((21, 18, 26, 34), fill=(244, 238, 182, 255))
    d.rectangle((21, 11, 26, 15), fill=(244, 238, 182, 255))
    return upscale(image, 3)


def node(color: tuple[int, int, int], border: tuple[int, int, int]) -> Image.Image:
    image, d = alpha_canvas((40, 40))
    d.ellipse((5, 5, 35, 35), fill=(*color, 235), outline=(*border, 245), width=3)
    d.ellipse((14, 14, 26, 26), fill=(245, 250, 240, 200))
    return upscale(image, 3)


def main() -> None:
    save(panel((96, 40), (20, 26, 34), (80, 219, 213), (98, 235, 230)), UI_ROOT / "panels" / "panel_dark.png")
    save(panel((128, 64), (35, 29, 38), (236, 196, 86), (255, 231, 132)), UI_ROOT / "panels" / "panel_result.png")
    save(panel((128, 64), (25, 32, 50), (92, 156, 244), (120, 210, 255)), UI_ROOT / "panels" / "panel_skill.png")
    save(panel((128, 64), (40, 32, 23), (224, 170, 92), (255, 214, 128)), UI_ROOT / "panels" / "panel_equipment.png")
    save(panel((128, 64), (24, 42, 40), (80, 219, 160), (128, 248, 210)), UI_ROOT / "panels" / "panel_hub.png")
    save(small_badge((214, 171, 74), (70, 197, 203)), UI_ROOT / "badges" / "badge_result_gold.png")
    save(icon_magic_sword(), UI_ROOT / "icons" / "icon_magic_sword.png")
    save(icon_armor(), UI_ROOT / "icons" / "icon_armor.png")
    save(icon_dungeon(), UI_ROOT / "icons" / "icon_dungeon.png")
    save(icon_skill_tree(), UI_ROOT / "icons" / "icon_skill_tree.png")
    save(icon_result(), UI_ROOT / "icons" / "icon_result.png")
    save(material_core(), UI_ROOT / "icons" / "icon_material_core.png")
    save(material_sinew(), UI_ROOT / "icons" / "icon_material_sinew.png")
    save(material_claw(), UI_ROOT / "icons" / "icon_material_claw.png")
    save(icon_history(), UI_ROOT / "icons" / "icon_history.png")
    save(icon_tutorial(), UI_ROOT / "icons" / "icon_tutorial.png")
    save(node((58, 226, 206), (235, 255, 246)), UI_ROOT / "nodes" / "node_unlocked.png")
    save(node((90, 96, 105), (155, 165, 175)), UI_ROOT / "nodes" / "node_locked.png")
    save(node((232, 184, 78), (255, 245, 190)), UI_ROOT / "nodes" / "node_active.png")


if __name__ == "__main__":
    main()
