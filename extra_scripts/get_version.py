import subprocess
import os
import json
import hashlib
Import("env")

project_dir = env.subst("$PROJECT_DIR")
pio_env = env.subst("$PIOENV")
dev_dir = os.path.join(project_dir, ".external", "rMesh-Flashtool")
build_counter_file = os.path.join(dev_dir, "build_counter.txt")
build_versions_file = os.path.join(dev_dir, "build_versions.json")
source_hash_file = os.path.join(dev_dir, "source_hash.txt")

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always", "--dirty"],
        stderr=subprocess.DEVNULL,
        cwd=project_dir,
        shell=(os.name == "nt")
    ).decode().strip()
except Exception as e:
    print(f"get_version.py: git describe failed ({e}), using 'unknown'")
    version = "unknown"

# Hash all relevant source files to detect changes
def hash_sources():
    h = hashlib.sha256()
    src_dirs = [
        os.path.join(project_dir, "src"),
        os.path.join(project_dir, "lib"),
        os.path.join(project_dir, "data"),
    ]
    src_exts = {".cpp", ".h", ".c", ".hpp", ".html", ".css", ".js", ".json"}
    skip = {"version.h"}
    files = []
    for d in src_dirs:
        if not os.path.isdir(d):
            continue
        for root, _, filenames in os.walk(d):
            for fn in filenames:
                if fn in skip:
                    continue
                if os.path.splitext(fn)[1].lower() in src_exts:
                    files.append(os.path.join(root, fn))

    pio_ini = os.path.join(project_dir, "platformio.ini")
    if os.path.isfile(pio_ini):
        files.append(pio_ini)

    for fp in sorted(files):
        try:
            with open(fp, "rb") as f:
                h.update(f.read())
        except OSError:
            pass
    return h.hexdigest()

current_hash = hash_sources()

# Read previous hash
prev_hash = ""
try:
    with open(source_hash_file, "r") as f:
        prev_hash = f.read().strip()
except FileNotFoundError:
    pass

# Read counter
try:
    with open(build_counter_file, "r") as f:
        counter = int(f.read().strip())
except (FileNotFoundError, ValueError):
    counter = 0

# Only increment if sources changed
if current_hash != prev_hash:
    counter += 1
    with open(build_counter_file, "w") as f:
        f.write(str(counter))
    with open(source_hash_file, "w") as f:
        f.write(current_hash)

version = f"{version}+b{counter}"

print(f"get_version.py: VERSION = {version} (env={pio_env})")

version_header = os.path.join(project_dir, "src", "version.h")
with open(version_header, "w") as f:
    f.write(f'#pragma once\n#define VERSION "{version}"\n')

# Save version per environment
try:
    with open(build_versions_file, "r") as f:
        versions = json.load(f)
except (FileNotFoundError, ValueError, json.JSONDecodeError):
    versions = {}

versions[pio_env] = version

with open(build_versions_file, "w") as f:
    json.dump(versions, f, indent=2)
    f.write("\n")
