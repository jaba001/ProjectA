import json
from collections import Counter
from pathlib import Path

import unreal


# Audit only the six purchased source packs; do not save source packages.
# 새로 구입한 원본 팩 6개만 검사하며 원본 패키지를 저장하지 않습니다.
PACKS = {
    "AOE and Spell Decal VFX ( with SFX )": ("/Game/__AoeVFX", "8908c188-5b4d-4131-94dd-701c1c200c52"),
    "Ground Attack VFX ( with SFX )": ("/Game/__GroundAttackVFX", "c6b26012-37e3-4807-b55d-b410c99c808d"),
    "Interactive LinkChain VFX ( with SFX )": ("/Game/___LinkChainVFX", "5e6daae7-7e14-467c-aa27-e9698b116b05"),
    "Level Up and Spawn VFX ( with SFX )": ("/Game/_LevelUpSpawn", "a77fc805-8e31-43ce-b613-17bd39ebf9cd"),
    "ProjectileVFX with Hit and Launch VFX ( with SFX )": ("/Game/ProjectileHitVFX", "4f4de421-d1dc-49c3-b66d-fb22fa8d015e"),
    "Slash and Hit VFX (with SFX)": ("/Game/SlashHitVFX", "f6bf529f-6db4-43a5-a5f1-2ff5b8d9a0f4"),
}


def audit():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    roots = [str(path) for path in registry.get_sub_paths("/Game", False)]
    output = {"roots": roots, "packs": [], "errors": [], "gameplay_test": "not run"}
    for pack, (root, listing) in PACKS.items():
        assets = registry.get_assets_by_path(root, recursive=True)
        row = {"name": pack, "root": root, "listing_url": "https://www.fab.com/listings/" + listing, "classes": dict(Counter(str(asset.asset_class_path.asset_name) for asset in assets)), "assets": []}
        for asset in assets:
            package = str(asset.package_name)
            name = str(asset.asset_name)
            class_name = str(asset.asset_class_path.asset_name)
            entry = {"name": name, "package": package, "object": package + "." + name, "class": class_name, "dependencies": [str(dep) for dep in registry.get_dependencies(package, unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True))]}
            if class_name in ("NiagaraSystem", "SoundCue", "SoundWave", "Blueprint"):
                loaded = unreal.EditorAssetLibrary.load_asset(package)
                if not loaded:
                    output["errors"].append("load failed: " + package)
                elif class_name == "NiagaraSystem":
                    entry["niagara"] = json.loads(unreal.CombatVfxAssetLibrary.inspect_niagara_space(loaded))
                elif class_name == "SoundWave":
                    entry["duration"] = loaded.get_editor_property("duration")
                    entry["looping"] = loaded.get_editor_property("looping")
            row["assets"].append(entry)
        output["packs"].append(row)
    target = Path(unreal.Paths.project_saved_dir(), "Automation", "DrGameSkills", "Inventory.json")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DRGAME_INVENTORY " + str(target))
    if output["errors"]:
        raise RuntimeError("; ".join(output["errors"]))


if __name__ == "__main__":
    audit()
