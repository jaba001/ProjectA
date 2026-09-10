import unreal

# Author the encounter-local training reward without replacing existing combat assets.
# 기존 전투 에셋을 교체하지 않고 전투 한정 훈련 보상을 작성합니다.
root = "/Game/User_JeHoon/Blueprint/DataAsset"
tools = unreal.AssetToolsHelpers.get_asset_tools()


def asset(name, cls):
    path = root + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, root, cls, factory)


skill = asset("DA_SweepingStrike", unreal.SkillDefinitionDataAsset)
skill.set_editor_property("skill_id", "SweepingStrike")
skill.set_editor_property("skill_name", "휩쓸기")
skill.set_editor_property("skill_description", "대상 주변 1칸의 적에게 피해 10. AP 1.")
skill.set_editor_property("ability_class", unreal.GA_AreaAttack)
skill.set_editor_property("action_point_cost", 1)
skill.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
skill.set_editor_property("area_type", unreal.SkillAreaType.AROUND_TARGET)
skill.set_editor_property("area_radius", 1)
skill.set_editor_property("move_to_target", False)
pool = asset("DA_EncounterSkillPool", unreal.SkillPoolDataAsset)
entry = unreal.SkillPoolEntry()
entry.set_editor_property("skill", skill)
entry.set_editor_property("weight", 1)
pool.set_editor_property("entries", [entry])
party = unreal.load_asset(root + "/DA_VerticalSliceParty")
party.set_editor_property("encounter_skill_pool", pool)
for item in [skill, pool, party]:
    if not unreal.EditorAssetLibrary.save_loaded_asset(item):
        raise RuntimeError("Could not save " + item.get_path_name())
