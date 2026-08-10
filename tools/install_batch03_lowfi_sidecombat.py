from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"
SOURCE_ROOT = ASSET_ROOT / "source_frames"
DEST_ROOT = ASSET_ROOT / "side_combat" / "characters"
PREVIEW_ROOT = ASSET_ROOT / "side_combat" / "previews" / "protag_batch03_lowfi"
BACKUP_ROOT = DEST_ROOT / "_backup_before_batch03_lowfi"

PROFILE_PIVOTS = {
    "body_512": (256, 430),
    "body_tall_640": (256, 548),
    "body_tall_768": (256, 680),
    "slash_640": (320, 430),
    "slash_heavy_768": (384, 430),
    "vertical_640": (320, 548),
    "vertical_tall_768": (320, 680),
    "dash_768": (384, 430),
    "dash_tall_768": (384, 548),
    "dash_1024": (512, 430),
    "floor_1024": (512, 430),
}

PROFILE_SIZES = {
    "body_512": (512, 512),
    "body_tall_640": (512, 640),
    "body_tall_768": (512, 768),
    "slash_640": (640, 512),
    "slash_heavy_768": (768, 512),
    "vertical_640": (640, 640),
    "vertical_tall_768": (640, 768),
    "dash_768": (768, 512),
    "dash_tall_768": (768, 640),
    "dash_1024": (1024, 512),
    "floor_1024": (1024, 512),
}

SOURCE_CLIPS = [
    ("idle", "idle", 8, "body_512"),
    ("run", "run", 10, "body_512"),
    ("jump_start", "jump_start", 4, "body_512"),
    ("jump_start", "jump", 4, "body_512"),
    ("jump_loop", "jump_loop", 4, "body_tall_640"),
    ("fall", "fall", 4, "body_tall_640"),
    ("land", "land", 4, "body_512"),
    ("basic1", "basic1", 7, "slash_640"),
    ("basic2", "basic2", 7, "slash_640"),
    ("basic3", "basic3", 9, "dash_1024"),
    ("air_basic", "air_basic", 7, "vertical_640"),
    ("launcher", "launcher", 9, "vertical_tall_768"),
    ("air_chase", "air_chase", 8, "dash_tall_768"),
    ("magic_bolt", "magic_bolt", 9, "body_512"),
    ("ally_support", "ally_support", 8, "slash_640"),
    ("break_limit", "break_limit", 12, "dash_1024"),
    ("hurt", "hurt", 5, "body_512"),
    ("hurt", "hit", 5, "body_512"),
    ("launched", "launched", 4, "body_tall_640"),
    ("knockdown", "knockdown", 5, "floor_1024"),
    ("recover", "recover", 6, "body_tall_640"),
    ("dead", "dead", 8, "floor_1024"),
]


def backup_existing() -> int:
    if BACKUP_ROOT.exists():
        return 0

    BACKUP_ROOT.mkdir(parents=True, exist_ok=True)
    count = 0
    for path in sorted(DEST_ROOT.glob("protag*.png")):
        shutil.copy2(path, BACKUP_ROOT / path.name)
        count += 1
    return count


def load_source_frame(action: str, index: int) -> Image.Image:
    path = SOURCE_ROOT / f"protag_{action}_lowfi" / f"protag_{action}_lowfi_{index:03}.png"
    if not path.exists():
        raise FileNotFoundError(path)
    return Image.open(path).convert("RGBA")


def bake_runtime_frame(source: Image.Image, profile: str) -> Image.Image:
    expected_size = PROFILE_SIZES[profile]
    if source.size != expected_size:
        raise ValueError(f"{profile}: source size {source.size} != {expected_size}")
    return source.copy()


def save_clip(source_action: str, dest_action: str, count: int, profile: str) -> list[Path]:
    written: list[Path] = []
    for index in range(count):
        source = load_source_frame(source_action, index)
        frame = bake_runtime_frame(source, profile)
        path = DEST_ROOT / f"protag_{dest_action}_{index + 1:02}.png"
        frame.save(path)
        written.append(path)
    return written


def make_contact_sheet(paths: list[Path]) -> Path:
    PREVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    sample_names = [
        "idle_01", "run_03", "basic1_04", "basic2_04", "basic3_06",
        "launcher_05", "air_chase_04", "magic_bolt_05", "hurt_03", "dead_06",
    ]
    samples = []
    for name in sample_names:
        path = DEST_ROOT / f"protag_{name}.png"
        if path.exists():
            samples.append(path)

    thumb_w, thumb_h = 128, 128
    label_h = 18
    sheet = Image.new("RGBA", (thumb_w * len(samples), thumb_h + label_h), (18, 20, 26, 255))
    draw = ImageDraw.Draw(sheet)
    for i, path in enumerate(samples):
        image = Image.open(path).convert("RGBA").resize((thumb_w, thumb_h), Image.Resampling.NEAREST)
        x = i * thumb_w
        sheet.alpha_composite(image, (x, 0))
        draw.text((x + 4, thumb_h + 2), path.stem.replace("protag_", ""), fill=(220, 226, 238, 255))

    out = PREVIEW_ROOT / "protag_batch03_lowfi_runtime_contact_sheet.png"
    sheet.save(out)
    return out


def validate(paths: list[Path]) -> list[str]:
    warnings: list[str] = []
    for path in paths:
        image = Image.open(path).convert("RGBA")
        expected = None
        if expected is None:
            for source_action, dest_action, _, profile in sorted(SOURCE_CLIPS, key=lambda item: len(item[1]), reverse=True):
                if path.name.startswith(f"protag_{dest_action}_"):
                    expected = PROFILE_SIZES[profile]
                    break
        if expected and image.size != expected:
            warnings.append(f"{path.name}: size {image.size} != {expected}")
        alpha = image.getchannel("A")
        if alpha.getpixel((0, 0)) != 0 or alpha.getpixel((image.width - 1, 0)) != 0:
            warnings.append(f"{path.name}: non-transparent top corner")
        bbox = alpha.getbbox()
        if not bbox:
            warnings.append(f"{path.name}: empty alpha")
        elif bbox[0] <= 0 or bbox[1] <= 0 or bbox[2] >= image.width or bbox[3] >= image.height:
            warnings.append(f"{path.name}: alpha touches canvas edge {bbox}")
    return warnings


def main() -> None:
    copied = backup_existing()
    all_written: list[Path] = []
    for source_action, dest_action, count, profile in SOURCE_CLIPS:
        all_written.extend(save_clip(source_action, dest_action, count, profile))
    preview = make_contact_sheet(all_written)
    warnings = validate(all_written)

    print(f"Backed up old protag PNGs: {copied}")
    print(f"Runtime frames written: {len(all_written)}")
    print("Runtime canvases: per-clip profile sizes; no downscale bake")
    print(f"Profile pivots: {PROFILE_PIVOTS}")
    print(f"Preview: {preview}")
    if warnings:
        print("Warnings:")
        for warning in warnings:
            print(f"  - {warning}")
    else:
        print("Validation: all runtime frames are RGBA PNGs with transparent corners and fixed canvas.")


if __name__ == "__main__":
    main()
