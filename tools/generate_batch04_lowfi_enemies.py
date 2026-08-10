from __future__ import annotations

import math
import shutil
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"
SOURCE_ROOT = ASSET_ROOT / "source_frames" / "batch04_lowfi"
DEST_ROOT = ASSET_ROOT / "side_combat" / "enemies"
SHEET_ROOT = ASSET_ROOT / "side_combat" / "sheets"
PREVIEW_ROOT = ASSET_ROOT / "side_combat" / "previews" / "batch04_lowfi"
BACKUP_ROOT = DEST_ROOT / "_backup_before_batch04_lowfi"


PROFILES = {
    "small_enemy": {
        "size": (512, 384),
        "pivot": (256, 306),
        "renderScale": (1.0, 1.0),
        "renderOffset": (0.0, 0.2969),
        "safe": (56, 42, 56, 42),
    },
    "small_enemy_wide": {
        "size": (768, 384),
        "pivot": (384, 306),
        "renderScale": (1.5, 1.0),
        "renderOffset": (0.0, 0.2969),
        "safe": (96, 42, 112, 42),
    },
    "small_enemy_air": {
        "size": (512, 512),
        "pivot": (256, 390),
        "renderScale": (1.0, 1.3333),
        "renderOffset": (0.0, 0.3516),
        "safe": (56, 58, 56, 64),
    },
    "small_enemy_air_wide": {
        "size": (768, 512),
        "pivot": (384, 390),
        "renderScale": (1.5, 1.3333),
        "renderOffset": (0.0, 0.3516),
        "safe": (96, 58, 112, 64),
    },
    "boss_bear_husband": {
        "size": (1024, 768),
        "pivot": (512, 626),
        "renderScale": (1.0, 1.0),
        "renderOffset": (0.0, 0.3151),
        "safe": (112, 72, 112, 72),
    },
    "boss_bear_husband_wide": {
        "size": (1280, 768),
        "pivot": (640, 626),
        "renderScale": (1.25, 1.0),
        "renderOffset": (0.0, 0.3151),
        "safe": (160, 72, 176, 72),
    },
}


CLIPS = [
    ("en_claw_beast", "idle", 4, 7.0, "small_enemy"),
    ("en_claw_beast", "run", 5, 11.0, "small_enemy"),
    ("en_claw_beast", "hit", 3, 12.0, "small_enemy_wide"),
    ("en_claw_beast", "fall", 3, 9.0, "small_enemy_air_wide"),
    ("en_claw_beast", "dead", 4, 7.0, "small_enemy_wide"),
    ("en_claw_beast", "attack", 4, 14.0, "small_enemy_wide"),
    ("boss_bear_husband", "idle", 4, 6.0, "boss_bear_husband"),
    ("boss_bear_husband", "walk", 5, 8.0, "boss_bear_husband"),
    ("boss_bear_husband", "hit", 3, 10.0, "boss_bear_husband"),
    ("boss_bear_husband", "fall", 3, 8.0, "boss_bear_husband"),
    ("boss_bear_husband", "dead", 4, 7.0, "boss_bear_husband_wide"),
    ("boss_bear_husband", "attack", 4, 12.0, "boss_bear_husband_wide"),
    ("boss_bear_husband", "charge", 4, 12.0, "boss_bear_husband"),
    ("boss_bear_husband", "shockwave", 4, 12.0, "boss_bear_husband_wide"),
]


C = {
    "outline": (8, 12, 16, 255),
    "outline_soft": (18, 24, 28, 255),
    "fur_dark": (28, 38, 34, 255),
    "fur_mid": (54, 70, 58, 255),
    "fur_hi": (91, 108, 90, 255),
    "beast_skin": (72, 83, 74, 255),
    "bear_dark": (24, 20, 28, 255),
    "bear_mid": (54, 43, 56, 255),
    "bear_hi": (94, 76, 84, 255),
    "claw": (216, 222, 205, 255),
    "cyan": (39, 225, 238, 255),
    "cyan_soft": (39, 225, 238, 105),
    "eye": (160, 250, 255, 255),
}


def poly(draw: ImageDraw.ImageDraw, pts, fill, outline=True) -> None:
    if outline:
        draw.line(pts + [pts[0]], fill=C["outline"], width=8, joint="curve")
    draw.polygon(pts, fill=fill)


def ellipse(draw: ImageDraw.ImageDraw, bbox, fill, outline=True, width=5) -> None:
    if outline:
        x0, y0, x1, y1 = bbox
        draw.ellipse((x0 - width, y0 - width, x1 + width, y1 + width), fill=C["outline"])
    draw.ellipse(bbox, fill=fill)


def line(draw: ImageDraw.ImageDraw, pts, fill, width, outline=True) -> None:
    if outline:
        draw.line(pts, fill=C["outline"], width=width + 8, joint="curve")
    draw.line(pts, fill=fill, width=width, joint="curve")


def claw(draw: ImageDraw.ImageDraw, root: tuple[int, int], tip: tuple[int, int], width=10) -> None:
    rx, ry = root
    tx, ty = tip
    dx, dy = tx - rx, ty - ry
    length = max(1.0, math.hypot(dx, dy))
    nx, ny = -dy / length, dx / length
    pts = [
        (rx + nx * width, ry + ny * width),
        (tx, ty),
        (rx - nx * width, ry - ny * width),
    ]
    poly(draw, pts, C["claw"], True)


def vein(draw: ImageDraw.ImageDraw, pts, width=4, alpha=160) -> None:
    draw.line(pts, fill=(39, 225, 238, alpha), width=width + 6, joint="curve")
    draw.line(pts, fill=C["cyan"], width=width, joint="curve")


def draw_small_beast(draw: ImageDraw.ImageDraw, profile: str, action: str, frame: int, count: int) -> None:
    px, py = PROFILES[profile]["pivot"]
    t = frame / max(1, count - 1)
    phase = frame * math.tau / max(1, count)
    x = px
    ground = py
    body_y = ground - 128
    lean = 0
    squash = 0
    airborne = 0
    stretch = 0

    if action == "idle":
        body_y += int(math.sin(phase) * 3)
        lean = int(math.sin(phase) * 4)
    elif action == "run":
        lean = 20
        body_y += int(math.sin(phase) * 5)
        stretch = int(math.cos(phase) * 14)
    elif action == "hit":
        lean = [-8, -32, -14][frame]
        body_y += [0, -8, -4][frame]
    elif action == "fall":
        airborne = [78, 52, 24][frame]
        lean = [-32, -18, -8][frame]
        body_y -= airborne
        stretch = [18, 10, 0][frame]
    elif action == "dead":
        body_y = ground - 54
        lean = [-20, 0, 14, 24][frame]
        squash = 34
    elif action == "attack":
        lean = [-20, -8, 40, 30][frame]
        body_y += [4, -6, 0, 8][frame]
        stretch = [0, 0, 42, 20][frame]

    if action == "dead":
        body = [(x - 150, body_y - 10), (x + 72, body_y - 18), (x + 126, body_y + 24), (x - 118, body_y + 44)]
        poly(draw, body, C["fur_dark"])
        ellipse(draw, (x + 92, body_y - 10, x + 142, body_y + 36), C["fur_mid"])
        line(draw, [(x - 106, body_y + 4), (x - 178, body_y + 32), (x - 218, body_y + 22)], C["fur_mid"], 18)
        for ox in [-56, -10, 48, 90]:
            line(draw, [(x + ox, body_y + 28), (x + ox + 40, body_y + 58)], C["fur_mid"], 14)
        vein(draw, [(x - 64, body_y + 0), (x - 12, body_y + 8), (x + 42, body_y + 6)], 3, 90)
        return

    body = [
        (x - 116 + lean - stretch, body_y + 28 + squash),
        (x - 72 + lean, body_y - 22),
        (x + 40 + lean + stretch, body_y - 24),
        (x + 116 + lean + stretch, body_y + 18),
        (x + 74 + lean, body_y + 58 + squash),
        (x - 92 + lean - stretch, body_y + 60 + squash),
    ]
    poly(draw, body, C["fur_dark"])
    poly(draw, [(x - 50 + lean, body_y - 8), (x + 40 + lean, body_y - 12), (x + 88 + lean, body_y + 20), (x + 18 + lean, body_y + 34)], C["fur_mid"], False)
    vein(draw, [(x - 42 + lean, body_y + 6), (x + 8 + lean, body_y + 12), (x + 54 + lean, body_y + 8)], 3, 110)

    head = (x + 118 + lean + stretch // 2, body_y + 2)
    ellipse(draw, (head[0] - 42, head[1] - 32, head[0] + 30, head[1] + 30), C["fur_mid"])
    poly(draw, [(head[0] + 10, head[1] - 18), (head[0] + 54, head[1] - 2), (head[0] + 12, head[1] + 16)], C["fur_mid"])
    poly(draw, [(head[0] - 16, head[1] - 28), (head[0] - 4, head[1] - 68), (head[0] + 12, head[1] - 24)], C["fur_dark"])
    ellipse(draw, (head[0] + 12, head[1] - 4, head[0] + 22, head[1] + 6), C["eye"], False)

    tail_base = (x - 108 + lean - stretch, body_y + 34)
    line(draw, [tail_base, (tail_base[0] - 70, tail_base[1] - 28), (tail_base[0] - 120, tail_base[1] - 10)], C["fur_mid"], 18)

    leg_offsets = [(-70, 44), (-20, 48), (42, 48), (86, 44)]
    for idx, (lx, ly) in enumerate(leg_offsets):
        stride = 0
        lift = 0
        if action == "run":
            stride = int(math.sin(phase + idx * 1.7) * 36)
            lift = max(0, int(math.cos(phase + idx * 1.7) * 18))
        if action == "attack" and idx >= 2:
            stride += [0, 18, 74, 42][frame]
            lift -= [0, 8, 0, 4][frame]
        knee = (x + lean + lx + stride // 2, body_y + ly + 34 - lift)
        foot = (x + lean + lx + stride, ground - lift)
        line(draw, [(x + lean + lx, body_y + ly), knee, foot], C["fur_mid"], 14)
        if idx >= 2:
            claw(draw, foot, (foot[0] + 26, foot[1] + 4), 5)

    if action == "attack" and frame in {1, 2}:
        arc_y = body_y + 24
        draw.line([(x + 80, arc_y - 38), (x + 180, arc_y - 20), (x + 268, arc_y + 10)],
                  fill=C["cyan_soft"], width=26, joint="curve")
        draw.line([(x + 94, arc_y - 30), (x + 192, arc_y - 12), (x + 278, arc_y + 18)],
                  fill=C["cyan"], width=5, joint="curve")


def draw_boss(draw: ImageDraw.ImageDraw, profile: str, action: str, frame: int, count: int) -> None:
    px, py = PROFILES[profile]["pivot"]
    t = frame / max(1, count - 1)
    phase = frame * math.tau / max(1, count)
    x = px
    ground = py
    y = ground - 345
    lean = 0
    crouch = 0
    forward = 0
    if action == "idle":
        y += int(math.sin(phase) * 5)
    elif action == "walk":
        forward = int(math.sin(phase) * 18)
        y += int(abs(math.cos(phase)) * 4)
    elif action == "hit":
        lean = [-10, -42, -18][frame]
        y += [0, -6, -3][frame]
    elif action == "fall":
        lean = [-16, -30, -22][frame]
        y += [-16, -8, 8][frame]
        crouch = [0, 20, 34][frame]
    elif action == "charge":
        lean = [0, 26, 48, 36][frame]
        crouch = [28, 46, 36, 32][frame]
        forward = [0, 10, 26, 42][frame]
    elif action == "attack":
        lean = [-34, -20, 54, 36][frame]
        crouch = [8, 26, 6, 20][frame]
        forward = [0, 0, 72, 36][frame]
    elif action == "shockwave":
        lean = [-10, 4, 22, 10][frame]
        crouch = [0, -30, 50, 32][frame]
    elif action == "dead":
        y = ground - 115
        body = [(x - 330, y - 28), (x + 210, y - 54), (x + 330, y + 18), (x + 260, y + 92), (x - 250, y + 88)]
        poly(draw, body, C["bear_dark"])
        ellipse(draw, (x + 232, y - 58, x + 376, y + 62), C["bear_mid"])
        line(draw, [(x - 245, y + 28), (x - 342, y + 78)], C["bear_mid"], 40)
        line(draw, [(x + 110, y + 52), (x + 258, y + 102)], C["bear_mid"], 44)
        vein(draw, [(x - 120, y - 16), (x - 20, y + 6), (x + 130, y - 6)], 6, 100)
        return

    shoulder = (x - 20 + lean + forward, y + 52 + crouch)
    hip = (x - 150 + lean // 2, y + 240 + crouch)
    body = [
        (x - 300 + lean // 3, y + 170 + crouch),
        (x - 210 + lean, y + 24 + crouch),
        (x + 70 + lean + forward, y + 2 + crouch),
        (x + 245 + lean + forward, y + 112 + crouch),
        (x + 160 + lean, y + 308 + crouch),
        (x - 232 + lean // 3, y + 318 + crouch),
    ]
    poly(draw, body, C["bear_dark"])
    poly(draw, [(x - 120 + lean, y + 54 + crouch), (x + 78 + lean + forward, y + 42 + crouch), (x + 166 + lean, y + 170 + crouch), (x - 36 + lean, y + 190 + crouch)], C["bear_mid"], False)
    vein(draw, [(x - 80 + lean, y + 84 + crouch), (x + 20 + lean, y + 112 + crouch), (x + 108 + lean, y + 86 + crouch)], 6, 130)

    head = (x + 220 + lean + forward, y + 78 + crouch)
    ellipse(draw, (head[0] - 96, head[1] - 72, head[0] + 72, head[1] + 76), C["bear_mid"])
    poly(draw, [(head[0] + 34, head[1] - 26), (head[0] + 130, head[1] + 8), (head[0] + 36, head[1] + 46)], C["bear_mid"])
    poly(draw, [(head[0] - 58, head[1] - 58), (head[0] - 42, head[1] - 132), (head[0] + 2, head[1] - 64)], C["bear_dark"])
    ellipse(draw, (head[0] + 12, head[1] - 20, head[0] + 30, head[1] - 4), C["eye"], False)
    if action == "shockwave" and frame == 2:
        draw.line([(x - 260, ground - 8), (x + 310, ground - 10)], fill=C["cyan_soft"], width=30)
        draw.line([(x - 260, ground - 8), (x + 310, ground - 10)], fill=C["cyan"], width=5)

    paw_pairs = [
        ((x - 180, y + 292 + crouch), (x - 245 + forward // 3, ground)),
        ((x + 80 + lean, y + 286 + crouch), (x + 92 + forward, ground)),
        ((x - 10 + lean, y + 176 + crouch), (x + 154 + forward, ground - 18)),
    ]
    if action == "attack":
        paw_pairs[2] = ((x + 8 + lean, y + 154 + crouch), (x + 410 + forward, ground - 78))
    if action == "charge":
        paw_pairs[2] = ((x + 0 + lean, y + 172 + crouch), (x + 240 + forward, ground - 10))
    for root, tip in paw_pairs:
        line(draw, [root, ((root[0] + tip[0]) // 2, (root[1] + tip[1]) // 2 + 44), tip], C["bear_mid"], 46)
        claw(draw, (tip[0] + 10, tip[1] - 2), (tip[0] + 62, tip[1] + 8), 14)
        claw(draw, (tip[0] - 8, tip[1] - 2), (tip[0] + 40, tip[1] + 18), 12)

    if action == "attack" and frame in {1, 2}:
        draw.line([(x + 180, ground - 300), (x + 390, ground - 220), (x + 560, ground - 126)],
                  fill=C["cyan_soft"], width=42, joint="curve")
        draw.line([(x + 195, ground - 292), (x + 410, ground - 210), (x + 572, ground - 118)],
                  fill=C["cyan"], width=8, joint="curve")


def draw_frame(entity: str, action: str, frame: int, count: int, profile: str) -> Image.Image:
    w, h = PROFILES[profile]["size"]
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")
    if entity == "en_claw_beast":
        draw_small_beast(draw, profile, action, frame, count)
    else:
        draw_boss(draw, profile, action, frame, count)
    return image


def save_strip(frames: list[Image.Image], path: Path) -> None:
    w, h = frames[0].size
    strip = Image.new("RGBA", (w * len(frames), h), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        strip.alpha_composite(frame, (w * i, 0))
    strip.save(path)


def checker_preview(frames: list[Image.Image], baseline: int, path: Path) -> None:
    w, h = frames[0].size
    total = w * len(frames)
    bg = Image.new("RGBA", (total, h), (22, 24, 29, 255))
    draw = ImageDraw.Draw(bg, "RGBA")
    tile = 32
    for yy in range(0, h, tile):
        for xx in range(0, total, tile):
            if (xx // tile + yy // tile) % 2 == 0:
                draw.rectangle((xx, yy, xx + tile - 1, yy + tile - 1), fill=(34, 36, 43, 255))
    for i, frame in enumerate(frames):
        bg.alpha_composite(frame, (w * i, 0))
        draw.rectangle((w * i, 0, w * (i + 1) - 1, h - 1), outline=(96, 101, 116, 150), width=2)
        draw.line((w * i, baseline, w * (i + 1), baseline), fill=(255, 210, 80, 170), width=2)
    scale = min(1.0, 1800 / bg.width)
    if scale < 1.0:
        bg = bg.resize((int(bg.width * scale), int(bg.height * scale)), Image.Resampling.NEAREST)
    bg.save(path)


def backup_existing() -> int:
    if BACKUP_ROOT.exists():
        return 0
    BACKUP_ROOT.mkdir(parents=True, exist_ok=True)
    count = 0
    for path in sorted(DEST_ROOT.glob("*.png")):
        shutil.copy2(path, BACKUP_ROOT / path.name)
        count += 1
    return count


def validate(frames: list[Image.Image], profile: str) -> None:
    expected = PROFILES[profile]["size"]
    for image in frames:
        if image.mode != "RGBA" or image.size != expected:
            raise ValueError(f"invalid image {image.mode} {image.size}, expected RGBA {expected}")
        alpha = image.getchannel("A")
        if alpha.getbbox() is None:
            raise ValueError("empty alpha")
        corners = [
            alpha.getpixel((0, 0)),
            alpha.getpixel((image.width - 1, 0)),
            alpha.getpixel((0, image.height - 1)),
            alpha.getpixel((image.width - 1, image.height - 1)),
        ]
        if corners != [0, 0, 0, 0]:
            raise ValueError(f"non-transparent corner {corners}")
        bbox = alpha.getbbox()
        if bbox[0] <= 0 or bbox[1] <= 0 or bbox[2] >= image.width or bbox[3] >= image.height:
            raise ValueError(f"alpha touches edge {bbox}")


def write_params() -> None:
    lines = [
        "productionMode: programmatic_lowfi_validation",
        "generatedWithAi: false",
        "purpose: engineering validation placeholder, not final art",
        "transparentPngRule:",
        "  requireRgba: true",
        "  unusedPixelsAlpha: 0",
        "  noBakedShadow: true",
        "",
        "profiles:",
    ]
    for name, profile in PROFILES.items():
        w, h = profile["size"]
        px, py = profile["pivot"]
        sx, sy = profile["renderScale"]
        ox, oy = profile["renderOffset"]
        lines += [
            f"  {name}:",
            f"    cellWidth: {w}",
            f"    cellHeight: {h}",
            "    pivot: bottom_center",
            f"    pivotX: {px}",
            f"    pivotY: {py}",
            f"    baselineY: {py}",
            f"    renderScale: [{sx}, {sy}]",
            f"    renderOffset: [{ox}, {oy}]",
            f"    safePadding: {list(profile['safe'])}",
        ]
    lines += ["", "clips:"]
    for entity, action, count, fps, profile in CLIPS:
        w, h = PROFILES[profile]["size"]
        lines += [
            f"  {entity}_{action}:",
            f"    frameCount: {count}",
            f"    frameRate: {fps}",
            f"    profile: {profile}",
            f"    cellWidth: {w}",
            f"    cellHeight: {h}",
            f"    runtimePattern: assets/vertical_slice/side_combat/enemies/{entity}_{action}_{{frame2}}.png",
            "    fixedCanvasPerClip: true",
            "    pivotBaselineStable: true",
            "    collisionIndependentFromCanvas: true",
        ]
    (SHEET_ROOT / "batch04_lowfi_sheet_params.yaml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    SOURCE_ROOT.mkdir(parents=True, exist_ok=True)
    DEST_ROOT.mkdir(parents=True, exist_ok=True)
    SHEET_ROOT.mkdir(parents=True, exist_ok=True)
    PREVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    copied = backup_existing()
    all_written: list[Path] = []
    for entity, action, count, fps, profile in CLIPS:
        source_dir = SOURCE_ROOT / f"{entity}_{action}"
        source_dir.mkdir(parents=True, exist_ok=True)
        frames = [draw_frame(entity, action, index, count, profile) for index in range(count)]
        validate(frames, profile)
        for index, frame in enumerate(frames):
            source_path = source_dir / f"{entity}_{action}_{index:03}.png"
            runtime_path = DEST_ROOT / f"{entity}_{action}_{index + 1:02}.png"
            frame.save(source_path)
            frame.save(runtime_path)
            all_written.append(runtime_path)
        save_strip(frames, SHEET_ROOT / f"{entity}_{action}_lowfi_strip.png")
        checker_preview(frames, PROFILES[profile]["pivot"][1], PREVIEW_ROOT / f"{entity}_{action}_preview.png")

    write_params()
    print(f"Backed up old enemy PNGs: {copied}")
    print(f"Runtime enemy frames written: {len(all_written)}")
    print("Runtime canvases: per-enemy profile sizes; no downscale bake")
    print(f"Preview: {PREVIEW_ROOT}")


if __name__ == "__main__":
    main()
