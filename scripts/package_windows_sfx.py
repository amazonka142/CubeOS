#!/usr/bin/env python3

from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
BUILD_WIN = REPO_ROOT / "build-win"
RELEASE_ASSETS = REPO_ROOT / "release-assets"
RELEASE_TAG = "v0.3.0-snapshot.1"
GAME_EXE_NAME = f"CubeOS-{RELEASE_TAG}.exe"
FINAL_ASSET_NAME = f"CubeOS-{RELEASE_TAG}-win64.exe"
FOOTER_MAGIC = b"CUBEOSSFX1"


def run(cmd: list[str], cwd: Path | None = None) -> None:
    subprocess.run(cmd, cwd=cwd or REPO_ROOT, check=True)


def write_readme(path: Path) -> None:
    path.write_text(
        "\n".join(
            [
                f"CubeOS {RELEASE_TAG} (Windows x64)",
                "",
                "This is a self-extracting snapshot build.",
                "On first launch it unpacks CubeOS into your local AppData folder and starts the game.",
                "",
                "Requirements:",
                "- Windows 10/11 x64",
                "- Vulkan-capable graphics drivers",
                "- PowerShell available for the built-in archive extraction step",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def main() -> int:
    exe_path = BUILD_WIN / "cubeos_voxel.exe"
    shaders_dir = BUILD_WIN / "shaders"
    audio_dir = BUILD_WIN / "audio"
    textures_dir = BUILD_WIN / "textures"
    stub_source = REPO_ROOT / "scripts" / "windows_sfx_stub.cpp"

    for path in (exe_path, shaders_dir, audio_dir, textures_dir, stub_source):
        if not path.exists():
            raise SystemExit(f"Missing required input: {path}")

    RELEASE_ASSETS.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="cubeos-win-sfx-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        payload_root = temp_dir / "payload"
        payload_root.mkdir()

        shutil.copy2(exe_path, payload_root / GAME_EXE_NAME)
        shutil.copytree(shaders_dir, payload_root / "shaders")
        shutil.copytree(audio_dir, payload_root / "audio")
        shutil.copytree(textures_dir, payload_root / "textures")
        write_readme(payload_root / "README.txt")

        payload_zip = temp_dir / "payload.zip"
        run(["/usr/bin/zip", "-qry", str(payload_zip), "."], cwd=payload_root)

        stub_exe = temp_dir / "cubeos_stub.exe"
        run(
            [
                "x86_64-w64-mingw32-g++",
                "-std=c++20",
                "-O2",
                "-static",
                "-static-libgcc",
                "-static-libstdc++",
                "-municode",
                "-mwindows",
                str(stub_source),
                "-o",
                str(stub_exe),
            ]
        )

        final_asset = RELEASE_ASSETS / FINAL_ASSET_NAME
        with stub_exe.open("rb") as stub_file, payload_zip.open("rb") as payload_file, final_asset.open(
            "wb"
        ) as output:
            output.write(stub_file.read())
            payload_bytes = payload_file.read()
            output.write(payload_bytes)
            output.write(FOOTER_MAGIC)
            output.write(struct.pack("<Q", len(payload_bytes)))

        print(final_asset)
    return 0


if __name__ == "__main__":
    sys.exit(main())
