import hashlib
import json
from pathlib import Path

import unreal


# Duplicate only the basic attack's authored assets; do not change loadouts or existing content.
# 기본공격 제작 에셋만 복제하며 장착 목록과 기존 콘텐츠는 변경하지 않습니다.
root = "/Game/User_JeHoon/Blueprint"
source_skill_path = root + "/DataAsset/Skills/BPDA_DefaulatAttack"
source_ability_path = root + "/GAS/Ability/BPGA_DefaultAttack"
skill_path = root + "/DataAsset/Skills/BPDA_RangedAttack"
ability_path = root + "/GAS/Ability/BPGA_RangedAttack"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
verify_only = "-RangedAttackVerifyOnly" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def package_hash(package):
    filename = Path(unreal.Paths.project_content_dir(), package.removeprefix("/Game/") + ".uasset")
    return hashlib.sha256(filename.read_bytes()).hexdigest()


def setting_value(value):
    return value.export_text() if isinstance(value, unreal.StructBase) else value


source_skill = require(unreal.load_asset(source_skill_path), "Missing basic attack DA")
source_ability = require(unreal.load_asset(source_ability_path), "Missing basic attack ability Blueprint")
source_class = source_ability.generated_class()
require(source_skill.get_editor_property("ability_class") == source_class, "Basic attack ability reference changed; review before duplication")
require(not source_skill.get_editor_property("use_round_definition"), "Basic attack now uses an explicit round profile; review before duplication")
original_hashes = {path: package_hash(path) for path in [source_skill_path, source_ability_path]}

if verify_only:
    skill = require(unreal.load_asset(skill_path), "Missing ranged attack DA")
    ability = require(unreal.load_asset(ability_path), "Missing ranged attack ability Blueprint")
else:
    require(not assets.does_asset_exist(skill_path) and not assets.does_asset_exist(ability_path), "Ranged attack already exists; use -RangedAttackVerifyOnly to inspect without overwriting")
    ability = require(tools.duplicate_asset("BPGA_RangedAttack", root + "/GAS/Ability", source_ability), "Could not duplicate attack ability")
    unreal.BlueprintEditorLibrary.compile_blueprint(ability)
    skill = require(tools.duplicate_asset("BPDA_RangedAttack", root + "/DataAsset/Skills", source_skill), "Could not duplicate attack DA")
    skill.set_editor_property("skill_id", "RangedAttack")
    skill.set_editor_property("skill_name", "원거리 공격")
    skill.set_editor_property("skill_description", "대상 방향으로 투사체를 발사하여 처음 충돌한 적 한 명에게 피해를 줍니다.")
    skill.set_editor_property("ability_class", ability.generated_class())
    skill.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
    skill.set_editor_property("area_type", unreal.SkillAreaType.SINGLE)
    skill.set_editor_property("area_radius", 0)
    skill.set_editor_property("move_to_target", False)
    skill.set_editor_property("use_round_definition", False)
    for item in [ability, skill]:
        require(assets.save_loaded_asset(item), "Could not save " + item.get_path_name())

# The existing Single/EnemyUnit conversion launches one collision projectile without approaching.
# 기존 Single/EnemyUnit 변환이 접근 없이 단일 충돌 투사체를 발사합니다.
require(skill.get_editor_property("ability_class") == ability.generated_class(), "Ranged ability reference mismatch")
require(skill.get_editor_property("target_rule") == unreal.SkillTargetRule.ENEMY_UNIT, "Ranged target must be an enemy unit")
require(skill.get_editor_property("area_type") == unreal.SkillAreaType.SINGLE and skill.get_editor_property("area_radius") == 0, "Ranged attack must use the existing single-target area")
require(not skill.get_editor_property("move_to_target") and not skill.get_editor_property("use_round_definition"), "Ranged attack must use stationary legacy-data conversion")
require(skill.get_editor_property("action_point_cost") == source_skill.get_editor_property("action_point_cost"), "AP cost differs from the basic attack")
source_defaults = unreal.get_default_object(source_class)
defaults = unreal.get_default_object(ability.generated_class())
for prop in ["damage_amount", "attack_montage", "damage_effect_class", "attack_release_event_tag", "ability_tags", "activation_required_tags", "activation_blocked_tags", "source_required_tags", "source_blocked_tags", "target_required_tags", "target_blocked_tags"]:
    require(setting_value(defaults.get_editor_property(prop)) == setting_value(source_defaults.get_editor_property(prop)), "Original ability setting not preserved: " + prop)
require(original_hashes == {path: package_hash(path) for path in original_hashes}, "Original attack packages changed")
montage = defaults.get_editor_property("attack_montage")
result = {
    "mode": "reload" if verify_only else "create",
    "skill": skill.get_path_name(),
    "ability": ability.generated_class().get_path_name(),
    "damage": defaults.get_editor_property("damage_amount"),
    "ap": skill.get_editor_property("action_point_cost"),
    "montage": montage.get_path_name() if montage else None,
    "area": "Single",
    "approach": False,
    "original_packages_unchanged": True,
    "loadouts_changed": False,
    "gameplay_test": "not run",
}
output = Path(unreal.Paths.project_saved_dir(), "Automation", "RangedAttackReload.json" if verify_only else "RangedAttackCreation.json")
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("RANGED_ATTACK_AUTHORING_COMPLETE " + str(output))
