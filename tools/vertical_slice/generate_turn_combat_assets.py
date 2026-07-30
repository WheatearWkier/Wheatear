from __future__ import annotations

import math
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice" / "turn_combat"
CHAR_ROOT = ASSET_ROOT / "characters"
AUDIO_ROOT = ASSET_ROOT / "audio"

CHARACTERS = {
    "protag_magic_swordsman": ((42, 207, 220), (23, 45, 72), True),
    "ally_white_mage": ((142, 235, 218), (232, 242, 240), True),
    "ally_guardian": ((232, 202, 112), (54, 79, 96), True),
    "en_bearling": ((139, 82, 46), (68, 38, 28), False),
    "en_wolf": ((120, 164, 180), (43, 63, 78), False),
    "en_apprentice_mage": ((152, 88, 216), (45, 33, 78), False),
}


def ensure_dirs() -> None:
    CHAR_ROOT.mkdir(parents=True, exist_ok=True)
    AUDIO_ROOT.mkdir(parents=True, exist_ok=True)


def fallback_base(name: str, accent: tuple[int, int, int], body: tuple[int, int, int], player_side: bool) -> Image.Image:
    image = Image.new("RGBA", (128, 160), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    cx = 62 if player_side else 68
    draw.ellipse((cx - 15, 14, cx + 15, 44), fill=(232, 178, 146, 255))
    draw.rectangle((cx - 18, 48, cx + 18, 112), fill=body + (255,))
    draw.rectangle((cx - 22, 112, cx - 8, 145), fill=(20, 36, 58, 255))
    draw.rectangle((cx + 8, 112, cx + 22, 145), fill=(20, 36, 58, 255))
    if player_side:
        draw.polygon([(cx + 28, 58), (cx + 54, 18), (cx + 62, 26), (cx + 36, 80)], fill=accent + (255,))
        draw.line((cx + 33, 76, cx + 60, 22), fill=(222, 255, 255, 255), width=4)
    else:
        draw.polygon([(cx - 30, 66), (cx - 60, 42), (cx - 42, 84)], fill=accent + (255,))
    draw.ellipse((cx - 24, 146, cx + 24, 153), fill=(16, 24, 30, 150))
    return image


def load_base(name: str, accent: tuple[int, int, int], body: tuple[int, int, int], player_side: bool) -> Image.Image:
    path = CHAR_ROOT / f"{name}.png"
    if path.exists():
        return Image.open(path).convert("RGBA")

    image = fallback_base(name, accent, body, player_side)
    image.save(path)
    return image


def shifted(image: Image.Image, dx: int = 0, dy: int = 0) -> Image.Image:
    out = Image.new("RGBA", image.size, (0, 0, 0, 0))
    out.alpha_composite(image, (dx, dy))
    return out


def tinted(image: Image.Image, color: tuple[int, int, int], amount: float) -> Image.Image:
    overlay = Image.new("RGBA", image.size, color + (0,))
    overlay.putalpha(image.getchannel("A"))
    return Image.blend(image, overlay, amount)


def add_glow(image: Image.Image, color: tuple[int, int, int], radius: int = 3) -> Image.Image:
    alpha = image.getchannel("A")
    glow = Image.new("RGBA", image.size, color + (0,))
    glow.putalpha(alpha.filter(ImageFilter.GaussianBlur(radius)))
    return Image.alpha_composite(glow, image)


def save_idle_frames(name: str, base: Image.Image, accent: tuple[int, int, int]) -> None:
    offsets = [0, -2, 0, 1]
    for index, dy in enumerate(offsets, 1):
        frame = shifted(base, 0, dy)
        if index == 2:
            frame = add_glow(frame, accent, 2)
        frame.save(CHAR_ROOT / f"{name}_idle_{index:02d}.png")


def save_attack_frames(name: str, base: Image.Image, accent: tuple[int, int, int], player_side: bool) -> None:
    direction = 1 if player_side else -1
    for index in range(1, 5):
        frame = shifted(base, direction * (index * 3 - 3), -1 if index == 2 else 0)
        draw = ImageDraw.Draw(frame)
        if player_side:
            x0 = 74 + index * 5
            draw.line((x0, 28, x0 + 32, 90), fill=accent + (210,), width=5)
            draw.line((x0 - 8, 32, x0 + 24, 92), fill=(232, 255, 255, 180), width=2)
        else:
            x0 = 48 - index * 5
            draw.arc((x0 - 28, 38, x0 + 46, 108), 205, 330, fill=accent + (220,), width=6)
        frame = add_glow(frame, accent, 2)
        frame.save(CHAR_ROOT / f"{name}_attack_{index:02d}.png")


def save_hit_frames(name: str, base: Image.Image, player_side: bool) -> None:
    direction = -1 if player_side else 1
    for index in range(1, 4):
        frame = shifted(base, direction * index * 3, 0)
        color = (255, 238, 210) if index == 1 else (255, 88, 66)
        frame = tinted(frame, color, 0.38)
        frame.save(CHAR_ROOT / f"{name}_hit_{index:02d}.png")


def save_down_frames(name: str, base: Image.Image, player_side: bool) -> None:
    angle = -72 if player_side else 72
    for index in range(1, 4):
        rotated = base.rotate(angle, resample=Image.Resampling.BICUBIC, expand=False)
        frame = shifted(rotated, -10 if player_side else 10, 22 + index * 2)
        frame = tinted(frame, (96, 112, 124), 0.35)
        frame.save(CHAR_ROOT / f"{name}_down_{index:02d}.png")


def generate_character_frames() -> None:
    for name, (accent, body, player_side) in CHARACTERS.items():
        base = load_base(name, accent, body, player_side)
        save_idle_frames(name, base, accent)
        save_attack_frames(name, base, accent, player_side)
        save_hit_frames(name, base, player_side)
        save_down_frames(name, base, player_side)


def write_wave(path: Path, samples: list[float], sample_rate: int = 44100) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        for sample in samples:
            value = max(-1.0, min(1.0, sample))
            output.writeframesraw(int(value * 32767).to_bytes(2, "little", signed=True))


def sine(freq: float, t: float) -> float:
    return math.sin(2.0 * math.pi * freq * t)


def generate_tone(duration: float, freqs: list[float], volume: float = 0.18, sample_rate: int = 44100) -> list[float]:
    count = int(duration * sample_rate)
    samples: list[float] = []
    for i in range(count):
        t = i / sample_rate
        env = min(1.0, t / 0.01) * max(0.0, 1.0 - t / duration)
        value = sum(sine(freq, t) for freq in freqs) / max(len(freqs), 1)
        samples.append(value * volume * env)
    return samples


def generate_bgm(duration: float = 18.0, sample_rate: int = 44100) -> list[float]:
    chords = [
        (146.83, 220.00, 293.66),
        (164.81, 246.94, 329.63),
        (130.81, 196.00, 261.63),
        (174.61, 261.63, 349.23),
    ]
    count = int(duration * sample_rate)
    samples: list[float] = []
    for i in range(count):
        t = i / sample_rate
        chord = chords[int(t / 2.25) % len(chords)]
        beat = 0.72 + 0.28 * max(0.0, sine(1.75, t))
        value = (
            sine(chord[0], t) * 0.40
            + sine(chord[1], t) * 0.26
            + sine(chord[2], t) * 0.20
            + sine(chord[1] * 2.0, t) * 0.08
        )
        samples.append(value * 0.13 * beat)
    return samples


def generate_audio() -> None:
    write_wave(AUDIO_ROOT / "turn_battle_bgm.wav", generate_bgm())
    write_wave(AUDIO_ROOT / "turn_slash.wav", generate_tone(0.22, [540.0, 980.0], 0.22))
    write_wave(AUDIO_ROOT / "turn_hit.wav", generate_tone(0.18, [140.0, 210.0, 320.0], 0.24))
    write_wave(AUDIO_ROOT / "turn_magic.wav", generate_tone(0.36, [330.0, 660.0, 990.0], 0.20))
    write_wave(AUDIO_ROOT / "turn_heal.wav", generate_tone(0.42, [392.0, 523.25, 783.99], 0.17))
    write_wave(AUDIO_ROOT / "turn_guard.wav", generate_tone(0.28, [196.0, 246.94], 0.18))


def main() -> None:
    ensure_dirs()
    generate_character_frames()
    generate_audio()
    print(f"Generated turn combat animation/audio assets under {ASSET_ROOT}")


if __name__ == "__main__":
    main()
