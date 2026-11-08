import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSETS = unreal.EditorAssetLibrary
STEEL = "/Game/User_JeHoon/Assets/ParagonAnimationsRetargetedToManny/steelmanny/Ability"
CHARGE_SOURCE = STEEL + "/Steel_Ability_CowCatcher_Charge"
IMPACT_SOURCE = STEEL + "/Steel_Ability_CowCatcher_End"
LOOP = STEEL + "/AS_ShieldCharge_Loop"
MONTAGE = STEEL + "/AM_ShieldCharge"
BASE_SKILL = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_MeleeAttack"
SKILL = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_ShieldCharge"
OWNER_KEY = "ProjectA.ShieldCharge"
OWNER = "ShieldCharge.v1"
# The impact raises the shield until 0.25 s; the second half recovers into the shared idle pose.
# 충돌 동작은 0.25초까지 방패를 들어 올리고 뒤 절반에서 공통 대기 자세로 돌아옵니다.
IMPACT_SPLIT = 0.25
IMPACT_END = 0.5
WINDUP = 0.17
# Steel's charge cycle covers about 1436 cm per second; the faster approach keeps strides close to the ground speed.
# Steel 돌진 순환은 초당 약 1436cm를 이동하므로 빠른 접근 속도로 보폭과 지면 속도를 맞춥니다.
MOVE_SPEED = 2000.0
APPROACH_PLAY_RATE = 0.6
# Placeholder until the physical damage reduction system exists: 50 equals the rare shield's own 50% reduction.
# 물리 피해 감소 시스템 구현 전 임시값이며 50은 희귀 방패 자체 감소 50%에 해당합니다.
PLACEHOLDER_POWER = 50.0
VERIFY = "-ShieldChargeVerifyOnly" in unreal.SystemLibrary.get_command_line()
OVERWRITE = "-ShieldChargeOverwrite" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def existing(path, asset_type):
    asset = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if asset:
        require(isinstance(asset, asset_type) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Preserve independently authored destination: " + path)
    else:
        require(not VERIFY, "Missing baked asset: " + path)
    return asset


def save(asset):
    require(not VERIFY, "Read-only mode cannot save")
    ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + asset.get_path_name())


def root_keys(sequence):
    keys = unreal.WarriorAssetLibrary.get_animation_bone_track_transforms(sequence, "root")
    require(keys, "Missing root track: " + sequence.get_path_name())
    return keys


def bake_loop(source):
    loop = existing(LOOP, unreal.AnimSequence)
    if loop is None or OVERWRITE:
        if loop is None:
            loop = require(ASSETS.duplicate_asset(source.get_path_name().split(".")[0], LOOP), "Could not duplicate the charge cycle")
        keys = root_keys(source)
        # Lock the root to its first key so the cycle stays in place; the round simulation moves the capsule.
        # 루트를 첫 키에 고정해 순환을 제자리 동작으로 만들며 캡슐 이동은 라운드 시뮬레이션이 맡습니다.
        first = keys[0].translation
        controller = require(loop.get_editor_property("controller"), "Animation has no controller")
        loop.modify()
        controller.open_bracket("Lock shield charge root / 방패 돌진 루트 고정", False)
        try:
            require(controller.set_bone_track_keys(unreal.Name("root"), [unreal.Vector(first.x, first.y, first.z) for _ in keys], [key.rotation for key in keys], [key.scale3d for key in keys], False), "Could not write the root track")
        finally:
            controller.close_bracket(False)
        loop.set_editor_property("enable_root_motion", False)
        save(loop)
    travel = max((key.translation - root_keys(loop)[0].translation).length() for key in root_keys(loop))
    require(travel < 0.01, "The approach cycle still moves its root: " + str(travel))
    require(abs(loop.get_play_length() - source.get_play_length()) < 0.0001, "The approach cycle length differs from its source")
    return loop, travel


def author_montage(impact):
    montage = existing(MONTAGE, unreal.AnimMontage)
    create = montage is None
    if create:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", impact.get_editor_property("skeleton"))
        factory.set_editor_property("source_animation", impact)
        folder, name = MONTAGE.rsplit("/", 1)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory), "Montage factory failed")
    arguments = [impact, impact, 0.0, IMPACT_SPLIT, IMPACT_SPLIT, IMPACT_END, IMPACT_SPLIT, 1.0]
    if not VERIFY and (create or OVERWRITE):
        require(unreal.WarriorAssetLibrary.configure_timed_attack_montage(montage, *arguments), "Shield impact montage authoring failed")
        save(montage)
    require(unreal.WarriorAssetLibrary.validate_timed_attack_montage(montage, *arguments), "Saved montage does not match the shield impact plan")
    return montage


def resolved(asset):
    result = asset.resolve_round_skill()
    return result if isinstance(result, unreal.CombatRoundSkill) else next(value for value in result if isinstance(value, unreal.CombatRoundSkill))


def author_skill(loop, montage):
    skill = existing(SKILL, unreal.SkillDefinitionDataAsset)
    if not VERIFY and (skill is None or OVERWRITE):
        if skill is None:
            skill = require(ASSETS.duplicate_asset(BASE_SKILL, SKILL), "Could not duplicate the basic melee skill")
        profile = skill.get_editor_property("round_definition")
        profile.set_editor_property("cast_montage", montage)
        profile.set_editor_property("approach_animation", loop)
        profile.set_editor_property("approach_animation_play_rate", APPROACH_PLAY_RATE)
        profile.set_editor_property("move_speed", MOVE_SPEED)
        profile.set_editor_property("windup_seconds", WINDUP)
        profile.set_editor_property("power", PLACEHOLDER_POWER)
        skill.set_editor_property("round_definition", profile)
        skill.set_editor_property("use_round_definition", True)
        skill.set_editor_property("skill_id", "ShieldCharge")
        skill.set_editor_property("skill_name", unreal.Text("방패 돌진"))
        skill.set_editor_property("skill_description", unreal.Text("방패를 앞세워 돌진해 대상 하나에게 물리 피해를 줍니다. 피해는 총 물리 피해 감소율 수치와 힘의 합이며, 감소 시스템 구현 전까지 임시 위력 50을 씁니다."))
        save(skill)
    skill = require(skill, "Missing shield charge skill")
    profile = resolved(skill)
    require(profile.get_editor_property("cast_montage") == montage and profile.get_editor_property("approach_animation") == loop, "Shield charge presentation is not connected")
    require(profile.get_editor_property("approach") == unreal.CombatRoundApproach.UNIT and profile.get_editor_property("kind") == unreal.CombatRoundSkillKind.MELEE, "Shield charge must approach a unit as a melee attack")
    require(abs(profile.get_editor_property("move_speed") - MOVE_SPEED) < 0.001 and abs(profile.get_editor_property("windup_seconds") - WINDUP) < 0.0001, "Shield charge timing differs")
    require(not profile.get_editor_property("use_equipped_weapon_damage"), "Shield charge must not resolve equipped weapon damage")
    return skill, profile


def main():
    charge = require(unreal.load_asset(CHARGE_SOURCE), "Import the Steel charge cycle first")
    impact = require(unreal.load_asset(IMPACT_SOURCE), "Import the Steel charge impact first")
    loop, travel = bake_loop(charge)
    montage = author_montage(impact)
    skill, profile = author_skill(loop, montage)
    report = {"mode": "verify" if VERIFY else "author", "skill": skill.get_path_name(), "loop": loop.get_path_name(), "montage": montage.get_path_name(),
              "loop_root_travel_cm": travel, "loop_seconds": loop.get_play_length(), "montage_seconds": montage.get_play_length(), "windup_seconds": WINDUP,
              "move_speed": MOVE_SPEED, "approach_play_rate": APPROACH_PLAY_RATE, "placeholder_power": profile.get_editor_property("power"),
              "runtime_review": "User review pending; no gameplay or automation tests executed."}
    output = ROOT / "Saved/ShieldCharge" / ("Verify.json" if VERIFY else "Author.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[ShieldCharge] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
