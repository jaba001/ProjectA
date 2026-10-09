import csv
import json
import math
import shutil
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureBowAnimation import BODY_PATHS, file_hash, load, package_file, require
from ImportParagonAnimations import DESTINATION, MESH_PATH, SOURCE, import_animation, load_manny, verify_animation
import CombatAnimationGrip


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC = json.loads(Path(__file__).with_name("CombatAnimationSpecs.json").read_text(encoding="utf-8"))
SKILLS = json.loads(Path(__file__).with_name("DrGameSkillSpecs.json").read_text(encoding="utf-8"))
VERIFY = "-CombatAnimationsAuthor" not in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
HELPER = unreal.WarriorAssetLibrary
OWNER_KEY = "ProjectA.CombatAnimations"
OWNER = "CombatAnimations.v1"
REPORT_DIR = ROOT / "Saved/Automation/CombatAnimations_20261009"


def save(asset, outputs):
    require(not VERIFY and package_file(asset.get_path_name().split(".")[0]) in outputs, "Save outside the declared animation outputs")
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save: " + asset.get_path_name())


def arguments(group, sequence):
    return [sequence, sequence, group["prepare_start"], group["release_at"], group["release_at"], group["end"], group["windup"]]


def protected_hashes(outputs):
    folders = [ROOT / "Content" / name for name in ["User_JeHoon", "Characters/Mannequins", "Primitive_Characters_Pack", "Skeleton_Guard"]]
    folders += [SOURCE / group["source"].split("/")[0] for group in SPEC["groups"]]
    folders += [ROOT / "Config", ROOT / "Saved/Config", ROOT / "Saved/SaveGames"]
    files = {path for folder in folders for path in folder.rglob("*") if path.is_file() and path not in outputs}
    files.update((ROOT / "DataCatalogs").glob("*.csv"))
    return {str(path.relative_to(ROOT)): file_hash(path) for path in sorted(files)}


def prepare():
    require(SPEC["version"] == 1, "Unknown animation specification")
    groups = {group["montage"]: group for group in SPEC["groups"]}
    require(len(groups) == len(SPEC["groups"]), "Duplicate montage group")
    entries = [entry for entry in SKILLS["entries"] if entry.get("cast_montage") in groups] + SPEC.get("extra_skills", [])
    require(len(entries) == SPEC["skill_count"] and len({entry["destination"] for entry in entries}) == len(entries), "Unexpected skill selection")
    with (ROOT / "DataCatalogs/SKILL_BALANCE.csv").open(encoding="utf-8-sig", newline="") as handle:
        balance = {row["스킬 경로"].split(".")[0]: float(row["선딜(초)"]) for row in csv.DictReader(handle)}
    prepared = []
    for entry in entries:
        group = groups[entry["cast_montage"]]
        require(entry["profile"] in group["profiles"], "Skill presentation group differs from the declared profile")
        skill = load(entry["destination"])
        require(isinstance(skill, unreal.SkillDefinitionDataAsset) and ASSETS.get_metadata_tag(skill, entry.get("owner_key", "ProjectA.DrGameSkills")) == entry.get("owner", "DrGameSkills.v1") and skill.get_editor_property("use_round_definition"), "Unknown skill ownership or profile")
        profile = skill.get_editor_property("round_definition")
        current = profile.get_editor_property("cast_montage")
        allowed_previous = group.get("previous_montages", [])
        require(current is None or current.get_path_name().split(".")[0] in [entry["cast_montage"], *allowed_previous], "Preserve an independently authored montage")
        require(not VERIFY or current is not None, "Missing authored montage")
        require(math.isclose(profile.get_editor_property("windup_seconds"), group["windup"], abs_tol=0.0001) and math.isclose(balance[entry["destination"]], group["windup"], abs_tol=0.0001), "Skill or CSV windup differs from the authored gesture")
        require(not profile.get_editor_property("use_weapon_trace"), "Do not change legacy sword trace presentation")
        preserved = {field: str(skill.get_editor_property(field)) for field in ["skill_id", "skill_name", "skill_description", "ability_class", "action_point_cost", "target_rule"]}
        prepared.append((skill, profile.export_text(), current, preserved, group))
    return prepared


def montage(group, sequence, path, outputs, recovery=None):
    if ASSETS.does_asset_exist(path):
        asset = load(path)
        require(isinstance(asset, unreal.AnimMontage) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Montage destination belongs to another author")
    else:
        require(not VERIFY, "Missing authored montage: " + path)
        folder, name = path.rsplit("/", 1)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
        factory.set_editor_property("source_animation", sequence)
        asset = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory), "Montage factory failed")
    parameters = [sequence, recovery, group["release_at"], group.get("prepare_duration", group["windup"]), group["recovery_start"]] if recovery else arguments(group, sequence)
    configure = HELPER.configure_timed_sword_montage if recovery else HELPER.configure_timed_attack_montage
    validate = HELPER.validate_timed_sword_montage if recovery else HELPER.validate_timed_attack_montage
    if not VERIFY:
        require(configure(asset, *parameters), "Could not configure gesture segments")
        ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
        save(asset, outputs)
    require(validate(asset, *parameters), "Montage graph differs from the specification")
    return asset


def body_samples(group, sequence, skeleton):
    records = []
    for path in BODY_PATHS:
        mesh = load(path)
        require(unreal.CharacterAppearanceAssetLibrary.validate_body_animation_skeleton(mesh, skeleton), "Body is incompatible with Manny")
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("optional_skeletal_mesh", mesh)
        samples = []
        for seconds in [group["prepare_start"], group["release_at"], group["end"]]:
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, seconds, options)
            require(unreal.AnimPoseExtensions.is_valid(pose), "Invalid imported body pose")
            bones = {}
            for name in ["root", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"]:
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
                require(all(math.isfinite(value) for value in transform.translation.to_tuple()) and (transform.scale3d - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Invalid body bone: " + name)
                bones[name] = list(transform.translation.to_tuple())
            samples.append({"source_seconds": seconds, "bones": bones})
        records.append({"body": path, "samples": samples})
    return records


def legacy_sequence(sequence, outputs, refresh=False):
    legacy = SPEC["legacy"]
    target = load(legacy["mesh"])
    destination = sequence.get_path_name().split(".")[0] + legacy["suffix"]
    exists = ASSETS.does_asset_exist(destination)
    if exists:
        converted = load(destination)
        require(ASSETS.get_metadata_tag(converted, OWNER_KEY) == OWNER, "Legacy sequence belongs to another author")
    if not exists or refresh:
        require(not VERIFY, "Missing legacy sequence")
        require(HELPER.retarget_animations([sequence], load(MESH_PATH), target, load(legacy["retargeter"]), destination.rsplit("/", 1)[0], legacy["suffix"], exists, False), "Selected legacy retarget failed")
        converted = load(destination)
        ASSETS.set_metadata_tag(converted, OWNER_KEY, OWNER)
        save(converted, outputs)
    require(converted.get_editor_property("skeleton") == target.get_editor_property("skeleton") and math.isclose(converted.get_play_length(), sequence.get_play_length(), abs_tol=0.0001), "Legacy skeleton or timing differs")
    return converted


def author_legacy(group, sequence, recovery, canonical, outputs):
    converted = legacy_sequence(sequence, outputs, not VERIFY and bool(group.get("grip_reference")))
    converted_recovery = legacy_sequence(recovery, outputs) if recovery else None
    replacement = montage(group, converted, group["montage"] + SPEC["legacy"]["suffix"], outputs, converted_recovery)
    return canonical, replacement


def selected_sequence(relative, skeleton):
    source = (SOURCE / (relative + ".FBX")).resolve()
    require(source.is_relative_to(SOURCE) and source.is_file(), "Missing selected original FBX")
    path = DESTINATION + "/" + relative
    require(not VERIFY or ASSETS.does_asset_exist(path), "Missing imported sequence")
    sequence = load(path) if ASSETS.does_asset_exist(path) else import_animation(source, path, skeleton)
    record = verify_animation(sequence, source, skeleton, load(MESH_PATH))
    record["source_sha256"] = file_hash(source)
    return sequence, record


def main():
    prepared = prepare()
    outputs = {package_file(skill.get_path_name().split(".")[0]) for skill, *_ in prepared}
    outputs.add(package_file(SPEC["legacy"]["blueprint"]))
    for group in SPEC["groups"]:
        require(group["montage"].startswith(DESTINATION + "/" + group["source"].rsplit("/", 1)[0] + "/"), "Montage must retain its original pack folders")
        outputs.add(package_file(group["montage"]))
        outputs.add(package_file(group["montage"] + SPEC["legacy"]["suffix"]))
        for source in [group["source"]] + ([group["recovery"]] if group.get("recovery") else []):
            outputs.add(package_file(DESTINATION + "/" + source))
            outputs.add(package_file(DESTINATION + "/" + source + SPEC["legacy"]["suffix"]))
    outputs.update(package_file(path) for path in SPEC.get("retired_montages", []))
    require(all(path.is_relative_to(ROOT / "Content/User_JeHoon") for path in outputs), "Outputs must be project-owned")
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    if not VERIFY:
        for path in outputs:
            backup = REPORT_DIR / "BeforeAssets" / path.relative_to(ROOT / "Content")
            if path.is_file() and not backup.exists():
                backup.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, backup)
    before = protected_hashes(set() if VERIFY else outputs)
    report = {"mode": "verify" if VERIFY else "author", "status": "in progress", "protected_file_count": len(before), "animations": [], "skills": [], "execution_scope": "Asset authoring or read-only reload; runtime visual review is recorded separately."}
    try:
        skeleton = load_manny()
        abp = load("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")
        require(HELPER.is_output_slot_connected(abp, "DefaultSlot"), "Current body AnimBP must output DefaultSlot")
        canonical = {}
        legacy_pairs = {}
        for group in SPEC["groups"]:
            sequence, record = selected_sequence(group["source"], skeleton)
            if group.get("grip_reference"):
                reference = load(group["grip_reference"])
                if not VERIFY:
                    record["grip_authoring"] = CombatAnimationGrip.configure(sequence, reference)
                    save(sequence, outputs)
                record["grip"] = CombatAnimationGrip.validate(sequence, reference)
            record["bodies"] = body_samples(group, sequence, skeleton)
            recovery = None
            if group.get("recovery"):
                recovery, record["recovery"] = selected_sequence(group["recovery"], skeleton)
            asset = montage(group, sequence, group["montage"], outputs, recovery)
            canonical[group["montage"]] = asset
            original, replacement = author_legacy(group, sequence, recovery, asset, outputs)
            legacy_pairs[original] = replacement
            record.update({"montage": asset.get_path_name(), "montage_seconds": asset.get_play_length(), "legacy_montage": replacement.get_path_name(), "segments_verified": True})
            report["animations"].append(record)
        blueprint = load(SPEC["legacy"]["blueprint"])
        defaults = unreal.get_default_object(blueprint.generated_class())
        component = defaults.get_editor_property("mesh")
        require(component.get_editor_property("skeletal_mesh_asset") == load(SPEC["legacy"]["mesh"]), "Legacy body differs")
        body = component.get_editor_property("skeletal_mesh_asset")
        animation_class = component.get_editor_property("anim_class")
        abp = load(component.get_editor_property("anim_class").get_path_name().removesuffix("_C"))
        require(HELPER.is_output_slot_connected(abp, "DefaultSlot"), "Legacy body AnimBP must output DefaultSlot")
        preserved = {field: str(defaults.get_editor_property(field)) for field in ["equipped_skill_data_assets", "equipped_skill_ability_classes", "default_attack_ability_class"]}
        previous = dict(defaults.get_editor_property("round_montage_overrides"))
        retired = set(SPEC.get("retired_montages", []))
        for key, value in list(previous.items()):
            if key.get_path_name().split(".")[0] in retired:
                require(not VERIFY and value.get_path_name().split(".")[0] in retired and ASSETS.get_metadata_tag(key, OWNER_KEY) == OWNER and ASSETS.get_metadata_tag(value, OWNER_KEY) == OWNER, "Unexpected retired override")
                del previous[key]
        require(all(key not in previous or previous[key] == value for key, value in legacy_pairs.items()), "Preserve independently authored legacy overrides")
        expected = {**previous, **legacy_pairs}
        if not VERIFY:
            defaults.set_editor_property("round_montage_overrides", expected)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint, outputs)
        defaults = unreal.get_default_object(blueprint.generated_class())
        require(dict(defaults.get_editor_property("round_montage_overrides")) == expected and preserved == {field: str(defaults.get_editor_property(field)) for field in preserved}, "Legacy overrides or loadout differ")
        component = defaults.get_editor_property("mesh")
        require(component.get_editor_property("skeletal_mesh_asset") == body and component.get_editor_property("anim_class") == animation_class, "Legacy body or animation class changed")
        for skill, serialized, original, preserved, group in prepared:
            if not VERIFY:
                profile = unreal.CombatRoundSkill()
                require(profile.import_text(serialized), "Could not copy the authored profile")
                profile.set_editor_property("cast_montage", canonical[group["montage"]])
                skill.set_editor_property("round_definition", profile)
                save(skill, outputs)
            actual = skill.get_editor_property("round_definition")
            require(actual.get_editor_property("cast_montage") == canonical[group["montage"]], "Saved skill montage differs")
            restored = unreal.CombatRoundSkill()
            require(restored.import_text(actual.export_text()), "Could not inspect the authored profile")
            restored.set_editor_property("cast_montage", original)
            require(restored.export_text() == serialized and preserved == {field: str(skill.get_editor_property(field)) for field in preserved}, "A nonpresentation skill field changed")
            report["skills"].append({"skill": skill.get_path_name(), "montage": canonical[group["montage"]].get_path_name(), "only_cast_montage_changed": True})
        for path in SPEC.get("retired_montages", []):
            if ASSETS.does_asset_exist(path):
                require(not VERIFY and ASSETS.get_metadata_tag(load(path), OWNER_KEY) == OWNER, "Retired montage is not owned by this author")
                require(not ASSETS.find_package_referencers_for_asset(path, load_assets_to_confirm=True), "A rejected montage still has asset references")
                require(ASSETS.delete_asset(path), "Could not remove the rejected montage through Unreal")
            require(not ASSETS.does_asset_exist(path), "A rejected montage remains")
        report["status"] = "passed"
    except Exception as error:
        report.update({"status": "failed", "error": str(error)})
        raise
    finally:
        report["protected_files_unchanged"] = before == protected_hashes(set() if VERIFY else outputs)
        report["outputs"] = {str(path.relative_to(ROOT)): {"bytes": path.stat().st_size, "sha256": file_hash(path)} for path in sorted(outputs) if path.is_file()}
        if not report["protected_files_unchanged"]:
            report["status"] = "failed"
        (REPORT_DIR / ("Reload.json" if VERIFY else "Author.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        require(report["protected_files_unchanged"], "An original, unrelated asset, CSV, configuration or save changed")
    unreal.log("COMBAT_ANIMATIONS_VERIFIED" if VERIFY else "COMBAT_ANIMATIONS_AUTHORED")


if __name__ == "__main__":
    main()
