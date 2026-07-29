from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCENE_ROOT = ROOT / "WheatearEditor" / "assets" / "scenes"


VN_SCENES = [
    "VerticalSliceIntro.wt",
    "VerticalSlicePostFake.wt",
    "VerticalSliceChapter3Preview.wt",
    "VisualNovelDemo.wt",
]


def animator_block(preset: str, delay: float, duration: float, amplitude: float, speed: float, offset: tuple[float, float], loop: bool = False) -> list[str]:
    return [
        "    UIAnimatorComponent:\n",
        f"      Preset: \"{preset}\"\n",
        "      PlayOnStart: true\n",
        f"      Loop: {'true' if loop else 'false'}\n",
        f"      Delay: {delay:.2f}\n",
        f"      Duration: {duration:.2f}\n",
        f"      Amplitude: {amplitude:.3f}\n",
        f"      Speed: {speed:.2f}\n",
        f"      FromOffset: [{offset[0]:.3f}, {offset[1]:.3f}]\n",
    ]


def add_animators(path: Path, tag_presets: dict[str, list[str]]) -> bool:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    output: list[str] = []
    active_tag: str | None = None
    in_widget = False
    changed = False

    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("Tag: "):
            active_tag = stripped[5:]
            in_widget = False

        output.append(line)

        if active_tag in tag_presets and stripped == "UIWidgetComponent:":
            in_widget = True
            continue

        if active_tag in tag_presets and in_widget and stripped.startswith("SortOrder:"):
            lookahead = "".join(lines[index + 1:index + 4])
            if "UIAnimatorComponent:" not in lookahead:
                output.extend(tag_presets[active_tag])
                changed = True
            in_widget = False

    if changed:
        path.write_text("".join(output), encoding="utf-8", newline="")
    return changed


def main() -> None:
    vn_presets = {
        "VN_CommandBar": animator_block("slide_fade_in", 0.10, 0.30, 0.035, 1.0, (0.0, 0.035)),
        "VN_HistoryPanel": animator_block("slide_fade_in", 0.00, 0.24, 0.035, 1.0, (0.0, 0.035)),
        "VN_SettingsPanel": animator_block("slide_fade_in", 0.00, 0.24, 0.035, 1.0, (0.0, 0.035)),
    }

    for tag in [
        "VN_Command_Skip",
        "VN_Command_Auto",
        "VN_Command_Save",
        "VN_Command_Load",
        "VN_Command_History",
        "VN_Command_Settings",
        "VN_Command_Hide",
        "VN_HistoryClose",
        "VN_SettingsClose",
    ]:
        vn_presets[tag] = animator_block("hover_pulse", 0.0, 0.18, 0.035, 1.0, (0.0, 0.0))

    for name in VN_SCENES:
        add_animators(SCENE_ROOT / name, vn_presets)

    menu_presets = {
        "Menu_Title": animator_block("fade_in", 0.06, 0.28, 0.035, 1.0, (0.0, 0.0)),
        "Menu_Subtitle": animator_block("fade_in", 0.12, 0.28, 0.035, 1.0, (0.0, 0.0)),
        "Menu_NewGame": animator_block("hover_pulse", 0.0, 0.18, 0.040, 1.0, (0.0, 0.0)),
        "Menu_LoadGame": animator_block("hover_pulse", 0.0, 0.18, 0.040, 1.0, (0.0, 0.0)),
        "Menu_Quit": animator_block("hover_pulse", 0.0, 0.18, 0.040, 1.0, (0.0, 0.0)),
    }
    add_animators(SCENE_ROOT / "VisualNovelMainMenu.wt", menu_presets)


if __name__ == "__main__":
    main()
