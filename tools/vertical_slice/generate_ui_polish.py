from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets"
SCENE_ROOT = ASSET_ROOT / "scenes"
UI_ROOT = ASSET_ROOT / "vertical_slice" / "ui"
MANIFEST_PATH = ASSET_ROOT / "vertical_slice" / "data" / "vertical_slice_manifest.json"
FONT = "assets/fonts/NotoSansSC-VF.ttf"


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


def framed_icon(base: tuple[int, int, int], accent: tuple[int, int, int], kind: str) -> Image.Image:
    image, d = canvas((64, 64))
    d.rounded_rectangle((5, 5, 59, 59), radius=7, fill=(*base, 245), outline=(238, 246, 226, 230), width=3)
    d.rounded_rectangle((11, 11, 53, 53), radius=4, fill=(*accent, 220))

    if kind == "slash":
        d.line((18, 47, 46, 16), fill=(245, 255, 240, 255), width=6)
        d.line((23, 50, 50, 21), fill=(96, 238, 226, 210), width=3)
    elif kind == "uppercut":
        d.arc((15, 14, 51, 55), 205, 325, fill=(245, 255, 240, 255), width=6)
        d.polygon([(44, 14), (51, 27), (38, 25)], fill=(245, 255, 240, 255))
    elif kind == "air":
        d.polygon([(17, 42), (33, 14), (47, 42), (37, 37), (33, 52), (29, 37)], fill=(232, 255, 250, 250))
        d.line((15, 49, 49, 49), fill=(91, 236, 224, 190), width=3)
    elif kind == "fire":
        d.polygon([(33, 9), (45, 28), (39, 49), (25, 53), (17, 40), (23, 25)], fill=(255, 210, 76, 255))
        d.polygon([(31, 20), (38, 33), (33, 45), (25, 42), (25, 30)], fill=(255, 93, 82, 245))
    elif kind == "support":
        d.ellipse((17, 15, 47, 45), outline=(235, 255, 240, 245), width=5)
        d.rectangle((29, 8, 35, 52), fill=(235, 255, 240, 245))
    elif kind == "dash":
        for y in (21, 32, 43):
            d.line((14, y, 37, y), fill=(238, 255, 244, 245), width=4)
        d.polygon([(37, 15), (52, 32), (37, 49)], fill=(238, 255, 244, 245))
    elif kind == "break":
        d.polygon([(12, 32), (25, 12), (29, 29), (41, 13), (34, 34), (52, 30), (36, 45), (40, 55), (25, 43)], fill=(255, 231, 122, 255))
        d.line((20, 52, 45, 12), fill=(37, 48, 58, 220), width=3)
    elif kind == "core":
        d.polygon([(32, 8), (47, 22), (42, 45), (32, 56), (22, 45), (17, 22)], fill=(85, 242, 255, 250), outline=(248, 255, 244, 235))
        d.polygon([(32, 17), (39, 26), (36, 41), (32, 47), (28, 41), (25, 26)], fill=(245, 255, 255, 245))
    elif kind == "locked":
        d.rectangle((18, 29, 46, 49), fill=(205, 212, 210, 230))
        d.arc((22, 14, 42, 38), 180, 360, fill=(205, 212, 210, 230), width=5)
        d.rectangle((30, 36, 34, 44), fill=(66, 75, 82, 230))
    return upscale(image)


def equipment_icon(base: tuple[int, int, int], accent: tuple[int, int, int], kind: str) -> Image.Image:
    image, d = canvas((64, 64))
    d.rounded_rectangle((6, 6, 58, 58), radius=5, fill=(*base, 245), outline=(245, 236, 204, 220), width=2)
    if kind == "armor":
        d.polygon([(17, 13), (32, 8), (47, 13), (50, 30), (42, 53), (32, 58), (22, 53), (14, 30)], fill=(*accent, 245), outline=(235, 244, 232, 220))
        d.line((20, 31, 44, 31), fill=(38, 55, 62, 170), width=2)
    elif kind == "leather":
        d.polygon([(18, 15), (32, 9), (46, 15), (47, 52), (32, 58), (17, 52)], fill=(111, 78, 56, 245), outline=(230, 202, 158, 220))
        for x in (24, 32, 40):
            d.line((x, 20, x - 4, 47), fill=(74, 49, 40, 150), width=2)
    elif kind == "pendant":
        d.arc((16, 8, 48, 45), 30, 150, fill=(230, 218, 180, 230), width=4)
        d.polygon([(32, 24), (43, 47), (31, 58), (21, 47)], fill=(*accent, 245), outline=(245, 236, 204, 220))
    elif kind == "ring":
        d.ellipse((17, 18, 47, 48), outline=(*accent, 250), width=7)
        d.polygon([(29, 12), (37, 12), (41, 19), (33, 27), (25, 19)], fill=(87, 237, 255, 235))
    elif kind == "boots":
        d.polygon([(14, 22), (29, 22), (32, 44), (48, 45), (52, 53), (22, 54), (15, 45)], fill=(*accent, 245))
        d.line((20, 33, 42, 33), fill=(240, 244, 220, 190), width=2)
    elif kind == "charm":
        d.rectangle((22, 12, 42, 48), fill=(*accent, 245), outline=(246, 235, 190, 230), width=2)
        d.line((27, 24, 37, 24), fill=(246, 235, 190, 230), width=2)
        d.line((32, 19, 32, 36), fill=(246, 235, 190, 230), width=2)
    elif kind == "blade":
        d.polygon([(35, 7), (44, 16), (31, 49), (24, 49), (23, 16)], fill=(228, 245, 247, 250))
        d.rectangle((18, 46, 42, 51), fill=(92, 64, 104, 245))
    elif kind == "feather":
        d.ellipse((16, 8, 45, 55), fill=(*accent, 235), outline=(250, 248, 230, 230))
        d.line((21, 50, 46, 13), fill=(255, 255, 255, 245), width=3)
        for y in (22, 30, 38):
            d.line((28, y, 42, y - 8), fill=(255, 255, 255, 180), width=2)
    return upscale(image)


def generate_assets() -> None:
    skill_specs = {
        "icon_skill_core.png": ((24, 68, 83), (33, 130, 145), "core"),
        "icon_skill_slash.png": ((46, 80, 77), (31, 118, 104), "slash"),
        "icon_skill_uppercut.png": ((52, 70, 96), (41, 100, 145), "uppercut"),
        "icon_skill_air.png": ((38, 82, 101), (31, 122, 140), "air"),
        "icon_skill_fire.png": ((94, 55, 43), (142, 65, 56), "fire"),
        "icon_skill_support.png": ((72, 58, 106), (96, 74, 152), "support"),
        "icon_skill_dash.png": ((54, 89, 70), (65, 136, 103), "dash"),
        "icon_skill_break.png": ((94, 76, 42), (139, 103, 46), "break"),
        "icon_skill_locked.png": ((62, 65, 70), (80, 86, 93), "locked"),
    }
    for name, (base, accent, kind) in skill_specs.items():
        save(framed_icon(base, accent, kind), UI_ROOT / "icons" / name)

    equipment_specs = {
        "icon_equipment_traveler_armor.png": ((58, 68, 67), (72, 178, 181), "armor"),
        "icon_equipment_black_forest_armor.png": ((52, 62, 48), (67, 110, 70), "leather"),
        "icon_equipment_beast_tooth.png": ((76, 55, 44), (230, 214, 160), "pendant"),
        "icon_equipment_magic_ring.png": ((56, 52, 82), (178, 132, 238), "ring"),
        "icon_equipment_wind_boots.png": ((42, 78, 73), (82, 198, 170), "boots"),
        "icon_equipment_ward_charm.png": ((74, 62, 44), (180, 142, 80), "charm"),
        "icon_equipment_training_blade.png": ((58, 62, 76), (138, 172, 190), "blade"),
        "icon_equipment_angel_feather.png": ((76, 68, 90), (225, 224, 246), "feather"),
    }
    for name, (base, accent, kind) in equipment_specs.items():
        save(equipment_icon(base, accent, kind), UI_ROOT / "icons" / name)


def q(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def transform() -> str:
    return """    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
"""


def entity(entity_id: int, tag: str) -> str:
    return f"""  - Entity: {entity_id}
    TagComponent:
      Tag: {tag}
"""


def camera(entity_id: int, tag: str) -> str:
    return entity(entity_id, tag) + """    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 1
        PerspectiveFOV: 0.785398185
        PerspectiveNear: 0.00999999978
        PerspectiveFar: 1000
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
"""


def background(entity_id: int, tag: str, tint: list[float]) -> str:
    return entity(entity_id, tag) + f"""    TransformComponent:
      Translation: [0, 0.35, -0.8]
      Rotation: [0, 0, 0]
      Scale: [20, 11.25, 1]
    SpriteRendererComponent:
      Color: [{', '.join(str(v) for v in tint)}]
      Texture: assets/vertical_slice/vn/backgrounds/bg_forest_camp_night.png
      TilingFactor: 1
"""


def anim(preset: str = "fade_in", delay: float = 0.0, duration: float = 0.24, loop: bool = False) -> str:
    return f"""    UIAnimatorComponent:
      Preset: "{preset}"
      PlayOnStart: true
      Loop: {'true' if loop else 'false'}
      Delay: {delay}
      Duration: {duration}
      Amplitude: 0.035
      Speed: 1
      FromOffset: [0, 0.03]
"""


def widget(pos: tuple[float, float], size: tuple[float, float], sort: int, visible: bool = True, rotation: float = 0.0, anchor: int = 0) -> str:
    return f"""    UIWidgetComponent:
      Visible: {'true' if visible else 'false'}
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size[0]}, {size[1]}]
      Rotation: {rotation}
      Anchor: {anchor}
      SortOrder: {sort}
"""


def text_component(body: str, color: list[float], font_size: int, style: str = "body") -> str:
    if style == "title":
        shadow, outline, thick, offset = [0.01, 0.015, 0.018, 0.88], [0.0, 0.0, 0.0, 0.94], 1.8, [2.4, 2.4]
    elif style == "small":
        shadow, outline, thick, offset = [0.01, 0.015, 0.018, 0.76], [0.0, 0.0, 0.0, 0.82], 1.1, [1.6, 1.6]
    else:
        shadow, outline, thick, offset = [0.01, 0.015, 0.018, 0.82], [0.0, 0.0, 0.0, 0.88], 1.35, [2.0, 2.0]
    return f"""    UITextComponent:
      Text: {q(body)}
      Color: [{', '.join(str(v) for v in color)}]
      FontSize: {font_size}
      FontPath: {FONT}
      ShadowColor: [{', '.join(str(v) for v in shadow)}]
      ShadowOffset: [{', '.join(str(v) for v in offset)}]
      OutlineColor: [{', '.join(str(v) for v in outline)}]
      OutlineThickness: {thick}
"""


def text(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], body: str, color: list[float], font_size: int, sort: int = 40, style: str = "body", visible: bool = True) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, visible) + text_component(body, color, font_size, style)


def image(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], texture: str, sort: int = 20, alpha: float = 1.0, visible: bool = True, pulse: bool = False) -> str:
    out = entity(entity_id, tag) + transform() + widget(pos, size, sort, visible)
    if pulse:
        out += anim("pulse", 0, 1.0, True)
    return out + f"""    UIImageComponent:
      Color: [1, 1, 1, {alpha}]
      TexturePath: {texture}
"""


def panel(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], bg: list[float], border: list[float], sort: int = 18, visible: bool = True) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, visible) + f"""    UIPanelComponent:
      BackgroundColor: [{', '.join(str(v) for v in bg)}]
      BorderColor: [{', '.join(str(v) for v in border)}]
      BorderThickness: 2
"""


def rect(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], color: list[float], sort: int = 24, rotation: float = 0.0, anchor: int = 4) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, True, rotation, anchor) + f"""    UIImageComponent:
      Color: [{', '.join(str(v) for v in color)}]
      TexturePath: ""
"""


def button(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], label: str, command: str, sort: int = 55, palette: str = "teal") -> str:
    palettes = {
        "teal": ([0.10, 0.29, 0.28, 0.92], [0.17, 0.45, 0.42, 0.98], [0.07, 0.18, 0.18, 1], [0.93, 1.0, 0.92, 1]),
        "gold": ([0.35, 0.25, 0.12, 0.92], [0.53, 0.39, 0.18, 0.98], [0.22, 0.15, 0.08, 1], [1.0, 0.95, 0.78, 1]),
        "blue": ([0.15, 0.25, 0.44, 0.92], [0.24, 0.38, 0.65, 0.98], [0.10, 0.16, 0.30, 1], [0.92, 0.97, 1.0, 1]),
        "violet": ([0.26, 0.20, 0.43, 0.92], [0.40, 0.30, 0.65, 0.98], [0.18, 0.12, 0.30, 1], [0.96, 0.94, 1.0, 1]),
        "dark": ([0.08, 0.10, 0.12, 0.70], [0.16, 0.22, 0.25, 0.92], [0.04, 0.05, 0.06, 0.92], [0.90, 0.96, 0.92, 1]),
    }
    normal, hover, pressed, color = palettes[palette]
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + anim("hover_pulse", 0, 0.16) + f"""    UIButtonComponent:
      NormalColor: [{', '.join(str(v) for v in normal)}]
      HoverColor: [{', '.join(str(v) for v in hover)}]
      PressedColor: [{', '.join(str(v) for v in pressed)}]
      OnClickFunction: {command}
""" + text_component(label, color, 20, "small")


def overlay_button(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], command: str, sort: int = 58) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + f"""    UIButtonComponent:
      NormalColor: [0, 0, 0, 0]
      HoverColor: [0.95, 1, 0.82, 0.13]
      PressedColor: [1, 0.90, 0.45, 0.22]
      OnClickFunction: {command}
"""


def progress(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], value: float, max_value: float, fg: list[float], bg: list[float] | None = None, sort: int = 44) -> str:
    if bg is None:
        bg = [0.06, 0.08, 0.10, 0.86]
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + f"""    UIProgressBarComponent:
      Value: {value}
      MaxValue: {max_value}
      ForegroundColor: [{', '.join(str(v) for v in fg)}]
      BackgroundColor: [{', '.join(str(v) for v in bg)}]
"""


def slider(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], value: float, min_value: float, max_value: float, sort: int = 48) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + f"""    UISliderComponent:
      Value: {value}
      MinValue: {min_value}
      MaxValue: {max_value}
      TrackColor: [0.08, 0.10, 0.12, 0.92]
      FillColor: [0.30, 0.78, 0.72, 0.96]
      HandleColor: [0.92, 0.98, 0.92, 1]
      HoverColor: [1, 0.92, 0.50, 1]
      OnValueChangedFunction: ""
"""


def skill_tree_scene() -> str:
    out = "Scene: VerticalSliceSkillTree\nEntities:\n"
    out += camera(970000001, "SkillTree_Camera")
    out += background(970000002, "SkillTree_Background", [0.72, 0.84, 0.92, 1])
    out += image(970000010, "SkillTree_MainPanel", (0.045, 0.075), (0.91, 0.79), "assets/vertical_slice/ui/panels/panel_skill.png", 10, 0.97)
    out += text(970000011, "SkillTree_Title", (0.085, 0.105), (0.42, 0.06), "魔剑技能树", [0.91, 0.98, 1, 1], 42, 42, "title")
    out += text(970000012, "SkillTree_Subtitle", (0.085, 0.17), (0.70, 0.035), "第2章 / 魔剑 Lv1 / 主角 Lv1", [0.68, 0.86, 0.90, 1], 18, 42, "small")
    out += panel(970000013, "SkillTree_NetworkPanel", (0.075, 0.235), (0.55, 0.51), [0.04, 0.07, 0.09, 0.58], [0.23, 0.58, 0.62, 0.85], 18)
    out += panel(970000014, "SkillTree_DetailPanel", (0.66, 0.235), (0.25, 0.51), [0.05, 0.06, 0.08, 0.70], [0.54, 0.70, 0.76, 0.85], 18)

    centers = {
        "core": (0.35, 0.49),
        "melee": (0.19, 0.35),
        "launcher": (0.19, 0.62),
        "air": (0.35, 0.30),
        "magic": (0.50, 0.37),
        "support": (0.51, 0.62),
        "mobility": (0.35, 0.70),
        "break": (0.50, 0.50),
    }
    lines = [("core", "melee"), ("core", "launcher"), ("core", "air"), ("core", "magic"), ("core", "support"), ("core", "mobility"), ("core", "break"), ("air", "break"), ("magic", "break")]
    for i, (a, b) in enumerate(lines):
        ax, ay = centers[a]
        bx, by = centers[b]
        mx, my = (ax + bx) * 0.5, (ay + by) * 0.5
        dx, dy = bx - ax, by - ay
        length = math.sqrt(dx * dx + dy * dy)
        angle = math.degrees(math.atan2(dy, dx))
        out += rect(970000100 + i, f"SkillTree_Line_{i+1}", (mx, my), (length, 0.006), [0.27, 0.88, 0.82, 0.55], 22, angle, 4)

    node_size = (0.062, 0.092)
    node_specs = [
        ("core", "SkillTree_Node_Core", "assets/vertical_slice/ui/icons/icon_skill_core.png", "progression:select_skill_core", "核心"),
        ("melee", "SkillTree_Node_Melee", "assets/vertical_slice/ui/icons/icon_skill_slash.png", "progression:select_skill_melee", "三段"),
        ("launcher", "SkillTree_Node_Launcher", "assets/vertical_slice/ui/icons/icon_skill_uppercut.png", "progression:select_skill_launcher", "上挑"),
        ("air", "SkillTree_Node_Air", "assets/vertical_slice/ui/icons/icon_skill_air.png", "progression:select_skill_air", "空连"),
        ("magic", "SkillTree_Node_Magic", "assets/vertical_slice/ui/icons/icon_skill_fire.png", "progression:select_skill_magic", "魔法"),
        ("support", "SkillTree_Node_Support", "assets/vertical_slice/ui/icons/icon_skill_support.png", "progression:select_skill_support", "支援"),
        ("mobility", "SkillTree_Node_Mobility", "assets/vertical_slice/ui/icons/icon_skill_dash.png", "progression:select_skill_mobility", "机动"),
        ("break", "SkillTree_Node_BreakLimit", "assets/vertical_slice/ui/icons/icon_skill_break.png", "progression:select_skill_break", "断限"),
    ]
    for idx, (key, tag, texture, command, label) in enumerate(node_specs):
        cx, cy = centers[key]
        pos = (cx - node_size[0] * 0.5, cy - node_size[1] * 0.5)
        out += image(970000150 + idx, tag, pos, node_size, texture, 34, 1.0, True, key in {"core", "break"})
        out += overlay_button(970000180 + idx, tag + "_Button", pos, node_size, command)
        out += text(970000210 + idx, tag + "_Label", (pos[0] - 0.006, pos[1] + 0.095), (0.075, 0.026), label, [0.86, 0.98, 0.93, 1], 15, 45, "small")

    out += text(970000030, "SkillTree_Status", (0.095, 0.755), (0.50, 0.075), "魔剑 Lv1 / 技能网络", [0.88, 0.96, 0.92, 1], 17, 42, "small")
    out += text(970000031, "SkillTree_Details", (0.685, 0.275), (0.20, 0.27), "魔剑核心", [0.95, 0.98, 0.92, 1], 19, 42, "body")
    out += text(970000032, "SkillTree_Materials", (0.685, 0.575), (0.20, 0.12), "材料栏", [1.0, 0.92, 0.72, 1], 16, 42, "small")
    out += progress(970000033, "SkillTree_MagicSwordBar", (0.685, 0.715), (0.20, 0.018), 1, 2, [0.35, 0.88, 0.86, 1])
    out += button(970000034, "SkillTree_Button_UpgradeMagicSword", (0.685, 0.755), (0.20, 0.055), "学习选中节点", "progression:learn_selected_skill", 55, "gold")
    out += button(970000035, "SkillTree_Button_Back", (0.085, 0.82), (0.15, 0.05), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    return out


def equipment_scene() -> str:
    out = "Scene: VerticalSliceEquipment\nEntities:\n"
    out += camera(980000001, "Equipment_Camera")
    out += background(980000002, "Equipment_Background", [0.82, 0.80, 0.70, 1])
    out += image(980000010, "Equipment_MainPanel", (0.05, 0.08), (0.90, 0.78), "assets/vertical_slice/ui/panels/panel_equipment.png", 10, 0.97)
    out += text(980000011, "Equipment_Title", (0.085, 0.11), (0.42, 0.06), "装备与背包", [1.0, 0.94, 0.78, 1], 42, 42, "title")
    out += text(980000012, "Equipment_Subtitle", (0.085, 0.176), (0.72, 0.035), "第2章 / 魔剑 Lv1 / 主角 Lv1", [0.90, 0.82, 0.68, 1], 18, 42, "small")
    out += panel(980000013, "Equipment_SlotsPanel", (0.08, 0.25), (0.24, 0.42), [0.06, 0.05, 0.05, 0.62], [0.72, 0.54, 0.28, 0.86], 18)
    out += panel(980000014, "Equipment_BagPanel", (0.35, 0.25), (0.28, 0.42), [0.07, 0.06, 0.05, 0.58], [0.70, 0.58, 0.34, 0.82], 18)
    out += panel(980000015, "Equipment_DetailPanel", (0.67, 0.25), (0.23, 0.42), [0.05, 0.05, 0.06, 0.70], [0.82, 0.62, 0.32, 0.85], 18)
    out += text(980000016, "Equipment_SlotTitle", (0.10, 0.275), (0.18, 0.035), "当前装备", [1.0, 0.92, 0.74, 1], 20, 42, "small")
    out += text(980000017, "Equipment_BagTitle", (0.37, 0.275), (0.20, 0.035), "背包格子", [1.0, 0.92, 0.74, 1], 20, 42, "small")

    slot_positions = [(0.105, 0.335), (0.205, 0.335), (0.105, 0.47), (0.205, 0.47)]
    slot_icons = [
        ("Equipment_SlotArmor", "assets/vertical_slice/ui/icons/icon_equipment_traveler_armor.png", "防具"),
        ("Equipment_SlotRing", "assets/vertical_slice/ui/icons/icon_equipment_magic_ring.png", "饰品"),
        ("Equipment_SlotCharm", "assets/vertical_slice/ui/icons/icon_equipment_beast_tooth.png", "护符"),
        ("Equipment_SlotBoots", "assets/vertical_slice/ui/icons/icon_equipment_wind_boots.png", "足部"),
    ]
    for i, ((x, y), (tag, icon, label)) in enumerate(zip(slot_positions, slot_icons)):
        out += panel(980000100 + i, tag + "_Frame", (x, y), (0.075, 0.10), [0.02, 0.025, 0.03, 0.72], [0.68, 0.58, 0.38, 0.82], 25)
        out += image(980000110 + i, tag, (x + 0.012, y + 0.012), (0.052, 0.076), icon, 34)
        out += text(980000120 + i, tag + "_Label", (x, y + 0.105), (0.075, 0.026), label, [0.90, 0.86, 0.76, 1], 14, 42, "small")

    items = [
        ("traveler_armor", "assets/vertical_slice/ui/icons/icon_equipment_traveler_armor.png"),
        ("black_forest_armor", "assets/vertical_slice/ui/icons/icon_equipment_black_forest_armor.png"),
        ("beast_tooth_pendant", "assets/vertical_slice/ui/icons/icon_equipment_beast_tooth.png"),
        ("novice_magic_ring", "assets/vertical_slice/ui/icons/icon_equipment_magic_ring.png"),
        ("wind_boots", "assets/vertical_slice/ui/icons/icon_equipment_wind_boots.png"),
        ("old_ward_charm", "assets/vertical_slice/ui/icons/icon_equipment_ward_charm.png"),
        ("training_blade", "assets/vertical_slice/ui/icons/icon_equipment_training_blade.png"),
        ("angel_feather", "assets/vertical_slice/ui/icons/icon_equipment_angel_feather.png"),
    ]
    start_x, start_y = 0.375, 0.33
    for i, (item_id, icon) in enumerate(items):
        col = i % 4
        row = i // 4
        x = start_x + col * 0.06
        y = start_y + row * 0.115
        out += panel(980000160 + i, f"Equipment_Item_{i+1}_Frame", (x, y), (0.052, 0.075), [0.025, 0.03, 0.035, 0.78], [0.58, 0.48, 0.31, 0.78], 25)
        out += image(980000180 + i, f"Equipment_Item_{i+1}", (x + 0.004, y + 0.005), (0.044, 0.064), icon, 34)
        out += overlay_button(980000200 + i, f"Equipment_Item_{i+1}_Button", (x, y), (0.052, 0.075), f"progression:select_equipment_{item_id}")

    out += text(980000030, "Equipment_Status", (0.10, 0.61), (0.22, 0.09), "装备页 1 / 2", [0.96, 0.92, 0.82, 1], 17, 42, "small")
    out += text(980000031, "Equipment_PageText", (0.37, 0.60), (0.24, 0.042), "第 1 页", [0.94, 0.90, 0.80, 1], 16, 42, "small")
    out += slider(980000032, "Equipment_PageSlider", (0.37, 0.64), (0.22, 0.035), 1, 1, 2, 44)
    out += button(980000033, "Equipment_Button_Page1", (0.37, 0.70), (0.07, 0.045), "1", "progression:equipment_page_1", 55, "gold")
    out += button(980000034, "Equipment_Button_Page2", (0.455, 0.70), (0.07, 0.045), "2", "progression:equipment_page_2", 55, "dark")
    out += text(980000035, "Equipment_Details", (0.69, 0.29), (0.18, 0.25), "旅人护衣", [0.98, 0.95, 0.86, 1], 19, 42, "body")
    out += text(980000036, "Equipment_Materials", (0.69, 0.57), (0.18, 0.12), "材料栏", [1.0, 0.90, 0.68, 1], 16, 42, "small")
    out += progress(980000037, "Equipment_ArmorBar", (0.69, 0.705), (0.18, 0.018), 0, 1, [0.84, 0.62, 0.30, 1])
    out += button(980000038, "Equipment_Button_UpgradeArmor", (0.69, 0.745), (0.18, 0.052), "强化旅人护衣 +1", "progression:upgrade_traveler_armor", 55, "gold")
    out += button(980000039, "Equipment_Button_Back", (0.085, 0.815), (0.15, 0.05), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    return out


def settings_scene() -> str:
    out = "Scene: VerticalSliceSettings\nEntities:\n"
    out += camera(840000001, "Settings_Camera")
    out += background(840000002, "Settings_Background", [0.76, 0.88, 0.82, 1])
    out += image(840000010, "Settings_MainPanel", (0.055, 0.08), (0.89, 0.78), "assets/vertical_slice/ui/panels/panel_settings.png", 10, 0.97)
    out += text(840000011, "Settings_Title", (0.09, 0.115), (0.44, 0.06), "系统设置", [0.88, 1.0, 0.92, 1], 42, 42, "title")
    out += text(840000012, "Settings_Subtitle", (0.09, 0.18), (0.72, 0.035), "第2章 / 魔剑 Lv1 / 主角 Lv1", [0.70, 0.90, 0.78, 1], 18, 42, "small")
    out += panel(840000013, "Settings_ControlPanel", (0.10, 0.26), (0.54, 0.42), [0.04, 0.07, 0.06, 0.65], [0.40, 0.80, 0.60, 0.85], 18)
    out += text(840000014, "Settings_TextSpeedLabel", (0.13, 0.31), (0.18, 0.035), "文字速度", [0.88, 1.0, 0.90, 1], 20, 42, "small")
    out += slider(840000015, "Settings_TextSpeedSlider", (0.31, 0.315), (0.25, 0.035), 48, 12, 180, 44)
    out += button(840000016, "Settings_Button_TextDown", (0.58, 0.30), (0.045, 0.045), "-", "progression:text_speed_down", 55, "dark")
    out += button(840000017, "Settings_Button_TextUp", (0.635, 0.30), (0.045, 0.045), "+", "progression:text_speed_up", 55, "teal")
    out += text(840000018, "Settings_MasterVolumeLabel", (0.13, 0.39), (0.18, 0.035), "主音量", [0.88, 1.0, 0.90, 1], 20, 42, "small")
    out += slider(840000019, "Settings_MasterVolumeSlider", (0.31, 0.395), (0.25, 0.035), 80, 0, 100, 44)
    out += button(840000020, "Settings_Button_VolumeDown", (0.58, 0.38), (0.045, 0.045), "-", "progression:master_volume_down", 55, "dark")
    out += button(840000021, "Settings_Button_VolumeUp", (0.635, 0.38), (0.045, 0.045), "+", "progression:master_volume_up", 55, "teal")
    out += button(840000022, "Settings_Button_Shake", (0.13, 0.49), (0.22, 0.052), "屏幕震动", "progression:toggle_screen_shake", 55, "blue")
    out += button(840000023, "Settings_Button_Fullscreen", (0.38, 0.49), (0.22, 0.052), "全屏偏好", "progression:toggle_fullscreen", 55, "blue")
    out += text(840000024, "Settings_Status", (0.68, 0.27), (0.20, 0.32), "设置状态", [0.94, 0.98, 0.90, 1], 19, 42, "body")
    out += button(840000025, "Settings_Button_Back", (0.10, 0.765), (0.15, 0.05), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    return out


def save_load_scene() -> str:
    out = "Scene: VerticalSliceSaveLoad\nEntities:\n"
    out += camera(850000001, "SaveLoad_Camera")
    out += background(850000002, "SaveLoad_Background", [0.86, 0.80, 0.70, 1])
    out += image(850000010, "SaveLoad_MainPanel", (0.055, 0.08), (0.89, 0.78), "assets/vertical_slice/ui/panels/panel_save_load.png", 10, 0.97)
    out += image(850000011, "SaveLoad_Icon", (0.78, 0.12), (0.10, 0.145), "assets/vertical_slice/ui/icons/icon_save.png", 35, 1.0, True, True)
    out += text(850000012, "SaveLoad_Title", (0.09, 0.115), (0.44, 0.06), "存档 / 读取", [1.0, 0.94, 0.78, 1], 42, 42, "title")
    out += text(850000013, "SaveLoad_Subtitle", (0.09, 0.18), (0.72, 0.035), "第2章 / 魔剑 Lv1 / 主角 Lv1", [0.92, 0.82, 0.66, 1], 18, 42, "small")
    out += panel(850000014, "SaveLoad_SlotCard_1", (0.10, 0.28), (0.62, 0.15), [0.06, 0.05, 0.04, 0.70], [0.80, 0.62, 0.28, 0.86], 18)
    out += image(850000015, "SaveLoad_SlotIcon_1", (0.125, 0.315), (0.065, 0.095), "assets/vertical_slice/ui/icons/icon_save.png", 34)
    out += text(850000016, "SaveLoad_Status", (0.21, 0.30), (0.46, 0.10), "1 号槽", [0.96, 0.92, 0.82, 1], 20, 42, "body")
    out += button(850000017, "SaveLoad_Button_1", (0.74, 0.30), (0.16, 0.052), "保存 1 号槽", "progression:save_1", 55, "gold")
    out += button(850000018, "SaveLoad_Button_2", (0.74, 0.37), (0.16, 0.052), "读取 1 号槽", "progression:load_1", 55, "teal")
    out += panel(850000019, "SaveLoad_SlotCard_2", (0.10, 0.49), (0.62, 0.12), [0.04, 0.04, 0.045, 0.55], [0.48, 0.42, 0.32, 0.65], 18)
    out += text(850000020, "SaveLoad_EmptySlotText", (0.13, 0.515), (0.54, 0.05), "槽位 2-4 会在正式多槽存档阶段开放。当前竖切先稳定一号槽。", [0.82, 0.78, 0.68, 1], 18, 42, "small")
    out += button(850000021, "SaveLoad_Button_3", (0.10, 0.74), (0.15, 0.052), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    out += button(850000022, "SaveLoad_Button_4", (0.28, 0.74), (0.15, 0.052), "返回标题", "scene:assets/scenes/VisualNovelMainMenu.wt", 55, "dark")
    return out


def append_vn_save_load_panel(path: Path) -> None:
    text_data = path.read_text(encoding="utf-8")
    text_data = text_data.replace("OnClickFunction: vn:savemenumenu", "OnClickFunction: vn:savemenu")
    text_data = text_data.replace("OnClickFunction: vn:loadmenumenu", "OnClickFunction: vn:loadmenu")
    text_data = text_data.replace("OnClickFunction: vn:save\n", "OnClickFunction: vn:savemenu\n")
    text_data = text_data.replace("OnClickFunction: vn:load\n", "OnClickFunction: vn:loadmenu\n")
    marker = "      Tag: VN_SaveLoadPanel"
    marker_index = text_data.find(marker)
    if marker_index != -1:
        block_start = text_data.rfind("\n  - Entity:", 0, marker_index)
        if block_start != -1:
            text_data = text_data[:block_start].rstrip() + "\n"

    base = 930000000 + sum((i + 1) * ord(ch) for i, ch in enumerate(path.name))
    panel_block = ""
    panel_block += panel(base + 1, "VN_SaveLoadPanel", (0.18, 0.15), (0.64, 0.56), [0.04, 0.045, 0.055, 0.88], [0.80, 0.64, 0.36, 0.90], 80, False)
    panel_block += image(base + 2, "VN_SaveLoadIcon", (0.22, 0.20), (0.075, 0.105), "assets/vertical_slice/ui/icons/icon_save.png", 86, 1.0, False)
    panel_block += text(base + 3, "VN_SaveLoadTitle", (0.31, 0.205), (0.30, 0.06), "存档 / 读取", [1.0, 0.94, 0.78, 1], 34, 86, "title", False)
    panel_block += text(base + 4, "VN_SaveLoadText", (0.23, 0.31), (0.52, 0.20), "存档 / 读取", [0.94, 0.96, 0.88, 1], 21, 86, "body", False)
    panel_block += button(base + 5, "VN_SaveLoad_SaveSlot1", (0.25, 0.56), (0.15, 0.055), "保存槽 1", "vn:save", 90, "gold")
    panel_block += button(base + 6, "VN_SaveLoad_LoadSlot1", (0.43, 0.56), (0.15, 0.055), "读取槽 1", "vn:load", 90, "teal")
    panel_block += button(base + 7, "VN_SaveLoad_Close", (0.61, 0.56), (0.12, 0.055), "关闭", "vn:close", 90, "dark")
    path.write_text(text_data.rstrip() + "\n" + panel_block, encoding="utf-8", newline="\n")


def update_manifest() -> None:
    if not MANIFEST_PATH.exists():
        return
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    assets = set(manifest.get("assets", []))
    for path in (UI_ROOT / "icons").glob("icon_skill_*.png"):
        assets.add("assets/" + path.relative_to(ASSET_ROOT).as_posix())
    for path in (UI_ROOT / "icons").glob("icon_equipment_*.png"):
        assets.add("assets/" + path.relative_to(ASSET_ROOT).as_posix())
    manifest["assets"] = sorted(assets)
    manifest["version"] = "0.6.0"
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    generate_assets()
    (SCENE_ROOT / "VerticalSliceSkillTree.wt").write_text(skill_tree_scene(), encoding="utf-8", newline="\n")
    (SCENE_ROOT / "VerticalSliceEquipment.wt").write_text(equipment_scene(), encoding="utf-8", newline="\n")
    (SCENE_ROOT / "VerticalSliceSettings.wt").write_text(settings_scene(), encoding="utf-8", newline="\n")
    (SCENE_ROOT / "VerticalSliceSaveLoad.wt").write_text(save_load_scene(), encoding="utf-8", newline="\n")
    for scene_name in [
        "VisualNovelDemo.wt",
        "VerticalSliceIntro.wt",
        "VerticalSlicePostFake.wt",
        "VerticalSliceChapter3Preview.wt",
    ]:
        append_vn_save_load_panel(SCENE_ROOT / scene_name)
    update_manifest()


if __name__ == "__main__":
    main()
