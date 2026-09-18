import json
from pathlib import Path

import unreal


def configure_sweeping_strike(skill):
    # Preserve authored power, cost and animation; reuse the basic montage only when none was authored.
    # 작성된 위력·비용·애니메이션을 보존하며 미지정 몽타주만 기본공격에서 재사용합니다.
    profile = skill.get_editor_property("round_definition") if skill.get_editor_property("use_round_definition") else unreal.CombatRoundSkill()
    ability = skill.get_editor_property("ability_class")
    defaults = unreal.get_default_object(ability) if ability else None
    if not skill.get_editor_property("use_round_definition"):
        if not defaults:
            raise RuntimeError("Sweeping Strike requires its authored damage source")
        profile.set_editor_property("power", defaults.get_editor_property("damage_amount"))
        profile.set_editor_property("action_point_cost", skill.get_editor_property("action_point_cost"))
    if not profile.get_editor_property("cast_montage"):
        montage = defaults.get_editor_property("attack_montage") if defaults else None
        if not montage:
            basic = unreal.load_asset("/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack")
            if not basic:
                raise RuntimeError("Missing basic attack for melee presentation")
            montage = basic.get_editor_property("round_definition").get_editor_property("cast_montage") if basic.get_editor_property("use_round_definition") else None
            if not montage:
                montage = unreal.get_default_object(basic.get_editor_property("ability_class")).get_editor_property("attack_montage")
        if not montage:
            raise RuntimeError("Missing melee attack montage")
        profile.set_editor_property("cast_montage", montage)
    profile.set_editor_property("kind", unreal.CombatRoundSkillKind.MELEE)
    profile.set_editor_property("melee_area", unreal.SkillAreaType.TARGET_AND_SIDES)
    profile.set_editor_property("approach", unreal.CombatRoundApproach.UNIT)
    profile.set_editor_property("target_loss", unreal.CombatRoundTargetLoss.CANCEL)
    profile.set_editor_property("remain_at_destination", False)
    skill.set_editor_property("round_definition", profile)
    skill.set_editor_property("use_round_definition", True)
    skill.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
    skill.set_editor_property("area_type", unreal.SkillAreaType.TARGET_AND_SIDES)
    skill.set_editor_property("area_radius", 1)
    skill.set_editor_property("move_to_target", True)
    power = profile.get_editor_property("power")
    cost = profile.get_editor_property("action_point_cost")
    skill.set_editor_property("skill_description", f"대상에게 접근해 대상과 양옆 한 칸의 적을 공격한 뒤 복귀합니다. 피해 {power:g}, AP {cost}.")


if __name__ == "__main__":
    skill = unreal.load_asset("/Game/User_JeHoon/Blueprint/DataAsset/Skills/DA_SweepingStrike")
    if not skill:
        raise RuntimeError("Missing Sweeping Strike")
    verify_only = "-SweepingStrikeVerifyOnly" in unreal.SystemLibrary.get_command_line()
    if not verify_only:
        configure_sweeping_strike(skill)
        if not unreal.EditorAssetLibrary.save_loaded_asset(skill):
            raise RuntimeError("Could not save Sweeping Strike")
    profile = skill.get_editor_property("round_definition")
    if not skill.get_editor_property("use_round_definition") or not skill.get_editor_property("move_to_target") or skill.get_editor_property("area_type") != unreal.SkillAreaType.TARGET_AND_SIDES:
        raise RuntimeError("Sweeping Strike does not use the adjacent melee pattern")
    if profile.get_editor_property("kind") != unreal.CombatRoundSkillKind.MELEE or profile.get_editor_property("melee_area") != unreal.SkillAreaType.TARGET_AND_SIDES or profile.get_editor_property("approach") != unreal.CombatRoundApproach.UNIT or profile.get_editor_property("remain_at_destination"):
        raise RuntimeError("Sweeping Strike must approach, strike adjacent sides and return")
    if not profile.get_editor_property("cast_montage"):
        raise RuntimeError("Sweeping Strike montage is missing")
    report = {"asset": skill.get_path_name(), "mode": "reload" if verify_only else "configure", "profile": profile.export_text(), "gameplay_test": "not run"}
    output = Path(unreal.Paths.project_saved_dir(), "Automation", "SweepingStrikeReload.json" if verify_only else "SweepingStrikeConfigure.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SWEEPING_STRIKE_CONFIGURED " + str(output))
