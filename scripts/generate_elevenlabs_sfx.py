#!/usr/bin/env python3

import argparse
import json
import os
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


API_URL = "https://api.elevenlabs.io/v1/sound-generation"
MODEL_ID = "eleven_text_to_sound_v2"

SOUND_DEFS = {
    "stone": {
        "filename": "block_break_stone.ogg",
        "duration": 0.8,
        "prompt_influence": 0.45,
        "prompt": (
            "Short one-shot game sound: iron pickaxe breaking a rough stone block. "
            "Sharp mineral crack, gritty rock fragments, tight impact, dry debris, "
            "clean game-ready foley, no ambience, no reverb, no voice."
        ),
    },
    "wood": {
        "filename": "block_break_wood.ogg",
        "duration": 0.75,
        "prompt_influence": 0.42,
        "prompt": (
            "Short one-shot game sound: tool breaking a dry wooden log block. "
            "Crisp woody crack, splinter snap, compact impact, clean game-ready foley, "
            "no ambience, no reverb, no voice."
        ),
    },
    "dirt": {
        "filename": "block_break_dirt.ogg",
        "duration": 0.7,
        "prompt_influence": 0.44,
        "prompt": (
            "Short one-shot game sound: breaking a grassy dirt block with a shovel. "
            "Soft dirt crumble, grass tear, earthy clumps, subtle grit, tight impact, "
            "clean game-ready foley, no ambience, no reverb, no voice."
        ),
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate CubeOS block break SFX with the ElevenLabs sound effects API."
    )
    parser.add_argument(
        "--api-key",
        default=os.environ.get("ELEVENLABS_API_KEY") or os.environ.get("ELEVEN_API_KEY"),
        help="ElevenLabs API key. Defaults to ELEVENLABS_API_KEY or ELEVEN_API_KEY.",
    )
    parser.add_argument(
        "--output-dir",
        default=str(Path(__file__).resolve().parents[1] / "assets" / "audio"),
        help="Directory for generated audio files.",
    )
    parser.add_argument(
        "--only",
        nargs="+",
        choices=sorted(SOUND_DEFS.keys()),
        default=sorted(SOUND_DEFS.keys()),
        help="Subset of sounds to generate.",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing files in the output directory.",
    )
    return parser.parse_args()


def request_sound(api_key: str, prompt: str, duration: float, prompt_influence: float) -> bytes:
    payload = json.dumps(
        {
            "text": prompt,
            "duration_seconds": duration,
            "prompt_influence": prompt_influence,
            "model_id": MODEL_ID,
        }
    ).encode("utf-8")
    request = urllib.request.Request(
        API_URL,
        data=payload,
        headers={
            "xi-api-key": api_key,
            "Content-Type": "application/json",
            "Accept": "audio/mpeg",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            return response.read()
    except urllib.error.HTTPError as exc:
        details = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"ElevenLabs API error {exc.code}: {details}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Failed to reach ElevenLabs API: {exc}") from exc


def transcode_mp3_to_ogg(mp3_path: Path, ogg_path: Path) -> None:
    command = [
        "ffmpeg",
        "-y",
        "-loglevel",
        "error",
        "-i",
        str(mp3_path),
        "-ac",
        "1",
        "-ar",
        "44100",
        "-c:a",
        "libvorbis",
        str(ogg_path),
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "ffmpeg failed")


def main() -> int:
    args = parse_args()
    if not args.api_key:
        print(
            "Missing ElevenLabs API key. Set ELEVENLABS_API_KEY or pass --api-key.",
            file=sys.stderr,
        )
        return 2

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    for name in args.only:
        sound = SOUND_DEFS[name]
        output_path = output_dir / sound["filename"]
        if output_path.exists() and not args.overwrite:
            print(f"Skipping {output_path.name}: file exists (use --overwrite to replace).")
            continue

        print(f"Generating {name} -> {output_path.name}")
        audio_mp3 = request_sound(
            args.api_key,
            sound["prompt"],
            sound["duration"],
            sound["prompt_influence"],
        )

        with tempfile.NamedTemporaryFile(suffix=".mp3", delete=False) as tmp_file:
            tmp_path = Path(tmp_file.name)
            tmp_file.write(audio_mp3)

        try:
            transcode_mp3_to_ogg(tmp_path, output_path)
        finally:
            tmp_path.unlink(missing_ok=True)

        print(f"Saved {output_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
