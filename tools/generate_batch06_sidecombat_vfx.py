from __future__ import annotations

import math
import random
import shutil
from pathlib import Path
from typing import Callable

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"
SOURCE_ROOT = ASSET_ROOT / "source_frames" / "batch06_runtime_vfx"
EFFECT_ROOT = ASSET_ROOT / "side_combat" / "effects"
SHEET_ROOT = ASSET_ROOT / "side_combat" / "vfx_sheets"
PREVIEW_ROOT = ASSET_ROOT / "side_combat" / "previews" / "batch06_runtime_vfx"
BACKUP_ROOT = EFFECT_ROOT / "_backup_before_batch06_runtime_vfx"


C = {
    "outline": (2, 6, 12, 220),
    "outline_soft": (4, 14, 22, 150),
    "silver": (142, 153, 164, 210),
    "silver_hi": (224, 232, 238, 235),
    "cyan_dark": (12, 86, 116, 155),
    "cyan": (24, 218, 255, 230),
    "cyan_hi": (142, 250, 255, 250),
    "white": (245, 255, 255, 255),
    "gold": (255, 198, 92, 230),
    "dust": (116, 105, 88, 130),
    "rock": (58, 58, 64, 190),
}


CLIPS: dict[str, dict[str, object]] = {
    "vfx_basic_slash": {
        "size": (320, 192),
        "frames": 4,
        "fps": 26.0,
        "factory": "basic_slash",
        "alias_frame": 2,
        "attach": "player sword arc",
    },
    "vfx_launcher_slash": {
        "size": (240, 320),
        "frames": 5,
        "fps": 26.0,
        "factory": "launcher_slash",
        "alias_frame": 3,
        "attach": "player uppercut arc",
    },
    "vfx_magic_bolt": {
        "size": (288, 160),
        "frames": 4,
        "fps": 18.0,
        "factory": "magic_bolt",
        "alias_frame": 2,
        "attach": "player magic projectile",
    },
    "vfx_ally_support": {
        "size": (256, 320),
        "frames": 5,
        "fps": 18.0,
        "factory": "ally_support",
        "alias_frame": 2,
        "attach": "support/break-limit target burst",
    },
    "vfx_enemy_claw": {
        "size": (256, 192),
        "frames": 3,
        "fps": 18.0,
        "factory": "enemy_claw",
        "alias_frame": 1,
        "attach": "grunt and boss melee fallback",
    },
    "vfx_boss_bear_charge": {
        "size": (320, 192),
        "frames": 3,
        "fps": 16.0,
        "factory": "boss_charge",
        "alias_frame": 1,
        "attach": "boss charge dust",
    },
    "vfx_boss_bear_shockwave": {
        "size": (384, 192),
        "frames": 4,
        "fps": 16.0,
        "factory": "boss_shockwave",
        "alias_frame": 2,
        "attach": "boss ground shockwave",
    },
}


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def canvas(size: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image, "RGBA")


def build_pixel_frame(size: tuple[int, int],
                      painter: Callable[[ImageDraw.ImageDraw, tuple[int, int]], None]) -> Image.Image:
    base_size = (size[0] // 2, size[1] // 2)
    image, draw = canvas(base_size)
    painter(draw, base_size)
    return image.resize(size, Image.Resampling.NEAREST)


def rgba(color: tuple[int, int, int, int], alpha_scale: float = 1.0) -> tuple[int, int, int, int]:
    return (color[0], color[1], color[2], max(0, min(255, int(color[3] * alpha_scale))))


def arc_point(cx: float, cy: float, rx: float, ry: float, degrees: float) -> tuple[int, int]:
    rad = math.radians(degrees)
    return (int(round(cx + math.cos(rad) * rx)), int(round(cy + math.sin(rad) * ry)))


def arc_points(cx: float,
               cy: float,
               rx: float,
               ry: float,
               start: float,
               end: float,
               count: int) -> list[tuple[int, int]]:
    if count <= 1:
        return [arc_point(cx, cy, rx, ry, start)]
    return [arc_point(cx, cy, rx, ry, start + (end - start) * i / (count - 1)) for i in range(count)]


def neon_line(draw: ImageDraw.ImageDraw,
              points: list[tuple[int, int]],
              width: int,
              main: tuple[int, int, int, int],
              alpha_scale: float = 1.0,
              warm_core: bool = False) -> None:
    if len(points) < 2:
        return
    draw.line(points, fill=rgba(C["outline"], alpha_scale), width=width + 8, joint="curve")
    draw.line(points, fill=rgba(C["silver"], alpha_scale), width=width + 4, joint="curve")
    draw.line(points, fill=rgba(main, alpha_scale), width=width, joint="curve")
    core_color = C["gold"] if warm_core else C["white"]
    draw.line(points, fill=rgba(core_color, min(1.0, alpha_scale * 0.95)), width=max(1, width // 3), joint="curve")


def neon_arc(draw: ImageDraw.ImageDraw,
             bbox: tuple[int, int, int, int],
             start: float,
             end: float,
             width: int,
             alpha_scale: float,
             warm_core: bool = False) -> None:
    draw.arc(bbox, start, end, fill=rgba(C["outline"], alpha_scale), width=width + 8)
    draw.arc(bbox, start, end, fill=rgba(C["silver"], alpha_scale), width=width + 4)
    draw.arc(bbox, start, end, fill=rgba(C["cyan"], alpha_scale), width=width)
    draw.arc(bbox, start, end, fill=rgba(C["gold"] if warm_core else C["white"], alpha_scale), width=max(1, width // 3))


def shard(draw: ImageDraw.ImageDraw,
          center: tuple[int, int],
          length: int,
          angle: float,
          color: tuple[int, int, int, int],
          width: int = 2,
          alpha_scale: float = 1.0) -> None:
    x, y = center
    dx = math.cos(angle) * length
    dy = math.sin(angle) * length
    px = math.cos(angle + math.pi * 0.5) * width
    py = math.sin(angle + math.pi * 0.5) * width
    pts = [
        (int(x - px), int(y - py)),
        (int(x + dx), int(y + dy)),
        (int(x + px), int(y + py)),
    ]
    draw.polygon(pts, fill=rgba(C["outline"], alpha_scale * 0.85))
    inner = [
        (int(x - px * 0.45), int(y - py * 0.45)),
        (int(x + dx * 0.84), int(y + dy * 0.84)),
        (int(x + px * 0.45), int(y + py * 0.45)),
    ]
    draw.polygon(inner, fill=rgba(color, alpha_scale))


def spark_burst(draw: ImageDraw.ImageDraw,
                center: tuple[int, int],
                radius: int,
                seed: int,
                count: int,
                alpha_scale: float = 1.0,
                warm: bool = False) -> None:
    rng = random.Random(seed)
    palette = [C["cyan"], C["cyan_hi"], C["silver_hi"], C["gold"] if warm else C["white"]]
    cx, cy = center
    for i in range(count):
        angle = rng.uniform(-math.pi, math.pi)
        length = rng.randint(max(4, radius // 3), radius)
        sx = int(cx + math.cos(angle) * rng.randint(2, max(3, radius // 4)))
        sy = int(cy + math.sin(angle) * rng.randint(2, max(3, radius // 4)))
        shard(draw, (sx, sy), length, angle, palette[i % len(palette)], rng.randint(1, 3), alpha_scale)


def scatter_pixels(draw: ImageDraw.ImageDraw,
                   seed: int,
                   bounds: tuple[int, int, int, int],
                   count: int,
                   colors: list[tuple[int, int, int, int]],
                   alpha_scale: float = 1.0) -> None:
    rng = random.Random(seed)
    x0, y0, x1, y1 = bounds
    for i in range(count):
        x = rng.randint(x0, x1)
        y = rng.randint(y0, y1)
        w = rng.choice([1, 1, 2, 3])
        h = rng.choice([1, 1, 2])
        draw.rectangle((x, y, x + w, y + h), fill=rgba(colors[i % len(colors)], alpha_scale * rng.uniform(0.55, 1.0)))


def basic_slash_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        t = frame / max(1, count - 1)
        cx, cy = w * 0.46, h * 0.53
        rx, ry = w * 0.36, h * 0.28
        starts = [210, 204, 195, 215]
        ends = [296, 340, 358, 352]
        widths = [5, 8, 11, 6]
        alphas = [0.58, 0.90, 1.0, 0.48]
        bbox = (int(cx - rx), int(cy - ry), int(cx + rx), int(cy + ry))
        neon_arc(draw, bbox, starts[frame], ends[frame], widths[frame], alphas[frame], warm_core=frame == 2)
        trail = arc_points(cx - 4, cy + 4, rx * 0.84, ry * 0.76, starts[frame] + 10, ends[frame] - 5, 7)
        neon_line(draw, trail, max(2, widths[frame] // 2), C["cyan_dark"], alphas[frame] * 0.56)
        tip = arc_point(cx, cy, rx, ry, ends[frame])
        if frame in (1, 2):
            spark_burst(draw, tip, 12 + 3 * frame, 7100 + frame, 12, 0.85, warm=frame == 2)
        scatter_pixels(draw, 8100 + frame, (16, 12, w - 18, h - 18), 16 + frame * 4,
                       [C["cyan"], C["cyan_hi"], C["silver"]], alphas[frame] * 0.8)
    return build_pixel_frame(size, paint)


def launcher_slash_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        t = frame / max(1, count - 1)
        base = (w // 2 - 6, h - 22)
        height = int((0.28 + 0.72 * min(1.0, t * 1.45)) * (h - 30))
        lean = int(14 + 14 * t)
        tip_y = max(30, base[1] - height)
        main = [
            (base[0] - 10, base[1]),
            (base[0] + 3 + lean // 2, base[1] - height // 2),
            (base[0] + lean, tip_y),
        ]
        neon_line(draw, main, [4, 7, 10, 8, 4][frame], C["cyan"], [0.45, 0.78, 1.0, 0.72, 0.36][frame])
        arc_bbox = (12, 18 + int(8 * (1.0 - t)), w - 14, h - 8)
        neon_arc(draw, arc_bbox, 238, 324 + 16 * min(1, t), [3, 5, 7, 6, 3][frame],
                 [0.42, 0.78, 0.96, 0.62, 0.32][frame])
        if frame in (2, 3):
            spark_burst(draw, (base[0] + lean, tip_y), 14, 9200 + frame, 14, 0.82)
        scatter_pixels(draw, 9300 + frame, (18, 10, w - 20, h - 16), 12 + frame * 3,
                       [C["cyan"], C["cyan_hi"], C["silver_hi"]], 0.72)
    return build_pixel_frame(size, paint)


def magic_bolt_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        phase = frame / max(1, count - 1)
        cy = h // 2 + int(math.sin(frame * math.tau / count) * 3)
        tip_x = int(w * 0.76 + 4 * phase)
        tail_x = int(w * 0.22)
        core = [(tail_x + 14, cy - 13), (tip_x, cy), (tail_x + 14, cy + 13), (tail_x - 4, cy)]
        draw.polygon([(tail_x - 10, cy), (tail_x + 36, cy - 24), (tail_x + 42, cy + 22)], fill=rgba(C["outline_soft"], 0.68))
        draw.polygon(core, fill=rgba(C["outline"], 0.92))
        draw.polygon([(tail_x + 20, cy - 9), (tip_x - 4, cy), (tail_x + 20, cy + 9), (tail_x + 4, cy)],
                     fill=rgba(C["cyan"], 0.98))
        draw.polygon([(tail_x + 38, cy - 4), (tip_x - 8, cy), (tail_x + 38, cy + 4)], fill=C["white"])
        for offset, alpha in [(0, 0.72), (8, 0.52), (14, 0.32)]:
            neon_line(draw,
                      [(tail_x - 8 - offset, cy - 12 + offset // 5),
                       (tail_x + 32 - offset, cy - 6),
                       (tip_x - 28 - offset // 2, cy - 2)],
                      2,
                      C["cyan"],
                      alpha)
            neon_line(draw,
                      [(tail_x - 4 - offset, cy + 13 - offset // 6),
                       (tail_x + 30 - offset, cy + 7),
                       (tip_x - 22 - offset // 2, cy + 2)],
                      2,
                      C["cyan_dark"],
                      alpha)
        spark_burst(draw, (tip_x - 4, cy), 12, 10100 + frame, 7, 0.78)
        scatter_pixels(draw, 10200 + frame, (6, cy - 30, w - 12, cy + 30), 14,
                       [C["cyan"], C["silver_hi"], C["cyan_dark"]], 0.68)
    return build_pixel_frame(size, paint)


def ally_support_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        t = frame / max(1, count - 1)
        cx, cy = w // 2, h // 2 + 4
        radius = [20, 34, 46, 50, 42][frame]
        alpha = [0.38, 0.68, 1.0, 0.72, 0.36][frame]
        draw.ellipse((cx - radius - 5, cy - radius - 5, cx + radius + 5, cy + radius + 5),
                     outline=rgba(C["outline"], alpha), width=6)
        draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius),
                     outline=rgba(C["cyan"], alpha), width=4)
        draw.ellipse((cx - radius + 8, cy - radius + 8, cx + radius - 8, cy + radius - 8),
                     outline=rgba(C["gold"], alpha * 0.75), width=2)
        neon_line(draw, [(cx, 16), (cx, h - 18)], 3 + (frame == 2) * 3, C["cyan_hi"], alpha * 0.78)
        neon_line(draw, [(cx - 36, cy + 34), (cx, cy - 40), (cx + 38, cy + 34)], 2, C["cyan"], alpha * 0.55)
        if frame in (1, 2, 3):
            spark_burst(draw, (cx, cy), 22 + 8 * frame, 11200 + frame, 16, 0.72, warm=True)
        scatter_pixels(draw, 11300 + frame, (20, 18, w - 20, h - 18), 16 + frame * 2,
                       [C["cyan"], C["cyan_hi"], C["gold"], C["silver_hi"]], alpha)
    return build_pixel_frame(size, paint)


def enemy_claw_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        offsets = [0, 8, 14]
        alpha = [0.68, 1.0, 0.45][frame]
        for i, x in enumerate([66, 80, 94]):
            start = (x + offsets[frame], 13 + i * 2)
            mid = (x - 10 + offsets[frame] // 2, 42 + i * 2)
            end = (x - 40 + offsets[frame], h - 17 + i * 2)
            neon_line(draw, [start, mid, end], 5, C["gold"], alpha, warm_core=True)
            neon_line(draw, [(start[0] - 5, start[1] + 8), (end[0] - 8, end[1] - 4)], 2, C["cyan"], alpha * 0.52)
        if frame == 1:
            spark_burst(draw, (w - 42, h // 2), 12, 12100, 10, 0.82, warm=True)
        scatter_pixels(draw, 12200 + frame, (20, 12, w - 16, h - 14), 12,
                       [C["gold"], C["cyan"], C["silver_hi"]], alpha)
    return build_pixel_frame(size, paint)


def boss_charge_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        ground = h - 24
        alpha = [0.58, 0.92, 0.50][frame]
        for i in range(5):
            y = ground - 4 - i * 8 + frame * 2
            x0 = 12 + i * 12
            x1 = w - 42 - i * 9
            neon_line(draw, [(x0, y), ((x0 + x1) // 2, y - 7), (x1, y - 2)], max(2, 4 - i // 2), C["cyan_dark"], alpha * (0.82 - i * 0.08))
        for i in range(8):
            x = 48 + i * 10 + frame * 5
            y = ground - (i % 3) * 7
            draw.ellipse((x - 8, y - 5, x + 13, y + 5), fill=rgba(C["dust"], alpha * 0.88))
            shard(draw, (x + 5, y - 8), 10 + i % 4, -0.85 - i * 0.11, C["rock"], 2, alpha)
        neon_line(draw, [(30, ground + 2), (w - 28, ground - 1)], 2, C["cyan"], alpha * 0.58)
        scatter_pixels(draw, 13200 + frame, (14, ground - 48, w - 24, ground + 4), 22,
                       [C["dust"], C["rock"], C["cyan"], C["silver"]], alpha)
    return build_pixel_frame(size, paint)


def boss_shockwave_frame(frame: int, count: int, size: tuple[int, int]) -> Image.Image:
    def paint(draw: ImageDraw.ImageDraw, s: tuple[int, int]) -> None:
        w, h = s
        ground = h - 34
        center = (w // 2, ground - 2)
        t = frame / max(1, count - 1)
        radius = [24, 42, 56, 62][frame]
        alpha = [0.52, 0.92, 1.0, 0.44][frame]
        neon_line(draw, [(center[0] - radius - 12, ground), (center[0], ground - 3), (center[0] + radius + 18, ground)],
                  3, C["cyan"], alpha * 0.7)
        left_box = (center[0] - radius - 14, ground - 30 - frame * 2, center[0] + 4, ground + 18)
        right_box = (center[0] - 8, ground - 30 - frame * 2, center[0] + radius + 18, ground + 18)
        neon_arc(draw, left_box, 198, 340, 4 + frame, alpha * 0.72)
        neon_arc(draw, right_box, 200, 342, 4 + frame, alpha * 0.72, warm_core=frame == 1)
        for xoff in [-54, -30, -12, 18, 44, 66]:
            crack = [(center[0] + xoff, ground),
                     (center[0] + xoff + int(10 * math.sin(xoff)), ground - 9 - frame * 2),
                     (center[0] + xoff + 18, ground - 2)]
            neon_line(draw, crack, 2, C["cyan"], alpha * 0.65)
        if frame in (1, 2):
            spark_burst(draw, center, 24 + 5 * frame, 14100 + frame, 18, 0.82, warm=True)
        for i in range(10):
            x = center[0] - radius + i * max(5, radius // 5)
            y = ground - 4 - (i % 4) * (4 + frame)
            shard(draw, (x, y), 8 + (i % 5) * 2, -1.35 + (i % 3) * 0.55, C["rock"], 2, alpha)
        scatter_pixels(draw, 14200 + frame, (16, ground - 58, w - 16, ground + 2), 22,
                       [C["cyan"], C["cyan_hi"], C["rock"], C["dust"]], alpha)
    return build_pixel_frame(size, paint)


FACTORIES: dict[str, Callable[[int, int, tuple[int, int]], Image.Image]] = {
    "basic_slash": basic_slash_frame,
    "launcher_slash": launcher_slash_frame,
    "magic_bolt": magic_bolt_frame,
    "ally_support": ally_support_frame,
    "enemy_claw": enemy_claw_frame,
    "boss_charge": boss_charge_frame,
    "boss_shockwave": boss_shockwave_frame,
}


def save_strip(frames: list[Image.Image], path: Path) -> None:
    w, h = frames[0].size
    strip = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        strip.alpha_composite(frame, (index * w, 0))
    save(strip, path)


def checker_image(size: tuple[int, int]) -> Image.Image:
    w, h = size
    image = Image.new("RGBA", size, (22, 24, 29, 255))
    draw = ImageDraw.Draw(image, "RGBA")
    tile = 16
    for y in range(0, h, tile):
        for x in range(0, w, tile):
            fill = (35, 37, 45, 255) if ((x // tile + y // tile) % 2 == 0) else (25, 27, 34, 255)
            draw.rectangle((x, y, x + tile - 1, y + tile - 1), fill=fill)
    return image


def checker_preview(frames: list[Image.Image], path: Path) -> Image.Image:
    w, h = frames[0].size
    preview = checker_image((w * len(frames), h))
    draw = ImageDraw.Draw(preview, "RGBA")
    for index, frame in enumerate(frames):
        preview.alpha_composite(frame, (index * w, 0))
        draw.rectangle((index * w, 0, (index + 1) * w - 1, h - 1), outline=(105, 112, 130, 150), width=2)
    scale = min(1.0, 1600 / max(1, preview.width))
    if scale < 1.0:
        preview = preview.resize((int(preview.width * scale), int(preview.height * scale)), Image.Resampling.NEAREST)
    save(preview, path)
    return preview


def combined_preview(rows: list[tuple[str, Image.Image]]) -> None:
    if not rows:
        return
    label_w = 260
    gap = 12
    max_w = max(label_w + image.width for _, image in rows)
    total_h = sum(image.height for _, image in rows) + gap * (len(rows) - 1)
    contact = Image.new("RGBA", (max_w, total_h), (16, 18, 24, 255))
    draw = ImageDraw.Draw(contact, "RGBA")
    y = 0
    for name, image in rows:
        row_bg = checker_image((max_w, image.height))
        contact.alpha_composite(row_bg, (0, y))
        contact.alpha_composite(image, (label_w, y))
        draw.text((16, y + 14), name, fill=(210, 232, 238, 255))
        y += image.height + gap
    save(contact, PREVIEW_ROOT / "batch06_runtime_vfx_contact_sheet.png")


def backup_existing() -> int:
    if BACKUP_ROOT.exists():
        return 0
    BACKUP_ROOT.mkdir(parents=True, exist_ok=True)
    count = 0
    for path in sorted(EFFECT_ROOT.glob("*.png")):
        shutil.copy2(path, BACKUP_ROOT / path.name)
        count += 1
    return count


def validate(frames: list[Image.Image], expected: tuple[int, int], name: str) -> None:
    for index, image in enumerate(frames):
        if image.mode != "RGBA" or image.size != expected:
            raise ValueError(f"{name}_{index:02}: invalid image {image.mode} {image.size}, expected RGBA {expected}")
        alpha = image.getchannel("A")
        bbox = alpha.getbbox()
        if bbox is None:
            raise ValueError(f"{name}_{index:02}: empty alpha")
        corners = [
            alpha.getpixel((0, 0)),
            alpha.getpixel((image.width - 1, 0)),
            alpha.getpixel((0, image.height - 1)),
            alpha.getpixel((image.width - 1, image.height - 1)),
        ]
        if corners != [0, 0, 0, 0]:
            raise ValueError(f"{name}_{index:02}: non-transparent corner {corners}")
        if bbox[0] <= 1 or bbox[1] <= 1 or bbox[2] >= image.width - 1 or bbox[3] >= image.height - 1:
            raise ValueError(f"{name}_{index:02}: alpha touches edge {bbox}")


def write_params() -> None:
    lines = [
        "productionMode: programmatic_batch06_runtime_vfx",
        "generatedWithAi: false",
        "purpose: current SideCombat runtime VFX replacement",
        "transparentPngRule:",
        "  requireRgba: true",
        "  unusedPixelsAlpha: 0",
        "  noBakedBackground: true",
        "  noWhiteMatte: true",
        "frameIntegrity:",
        "  sourceFramesFirst: true",
        "  fixedCanvasPerClip: true",
        "  noCrossCellBleed: true",
        "  collisionIndependentFromVisualCanvas: true",
        "",
        "clips:",
    ]
    for name, spec in CLIPS.items():
        w, h = spec["size"]
        frames = int(spec["frames"])
        fps = float(spec["fps"])
        lines += [
            f"  {name}:",
            f"    cellWidth: {w}",
            f"    cellHeight: {h}",
            f"    frameCount: {frames}",
            f"    frameRate: {fps}",
            f"    pivotX: {w // 2}",
            f"    pivotY: {h // 2}",
            f"    attach: {spec['attach']}",
            f"    runtimePattern: assets/vertical_slice/side_combat/effects/{name}_{{frame2}}.png",
            f"    sourceFrames: WheatearEditor/assets/vertical_slice/source_frames/batch06_runtime_vfx/{name}/{name}_{{frame:03}}.png",
            f"    stripOutput: WheatearEditor/assets/vertical_slice/side_combat/vfx_sheets/{name}_strip.png",
        ]
    save(Image.new("RGBA", (1, 1), (0, 0, 0, 0)), SHEET_ROOT / ".keep.png")
    (SHEET_ROOT / "batch06_runtime_vfx_params.yaml").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (SHEET_ROOT / ".keep.png").unlink(missing_ok=True)


def main() -> None:
    SOURCE_ROOT.mkdir(parents=True, exist_ok=True)
    EFFECT_ROOT.mkdir(parents=True, exist_ok=True)
    SHEET_ROOT.mkdir(parents=True, exist_ok=True)
    PREVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    copied = backup_existing()
    preview_rows: list[tuple[str, Image.Image]] = []
    runtime_count = 0

    for name, spec in CLIPS.items():
        size = spec["size"]
        frame_count = int(spec["frames"])
        factory = FACTORIES[str(spec["factory"])]
        frames = [factory(index, frame_count, size) for index in range(frame_count)]
        validate(frames, size, name)

        source_dir = SOURCE_ROOT / name
        source_dir.mkdir(parents=True, exist_ok=True)
        for index, frame in enumerate(frames):
            save(frame, source_dir / f"{name}_{index:03}.png")
            save(frame, EFFECT_ROOT / f"{name}_{index + 1:02}.png")
            runtime_count += 1

        alias_index = min(frame_count - 1, max(0, int(spec["alias_frame"])))
        save(frames[alias_index], EFFECT_ROOT / f"{name}.png")
        save_strip(frames, SHEET_ROOT / f"{name}_strip.png")
        preview = checker_preview(frames, PREVIEW_ROOT / f"{name}_preview.png")
        preview_rows.append((name, preview))

    # Single-file fallback used by older enemy projectile/shockwave paths.
    fallback = EFFECT_ROOT / "vfx_boss_bear_shockwave_03.png"
    if fallback.exists():
        shutil.copy2(fallback, EFFECT_ROOT / "vfx_enemy_projectile.png")

    write_params()
    combined_preview(preview_rows)
    print(f"Backed up old effect PNGs: {copied}")
    print(f"Runtime VFX frames written: {runtime_count}")
    print(f"Runtime effect root: {EFFECT_ROOT}")
    print(f"Preview: {PREVIEW_ROOT / 'batch06_runtime_vfx_contact_sheet.png'}")


if __name__ == "__main__":
    main()
