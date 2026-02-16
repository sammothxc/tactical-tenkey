import datetime, json, os

ver_file = "version.json"

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

with open("include/version.h", "w") as f:
    f.write(f'#define FW_VERSION "{version}"\n')
    f.write(f'#define FW_RELEASE "{release}"\n')
    f.write(f'#define FW_DATE "{datetime.date.today()}"\n')