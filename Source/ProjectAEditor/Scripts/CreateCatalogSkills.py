import csv
import hashlib
import json
import math
import re
from collections import Counter
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_PATH = Path(__file__).resolve().with_name("CatalogSkillSpecs.json")
CATALOG_ROOT = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/"
VERIFY_ONLY = "-CatalogSkillsVerifyOnly" in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def file_hash(path):
    result = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def package_file(package):
    require(package.startswith("/Game/"), "Only project packages are supported: " + package)
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def make_tags(names):
    tags = unreal.GameplayTagContainer()
    require(tags.import_text("(GameplayTags=(" + ",".join('(TagName="' + name + '")' for name in names) + "))"), "Could not import catalog GameplayTags")
    imported = re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', tags.export_text())
    require(sorted(imported) == sorted(names), "Catalog GameplayTags changed during import: " + tags.export_text())
    return tags


def make_profile(spec, entry, source):
    settings = spec["profiles"][entry["profile"]]
    support = settings["effect"] in ("Heal", "Shield")
    profile = unreal.CombatRoundSkill()
    for key, value in spec["defaults"].items():
        profile.set_editor_property(key, value)
    profile.set_editor_property("skill_id", entry["skill_id"])
    profile.set_editor_property("name", entry["name"])
    profile.set_editor_property("kind", getattr(unreal.CombatRoundSkillKind, settings["kind"]))
    profile.set_editor_property("target_rule", getattr(unreal.SkillTargetRule, settings["target_rule"]))
    profile.set_editor_property("approach", getattr(unreal.CombatRoundApproach, settings["approach"]))
    profile.set_editor_property("target_loss", unreal.CombatRoundTargetLoss.CANCEL if support else unreal.CombatRoundTargetLoss.KEEP_LOCATION if settings["kind"] == "GROUND_ATTACK" else unreal.CombatRoundTargetLoss.NEAREST_ENEMY)
    profile.set_editor_property("target_only", support)
    profile.set_editor_property("remain_at_destination", False)
    profile.set_editor_property("homing", False)
    profile.set_editor_property("use_weapon_trace", False)
    profile.set_editor_property("use_melee_area_collision", False)
    profile.set_editor_property("use_effect_collision", settings["kind"] != "PROJECTILE")
    profile.set_editor_property("effect_half_extent", unreal.Vector(*settings["half_extent"]))
    profile.set_editor_property("effect_offset", unreal.Vector(*settings["offset"]))
    profile.set_editor_property("effect_travel", unreal.Vector(*settings.get("travel", [0, 0, 0])))
    profile.set_editor_property("effect_duration", settings["duration"])
    profile.set_editor_property("effect_sphere", settings["sphere"])
    tags = entry["element_tags"] + ["Skill.Effect." + settings["effect"], "Skill.Shape." + settings["shape"]]
    if settings["effect"] == "Damage":
        tags.append("Attack.Close" if settings["shape"] == "Slash" else "Attack.Ranged")
    profile.set_editor_property("effect_tags", make_tags(tags))
    effect_class = require(unreal.load_class(None, "/Script/ProjectA.GE_" + settings["effect"]), "Missing native GameplayEffect: " + settings["effect"])
    profile.set_editor_property("effect_class", effect_class)
    vfx = unreal.CombatSkillVfx()
    presentation_source = entry.get("direction_source", entry["source"])
    if presentation_source != entry["source"]:
        require(presentation_source == "/Game/User_JeHoon/" + entry["source"].removeprefix("/Game/") + "_TargetDirection", "Direction derivative must preserve its source pack and folder: " + presentation_source)
        source = require(unreal.load_asset(presentation_source), "Missing authored direction derivative: " + presentation_source)
    vfx.set_editor_property("niagara" if entry["source_class"] == "NiagaraSystem" else "cascade", source)
    profile.set_editor_property("vfx", vfx)
    # Optional per-skill timing keeps source visuals intact while authoring their collision and flight clocks.
    # 스킬별 선택 시간 설정으로 원본 연출을 유지하며 충돌과 비행 시계를 작성합니다.
    timing = entry.get("timing", {})
    require(isinstance(timing, dict), "Skill timing must be an object: " + entry["source"])
    allowed = {"windup_seconds", "projectile_speed", "projectile_lifetime"} if settings["kind"] == "PROJECTILE" else {"windup_seconds", "effect_hit_delay_seconds", "effect_duration"}
    for key, value in timing.items():
        require(key in allowed, "Unsupported timing field for this skill: " + entry["source"] + " / " + key)
        require(isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value), "Skill timing must be a finite number: " + entry["source"] + " / " + key)
        limits = {"windup_seconds": (0.0, 60.0), "effect_hit_delay_seconds": (0.0, 10.0), "effect_duration": (0.01, 10.0), "projectile_speed": (0.01, 100000.0), "projectile_lifetime": (0.01, 60.0)}
        minimum, maximum = limits[key]
        require(minimum <= value <= maximum, "Skill timing is outside its supported range: " + entry["source"] + " / " + key)
        profile.set_editor_property(key, value)
    return profile


def description(spec, entry):
    power = spec["defaults"]["power"]
    descriptions = {"slash": f"전방 범위 안의 적들에게 각각 {power:g} 피해를 줍니다.", "spin": f"주변의 적들에게 각각 {power:g} 피해를 줍니다.", "projectile": f"단일 투사체를 발사해 처음 부딪힌 적에게 {power:g} 피해를 줍니다. 적이나 장애물에 충돌하면 소멸합니다.", "area": f"선택한 지점의 범위 안 적들에게 각각 {power:g} 피해를 줍니다.", "beam": f"전방 직선 범위 안의 적들에게 각각 {power:g} 피해를 줍니다.", "heal": f"대상 아군의 HP를 즉시 {power:g} 회복합니다.", "shield": f"대상 아군에게 피해 {power:g}을 흡수하는 보호막을 부여합니다. 남은 보호막은 해당 라운드 종료 시 사라집니다."}
    return descriptions[entry["profile"]]


def expected_properties(spec, entry, profile):
    settings = spec["profiles"][entry["profile"]]
    return {"skill_id": entry["skill_id"], "skill_name": entry["name"], "skill_description": description(spec, entry), "ability_class": None, "use_round_definition": True, "round_definition": profile, "action_point_cost": spec["defaults"]["action_point_cost"], "target_rule": getattr(unreal.SkillTargetRule, settings["target_rule"]), "area_type": unreal.SkillAreaType.SINGLE, "area_radius": 0, "move_to_target": settings["approach"] == "UNIT"}


def normalized(value):
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    return value


def verify_properties(asset, expected):
    for name, value in expected.items():
        require(normalized(asset.get_editor_property(name)) == normalized(value), "Existing asset differs; review without overwriting: " + asset.get_path_name() + " / " + name)


def create_data_asset(package, asset_class):
    folder, name = package.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return require(TOOLS.create_asset(name, folder, asset_class, factory), "Could not create " + package)


def main():
    spec = json.loads(SPEC_PATH.read_text(encoding="utf-8"))
    source_csv = ROOT / spec["source_csv"]
    require(file_hash(source_csv) == spec["source_csv_sha256"], "Source CSV changed; review the catalog specification before authoring")
    source_rows = list(csv.DictReader(source_csv.open(encoding="utf-8-sig", newline="")))
    entries = spec["entries"]
    require(len(entries) == len(source_rows) == spec["source_count"], "Source row count mismatch")
    for row, entry in zip(source_rows, entries):
        require(row["위치"] + "/" + row["에셋 이름"] == entry["source"] and row["게임 내 이름"] == entry["name"] and row["속성·테마"] == entry["theme"], "Source identity/name/theme changed: " + entry["source"])
    selected = [entry for entry in entries if entry["profile"]]
    require(len({entry["destination"].rsplit("/", 1)[-1].casefold() for entry in selected}) == len(selected), "Duplicate catalog primary asset names")
    require(len({entry["skill_id"] for entry in selected}) == len(selected), "Duplicate catalog skill IDs")
    require(all(entry["destination"].rsplit("/", 1)[0] + "/" == CATALOG_ROOT for entry in selected), "Catalog destination must be directly under the Skills folder")

    # Hash source packages and existing skill content; only newly authored catalog packages may change.
    # 원본 패키지와 기존 스킬 콘텐츠를 해시 검사하며 새 카탈로그 패키지만 작성합니다.
    protected_files = {package_file(entry["source"]) for entry in entries}
    catalog_files = {package_file(entry["destination"]) for entry in selected} | {package_file(spec["pool"])}
    for folder in ["Skills", "SkillPools"]:
        for filename in (ROOT / "Content/User_JeHoon/Blueprint/DataAsset" / folder).rglob("*.uasset"):
            if filename not in catalog_files:
                protected_files.add(filename)
    protected_hashes = {str(filename.relative_to(ROOT)): file_hash(filename) for filename in sorted(protected_files)}
    spec_hash = file_hash(SPEC_PATH)
    validator = require(unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem), "Missing editor data validation subsystem")
    authored = []
    created = []
    warnings = []
    for entry in selected:
        source = require(unreal.load_asset(entry["source"]), "Missing source VFX: " + entry["source"])
        expected_class = unreal.NiagaraSystem if entry["source_class"] == "NiagaraSystem" else unreal.ParticleSystem
        require(isinstance(source, expected_class), "Unexpected VFX class: " + entry["source"])
        profile = make_profile(spec, entry, source)
        properties = expected_properties(spec, entry, profile)
        exists = ASSETS.does_asset_exist(entry["destination"])
        require(exists or not VERIFY_ONLY, "Missing catalog asset in verify-only mode: " + entry["destination"])
        skill = require(unreal.load_asset(entry["destination"]), "Could not load catalog skill") if exists else create_data_asset(entry["destination"], unreal.SkillDefinitionDataAsset)
        require(isinstance(skill, unreal.SkillDefinitionDataAsset), "Destination has an unexpected asset class: " + entry["destination"])
        if exists:
            require(str(ASSETS.get_metadata_tag(skill, "CatalogSource")) == entry["source"], "Existing catalog source metadata mismatch: " + entry["destination"])
            verify_properties(skill, properties)
        else:
            for name, value in properties.items():
                skill.set_editor_property(name, value)
            ASSETS.set_metadata_tag(skill, "CatalogSource", entry["source"])
            ASSETS.set_metadata_tag(skill, "CatalogSpecificationSHA256", spec_hash)
            created.append(skill)
        result, errors, asset_warnings = validator.is_object_valid(skill, unreal.DataValidationUsecase.SCRIPT)
        require(result == unreal.DataValidationResult.VALID and not errors, "Catalog data validation failed: " + entry["destination"] + " / " + "; ".join(str(error) for error in errors))
        warnings.extend({"asset": entry["destination"], "message": str(warning)} for warning in asset_warnings)
        authored.append(skill)
    pool_exists = ASSETS.does_asset_exist(spec["pool"])
    require(pool_exists or not VERIFY_ONLY, "Catalog pool is missing in verify-only mode")
    pool = require(unreal.load_asset(spec["pool"]), "Could not load catalog pool") if pool_exists else create_data_asset(spec["pool"], unreal.SkillPoolDataAsset)
    require(isinstance(pool, unreal.SkillPoolDataAsset), "Catalog pool has an unexpected asset class")
    if pool_exists:
        pool_entries = pool.get_editor_property("entries")
        require(len(pool_entries) == len(authored) and all(entry.get_editor_property("skill") == skill and entry.get_editor_property("weight") == 1 for entry, skill in zip(pool_entries, authored)), "Existing catalog pool differs; review without overwriting")
    else:
        pool_entries = []
        for skill in authored:
            pool_entry = unreal.SkillPoolEntry()
            pool_entry.set_editor_property("skill", skill)
            pool_entry.set_editor_property("weight", 1)
            pool_entries.append(pool_entry)
        pool.set_editor_property("entries", pool_entries)
        created.append(pool)
    for asset in created:
        require(ASSETS.save_loaded_asset(asset), "Could not save " + asset.get_path_name())
    require(protected_hashes == {filename: file_hash(ROOT / filename) for filename in protected_hashes}, "Original source or existing skill packages changed")
    report = {"mode": "reload" if VERIFY_ONLY else "create", "specification_sha256": spec_hash, "source_csv_sha256": file_hash(source_csv), "source_count": len(entries), "skills": len(authored), "created_packages": len(created), "deferred": len(entries) - len(authored), "profiles": dict(Counter(entry["profile"] for entry in selected)), "pool": spec["pool"], "data_validation": "passed", "warnings": warnings, "source_packages_unchanged": True, "protected_package_count": len(protected_hashes), "protected_package_hashes": protected_hashes, "assets": [entry["destination"] for entry in selected], "gameplay_test": "not run", "visual_alignment": "not verified", "existing_pools_and_loadouts_changed": False}
    output = ROOT / "Saved/Automation/CatalogSkills" / ("Reload.json" if VERIFY_ONLY else "Creation.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("CATALOG_SKILLS_AUTHORING_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
