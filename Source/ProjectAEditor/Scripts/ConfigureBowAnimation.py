import csv
import hashlib
import json
import math
import shutil
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportParagonAnimations import DESTINATION, MESH_PATH, SOURCE, import_animation, load_manny, verify_animation


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC = json.loads(Path(__file__).with_name("BowAnimationSpecs.json").read_text(encoding="utf-8"))
SKILL_SPEC = json.loads(Path(__file__).with_name("DrGameSkillSpecs.json").read_text(encoding="utf-8"))
ASSETS = unreal.EditorAssetLibrary
HELPER = unreal.WarriorAssetLibrary
OWNER_KEY = "ProjectA.BowAnimation"
OWNER = "BowAnimation.v1"
REPORT_DIR = ROOT / "Saved/Automation/BowAnimation_20261009"
BODY_PATHS = ["/Game/Primitive_Characters_Pack/Mesh/Primitive_01/Mesh_UE5/Separate/SKM_Primitive_Charater_01_Body", "/Game/Primitive_Characters_Pack/Mesh/Primitive_02/Mesh_UE5/Separate/SKM_Primitive_02_Body"]


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def package_file(package):
    require(package.startswith("/Game/"), "Expected a game package")
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def protected_hashes(outputs):
    folders = ["User_JeHoon", "Characters/Mannequins", "Primitive_Characters_Pack", "Skeleton_Guard", "ParagonAnimationsRetargetedToManny/SparrowManny"]
    files = {path for folder in folders for path in (ROOT / "Content" / folder).rglob("*") if path.is_file() and path not in outputs}
    files.add(ROOT / "DataCatalogs/SKILL_BALANCE.csv")
    return {str(path.relative_to(ROOT)): file_hash(path) for path in sorted(files)}


def save(asset, outputs):
    require(package_file(asset.get_path_name().split(".")[0]) in outputs, "Save outside the declared bow outputs")
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save: " + asset.get_path_name())


def montage_arguments(sequence):
    return [sequence, sequence, SPEC["draw_start"], SPEC["draw_end"], SPEC["release_start"], SPEC["release_end"], SPEC["draw_duration"]]


def validate_specs():
    require(SPEC["version"] == 1 and len(SPEC["skills"]) == 2 and len(set(SPEC["skills"])) == 2, "Expected the two authored bow skills")
    selected = [entry for entry in SKILL_SPEC["entries"] if entry.get("destination") in SPEC["skills"]]
    require(len(selected) == 2 and all(entry.get("cast_montage") == SPEC["montage"] for entry in selected), "DrGame bow montage declarations differ")
    require(math.isclose(SKILL_SPEC["defaults"]["windup_seconds"], SPEC["draw_duration"], abs_tol=0.0001), "Default windup changed; review the bow montage timing")
    with (ROOT / "DataCatalogs/SKILL_BALANCE.csv").open(encoding="utf-8-sig", newline="") as handle:
        rows = [row for row in csv.DictReader(handle) if row["스킬 경로"].split(".")[0] in SPEC["skills"]]
    require(len(rows) == 2 and all(math.isclose(float(row["선딜(초)"]), SPEC["draw_duration"], abs_tol=0.0001) for row in rows), "Balance windup changed; review the bow montage timing")
    source = (SOURCE / (SPEC["source"] + ".FBX")).resolve()
    require(source.is_relative_to(SOURCE.resolve()) and source.is_file(), "Missing selected original FBX")
    require(SPEC["montage"].startswith(DESTINATION + "/SparrowManny/"), "Montage must preserve its source pack path")
    return source


def prepare_skills():
    prepared = []
    for path in SPEC["skills"]:
        skill = load(path)
        require(isinstance(skill, unreal.SkillDefinitionDataAsset) and ASSETS.get_metadata_tag(skill, "ProjectA.DrGameSkills") == "DrGameSkills.v1", "Unknown skill owner: " + path)
        require(skill.get_editor_property("use_round_definition"), "Bow skill must use its authored round definition")
        profile = skill.get_editor_property("round_definition")
        current = profile.get_editor_property("cast_montage")
        require(current is None or current.get_path_name().split(".")[0] == SPEC["montage"], "Preserve a differently authored cast montage: " + path)
        require(math.isclose(profile.get_editor_property("windup_seconds"), SPEC["draw_duration"], abs_tol=0.0001), "Authored skill windup changed")
        require(not profile.get_editor_property("use_weapon_trace"), "Bow animation must not change server weapon sampling")
        # Keep a serialized snapshot because nested Unreal struct getters can be live views.
        # Unreal 중첩 구조체 getter는 원본을 가리킬 수 있으므로 직렬화한 값을 보존합니다.
        before = profile.export_text()
        preserved = {field: str(skill.get_editor_property(field)) for field in ["skill_id", "skill_name", "skill_description", "ability_class", "action_point_cost", "target_rule"]}
        prepared.append((skill, before, current, preserved))
    return prepared


def author_montage(sequence, outputs, path):
    if ASSETS.does_asset_exist(path):
        montage = load(path)
        require(isinstance(montage, unreal.AnimMontage) and ASSETS.get_metadata_tag(montage, OWNER_KEY) == OWNER, "Montage destination belongs to another author")
    else:
        folder, name = path.rsplit("/", 1)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
        factory.set_editor_property("source_animation", sequence)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory), "Could not create bow montage")
    require(HELPER.configure_timed_attack_montage(montage, *montage_arguments(sequence)), "Could not configure bow segments")
    ASSETS.set_metadata_tag(montage, OWNER_KEY, OWNER)
    save(montage, outputs)
    require(HELPER.validate_timed_attack_montage(montage, *montage_arguments(sequence)), "Saved bow montage graph differs")
    return montage


def author_legacy(sequence, montage, outputs):
    legacy = SPEC["legacy"]
    blueprint = load(legacy["blueprint"])
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    target = load(legacy["mesh"])
    require(component.get_editor_property("skeletal_mesh_asset") == target, "Legacy snapshot body changed")
    animation = load(component.get_editor_property("anim_class").get_path_name().removesuffix("_C"))
    require(HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Legacy snapshot must already output DefaultSlot")
    before_overrides = dict(defaults.get_editor_property("round_montage_overrides"))
    preserved = {field: str(defaults.get_editor_property(field)) for field in ["equipped_skill_data_assets", "equipped_skill_ability_classes", "default_attack_ability_class"]}
    animation_class = component.get_editor_property("anim_class")
    source_path = sequence.get_path_name().split(".")[0]
    destination = source_path + legacy["suffix"]
    if ASSETS.does_asset_exist(destination):
        converted = load(destination)
        require(ASSETS.get_metadata_tag(converted, OWNER_KEY) == OWNER, "Legacy sequence belongs to another author")
    else:
        # Retarget only the selected sequence through the existing rig without regenerating bodies or locomotion.
        # 기존 Rig로 선택 시퀀스만 리타깃하며 몸체나 이동 애니메이션은 다시 생성하지 않습니다.
        retargeter = load(legacy["retargeter"])
        require(HELPER.retarget_animations([sequence], load(MESH_PATH), target, retargeter, destination.rsplit("/", 1)[0], legacy["suffix"], False, False), "Could not retarget the legacy bow sequence")
        converted = load(destination)
    require(isinstance(converted, unreal.AnimSequence) and converted.get_editor_property("skeleton") == target.get_editor_property("skeleton"), "Legacy bow skeleton differs")
    require(math.isclose(converted.get_play_length(), sequence.get_play_length(), abs_tol=0.0001), "Legacy retarget changed source timing")
    ASSETS.set_metadata_tag(converted, OWNER_KEY, OWNER)
    save(converted, outputs)
    replacement = author_montage(converted, outputs, SPEC["montage"] + legacy["suffix"])
    require(montage not in before_overrides or before_overrides[montage] == replacement, "Preserve the authored legacy bow override")
    overrides = dict(before_overrides)
    overrides[montage] = replacement
    defaults.set_editor_property("round_montage_overrides", overrides)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint, outputs)
    defaults = unreal.get_default_object(blueprint.generated_class())
    require(dict(defaults.get_editor_property("round_montage_overrides")) == overrides and preserved == {field: str(defaults.get_editor_property(field)) for field in preserved}, "Legacy montage mapping or loadout differs")
    component = defaults.get_editor_property("mesh")
    require(component.get_editor_property("skeletal_mesh_asset") == target and component.get_editor_property("anim_class") == animation_class, "Legacy mesh or animation class changed")
    return {"blueprint": blueprint.get_path_name(), "sequence": converted.get_path_name(), "montage": replacement.get_path_name(), "seconds": replacement.get_play_length(), "source_montage": montage.get_path_name(), "existing_overrides_preserved": True, "mesh": target.get_path_name(), "skeleton": target.get_editor_property("skeleton").get_path_name()}


def bind_skills(prepared, montage, outputs):
    records = []
    for skill, before, original_montage, preserved in prepared:
        profile = unreal.CombatRoundSkill()
        require(profile.import_text(before), "Could not copy authored round definition")
        profile.set_editor_property("cast_montage", montage)
        skill.set_editor_property("round_definition", profile)
        save(skill, outputs)
        actual = skill.get_editor_property("round_definition")
        require(actual.get_editor_property("cast_montage") == montage, "Saved bow montage reference differs")
        restored = unreal.CombatRoundSkill()
        require(restored.import_text(actual.export_text()), "Could not inspect saved round definition")
        restored.set_editor_property("cast_montage", original_montage)
        require(restored.export_text() == before, "A nonpresentation skill field changed")
        require(preserved == {field: str(skill.get_editor_property(field)) for field in preserved}, "Skill identity or legacy fields changed")
        records.append({"skill": skill.get_path_name(), "before": before, "after": actual.export_text(), "only_cast_montage_changed": True})
    return records


def validate_bodies(sequence, skeleton):
    animation = load("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")
    require(animation.get_editor_property("target_skeleton") == skeleton and HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Shared animation blueprint must already output DefaultSlot")
    bodies = []
    for path in BODY_PATHS:
        mesh = load(path)
        require(unreal.CharacterAppearanceAssetLibrary.validate_body_animation_skeleton(mesh, skeleton), "Body is not compatible with the original Manny skeleton")
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("optional_skeletal_mesh", mesh)
        samples = []
        for seconds in sorted(set([SPEC["draw_start"], SPEC["draw_end"], SPEC["release_start"], SPEC["release_end"]])):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, seconds, options)
            require(unreal.AnimPoseExtensions.is_valid(pose), "Invalid imported body pose")
            bones = {}
            for bone in ["root", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"]:
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                require(all(math.isfinite(value) for value in transform.translation.to_tuple()) and (transform.scale3d - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Invalid imported body bone: " + bone)
                bones[bone] = list(transform.translation.to_tuple())
            samples.append({"source_seconds": seconds, "bones": bones})
        bodies.append({"mesh": path, "skeleton": mesh.get_editor_property("skeleton").get_path_name(), "samples": samples})
    return bodies


def main():
    source = validate_specs()
    animation_path = DESTINATION + "/" + SPEC["source"]
    outputs = {package_file(path) for path in SPEC["skills"] + [animation_path, SPEC["montage"], animation_path + SPEC["legacy"]["suffix"], SPEC["montage"] + SPEC["legacy"]["suffix"], SPEC["legacy"]["blueprint"]]}
    require(all(path.is_relative_to(ROOT / "Content/User_JeHoon") for path in outputs), "Outputs must be project-owned")
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    before_outputs = {str(path.relative_to(ROOT)): file_hash(path) for path in sorted(outputs) if path.is_file()}
    for path in sorted(outputs):
        backup = REPORT_DIR / "BeforeAssets" / path.name
        if path.is_file() and not backup.exists():
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, backup)
    before = protected_hashes(outputs)
    report = {"status": "in progress", "before_outputs": before_outputs, "protected_file_count": len(before), "gameplay_test": "not run", "render_test": "not run", "reload_validation": "saved and inspected in the authoring process; no separate process reload", "original_fbx": str(source.relative_to(ROOT)), "original_fbx_bytes": source.stat().st_size, "original_fbx_sha256": file_hash(source)}
    try:
        prepared = prepare_skills()
        skeleton = load_manny()
        sequence = load(animation_path) if ASSETS.does_asset_exist(animation_path) else import_animation(source, animation_path, skeleton)
        report["animation"] = verify_animation(sequence, source, skeleton, load(MESH_PATH))
        report["bodies"] = validate_bodies(sequence, skeleton)
        montage = author_montage(sequence, outputs, SPEC["montage"])
        report["montage"] = {"path": montage.get_path_name(), "seconds": montage.get_editor_property("sequence_length"), "specification": SPEC, "graph_validation": "passed"}
        report["legacy"] = author_legacy(sequence, montage, outputs)
        report["skills"] = bind_skills(prepared, montage, outputs)
        report["status"] = "passed"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        report["protected_files_unchanged"] = before == protected_hashes(outputs)
        report["outputs"] = {str(path.relative_to(ROOT)): {"bytes": path.stat().st_size, "sha256": file_hash(path)} for path in sorted(outputs) if path.is_file()}
        if not report["protected_files_unchanged"]:
            report["status"] = "failed"
        (REPORT_DIR / "Author.json").write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        require(report["protected_files_unchanged"], "Protected originals or unrelated project assets changed")
    unreal.log("BOW_ANIMATION_AUTHORED " + str(REPORT_DIR / "Author.json"))


if __name__ == "__main__":
    main()
