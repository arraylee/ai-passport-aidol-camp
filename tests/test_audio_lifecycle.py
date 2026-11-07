"""Run production audio lifecycle functions with deterministic RTOS/BSP fakes.

The implementation is extracted on every run. Only external peripherals,
allocation, task creation and worker scheduling are replaced by the fixture.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/hr_platform.c").read_text()
# Ignore braces in comments/literals while retaining exact source offsets.
masked = re.sub(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
    lambda match: " " * len(match.group()),
    source,
    flags=re.DOTALL,
)


def function(name):
    match = re.search(r"(?m)^bool\s+" + re.escape(name) + r"\s*\([^;]*?\)\s*\{", masked)
    if match is None:
        raise AssertionError(f"Production function missing: {name}")
    end = match.end()
    depth = 1
    while depth:
        if end >= len(masked):
            raise AssertionError(f"Unclosed production function: {name}")
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    return source[match.start():end]


with tempfile.TemporaryDirectory(prefix="hr-audio-lifecycle-") as tmp:
    directory = Path(tmp)
    (directory / "production_audio_lifecycle.inc").write_text(
        "\n\n".join(function(name) for name in
                     ("hr_audio_pause", "hr_audio_init", "hr_audio_paused")) + "\n"
    )
    executable = directory / "test_audio_lifecycle"
    subprocess.run(
        [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra",
         "-Werror", "-fsanitize=undefined", "-I", str(ROOT / "main"),
         "-I", str(directory), str(ROOT / "tests/host/audio_lifecycle_fixture.c"),
         str(ROOT / "main/hr_sfx.c"), str(ROOT / "main/hr_game.c"), str(ROOT / "main/hr_core.c"), "-o", str(executable)],
        check=True,
    )
    subprocess.run([str(executable)], check=True, timeout=20)
