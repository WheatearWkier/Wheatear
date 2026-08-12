from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets"
SCENE_ROOT = ASSET_ROOT / "scenes"
UI_ROOT = ASSET_ROOT / "vertical_slice" / "ui"
SC_UI_ROOT = ASSET_ROOT / "vertical_slice" / "side_combat" / "ui"
MANIFEST_PATH = ASSET_ROOT / "vertical_slice" / "data" / "vertical_slice_manifest.json"
FONT = "assets/fonts/wqy-microhei.ttc"


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


def q(text_value: str) -> str:
    return '"' + text_value.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def safe_id(node_id: str) -> str:
    return node_id.replace("-", "_").replace(":", "_").lower()


def skill_nodes() -> list[dict[str, object]]:
    nodes: list[dict[str, object]] = [
        {"id": "magic_sword_core", "parent": "", "pos": (0.50, 0.50)},
        {"id": "magic_sword_lv2", "parent": "magic_sword_core", "pos": (0.50, 0.36)},
    ]

    def add_branch(prefix: str, base_degrees: float, curve_degrees: float) -> None:
        parent = "magic_sword_core"
        for i in range(1, 13):
            t = (i - 1) / 11.0
            radius = 0.11 + 0.045 * i
            ring_offset = 0.014 if i % 3 == 0 else (-0.006 if i % 3 == 1 else 0.006)
            angle = math.radians(base_degrees + curve_degrees * t)
            x = 0.50 + math.cos(angle) * (radius + ring_offset)
            y = 0.50 + math.sin(angle) * (radius + ring_offset)
            node_id = f"{prefix}-{i:02d}"
            nodes.append({"id": node_id, "parent": parent, "pos": (x, y)})
            parent = node_id

    add_branch("ME", -90.0, 50.0)
    add_branch("MA", -18.0, 52.0)
    add_branch("FU", 54.0, 52.0)
    add_branch("MO", 126.0, 52.0)
    add_branch("LI", 198.0, 50.0)
    return nodes


def skill_icon_file(node_id: str) -> str:
    if node_id in {"magic_sword_core", "magic_sword_lv2"}:
        return "skill_magic_sword_core.png"

    branch = node_id.split("-", 1)[0].lower()
    if branch in {"me", "ma", "fu", "mo", "li"}:
        return f"skill_{branch}.png"

    return f"skill_{safe_id(node_id)}.png"


def framed_skill_icon(node_id: str, name: str) -> Image.Image:
    branch = node_id.split("-")[0] if "-" in node_id else "CORE"
    palettes = {
        "CORE": ((26, 72, 86), (42, 228, 236), (246, 255, 240)),
        "ME": ((54, 78, 72), (58, 210, 190), (245, 255, 240)),
        "MA": ((78, 54, 86), (220, 100, 226), (255, 235, 252)),
        "FU": ((72, 58, 42), (238, 184, 78), (255, 244, 210)),
        "MO": ((48, 82, 78), (72, 218, 152), (232, 255, 232)),
        "LI": ((82, 66, 42), (248, 218, 86), (255, 248, 214)),
    }
    base, accent, light = palettes.get(branch, palettes["CORE"])
    image, d = canvas((64, 64))
    d.rounded_rectangle((4, 4, 60, 60), radius=7, fill=(*base, 246), outline=(*light, 224), width=3)
    d.rounded_rectangle((10, 10, 54, 54), radius=4, fill=(12, 20, 24, 180), outline=(*accent, 180), width=2)

    if branch == "CORE":
        d.polygon([(32, 8), (48, 23), (43, 46), (32, 58), (21, 46), (16, 23)], fill=(*accent, 245), outline=(*light, 230))
        d.polygon([(32, 18), (39, 28), (36, 43), (32, 49), (28, 43), (25, 28)], fill=(248, 255, 255, 245))
    elif branch == "ME":
        d.line((17, 48, 48, 16), fill=(*light, 250), width=6)
        d.line((26, 51, 51, 25), fill=(*accent, 220), width=3)
        d.polygon([(16, 47), (24, 55), (13, 57)], fill=(*accent, 230))
    elif branch == "MA":
        d.ellipse((17, 17, 47, 47), fill=(*accent, 230), outline=(*light, 220), width=3)
        d.arc((9, 14, 55, 50), 25, 325, fill=(*light, 220), width=2)
        d.ellipse((28, 28, 36, 36), fill=(255, 255, 255, 245))
    elif branch == "FU":
        d.line((19, 45, 45, 19), fill=(*light, 245), width=5)
        d.ellipse((15, 15, 33, 33), outline=(*accent, 240), width=4)
        d.arc((25, 24, 51, 50), 210, 60, fill=(*accent, 230), width=4)
    elif branch == "MO":
        for y in (21, 32, 43):
            d.line((13, y, 36, y), fill=(*light, 235), width=4)
        d.polygon([(36, 15), (52, 32), (36, 49)], fill=(*accent, 240))
    elif branch == "LI":
        d.polygon([(12, 33), (25, 11), (30, 29), (43, 12), (36, 34), (52, 31), (37, 45), (41, 55), (25, 43)], fill=(*accent, 245))
        d.line((19, 52, 45, 12), fill=(22, 30, 34, 220), width=3)
    return upscale(image)


def combat_skill_icon(kind: str) -> Image.Image:
    specs = {
        "j_slash": ((40, 82, 78), (68, 220, 204)),
        "k_launcher": ((52, 72, 104), (76, 172, 238)),
        "sj_uppercut": ((44, 78, 92), (72, 220, 238)),
        "u_magic": ((84, 54, 96), (224, 116, 238)),
        "i_support": ((84, 64, 44), (238, 204, 98)),
        "l_break": ((88, 76, 42), (255, 225, 82)),
    }
    base, accent = specs[kind]
    image, d = canvas((64, 64))
    d.rounded_rectangle((5, 5, 59, 59), radius=8, fill=(*base, 248), outline=(238, 246, 226, 230), width=3)
    d.rounded_rectangle((11, 11, 53, 53), radius=5, fill=(8, 16, 20, 170))
    if kind == "j_slash":
        d.line((17, 49, 47, 17), fill=(240, 255, 246, 250), width=6)
        d.line((24, 52, 51, 25), fill=(*accent, 225), width=3)
    elif kind == "k_launcher":
        d.arc((15, 12, 51, 54), 205, 325, fill=(240, 255, 246, 250), width=6)
        d.polygon([(43, 13), (52, 27), (37, 25)], fill=(*accent, 245))
    elif kind == "sj_uppercut":
        d.line((22, 50, 44, 19), fill=(240, 255, 246, 250), width=6)
        d.line((18, 41, 38, 13), fill=(*accent, 235), width=3)
        d.arc((14, 16, 50, 55), 210, 328, fill=(*accent, 238), width=5)
        d.polygon([(43, 14), (53, 28), (37, 25)], fill=(244, 255, 250, 250))
    elif kind == "u_magic":
        d.ellipse((18, 18, 46, 46), fill=(*accent, 230), outline=(250, 248, 255, 230), width=3)
        d.arc((11, 15, 53, 49), 20, 330, fill=(250, 248, 255, 210), width=2)
    elif kind == "i_support":
        d.ellipse((18, 14, 46, 42), outline=(250, 250, 222, 245), width=5)
        d.rectangle((29, 10, 35, 52), fill=(*accent, 245))
        d.rectangle((19, 27, 45, 33), fill=(*accent, 245))
    elif kind == "l_break":
        d.polygon([(12, 32), (26, 12), (31, 29), (43, 13), (36, 34), (52, 31), (37, 46), (40, 55), (25, 43)], fill=(*accent, 250))
        d.line((20, 52, 45, 12), fill=(35, 38, 42, 230), width=3)
    return upscale(image)


def generate_assets() -> None:
    for node_id in ["magic_sword_core", "ME-01", "MA-01", "FU-01", "MO-01", "LI-01"]:
        save(framed_skill_icon(node_id, node_id), UI_ROOT / "skill_tree" / skill_icon_file(node_id))

    lock, d = canvas((64, 64))
    d.rounded_rectangle((5, 5, 59, 59), radius=7, fill=(8, 10, 12, 145))
    d.rectangle((18, 30, 46, 50), fill=(216, 222, 220, 220))
    d.arc((22, 15, 42, 39), 180, 360, fill=(216, 222, 220, 220), width=5)
    d.rectangle((30, 37, 34, 45), fill=(48, 54, 58, 220))
    save(upscale(lock), UI_ROOT / "skill_tree" / "skill_lock_overlay.png")

    sel, d = canvas((64, 64))
    d.rounded_rectangle((3, 3, 61, 61), radius=8, outline=(255, 220, 96, 245), width=5)
    d.rounded_rectangle((9, 9, 55, 55), radius=5, outline=(112, 240, 226, 210), width=2)
    save(upscale(sel), UI_ROOT / "skill_tree" / "skill_selected_frame.png")

    skill_icon_files = {
        "j_slash": "icon_skill_basic_slash.png",
        "k_launcher": "icon_skill_launcher_slash.png",
        "sj_uppercut": "icon_skill_uppercut.png",
        "u_magic": "icon_skill_magic_bolt.png",
        "i_support": "icon_skill_ally_support.png",
        "l_break": "icon_skill_break_limit.png",
    }
    for kind, filename in skill_icon_files.items():
        save(combat_skill_icon(kind), SC_UI_ROOT / filename)


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


def background(entity_id: int, tag: str, color: list[float], texture: str = "assets/vertical_slice/vn/backgrounds/bg_forest_camp_night.png") -> str:
    return entity(entity_id, tag) + f"""    TransformComponent:
      Translation: [0, 0.35, -0.8]
      Rotation: [0, 0, 0]
      Scale: [20, 11.25, 1]
    SpriteRendererComponent:
      Color: [{', '.join(str(v) for v in color)}]
      Texture: {texture}
      TilingFactor: 1
"""


def widget(pos: tuple[float, float], size: tuple[float, float], sort: int, anchor: int = 0, rotation: float = 0.0) -> str:
    return f"""    UIWidgetComponent:
      Visible: true
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size[0]}, {size[1]}]
      Rotation: {rotation}
      Anchor: {anchor}
      SortOrder: {sort}
      ParentEntity: 0
"""


def text_component(value: str, color: list[float], font_size: int, style: str = "body") -> str:
    if style == "title":
        shadow = [0.01, 0.015, 0.018, 0.90]
        outline = [0.0, 0.0, 0.0, 0.94]
        thickness = 1.8
        offset = [2.4, 2.4]
    elif style == "button":
        shadow = [0.01, 0.015, 0.018, 0.78]
        outline = [0.0, 0.0, 0.0, 0.86]
        thickness = 1.2
        offset = [1.6, 1.6]
    else:
        shadow = [0.01, 0.015, 0.018, 0.82]
        outline = [0.0, 0.0, 0.0, 0.86]
        thickness = 1.1
        offset = [1.6, 1.6]
    return f"""    UITextComponent:
      Text: {q(value)}
      Color: [{', '.join(str(v) for v in color)}]
      FontSize: {font_size}
      FontPath: {FONT}
      ShadowColor: [{', '.join(str(v) for v in shadow)}]
      ShadowOffset: [{', '.join(str(v) for v in offset)}]
      OutlineColor: [{', '.join(str(v) for v in outline)}]
      OutlineThickness: {thickness}
"""


def text(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], value: str, color: list[float], font_size: int, sort: int = 42, style: str = "body") -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + text_component(value, color, font_size, style)


def panel(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], bg: list[float], border: list[float], sort: int = 18, border_width: float = 2.0, anchor: int = 0, rotation: float = 0.0) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, anchor, rotation) + f"""    UIPanelComponent:
      BackgroundColor: [{', '.join(str(v) for v in bg)}]
      BorderColor: [{', '.join(str(v) for v in border)}]
      BorderThickness: {border_width}
"""


def image(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], texture_path: str, sort: int = 34, alpha: float = 1.0, anchor: int = 0) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, anchor) + f"""    UIImageComponent:
      Color: [1, 1, 1, {alpha}]
      TexturePath: {texture_path}
"""


def button(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], label: str, command: str, sort: int = 55, palette: str = "dark") -> str:
    colors = {
        "dark": ([0.10, 0.12, 0.14, 0.90], [0.20, 0.24, 0.28, 0.96], [0.07, 0.08, 0.10, 0.96], [0.90, 0.96, 0.94, 1]),
        "gold": ([0.74, 0.52, 0.20, 0.94], [0.92, 0.72, 0.30, 0.96], [0.52, 0.35, 0.14, 0.96], [1.0, 0.94, 0.78, 1]),
        "teal": ([0.16, 0.46, 0.48, 0.94], [0.22, 0.66, 0.68, 0.96], [0.10, 0.30, 0.34, 0.96], [0.86, 1.0, 0.96, 1]),
    }
    normal, hover, pressed, text_color = colors[palette]
    label_pos = (pos[0] + 0.006, pos[1] + size[1] * 0.22)
    return entity(entity_id, tag, ) + transform() + widget(pos, size, sort) + f"""    UIButtonComponent:
      NormalColor: [{', '.join(str(v) for v in normal)}]
      HoverColor: [{', '.join(str(v) for v in hover)}]
      PressedColor: [{', '.join(str(v) for v in pressed)}]
      OnClickFunction: {q(command)}
""" + text_component(label, text_color, 18, "button").replace("    UITextComponent:", "    UITextComponent:")


def overlay_button(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], command: str, sort: int = 60) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + f"""    UIButtonComponent:
      NormalColor: [0, 0, 0, 0]
      HoverColor: [0.95, 1, 0.82, 0.14]
      PressedColor: [1, 0.90, 0.45, 0.24]
      OnClickFunction: {q(command)}
"""


def progress(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], value: float, max_value: float, fg: list[float], bg: list[float], sort: int = 44, anchor: int = 0) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort, anchor) + f"""    UIProgressBarComponent:
      Value: {value}
      MaxValue: {max_value}
      ForegroundColor: [{', '.join(str(v) for v in fg)}]
      BackgroundColor: [{', '.join(str(v) for v in bg)}]
"""


def slider(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], value: float, min_value: float, max_value: float, command: str, sort: int = 48) -> str:
    return entity(entity_id, tag) + transform() + widget(pos, size, sort) + f"""    UISliderComponent:
      Value: {value}
      MinValue: {min_value}
      MaxValue: {max_value}
      TrackColor: [0.08, 0.10, 0.12, 0.92]
      FillColor: [0.30, 0.78, 0.72, 0.96]
      HandleColor: [0.92, 0.98, 0.92, 1]
      HoverColor: [1, 0.92, 0.50, 1]
      OnValueChangedFunction: {q(command)}
"""


def skill_tree_view_component(nodes: list[dict[str, object]]) -> str:
    out = """    UISkillTreeViewComponent:
      Pan: [0, 0]
      MinPan: [-0.46, -0.46]
      MaxPan: [0.46, 0.46]
      NodeSize: [0.076, 0.104]
      NodeEdgeInset: 0.05
      LineThickness: 0.0095
      CurveAmount: 0.045
      CommandPrefix: "progression:select_skill_node:"
      BackgroundColor: [0.01, 0.018, 0.02, 0.22]
      GridColor: [0.13, 0.27, 0.24, 0.22]
      LineColor: [0.18, 0.34, 0.30, 0.58]
      ActiveLineColor: [0.38, 0.96, 0.72, 0.82]
      NodeColor: [0.08, 0.12, 0.12, 0.95]
      LockedNodeColor: [0.05, 0.06, 0.065, 0.90]
      HoverNodeColor: [0.42, 0.86, 0.72, 0.96]
      SelectedNodeColor: [1.0, 0.82, 0.38, 0.98]
      CoreNodeColor: [0.18, 0.42, 0.40, 0.98]
      LockColor: [0.0, 0.0, 0.0, 0.54]
      SelectedNodeId: "magic_sword_core"
      Nodes:
"""
    for node in nodes:
        node_id = str(node["id"])
        parent = str(node["parent"])
        x, y = node["pos"]  # type: ignore[misc]
        out += f"""        - Id: {q(node_id)}
          ParentId: {q(parent)}
          Position: [{x:.6f}, {y:.6f}]
          IconPath: "assets/vertical_slice/ui/skill_tree/{skill_icon_file(node_id)}"
          Learned: {str(node_id == "magic_sword_core").lower()}
          Available: true
          Selected: {str(node_id == "magic_sword_core").lower()}
          Locked: {str(node_id != "magic_sword_core").lower()}
"""
    return out


def skill_tree_scene() -> str:
    nodes = skill_nodes()
    out = "Scene: VerticalSliceSkillTree\nEntities:\n"
    out += camera(970000001, "SkillTree_Camera")
    out += background(970000002, "SkillTree_Background", [0.72, 0.84, 0.92, 1])
    out += panel(970000010, "SkillTree_MainPanel", (0.04, 0.07), (0.92, 0.80), [0.035, 0.047, 0.058, 0.88], [0.26, 0.66, 0.72, 0.92], 10, 2.0)
    out += text(970000011, "SkillTree_Title", (0.075, 0.105), (0.46, 0.06), "魔剑技能树", [0.90, 0.99, 1.0, 1], 42, 42, "title")
    out += text(970000012, "SkillTree_Subtitle", (0.075, 0.17), (0.70, 0.035), "第 2 章 / 魔剑 Lv1 / 主角 Lv1", [0.70, 0.90, 0.92, 1], 18, 42, "body")
    out += panel(970000013, "SkillTree_View", (0.06, 0.215), (0.61, 0.57), [0.03, 0.045, 0.052, 0.76], [0.18, 0.58, 0.62, 0.88], 18, 2.0)
    out += skill_tree_view_component(nodes)
    out += panel(970000014, "SkillTree_DetailPanel", (0.70, 0.215), (0.235, 0.57), [0.04, 0.046, 0.056, 0.84], [0.56, 0.72, 0.76, 0.88], 18, 2.0)
    out += text(970000015, "SkillTree_DragHint", (0.075, 0.735), (0.52, 0.03), "拖动画布浏览完整技能树；点击节点查看详情", [0.70, 0.88, 0.88, 1], 16, 42, "body")

    out += text(970000030, "SkillTree_Status", (0.085, 0.792), (0.56, 0.07), "魔剑 Lv1 / 完整技能网", [0.88, 0.98, 0.92, 1], 16, 42, "body")
    out += text(970000031, "SkillTree_Details", (0.725, 0.255), (0.18, 0.30), "魔剑核心", [0.96, 0.99, 0.94, 1], 18, 42, "body")
    out += text(970000032, "SkillTree_Materials", (0.725, 0.590), (0.18, 0.11), "材料", [1.0, 0.93, 0.72, 1], 15, 42, "body")
    out += progress(970000033, "SkillTree_MagicSwordBar", (0.725, 0.725), (0.18, 0.018), 1, 2, [0.35, 0.88, 0.86, 1], [0.05, 0.08, 0.10, 0.92])
    out += button(970000034, "SkillTree_Button_LearnSelectedSkill", (0.725, 0.755), (0.18, 0.052), "学习选中节点", "progression:learn_selected_skill", 55, "gold")
    out += button(970000035, "SkillTree_Button_Back", (0.075, 0.825), (0.15, 0.05), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    return out


def equipment_scene() -> str:
    out = "Scene: VerticalSliceEquipment\nEntities:\n"
    out += camera(980000001, "Equipment_Camera")
    out += background(980000002, "Equipment_Background", [0.82, 0.80, 0.70, 1])
    out += panel(980000010, "Equipment_MainPanel", (0.05, 0.08), (0.90, 0.78), [0.06, 0.052, 0.045, 0.88], [0.82, 0.62, 0.32, 0.90], 10)
    out += text(980000011, "Equipment_Title", (0.085, 0.11), (0.42, 0.06), "装备与背包", [1.0, 0.94, 0.78, 1], 42, 42, "title")
    out += text(980000012, "Equipment_Subtitle", (0.085, 0.176), (0.72, 0.035), "第 2 章 / 魔剑 Lv1 / 主角 Lv1", [0.90, 0.82, 0.68, 1], 18, 42, "body")
    out += panel(980000013, "Equipment_SlotsPanel", (0.08, 0.25), (0.24, 0.42), [0.06, 0.05, 0.05, 0.64], [0.72, 0.54, 0.28, 0.86], 18)
    out += panel(980000014, "Equipment_BagPanel", (0.35, 0.25), (0.28, 0.42), [0.07, 0.06, 0.05, 0.62], [0.70, 0.58, 0.34, 0.84], 18)
    out += panel(980000015, "Equipment_DetailPanel", (0.67, 0.25), (0.23, 0.42), [0.05, 0.05, 0.06, 0.76], [0.82, 0.62, 0.32, 0.86], 18)
    out += text(980000016, "Equipment_SlotTitle", (0.10, 0.275), (0.18, 0.035), "当前装备", [1.0, 0.92, 0.74, 1], 20, 42, "body")
    out += text(980000017, "Equipment_BagTitle", (0.37, 0.275), (0.20, 0.035), "背包分页", [1.0, 0.92, 0.74, 1], 20, 42, "body")

    slot_icons = [
        ("Equipment_SlotArmor", "assets/vertical_slice/ui/icons/icon_equipment_traveler_armor.png", "防具"),
        ("Equipment_SlotRing", "assets/vertical_slice/ui/icons/icon_equipment_magic_ring.png", "饰品"),
        ("Equipment_SlotCharm", "assets/vertical_slice/ui/icons/icon_equipment_beast_tooth.png", "护符"),
        ("Equipment_SlotBoots", "assets/vertical_slice/ui/icons/icon_equipment_wind_boots.png", "足部"),
    ]
    for i, (tag, icon_path, label) in enumerate(slot_icons):
        x = 0.105 + (i % 2) * 0.10
        y = 0.335 + (i // 2) * 0.135
        out += panel(980000100 + i, tag + "_Frame", (x, y), (0.075, 0.10), [0.025, 0.03, 0.035, 0.78], [0.68, 0.58, 0.38, 0.82], 25)
        out += image(980000110 + i, tag, (x + 0.010, y + 0.011), (0.055, 0.075), icon_path, 34)
        out += text(980000120 + i, tag + "_Label", (x, y + 0.105), (0.075, 0.026), label, [0.90, 0.86, 0.76, 1], 14, 42, "body")

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
    for i, (item_id, icon_path) in enumerate(items, 1):
        slot = (i - 1) % 4
        x = 0.385 + (slot % 2) * 0.105
        y = 0.335 + (slot // 2) * 0.135
        out += panel(980000160 + i, f"Equipment_Item_{i}_Frame", (x, y), (0.075, 0.098), [0.025, 0.03, 0.035, 0.78], [0.58, 0.48, 0.31, 0.78], 25)
        out += image(980000180 + i, f"Equipment_Item_{i}", (x + 0.010, y + 0.011), (0.055, 0.075), icon_path, 34)
        out += overlay_button(980000200 + i, f"Equipment_Item_{i}_Button", (x, y), (0.075, 0.098), f"progression:select_equipment_{item_id}", 58)

    out += text(980000030, "Equipment_Status", (0.10, 0.61), (0.22, 0.09), "装备页 1 / 2", [0.96, 0.92, 0.82, 1], 17, 42, "body")
    out += text(980000031, "Equipment_PageText", (0.37, 0.60), (0.24, 0.042), "第 1 页", [0.94, 0.90, 0.80, 1], 16, 42, "body")
    out += slider(980000032, "Equipment_PageSlider", (0.37, 0.64), (0.22, 0.035), 1, 1, 2, "progression:equipment_page_slider", 44)
    out += button(980000033, "Equipment_Button_Page1", (0.37, 0.70), (0.07, 0.045), "1", "progression:equipment_page_1", 55, "gold")
    out += button(980000034, "Equipment_Button_Page2", (0.455, 0.70), (0.07, 0.045), "2", "progression:equipment_page_2", 55, "dark")
    out += text(980000035, "Equipment_Details", (0.69, 0.29), (0.18, 0.25), "旅人护衣", [0.98, 0.95, 0.86, 1], 19, 42, "body")
    out += text(980000036, "Equipment_Materials", (0.69, 0.57), (0.18, 0.12), "材料", [1.0, 0.90, 0.68, 1], 16, 42, "body")
    out += progress(980000037, "Equipment_ArmorBar", (0.69, 0.705), (0.18, 0.018), 0, 1, [0.84, 0.62, 0.30, 1], [0.06, 0.05, 0.05, 0.90])
    out += button(980000038, "Equipment_Button_UpgradeArmor", (0.69, 0.745), (0.18, 0.052), "强化旅人护衣 +1", "progression:upgrade_traveler_armor", 55, "gold")
    out += button(980000039, "Equipment_Button_Back", (0.085, 0.815), (0.15, 0.05), "返回据点", "scene:assets/scenes/VerticalSliceHub.wt", 55, "dark")
    return out


def skill_bar_entities(base_id: int) -> str:
    specs = [
        ("SJ", "S+J", "icon_skill_uppercut.png"),
        ("U", "U", "icon_skill_magic_bolt.png"),
        ("I", "I", "icon_skill_ally_support.png"),
        ("L", "L", "icon_skill_break_limit.png"),
    ]
    out = ""
    out += panel(base_id, "SC_SkillBarPanel", (0.580, 0.830), (0.330, 0.128), [0.03, 0.045, 0.055, 0.72], [0.20, 0.72, 0.78, 0.70], 50, 1.5)
    for i, (key, label, icon_name) in enumerate(specs):
        x = 0.602 + i * 0.074
        y = 0.848
        out += panel(base_id + 1 + i * 10, f"SC_SkillSlot_{key}", (x, y), (0.058, 0.083), [0.025, 0.032, 0.038, 0.88], [0.32, 0.70, 0.76, 0.78], 51, 1.5)
        out += image(base_id + 2 + i * 10, f"SC_SkillIcon_{key}", (x + 0.006, y + 0.006), (0.046, 0.061), f"assets/vertical_slice/side_combat/ui/{icon_name}", 52) + """    UIButtonComponent:
      NormalColor: [0, 0, 0, 0]
      HoverColor: [0.95, 1, 0.82, 0.14]
      PressedColor: [1, 0.90, 0.45, 0.24]
      OnClickFunction: ""
"""
        out += progress(base_id + 3 + i * 10, f"SC_SkillCooldown_{key}", (x + 0.006, y + 0.006), (0.046, 0.061), 0, 1, [0.02, 0.02, 0.025, 0.68], [0.02, 0.02, 0.025, 0.28], 54)
        out += text(base_id + 4 + i * 10, f"SC_SkillCooldownText_{key}", (x + 0.005, y + 0.024), (0.048, 0.030), "", [1, 1, 1, 1], 16, 57, "button")
        out += text(base_id + 5 + i * 10, f"SC_SkillKey_{key}", (x + 0.006, y + 0.066), (0.046, 0.020), label, [0.90, 0.98, 1, 1], 14, 57, "button")

    out += f"""  - Entity: {base_id + 900}
    TagComponent:
      Tag: SC_SkillTooltipPanel
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UIWidgetComponent:
      Visible: false
      Position: [0.580, 0.725]
      Size: [0.225, 0.090]
      Rotation: 0.0
      Anchor: 0
      SortOrder: 76
      ParentEntity: 0
    UIPanelComponent:
      BackgroundColor: [0.025, 0.032, 0.038, 0.92]
      BorderColor: [0.28, 0.78, 0.82, 0.82]
      BorderThickness: 1.25
  - Entity: {base_id + 901}
    TagComponent:
      Tag: SC_SkillTooltipText
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UIWidgetComponent:
      Visible: false
      Position: [0.592, 0.735]
      Size: [0.201, 0.070]
      Rotation: 0.0
      Anchor: 0
      SortOrder: 77
      ParentEntity: 0
    UITextComponent:
      Text: ""
      Color: [0.92, 0.98, 0.96, 1]
      FontSize: 15
      FontPath: {FONT}
      ShadowColor: [0.01, 0.015, 0.018, 0.82]
      ShadowOffset: [1.6, 1.6]
      OutlineColor: [0.0, 0.0, 0.0, 0.86]
      OutlineThickness: 1.1
"""

    item_slot_specs = [
        ("1", "assets/vertical_slice/side_combat/ui/items/icon_item_heal_potion.png", "1"),
        ("2", "assets/vertical_slice/side_combat/ui/items/icon_item_focus_vial.png", "2"),
        ("3", "assets/vertical_slice/side_combat/ui/items/icon_item_burst_bomb.png", "3"),
    ]
    for i, (key, icon_path, count) in enumerate(item_slot_specs):
        x = 0.040 + i * 0.058
        y = 0.807
        out += panel(base_id + 700 + i * 10, f"SC_ItemSlot_{key}_Frame", (x, y), (0.050, 0.066), [0.025, 0.032, 0.038, 0.82], [0.70, 0.58, 0.30, 0.78], 58, 1.2)
        out += image(base_id + 701 + i * 10, f"SC_ItemSlot_{key}_Icon", (x + 0.007, y + 0.006), (0.036, 0.046), icon_path, 59)
        out += overlay_button(base_id + 702 + i * 10, f"SC_ItemSlot_{key}_Button", (x, y), (0.050, 0.066), "", 61)
        out += text(base_id + 703 + i * 10, f"SC_ItemSlot_{key}_Count", (x + 0.016, y + 0.048), (0.032, 0.018), count, [1.0, 0.92, 0.68, 1], 13, 62, "button")
    return out


def result_reward_entities(base_id: int) -> str:
    specs = [
        ("Core", "assets/vertical_slice/side_combat/ui/icon_drop_magic_core.png", "x1"),
        ("Sinew", "assets/vertical_slice/side_combat/ui/icon_drop_beast_sinew.png", "x2"),
        ("Claw", "assets/vertical_slice/side_combat/ui/icon_drop_beast_claw.png", "x1"),
    ]
    out = ""
    for i, (key, icon_path, count) in enumerate(specs):
        x = 0.110 + i * 0.085
        y = 0.735
        out += panel(base_id + i * 10, f"Result_Drop_{key}_Frame", (x, y), (0.066, 0.090), [0.025, 0.032, 0.038, 0.84], [0.74, 0.60, 0.30, 0.82], 35, 1.4)
        out += image(base_id + 1 + i * 10, f"Result_Drop_{key}_Icon", (x + 0.010, y + 0.009), (0.046, 0.060), icon_path, 38)
        out += overlay_button(base_id + 2 + i * 10, f"Result_Drop_{key}_Button", (x, y), (0.066, 0.090), "", 48)
        out += text(base_id + 3 + i * 10, f"Result_Drop_{key}_Count", (x + 0.026, y + 0.067), (0.038, 0.020), count, [1.0, 0.92, 0.68, 1], 15, 50, "button")

    out += f"""  - Entity: {base_id + 90}
    TagComponent:
      Tag: Result_DropTooltipPanel
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UIWidgetComponent:
      Visible: false
      Position: [0.105, 0.615]
      Size: [0.265, 0.120]
      Rotation: 0.0
      Anchor: 0
      SortOrder: 78
      ParentEntity: 0
    UIPanelComponent:
      BackgroundColor: [0.025, 0.032, 0.038, 0.94]
      BorderColor: [0.74, 0.60, 0.30, 0.86]
      BorderThickness: 1.25
  - Entity: {base_id + 91}
    TagComponent:
      Tag: Result_DropTooltipText
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UIWidgetComponent:
      Visible: false
      Position: [0.117, 0.625]
      Size: [0.241, 0.100]
      Rotation: 0.0
      Anchor: 0
      SortOrder: 79
      ParentEntity: 0
    UITextComponent:
      Text: ""
      Color: [0.94, 0.98, 0.92, 1]
      FontSize: 16
      FontPath: {FONT}
      ShadowColor: [0.01, 0.015, 0.018, 0.82]
      ShadowOffset: [1.6, 1.6]
      OutlineColor: [0.0, 0.0, 0.0, 0.86]
      OutlineThickness: 1.1
"""
    return out


def patch_result_scene(path: Path) -> None:
    text_value = path.read_text(encoding="utf-8")
    prefixes = ("Result_Drop_", "Result_DropTooltip")
    blocks = text_value.split("  - Entity:")
    kept = [blocks[0]]
    for block in blocks[1:]:
        tag = ""
        for line in block.splitlines():
            stripped = line.strip()
            if stripped.startswith("Tag: "):
                tag = stripped[5:]
                break
        if any(tag.startswith(prefix) for prefix in prefixes):
            continue

        if tag == "Result_Rewards":
            block = block.replace("Position: [0.105, 0.725]", "Position: [0.355, 0.725]")
            block = block.replace("Size: [0.48, 0.105]", "Size: [0.245, 0.105]")
            block = block.replace(
                'Text: "获得: 魔核碎片 x1 / 兽筋 x2 / 熊爪 x1\\n下一步: 回据点升级，或重刷继续练习空连。"',
                'Text: "掉落奖励\\n悬浮图标查看用途和背包数量。\\n下一步: 回据点升级或重刷练连招。"')
        kept.append("  - Entity:" + block)

    path.write_text("".join(kept).rstrip() + "\n" + result_reward_entities(960000100), encoding="utf-8", newline="\n")


def remove_generated_blocks(scene_text: str) -> str:
    prefixes = (
        "SC_SkillBarPanel",
        "SC_SkillSlot_",
        "SC_SkillIcon_",
        "SC_SkillCooldown_",
        "SC_SkillCooldownText_",
        "SC_SkillKey_",
        "SC_SkillTooltip",
        "SC_ItemSlot_",
    )
    blocks = scene_text.split("  - Entity:")
    kept = [blocks[0]]
    for block in blocks[1:]:
        tag = ""
        for line in block.splitlines():
            stripped = line.strip()
            if stripped.startswith("Tag: "):
                tag = stripped[5:]
                break
        if any(tag.startswith(prefix) for prefix in prefixes):
            continue
        kept.append("  - Entity:" + block)
    return "".join(kept)


def patch_side_combat_scene(path: Path, base_id: int) -> None:
    text_value = path.read_text(encoding="utf-8")
    text_value = text_value.replace(
        'Text: "A/D推进  W/S纵深  Space跳  J三斩  K上挑  U魔法  I支援"',
        'Text: "A/D移动  W/S纵深  K跳跃  J斩击  S+J上挑  U魔法  I支援"')
    text_value = text_value.replace(
        'Text: "K 0.0  U 0.0  I 0.0"',
        'Text: "魔剑槽 3.0/3  空中动作 3"')
    text_value = text_value.replace(
        'Text: "掉落: 魔核碎片 / 兽筋 / 熊爪"',
        'Text: "主要掉落"')
    text_value = remove_generated_blocks(text_value).rstrip() + "\n" + skill_bar_entities(base_id)
    path.write_text(text_value, encoding="utf-8", newline="\n")


def patch_settings_scene(path: Path) -> None:
    text_value = path.read_text(encoding="utf-8")
    text_value = text_value.replace('Tag: Settings_TextSpeedSlider', 'Tag: Settings_TextSpeedSlider')
    lines = text_value.splitlines()
    out: list[str] = []
    current_tag = ""
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("Tag: "):
            current_tag = stripped[5:]
        if current_tag == "Settings_TextSpeedSlider" and stripped.startswith("OnValueChangedFunction:"):
            line = '      OnValueChangedFunction: "progression:set_text_speed"'
        elif current_tag == "Settings_MasterVolumeSlider" and stripped.startswith("OnValueChangedFunction:"):
            line = '      OnValueChangedFunction: "progression:set_master_volume"'
        out.append(line)
    path.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")


def update_manifest() -> None:
    if not MANIFEST_PATH.exists():
        return
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    manifest["interactive_ui_revision"] = "2026-07-28-skillbar-tree-pages"
    generated = set(manifest.get("generated_assets", []))
    for path in [
        "assets/scenes/VerticalSliceSkillTree.wt",
        "assets/scenes/VerticalSliceEquipment.wt",
        "assets/vertical_slice/ui/skill_tree/skill_magic_sword_core.png",
        "assets/vertical_slice/ui/skill_tree/skill_me.png",
        "assets/vertical_slice/ui/skill_tree/skill_ma.png",
        "assets/vertical_slice/ui/skill_tree/skill_fu.png",
        "assets/vertical_slice/ui/skill_tree/skill_mo.png",
        "assets/vertical_slice/ui/skill_tree/skill_li.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_basic_slash.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_launcher_slash.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_uppercut.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_magic_bolt.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_ally_support.png",
        "assets/vertical_slice/side_combat/ui/icon_skill_break_limit.png",
    ]:
        generated.add(path)
    manifest["generated_assets"] = sorted(generated)
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")


def main() -> None:
    generate_assets()
    ensure(SCENE_ROOT / "VerticalSliceSkillTree.wt")
    (SCENE_ROOT / "VerticalSliceSkillTree.wt").write_text(skill_tree_scene(), encoding="utf-8", newline="\n")
    (SCENE_ROOT / "VerticalSliceEquipment.wt").write_text(equipment_scene(), encoding="utf-8", newline="\n")
    patch_settings_scene(SCENE_ROOT / "VerticalSliceSettings.wt")
    patch_side_combat_scene(SCENE_ROOT / "SideCombatBeastPath.wt", 946200000)
    patch_result_scene(SCENE_ROOT / "VerticalSliceResult.wt")
    update_manifest()


if __name__ == "__main__":
    main()
