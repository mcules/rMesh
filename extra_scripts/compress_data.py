"""
PlatformIO extra_script: gzip-compress web assets before building LittleFS.

data/ contains the editable source files.
data_gz/ is auto-generated with gzip-compressed HTML/JS/CSS/TXT.
After mklittlefs builds littlefs.bin from data/, a post-action rebuilds it
from data_gz/ using the same tool and parameters.

ESPAsyncWebServer serves .gz files automatically to gzip-capable browsers.
"""

Import("env")  # noqa: F821 - SCons built-in

import gzip
import os
import shutil
import subprocess

COMPRESS_EXTENSIONS = {".html", ".js", ".css"}

project_dir = env.subst("$PROJECT_DIR")
data_dir = os.path.join(project_dir, "data")
out_dir = os.path.join(project_dir, "data_gz")


def _generate_data_gz():
    """Generate data_gz/ with gzip-compressed web assets from data/."""
    if not os.path.isdir(data_dir):
        return False

    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)

    total_orig = 0
    total_out = 0

    for root, _dirs, files in os.walk(data_dir):
        for filename in files:
            src = os.path.join(root, filename)
            rel = os.path.relpath(src, data_dir)
            _, ext = os.path.splitext(filename)
            orig_size = os.path.getsize(src)
            total_orig += orig_size

            if ext.lower() in COMPRESS_EXTENSIONS:
                dst = os.path.join(out_dir, rel + ".gz")
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(src, "rb") as f_in, gzip.open(dst, "wb", compresslevel=9) as f_out:
                    shutil.copyfileobj(f_in, f_out)
                gz_size = os.path.getsize(dst)
                total_out += gz_size
                pct = 100 - gz_size * 100 // orig_size
                print(f"  [gzip] {rel}: {orig_size}B -> {gz_size}B (-{pct}%)")
            else:
                dst = os.path.join(out_dir, rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(src, dst)
                total_out += orig_size

    pct_total = 100 - total_out * 100 // total_orig
    print(f"\n[compress_data] {total_orig}B -> {total_out}B (-{pct_total}%)\n")
    return True


def _rebuild_fs_from_gz(source, target, env):
    """Post-action: rebuild LittleFS image from data_gz/ instead of data/."""
    if not os.path.isdir(out_dir):
        print("[compress_data] data_gz/ not found, skipping")
        return

    # FS_SIZE/PAGE/BLOCK are set as integers by the platform builder emitter
    fs_size = env.get("FS_SIZE")
    fs_page = env.get("FS_PAGE")
    fs_block = env.get("FS_BLOCK")
    mkfstool = env.subst("$MKFSTOOL")  # e.g. "mklittlefs"
    # target[0] resolves to the "buildfs" alias, not the actual .bin path
    littlefs_bin = env.subst("$BUILD_DIR/${ESP32_FS_IMAGE_NAME}.bin")

    if not fs_size:
        print("[compress_data] FS_SIZE not set - is this a buildfs target?")
        return

    cmd = [
        mkfstool,
        "-c", out_dir,
        "-s", str(fs_size),
        "-p", str(fs_page),
        "-b", str(fs_block),
        littlefs_bin,
    ]

    # Use SCons build environment so PlatformIO tool directories are in PATH
    build_env = env.get("ENV", os.environ.copy())

    print(f"\n[compress_data] Rebuilding {os.path.basename(littlefs_bin)} from data_gz/ ...")
    result = subprocess.run(cmd, env=build_env, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[compress_data] ERROR: {result.stderr.strip()}")
    else:
        bin_size = os.path.getsize(littlefs_bin)
        print(f"[compress_data] Done. Image: {bin_size // 1024} KB\n")


# --- Main ---

# Generate data_gz/ unconditionally (fast, ~ms for small web files)
_generate_data_gz()

# After buildfs creates the image from data/, replace it with one from data_gz/
env.AddPostAction("buildfs", _rebuild_fs_from_gz)  # noqa: F821
