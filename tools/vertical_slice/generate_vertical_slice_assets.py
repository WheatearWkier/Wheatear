from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure_dir(path.parent)
    image.save(path)


def ellipse(draw: ImageDraw.ImageDraw, box, fill, outline=None, width=1) -> None:
    draw.ellipse(box, fill=fill, outline=outline, width=width)


def portrait(base: tuple[int, int, int], hair: tuple[int, int, int], outfit: tuple[int, int, int], mood: str) -> Image.Image:
    image = Image.new("RGBA", (768, 1152), (0, 0, 0, 0))
    shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse((210, 120, 575, 1060), fill=(0, 0, 0, 62))
    shadow = shadow.filter(ImageFilter.GaussianBlur(34))
    image.alpha_composite(shadow)

    d = ImageDraw.Draw(image)

    # Hair mass
    ellipse(d, (165, 90, 610, 590), (*hair, 255))
    d.polygon([(188, 385), (146, 1020), (292, 760)], fill=tuple(max(0, c - 38) for c in hair) + (255,))
    d.polygon([(590, 388), (620, 1035), (477, 760)], fill=tuple(max(0, c - 46) for c in hair) + (255,))
    d.polygon([(210, 96), (384, 58), (562, 106), (470, 210), (290, 190)], fill=tuple(min(255, c + 18) for c in hair) + (255,))

    # Neck and face
    d.rounded_rectangle((333, 485, 435, 670), radius=42, fill=(242, 202, 182, 255))
    ellipse(d, (233, 170, 536, 540), (250, 216, 195, 255))
    d.polygon([(265, 235), (505, 205), (552, 280), (516, 183), (350, 138), (235, 208)], fill=(*hair, 255))
    d.pieslice((197, 255, 292, 390), 90, 260, fill=(250, 216, 195, 255))
    d.pieslice((480, 255, 575, 390), -80, 90, fill=(250, 216, 195, 255))

    # Body
    d.polygon([(210, 1110), (300, 640), (468, 640), (560, 1110)], fill=(*outfit, 255))
    d.polygon([(300, 640), (384, 790), (468, 640)], fill=(246, 232, 218, 255))
    d.line((300, 650, 384, 790, 468, 650), fill=(130, 165, 186, 255), width=8)
    d.rounded_rectangle((206, 766, 562, 1118), radius=38, fill=(*outfit, 245))
    d.line((246, 822, 524, 822), fill=(255, 255, 255, 55), width=5)

    # Eyes by mood
    eye_color = base
    if mood == "happy":
        d.arc((286, 326, 354, 372), 0, 180, fill=(66, 45, 58, 255), width=7)
        d.arc((414, 326, 482, 372), 0, 180, fill=(66, 45, 58, 255), width=7)
        d.arc((346, 430, 424, 470), 10, 170, fill=(160, 62, 82, 255), width=6)
    else:
        left_eye = (286, 318, 354, 388)
        right_eye = (414, 318, 482, 388)
        if mood == "serious":
            left_eye = (286, 326, 354, 382)
            right_eye = (414, 326, 482, 382)
        if mood == "surprised":
            left_eye = (282, 305, 358, 395)
            right_eye = (410, 305, 486, 395)
        ellipse(d, left_eye, (*eye_color, 255), (34, 28, 38, 255), 5)
        ellipse(d, right_eye, (*eye_color, 255), (34, 28, 38, 255), 5)
        ellipse(d, (310, 332, 330, 355), (255, 255, 255, 220))
        ellipse(d, (438, 332, 458, 355), (255, 255, 255, 220))
        mouth = {
            "neutral": (354, 438, 414, 452, 10, 170),
            "thinking": (354, 444, 414, 462, 200, 335),
            "serious": (354, 448, 414, 449, 0, 180),
            "surprised": (366, 430, 402, 470, 0, 360),
        }.get(mood, (354, 438, 414, 452, 10, 170))
        if mood == "serious":
            d.line((354, 448, 414, 448), fill=(130, 54, 70, 255), width=5)
        elif mood == "surprised":
            ellipse(d, mouth[:4], (125, 50, 68, 255))
        else:
            d.arc(mouth[:4], mouth[4], mouth[5], fill=(150, 58, 76, 255), width=5)

    # Hair shine and blush
    d.arc((246, 130, 520, 330), 208, 320, fill=(255, 255, 255, 70), width=12)
    d.ellipse((250, 400, 310, 430), fill=(250, 132, 150, 60))
    d.ellipse((456, 400, 516, 430), fill=(250, 132, 150, 60))

    return image


def vn_background(path: Path, palette: tuple[tuple[int, int, int], tuple[int, int, int]], kind: str) -> None:
    w, h = 1920, 1080
    image = Image.new("RGB", (w, h), palette[0])
    d = ImageDraw.Draw(image)
    for y in range(h):
        t = y / h
        color = tuple(int(palette[0][i] * (1 - t) + palette[1][i] * t) for i in range(3))
        d.line((0, y, w, y), fill=color)
    if kind == "school":
        d.rectangle((0, 650, w, h), fill=(82, 96, 92))
        d.polygon([(0, 820), (1920, 700), (1920, 1080), (0, 1080)], fill=(65, 72, 80))
        d.rectangle((90, 380, 560, 780), fill=(205, 212, 208))
        for x in range(130, 520, 90):
            d.rectangle((x, 430, x + 52, 500), fill=(112, 170, 190))
            d.rectangle((x, 545, x + 52, 615), fill=(112, 170, 190))
        d.ellipse((1320, 80, 1550, 310), fill=(255, 224, 156))
    elif kind == "forest":
        d.rectangle((0, 670, w, h), fill=(42, 58, 50))
        for x in range(-80, w, 155):
            d.rectangle((x + 50, 260, x + 92, 830), fill=(44, 38, 34))
            d.ellipse((x - 30, 90, x + 190, 380), fill=(42, 86, 70))
        d.polygon([(0, 790), (420, 680), (780, 780), (1160, 660), (1920, 790), (1920, 1080), (0, 1080)], fill=(48, 68, 54))
    else:
        d.rectangle((0, 680, w, h), fill=(34, 42, 48))
        for x in range(0, w, 180):
            d.polygon([(x, 700), (x + 70, 420), (x + 140, 700)], fill=(36, 62, 64))
        d.ellipse((1340, 150, 1560, 370), fill=(230, 218, 175))
        d.rectangle((780, 660, 1120, 820), fill=(82, 60, 46))
        d.polygon([(740, 660), (960, 500), (1160, 660)], fill=(120, 82, 58))
    save(image.convert("RGBA"), path)


def pixel_canvas(size=(64, 64)) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def pixel_player() -> Image.Image:
    image, d = pixel_canvas((96, 96))
    d.rectangle((42, 18, 56, 32), fill=(239, 202, 170, 255))
    d.rectangle((34, 32, 64, 64), fill=(54, 105, 138, 255))
    d.rectangle((36, 64, 47, 86), fill=(32, 42, 62, 255))
    d.rectangle((53, 64, 64, 86), fill=(32, 42, 62, 255))
    d.rectangle((30, 34, 38, 58), fill=(239, 202, 170, 255))
    d.rectangle((60, 34, 68, 58), fill=(239, 202, 170, 255))
    d.polygon([(34, 19), (54, 8), (66, 24), (56, 20), (48, 26)], fill=(50, 38, 54, 255))
    d.rectangle((68, 20, 74, 72), fill=(170, 235, 255, 255))
    d.rectangle((72, 10, 76, 24), fill=(235, 255, 255, 255))
    return image


def pixel_bear() -> Image.Image:
    image, d = pixel_canvas((128, 96))
    d.ellipse((28, 22, 112, 78), fill=(88, 54, 38, 255))
    d.ellipse((66, 8, 118, 58), fill=(103, 65, 44, 255))
    d.ellipse((68, 8, 84, 24), fill=(64, 38, 28, 255))
    d.ellipse((102, 8, 118, 24), fill=(64, 38, 28, 255))
    d.rectangle((38, 68, 54, 90), fill=(60, 38, 30, 255))
    d.rectangle((82, 68, 98, 90), fill=(60, 38, 30, 255))
    d.ellipse((84, 28, 92, 36), fill=(255, 72, 58, 255))
    d.ellipse((104, 28, 112, 36), fill=(255, 72, 58, 255))
    d.rectangle((94, 39, 103, 45), fill=(36, 24, 22, 255))
    d.line((80, 50, 116, 52), fill=(240, 220, 180, 255), width=3)
    return image


def effect(kind: str) -> Image.Image:
    image, d = pixel_canvas((96, 96))
    if kind == "slash":
        d.arc((8, 22, 88, 74), 200, 345, fill=(170, 240, 255, 255), width=10)
        d.arc((16, 30, 88, 80), 210, 340, fill=(255, 255, 255, 210), width=4)
    elif kind == "launcher":
        d.polygon([(40, 84), (50, 8), (62, 84)], fill=(152, 238, 255, 210))
        d.line((50, 8, 50, 84), fill=(255, 255, 255, 255), width=5)
    elif kind == "bolt":
        d.ellipse((10, 34, 72, 62), fill=(126, 210, 255, 220))
        d.ellipse((30, 40, 92, 56), fill=(255, 255, 255, 220))
    elif kind == "support":
        d.rectangle((42, 8, 54, 88), fill=(255, 238, 160, 220))
        d.ellipse((20, 24, 76, 80), outline=(255, 250, 190, 240), width=5)
    elif kind == "claw":
        for x in (25, 44, 63):
            d.line((x, 18, x - 18, 80), fill=(255, 200, 152, 240), width=6)
    else:
        d.ellipse((12, 36, 84, 60), fill=(210, 72, 62, 230))
        d.rectangle((16, 44, 82, 52), fill=(255, 170, 80, 220))
    return image


def icon(kind: str) -> Image.Image:
    image, d = pixel_canvas((64, 64))
    if kind == "core":
        d.polygon([(32, 6), (54, 24), (46, 54), (18, 54), (10, 24)], fill=(130, 90, 255, 255))
        d.polygon([(32, 14), (44, 28), (40, 46), (24, 46), (20, 28)], fill=(230, 220, 255, 255))
    elif kind == "sinew":
        d.arc((10, 14, 54, 50), 20, 340, fill=(220, 178, 126, 255), width=8)
        d.arc((16, 18, 48, 46), 20, 340, fill=(120, 78, 54, 255), width=3)
    elif kind == "claw":
        for x in (18, 30, 42):
            d.polygon([(x, 8), (x + 10, 48), (x - 6, 56)], fill=(238, 224, 184, 255))
    else:
        d.rectangle((26, 8, 38, 56), fill=(170, 235, 255, 255))
        d.polygon([(20, 20), (44, 20), (38, 2), (26, 2)], fill=(230, 255, 255, 255))
    return image


def main() -> None:
    portraits = ASSET_ROOT / "vn" / "portraits"
    for mood in ["neutral", "happy", "serious", "surprised", "thinking"]:
        save(portrait((88, 150, 190), (55, 72, 112), (52, 86, 118), mood), portraits / f"aoba_{mood}.png")
        save(portrait((170, 86, 126), (120, 68, 98), (78, 64, 118), mood), portraits / f"mentor_{mood}.png")
        save(portrait((92, 150, 105), (56, 48, 54), (60, 102, 78), mood), portraits / f"hero_{mood}.png")

    backgrounds = ASSET_ROOT / "vn" / "backgrounds"
    vn_background(backgrounds / "bg_modern_schoolroad_morning.png", ((154, 206, 230), (248, 202, 148)), "school")
    vn_background(backgrounds / "bg_otherworld_forest_wake.png", ((68, 116, 102), (24, 38, 46)), "forest")
    vn_background(backgrounds / "bg_forest_camp_night.png", ((36, 54, 70), (16, 20, 28)), "camp")
    vn_background(backgrounds / "bg_chapter3_road.png", ((92, 132, 146), (44, 64, 86)), "forest")

    sc = ASSET_ROOT / "side_combat"
    save(pixel_player(), sc / "sheets" / "runtime_characters" / "protag_magic_swordsman_idle_sheet.png")
    save(pixel_bear(), sc / "sheets" / "runtime_enemies" / "boss_bear_husband_idle_sheet.png")
    save(pixel_bear().resize((96, 72), Image.Resampling.NEAREST), sc / "sheets" / "runtime_enemies" / "en_claw_beast_idle_sheet.png")
    vn_background(sc / "backgrounds" / "bg_black_forest_stage.png", ((50, 78, 72), (16, 22, 30)), "forest")
    save(effect("slash"), sc / "vfx_sheets" / "runtime_effects" / "vfx_basic_slash_sheet.png")
    save(effect("launcher"), sc / "vfx_sheets" / "runtime_effects" / "vfx_launcher_slash_sheet.png")
    save(effect("bolt"), sc / "vfx_sheets" / "runtime_effects" / "vfx_magic_bolt_sheet.png")
    save(effect("support"), sc / "vfx_sheets" / "runtime_effects" / "vfx_ally_support_sheet.png")
    save(effect("claw"), sc / "vfx_sheets" / "runtime_effects" / "vfx_enemy_claw_sheet.png")
    save(effect("projectile"), sc / "vfx_sheets" / "runtime_effects" / "vfx_enemy_projectile_sheet.png")
    save(icon("core"), sc / "ui" / "icon_drop_magic_core.png")
    save(icon("sinew"), sc / "ui" / "icon_drop_beast_sinew.png")
    save(icon("claw"), sc / "ui" / "icon_drop_beast_claw.png")


if __name__ == "__main__":
    main()
