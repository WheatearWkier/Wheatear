from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
SC_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice" / "side_combat"


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def upscale(image: Image.Image, scale: int = 4) -> Image.Image:
    return image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)


def alpha_canvas(size: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def stage_background() -> Image.Image:
    w, h = 1920, 1080
    image = Image.new("RGBA", (w, h), (18, 24, 34, 255))
    d = ImageDraw.Draw(image)

    for y in range(h):
        t = y / h
        top = (34, 67, 84)
        bottom = (15, 19, 29)
        color = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3))
        d.line((0, y, w, y), fill=(*color, 255))

    d.ellipse((1290, 92, 1508, 310), fill=(212, 228, 218, 190))
    d.ellipse((1340, 112, 1528, 300), fill=(34, 67, 84, 255))

    for x in range(-160, w + 180, 130):
        height = 360 + (x % 4) * 42
        d.rectangle((x + 46, 310, x + 78, 760), fill=(35, 35, 39, 255))
        d.ellipse((x - 42, 135, x + 178, height), fill=(31, 72, 64, 245))
        d.ellipse((x - 6, 210, x + 212, height + 70), fill=(26, 58, 56, 230))

    d.polygon([(0, 432), (1920, 390), (1920, 1080), (0, 1080)], fill=(45, 54, 62, 255))
    d.polygon([(0, 610), (1920, 548), (1920, 1080), (0, 1080)], fill=(65, 75, 78, 255))
    d.polygon([(0, 748), (1920, 680), (1920, 1080), (0, 1080)], fill=(79, 89, 83, 255))

    for i in range(10):
        y = 520 + i * 54
        d.line((0, y + i * 4, w, y - i * 5), fill=(105, 124, 118, 115), width=3)

    vanishing_x = 1110
    for x in range(-300, w + 420, 220):
        d.line((x, h, vanishing_x, 430), fill=(108, 130, 129, 95), width=4)

    d.line((0, 790, 1920, 724), fill=(220, 205, 86, 170), width=7)
    d.line((0, 812, 1920, 746), fill=(174, 161, 72, 125), width=5)

    for x in (1040, 1210, 1380):
        d.rounded_rectangle((x, 430, x + 72, 610), radius=18, fill=(85, 86, 85, 255), outline=(148, 151, 145, 255), width=4)
        d.ellipse((x + 4, 402, x + 68, 450), fill=(92, 82, 58, 255), outline=(174, 151, 88, 255), width=4)

    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for x in range(120, w, 180):
        gd.ellipse((x, 714, x + 42, 736), fill=(80, 224, 255, 115))
    glow = glow.filter(ImageFilter.GaussianBlur(12))
    image.alpha_composite(glow)
    return image


def blob_shadow() -> Image.Image:
    image, d = alpha_canvas((128, 48))
    d.ellipse((8, 10, 120, 40), fill=(0, 0, 0, 128))
    return image.filter(ImageFilter.GaussianBlur(4))


def player_magic_swordsman() -> Image.Image:
    image, d = alpha_canvas((80, 96))
    d.ellipse((24, 84, 58, 91), fill=(0, 0, 0, 62))
    d.polygon([(39, 14), (51, 20), (48, 32), (33, 30), (29, 20)], fill=(236, 190, 158, 255))
    d.polygon([(30, 15), (42, 7), (58, 16), (48, 19), (38, 18)], fill=(39, 43, 62, 255))
    d.rectangle((31, 32, 51, 58), fill=(34, 104, 128, 255))
    d.polygon([(31, 34), (21, 52), (29, 58), (38, 42)], fill=(42, 161, 158, 255))
    d.polygon([(51, 35), (62, 52), (55, 59), (45, 42)], fill=(29, 72, 102, 255))
    d.rectangle((35, 58, 43, 80), fill=(27, 36, 54, 255))
    d.rectangle((47, 58, 55, 81), fill=(27, 36, 54, 255))
    d.rectangle((33, 80, 44, 86), fill=(41, 75, 85, 255))
    d.rectangle((46, 80, 58, 86), fill=(41, 75, 85, 255))
    d.line((60, 17, 66, 72), fill=(35, 222, 238, 255), width=4)
    d.line((61, 17, 68, 7), fill=(228, 255, 255, 255), width=3)
    d.line((65, 26, 69, 64), fill=(137, 255, 255, 210), width=2)
    d.rectangle((28, 44, 54, 50), fill=(74, 226, 222, 210))
    d.point((38, 23), fill=(53, 92, 118, 255))
    d.point((47, 23), fill=(53, 92, 118, 255))
    return upscale(image, 3)


def bear_husband() -> Image.Image:
    image, d = alpha_canvas((112, 88))
    d.ellipse((22, 26, 92, 76), fill=(87, 54, 42, 255))
    d.ellipse((54, 12, 104, 60), fill=(112, 70, 48, 255))
    d.ellipse((54, 9, 70, 25), fill=(60, 38, 32, 255))
    d.ellipse((91, 9, 108, 27), fill=(60, 38, 32, 255))
    d.rectangle((28, 62, 42, 84), fill=(54, 34, 28, 255))
    d.rectangle((69, 62, 85, 84), fill=(54, 34, 28, 255))
    d.polygon([(18, 38), (5, 48), (18, 55), (34, 45)], fill=(95, 58, 42, 255))
    d.polygon([(87, 42), (110, 46), (93, 56)], fill=(82, 50, 38, 255))
    d.ellipse((73, 28, 80, 35), fill=(248, 75, 54, 255))
    d.ellipse((92, 28, 99, 35), fill=(248, 75, 54, 255))
    d.rectangle((83, 40, 91, 46), fill=(32, 24, 22, 255))
    d.line((70, 52, 101, 53), fill=(230, 210, 164, 255), width=3)
    d.line((11, 55, 2, 61), fill=(230, 220, 188, 255), width=2)
    d.line((98, 55, 111, 62), fill=(230, 220, 188, 255), width=2)
    return upscale(image, 3)


def small_claw_beast() -> Image.Image:
    image, d = alpha_canvas((80, 72))
    d.ellipse((15, 30, 62, 58), fill=(73, 82, 78, 255))
    d.ellipse((36, 18, 72, 48), fill=(92, 112, 108, 255))
    d.polygon([(42, 18), (49, 6), (56, 21)], fill=(54, 65, 66, 255))
    d.polygon([(62, 20), (72, 10), (70, 28)], fill=(54, 65, 66, 255))
    d.ellipse((51, 31, 56, 36), fill=(96, 233, 255, 255))
    d.ellipse((65, 31, 70, 36), fill=(96, 233, 255, 255))
    d.rectangle((22, 54, 31, 67), fill=(45, 50, 50, 255))
    d.rectangle((49, 53, 58, 67), fill=(45, 50, 50, 255))
    d.line((9, 45, 0, 51), fill=(230, 228, 197, 255), width=2)
    return upscale(image, 3)


def slash_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((96, 96))
    alpha = 165 + frame * 20
    x_offset = (frame - 1) * 3
    d.arc((6 + x_offset, 20, 90 + x_offset, 78), 198, 342, fill=(70, 234, 255, alpha), width=13)
    d.arc((14 + x_offset, 27, 91 + x_offset, 82), 205, 335, fill=(255, 255, 255, min(255, alpha + 20)), width=5)
    d.line((24 + x_offset, 63, 72 + x_offset, 31), fill=(79, 255, 226, 155), width=3)
    return upscale(image, 2)


def launcher_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((96, 128))
    grow = frame * 5
    d.polygon([(42, 122), (50, 8), (60 + grow // 3, 122)], fill=(65, 230, 255, 120 + frame * 22))
    d.line((50, 12, 50, 118), fill=(255, 255, 255, 235), width=5)
    d.arc((21 - grow // 2, 34 - grow, 76 + grow // 2, 120), 246, 340, fill=(104, 255, 231, 210), width=7)
    d.arc((17, 46, 82, 122), 210, 282, fill=(53, 155, 255, 150), width=5)
    return upscale(image, 2)


def magic_bolt_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((96, 64))
    shift = (frame % 2) * 3
    d.ellipse((23 + shift, 20, 76 + shift, 45), fill=(70, 210, 255, 210))
    d.ellipse((42 + shift, 24, 92 + shift, 40), fill=(232, 255, 255, 235))
    d.polygon([(4, 30), (28 + shift, 20), (28 + shift, 45)], fill=(71, 130, 255, 135))
    d.line((10, 36, 60 + shift, 32), fill=(180, 255, 255, 180), width=3)
    return upscale(image, 2)


def support_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((96, 128))
    radius = 16 + frame * 6
    d.ellipse((48 - radius, 62 - radius, 48 + radius, 62 + radius), outline=(255, 236, 132, 235), width=4)
    d.rectangle((43, 8, 54, 120), fill=(255, 248, 166, 96 + frame * 24))
    d.line((35, 28, 62, 96), fill=(255, 255, 255, 210), width=3)
    d.line((63, 30, 35, 98), fill=(149, 244, 255, 190), width=3)
    return upscale(image, 2)


def enemy_claw_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((96, 96))
    offset = frame * 4
    for x in (30, 48, 66):
        d.line((x + offset, 16, x - 20 + offset, 82), fill=(255, 190, 126, 230), width=6)
        d.line((x + offset + 3, 18, x - 17 + offset, 78), fill=(255, 74, 54, 135), width=2)
    return upscale(image, 2)


def bear_charge_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((128, 80))
    for i in range(5):
        y = 20 + i * 9 + frame
        d.line((8, y, 96 - i * 8, y - 6), fill=(182, 210, 206, 95 + frame * 30), width=4)
    d.ellipse((62, 44, 124, 68), fill=(110, 92, 68, 150))
    return upscale(image, 2)


def shockwave_frame(frame: int) -> Image.Image:
    image, d = alpha_canvas((128, 64))
    width = 30 + frame * 20
    d.arc((8, 24 - frame, 8 + width, 66 + frame), 200, 342, fill=(106, 230, 255, 190), width=5)
    d.arc((34, 28 - frame, 34 + width, 70 + frame), 200, 342, fill=(255, 240, 150, 150), width=4)
    d.line((8, 50, 122, 50), fill=(132, 192, 205, 95), width=3)
    return upscale(image, 2)


def save_sequence(prefix: str, frame_count: int, factory) -> None:
    first = None
    for frame in range(1, frame_count + 1):
        image = factory(frame)
        if first is None:
            first = image
        save(image, SC_ROOT / "effects" / f"{prefix}_{frame:02}.png")
    if first is not None:
        alias = {
            "slash_basic": "slash_basic.png",
            "slash_launcher": "slash_launcher.png",
            "magic_bolt": "magic_bolt.png",
            "ally_support": "ally_support.png",
            "enemy_claw": "enemy_claw.png",
            "bear_shockwave": "enemy_projectile.png",
        }.get(prefix)
        if alias:
            save(first, SC_ROOT / "effects" / alias)


def main() -> None:
    save(stage_background(), SC_ROOT / "backgrounds" / "black_forest_stage.png")
    save(player_magic_swordsman(), SC_ROOT / "characters" / "player_magic_swordsman.png")
    save(bear_husband(), SC_ROOT / "enemies" / "bear_husband.png")
    save(small_claw_beast(), SC_ROOT / "enemies" / "small_claw_beast.png")
    save(blob_shadow(), SC_ROOT / "ui" / "blob_shadow.png")

    save_sequence("slash_basic", 4, slash_frame)
    save_sequence("slash_launcher", 5, launcher_frame)
    save_sequence("magic_bolt", 4, magic_bolt_frame)
    save_sequence("ally_support", 5, support_frame)
    save_sequence("enemy_claw", 3, enemy_claw_frame)
    save_sequence("bear_charge", 3, bear_charge_frame)
    save_sequence("bear_shockwave", 4, shockwave_frame)


if __name__ == "__main__":
    main()
