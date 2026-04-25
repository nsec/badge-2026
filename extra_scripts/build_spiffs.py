"""
Post-build script: automatically build SPIFFS image after firmware compilation.

Runs mkspiffs to create spiffs.bin from the environment's data directory.
Skips if no data directory exists or image is already up to date.

Added as: post:extra_scripts/build_spiffs.py
"""

Import("env")

import os
import subprocess
import sys


def auto_build_spiffs(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    build_dir = env.subst("$BUILD_DIR")

    # Resolve data directory (respects board_build.data_dir override)
    data_dir_opt = env.GetProjectOption("board_build.data_dir", "")
    if data_dir_opt:
        data_dir = os.path.join(project_dir, data_dir_opt)
    else:
        data_dir = os.path.join(project_dir, "data")

    if not os.path.isdir(data_dir):
        return

    spiffs_bin = os.path.join(build_dir, "spiffs.bin")

    # Skip if image is up to date
    if os.path.isfile(spiffs_bin):
        spiffs_mtime = os.path.getmtime(spiffs_bin)
        needs_rebuild = False
        for root, _dirs, files in os.walk(data_dir):
            for f in files:
                if os.path.getmtime(os.path.join(root, f)) > spiffs_mtime:
                    needs_rebuild = True
                    break
            if needs_rebuild:
                break
        if not needs_rebuild:
            size_kb = os.path.getsize(spiffs_bin) / 1024
            print("SPIFFS: up to date (%.1f KB)" % size_kb)
            return

    # Locate mkspiffs from PlatformIO packages
    try:
        tool_dir = env.PioPlatform().get_package_dir("tool-mkspiffs")
        if not tool_dir:
            raise RuntimeError("tool-mkspiffs package not installed")
        # PlatformIO names the binary per platform variant, e.g.
        # mkspiffs_espressif32_arduino(.exe)
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

    cmd = [mkspiffs, "-c", data_dir, "-p", "256", "-b", "4096", "-s", spiffs_size, spiffs_bin]

    print("SPIFFS: building from %s/" % os.path.basename(data_dir))
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("SPIFFS: build failed: %s" % result.stderr.strip())
    else:
        size_kb = os.path.getsize(spiffs_bin) / 1024
        print("SPIFFS: %.1f KB -> %s" % (size_kb, os.path.basename(spiffs_bin)))


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", auto_build_spiffs)
