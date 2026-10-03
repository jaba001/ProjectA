import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from RetiredSkillContent import REMOVED_SKILL_IDS, REMOVED_SKILL_PATHS


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC = json.loads(Path(__file__).with_name("DrGameSkillSpecs.json").read_text(encoding="utf-8"))
VERIFY = "-DrGameSkillsVerifyOnly" in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
OWNER_KEY = "ProjectA.DrGameSkills"
OWNER = "DrGameSkills.v1"
REPORT_DIR = ROOT / "Saved/Automation/DrGameSkills"


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def file_hash(path):
    result = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def package_file(package):
    require(package.startswith("/Game/"), "Unexpected package namespace")
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def make_tags(names):
    tags = unreal.GameplayTagContainer()
    require(tags.import_text("(GameplayTags=(" + ",".join('(TagName="' + name + '")' for name in names) + "))"), "GameplayTag import failed")
    require(sorted(re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', tags.export_text())) == sorted(names), "GameplayTag registration mismatch")
    return tags


def normalized(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.StructBase):
        # Transform serialization can change the sign of zero without changing its mathematical value.
        # Transform 저장은 수학적 값의 변화 없이 0의 부호를 바꿀 수 있습니다.
        return re.sub(r"(?<=[=(,])-0(\.0+)?(?=[,)])", r"0\1", value.export_text())
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    return value


def obtain(path, asset_class):
    require(path.startswith("/Game/User_JeHoon/"), "Destination must be project-owned")
    require(path not in REMOVED_SKILL_PATHS, "Retired package reuse is forbidden: " + path)
    if ASSETS.does_asset_exist(path):
        asset = load(path)
        require(isinstance(asset, asset_class) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Unowned destination: " + path)
        return asset, False
    require(not VERIFY, "Missing authored package: " + path)
    folder, name = path.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = require(TOOLS.create_asset(name, folder, asset_class, factory), "Asset factory failed: " + path)
    ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
    return asset, True


def save(asset):
    require(not VERIFY and asset.get_path_name().startswith("/Game/User_JeHoon/"), "Save scope rejected")
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + asset.get_path_name())


def set_or_check(asset, properties, created):
    for name, expected in properties.items():
        if created:
            asset.set_editor_property(name, expected)
        else:
            actual = asset.get_editor_property(name)
            if normalized(actual) != normalized(expected):
                REPORT_DIR.mkdir(parents=True, exist_ok=True)
                (REPORT_DIR / "PropertyDifference.json").write_text(json.dumps({"asset": asset.get_path_name(), "property": name, "actual": normalized(actual), "expected": normalized(expected)}, ensure_ascii=False, indent=2), encoding="utf-8")
                raise RuntimeError("Existing authored value changed; review without overwriting: " + asset.get_path_name() + " / " + name)


def make_vfx(entry, inspections):
    source = load(entry["source"])
    require(isinstance(source, unreal.NiagaraSystem), "Source must be Niagara")
    if entry["source"] not in inspections:
        inspections[entry["source"]] = json.loads(unreal.CombatVfxAssetLibrary.inspect_niagara_space(source))
    inspected = inspections[entry["source"]]
    # IsReadyToRun is false by engine contract under NullRHI; this authoring check validates compiled scripts only.
    # NullRHI에서는 엔진 계약상 IsReadyToRun이 false이므로 작성 검사에서 컴파일된 스크립트만 검증합니다.
    require(inspected["valid"] and inspected["systemValid"], "Niagara compilation is not valid: " + entry["source"])
    parameter_types = {param["name"]: param["type"] for param in inspected["userParameters"]}
    for name in entry.get("bool_parameters", {}):
        require(parameter_types.get(name) == "NiagaraBool", "Niagara bool mismatch: " + name)
    for name in entry.get("float_parameters", {}):
        require(parameter_types.get(name) == "NiagaraFloat", "Niagara float mismatch: " + name)
    for name in [entry.get("start_position_parameter"), entry.get("end_position_parameter")]:
        require(not name or parameter_types.get(name) in ("Vector3f", "NiagaraPosition"), "Niagara endpoint mismatch: " + str(name))
    require(not entry.get("sound"), "Embedded Niagara SFX must not have duplicate external playback")
    vfx = unreal.CombatSkillVfx()
    vfx.set_editor_property("niagara", source)
    vfx.set_editor_property("relative_transform", unreal.Transform(location=unreal.Vector(*entry["translation"])))
    vfx.set_editor_property("bool_parameters", entry.get("bool_parameters", {}))
    vfx.set_editor_property("float_parameters", entry.get("float_parameters", {}))
    if entry.get("start_position_parameter"):
        vfx.set_editor_property("start_position_parameter", entry["start_position_parameter"])
        vfx.set_editor_property("start_position_offset", unreal.Vector(*entry.get("start_position_offset", [0, 0, 0])))
    if entry.get("end_position_parameter"):
        vfx.set_editor_property("end_position_parameter", entry["end_position_parameter"])
    return vfx


def make_profile(entry, inspections):
    settings = SPEC["profiles"][entry["profile"]]
    profile = unreal.CombatRoundSkill()
    for name, value in SPEC["defaults"].items():
        profile.set_editor_property(name, value)
    profile.set_editor_property("skill_id", entry["skill_id"])
    profile.set_editor_property("name", entry["name"])
    profile.set_editor_property("kind", getattr(unreal.CombatRoundSkillKind, settings["kind"]))
    profile.set_editor_property("approach", getattr(unreal.CombatRoundApproach, settings["approach"]))
    profile.set_editor_property("target_rule", getattr(unreal.SkillTargetRule, settings["target_rule"]))
    profile.set_editor_property("target_loss", unreal.CombatRoundTargetLoss.CANCEL if settings["effect"] != "Damage" else unreal.CombatRoundTargetLoss.KEEP_LOCATION if settings["kind"] == "GROUND_ATTACK" else unreal.CombatRoundTargetLoss.NEAREST_ENEMY)
    profile.set_editor_property("target_only", settings["target_only"])
    profile.set_editor_property("use_effect_collision", settings["kind"] != "PROJECTILE")
    profile.set_editor_property("effect_half_extent", unreal.Vector(*settings["half_extent"]))
    profile.set_editor_property("effect_offset", unreal.Vector(*settings["offset"]))
    profile.set_editor_property("effect_sphere", settings["sphere"])
    profile.set_editor_property("effect_hit_delay_seconds", settings["delay"])
    profile.set_editor_property("effect_duration", settings["duration"])
    tags = entry["element_tags"] + ["Skill.Effect." + settings["effect"], "Skill.Shape." + settings["shape"]]
    if settings["effect"] == "Damage":
        tags.append("Attack.Close" if settings["shape"] == "Slash" else "Attack.Ranged")
    profile.set_editor_property("effect_tags", make_tags(tags))
    profile.set_editor_property("effect_class", require(unreal.load_class(None, "/Script/ProjectA.GE_" + settings["effect"]), "Native GameplayEffect is missing"))
    profile.set_editor_property("vfx", make_vfx(entry["vfx"], inspections))
    if entry.get("impact"):
        profile.set_editor_property("impact_vfx", make_vfx(entry["impact"], inspections))
    return profile


def description(entry):
    power = SPEC["defaults"]["power"]
    return {"area": f"선택한 지점의 범위 안 적들에게 각각 {power:g} 피해를 줍니다.", "line": f"전방 직선 범위 안 적들에게 각각 {power:g} 피해를 줍니다.", "link": f"선택한 적 하나를 연결해 {power:g} 피해를 줍니다.", "slash": f"접근한 뒤 전방 범위 안 적들에게 각각 {power:g} 피해를 줍니다.", "projectile": f"직선 투사체로 처음 부딪힌 적에게 {power:g} 피해를 줍니다. 적이나 장애물에 충돌하면 소멸합니다.", "heal": f"대상 아군의 HP를 {power:g} 회복합니다.", "shield": f"대상 아군에게 피해 {power:g}을 흡수하는 보호막을 부여합니다. 남은 보호막은 라운드 종료 시 사라집니다."}[entry["profile"]]


def main():
    selected = [entry for entry in SPEC["entries"] if entry["profile"]]
    require(len(selected) == 60 and len({entry["skill_id"] for entry in selected}) == 60 and len({entry["destination"] for entry in selected}) == 60, "Specification identities must be unique")
    require(all(entry["skill_id"].startswith("DrGame_") for entry in selected), "New skill IDs must be distinct from the retired catalog")
    require(all(entry["skill_id"] not in REMOVED_SKILL_IDS for entry in selected), "Retired skill ID reuse is forbidden")
    # Protect every purchased source and all retained skill packages before any authoring mutation.
    # 작성 변경에 앞서 구입 원본 전체와 보존한 스킬 패키지를 보호합니다.
    source_hashes = {str(path.relative_to(ROOT)): file_hash(path) for pack in SPEC["sources"] for path in (ROOT / "Content" / pack["root"].removeprefix("/Game/")).rglob("*") if path.is_file()}
    original_map = ROOT / "Content/Untitled.umap"
    if original_map.is_file():
        source_hashes[str(original_map.relative_to(ROOT))] = file_hash(original_map)
    installed_baseline = REPORT_DIR / "OriginalHashes.json"
    if installed_baseline.is_file():
        installed_hashes = json.loads(installed_baseline.read_text(encoding="utf-8"))
        require(all(file_hash(ROOT / filename) == expected for filename, expected in installed_hashes.items()), "Installed source baseline changed before authoring")
    new_files = {package_file(entry["destination"]) for entry in selected}
    retained_hashes = {str(path.relative_to(ROOT)): file_hash(path) for path in (ROOT / "Content/User_JeHoon/Blueprint/DataAsset/Skills").rglob("*.uasset") if path not in new_files}
    validator = require(unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem), "Validator subsystem is missing")
    inspections, skills, created, warnings = {}, [], [], []
    for entry in selected:
        profile = make_profile(entry, inspections)
        settings = SPEC["profiles"][entry["profile"]]
        properties = {"skill_id": entry["skill_id"], "skill_name": entry["name"], "skill_description": description(entry), "use_round_definition": True, "round_definition": profile, "ability_class": None, "action_point_cost": SPEC["defaults"]["action_point_cost"], "target_rule": getattr(unreal.SkillTargetRule, settings["target_rule"]), "area_type": unreal.SkillAreaType.SINGLE, "area_radius": 0, "move_to_target": settings["approach"] == "UNIT"}
        skill, is_new = obtain(entry["destination"], unreal.SkillDefinitionDataAsset)
        set_or_check(skill, properties, is_new)
        if is_new:
            ASSETS.set_metadata_tag(skill, "DrGameSource", entry["source"])
            created.append(skill)
        result, errors, asset_warnings = validator.is_object_valid(skill, unreal.DataValidationUsecase.SCRIPT)
        require(result == unreal.DataValidationResult.VALID and not errors, "Skill data validation failed: " + entry["destination"] + " / " + "; ".join(str(error) for error in errors))
        warnings.extend(str(warning) for warning in asset_warnings)
        skills.append(skill)
    # Keep the shared starting attack and monster attacks unchanged; expose new skills through the existing shop pool.
    # 공통 시작 공격과 몬스터 공격을 보존하며 기존 상점 풀 기능으로 새 스킬을 제공합니다.
    melee = load("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack")
    pool, is_new = obtain(SPEC["pool"], unreal.SkillPoolDataAsset)
    entries = []
    for skill in [melee] + skills:
        entry = unreal.SkillPoolEntry()
        entry.set_editor_property("skill", skill)
        entry.set_editor_property("weight", 1)
        entries.append(entry)
    candidate_ids = [str(entry.get_editor_property("skill").get_editor_property("skill_id")) for entry in entries]
    require(len(candidate_ids) == len(set(candidate_ids)) == 61 and all(identifier not in REMOVED_SKILL_IDS for identifier in candidate_ids), "Shop candidate IDs must be unique and current")
    if is_new:
        pool.set_editor_property("entries", entries)
        created.append(pool)
    else:
        actual = list(pool.get_editor_property("entries"))
        require(len(actual) == len(entries) and all(normalized(a) == normalized(e) for a, e in zip(actual, entries)), "Existing shop pool changed")
    run_pool, is_new = obtain(SPEC["run_pool"], unreal.RunEncounterPoolDataAsset)
    set_or_check(run_pool, {"skill_shop_pool": pool, "fixed_skill_offers": []}, is_new)
    require(len(run_pool.get_editor_property("fixed_offers")) == 3, "Native encounter shop choices must remain intact")
    if is_new:
        created.append(run_pool)
    party = load(SPEC["party"])
    previous_pool = party.get_editor_property("run_encounter_pool")
    require(previous_pool is None or previous_pool == run_pool, "Party has an unrelated Run pool; preserve it for review")
    party_changed = previous_pool != run_pool
    require(not VERIFY or not party_changed, "Saved party is not connected to the new Run shop pool")
    before = {name: normalized(party.get_editor_property(name)) for name in ["unarmed_starting_skill", "encounter_skill_pool", "fallback_player_unit_class"]}
    professions = party.get_editor_property("professions")
    profession_values = {str(name): normalized(value) for name, value in professions.items()}
    if party_changed:
        party.set_editor_property("run_encounter_pool", run_pool)
    require({name: normalized(party.get_editor_property(name)) for name in before} == before and {str(name): normalized(value) for name, value in party.get_editor_property("professions").items()} == profession_values, "Party starting content changed")
    if not VERIFY:
        for asset in created:
            save(asset)
        if party_changed:
            save(party)
    for filename, expected in {**source_hashes, **retained_hashes}.items():
        require(file_hash(ROOT / filename) == expected, "Protected package changed: " + filename)
    report = {"mode": "reload" if VERIFY else "author", "specification_sha256": file_hash(Path(__file__).with_name("DrGameSkillSpecs.json")), "skills": len(skills), "profiles": dict(Counter(entry["profile"] for entry in selected)), "source_effects": len(SPEC["entries"]), "auxiliary_effects": len(SPEC["entries"]) - len(skills), "created_packages": len(created), "shop_candidates": len(entries), "pool": SPEC["pool"], "run_pool": SPEC["run_pool"], "party": SPEC["party"], "party_reference_changed": party_changed, "retained_skill_count": len(retained_hashes), "source_files_unchanged": len(source_hashes), "data_validation": "passed", "warnings": warnings, "assets": [skill.get_path_name() for skill in skills], "niagara": inspections, "gameplay_test": "not run", "visual_alignment": "user verification pending", "sfx_playback": "user verification pending"}
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    output = REPORT_DIR / ("Reload.json" if VERIFY else "Author.json")
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DRGAME_SKILLS " + str(output))


if __name__ == "__main__":
    main()
