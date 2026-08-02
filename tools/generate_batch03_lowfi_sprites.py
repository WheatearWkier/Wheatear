from __future__ import annotations

import math
from pathlib import Path
from typing import Callable

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"
SOURCE_ROOT = ASSET_ROOT / "source_frames"
SHEET_ROOT = ASSET_ROOT / "side_combat" / "sheets"
PREVIEW_ROOT = ASSET_ROOT / "side_combat" / "previews" / "protag_batch03_lowfi"


COLORS = {
    "outline": (8, 10, 18, 255),
    "outline2": (20, 24, 36, 255),
    "hair": (34, 37, 50, 255),
    "hair_hi": (86, 92, 112, 255),
    "skin": (230, 172, 139, 255),
    "skin_shadow": (174, 104, 87, 255),
    "jacket": (24, 37, 70, 255),
    "jacket_hi": (65, 78, 112, 255),
    "shirt": (225, 224, 220, 255),
    "pants": (36, 39, 47, 255),
    "pants_hi": (70, 72, 82, 255),
    "boot": (22, 24, 31, 255),
    "boot_hi": (92, 96, 111, 255),
    "metal": (190, 196, 207, 255),
    "metal_hi": (245, 248, 255, 255),
    "cyan": (34, 218, 238, 255),
    "cyan_soft": (34, 218, 238, 100),
    "cyan_faint": (34, 218, 238, 48),
    "gold": (235, 198, 105, 255),
    "shadow": (0, 0, 0, 70),
}


PROFILES = {
    "body_512": {
        "logical": (256, 256),
        "final": (512, 512),
        "pivot": (128, 215),
        "baseline": 215,
        "safe": [64, 48, 64, 40],
    },
    "body_tall_640": {
        "logical": (256, 320),
        "final": (512, 640),
        "pivot": (128, 274),
        "baseline": 274,
        "safe": [64, 72, 64, 40],
    },
    "body_tall_768": {
        "logical": (256, 384),
        "final": (512, 768),
        "pivot": (128, 340),
        "baseline": 340,
        "safe": [64, 96, 64, 48],
    },
    "slash_640": {
        "logical": (320, 256),
        "final": (640, 512),
        "pivot": (160, 215),
        "baseline": 215,
        "safe": [72, 48, 96, 40],
    },
    "slash_heavy_768": {
        "logical": (384, 256),
        "final": (768, 512),
        "pivot": (192, 215),
        "baseline": 215,
        "safe": [96, 48, 120, 40],
    },
    "vertical_640": {
        "logical": (320, 320),
        "final": (640, 640),
        "pivot": (160, 274),
        "baseline": 274,
        "safe": [72, 72, 96, 40],
    },
    "vertical_tall_768": {
        "logical": (320, 384),
        "final": (640, 768),
        "pivot": (160, 340),
        "baseline": 340,
        "safe": [72, 96, 96, 48],
    },
    "dash_768": {
        "logical": (384, 256),
        "final": (768, 512),
        "pivot": (192, 215),
        "baseline": 215,
        "safe": [96, 48, 120, 40],
    },
    "dash_tall_768": {
        "logical": (384, 320),
        "final": (768, 640),
        "pivot": (192, 274),
        "baseline": 274,
        "safe": [96, 72, 120, 40],
    },
    "dash_1024": {
        "logical": (512, 256),
        "final": (1024, 512),
        "pivot": (256, 215),
        "baseline": 215,
        "safe": [128, 48, 160, 40],
    },
    "floor_1024": {
        "logical": (512, 256),
        "final": (1024, 512),
        "pivot": (256, 215),
        "baseline": 215,
        "safe": [128, 48, 160, 40],
    },
}


CLIPS = [
    ("idle", 8, 12, "body_512"),
    ("run", 10, 18, "body_512"),
    ("jump_start", 4, 18, "body_512"),
    ("jump_loop", 4, 12, "body_tall_640"),
    ("fall", 4, 12, "body_tall_640"),
    ("land", 4, 18, "body_512"),
    ("basic1", 7, 24, "slash_640"),
    ("basic2", 7, 24, "slash_640"),
    ("basic3", 9, 24, "dash_1024"),
    ("air_basic", 7, 24, "vertical_640"),
    ("launcher", 9, 24, "vertical_tall_768"),
    ("air_chase", 8, 24, "dash_tall_768"),
    ("magic_bolt", 9, 20, "body_512"),
    ("ally_support", 8, 18, "slash_640"),
    ("break_limit", 12, 24, "dash_1024"),
    ("hurt", 5, 18, "body_512"),
    ("launched", 4, 12, "body_tall_640"),
    ("knockdown", 5, 12, "floor_1024"),
    ("recover", 6, 16, "body_tall_640"),
    ("dead", 8, 12, "floor_1024"),
]


def poly(draw: ImageDraw.ImageDraw, pts, fill, outline: bool = True) -> None:
    if outline:
        cx = sum(x for x, _ in pts) / len(pts)
        cy = sum(y for _, y in pts) / len(pts)
        expanded = []
        for x, y in pts:
            vx, vy = x - cx, y - cy
            length = max(1.0, (vx * vx + vy * vy) ** 0.5)
            expanded.append((x + vx / length * 3, y + vy / length * 3))
        draw.polygon(expanded, fill=COLORS["outline"])
    draw.polygon(pts, fill=fill)


def thick_line(draw: ImageDraw.ImageDraw, pts, fill, width: int, outline: bool = True) -> None:
    if outline:
        draw.line(pts, fill=COLORS["outline"], width=width + 5, joint="curve")
    draw.line(pts, fill=fill, width=width, joint="curve")


def ellipse(draw: ImageDraw.ImageDraw, bbox, fill, outline: bool = True) -> None:
    if outline:
        x0, y0, x1, y1 = bbox
        draw.ellipse((x0 - 2, y0 - 2, x1 + 2, y1 + 2), fill=COLORS["outline"])
    draw.ellipse(bbox, fill=fill)


def draw_sword(
    draw: ImageDraw.ImageDraw,
    hilt: tuple[int, int],
    tip: tuple[int, int],
    width: int = 5,
    glow: bool = True,
) -> None:
    hx, hy = hilt
    tx, ty = tip
    dx, dy = tx - hx, ty - hy
    length = max(1.0, (dx * dx + dy * dy) ** 0.5)
    nx, ny = -dy / length, dx / length
    if glow:
        draw.line([hilt, tip], fill=COLORS["cyan_soft"], width=width + 10)
    draw.line([hilt, tip], fill=COLORS["outline"], width=width + 7)
    blade = [(hx + nx * 2, hy + ny * 2), (tx, ty), (hx - nx * 2, hy - ny * 2)]
    draw.polygon(blade, fill=COLORS["metal"])
    draw.line([(hx + nx * 0.4, hy + ny * 0.4), (tx - dx * 0.10, ty - dy * 0.10)], fill=COLORS["metal_hi"], width=1)
    draw.line([(hx - nx * 0.8, hy - ny * 0.8), (tx - dx * 0.20, ty - dy * 0.20)], fill=COLORS["cyan"], width=1)
    draw.line([(hx - nx * 11, hy - ny * 11), (hx + nx * 11, hy + ny * 11)], fill=COLORS["outline"], width=5)
    draw.line([(hx - nx * 9, hy - ny * 9), (hx + nx * 9, hy + ny * 9)], fill=COLORS["metal"], width=3)
    ellipse(draw, (hx - 3, hy - 3, hx + 3, hy + 3), COLORS["cyan"], True)


def draw_hair(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    poly(draw, [(x - 18, y - 4), (x - 9, y - 18), (x + 4, y - 22), (x + 19, y - 14), (x + 27, y - 2), (x + 16, y + 10), (x - 5, y + 12)], COLORS["hair"])
    for spike in [
        [(x - 10, y - 12), (x - 26, y - 25), (x - 18, y - 3)],
        [(x + 0, y - 20), (x + 2, y - 38), (x + 11, y - 17)],
        [(x + 14, y - 12), (x + 33, y - 23), (x + 22, y + 2)],
        [(x - 2, y + 6), (x - 18, y + 25), (x + 5, y + 11)],
    ]:
        poly(draw, spike, COLORS["hair"], False)
    draw.line([(x - 12, y - 10), (x + 6, y - 18), (x + 20, y - 6)], fill=COLORS["hair_hi"], width=2)


def draw_head(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    ellipse(draw, (x - 12, y - 12, x + 13, y + 13), COLORS["skin"])
    draw.rectangle((x + 4, y - 2, x + 14, y + 7), fill=COLORS["skin"])
    draw.rectangle((x + 8, y - 3, x + 12, y - 1), fill=(40, 55, 84, 255))
    draw.rectangle((x + 13, y + 2, x + 15, y + 3), fill=COLORS["skin_shadow"])
    draw_hair(draw, x, y - 7)


def draw_boot(draw: ImageDraw.ImageDraw, foot: tuple[int, int], facing: int = 1) -> None:
    x, y = foot
    poly(draw, [(x - 7, y - 5), (x + 9 * facing, y - 4), (x + 15 * facing, y + 2), (x + 2 * facing, y + 5), (x - 9, y + 4)], COLORS["boot"])
    draw.line([(x - 3, y - 3), (x + 8 * facing, y - 1)], fill=COLORS["boot_hi"], width=2)


def slash_trail(draw: ImageDraw.ImageDraw, pts, width: int = 15, alpha: int = 130) -> None:
    draw.line(pts, fill=(34, 218, 238, alpha // 2), width=width, joint="curve")
    draw.line(pts, fill=(165, 247, 255, alpha), width=max(3, width // 3), joint="curve")


def draw_aura(draw: ImageDraw.ImageDraw, center: tuple[int, int], radius: int, alpha: int = 70) -> None:
    x, y = center
    draw.ellipse((x - radius, y - radius, x + radius, y + radius), outline=(34, 218, 238, alpha), width=3)
    draw.ellipse((x - radius // 2, y - radius // 2, x + radius // 2, y + radius // 2), outline=(34, 218, 238, alpha), width=2)


def draw_character(draw: ImageDraw.ImageDraw, x: int, baseline: int, pose: dict) -> None:
    lean = pose.get("lean", 0)
    bob = pose.get("bob", 0)
    airborne = pose.get("airborne", 0)
    torso_x = x + lean
    shoulder = (torso_x + 2, baseline - 139 + bob - airborne)
    hip = (torso_x - 1, baseline - 78 + bob - airborne)
    head = (torso_x + 14, baseline - 160 + bob - airborne)
    if pose.get("shadow", False):
        draw.ellipse((x - 50, baseline - 8, x + 48, baseline + 7), fill=COLORS["shadow"])
    tail = pose.get("tail", -20)
    poly(draw, [(hip[0] - 22, hip[1] - 2), (hip[0] - 56 + tail, hip[1] + 62), (hip[0] - 20, hip[1] + 38), (hip[0] - 6, hip[1] + 4)], COLORS["jacket"])
    draw.line([(hip[0] - 35 + tail, hip[1] + 52), (hip[0] - 18, hip[1] + 33)], fill=COLORS["jacket_hi"], width=2)
    for leg in pose.get("legs", default_legs()):
        color = COLORS["pants"] if leg.get("side", 0) == 0 else (30, 32, 40, 255)
        knee = (hip[0] + leg["knee"][0], hip[1] + leg["knee"][1])
        foot = (hip[0] + leg["foot"][0], hip[1] + leg["foot"][1])
        thick_line(draw, [hip, knee, foot], color, 11)
        draw.line([knee, foot], fill=COLORS["pants_hi"], width=2)
        draw_boot(draw, foot, 1)
    poly(draw, [(shoulder[0] - 24, shoulder[1] + 2), (shoulder[0] + 22, shoulder[1] + 3), (hip[0] + 18, hip[1] + 8), (hip[0] - 16, hip[1] + 8)], COLORS["jacket"])
    poly(draw, [(shoulder[0] - 7, shoulder[1] + 7), (shoulder[0] + 17, shoulder[1] + 8), (hip[0] + 8, hip[1] + 3), (hip[0] - 4, hip[1] + 5)], COLORS["shirt"], False)
    draw.line([(shoulder[0] - 20, shoulder[1] + 8), (hip[0] - 12, hip[1] + 2)], fill=COLORS["jacket_hi"], width=2)
    poly(draw, [(shoulder[0] + 2, shoulder[1] + 11), (shoulder[0] + 12, shoulder[1] + 14), (hip[0] + 6, hip[1] - 4), (hip[0] - 2, hip[1] - 6)], (20, 28, 55, 255), True)

    right_hand = pose.get("right_hand", (shoulder[0] + 17 + pose.get("arm", 0), shoulder[1] + 58))
    left_hand = pose.get("left_hand", (shoulder[0] - 24 - pose.get("arm", 0), shoulder[1] + 50))
    thick_line(draw, [(shoulder[0] - 18, shoulder[1] + 12), (left_hand[0] + 8, left_hand[1] - 12), left_hand], COLORS["jacket"], 10)
    thick_line(draw, [(shoulder[0] + 18, shoulder[1] + 12), (right_hand[0] - 7, right_hand[1] - 13), right_hand], COLORS["jacket"], 10)
    ellipse(draw, (right_hand[0] - 6, right_hand[1] - 6, right_hand[0] + 6, right_hand[1] + 7), COLORS["skin"])
    ellipse(draw, (left_hand[0] - 5, left_hand[1] - 5, left_hand[0] + 5, left_hand[1] + 6), COLORS["skin"])

    if pose.get("left_cast"):
        lx, ly = left_hand
        draw_aura(draw, (lx - 12, ly - 4), 14 + pose.get("cast_radius", 0), 120)
        draw.line([(lx - 24, ly - 4), (lx - 44, ly - 4)], fill=COLORS["cyan"], width=2)
    if pose.get("sword_tip"):
        draw_sword(draw, right_hand, pose["sword_tip"], pose.get("sword_width", 4), pose.get("sword_glow", True))
    draw_head(draw, head[0], head[1])


def default_legs():
    return [
        {"side": 0, "knee": (-20, 42), "foot": (-48, 78)},
        {"side": 1, "knee": (22, 40), "foot": (42, 78)},
    ]


def make_frame(profile_name: str, painter: Callable[[ImageDraw.ImageDraw, dict], None], pose: dict) -> Image.Image:
    profile = PROFILES[profile_name]
    logical = profile["logical"]
    img = Image.new("RGBA", logical, (0, 0, 0, 0))
    draw = ImageDraw.Draw(img, "RGBA")
    painter(draw, pose)
    return img.resize(profile["final"], Image.Resampling.NEAREST)


def base_pose(profile_name: str) -> dict:
    baseline = PROFILES[profile_name]["baseline"]
    pivot_x = PROFILES[profile_name]["pivot"][0]
    return {
        "x": pivot_x,
        "baseline": baseline,
        "lean": 0,
        "bob": 0,
        "tail": -14,
        "legs": default_legs(),
        "right_hand": (pivot_x + 34, baseline - 86),
        "left_hand": (pivot_x - 34, baseline - 88),
        "sword_tip": (pivot_x + 92, baseline - 18),
        "sword_width": 4,
        "weapon_hand": "right",
    }


def painter_standard(draw: ImageDraw.ImageDraw, pose: dict) -> None:
    for trail in pose.get("trails", []):
        slash_trail(draw, trail["pts"], trail.get("width", 15), trail.get("alpha", 130))
    for aura in pose.get("auras", []):
        draw_aura(draw, aura["center"], aura["radius"], aura.get("alpha", 80))
    draw_character(draw, pose["x"], pose["baseline"], pose)


def leg_pair(phase: float):
    f1 = math.sin(phase)
    c1 = math.cos(phase)
    f2 = -f1
    c2 = -c1

    def leg(f, c, side):
        return {
            "side": side,
            "knee": (int(18 * f - 5), int(38 - max(0, c) * 8)),
            "foot": (int(36 * f + 6), int(78 - max(0, c) * 16)),
        }

    return [leg(f1, c1, 0), leg(f2, c2, 1)]


def ground_pose(profile: str, lean=0, bob=0, sword_angle="down") -> dict:
    p = base_pose(profile)
    p.update({"lean": lean, "bob": bob})
    x, b = p["x"], p["baseline"]
    if sword_angle == "down":
        p["right_hand"] = (x + 34 + lean, b - 86 + bob)
        p["sword_tip"] = (x + 96 + lean, b - 20 + bob)
    elif sword_angle == "back":
        p["right_hand"] = (x + 18 + lean, b - 90 + bob)
        p["sword_tip"] = (x - 88 + lean, b - 45 + bob)
    elif sword_angle == "up":
        p["right_hand"] = (x + 24 + lean, b - 112 + bob)
        p["sword_tip"] = (x + 12 + lean, b - 196 + bob)
    elif sword_angle == "forward":
        p["right_hand"] = (x + 46 + lean, b - 98 + bob)
        p["sword_tip"] = (x + 150 + lean, b - 88 + bob)
    return p


def poses_for_clip(action: str, count: int, profile: str) -> list[dict]:
    x, baseline = PROFILES[profile]["pivot"][0], PROFILES[profile]["baseline"]
    poses: list[dict] = []
    for i in range(count):
        t = i / max(1, count - 1)
        phase = 2 * math.pi * i / count
        p = ground_pose(profile)

        if action == "idle":
            p.update({"bob": int(round(math.sin(phase) * 1)), "tail": -14 + int(3 * math.sin(phase)), "legs": default_legs()})
            p["sword_tip"] = (x + 92, baseline - 20 + int(2 * math.sin(phase)))

        elif action == "run":
            p.update({"lean": 9, "bob": int(round(math.sin(phase) * 2)), "legs": leg_pair(phase), "tail": int(-8 * math.sin(phase))})
            p["right_hand"] = (x + 28, baseline - 88 + p["bob"])
            p["sword_tip"] = (x - 72, baseline - 38 + int(4 * math.sin(phase)))

        elif action == "jump_start":
            crouch = [0, 8, 14, 4][i]
            p.update({"lean": 4, "bob": crouch, "tail": -8, "legs": [
                {"side": 0, "knee": (-26, 52), "foot": (-44, 78)},
                {"side": 1, "knee": (30, 52), "foot": (54, 78)},
            ]})
            p["right_hand"] = (x + 20, baseline - 86 + crouch)
            p["sword_tip"] = (x - 70, baseline - 32 + crouch)

        elif action == "jump_loop":
            air = [28, 38, 36, 30][i]
            p.update({"lean": 6, "airborne": air, "shadow": False, "tail": -4, "legs": [
                {"side": 0, "knee": (-18, 34), "foot": (-28, 58)},
                {"side": 1, "knee": (30, 34), "foot": (42, 56)},
            ]})
            p["right_hand"] = (x + 34, baseline - 86 - air)
            p["sword_tip"] = (x + 100, baseline - 58 - air)

        elif action == "fall":
            air = [34, 26, 18, 10][i]
            p.update({"lean": 4, "airborne": air, "tail": -18, "legs": [
                {"side": 0, "knee": (-14, 46), "foot": (-28, 80)},
                {"side": 1, "knee": (26, 50), "foot": (46, 86)},
            ]})
            p["right_hand"] = (x + 30, baseline - 82 - air)
            p["sword_tip"] = (x + 92, baseline - 8 - air)

        elif action == "land":
            crouch = [10, 20, 10, 0][i]
            p.update({"lean": 2, "bob": crouch, "tail": -10, "legs": [
                {"side": 0, "knee": (-28, 50), "foot": (-54, 78)},
                {"side": 1, "knee": (28, 52), "foot": (58, 78)},
            ]})
            p["right_hand"] = (x + 22, baseline - 86 + crouch)
            p["sword_tip"] = (x + 88, baseline - 20 + crouch)

        elif action in {"basic1", "basic2", "basic3", "air_basic", "air_chase", "break_limit"}:
            p = attack_pose(action, i, count, profile)

        elif action == "launcher":
            p = launcher_pose(i, count, profile)

        elif action == "magic_bolt":
            p.update({"lean": -2, "bob": 0, "legs": default_legs(), "left_cast": True, "cast_radius": i % 4})
            p["right_hand"] = (x + 32, baseline - 86)
            p["sword_tip"] = (x + 78, baseline - 18)
            p["left_hand"] = (x - 42 - min(i * 3, 18), baseline - 112)
            p["auras"] = [{"center": (x - 72, baseline - 116), "radius": 12 + i * 2, "alpha": 70}]

        elif action == "ally_support":
            p = ground_pose(profile, lean=0, bob=int(math.sin(phase) * 1), sword_angle="up")
            p["auras"] = [{"center": (x + 4, baseline - 104), "radius": 36 + i * 2, "alpha": 60}]
            p["left_hand"] = (x - 34, baseline - 108)

        elif action == "hurt":
            recoil = [0, -8, -14, -8, -2][i]
            p.update({"lean": recoil, "bob": [0, -2, -4, -2, 0][i], "tail": -28, "legs": [
                {"side": 0, "knee": (-18, 42), "foot": (-46, 78)},
                {"side": 1, "knee": (20, 42), "foot": (36, 78)},
            ]})
            p["right_hand"] = (x + recoil + 28, baseline - 82 + p["bob"])
            p["sword_tip"] = (x + recoil + 90, baseline - 2 + p["bob"])

        elif action == "launched":
            air = [20, 46, 64, 54][i]
            p.update({"lean": -4, "airborne": air, "tail": -30, "legs": [
                {"side": 0, "knee": (-16, 26), "foot": (-36, 44)},
                {"side": 1, "knee": (26, 22), "foot": (48, 38)},
            ]})
            p["right_hand"] = (x + 26, baseline - 92 - air)
            p["sword_tip"] = (x + 70, baseline - 156 - air)

        elif action in {"knockdown", "dead"}:
            p = floor_pose(action, i, count, profile)

        elif action == "recover":
            crouch = [26, 20, 14, 8, 2, 0][i]
            lean = [-14, -10, -4, 0, 2, 0][i]
            p.update({"lean": lean, "bob": crouch, "tail": -8, "legs": [
                {"side": 0, "knee": (-28, 50), "foot": (-50, 78)},
                {"side": 1, "knee": (30, 50), "foot": (54, 78)},
            ]})
            p["right_hand"] = (x + 28 + lean, baseline - 86 + crouch)
            p["sword_tip"] = (x + 92 + lean, baseline - 18 + crouch)

        poses.append(p)
    return poses


def attack_pose(action: str, i: int, count: int, profile: str) -> dict:
    x, baseline = PROFILES[profile]["pivot"][0], PROFILES[profile]["baseline"]
    p = ground_pose(profile)
    if action == "basic1":
        raw = [
            (-2, 0, (x - 38, baseline - 78), (x - 104, baseline - 28), None),
            (-7, 4, (x - 48, baseline - 108), (x - 88, baseline - 158), None),
            (-2, 0, (x - 28, baseline - 124), (x - 56, baseline - 190), None),
            (10, -2, (x + 30, baseline - 94), (x + 138, baseline - 108), [(x + 10, baseline - 118), (x + 70, baseline - 138), (x + 146, baseline - 136)]),
            (14, 1, (x + 50, baseline - 74), (x + 134, baseline - 42), [(x + 26, baseline - 90), (x + 92, baseline - 70), (x + 154, baseline - 48)]),
            (6, 3, (x + 26, baseline - 68), (x + 98, baseline - 18), None),
            (2, 0, (x + 0, baseline - 76), (x + 66, baseline - 24), None),
        ]
    elif action == "basic2":
        raw = [
            (4, 0, (x + 28, baseline - 74), (x + 96, baseline - 22), None),
            (8, -2, (x + 44, baseline - 90), (x + 132, baseline - 74), None),
            (2, -3, (x + 18, baseline - 122), (x - 28, baseline - 188), None),
            (-8, 0, (x - 30, baseline - 102), (x - 130, baseline - 92), [(x - 4, baseline - 122), (x - 70, baseline - 126), (x - 148, baseline - 108)]),
            (-6, 2, (x - 26, baseline - 78), (x - 124, baseline - 40), [(x - 20, baseline - 96), (x - 84, baseline - 74), (x - 150, baseline - 46)]),
            (0, 2, (x + 2, baseline - 76), (x + 80, baseline - 20), None),
            (2, 0, (x + 20, baseline - 78), (x + 92, baseline - 24), None),
        ]
    elif action == "basic3":
        raw = []
        for k in range(count):
            tt = k / (count - 1)
            lean = int(-10 + 28 * tt)
            bob = int(6 * math.sin(math.pi * tt))
            hand = (x - 60 + int(150 * tt), baseline - 132 + int(70 * tt))
            tip = (x - 132 + int(296 * tt), baseline - 190 + int(128 * tt))
            trail = None
            if 3 <= k <= 6:
                trail = [(x - 126 + 36 * (k - 3), baseline - 170 + 18 * (k - 3)), (x + 24 + 36 * (k - 3), baseline - 120 + 15 * (k - 3)), (x + 168 + 28 * (k - 3), baseline - 86 + 10 * (k - 3))]
            raw.append((lean, bob, hand, tip, trail))
    elif action == "air_basic":
        raw = []
        for k in range(count):
            tt = k / (count - 1)
            swing = math.sin(math.pi * tt)
            hand = (x + 24 + int(28 * swing), baseline - 108 + int(2 * math.sin(math.tau * tt)))
            tip = (x + 154, baseline - 106 + int(8 * math.sin(math.pi * tt)))
            trail = [(x + 18, baseline - 114), (x + 82, baseline - 112), (x + 158, baseline - 106)] if 2 <= k <= 4 else None
            raw.append((4 + int(8 * swing), 0, hand, tip, trail))
    elif action == "air_chase":
        raw = []
        for k in range(count):
            tt = k / (count - 1)
            raw.append((20 + int(18 * tt), -4, (x + 46 + int(22 * tt), baseline - 100), (x + 170, baseline - 96 + int(18 * math.sin(tt * math.pi))), [(x + 14, baseline - 102), (x + 88, baseline - 110), (x + 174, baseline - 102)] if 2 <= k <= 5 else None))
    else:  # break_limit
        raw = []
        for k in range(count):
            tt = k / (count - 1)
            raw.append((20 + int(24 * math.sin(math.pi * tt)), -3, (x + 50, baseline - 100), (x + 184, baseline - 104 + int(22 * math.sin(2 * math.pi * tt))), [(x - 40, baseline - 114), (x + 76, baseline - 112), (x + 198, baseline - 106)] if 3 <= k <= 8 else None))
    lean, bob, hand, tip, trail = raw[i]
    p.update({"lean": lean, "bob": bob, "right_hand": hand, "sword_tip": tip, "sword_width": 5 if action in {"basic3", "break_limit"} else 4})
    p["legs"] = [
        {"side": 0, "knee": (-24 + lean // 4, 42), "foot": (-56, 78)},
        {"side": 1, "knee": (28 + lean // 3, 38), "foot": (56 + max(0, lean), 76)},
    ]
    if action in {"air_basic", "air_chase"}:
        p["airborne"] = 24
        p["shadow"] = False
    if action == "break_limit":
        p["auras"] = [{"center": (x + 8, baseline - 102), "radius": 34 + i * 2, "alpha": 52}]
    if trail:
        p["trails"] = [{"pts": trail, "width": 15 if action != "basic3" else 20, "alpha": 150}]
    return p


def launcher_pose(i: int, count: int, profile: str) -> dict:
    x, baseline = PROFILES[profile]["pivot"][0], PROFILES[profile]["baseline"]
    p = ground_pose(profile)
    air = [0, 0, 8, 22, 34, 26, 14, 4, 0][i]
    lean = [-4, -8, -4, 4, 8, 6, 2, 0, 0][i]
    p.update({"lean": lean, "airborne": air, "legs": [
        {"side": 0, "knee": (-20, 38), "foot": (-42, 74 - air // 3)},
        {"side": 1, "knee": (28, 34), "foot": (46, 70 - air // 4)},
    ]})
    hand = [(x - 20, baseline - 116), (x - 8, baseline - 144), (x + 8, baseline - 164), (x + 20, baseline - 178), (x + 34, baseline - 190), (x + 30, baseline - 172), (x + 24, baseline - 148), (x + 20, baseline - 116), (x + 24, baseline - 94)][i]
    tip = [(x - 40, baseline - 184), (x - 24, baseline - 222), (x + 6, baseline - 252), (x + 42, baseline - 280), (x + 78, baseline - 292), (x + 90, baseline - 244), (x + 70, baseline - 204), (x + 62, baseline - 154), (x + 84, baseline - 42)][i]
    p["right_hand"] = hand
    p["sword_tip"] = tip
    if 2 <= i <= 5:
        p["trails"] = [{"pts": [(x + 0, baseline - 150), (x + 32, baseline - 214), (x + 82, baseline - 286)], "width": 18, "alpha": 150}]
    return p


def floor_pose(action: str, i: int, count: int, profile: str) -> dict:
    x, baseline = PROFILES[profile]["pivot"][0], PROFILES[profile]["baseline"]
    p = ground_pose(profile)
    if action == "knockdown":
        stages = ["stand", "recoil", "fall", "ground", "ground"]
    else:
        stages = ["stand", "recoil", "fall", "ground", "ground", "ground", "ground", "ground"]
    stage = stages[i]
    if stage == "stand":
        p.update({"lean": -6, "bob": 0})
        p["right_hand"] = (x + 24, baseline - 86)
        p["sword_tip"] = (x + 92, baseline - 20)
        return p
    if stage == "recoil":
        p.update({"lean": -22, "bob": -4, "tail": -30})
        p["right_hand"] = (x + 8, baseline - 88)
        p["sword_tip"] = (x + 72, baseline - 10)
        return p
    if stage == "fall":
        # Diagonal falling body.
        draw_x = x - 12
        p.update({"lean": -30, "bob": 24, "tail": -28, "legs": [
            {"side": 0, "knee": (-52, 20), "foot": (-106, 36)},
            {"side": 1, "knee": (18, 34), "foot": (74, 54)},
        ]})
        p["x"] = draw_x
        p["right_hand"] = (draw_x + 52, baseline - 48)
        p["sword_tip"] = (draw_x + 158, baseline - 30)
        return p

    # Grounded prone pose, drawn by specialized painter.
    return {
        "x": x,
        "baseline": baseline,
        "stage": "prone",
        "dead_dim": action == "dead" and i >= count - 3,
        "weapon_hand": "right",
    }


def painter_floor(draw: ImageDraw.ImageDraw, pose: dict) -> None:
    if pose.get("stage") != "prone":
        painter_standard(draw, pose)
        return
    x, baseline = pose["x"], pose["baseline"]
    alpha = 170 if pose.get("dead_dim") else 255
    dim = lambda c: (c[0], c[1], c[2], min(c[3], alpha))
    body = [(x - 96, baseline - 42), (x + 52, baseline - 46), (x + 90, baseline - 28), (x - 72, baseline - 24)]
    poly(draw, body, dim(COLORS["jacket"]))
    poly(draw, [(x - 62, baseline - 35), (x + 52, baseline - 38), (x + 76, baseline - 28), (x - 50, baseline - 23)], dim(COLORS["shirt"]), False)
    # Head and hair at left, legs at right.
    ellipse(draw, (x - 122, baseline - 52, x - 96, baseline - 27), dim(COLORS["skin"]))
    draw_hair(draw, x - 108, baseline - 48)
    thick_line(draw, [(x + 42, baseline - 34), (x + 102, baseline - 30), (x + 144, baseline - 22)], dim(COLORS["pants"]), 12)
    thick_line(draw, [(x + 30, baseline - 24), (x + 88, baseline - 16), (x + 126, baseline - 8)], dim(COLORS["pants"]), 11)
    draw_boot(draw, (x + 146, baseline - 22), 1)
    draw_boot(draw, (x + 128, baseline - 8), 1)
    right_hand = (x - 30, baseline - 28)
    ellipse(draw, (right_hand[0] - 6, right_hand[1] - 5, right_hand[0] + 6, right_hand[1] + 6), dim(COLORS["skin"]))
    draw_sword(draw, right_hand, (x + 192, baseline - 18), 4, True)


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
    for y in range(0, h, tile):
        for x in range(0, total, tile):
            if (x // tile + y // tile) % 2 == 0:
                draw.rectangle((x, y, x + tile - 1, y + tile - 1), fill=(34, 36, 43, 255))
    for i, frame in enumerate(frames):
        bg.alpha_composite(frame, (w * i, 0))
        draw.rectangle((w * i, 0, w * (i + 1) - 1, h - 1), outline=(96, 101, 116, 150), width=2)
        draw.line((w * i, baseline, w * (i + 1), baseline), fill=(255, 210, 80, 160), width=2)
    bg.save(path)


def gif_preview(frames: list[Image.Image], baseline: int, path: Path, duration: int) -> None:
    w, h = frames[0].size
    rendered = []
    for frame in frames:
        bg = Image.new("RGBA", (w, h), (22, 24, 29, 255))
        draw = ImageDraw.Draw(bg, "RGBA")
        tile = 16
        for yy in range(0, h, tile):
            for xx in range(0, w, tile):
                if (xx // tile + yy // tile) % 2 == 0:
                    draw.rectangle((xx, yy, xx + tile - 1, yy + tile - 1), fill=(34, 36, 43, 255))
        bg.alpha_composite(frame, (0, 0))
        draw.line((0, baseline, w, baseline), fill=(255, 210, 80, 150), width=2)
        bg = bg.resize((max(1, w // 2), max(1, h // 2)), Image.Resampling.NEAREST)
        rendered.append(bg.convert("P", palette=Image.Palette.ADAPTIVE))
    rendered[0].save(path, save_all=True, append_images=rendered[1:], duration=duration, loop=0, disposal=2)


def contact_sheet(previews: list[Path], out: Path) -> None:
    thumbs = []
    for path in previews:
        img = Image.open(path).convert("RGBA")
        scale = min(1.0, 960 / img.width)
        thumb = img.resize((int(img.width * scale), int(img.height * scale)), Image.Resampling.NEAREST)
        thumbs.append((path.stem.replace("_preview", ""), thumb))
    width = 1000
    y = 18
    rows = []
    try:
        from PIL import ImageFont
        font = ImageFont.load_default()
    except Exception:
        font = None
    for label, thumb in thumbs:
        row_h = thumb.height + 30
        rows.append((label, thumb, y))
        y += row_h
    sheet = Image.new("RGBA", (width, y + 10), (18, 20, 24, 255))
    draw = ImageDraw.Draw(sheet)
    for label, thumb, yy in rows:
        draw.text((12, yy), label, fill=(230, 235, 245, 255), font=font)
        sheet.alpha_composite(thumb, (12, yy + 18))
    sheet.save(out)


def fixed_scale_contact_sheet(previews: list[Path], out: Path, scale: float = 0.125) -> None:
    rows = []
    width = 0
    y = 18
    try:
        from PIL import ImageFont
        font = ImageFont.load_default()
    except Exception:
        font = None
    for path in previews:
        img = Image.open(path).convert("RGBA")
        thumb = img.resize((max(1, int(img.width * scale)), max(1, int(img.height * scale))), Image.Resampling.NEAREST)
        label = path.stem.replace("_preview", "")
        rows.append((label, thumb, y))
        width = max(width, thumb.width + 24)
        y += thumb.height + 30
    sheet = Image.new("RGBA", (max(1000, width), y + 10), (18, 20, 24, 255))
    draw = ImageDraw.Draw(sheet)
    for label, thumb, yy in rows:
        draw.text((12, yy), label, fill=(230, 235, 245, 255), font=font)
        sheet.alpha_composite(thumb, (12, yy + 18))
    sheet.save(out)


def validate_clip(frames: list[Image.Image], expected_size: tuple[int, int]) -> None:
    for frame in frames:
        assert frame.mode == "RGBA"
        assert frame.size == expected_size
        assert frame.getchannel("A").getbbox() is not None
        corners = [
            frame.getpixel((0, 0))[3],
            frame.getpixel((frame.width - 1, 0))[3],
            frame.getpixel((0, frame.height - 1))[3],
            frame.getpixel((frame.width - 1, frame.height - 1))[3],
        ]
        assert corners == [0, 0, 0, 0]


def write_params() -> None:
    lines = [
        "productionMode: programmatic_lowfi_validation",
        "generatedWithAi: false",
        "purpose: engineering validation placeholder, not final art",
        "weaponHand: right",
        "transparentPngRule:",
        "  requireRgba: true",
        "  unusedPixelsAlpha: 0",
        "",
        "profiles:",
    ]
    for name, profile in PROFILES.items():
        fw, fh = profile["final"]
        px, py = profile["pivot"][0] * 2, profile["pivot"][1] * 2
        baseline = profile["baseline"] * 2
        lines += [
            f"  {name}:",
            f"    cellWidth: {fw}",
            f"    cellHeight: {fh}",
            "    pivot: bottom_center",
            f"    pivotX: {px}",
            f"    pivotY: {py}",
            f"    baselineY: {baseline}",
            f"    renderOffset: [{-px}, {-py}]",
            f"    safePadding: {profile['safe']}",
        ]
    lines += ["", "clips:"]
    for action, count, fps, profile_name in CLIPS:
        fw, fh = PROFILES[profile_name]["final"]
        lines += [
            f"  protag_{action}_lowfi_strip:",
            f"    frameCount: {count}",
            f"    frameRate: {fps}",
            f"    profile: {profile_name}",
            f"    cellWidth: {fw}",
            f"    cellHeight: {fh}",
            f"    sourceFrames: WheatearEditor/assets/vertical_slice/source_frames/protag_{action}_lowfi/protag_{action}_lowfi_{{frame:03}}.png",
            f"    output: WheatearEditor/assets/vertical_slice/side_combat/sheets/protag_{action}_lowfi_strip.png",
            f"    loop: {'true' if action in {'idle', 'run', 'jump_loop'} else 'false'}",
            "    assembledFromSourceFrames: true",
            "    noCrossCellBleed: true",
            "    fixedCanvasPerClip: true",
            "    pivotBaselineStable: true",
            "    weaponHand: right",
        ]
    lines += [
        "",
        "validation:",
        "  fixedCanvasPerClip: true",
        "  pivotBaselineStable: true",
        "  visualScaleStable: true",
        "  collisionIndependentFromCanvas: true",
        "  lowfiPlaceholderOnly: true",
    ]
    (SHEET_ROOT / "protag_batch03_lowfi_sheet_params.yaml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    SHEET_ROOT.mkdir(parents=True, exist_ok=True)
    PREVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    preview_paths = []
    for action, count, fps, profile_name in CLIPS:
        source_dir = SOURCE_ROOT / f"protag_{action}_lowfi"
        source_dir.mkdir(parents=True, exist_ok=True)
        poses = poses_for_clip(action, count, profile_name)
        frames: list[Image.Image] = []
        for index, pose in enumerate(poses):
            painter = painter_floor if profile_name == "floor_1024" else painter_standard
            frame = make_frame(profile_name, painter, pose)
            frame_path = source_dir / f"protag_{action}_lowfi_{index:03}.png"
            frame.save(frame_path)
            frames.append(frame)
        final_size = PROFILES[profile_name]["final"]
        validate_clip(frames, final_size)
        strip_path = SHEET_ROOT / f"protag_{action}_lowfi_strip.png"
        save_strip(frames, strip_path)
        baseline = PROFILES[profile_name]["baseline"] * 2
        png_preview = PREVIEW_ROOT / f"protag_{action}_lowfi_preview.png"
        checker_preview(frames, baseline, png_preview)
        preview_paths.append(png_preview)
        gif_preview(frames, baseline, PREVIEW_ROOT / f"protag_{action}_lowfi_preview.gif", max(40, int(1000 / fps)))
    write_params()
    contact_sheet(preview_paths, PREVIEW_ROOT / "protag_batch03_lowfi_contact_sheet.png")
    fixed_scale_contact_sheet(preview_paths, PREVIEW_ROOT / "protag_batch03_lowfi_contact_sheet_fixed_scale.png")
    print(f"Generated {len(CLIPS)} clips")
    print(SOURCE_ROOT)
    print(SHEET_ROOT / "protag_batch03_lowfi_sheet_params.yaml")
    print(PREVIEW_ROOT / "protag_batch03_lowfi_contact_sheet.png")
    print(PREVIEW_ROOT / "protag_batch03_lowfi_contact_sheet_fixed_scale.png")


if __name__ == "__main__":
    main()
