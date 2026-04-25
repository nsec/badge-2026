"""
SPIFFS build script: stages a merged data directory (env data + shared animations)
and wires it into PlatformIO's native SPIFFS pipeline.

1. Pre-build: copies files from the environment's data directory and the shared
   animations/ directory into a staging folder, then overrides PROJECT_DATA_DIR
   so that `pio run -t buildfs` / `pio run -t uploadfs` use the merged content.

2. Post-build: automatically runs mkspiffs after firmware compilation so that
   a plain `pio run` also produces an up-to-date spiffs.bin.

Added as: pre:extra_scripts/build_spiffs.py
"""

Import("env")

import os
import shutil
import subprocess
import sys


def _collect_source_dirs(project_dir, env):
    """Return the list of directories whose contents should be merged into SPIFFS."""
    dirs = []

    # Environment-specific data directory
    data_dir_opt = env.GetProjectOption("board_build.data_dir", "")
    if data_dir_opt:
        data_dir = os.path.join(project_dir, data_dir_opt)
    else:
        data_dir = os.path.join(project_dir, "data")

    if os.path.isdir(data_dir):
        dirs.append(data_dir)

    # Shared animations directory (always included when present)
    anim_dir = os.path.join(project_dir, "animations")
    if os.path.isdir(anim_dir):
        dirs.append(anim_dir)

    return dirs


def _newest_mtime(dirs):
    """Return the newest mtime across all files in the given directories."""
    newest = 0
    for d in dirs:
        for root, _dirs, files in os.walk(d):
            for f in files:
                mt = os.path.getmtime(os.path.join(root, f))
                if mt > newest:
                    newest = mt
    return newest


def _stage_files(source_dirs, staging_dir):
    """Copy files from all source directories into a flat staging directory.

    Later directories win on filename conflicts (animations overlay data).
    """
    if os.path.isdir(staging_dir):
        shutil.rmtree(staging_dir)
    os.makedirs(staging_dir)
    for d in source_dirs:
        for entry in os.listdir(d):
            src = os.path.join(d, entry)
            if os.path.isfile(src):
                shutil.copy2(src, staging_dir)


# ---------------------------------------------------------------------------
# Pre-build: stage merged directory and override PROJECT_DATA_DIR
# ---------------------------------------------------------------------------

project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
source_dirs = _collect_source_dirs(project_dir, env)
staging_dir = os.path.join(build_dir, "spiffs_data")

if source_dirs:
    _stage_files(source_dirs, staging_dir)
    dir_names = " + ".join(os.path.basename(d) for d in source_dirs)
    print("SPIFFS: staged from %s" % dir_names)

    # Let PlatformIO's native buildfs / uploadfs use the merged directory.
    env.Replace(PROJECT_DATA_DIR=staging_dir)


# ---------------------------------------------------------------------------
# Post-build: auto-build spiffs.bin after firmware so `pio run` is sufficient
# ---------------------------------------------------------------------------


def auto_build_spiffs(source, target, env):
    if not source_dirs:
        return

    spiffs_bin = os.path.join(build_dir, "spiffs.bin")

    # Skip if image is up to date relative to the *original* source files.
    if os.path.isfile(spiffs_bin):
        spiffs_mtime = os.path.getmtime(spiffs_bin)
        if _newest_mtime(source_dirs) <= spiffs_mtime:
            size_kb = os.path.getsize(spiffs_bin) / 1024
            print("SPIFFS: up to date (%.1f KB)" % size_kb)
            return

    # Locate mkspiffs from PlatformIO packages
    try:
        tool_dir = env.PioPlatform().get_package_dir("tool-mkspiffs")
        if not tool_dir:
            raise RuntimeError("tool-mkspiffs package not installed")
        ext = ".exe" if sys.platform == "win32" else ""
        candidates = [
            "mkspiffs_espressif32_arduino" + ext,
            "mkspiffs" + ext,
        ]
        mkspiffs = None
        for name in candidates:
            path = os.path.join(tool_dir, name)
            if os.path.isfile(path):
                mkspiffs = path
                break
        if not mkspiffs:
            raise FileNotFoundError("no mkspiffs binary in %s" % tool_dir)
    except Exception as e:
        print("SPIFFS: mkspiffs not found (%s), skipping" % e)
        return

    # SPIFFS partition size must match partitions/badge_factory_ota.csv
    spiffs_size = str(0x160000)

    cmd = [mkspiffs, "-c", staging_dir, "-p", "256", "-b", "4096", "-s", spiffs_size, spiffs_bin]

    dir_names = " + ".join(os.path.basename(d) for d in source_dirs)
    print("SPIFFS: building from %s" % dir_names)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("SPIFFS: build failed: %s" % result.stderr.strip())
    else:
        size_kb = os.path.getsize(spiffs_bin) / 1024
        print("SPIFFS: %.1f KB -> %s" % (size_kb, os.path.basename(spiffs_bin)))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", auto_build_spiffs)
