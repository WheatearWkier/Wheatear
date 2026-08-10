from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path
from typing import Callable

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
SC_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice" / "side_combat"


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def upscale(image: Image.Image, scale: int = 3) -> Image.Image:
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
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(12)))
    return image


def blob_shadow() -> Image.Image:
    image, d = alpha_canvas((128, 48))
    d.ellipse((8, 10, 120, 40), fill=(0, 0, 0, 128))
    return image.filter(ImageFilter.GaussianBlur(4))


def draw_player_pose(
    pose: str,
    frame: int,
    frame_count: int,
) -> Image.Image:
    image, d = alpha_canvas((92, 104))
    t = 0.0 if frame_count <= 1 else (frame - 1) / (frame_count - 1)
    wave_offset = int(round(math.sin((frame - 1) * math.pi * 2.0 / max(1, frame_count)) * 2))

    body_x = 43
    body_y = 36
    torso_color = (34, 118, 142, 255)
    cloak_dark = (23, 35, 57, 255)
    cyan = (57, 236, 243, 255)

    if pose == "run":
        body_x += wave_offset
        body_y += abs(wave_offset) // 2
    elif pose in {"jump", "launcher", "break_limit"}:
        body_y -= 4
    elif pose in {"fall", "air_basic", "air_chase"}:
        body_y -= 2
    elif pose == "hit":
        body_x -= 3 + frame
    elif pose == "dead":
        body_y += 14

    d.ellipse((22, 88, 70, 98), fill=(0, 0, 0, 65))

    if pose == "dead":
        d.polygon([(25, 74), (67, 76), (70, 88), (21, 88)], fill=cloak_dark)
        d.rectangle((32, 67, 58, 82), fill=torso_color)
        d.ellipse((57, 57, 73, 73), fill=(238, 190, 158, 255))
        d.line((26, 62, 73, 82), fill=cyan, width=3)
        return upscale(image)

    d.polygon([(body_x - 8, body_y - 24), (body_x + 3, body_y - 31), (body_x + 21, body_y - 22), (body_x + 8, body_y - 18)],
              fill=(38, 42, 64, 255))
    d.polygon([(body_x - 7, body_y - 21), (body_x + 9, body_y - 24), (body_x + 18, body_y - 14), (body_x + 9, body_y - 2), (body_x - 6, body_y - 4)],
              fill=(238, 190, 158, 255))
    d.point((body_x + 3, body_y - 12), fill=(46, 93, 122, 255))
    d.point((body_x + 11, body_y - 12), fill=(46, 93, 122, 255))

    d.rectangle((body_x - 8, body_y, body_x + 12, body_y + 30), fill=torso_color)
    d.polygon([(body_x - 8, body_y + 2), (body_x - 20, body_y + 20), (body_x - 11, body_y + 26), (body_x - 2, body_y + 11)],
              fill=(43, 178, 171, 255))
    d.polygon([(body_x + 12, body_y + 2), (body_x + 25, body_y + 19), (body_x + 16, body_y + 27), (body_x + 6, body_y + 11)],
              fill=(28, 75, 105, 255))

    if pose == "run":
        leg_a = 12 if frame % 2 else -9
        leg_b = -leg_a
    elif pose in {"jump", "air_basic", "air_chase", "break_limit"}:
        leg_a, leg_b = -4, 7
    elif pose == "launcher":
        leg_a, leg_b = -8, 10
    elif pose == "hit":
        leg_a, leg_b = -3, -8
    else:
        leg_a, leg_b = 0, 5

    d.line((body_x - 3, body_y + 30, body_x - 5 + leg_a, body_y + 55), fill=(24, 37, 57, 255), width=6)
    d.line((body_x + 8, body_y + 30, body_x + 8 + leg_b, body_y + 56), fill=(24, 37, 57, 255), width=6)
    d.rectangle((body_x - 13 + leg_a, body_y + 55, body_x + 1 + leg_a, body_y + 61), fill=(38, 76, 88, 255))
    d.rectangle((body_x + 1 + leg_b, body_y + 55, body_x + 16 + leg_b, body_y + 61), fill=(38, 76, 88, 255))
    d.rectangle((body_x - 12, body_y + 11, body_x + 14, body_y + 17), fill=(75, 232, 224, 218))

    sword_start = (body_x + 22, body_y - 14)
    sword_end = (body_x + 31, body_y + 48)
    if pose in {"basic1", "basic2", "basic3"}:
        sword_start = (body_x + 6, body_y + 8)
        sword_end = (body_x + int(8 + 42 * t), body_y + int(45 - 42 * t))
        d.arc((body_x + 8, body_y - 8, body_x + 61, body_y + 47), 210 - int(t * 70), 316, fill=(70, 236, 255, 155), width=5)
    elif pose == "launcher":
        sword_start = (body_x + 2, body_y + 28)
        sword_end = (body_x + int(3 + 22 * t), body_y - 34)
        d.line((body_x + 20, body_y + 35, body_x + 20, body_y - 31), fill=(77, 240, 255, 126), width=7)
    elif pose in {"air_basic", "air_chase"}:
        sword_start = (body_x + 1, body_y + 3)
        sword_end = (body_x + 46, body_y + int(4 + 20 * t))
        d.arc((body_x + 1, body_y - 12, body_x + 59, body_y + 45), 190, 338, fill=(88, 248, 255, 150), width=5)
    elif pose == "magic_bolt":
        sword_start = (body_x + 15, body_y + 7)
        sword_end = (body_x + 45, body_y + 7)
        d.ellipse((body_x + 33 + frame * 2, body_y - 3, body_x + 51 + frame * 2, body_y + 15), fill=(90, 222, 255, 190))
    elif pose == "ally_support":
        d.ellipse((body_x - 16 - frame, body_y - 16 - frame, body_x + 31 + frame, body_y + 32 + frame), outline=(255, 235, 128, 190), width=3)
    elif pose == "break_limit":
        sword_start = (body_x - 7, body_y + 24)
        sword_end = (body_x + 42, body_y - 22)
        d.arc((body_x - 23, body_y - 23, body_x + 67, body_y + 66), 206, 336, fill=(91, 255, 223, 180), width=6)

    d.line(sword_start + sword_end, fill=(35, 222, 238, 255), width=4)
    d.line((sword_end[0] - 1, sword_end[1], sword_end[0] + 5, sword_end[1] - 7), fill=(228, 255, 255, 255), width=2)
    return upscale(image)


def draw_bear_pose(pose: str, frame: int, frame_count: int) -> Image.Image:
    image, d = alpha_canvas((126, 92))
    t = 0.0 if frame_count <= 1 else (frame - 1) / (frame_count - 1)
    bob = int(round(math.sin((frame - 1) * math.pi * 2.0 / max(1, frame_count)) * 2))
    x = 60 + (bob if pose == "run" else 0)
    y = 42 + (abs(bob) if pose == "run" else 0)
    if pose == "hit":
        x -= 2 + frame
    if pose == "fall":
        y -= 5
    if pose == "dead":
        y += 16

    d.ellipse((24, 78, 104, 88), fill=(0, 0, 0, 72))
    if pose == "dead":
        d.ellipse((19, y + 3, 96, y + 36), fill=(87, 54, 42, 255))
        d.ellipse((63, y - 4, 113, y + 30), fill=(111, 69, 48, 255))
        d.line((72, y + 14, 101, y + 15), fill=(225, 200, 158, 255), width=3)
        return upscale(image)

    d.ellipse((20, y - 14, 96, y + 37), fill=(87, 54, 42, 255))
    d.ellipse((61, y - 30, 114, y + 18), fill=(112, 70, 48, 255))
    d.ellipse((61, y - 33, 78, y - 16), fill=(60, 38, 32, 255))
    d.ellipse((98, y - 33, 116, y - 14), fill=(60, 38, 32, 255))
    leg_shift = 5 if frame % 2 else -3
    d.rectangle((30 + leg_shift, y + 26, 45 + leg_shift, y + 50), fill=(54, 34, 28, 255))
    d.rectangle((72 - leg_shift, y + 25, 88 - leg_shift, y + 50), fill=(54, 34, 28, 255))

    arm_extend = int(12 * t) if pose in {"enemy_claw", "bear_charge"} else 0
    d.polygon([(18, y - 4), (2, y + 7), (18, y + 14), (35, y + 4)], fill=(95, 58, 42, 255))
    d.polygon([(90, y + 1), (121 + arm_extend, y + 3), (96, y + 16)], fill=(82, 50, 38, 255))
    d.ellipse((80, y - 13, 87, y - 6), fill=(248, 75, 54, 255))
    d.ellipse((100, y - 13, 107, y - 6), fill=(248, 75, 54, 255))
    d.rectangle((90, y, 99, y + 6), fill=(32, 24, 22, 255))
    d.line((76, y + 12, 108, y + 13), fill=(230, 210, 164, 255), width=3)

    if pose == "enemy_claw":
        for c in range(3):
            sx = 101 + c * 5 + int(10 * t)
            d.line((sx, y - 11, sx - 16, y + 26), fill=(255, 196, 132, 230), width=3)
    elif pose == "bear_charge":
        for c in range(4):
            d.line((8, y - 20 + c * 8, 55 - c * 5, y - 24 + c * 8), fill=(182, 210, 206, 120), width=3)
    elif pose == "bear_shockwave":
        d.arc((74, y - 12 - frame, 122 + frame * 8, y + 38 + frame), 205, 335, fill=(94, 228, 255, 190), width=4)
    elif pose == "hit":
        d.line((47, y - 25, 37, y - 37), fill=(255, 230, 140, 220), width=3)
    return upscale(image)


def draw_claw_beast_pose(pose: str, frame: int, frame_count: int) -> Image.Image:
    image, d = alpha_canvas((84, 76))
    t = 0.0 if frame_count <= 1 else (frame - 1) / (frame_count - 1)
    bob = int(round(math.sin((frame - 1) * math.pi * 2.0 / max(1, frame_count)) * 2))
    x = 40 + (bob if pose == "run" else 0)
    y = 38 + (abs(bob) if pose == "run" else 0)
    if pose == "hit":
        x -= 2 + frame
    if pose == "fall":
        y -= 3
    if pose == "dead":
        y += 12

    d.ellipse((13, 63, 67, 71), fill=(0, 0, 0, 64))
    if pose == "dead":
        d.ellipse((13, y + 5, 63, y + 25), fill=(73, 82, 78, 255))
        d.ellipse((37, y - 1, 75, y + 22), fill=(92, 112, 108, 255))
        return upscale(image)

    d.ellipse((x - 28, y - 7, x + 18, y + 21), fill=(73, 82, 78, 255))
    d.ellipse((x - 5, y - 20, x + 33, y + 10), fill=(92, 112, 108, 255))
    d.polygon([(x + 1, y - 20), (x + 9, y - 33), (x + 15, y - 17)], fill=(54, 65, 66, 255))
    d.polygon([(x + 20, y - 18), (x + 31, y - 30), (x + 28, y - 11)], fill=(54, 65, 66, 255))
    d.ellipse((x + 10, y - 6, x + 15, y - 1), fill=(96, 233, 255, 255))
    d.ellipse((x + 25, y - 6, x + 30, y - 1), fill=(96, 233, 255, 255))
    leg_shift = 4 if frame % 2 else -2
    d.rectangle((x - 19 + leg_shift, y + 17, x - 10 + leg_shift, y + 31), fill=(45, 50, 50, 255))
    d.rectangle((x + 5 - leg_shift, y + 16, x + 14 - leg_shift, y + 31), fill=(45, 50, 50, 255))
    d.line((x - 32, y + 8, x - 42, y + 15), fill=(230, 228, 197, 255), width=2)

    if pose == "enemy_claw":
        for c in range(2):
            sx = x + 31 + c * 7 + int(9 * t)
            d.line((sx, y - 12, sx - 18, y + 22), fill=(255, 190, 126, 230), width=3)
    elif pose == "hit":
        d.line((x - 10, y - 22, x - 24, y - 31), fill=(255, 230, 140, 220), width=3)
    return upscale(image)


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


def save_effect_sequence(prefix: str, frame_count: int, factory: Callable[[int], Image.Image]) -> None:
    for frame in range(1, frame_count + 1):
        image = factory(frame)
        save(image, SC_ROOT / "effects" / f"{prefix}_{frame:02}.png")


def save_character_sequence(
    folder: str,
    prefix: str,
    pose: str,
    frame_count: int,
    factory: Callable[[str, int, int], Image.Image],
    alias: str | None = None,
) -> None:
    for frame in range(1, frame_count + 1):
        image = factory(pose, frame, frame_count)
        save(image, SC_ROOT / folder / f"{prefix}_{frame:02}.png")


def write_wav(path: Path, duration: float, generator: Callable[[float], float], sample_rate: int = 44100) -> None:
    ensure(path)
    sample_count = int(duration * sample_rate)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        frames = bytearray()
        for i in range(sample_count):
            t = i / sample_rate
            env = max(0.0, 1.0 - t / max(0.001, duration))
            sample = max(-1.0, min(1.0, generator(t) * env))
            frames += struct.pack("<h", int(sample * 32767))
        wav.writeframes(frames)


def sine(freq: float, t: float) -> float:
    return math.sin(2.0 * math.pi * freq * t)


def make_audio() -> None:
    random.seed(7)
    audio = SC_ROOT / "audio"
    write_wav(audio / "swing_light.wav", 0.12, lambda t: 0.32 * sine(880 - 2600 * t, t) + random.uniform(-0.18, 0.18))
    write_wav(audio / "swing_heavy.wav", 0.16, lambda t: 0.42 * sine(620 - 2100 * t, t) + random.uniform(-0.22, 0.22))
    write_wav(audio / "swing_air.wav", 0.11, lambda t: 0.28 * sine(1180 - 3200 * t, t) + random.uniform(-0.14, 0.14))
    write_wav(audio / "swing_upper.wav", 0.18, lambda t: 0.38 * sine(520 + 2500 * t, t) + random.uniform(-0.16, 0.16))
    write_wav(audio / "hit_light.wav", 0.13, lambda t: 0.50 * sine(170, t) + random.uniform(-0.45, 0.45))
    write_wav(audio / "hit_heavy.wav", 0.18, lambda t: 0.62 * sine(105, t) + random.uniform(-0.48, 0.48))
    write_wav(audio / "hit_air.wav", 0.12, lambda t: 0.42 * sine(260, t) + random.uniform(-0.34, 0.34))
    write_wav(audio / "hit_launcher.wav", 0.20, lambda t: 0.54 * sine(130 + 520 * t, t) + random.uniform(-0.38, 0.38))
    write_wav(audio / "player_hit.wav", 0.16, lambda t: 0.46 * sine(120, t) + random.uniform(-0.40, 0.40))
    write_wav(audio / "magic_cast.wav", 0.20, lambda t: 0.30 * sine(740 + 1100 * t, t) + 0.22 * sine(1480 + 1400 * t, t))
    write_wav(audio / "magic_hit.wav", 0.20, lambda t: 0.38 * sine(220, t) + 0.24 * sine(920 - 1300 * t, t) + random.uniform(-0.18, 0.18))
    write_wav(audio / "support_cast.wav", 0.24, lambda t: 0.26 * sine(520, t) + 0.22 * sine(1040 + 400 * t, t))
    write_wav(audio / "support_hit.wav", 0.20, lambda t: 0.34 * sine(360, t) + random.uniform(-0.28, 0.28))
    write_wav(audio / "break_limit.wav", 0.30, lambda t: 0.42 * sine(180 + 820 * t, t) + 0.22 * sine(1460, t) + random.uniform(-0.18, 0.18))
    write_wav(audio / "enemy_swing.wav", 0.14, lambda t: 0.36 * sine(360 - 1200 * t, t) + random.uniform(-0.24, 0.24))
    write_wav(audio / "bear_charge.wav", 0.24, lambda t: 0.50 * sine(80, t) + random.uniform(-0.34, 0.34))
    write_wav(audio / "shockwave_cast.wav", 0.24, lambda t: 0.40 * sine(90 + 280 * t, t) + 0.18 * sine(760, t))
    write_wav(audio / "jump.wav", 0.14, lambda t: 0.25 * sine(420 + 1200 * t, t))
    write_wav(audio / "land.wav", 0.14, lambda t: 0.40 * sine(95, t) + random.uniform(-0.25, 0.25))


def main() -> None:
    save(stage_background(), SC_ROOT / "backgrounds" / "bg_black_forest_stage.png")
    save(blob_shadow(), SC_ROOT / "ui" / "blob_shadow_soft.png")

    player_clips = {
        "protag_idle": ("idle", 4, None),
        "protag_run": ("run", 6, None),
        "protag_jump": ("jump", 3, None),
        "protag_fall": ("fall", 3, None),
        "protag_hit": ("hit", 3, None),
        "protag_dead": ("dead", 4, None),
        "protag_basic1": ("basic1", 4, None),
        "protag_basic2": ("basic2", 4, None),
        "protag_basic3": ("basic3", 5, None),
        "protag_air_basic": ("air_basic", 4, None),
        "protag_launcher": ("launcher", 5, None),
        "protag_air_chase": ("air_chase", 4, None),
        "protag_magic_bolt": ("magic_bolt", 4, None),
        "protag_ally_support": ("ally_support", 4, None),
        "protag_break_limit": ("break_limit", 5, None),
    }
    for prefix, (pose, frames, alias) in player_clips.items():
        save_character_sequence("characters", prefix, pose, frames, draw_player_pose, alias)

    bear_clips = {
        "boss_bear_husband_idle": ("idle", 4, None),
        "boss_bear_husband_walk": ("run", 5, None),
        "boss_bear_husband_hit": ("hit", 3, None),
        "boss_bear_husband_fall": ("fall", 3, None),
        "boss_bear_husband_dead": ("dead", 4, None),
        "boss_bear_husband_attack": ("enemy_claw", 4, None),
        "boss_bear_husband_charge": ("bear_charge", 4, None),
        "boss_bear_husband_shockwave": ("bear_shockwave", 4, None),
    }
    for prefix, (pose, frames, alias) in bear_clips.items():
        save_character_sequence("enemies", prefix, pose, frames, draw_bear_pose, alias)

    beast_clips = {
        "en_claw_beast_idle": ("idle", 4, None),
        "en_claw_beast_run": ("run", 5, None),
        "en_claw_beast_hit": ("hit", 3, None),
        "en_claw_beast_fall": ("fall", 3, None),
        "en_claw_beast_dead": ("dead", 4, None),
        "en_claw_beast_attack": ("enemy_claw", 4, None),
    }
    for prefix, (pose, frames, alias) in beast_clips.items():
        save_character_sequence("enemies", prefix, pose, frames, draw_claw_beast_pose, alias)

    save_effect_sequence("vfx_basic_slash", 4, slash_frame)
    save_effect_sequence("vfx_launcher_slash", 5, launcher_frame)
    save_effect_sequence("vfx_magic_bolt", 4, magic_bolt_frame)
    save_effect_sequence("vfx_ally_support", 5, support_frame)
    save_effect_sequence("vfx_enemy_claw", 3, enemy_claw_frame)
    save_effect_sequence("vfx_boss_bear_charge", 3, bear_charge_frame)
    save_effect_sequence("vfx_boss_bear_shockwave", 4, shockwave_frame)
    make_audio()


if __name__ == "__main__":
    main()
