from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT / "WheatearEditor" / "assets" / "vertical_slice"
BGM_ROOT = ASSET_ROOT / "audio" / "bgm"
ITEM_ROOT = ASSET_ROOT / "side_combat" / "ui" / "items"
SAMPLE_RATE = 44100


def ensure(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def midi_to_hz(note: int) -> float:
    return 440.0 * (2.0 ** ((note - 69) / 12.0))


def sine(phase: float) -> float:
    return math.sin(phase * math.tau)


def tri(phase: float) -> float:
    return 2.0 * abs(2.0 * (phase - math.floor(phase + 0.5))) - 1.0


def soft_clip(value: float) -> float:
    return math.tanh(value * 1.35)


def write_wav(path: Path, samples: list[float]) -> None:
    ensure(path)
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SAMPLE_RATE)
        for sample in samples:
            value = max(-1.0, min(1.0, sample))
            out.writeframes(struct.pack("<h", int(value * 32767.0)))


def synth_track(
    path: Path,
    seconds: float,
    bpm: float,
    chords: list[tuple[int, int, int]],
    melody: list[int | None],
    *,
    pad_gain: float,
    bell_gain: float,
    bass_gain: float,
    pulse_gain: float = 0.0,
) -> None:
    total = int(seconds * SAMPLE_RATE)
    samples: list[float] = []
    beat_length = 60.0 / bpm
    step_length = beat_length * 0.5

    for i in range(total):
        t = i / SAMPLE_RATE
        beat = t / beat_length
        chord = chords[int(beat // 4) % len(chords)]
        step = int(t / step_length)
        local_step_time = t - step * step_length
        melody_note = melody[step % len(melody)]

        fade = min(1.0, t / 0.10, (seconds - t) / 0.10)
        slow = 0.72 + 0.28 * sine(t * 0.11)

        value = 0.0
        for index, note in enumerate(chord):
            hz = midi_to_hz(note)
            value += pad_gain * slow * tri(t * hz + index * 0.11)
            value += pad_gain * 0.35 * sine(t * hz * 2.0 + index * 0.23)

        bass_hz = midi_to_hz(chord[0] - 24)
        value += bass_gain * sine(t * bass_hz)

        if melody_note is not None:
            attack = min(1.0, local_step_time / 0.025)
            decay = math.exp(-local_step_time * 4.2)
            bell_env = attack * decay
            hz = midi_to_hz(melody_note)
            value += bell_gain * bell_env * (sine(t * hz) + 0.45 * sine(t * hz * 2.01))

        if pulse_gain > 0.0:
            pulse_env = math.exp(-(t % beat_length) * 5.4)
            value += pulse_gain * pulse_env * sine(t * midi_to_hz(chord[0] - 12))

        samples.append(soft_clip(value) * fade)

    write_wav(path, samples)


def pixel_canvas(size: int = 64) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def save_icon(image: Image.Image, path: Path) -> None:
    ensure(path)
    image.save(path)


def draw_heal_potion() -> Image.Image:
    image, d = pixel_canvas()
    d.ellipse((15, 42, 49, 54), fill=(0, 0, 0, 80))
    d.rectangle((27, 7, 37, 16), fill=(182, 214, 214, 255))
    d.rectangle((24, 13, 40, 20), fill=(232, 248, 245, 255))
    d.polygon([(19, 20), (45, 20), (50, 45), (39, 55), (24, 55), (14, 45)], fill=(158, 36, 49, 255))
    d.polygon([(20, 22), (44, 22), (47, 35), (17, 35)], fill=(237, 78, 79, 255))
    d.rectangle((28, 31, 36, 47), fill=(255, 220, 218, 255))
    d.rectangle((24, 35, 40, 43), fill=(255, 220, 218, 255))
    d.line((23, 23, 18, 43), fill=(255, 165, 151, 255), width=2)
    d.line((42, 24, 47, 43), fill=(91, 24, 34, 255), width=2)
    d.rectangle((26, 5, 38, 9), fill=(91, 126, 132, 255))
    return image


def draw_focus_vial() -> Image.Image:
    image, d = pixel_canvas()
    d.ellipse((14, 44, 50, 55), fill=(0, 0, 0, 74))
    d.rectangle((25, 6, 39, 13), fill=(173, 210, 230, 255))
    d.rectangle((22, 12, 42, 18), fill=(225, 245, 255, 255))
    d.rounded_rectangle((18, 18, 46, 54), radius=6, fill=(31, 89, 150, 255), outline=(188, 238, 255, 255), width=2)
    d.polygon([(21, 31), (43, 24), (43, 47), (21, 49)], fill=(59, 202, 235, 220))
    for y in (30, 38, 46):
        d.line((23, y, 41, y - 5), fill=(125, 241, 255, 160), width=1)
    d.ellipse((25, 25, 31, 31), fill=(220, 252, 255, 230))
    return image


def draw_burst_bomb() -> Image.Image:
    image, d = pixel_canvas()
    d.ellipse((13, 44, 52, 55), fill=(0, 0, 0, 78))
    d.ellipse((16, 23, 48, 55), fill=(39, 48, 56, 255), outline=(144, 180, 173, 255), width=2)
    d.polygon([(30, 17), (36, 17), (39, 25), (27, 25)], fill=(93, 120, 112, 255))
    d.line((36, 17, 48, 7), fill=(244, 185, 86, 255), width=2)
    d.line((47, 7, 52, 4), fill=(255, 240, 132, 255), width=2)
    d.polygon([(48, 4), (58, 6), (52, 12)], fill=(255, 118, 66, 255))
    d.ellipse((23, 30, 32, 39), fill=(70, 230, 209, 220))
    d.arc((19, 27, 44, 51), 205, 316, fill=(95, 255, 229, 170), width=2)
    d.rectangle((26, 36, 38, 40), fill=(18, 23, 28, 110))
    return image


def main() -> None:
    synth_track(
        BGM_ROOT / "vn_school_morning.wav",
        16.0,
        92.0,
        [(60, 64, 67), (67, 71, 74), (69, 72, 76), (65, 69, 72)],
        [72, None, 76, None, 74, None, 72, 67],
        pad_gain=0.020,
        bell_gain=0.070,
        bass_gain=0.025,
    )
    synth_track(
        BGM_ROOT / "vn_unease.wav",
        14.0,
        78.0,
        [(50, 53, 57), (49, 52, 56), (47, 50, 53), (48, 52, 55)],
        [62, None, 61, None, 57, None, 56, None],
        pad_gain=0.018,
        bell_gain=0.032,
        bass_gain=0.040,
        pulse_gain=0.018,
    )
    synth_track(
        BGM_ROOT / "vn_isekai_forest.wav",
        18.0,
        86.0,
        [(52, 55, 59), (57, 60, 64), (59, 62, 66), (55, 59, 62)],
        [71, 74, None, 76, 74, None, 71, 67],
        pad_gain=0.019,
        bell_gain=0.066,
        bass_gain=0.026,
    )
    synth_track(
        BGM_ROOT / "vn_abduction.wav",
        14.0,
        104.0,
        [(54, 57, 61), (53, 56, 60), (50, 54, 57), (49, 53, 56)],
        [66, 65, None, 61, 66, 68, None, 65],
        pad_gain=0.017,
        bell_gain=0.050,
        bass_gain=0.052,
        pulse_gain=0.026,
    )
    synth_track(
        BGM_ROOT / "vn_comedic_roast.wav",
        12.0,
        118.0,
        [(60, 64, 67), (62, 65, 69), (59, 62, 67), (55, 60, 64)],
        [72, 76, 79, None, 74, 77, None, 72],
        pad_gain=0.014,
        bell_gain=0.085,
        bass_gain=0.020,
    )
    synth_track(
        BGM_ROOT / "vn_tutorial_resolve.wav",
        16.0,
        96.0,
        [(57, 60, 64), (55, 59, 62), (52, 55, 59), (53, 57, 60)],
        [69, None, 72, 74, 76, None, 74, 72],
        pad_gain=0.021,
        bell_gain=0.064,
        bass_gain=0.034,
    )
    synth_track(
        BGM_ROOT / "vn_chapter3_road.wav",
        16.0,
        102.0,
        [(55, 59, 62), (60, 64, 67), (62, 65, 69), (57, 60, 64)],
        [71, None, 74, 76, 79, 76, 74, None],
        pad_gain=0.018,
        bell_gain=0.070,
        bass_gain=0.030,
    )

    save_icon(draw_heal_potion(), ITEM_ROOT / "item_slot_1_heal_potion.png")
    save_icon(draw_focus_vial(), ITEM_ROOT / "item_slot_2_focus_vial.png")
    save_icon(draw_burst_bomb(), ITEM_ROOT / "item_slot_3_burst_bomb.png")


if __name__ == "__main__":
    main()
