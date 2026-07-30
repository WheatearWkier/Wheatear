from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path
from typing import Callable

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice" / "tactical_combat"
SCENE_PATH = ROOT / "WheatearEditor" / "assets" / "scenes" / "VerticalSliceTacticalCombat.wt"


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def save(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def canvas(size: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def upscale(image: Image.Image, scale: int = 3) -> Image.Image:
    return image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)


def background() -> Image.Image:
    w, h = 1920, 1080
    image = Image.new("RGBA", (w, h), (20, 24, 32, 255))
    draw = ImageDraw.Draw(image)

    for y in range(h):
        t = y / h
        top = (48, 61, 85)
        bottom = (22, 26, 36)
        color = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3))
        draw.line((0, y, w, y), fill=(*color, 255))

    draw.rectangle((0, 0, w, 180), fill=(31, 38, 54, 220))
    draw.polygon([(0, 220), (1920, 160), (1920, 560), (0, 650)], fill=(45, 58, 72, 210))
    draw.polygon([(0, 620), (1920, 520), (1920, 1080), (0, 1080)], fill=(64, 74, 76, 240))
    for x in range(-100, w + 160, 220):
        draw.rectangle((x + 60, 220, x + 92, 530), fill=(45, 42, 53, 230))
        draw.polygon([(x + 36, 220), (x + 116, 220), (x + 98, 170), (x + 54, 170)], fill=(72, 70, 84, 245))
    for i in range(18):
        x = 120 + i * 110
        draw.ellipse((x, 710 + (i % 3) * 14, x + 34, 724 + (i % 3) * 14), fill=(86, 230, 255, 90))

    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((1120, 230, 1720, 820), fill=(64, 180, 210, 38))
    gd.ellipse((240, 370, 760, 950), fill=(120, 92, 220, 28))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(42)))
    return image


def tile(kind: str) -> Image.Image:
    image, draw = canvas((160, 160))
    base = {
        "plain": (72, 84, 86, 235),
        "move": (54, 145, 170, 235),
        "attack": (168, 66, 58, 235),
        "selected": (190, 160, 66, 245),
    }[kind]
    draw.rounded_rectangle((8, 8, 152, 152), radius=10, fill=base, outline=(198, 216, 206, 120), width=3)
    for offset in range(22, 150, 28):
        draw.line((18, offset, 142, offset - 12), fill=(18, 26, 32, 70), width=2)
    draw.rectangle((15, 15, 145, 145), outline=(255, 255, 255, 52), width=2)
    if kind != "plain":
        draw.rounded_rectangle((18, 18, 142, 142), radius=9, outline=(255, 255, 255, 120), width=5)
    return image


def panel(size: tuple[int, int], accent: tuple[int, int, int]) -> Image.Image:
    image, draw = canvas(size)
    w, h = size
    draw.rounded_rectangle((4, 4, w - 4, h - 4), radius=12, fill=(20, 24, 34, 220), outline=accent + (190,), width=3)
    draw.rounded_rectangle((16, 16, w - 16, h - 16), radius=8, outline=(220, 236, 230, 54), width=2)
    return image


def icon(kind: str) -> Image.Image:
    image, draw = canvas((128, 128))
    draw.rounded_rectangle((8, 8, 120, 120), radius=18, fill=(26, 34, 48, 235), outline=(80, 225, 240, 180), width=4)
    if kind == "attack":
        draw.line((34, 92, 90, 28), fill=(226, 255, 255, 255), width=9)
        draw.line((42, 98, 96, 38), fill=(58, 226, 240, 255), width=4)
    elif kind == "magic":
        draw.ellipse((32, 30, 96, 94), outline=(86, 210, 255, 255), width=7)
        draw.polygon([(64, 20), (74, 58), (112, 62), (76, 76), (64, 112), (52, 76), (16, 62), (54, 58)],
                     fill=(124, 98, 238, 210))
    elif kind == "heal":
        draw.rectangle((56, 26, 72, 102), fill=(180, 255, 208, 255))
        draw.rectangle((26, 56, 102, 72), fill=(180, 255, 208, 255))
        draw.ellipse((28, 28, 100, 100), outline=(116, 230, 190, 200), width=5)
    elif kind == "guard":
        draw.polygon([(64, 18), (100, 34), (94, 86), (64, 110), (34, 86), (28, 34)], fill=(226, 198, 94, 255))
        draw.polygon([(64, 30), (88, 40), (82, 78), (64, 96), (46, 78), (40, 40)], fill=(48, 70, 88, 255))
    elif kind == "enemy":
        draw.polygon([(30, 88), (62, 22), (98, 88)], fill=(232, 92, 74, 255))
        draw.line((42, 78, 86, 78), fill=(255, 220, 168, 255), width=6)
    elif kind == "enemy_magic":
        draw.ellipse((34, 30, 94, 92), fill=(120, 62, 190, 220))
        draw.ellipse((50, 46, 78, 74), fill=(235, 215, 255, 230))
        draw.arc((22, 16, 106, 112), 210, 338, fill=(255, 96, 174, 220), width=6)
    elif kind == "wait":
        draw.arc((34, 30, 94, 92), 40, 330, fill=(226, 220, 154, 255), width=8)
        draw.polygon([(91, 30), (110, 34), (98, 50)], fill=(226, 220, 154, 255))
    return image


UNIT_DEFS = {
    "unit_protag_magic_swordsman": ((64, 224, 238), (36, 68, 92), True),
    "unit_white_mage": ((158, 248, 218), (222, 236, 228), True),
    "unit_guardian": ((232, 198, 92), (54, 76, 98), True),
    "unit_en_spear_soldier": ((238, 92, 72), (70, 72, 80), False),
    "unit_en_dark_mage": ((180, 92, 232), (48, 36, 72), False),
    "unit_en_beast": ((128, 164, 168), (66, 78, 76), False),
}


def unit_base(name: str, accent: tuple[int, int, int], body: tuple[int, int, int], player: bool) -> Image.Image:
    image, draw = canvas((96, 112))
    cx = 47 if player else 50
    draw.ellipse((20, 94, 78, 105), fill=(0, 0, 0, 88))
    if name.endswith("beast"):
        draw.ellipse((18, 52, 70, 84), fill=body + (255,))
        draw.ellipse((46, 38, 86, 70), fill=accent + (235,))
        draw.polygon([(52, 40), (60, 24), (66, 44)], fill=(42, 50, 52, 255))
        draw.polygon([(76, 42), (88, 28), (86, 50)], fill=(42, 50, 52, 255))
        draw.ellipse((58, 54, 63, 59), fill=(255, 82, 64, 255))
        draw.ellipse((76, 54, 81, 59), fill=(255, 82, 64, 255))
        draw.rectangle((28, 78, 40, 98), fill=(38, 44, 44, 255))
        draw.rectangle((58, 78, 70, 98), fill=(38, 44, 44, 255))
        return image

    draw.ellipse((cx - 15, 14, cx + 15, 44), fill=(232, 184, 150, 255))
    draw.polygon([(cx - 16, 16), (cx + 2, 5), (cx + 20, 18), (cx + 14, 30), (cx - 12, 30)], fill=(28, 34, 48, 255))
    draw.rectangle((cx - 16, 48, cx + 16, 82), fill=body + (255,))
    draw.rectangle((cx - 18, 82, cx - 6, 103), fill=(24, 34, 52, 255))
    draw.rectangle((cx + 6, 82, cx + 18, 103), fill=(24, 34, 52, 255))
    if player:
        draw.line((cx + 24, 28, cx + 38, 88), fill=accent + (255,), width=5)
        draw.line((cx + 26, 30, cx + 40, 90), fill=(232, 255, 255, 220), width=2)
    else:
        draw.line((cx - 26, 34, cx + 22, 78), fill=accent + (255,), width=5)
    if "white_mage" in name:
        draw.ellipse((cx - 24, 35, cx + 24, 92), outline=accent + (180,), width=3)
    if "guardian" in name:
        draw.polygon([(cx - 34, 50), (cx - 16, 40), (cx - 12, 74), (cx - 32, 82)], fill=accent + (240,))
    return image


def shifted(image: Image.Image, dx: int = 0, dy: int = 0) -> Image.Image:
    out = Image.new("RGBA", image.size, (0, 0, 0, 0))
    out.alpha_composite(image, (dx, dy))
    return out


def tinted(image: Image.Image, color: tuple[int, int, int], amount: float) -> Image.Image:
    overlay = Image.new("RGBA", image.size, color + (0,))
    overlay.putalpha(image.getchannel("A"))
    return Image.blend(image, overlay, amount)


def save_unit_frames() -> None:
    root = ASSET_ROOT / "characters"
    for name, (accent, body, player) in UNIT_DEFS.items():
        base = unit_base(name, accent, body, player)
        save(base, root / f"{name}.png")
        for i, dy in enumerate([0, -2, 0, 1], 1):
            frame = shifted(base, 0, dy)
            if i == 2:
                glow = Image.new("RGBA", frame.size, accent + (0,))
                glow.putalpha(frame.getchannel("A").filter(ImageFilter.GaussianBlur(3)))
                frame = Image.alpha_composite(glow, frame)
            save(frame, root / f"{name}_idle_{i:02}.png")
        for i in range(1, 6):
            dx = (i - 1) * (3 if player else -3)
            frame = shifted(base, dx, -1 if i == 3 else 0)
            draw = ImageDraw.Draw(frame)
            direction = 1 if player else -1
            draw.arc((20 + direction * i * 3, 24, 96 + direction * i * 5, 100), 205 if player else 20, 330 if player else 150, fill=accent + (230,), width=5)
            save(frame, root / f"{name}_attack_{i:02}.png")
        for i in range(1, 4):
            frame = tinted(shifted(base, -3 if player else 3, 0), (255, 102, 76), 0.34 + i * 0.04)
            save(frame, root / f"{name}_hit_{i:02}.png")
        for i in range(1, 4):
            frame = tinted(base.rotate(-62 if player else 62, resample=Image.Resampling.BICUBIC, expand=False), (84, 92, 104), 0.36)
            frame = shifted(frame, -8 if player else 8, 17 + i)
            save(frame, root / f"{name}_down_{i:02}.png")


def effect_frame(kind: str, frame: int, count: int) -> Image.Image:
    image, draw = canvas((160, 160))
    t = frame / max(count, 1)
    if kind == "slash":
        draw.arc((18, 34, 148, 132), 205, 340, fill=(82, 235, 255, 210), width=12)
        draw.arc((30, 42, 150, 130), 210, 330, fill=(255, 255, 255, 230), width=5)
    elif kind == "magic":
        r = 18 + int(t * 46)
        draw.ellipse((80 - r, 80 - r, 80 + r, 80 + r), outline=(114, 118, 255, 230), width=8)
        draw.polygon([(80, 20), (96, 70), (145, 80), (96, 94), (80, 142), (64, 94), (16, 80), (64, 70)], fill=(90, 220, 255, 120))
    elif kind == "heal":
        r = 22 + int(t * 38)
        draw.ellipse((80 - r, 80 - r, 80 + r, 80 + r), outline=(156, 255, 198, 230), width=7)
        draw.rectangle((72, 38, 88, 122), fill=(205, 255, 222, 200))
        draw.rectangle((38, 72, 122, 88), fill=(205, 255, 222, 200))
    elif kind == "guard":
        draw.polygon([(80, 24), (124, 42), (114, 105), (80, 138), (46, 105), (36, 42)], fill=(238, 206, 92, 150))
        draw.polygon([(80, 40), (108, 52), (102, 96), (80, 120), (58, 96), (52, 52)], outline=(255, 255, 220, 230), width=5)
    elif kind == "dark":
        r = 18 + int(t * 42)
        draw.ellipse((80 - r, 80 - r, 80 + r, 80 + r), fill=(124, 48, 180, 150), outline=(255, 92, 188, 210), width=6)
        draw.arc((22, 28, 138, 132), 210, 340, fill=(255, 68, 120, 180), width=7)
    return image.filter(ImageFilter.GaussianBlur(0.25))


def save_effects() -> None:
    specs = {
        "slash": 5,
        "magic": 6,
        "heal": 6,
        "guard": 4,
        "dark": 6,
    }
    for kind, count in specs.items():
        for frame in range(1, count + 1):
            save(effect_frame(kind, frame, count), ASSET_ROOT / "effects" / f"vfx_tac_{kind}_{frame:02}.png")


def sine(freq: float, t: float) -> float:
    return math.sin(2.0 * math.pi * freq * t)


def write_wav(path: Path, duration: float, generator: Callable[[float], float], sample_rate: int = 44100) -> None:
    ensure(path)
    sample_count = int(duration * sample_rate)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        data = bytearray()
        for i in range(sample_count):
            t = i / sample_rate
            env = min(1.0, t / 0.02) * max(0.0, 1.0 - t / max(duration, 0.001))
            sample = max(-1.0, min(1.0, generator(t) * env))
            data += struct.pack("<h", int(sample * 32767))
        wav.writeframes(data)


def bgm(duration: float = 22.0, sample_rate: int = 44100) -> list[float]:
    chords = [
        (130.81, 196.00, 261.63),
        (146.83, 220.00, 293.66),
        (164.81, 246.94, 329.63),
        (123.47, 196.00, 246.94),
    ]
    count = int(duration * sample_rate)
    result = []
    for i in range(count):
        t = i / sample_rate
        chord = chords[int(t / 2.75) % len(chords)]
        pulse = 0.70 + 0.30 * max(0.0, sine(1.45, t))
        value = sine(chord[0], t) * 0.44 + sine(chord[1], t) * 0.26 + sine(chord[2], t) * 0.18
        result.append(value * 0.11 * pulse)
    return result


def write_samples(path: Path, samples: list[float], sample_rate: int = 44100) -> None:
    ensure(path)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        for sample in samples:
            wav.writeframesraw(int(max(-1.0, min(1.0, sample)) * 32767).to_bytes(2, "little", signed=True))


def save_audio() -> None:
    random.seed(11)
    root = ASSET_ROOT / "audio"
    write_samples(root / "tac_battle_bgm.wav", bgm())
    write_wav(root / "tac_select.wav", 0.10, lambda t: 0.22 * sine(660, t) + 0.16 * sine(990, t))
    write_wav(root / "tac_move.wav", 0.12, lambda t: 0.18 * sine(180, t) + random.uniform(-0.18, 0.18))
    write_wav(root / "tac_slash.wav", 0.16, lambda t: 0.34 * sine(720 - 1800 * t, t) + random.uniform(-0.18, 0.18))
    write_wav(root / "tac_magic.wav", 0.28, lambda t: 0.24 * sine(360 + 900 * t, t) + 0.18 * sine(880 + 1200 * t, t))
    write_wav(root / "tac_heal.wav", 0.34, lambda t: 0.22 * sine(392, t) + 0.18 * sine(523.25, t) + 0.12 * sine(783.99, t))
    write_wav(root / "tac_guard.wav", 0.20, lambda t: 0.26 * sine(196, t) + 0.15 * sine(392, t))
    write_wav(root / "tac_hit.wav", 0.16, lambda t: 0.40 * sine(130, t) + random.uniform(-0.36, 0.36))
    write_wav(root / "tac_victory.wav", 0.55, lambda t: 0.22 * sine(523.25, t) + 0.18 * sine(659.25, t) + 0.14 * sine(783.99, t))


def quote(value: str) -> str:
    return '"' + value.replace("\\", "/").replace('"', '\\"') + '"'


def entity_header(entity_id: int, tag: str) -> list[str]:
    return [
        f"  - Entity: {entity_id}",
        "    TagComponent:",
        f"      Tag: {tag}",
        "    TransformComponent:",
        "      Translation: [0, 0, 0]",
        "      Rotation: [0, 0, 0]",
        "      Scale: [1, 1, 1]",
    ]


def widget_block(pos: tuple[float, float], size: tuple[float, float], sort: int, parent: str = "WT_UI_Canvas", visible: bool = True) -> list[str]:
    return [
        "    UIWidgetComponent:",
        f"      Visible: {'true' if visible else 'false'}",
        f"      Position: [{pos[0]:.5f}, {pos[1]:.5f}]",
        f"      Size: [{size[0]:.5f}, {size[1]:.5f}]",
        "      Rotation: 0",
        "      Anchor: 0",
        f"      SortOrder: {sort}",
        "      ParentEntity: 0",
        f"      ParentTag: {quote(parent)}",
    ]


def image_block(path: str, color: tuple[float, float, float, float] = (1, 1, 1, 1)) -> list[str]:
    return [
        "    UIImageComponent:",
        f"      Color: [{color[0]}, {color[1]}, {color[2]}, {color[3]}]",
        f"      TexturePath: {quote(path)}",
    ]


def text_block(text: str, size: int = 20, color: tuple[float, float, float, float] = (0.92, 0.94, 0.86, 1)) -> list[str]:
    return [
        "    UITextComponent:",
        f"      Text: {quote(text)}",
        f"      Color: [{color[0]}, {color[1]}, {color[2]}, {color[3]}]",
        f"      FontSize: {size}",
        "      FontPath: assets/fonts/wqy-microhei.ttc",
        "      ShadowColor: [0.02, 0.02, 0.025, 0.82]",
        "      ShadowOffset: [2, 2]",
        "      OutlineColor: [0.01, 0.01, 0.012, 0.92]",
        "      OutlineThickness: 1.4",
    ]


def button_block(command: str) -> list[str]:
    return [
        "    UIButtonComponent:",
        "      NormalColor: [0.06, 0.12, 0.15, 0.08]",
        "      HoverColor: [0.20, 0.42, 0.46, 0.35]",
        "      PressedColor: [0.08, 0.20, 0.24, 0.45]",
        f"      OnClickFunction: {quote(command)}",
    ]


def progress_block() -> list[str]:
    return [
        "    UIProgressBarComponent:",
        "      Value: 1",
        "      MaxValue: 1",
        "      ForegroundColor: [0.34, 0.86, 0.58, 1]",
        "      BackgroundColor: [0.08, 0.10, 0.12, 0.85]",
    ]


def unit_component_block(unit: dict[str, object]) -> list[str]:
    prefix = str(unit["asset"])
    tag = str(unit["tag"])
    health_bar = f"TK_Status_{tag}_HP"
    status_text = f"TK_Status_{tag}_Text"
    marker = f"TK_Marker_{tag}"
    idle_pattern = f"assets/vertical_slice/tactical_combat/characters/{prefix}_idle_{{frame2}}.png"
    attack_pattern = f"assets/vertical_slice/tactical_combat/characters/{prefix}_attack_{{frame2}}.png"
    hit_pattern = f"assets/vertical_slice/tactical_combat/characters/{prefix}_hit_{{frame2}}.png"
    down_pattern = f"assets/vertical_slice/tactical_combat/characters/{prefix}_down_{{frame2}}.png"
    return [
        "    TacticalUnitComponent:",
        f"      Team: {unit['team']}",
        f"      Slot: {unit['slot']}",
        f"      GridX: {unit['x']}",
        f"      GridY: {unit['y']}",
        f"      DisplayName: {quote(str(unit['name']))}",
        f"      ClassName: {quote(str(unit['class']))}",
        f"      MaxHealth: {unit['hp']}",
        f"      Health: {unit['hp']}",
        f"      Attack: {unit['atk']}",
        f"      Magic: {unit['mag']}",
        f"      Defense: {unit['def']}",
        f"      MoveRange: {unit['move']}",
        f"      AttackRange: {unit['range']}",
        f"      Controllable: {'true' if unit['team'] == 1 else 'false'}",
        "      Invulnerable: false",
        f"      BasicSkillId: {quote(str(unit['basic']))}",
        f"      Skill1Id: {quote(str(unit['skill1']))}",
        f"      Skill2Id: {quote(str(unit['skill2']))}",
        f"      HealthBarEntityName: {quote(health_bar)}",
        f"      StatusTextEntityName: {quote(status_text)}",
        f"      MarkerEntityName: {quote(marker)}",
        f"      IdleFramePattern: {quote(idle_pattern)}",
        f"      AttackFramePattern: {quote(attack_pattern)}",
        f"      HitFramePattern: {quote(hit_pattern)}",
        f"      DownFramePattern: {quote(down_pattern)}",
        "      IdleFrameCount: 4",
        "      AttackFrameCount: 5",
        "      HitFrameCount: 3",
        "      DownFrameCount: 3",
        "      AnimationFrameRate: 8",
    ]


def generate_scene() -> None:
    eid = 950200000

    def next_id() -> int:
        nonlocal eid
        eid += 1
        return eid

    lines: list[str] = ["Scene: VerticalSliceTacticalCombat", "Entities:"]

    lines += entity_header(next_id(), "WT_UI_Canvas")
    lines += [
        "    UICanvasComponent:",
        "      Visible: true",
        "      ReferenceWidth: 1920",
        "      ReferenceHeight: 1080",
    ]
    lines += widget_block((0, 0), (1, 1), 0, "", True)

    lines += entity_header(next_id(), "TK_Camera")
    lines += [
        "    CameraComponent:",
        "      Camera:",
        "        ProjectionType: 1",
        "        PerspectiveFOV: 0.785398185",
        "        PerspectiveNear: 0.00999999978",
        "        PerspectiveFar: 1000",
        "        OrthographicSize: 10",
        "        OrthographicNear: -1",
        "        OrthographicFar: 1",
        "      Primary: true",
        "      FixedAspectRatio: false",
    ]

    lines += entity_header(next_id(), "TK_Background")
    lines += widget_block((0, 0), (1, 1), 1)
    lines += image_block("assets/vertical_slice/tactical_combat/backgrounds/bg_tactical_ruins_grid.png")

    lines += entity_header(next_id(), "TK_BGM")
    lines += [
        "    AudioSourceComponent:",
        "      AudioFilePath: assets/vertical_slice/tactical_combat/audio/tac_battle_bgm.wav",
        "      Volume: 0.42",
        "      Loop: true",
        "      PlayOnStart: true",
    ]

    lines += entity_header(next_id(), "TK_Controller")
    lines += [
        "    TacticalCombatLevelComponent:",
        "      PlayOnStart: true",
        "      LevelId: CH01_TACTICAL_FakeHeavenlyTribulation",
        "      GridWidth: 8",
        "      GridHeight: 6",
        "      BoardOrigin: [0.275, 0.115]",
        "      CellSize: [0.0625, 0.0875]",
        "      CellEntityPrefix: TK_Cell_",
        "      UnitEntityPrefix: TK_Unit_",
        "      FadeEntityName: TK_Fade",
        "      MessageTextEntityName: TK_MessageText",
        "      PhaseTextEntityName: TK_PhaseText",
        "      DetailTextEntityName: TK_DetailText",
        "      CommandPanelEntityName: TK_CommandPanel",
        "      ActionEffectEntityName: TK_ActionEffect",
        "      VictorySceneCommand: \"scene:assets/scenes/VerticalSliceHub.wt\"",
        "      DefeatSceneCommand: \"scene:assets/scenes/VerticalSliceTacticalCombat.wt\"",
        "      StartFadeDuration: 0.45",
        "      IntroDuration: 0.65",
        "      ActionDuration: 0.62",
        "      EnemyStepDuration: 0.42",
        "      VictoryReturnDelay: 1.75",
        "      DefeatReturnDelay: 1.35",
        "      TileNormalColor: [1, 1, 1, 0.92]",
        "      TileMoveColor: [0.32, 0.78, 1, 0.88]",
        "      TileAttackColor: [1, 0.36, 0.28, 0.92]",
        "      TileSelectedColor: [1, 0.88, 0.32, 1]",
    ]

    board_origin = (0.275, 0.115)
    cell = (0.0625, 0.0875)
    for y in range(6):
        for x in range(8):
            tag = f"TK_Cell_{x}_{y}"
            lines += entity_header(next_id(), tag)
            lines += widget_block((board_origin[0] + x * cell[0], board_origin[1] + y * cell[1]), cell, 5)
            lines += image_block("assets/vertical_slice/tactical_combat/tiles/tile_plain.png")
            lines += button_block(f"tactic:cell:{x}:{y}")

    units = [
        {"tag": "Hero", "asset": "unit_protag_magic_swordsman", "team": 1, "slot": 0, "x": 1, "y": 3, "name": "主角", "class": "魔剑士", "hp": 160, "atk": 31, "mag": 24, "def": 12, "move": 3, "range": 1, "basic": "sword_slash", "skill1": "aether_lance", "skill2": "white_pulse"},
        {"tag": "WhiteMage", "asset": "unit_white_mage", "team": 1, "slot": 1, "x": 0, "y": 4, "name": "白魔队友", "class": "白魔法", "hp": 112, "atk": 14, "mag": 28, "def": 8, "move": 3, "range": 1, "basic": "sword_slash", "skill1": "white_pulse", "skill2": "guard_wait"},
        {"tag": "Guardian", "asset": "unit_guardian", "team": 1, "slot": 2, "x": 1, "y": 5, "name": "护卫队友", "class": "剑盾", "hp": 190, "atk": 24, "mag": 10, "def": 18, "move": 2, "range": 1, "basic": "sword_slash", "skill1": "guard_wait", "skill2": ""},
        {"tag": "Spear", "asset": "unit_en_spear_soldier", "team": 2, "slot": 0, "x": 6, "y": 2, "name": "敌方枪兵", "class": "近战", "hp": 110, "atk": 24, "mag": 8, "def": 9, "move": 2, "range": 1, "basic": "enemy_strike", "skill1": "enemy_strike", "skill2": ""},
        {"tag": "DarkMage", "asset": "unit_en_dark_mage", "team": 2, "slot": 1, "x": 7, "y": 3, "name": "敌方法师", "class": "黑魔法", "hp": 92, "atk": 12, "mag": 30, "def": 6, "move": 2, "range": 3, "basic": "enemy_dark", "skill1": "enemy_dark", "skill2": ""},
        {"tag": "Beast", "asset": "unit_en_beast", "team": 2, "slot": 2, "x": 6, "y": 5, "name": "敌方兽兵", "class": "魔物", "hp": 128, "atk": 27, "mag": 6, "def": 10, "move": 3, "range": 1, "basic": "enemy_strike", "skill1": "enemy_strike", "skill2": ""},
    ]

    for unit in units:
        tag = f"TK_Unit_{unit['tag']}"
        lines += entity_header(next_id(), tag)
        lines += widget_block((0.1, 0.1), (0.05, 0.08), 12)
        lines += image_block(f"assets/vertical_slice/tactical_combat/characters/{unit['asset']}.png")
        lines += unit_component_block(unit)

        lines += entity_header(next_id(), f"TK_Marker_{unit['tag']}")
        lines += widget_block((0.1, 0.1), (0.05, 0.08), 11, "WT_UI_Canvas", False)
        lines += image_block("assets/vertical_slice/tactical_combat/ui/marker_selected.png")

    lines += entity_header(next_id(), "TK_TopPanel")
    lines += widget_block((0.045, 0.035), (0.91, 0.075), 20)
    lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_top.png")

    lines += entity_header(next_id(), "TK_PhaseText")
    lines += widget_block((0.065, 0.052), (0.25, 0.04), 24)
    lines += text_block("我方回合", 22)

    lines += entity_header(next_id(), "TK_MessageText")
    lines += widget_block((0.34, 0.052), (0.48, 0.04), 24)
    lines += text_block("点击角色开始行动。", 19)

    lines += entity_header(next_id(), "TK_StatusPanel")
    lines += widget_block((0.045, 0.155), (0.205, 0.60), 20)
    lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_status.png")

    for i, unit in enumerate(units):
        y = 0.18 + i * 0.091
        lines += entity_header(next_id(), f"TK_Status_{unit['tag']}_Text")
        lines += widget_block((0.062, y), (0.168, 0.034), 25)
        lines += text_block(f"{unit['name']} {unit['hp']}/{unit['hp']}", 15, (0.9, 0.93, 0.86, 1) if unit["team"] == 1 else (1, 0.82, 0.76, 1))

        lines += entity_header(next_id(), f"TK_Status_{unit['tag']}_HP")
        lines += widget_block((0.062, y + 0.039), (0.16, 0.012), 25)
        lines += progress_block()

    lines += entity_header(next_id(), "TK_DetailPanel")
    lines += widget_block((0.735, 0.155), (0.22, 0.275), 20)
    lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_detail.png")

    lines += entity_header(next_id(), "TK_DetailText")
    lines += widget_block((0.752, 0.178), (0.185, 0.22), 25)
    lines += text_block("选择我方单位。", 16)

    lines += entity_header(next_id(), "TK_CommandPanel")
    lines += widget_block((0.735, 0.465), (0.22, 0.285), 22, "WT_UI_Canvas", False)
    lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_command.png")

    commands = [
        ("1", "slot0", "cmd_tac_attack.png", "攻击"),
        ("2", "slot1", "cmd_tac_magic.png", "技能1"),
        ("3", "slot2", "cmd_tac_heal.png", "技能2"),
        ("4", "wait", "cmd_tac_guard.png", "待机"),
    ]
    for idx, slot, icon_path, label in commands:
        x = 0.05 + (int(idx) - 1) % 2 * 0.46
        y = 0.09 + (int(idx) - 1) // 2 * 0.42
        lines += entity_header(next_id(), f"TK_Command_{idx}_Root")
        lines += widget_block((x, y), (0.39, 0.34), 24, "TK_CommandPanel")
        lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_button.png")
        lines += button_block(f"tactic:skill:{slot}")

        lines += entity_header(next_id(), f"TK_Command_{idx}_Icon")
        lines += widget_block((0.12, 0.07), (0.32, 0.52), 27, f"TK_Command_{idx}_Root")
        lines += image_block(f"assets/vertical_slice/tactical_combat/ui/icons/{icon_path}")

        lines += entity_header(next_id(), f"TK_Command_{idx}_Text")
        lines += widget_block((0.48, 0.24), (0.42, 0.26), 28, f"TK_Command_{idx}_Root")
        lines += text_block(label, 15)

    lines += entity_header(next_id(), "TK_CancelButton")
    lines += widget_block((0.75, 0.77), (0.18, 0.048), 24)
    lines += image_block("assets/vertical_slice/tactical_combat/ui/panels/panel_tactical_button.png")
    lines += button_block("tactic:skill:cancel")

    lines += entity_header(next_id(), "TK_CancelText")
    lines += widget_block((0.792, 0.781), (0.10, 0.025), 27)
    lines += text_block("取消", 15)

    lines += entity_header(next_id(), "TK_ActionEffect")
    lines += widget_block((0.1, 0.1), (0.08, 0.11), 30, "WT_UI_Canvas", False)
    lines += image_block("assets/vertical_slice/tactical_combat/effects/vfx_tac_slash_01.png", (1, 1, 1, 0))

    lines += entity_header(next_id(), "TK_Fade")
    lines += widget_block((0, 0), (1, 1), 100)
    lines += image_block("", (0, 0, 0, 1))

    ensure(SCENE_PATH)
    SCENE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    save(background(), ASSET_ROOT / "backgrounds" / "bg_tactical_ruins_grid.png")
    for kind in ("plain", "move", "attack", "selected"):
        save(tile(kind), ASSET_ROOT / "tiles" / f"tile_{kind}.png")
    save(panel((420, 96), (86, 220, 236)), ASSET_ROOT / "ui" / "panels" / "panel_tactical_top.png")
    save(panel((420, 720), (86, 220, 236)), ASSET_ROOT / "ui" / "panels" / "panel_tactical_status.png")
    save(panel((420, 420), (206, 182, 92)), ASSET_ROOT / "ui" / "panels" / "panel_tactical_detail.png")
    save(panel((420, 420), (86, 220, 236)), ASSET_ROOT / "ui" / "panels" / "panel_tactical_command.png")
    save(panel((180, 96), (86, 220, 236)), ASSET_ROOT / "ui" / "panels" / "panel_tactical_button.png")
    for kind, name in (
        ("attack", "cmd_tac_attack.png"),
        ("magic", "cmd_tac_magic.png"),
        ("heal", "cmd_tac_heal.png"),
        ("guard", "cmd_tac_guard.png"),
        ("wait", "cmd_tac_wait.png"),
        ("enemy", "cmd_tac_enemy.png"),
        ("enemy_magic", "cmd_tac_enemy_magic.png"),
    ):
        save(icon(kind), ASSET_ROOT / "ui" / "icons" / name)

    marker, md = canvas((160, 160))
    md.rounded_rectangle((12, 12, 148, 148), radius=18, outline=(255, 226, 84, 235), width=8)
    md.ellipse((54, 54, 106, 106), outline=(255, 255, 255, 120), width=4)
    save(marker, ASSET_ROOT / "ui" / "marker_selected.png")

    save_unit_frames()
    save_effects()
    save_audio()
    generate_scene()
    print(f"Generated tactical combat assets and scene under {ASSET_ROOT}")


if __name__ == "__main__":
    main()
