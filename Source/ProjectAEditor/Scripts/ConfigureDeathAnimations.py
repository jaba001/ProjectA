import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from WarriorContentPaths import ENEMY_RIGS, ENEMY_SOURCE, ROOT, SWORD_SOURCE_MESH, mirrored_path


PROFESSION_DEATHS = {"Warrior": ("Kwang", "Death_Bwd"), "Archer": ("Sparrow", "Death_Bwd"), "Mage": ("Gideon", "Death_Back"), "Rogue": ("Countess", "Death")}
MANNY_DEATH = "/Game/Characters/Mannequins/Anims/Death/MM_Death_Back_01"
ENEMY_DEATH = mirrored_path(MANNY_DEATH, "_SwordEnemy")
ASSETS = unreal.EditorAssetLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def hero_death_path(hero):
    name = next(name for configured_hero, name in PROFESSION_DEATHS.values() if configured_hero == hero)
    return "/Game/Paragon" + hero + "/Characters/Heroes/" + hero + "/Animations/" + name


def assignments():
    result = [("BP_PlayerUnit", MANNY_DEATH), ("BP_EnemyUnit", ENEMY_DEATH), ("BP_SnapshotOpponent", ENEMY_DEATH)]
    for profession, (hero, name) in PROFESSION_DEATHS.items():
        result.extend(("BP_" + profession + suffix, hero_death_path(hero)) for suffix in ["Unit", "SnapshotOpponent"])
    return [(ROOT + "/Blueprint/Unit/" + blueprint, sequence) for blueprint, sequence in result]


def validate_sequence(mesh, sequence):
    require(isinstance(sequence, unreal.AnimSequence), "Death animation must be a sequence")
    require(mesh and mesh.get_editor_property("skeleton") == sequence.get_editor_property("skeleton"), "Death skeleton mismatch: " + sequence.get_path_name())
    require(math.isfinite(sequence.get_play_length()) and sequence.get_play_length() > 0.0, "Invalid death duration")
    require(math.isfinite(sequence.get_editor_property("rate_scale")) and sequence.get_editor_property("rate_scale") > 0.0, "Invalid death playback rate")
    require(sequence.get_editor_property("additive_anim_type") == unreal.AdditiveAnimationType.AAT_NONE, "Death animation must contain a full pose")


def configure():
    if not ASSETS.does_asset_exist(ENEMY_DEATH):
        # Retarget only the missing enemy sequence with the existing rigs; never copy meshes or source animations.
        # 기존 리그로 부족한 적 사망 시퀀스만 리타깃하며 메시나 원본 애니메이션을 복사하지 않습니다.
        source = load(MANNY_DEATH)
        require(unreal.WarriorAssetLibrary.retarget_animations([source], load(SWORD_SOURCE_MESH), load(ENEMY_SOURCE), load(ENEMY_RIGS + "/RTG_SwordEnemy"), ENEMY_DEATH.rsplit("/", 1)[0], "_SwordEnemy", False, False), "Enemy death retarget failed")
        require(ASSETS.save_loaded_asset(load(ENEMY_DEATH), only_if_is_dirty=False), "Could not save enemy death")
    for blueprint_path, sequence_path in assignments():
        blueprint = load(blueprint_path)
        defaults = unreal.get_default_object(blueprint.generated_class())
        sequence = load(sequence_path)
        validate_sequence(defaults.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset"), sequence)
        if defaults.get_editor_property("death_animation") == sequence:
            continue
        previous_skills = list(defaults.get_editor_property("equipped_skill_data_assets"))
        defaults.set_editor_property("death_animation", sequence)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        require(list(unreal.get_default_object(blueprint.generated_class()).get_editor_property("equipped_skill_data_assets")) == previous_skills, "Death setup changed equipped skills")
        require(ASSETS.save_loaded_asset(blueprint, only_if_is_dirty=False), "Could not save " + blueprint_path)


def verify():
    report = {"gameplay_test": "not run", "units": [], "poses": {}}
    for blueprint_path, sequence_path in assignments():
        defaults = unreal.get_default_object(load(blueprint_path).generated_class())
        mesh = defaults.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
        sequence = load(sequence_path)
        require(defaults.get_editor_property("death_animation") == sequence, "Death reference mismatch: " + blueprint_path)
        validate_sequence(mesh, sequence)
        if sequence_path not in report["poses"]:
            options = unreal.AnimPoseEvaluationOptions()
            options.set_editor_property("optional_skeletal_mesh", mesh)
            samples = []
            for position in [0.0, sequence.get_play_length() * 0.5, sequence.get_play_length()]:
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, position, options)
                require(unreal.AnimPoseExtensions.is_valid(pose), "Invalid death pose: " + sequence_path)
                pelvis = unreal.AnimPoseExtensions.get_bone_pose(pose, "pelvis", unreal.AnimPoseSpaces.WORLD).translation
                require(all(math.isfinite(number) for number in [pelvis.x, pelvis.y, pelvis.z]), "Non-finite death pose")
                samples.append({"time": position, "pelvis": [pelvis.x, pelvis.y, pelvis.z]})
            report["poses"][sequence_path] = {"samples": samples, "notifies": [str(name) for name in unreal.AnimationLibrary.get_animation_notify_event_names(sequence)]}
        report["units"].append({"blueprint": blueprint_path, "sequence": sequence_path, "seconds": sequence.get_play_length(), "skeleton": sequence.get_editor_property("skeleton").get_path_name()})
    return report


if __name__ == "__main__":
    verify_only = "-DeathAnimationsVerifyOnly" in unreal.SystemLibrary.get_command_line()
    try:
        if not verify_only:
            configure()
        report = verify()
        report_path = Path(unreal.Paths.project_saved_dir()).resolve() / "Automation" / ("DeathAnimationsReload.json" if verify_only else "DeathAnimationsConfigure.json")
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log("DEATH_ANIMATIONS_COMPLETE " + str(report_path))
    finally:
        if not verify_only:
            unreal.SystemLibrary.quit_editor()
