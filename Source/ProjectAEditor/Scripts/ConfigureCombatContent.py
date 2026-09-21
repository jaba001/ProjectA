import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureSweepingStrike import configure_sweeping_strike

# Author the encounter-local training reward without replacing existing combat assets.
# 기존 전투 에셋을 교체하지 않고 전투 한정 훈련 보상을 작성합니다.
root = "/Game/User_JeHoon/Blueprint/DataAsset"
tools = unreal.AssetToolsHelpers.get_asset_tools()


def asset(name, cls, folder):
    directory = root + "/" + folder
    path = directory + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, directory, cls, factory)


skill = asset("BPDA_SweepingStrike", unreal.SkillDefinitionDataAsset, "Skills")
skill.set_editor_property("skill_id", "SweepingStrike")
skill.set_editor_property("skill_name", "휩쓸기")
skill.set_editor_property("ability_class", unreal.GA_AreaAttack)
skill.set_editor_property("action_point_cost", 1)
skill.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
configure_sweeping_strike(skill)
pool = asset("DA_EncounterSkillPool", unreal.SkillPoolDataAsset, "SkillPools")
entry = unreal.SkillPoolEntry()
entry.set_editor_property("skill", skill)
entry.set_editor_property("weight", 1)
pool.set_editor_property("entries", [entry])
party = unreal.load_asset(root + "/Parties/DA_VerticalSliceParty")
party.set_editor_property("encounter_skill_pool", pool)
for item in [skill, pool, party]:
    if not unreal.EditorAssetLibrary.save_loaded_asset(item):
        raise RuntimeError("Could not save " + item.get_path_name())
