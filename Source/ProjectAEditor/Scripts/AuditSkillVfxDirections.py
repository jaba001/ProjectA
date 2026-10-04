import hashlib
import json
import re
from collections import Counter
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SKILL_ROOT = "/Game/User_JeHoon/Blueprint/DataAsset/Skills"
REPORT_DIR = ROOT / "Saved/Automation" / ("ChainSkills" if "-ChainSkillsAudit" in unreal.SystemLibrary.get_command_line() else "SkillVfxDirection")
ORIGINAL_HASHES = ROOT / "Saved/Automation/DrGameSkills/OriginalHashes.json"
DIRECTION_WORDS = ("velocity", "rotation", "orientation", "direction", "facing", "axis", "position", "location", "beam", "start", "end", "target", "source", "space", "transform")
CASCADE_FIELDS = ("enabled", "use_local_space", "screen_alignment", "emitter_origin", "emitter_rotation", "orbit_module_affects_velocity_alignment", "start_velocity", "start_velocity_radial", "in_world_space", "apply_owner_scale", "acceleration", "velocity", "velocity_scale", "scale", "start_rotation", "start_rotation_rate", "rotation_rate", "rotation", "inherit_parent", "axis_lock", "axis_lock_option", "lock_axis_flags", "mesh_alignment", "mesh", "camera_facing", "camera_facing_option", "roll_pitch_yaw_range", "start_location", "location", "end_point", "source", "source_method", "source_name", "source_absolute", "source_tangent", "source_tangent_method", "target", "target_method", "target_name", "target_absolute", "target_tangent", "target_tangent_method")
DISTRIBUTION_FIELDS = ("constant", "min", "max", "constant_curve", "distribution", "parameter_name", "param_modes", "param_mode", "min_input", "max_input", "min_output", "max_output", "lock_axes", "locked_axes", "mirror_flags", "use_extremes")


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hashes_for(paths):
    return {path.relative_to(ROOT).as_posix(): file_hash(path) for path in sorted(paths)}


def read_optional(value, name):
    try:
        return True, value.get_editor_property(name)
    except Exception as error:
        return False, str(error)


def serialize(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.Vector):
        return [value.x, value.y, value.z]
    if isinstance(value, unreal.Quat):
        return [value.x, value.y, value.z, value.w]
    if isinstance(value, unreal.Rotator):
        return {"pitch": value.pitch, "yaw": value.yaw, "roll": value.roll}
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    if hasattr(value, "items"):
        return {str(key): serialize(item) for key, item in value.items()}
    if isinstance(value, (list, tuple, unreal.Array)):
        return [serialize(item) for item in value]
    return str(value)


def describe_distribution(value, depth=0):
    if value is None:
        return None
    if depth >= 5:
        return {"value": serialize(value), "limit": "nested distribution depth"}
    result = {"value": serialize(value), "fields": {}}
    if isinstance(value, unreal.Object):
        result["class"] = value.get_class().get_path_name()
    for name in DISTRIBUTION_FIELDS:
        exists, item = read_optional(value, name)
        if exists:
            result["fields"][name] = describe_distribution(item, depth + 1) if name == "distribution" else serialize(item)
    return result


def describe_module(module):
    if module is None:
        return None
    result = {"object": module.get_path_name(), "class": module.get_class().get_path_name(), "fields": {}}
    for name in CASCADE_FIELDS:
        exists, value = read_optional(module, name)
        if exists:
            result["fields"][name] = describe_distribution(value) if isinstance(value, unreal.StructBase) and not isinstance(value, (unreal.Vector, unreal.Rotator)) else serialize(value)
    return result


def inspect_cascade(system):
    result = {"asset": system.get_path_name(), "emitters": [], "unavailable_properties": []}
    exists, emitters = read_optional(system, "emitters")
    if not exists:
        result["unavailable_properties"].append({"object": system.get_path_name(), "property": "emitters", "reason": emitters})
        return result
    for emitter in emitters:
        if emitter is None:
            continue
        row = {"object": emitter.get_path_name(), "class": emitter.get_class().get_path_name(), "lods": []}
        exists, name = read_optional(emitter, "emitter_name")
        if exists:
            row["name"] = str(name)
        exists, levels = read_optional(emitter, "lod_levels")
        if not exists:
            result["unavailable_properties"].append({"object": emitter.get_path_name(), "property": "lod_levels", "reason": levels})
            result["emitters"].append(row)
            continue
        for level in levels:
            if level is None:
                continue
            lod = {"object": level.get_path_name(), "modules": []}
            for name in ["level", "enabled"]:
                exists, value = read_optional(level, name)
                if exists:
                    lod[name] = serialize(value)
            for name in ["required_module", "type_data_module", "modules"]:
                exists, value = read_optional(level, name)
                if not exists:
                    result["unavailable_properties"].append({"object": level.get_path_name(), "property": name, "reason": value})
                elif name == "modules":
                    lod["modules"] = [describe_module(module) for module in value if module is not None]
                else:
                    lod[name] = describe_module(value)
            row["lods"].append(lod)
        result["emitters"].append(row)
    return result


def load_reference(value):
    if isinstance(value, unreal.Object) or value is None:
        return value
    path = str(value)
    if path in ("", "None", "none"):
        return None
    match = re.search(r"(?:/Game|/Engine|/Niagara)/[^'\"\s)]+", path)
    return require(unreal.load_asset(match.group(0) if match else path), "VFX reference load failed: " + path)


def inspect_visual(visual, niagara, cascade, errors, skill_path, field):
    result = {name: serialize(visual.get_editor_property(name)) for name in ["relative_transform", "start_position_parameter", "start_position_offset", "end_position_parameter", "start_position_space", "end_position_space", "bool_parameters", "float_parameters", "sound"]}
    transform = visual.get_editor_property("relative_transform")
    result["transform_components"] = {name: serialize(transform.get_editor_property(name)) for name in ["translation", "rotation", "scale3d"]}
    for name, kind, cache in [("niagara", unreal.NiagaraSystem, niagara), ("cascade", unreal.ParticleSystem, cascade)]:
        system = load_reference(visual.get_editor_property(name))
        result[name] = system.get_path_name() if system else None
        if system is None:
            continue
        require(isinstance(system, kind), "Unexpected " + name + " class: " + system.get_path_name())
        if system.get_path_name() not in cache:
            cache[system.get_path_name()] = json.loads(unreal.CombatVfxAssetLibrary.inspect_niagara_space(system)) if name == "niagara" else inspect_cascade(system)
        inspected = cache[system.get_path_name()]
        if name == "niagara":
            if not inspected.get("valid") or not inspected.get("systemValid"):
                errors.append(skill_path + " / " + field + " / Niagara compiled data is invalid")
            types = {parameter["name"]: parameter["type"] for parameter in inspected["userParameters"]}
            result["endpoint_types"] = {}
            result["endpoint_effective_spaces"] = {}
            for endpoint in ["start_position_parameter", "end_position_parameter"]:
                parameter = result[endpoint]
                if parameter in ("", "None", "none"):
                    continue
                result["endpoint_types"][endpoint] = types.get(parameter)
                space = visual.get_editor_property(endpoint.replace("_parameter", "_space"))
                if space == unreal.CombatVfxEndpointSpace.PARAMETER_TYPE:
                    result["endpoint_effective_spaces"][endpoint] = "ComponentLocal" if types.get(parameter) == "Vector3f" else "World"
                else:
                    result["endpoint_effective_spaces"][endpoint] = "ComponentLocal" if space == unreal.CombatVfxEndpointSpace.COMPONENT_LOCAL else "World"
                if types.get(parameter) not in ("Vector3f", "NiagaraPosition"):
                    errors.append(skill_path + " / " + field + " / Endpoint parameter type mismatch: " + parameter)
            for values, expected in [("bool_parameters", "NiagaraBool"), ("float_parameters", "NiagaraFloat")]:
                for parameter in result[values]:
                    if types.get(parameter) != expected:
                        errors.append(skill_path + " / " + field + " / Override parameter type mismatch: " + parameter)
    return result


def direction_parameters(inspected):
    result = []
    for emitter in inspected.get("emitters", []):
        for script in emitter.get("scripts", []):
            for parameter in script.get("parameters", []):
                if any(word in parameter["name"].lower() for word in DIRECTION_WORDS):
                    result.append({"emitter": emitter["name"], "local_space": emitter.get("localSpace"), "script": script["script"], **parameter})
    return result


def main():
    # Loading and resolving are the only asset operations; no saving, authoring, activation or gameplay is permitted.
    # 에셋 작업은 로드와 해석만 수행하며 저장·작성·활성화·게임 실행을 허용하지 않습니다.
    require(ORIGINAL_HASHES.is_file(), "Purchased source hash baseline is missing")
    baseline = {Path(filename).as_posix(): expected for filename, expected in json.loads(ORIGINAL_HASHES.read_text(encoding="utf-8")).items()}
    require(len(baseline) == 1900, "Purchased source baseline must contain 1900 files")
    source_before = hashes_for([ROOT / filename for filename in baseline])
    require(source_before == baseline, "Purchased source differs from the installation baseline")
    skill_directory = ROOT / "Content" / SKILL_ROOT.removeprefix("/Game/")
    skill_before = hashes_for(skill_directory.rglob("*.uasset"))
    require(len(skill_before) == 74, "Expected 74 existing skill packages")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    entries = sorted(registry.get_assets_by_path(SKILL_ROOT, recursive=True), key=lambda item: str(item.package_name))
    niagara, cascade, skills, errors = {}, {}, [], []
    for entry in entries:
        if str(entry.asset_class_path.asset_name) != "SkillDefinitionDataAsset":
            continue
        skill = require(entry.get_asset(), "Skill package failed to load: " + str(entry.package_name))
        require(isinstance(skill, unreal.SkillDefinitionDataAsset), "Unexpected skill asset class")
        path = skill.get_path_name()
        valid = unreal.MonsterAssetLibrary.validate_monster_skill(skill)
        if not valid:
            errors.append("Runtime skill validation failed: " + path)
        profile = unreal.MonsterAssetLibrary.get_resolved_monster_skill(skill)
        row = {"asset": path, "valid": bool(valid), "use_round_definition": bool(skill.get_editor_property("use_round_definition")), "resolved": {name: serialize(profile.get_editor_property(name)) for name in ["skill_id", "name", "kind", "target_rule", "approach", "effect_tags", "target_only", "homing", "effect_offset", "effect_travel", "use_effect_collision", "effect_half_extent", "effect_sphere", "projectile_speed", "projectile_radius"]}}
        chain = profile.get_editor_property("chain")
        row["resolved"]["chain"] = {name: chain.get_editor_property(name) for name in ["max_targets", "jump_distance", "jump_interval_seconds", "damage_multiplier_per_jump"]}
        row["resolved"]["vfx"] = inspect_visual(profile.get_editor_property("vfx"), niagara, cascade, errors, path, "vfx")
        row["resolved"]["impact_vfx"] = inspect_visual(profile.get_editor_property("impact_vfx"), niagara, cascade, errors, path, "impact_vfx")
        skills.append(row)
    require(len(skills) == 74, "Asset registry must resolve all 74 skill DataAssets")
    for inspected in niagara.values():
        inspected["directionParameters"] = direction_parameters(inspected)
    # Compare on-disk packages again after every inspection, including any Niagara compile readiness checks.
    # Niagara 컴파일 준비 검사를 포함한 모든 검수 이후 디스크 패키지를 다시 비교합니다.
    source_after = hashes_for([ROOT / filename for filename in baseline])
    skill_after = hashes_for(skill_directory.rglob("*.uasset"))
    source_unchanged, skills_unchanged = source_before == source_after == baseline, skill_before == skill_after
    if not source_unchanged:
        errors.append("Purchased source packages changed during inspection")
    if not skills_unchanged:
        errors.append("Skill packages changed during read-only inspection")
    report = {"mode": "read_only", "skills": skills, "skill_count": len(skills), "resolved_kinds": dict(Counter(row["resolved"]["kind"] for row in skills)), "niagara": niagara, "cascade": cascade, "niagara_count": len(niagara), "cascade_count": len(cascade), "source_files_unchanged": len(source_before) if source_unchanged else 0, "skill_files_unchanged": len(skill_before) if skills_unchanged else 0, "source_hashes_before": source_before, "source_hashes_after": source_after, "skill_hashes_before": skill_before, "skill_hashes_after": skill_after, "errors": errors, "gameplay_test": "not run", "visual_alignment": "user verification pending", "inspection_limits": ["NullRHI does not render effects; ready=false is not a rendering result", "Rapid iteration values are static evidence; graph and material direction behavior requires separate source inspection", "Cascade unavailable reflected properties are explicitly recorded"]}
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    output = REPORT_DIR / "AssetAudit.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SKILL_VFX_DIRECTION_AUDIT " + json.dumps({"report": str(output), "skills": len(skills), "niagara": len(niagara), "cascade": len(cascade), "source_files_unchanged": report["source_files_unchanged"], "skill_files_unchanged": report["skill_files_unchanged"], "errors": len(errors)}))
    require(not errors, "; ".join(errors))


if __name__ == "__main__":
    main()
