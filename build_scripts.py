import datetime, json, os, shutil

Import("env")

# --- PRE-BUILD: version bump ---
ver_file = "version.json"
override = os.environ.get("FW_VERSION_OVERRIDE", "").strip().lstrip("v")

if override:
    # CI path: version is pinned from the git tag; don't bump or write version.json
    parts = override.split(".")
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise ValueError(f"FW_VERSION_OVERRIDE must look like 'X.Y.Z', got '{override}'")
    ver = {"major": int(parts[0]), "minor": int(parts[1]), "build": int(parts[2])}
else:
    # local path: load, bump, persist
    if os.path.exists(ver_file):
        with open(ver_file) as f:
            ver = json.load(f)
    else:
        ver = {"major": 0, "minor": 1, "build": 0}

    ver["build"] += 1

    with open(ver_file, "w") as f:
        json.dump(ver, f, indent=2)

version = f'{ver["major"]}.{ver["minor"]}.{ver["build"]}'
release = f'{ver["major"]}.{ver["minor"]}'
today = str(datetime.date.today())

with open("include/version.h", "w") as f:
    f.write(f'#define FW_VERSION "{version}"\n')
    f.write(f'#define FW_RELEASE "{release}"\n')
    f.write(f'#define FW_DATE "{today}"\n')

os.makedirs("firmware", exist_ok=True)

def copy_bin(source, target, env):
    shutil.copy(str(target[0]), "firmware/firmware.bin")
    print(f">> Copied firmware to firmware/firmware.bin (v{version})")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_bin)