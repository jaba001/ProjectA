import json
import re
from pathlib import Path


# Keep authoring paths aligned with the reviewed Unreal asset move manifest.
# 작성 경로를 검토된 Unreal 에셋 이동 명세와 일치시킵니다.
LAYOUT = json.loads(Path(__file__).with_name("LevelFolderLayout.json").read_text(encoding="utf-8"))
if LAYOUT.get("schema_version") != 1:
    raise RuntimeError("Unsupported project level folder layout schema")

MAP_PATHS = {}
for definition in LAYOUT["maps"]:
    name = definition["name"]
    path = definition["path"]
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name) or not re.fullmatch(r"/Game/User_JeHoon/LEVEL/(?:[A-Za-z][A-Za-z0-9_]*/)+" + name, path) or name in MAP_PATHS or path in MAP_PATHS.values():
        raise RuntimeError("Invalid or duplicate project level identity: " + str(definition))
    MAP_PATHS[name] = path


def project_level_path(name):
    if name not in MAP_PATHS:
        raise RuntimeError("Unknown project level: " + str(name))
    return MAP_PATHS[name]
