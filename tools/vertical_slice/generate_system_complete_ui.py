from __future__ import annotations

import json
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


def alpha_canvas(size: tuple[int, int]) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def upscale(image: Image.Image, scale: int = 3) -> Image.Image:
    return image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)


def panel(size: tuple[int, int], body: tuple[int, int, int], border: tuple[int, int, int], glow: tuple[int, int, int]) -> Image.Image:
    image, d = alpha_canvas(size)
    w, h = size
    d.rounded_rectangle((2, 2, w - 3, h - 3), radius=4, fill=(*body, 224), outline=(*border, 246), width=2)
    d.rectangle((6, 5, w - 7, 7), fill=(*glow, 215))
    d.rectangle((6, h - 8, w - 7, h - 6), fill=(0, 0, 0, 70))
    for x in range(12, w - 10, 18):
        d.line((x, 11, x - 8, h - 12), fill=(*glow, 18), width=1)
    return upscale(image, 3)


def icon_circle(base: tuple[int, int, int], accent: tuple[int, int, int], mark: str) -> Image.Image:
    image, d = alpha_canvas((64, 64))
    d.ellipse((5, 5, 59, 59), fill=(*base, 245), outline=(242, 246, 226, 225), width=3)
    d.ellipse((14, 14, 50, 50), fill=(*accent, 218))
    if mark == "heart":
        d.polygon([(32, 47), (16, 28), (20, 17), (30, 18), (32, 24), (35, 18), (45, 17), (49, 28)], fill=(255, 216, 225, 245))
    elif mark == "support":
        d.rectangle((18, 22, 46, 30), fill=(238, 245, 220, 245))
        d.rectangle((26, 14, 38, 50), fill=(238, 245, 220, 245))
    elif mark == "settings":
        for angle in range(0, 360, 45):
            import math
            x = 32 + int(math.cos(math.radians(angle)) * 19)
            y = 32 + int(math.sin(math.radians(angle)) * 19)
            d.rectangle((x - 3, y - 3, x + 3, y + 3), fill=(238, 245, 220, 235))
        d.ellipse((22, 22, 42, 42), fill=(40, 50, 58, 255), outline=(238, 245, 220, 235), width=3)
    elif mark == "save":
        d.rounded_rectangle((16, 12, 48, 52), radius=3, fill=(238, 245, 220, 245))
        d.rectangle((21, 16, 42, 26), fill=(*base, 245))
        d.rectangle((22, 36, 43, 47), fill=(*accent, 220))
    elif mark == "back":
        d.polygon([(20, 32), (38, 16), (38, 26), (50, 26), (50, 38), (38, 38), (38, 48)], fill=(238, 245, 220, 245))
    return upscale(image, 3)


def generate_assets() -> None:
    save(panel((128, 64), (26, 34, 46), (92, 180, 244), (118, 218, 255)), UI_ROOT / "panels" / "panel_dungeon.png")
    save(panel((128, 64), (42, 30, 42), (226, 138, 162), (255, 190, 205)), UI_ROOT / "panels" / "panel_relationship.png")
    save(panel((128, 64), (34, 36, 48), (156, 142, 238), (190, 184, 255)), UI_ROOT / "panels" / "panel_support.png")
    save(panel((128, 64), (28, 34, 36), (128, 220, 178), (176, 250, 210)), UI_ROOT / "panels" / "panel_settings.png")
    save(panel((128, 64), (38, 34, 30), (236, 196, 96), (255, 226, 150)), UI_ROOT / "panels" / "panel_save_load.png")
    save(icon_circle((172, 66, 98), (92, 42, 70), "heart"), UI_ROOT / "icons" / "icon_relationship.png")
    save(icon_circle((83, 78, 177), (48, 52, 114), "support"), UI_ROOT / "icons" / "icon_support.png")
    save(icon_circle((74, 136, 108), (34, 72, 65), "settings"), UI_ROOT / "icons" / "icon_settings.png")
    save(icon_circle((188, 139, 62), (82, 65, 42), "save"), UI_ROOT / "icons" / "icon_save.png")
    save(icon_circle((65, 92, 112), (32, 48, 62), "back"), UI_ROOT / "icons" / "icon_back.png")


def q(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def transform() -> str:
    return """    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
"""


def animator(preset: str = "fade_in", delay: float = 0.0, duration: float = 0.25) -> str:
    return f"""    UIAnimatorComponent:
      Preset: "{preset}"
      PlayOnStart: true
      Loop: false
      Delay: {delay}
      Duration: {duration}
      Amplitude: 0.035
      Speed: 1
      FromOffset: [0, 0.035]
"""


def text_component(text: str, color: list[float], size: int, style: str = "body") -> str:
    if style == "title":
        shadow = [0.02, 0.02, 0.025, 0.86]
        outline = [0.01, 0.01, 0.012, 0.92]
        outline_px = 1.7
        offset = [2.5, 2.5]
    elif style == "button":
        shadow = [0.02, 0.02, 0.025, 0.72]
        outline = [0.01, 0.01, 0.012, 0.82]
        outline_px = 1.2
        offset = [1.8, 1.8]
    else:
        shadow = [0.02, 0.02, 0.025, 0.82]
        outline = [0.01, 0.01, 0.012, 0.88]
        outline_px = 1.35
        offset = [2.0, 2.0]

    return f"""    UITextComponent:
      Text: {q(text)}
      Color: [{', '.join(str(v) for v in color)}]
      FontSize: {size}
      FontPath: {FONT}
      ShadowColor: [{', '.join(str(v) for v in shadow)}]
      ShadowOffset: [{', '.join(str(v) for v in offset)}]
      OutlineColor: [{', '.join(str(v) for v in outline)}]
      OutlineThickness: {outline_px}
"""


def entity_header(entity_id: int, tag: str) -> str:
    return f"""  - Entity: {entity_id}
    TagComponent:
      Tag: {tag}
"""


def camera(entity_id: int, tag: str) -> str:
    return entity_header(entity_id, tag) + """    TransformComponent:
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


def background(entity_id: int, tag: str, texture: str = "assets/vertical_slice/vn/backgrounds/forest_camp.png") -> str:
    return entity_header(entity_id, tag) + f"""    TransformComponent:
      Translation: [0, 0.35, -0.8]
      Rotation: [0, 0, 0]
      Scale: [20, 11.25, 1]
    SpriteRendererComponent:
      Color: [0.88, 0.94, 0.98, 1]
      Texture: {texture}
      TilingFactor: 1
"""


def image(entity_id: int, tag: str, pos: tuple[float, float], size: tuple[float, float], texture: str, sort: int = 10, alpha: float = 0.96, delay: float = 0.0) -> str:
    return entity_header(entity_id, tag) + transform() + f"""    UIWidgetComponent:
      Visible: true
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size[0]}, {size[1]}]
      Rotation: 0
      Anchor: 0
      SortOrder: {sort}
""" + animator("slide_fade_in", delay, 0.34) + f"""    UIImageComponent:
      Color: [1, 1, 1, {alpha}]
      TexturePath: {texture}
"""


def text(entity_id: int, tag: str, pos: tuple[float, float], size_xy: tuple[float, float], body: str, color: list[float], font_size: int, sort: int = 30, style: str = "body", delay: float = 0.0) -> str:
    return entity_header(entity_id, tag) + transform() + f"""    UIWidgetComponent:
      Visible: true
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size_xy[0]}, {size_xy[1]}]
      Rotation: 0
      Anchor: 0
      SortOrder: {sort}
""" + animator("fade_in", delay, 0.25) + text_component(body, color, font_size, style)


def button(entity_id: int, tag: str, pos: tuple[float, float], size_xy: tuple[float, float], label: str, command: str, palette: str = "green") -> str:
    palettes = {
        "green": ([0.13, 0.30, 0.26, 0.94], [0.20, 0.44, 0.38, 0.98], [0.08, 0.20, 0.18, 1], [0.94, 0.98, 0.90, 1]),
        "blue": ([0.20, 0.30, 0.50, 0.94], [0.30, 0.43, 0.70, 0.98], [0.13, 0.20, 0.35, 1], [0.94, 0.98, 1, 1]),
        "gold": ([0.34, 0.25, 0.13, 0.94], [0.52, 0.38, 0.20, 0.98], [0.22, 0.16, 0.09, 1], [1, 0.94, 0.82, 1]),
        "pink": ([0.36, 0.16, 0.26, 0.94], [0.54, 0.24, 0.38, 0.98], [0.24, 0.10, 0.17, 1], [1, 0.92, 0.96, 1]),
        "violet": ([0.27, 0.22, 0.48, 0.94], [0.40, 0.32, 0.68, 0.98], [0.18, 0.14, 0.34, 1], [0.96, 0.94, 1, 1]),
    }
    normal, hover, pressed, color = palettes[palette]
    return entity_header(entity_id, tag) + transform() + f"""    UIWidgetComponent:
      Visible: true
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size_xy[0]}, {size_xy[1]}]
      Rotation: 0
      Anchor: 0
      SortOrder: 40
""" + animator("hover_pulse", 0.0, 0.18) + f"""    UIButtonComponent:
      NormalColor: [{', '.join(str(v) for v in normal)}]
      HoverColor: [{', '.join(str(v) for v in hover)}]
      PressedColor: [{', '.join(str(v) for v in pressed)}]
      OnClickFunction: {command}
""" + text_component(label, color, 23, "button")


def progress(entity_id: int, tag: str, pos: tuple[float, float], size_xy: tuple[float, float], value: float, max_value: float, color: list[float]) -> str:
    return entity_header(entity_id, tag) + transform() + f"""    UIWidgetComponent:
      Visible: true
      Position: [{pos[0]}, {pos[1]}]
      Size: [{size_xy[0]}, {size_xy[1]}]
      Rotation: 0
      Anchor: 0
      SortOrder: 32
    UIProgressBarComponent:
      Value: {value}
      MaxValue: {max_value}
      ForegroundColor: [{', '.join(str(v) for v in color)}]
      BackgroundColor: [0.07, 0.09, 0.11, 0.86]
"""


def scene_header(name: str) -> str:
    return f"Scene: {name}\nEntities:\n"


def common_page(scene_name: str, base: int, prefix: str, title: str, panel_texture: str, icon_texture: str, status_tag: str, subtitle_tag: str, status_placeholder: str, buttons: list[tuple[str, str, str]], secondary_tag: str | None = None, secondary_placeholder: str = "") -> str:
    out = scene_header(scene_name)
    out += camera(base + 1, f"{prefix}_Camera")
    out += background(base + 2, f"{prefix}_Background")
    out += image(base + 10, f"{prefix}_MainPanel", (0.055, 0.08), (0.89, 0.80), panel_texture, 10, 0.96)
    out += image(base + 11, f"{prefix}_Icon", (0.81, 0.12), (0.10, 0.145), icon_texture, 31, 1.0, 0.08)
    out += text(base + 20, f"{prefix}_Title", (0.09, 0.115), (0.55, 0.065), title, [0.94, 0.98, 0.92, 1], 42, 30, "title", 0.06)
    out += text(base + 21, subtitle_tag, (0.09, 0.18), (0.72, 0.045), "第2章 / 魔剑 Lv1 / 主角 Lv1", [0.74, 0.90, 0.84, 1], 20, 30, "body", 0.10)
    out += text(base + 22, status_tag, (0.09, 0.27), (0.58, 0.45), status_placeholder, [0.93, 0.96, 0.90, 1], 20, 30, "body", 0.14)
    if secondary_tag:
        out += text(base + 23, secondary_tag, (0.09, 0.70), (0.58, 0.12), secondary_placeholder, [1.0, 0.92, 0.74, 1], 18, 30, "body", 0.18)

    y = 0.32
    for i, (label, command, palette) in enumerate(buttons):
        out += button(base + 50 + i, f"{prefix}_Button_{i+1}", (0.68, y), (0.23, 0.068), label, command, palette)
        y += 0.085
    return out


def generate_hub() -> str:
    base = 950000000
    out = scene_header("VerticalSliceHub")
    out += camera(base + 1, "Hub_Camera")
    out += background(base + 2, "Hub_Background")
    out += """  - Entity: 950000003
    TagComponent:
      Tag: Hub_Mentor
    TransformComponent:
      Translation: [5.7, -1.22, -0.1]
      Rotation: [0, 0, 0]
      Scale: [2.75, 4.45, 1]
    SpriteRendererComponent:
      Color: [1, 1, 1, 0.95]
      Texture: assets/vertical_slice/vn/portraits/mentor_happy.png
      TilingFactor: 1
"""
    out += image(base + 4, "Hub_MainPanel", (0.045, 0.075), (0.61, 0.41), "assets/vertical_slice/ui/panels/panel_hub.png", 8, 0.95)
    out += image(base + 5, "Hub_NavPanel", (0.065, 0.50), (0.43, 0.435), "assets/vertical_slice/ui/panels/panel_dark.png", 8, 0.94, 0.08)
    out += text(base + 10, "Hub_Title", (0.07, 0.095), (0.62, 0.08), "黑林临时据点", [0.94, 0.98, 0.90, 1], 44, 20, "title", 0.06)
    out += text(base + 11, "Hub_Subtitle", (0.075, 0.18), (0.70, 0.05), "魔剑 Lv1 / 主角 Lv1 / 黑林兽道未解锁", [0.76, 0.90, 0.82, 1], 22, 20, "body", 0.12)
    out += text(base + 12, "Hub_Status", (0.075, 0.275), (0.52, 0.18), "目标、材料和能力会显示在这里。", [0.94, 0.92, 0.82, 1], 20, 20, "body", 0.18)

    buttons = [
        ("副本选择", "scene:assets/scenes/VerticalSliceDungeonSelect.wt", "green", "icon_dungeon.png", "Hub_Button_Dungeon"),
        ("魔剑技能树", "scene:assets/scenes/VerticalSliceSkillTree.wt", "blue", "icon_skill_tree.png", "Hub_Button_Skill"),
        ("装备强化", "scene:assets/scenes/VerticalSliceEquipment.wt", "gold", "icon_armor.png", "Hub_Button_Equip"),
        ("好感关系", "scene:assets/scenes/VerticalSliceRelationship.wt", "pink", "icon_relationship.png", "Hub_Button_Relationship"),
        ("支援配置", "scene:assets/scenes/VerticalSliceSupport.wt", "violet", "icon_support.png", "Hub_Button_Support"),
        ("保存读取", "scene:assets/scenes/VerticalSliceSaveLoad.wt", "gold", "icon_save.png", "Hub_Button_SaveLoad"),
        ("系统设置", "scene:assets/scenes/VerticalSliceSettings.wt", "green", "icon_settings.png", "Hub_Button_Settings"),
        ("继续剧情", "scene:assets/scenes/VerticalSliceChapter3Preview.wt", "pink", "icon_result.png", "Hub_Button_Continue"),
    ]

    y = 0.523
    for i, (label, command, palette, icon_name, tag) in enumerate(buttons):
        out += image(base + 100 + i, f"Hub_Icon_{i+1}", (0.085, y + 0.006), (0.030, 0.046), f"assets/vertical_slice/ui/icons/{icon_name}", 35, 1.0, 0.12 + i * 0.02)
        out += button(base + 120 + i, tag, (0.128, y), (0.30, 0.044), label, command, palette)
        y += 0.048
    return out


def generate_pages() -> None:
    scenes: dict[str, str] = {}
    scenes["VerticalSliceHub.wt"] = generate_hub()
    scenes["VerticalSliceDungeonSelect.wt"] = common_page(
        "VerticalSliceDungeonSelect",
        981000000,
        "Dungeon",
        "副本选择",
        "assets/vertical_slice/ui/panels/panel_dungeon.png",
        "assets/vertical_slice/ui/icons/icon_dungeon.png",
        "Dungeon_Status",
        "Dungeon_Subtitle",
        "副本状态会显示在这里。",
        [
            ("挑战黑熊丈夫", "scene:assets/scenes/SideCombatVerticalSlice.wt", "blue"),
            ("重刷黑林兽道", "scene:assets/scenes/SideCombatBeastPath.wt", "green"),
            ("魔剑技能树", "scene:assets/scenes/VerticalSliceSkillTree.wt", "violet"),
            ("返回据点", "scene:assets/scenes/VerticalSliceHub.wt", "gold"),
        ],
        "Dungeon_Rewards",
        "主要掉落和材料用途会显示在这里。",
    )
    scenes["VerticalSliceRelationship.wt"] = common_page(
        "VerticalSliceRelationship",
        982000000,
        "Relationship",
        "好感与关系",
        "assets/vertical_slice/ui/panels/panel_relationship.png",
        "assets/vertical_slice/ui/icons/icon_relationship.png",
        "Relationship_Status",
        "Relationship_Subtitle",
        "角色好感、加入状态和支援等级会显示在这里。",
        [
            ("打开支援配置", "scene:assets/scenes/VerticalSliceSupport.wt", "violet"),
            ("保存进度", "progression:save_1", "gold"),
            ("返回据点", "scene:assets/scenes/VerticalSliceHub.wt", "green"),
        ],
    )
    scenes["VerticalSliceSupport.wt"] = common_page(
        "VerticalSliceSupport",
        983000000,
        "Support",
        "支援配置",
        "assets/vertical_slice/ui/panels/panel_support.png",
        "assets/vertical_slice/ui/icons/icon_support.png",
        "Support_Status",
        "Support_Subtitle",
        "当前支援槽会显示在这里。",
        [
            ("配置导师支援", "progression:select_support_mentor", "violet"),
            ("白魔法支援", "progression:select_support_white_mage", "green"),
            ("剑盾护卫支援", "progression:select_support_guard", "blue"),
            ("黑魔法支援", "progression:select_support_black_mage", "pink"),
            ("返回据点", "scene:assets/scenes/VerticalSliceHub.wt", "gold"),
        ],
    )
    scenes["VerticalSliceSettings.wt"] = common_page(
        "VerticalSliceSettings",
        984000000,
        "Settings",
        "系统设置",
        "assets/vertical_slice/ui/panels/panel_settings.png",
        "assets/vertical_slice/ui/icons/icon_settings.png",
        "Settings_Status",
        "Settings_Subtitle",
        "系统设置会显示在这里。",
        [
            ("文字速度 -", "progression:text_speed_down", "blue"),
            ("文字速度 +", "progression:text_speed_up", "blue"),
            ("主音量 -", "progression:master_volume_down", "green"),
            ("主音量 +", "progression:master_volume_up", "green"),
            ("屏幕震动开关", "progression:toggle_screen_shake", "violet"),
            ("全屏偏好开关", "progression:toggle_fullscreen", "pink"),
            ("返回据点", "scene:assets/scenes/VerticalSliceHub.wt", "gold"),
        ],
    )
    scenes["VerticalSliceSaveLoad.wt"] = common_page(
        "VerticalSliceSaveLoad",
        985000000,
        "SaveLoad",
        "保存 / 读取",
        "assets/vertical_slice/ui/panels/panel_save_load.png",
        "assets/vertical_slice/ui/icons/icon_save.png",
        "SaveLoad_Status",
        "SaveLoad_Subtitle",
        "当前进度会显示在这里。",
        [
            ("保存到 1 号槽", "progression:save_1", "gold"),
            ("读取 1 号槽", "progression:load_1", "green"),
            ("返回据点", "scene:assets/scenes/VerticalSliceHub.wt", "blue"),
            ("回主菜单", "scene:assets/scenes/VisualNovelMainMenu.wt", "pink"),
        ],
    )

    for filename, content in scenes.items():
        path = SCENE_ROOT / filename
        ensure(path)
        path.write_text(content, encoding="utf-8", newline="\n")

    source = SCENE_ROOT / "SideCombatVerticalSlice.wt"
    beast_path = SCENE_ROOT / "SideCombatBeastPath.wt"
    content = source.read_text(encoding="utf-8")
    content = content.replace("Scene: SideCombatVerticalSlice", "Scene: SideCombatBeastPath", 1)
    content = content.replace("LevelId: CH02_MAIN_BearAwakening", "LevelId: CH02_MAT_BeastPath", 1)
    content = content.replace(
        'FirstClearRewardText: "获得: 魔核碎片 x1 / 兽筋 x2 / 熊爪 x1"',
        'FirstClearRewardText: "获得: 兽筋 x2 / 熊爪 x2 / 魔核碎片 x0-1"'
    )
    beast_path.write_text(content, encoding="utf-8", newline="\n")


def style_existing_text() -> None:
    scene_paths = [
        SCENE_ROOT / "VisualNovelDemo.wt",
        SCENE_ROOT / "VerticalSliceIntro.wt",
        SCENE_ROOT / "VerticalSlicePostFake.wt",
        SCENE_ROOT / "VerticalSliceChapter3Preview.wt",
        SCENE_ROOT / "VisualNovelMainMenu.wt",
        SCENE_ROOT / "SideCombatVerticalSlice.wt",
        SCENE_ROOT / "SideCombatBeastPath.wt",
        SCENE_ROOT / "VerticalSliceResult.wt",
        SCENE_ROOT / "VerticalSliceSkillTree.wt",
        SCENE_ROOT / "VerticalSliceEquipment.wt",
        SCENE_ROOT / "VerticalSliceHub.wt",
        SCENE_ROOT / "VerticalSliceDungeonSelect.wt",
        SCENE_ROOT / "VerticalSliceRelationship.wt",
        SCENE_ROOT / "VerticalSliceSupport.wt",
        SCENE_ROOT / "VerticalSliceSettings.wt",
        SCENE_ROOT / "VerticalSliceSaveLoad.wt",
    ]

    for path in scene_paths:
        if not path.exists():
            continue

        lines = path.read_text(encoding="utf-8").splitlines()
        out: list[str] = []
        current_tag = ""
        for idx, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("Tag: "):
                current_tag = stripped[5:]

            if current_tag == "VN_BodyText" and stripped == "FontSize: 26":
                line = line.replace("26", "30")
            elif current_tag == "VN_SpeakerText" and stripped == "FontSize: 32":
                line = line.replace("32", "34")
            elif current_tag == "VN_AdvanceHint" and stripped == "FontSize: 17":
                line = line.replace("17", "18")

            out.append(line)

            if stripped.startswith("FontPath:") and "ShadowColor:" not in "\n".join(lines[idx + 1:idx + 7]):
                if current_tag == "VN_BodyText":
                    out.extend([
                        "      ShadowColor: [0.02, 0.02, 0.025, 0.90]",
                        "      ShadowOffset: [2.2, 2.2]",
                        "      OutlineColor: [0.01, 0.01, 0.012, 0.94]",
                        "      OutlineThickness: 1.8",
                    ])
                elif "Title" in current_tag:
                    out.extend([
                        "      ShadowColor: [0.02, 0.02, 0.025, 0.86]",
                        "      ShadowOffset: [2.5, 2.5]",
                        "      OutlineColor: [0.01, 0.01, 0.012, 0.92]",
                        "      OutlineThickness: 1.7",
                    ])
                else:
                    out.extend([
                        "      ShadowColor: [0.02, 0.02, 0.025, 0.78]",
                        "      ShadowOffset: [2.0, 2.0]",
                        "      OutlineColor: [0.01, 0.01, 0.012, 0.86]",
                        "      OutlineThickness: 1.25",
                    ])

        path.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")


def update_manifest() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    flow = manifest.setdefault("flow", [])
    for scene in [
        "assets/scenes/SideCombatBeastPath.wt",
        "assets/scenes/VerticalSliceDungeonSelect.wt",
        "assets/scenes/VerticalSliceRelationship.wt",
        "assets/scenes/VerticalSliceSupport.wt",
        "assets/scenes/VerticalSliceSettings.wt",
        "assets/scenes/VerticalSliceSaveLoad.wt",
    ]:
        if scene not in flow:
            flow.append(scene)

    ui = manifest.setdefault("ui", {})
    panels = ui.setdefault("panels", [])
    for panel_path in [
        "assets/vertical_slice/ui/panels/panel_dungeon.png",
        "assets/vertical_slice/ui/panels/panel_relationship.png",
        "assets/vertical_slice/ui/panels/panel_support.png",
        "assets/vertical_slice/ui/panels/panel_settings.png",
        "assets/vertical_slice/ui/panels/panel_save_load.png",
    ]:
        if panel_path not in panels:
            panels.append(panel_path)

    icons = ui.setdefault("icons", [])
    for icon_path in [
        "assets/vertical_slice/ui/icons/icon_relationship.png",
        "assets/vertical_slice/ui/icons/icon_support.png",
        "assets/vertical_slice/ui/icons/icon_settings.png",
        "assets/vertical_slice/ui/icons/icon_save.png",
        "assets/vertical_slice/ui/icons/icon_back.png",
    ]:
        if icon_path not in icons:
            icons.append(icon_path)

    manifest["version"] = "0.5.0"
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    generate_assets()
    generate_pages()
    style_existing_text()
    update_manifest()


if __name__ == "__main__":
    main()
