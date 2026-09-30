import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_PATH = Path(__file__).resolve().with_name("CatalogSkillSpecs.json")
ASSETS = unreal.EditorAssetLibrary
LIBRARY = unreal.CombatVfxAssetLibrary
PROJECTILE_SOURCES = {"/Game/RPGEffects/ParticlesNiagara/Archer/Arrow/NS_Archer_Arrow", "/Game/RPGEffects/ParticlesNiagara/Archer/ArrowFire/NS_Archer_Arrow_Fire", "/Game/RPGEffects/ParticlesNiagara/Warrior/ThrowingAxe/NS_Warrior_ThrowingAxe"}


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def package_file(package):
    require(package.startswith("/Game/"), "Expected project package: " + package)
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def file_hash(package):
    return hashlib.sha256(package_file(package).read_bytes()).hexdigest()


def inspect(system):
    return json.loads(LIBRARY.inspect_niagara_space(system))


def enabled_emitters(description):
    return [emitter for emitter in description["emitters"] if emitter["enabled"] and emitter["stateful"]]


def mesh_body(emitter):
    return any(renderer["enabled"] and renderer["class"].endswith(".NiagaraMeshRendererProperties") for renderer in emitter["renderers"])


def configure(system, names, local_space, clear_velocity):
    if not names:
        return
    error = LIBRARY.configure_emitter_space(system, names, local_space, clear_velocity)
    require(not error, system.get_path_name() + " / " + str(error))


def main():
    spec = json.loads(SPEC_PATH.read_text(encoding="utf-8"))
    candidates = [entry for entry in spec["entries"] if entry["source_class"] == "NiagaraSystem" and (entry["profile"] == "slash" or entry["source"] in PROJECTILE_SOURCES)]
    source_hashes = {entry["source"]: file_hash(entry["source"]) for entry in candidates}
    reports = []
    changed = []
    for entry in candidates:
        source = require(unreal.load_asset(entry["source"]), "Source not found: " + entry["source"])
        before = inspect(source)
        emitters = require(enabled_emitters(before), "No enabled stateful emitters: " + entry["source"])
        projectile = entry["profile"] == "projectile"
        # Already-local bodies retain their source; incidental world sparks do not justify copying the system.
        # 본체가 이미 로컬이면 원본을 유지하며 부수적인 월드 불꽃 때문에 시스템을 복제하지 않습니다.
        needs_direction = all(not emitter["localSpace"] for emitter in emitters)
        row = {"name": entry["name"], "source": entry["source"], "before": before, "changed": needs_direction}
        if not needs_direction:
            reports.append(row)
            continue
        destination = "/Game/User_JeHoon/" + entry["source"].removeprefix("/Game/") + "_TargetDirection"
        exists = ASSETS.does_asset_exist(destination)
        derivative = require(unreal.load_asset(destination) if exists else ASSETS.duplicate_asset(entry["source"], destination), "Could not author derivative: " + destination)
        if exists:
            metadata = str(ASSETS.get_metadata_tag(derivative, "CombatDirectionSource"))
            require(metadata == entry["source"], "Existing derivative has missing or different source metadata: " + destination)
        ASSETS.set_metadata_tag(derivative, "CombatDirectionSource", entry["source"])
        require(ASSETS.save_loaded_asset(derivative), "Could not retain derivative provenance: " + destination)
        inspection_output = ROOT / "Saved/Automation/CombatVfxDirection" / (derivative.get_name() + ".json")
        inspection_output.parent.mkdir(parents=True, exist_ok=True)
        inspection_output.write_text(json.dumps(inspect(derivative), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if projectile:
            error = LIBRARY.upgrade_location_event_to_world_version(derivative)
            require(not error, destination + " / " + str(error))
            # The local mesh follows the projectile; world trails stay behind it without a second forward velocity.
            # 로컬 메시는 투사체를 따르고 월드 잔상은 추가 전진 속도 없이 비행 경로에 남습니다.
            local_names = [emitter["name"] for emitter in emitters if mesh_body(emitter) or emitter["localSpace"]]
            world_names = [emitter["name"] for emitter in emitters if emitter["name"] not in local_names]
            require(local_names, "Projectile needs an identifiable mesh body: " + entry["source"])
            configure(derivative, local_names, True, True)
            configure(derivative, world_names, False, True)
        else:
            local_names = [emitter["name"] for emitter in emitters]
            world_names = []
            configure(derivative, local_names, True, False)
        after = inspect(derivative)
        after_emitters = {emitter["name"]: emitter for emitter in enabled_emitters(after)}
        require(all(after_emitters[name]["localSpace"] for name in local_names), "Directional body remains in world space: " + destination)
        require(all(not after_emitters[name]["localSpace"] for name in world_names), "World trail space changed: " + destination)
        if projectile:
            for emitter in after_emitters.values():
                require(all(module["major"] == 1 and module["minor"] == 1 for module in emitter.get("locationEventModules", [])), "Projectile location event space conversion is missing: " + destination)
                for script in emitter["scripts"]:
                    for parameter in script["parameters"]:
                        if parameter["initialVelocity"]:
                            require(parameter.get("value") == [0, 0, 0], "Projectile retains independent forward velocity: " + destination + " / " + parameter["name"])
        require(ASSETS.save_loaded_asset(derivative), "Could not save derivative: " + destination)
        skill = require(unreal.load_asset(entry["destination"]), "Missing skill: " + entry["destination"])
        definition = skill.get_editor_property("round_definition")
        visual = definition.get_editor_property("vfx")
        current_source = visual.get_editor_property("niagara")
        require(current_source == source or current_source == derivative, "Existing skill has a custom visual; preserve it: " + entry["destination"])
        visual.set_editor_property("niagara", derivative)
        definition.set_editor_property("vfx", visual)
        skill.set_editor_property("round_definition", definition)
        require(ASSETS.save_loaded_asset(skill), "Could not save skill: " + entry["destination"])
        entry["direction_source"] = destination
        row.update({"destination": destination, "skill": entry["destination"], "local_emitters": local_names, "world_emitters": world_names, "after": after, "size_bytes": package_file(destination).stat().st_size})
        changed.append(destination)
        reports.append(row)
    require(source_hashes == {package: file_hash(package) for package in source_hashes}, "Original visual packages changed")
    SPEC_PATH.write_text(json.dumps(spec, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    output = ROOT / "Saved/Automation/CombatVfxDirection/Authoring.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"candidate_count": len(candidates), "changed_count": len(changed), "derivative_bytes": sum(row.get("size_bytes", 0) for row in reports), "source_hashes": source_hashes, "sources_unchanged": True, "gameplay_test": "not run", "visual_alignment": "not verified", "assets": reports}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("COMBAT_VFX_DIRECTION_AUTHORED " + str(output))


if __name__ == "__main__":
    main()
