import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSETS = unreal.EditorAssetLibrary
BASE = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_MeleeAttack"
SOURCE_MONTAGE = "/Game/User_JeHoon/Assets/ParagonAnimationsRetargetedToManny/GreystoneManny/Attack/AM_MeleeAttack"
FOLDER = SOURCE_MONTAGE.rsplit("/", 1)[0]
MONTAGE = FOLDER + "/AM_DoubleAttack"
SKILL = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_DoubleAttack"
RIGHT = FOLDER + "/AS_DoubleAttack_Right"
LEFT = FOLDER + "/AS_DoubleAttack_Left"
OWNER_KEY = "ProjectA.DoubleAttack"
OWNER = "DoubleAttack.MirroredMelee.v2"
VERIFY = "-DoubleAttackVerifyOnly" in unreal.SystemLibrary.get_command_line()
OVERWRITE = "-DoubleAttackOverwrite" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def package_file(path):
    return ROOT / "Content" / (path.split(".")[0].removeprefix("/Game/") + ".uasset")


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def existing(path, asset_type):
    asset = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if asset:
        owner = ASSETS.get_metadata_tag(asset, OWNER_KEY)
        require(isinstance(asset, asset_type) and owner in [OWNER, "DoubleAttack.v1"], "Preserve independently authored destination: " + path)
        require(owner == OWNER or (not VERIFY and OVERWRITE), "Use explicit overwrite to migrate the previous double attack")
    else:
        require(not VERIFY, "Missing baked asset: " + path)
    return asset


def save(asset):
    require(not VERIFY, "Read-only mode cannot save")
    ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + asset.get_path_name())


def motion(path, source, mirror):
    sequence = existing(path, unreal.AnimSequence)
    create = sequence is None
    if create:
        factory = unreal.AnimSequenceFactory()
        factory.set_editor_property("target_skeleton", source.get_editor_property("skeleton"))
        factory.set_editor_property("preview_skeletal_mesh", unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"))
        folder, name = path.rsplit("/", 1)
        sequence = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimSequence, factory), "Sequence factory failed")
    if not VERIFY and (create or OVERWRITE):
        require(unreal.EquipmentSkillAssetLibrary.bake_melee_montage_motion(sequence, source, mirror), "Melee pose baking failed: " + path)
        save(sequence)
    require(unreal.EquipmentSkillAssetLibrary.validate_melee_montage_motion(sequence, source, mirror), "Baked bone keys differ from the requested melee motion: " + path)
    return sequence



def validate_hands(left, right):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", require(unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"), "Missing original Manny preview mesh"))
    right_positions = []
    maximum_error = 0.0
    for frame in range(17):
        seconds = right.get_play_length() * frame / 16
        right_pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(right, seconds, options)
        left_pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(left, seconds, options)
        require(unreal.AnimPoseExtensions.is_valid(right_pose) and unreal.AnimPoseExtensions.is_valid(left_pose), "Invalid baked hand pose")
        for original, mirrored in [("hand_r", "hand_l"), ("hand_l", "hand_r")]:
            position = unreal.AnimPoseExtensions.get_bone_pose(right_pose, original, unreal.AnimPoseSpaces.WORLD).translation
            reflection = unreal.AnimPoseExtensions.get_bone_pose(left_pose, mirrored, unreal.AnimPoseSpaces.WORLD).translation
            error = (reflection - unreal.Vector(-position.x, position.y, position.z)).length()
            maximum_error = max(maximum_error, error)
            if original == "hand_r":
                right_positions.append(position)
    travel = max((position - right_positions[0]).length() for position in right_positions)
    require(travel > 10.0 and maximum_error < 1.0, "Hand motion is static or its mirrored trajectory differs: " + str([travel, maximum_error]))
    return {"samples": 17, "hand_travel_cm": travel, "maximum_mirror_error_cm": maximum_error}


def resolved(asset):
    result = asset.resolve_round_skill()
    return result if isinstance(result, unreal.CombatRoundSkill) else next(value for value in result if isinstance(value, unreal.CombatRoundSkill))


def main():
    require(not (VERIFY and OVERWRITE), "Use verification or explicit overwrite separately")
    source_paths = [SOURCE_MONTAGE, SOURCE_MONTAGE.replace("/Assets/", "/")]
    source_path = require(next((path for path in source_paths if ASSETS.does_asset_exist(path)), None), "Missing authored MeleeAttack montage")
    source = require(unreal.load_asset(source_path), "Cannot load the authored MeleeAttack montage")
    base = require(unreal.load_asset(BASE), "Missing basic melee attack")
    sources = unreal.WarriorAssetLibrary.get_montage_animations(source)
    protected = {package_file(path) for path in [BASE, source_path, *[sequence.get_path_name() for sequence in sources]]}
    before = {str(path): file_hash(path) for path in protected}
    right = motion(RIGHT, source, False)
    left = motion(LEFT, source, True)
    hands = validate_hands(left, right)
    duration = right.get_play_length()
    require(abs(duration - left.get_play_length()) < 0.0001, "Mirrored motions have different durations")
    montage = existing(MONTAGE, unreal.AnimMontage)
    create = montage is None
    if create:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", source.get_editor_property("skeleton"))
        factory.set_editor_property("source_animation", left)
        folder, name = MONTAGE.rsplit("/", 1)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory), "Montage factory failed")
    arguments = [left, right, 0.0, duration, 0.0, duration, duration]
    if not VERIFY and (create or OVERWRITE):
        require(unreal.WarriorAssetLibrary.configure_timed_attack_montage(montage, *arguments), "Two-strike montage authoring failed")
        save(montage)
    require(unreal.WarriorAssetLibrary.validate_timed_attack_montage(montage, *arguments), "Saved montage does not repeat the mirrored and original melee motions")
    skill = existing(SKILL, unreal.SkillDefinitionDataAsset)
    create = skill is None
    if create:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.SkillDefinitionDataAsset)
        folder, name = SKILL.rsplit("/", 1)
        skill = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.SkillDefinitionDataAsset, factory), "Skill factory failed")
    windup = resolved(base).get_editor_property("windup_seconds")
    if not VERIFY and (create or OVERWRITE):
        require(unreal.EquipmentSkillAssetLibrary.configure_double_attack(skill, base, montage, windup, duration), "Skill authoring failed")
        save(skill)
    result = unreal.EquipmentSkillAssetLibrary.validate_double_attack(skill, base)
    contract = result if isinstance(result, str) else result[1] if isinstance(result, tuple) and result[0] else None
    require(contract and contract.startswith("Dual daggers required;"), "Double attack static contract failed: " + str(result))
    require(before == {str(path): file_hash(path) for path in protected}, "Protected melee source files changed")
    report = {"mode": "verify" if VERIFY else "author", "skill": skill.get_path_name(), "montage": montage.get_path_name(), "source_montage": source.get_path_name(),
              "left": left.get_path_name(), "right": right.get_path_name(), "hands": hands, "strike_seconds": [windup, windup + duration], "static_contract": contract, "source_hashes": before,
              "runtime_review": "User review pending; no gameplay or automation tests executed."}
    output = ROOT / "Saved/DoubleAttack" / ("Verify.json" if VERIFY else "Author.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[DoubleAttack] Mirrored MeleeAttack then original MeleeAttack; " + contract)


if __name__ == "__main__":
    main()
