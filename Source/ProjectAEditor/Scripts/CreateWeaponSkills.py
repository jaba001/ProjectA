import hashlib
import json
import re
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSETS = unreal.EditorAssetLibrary
OWNER_KEY = "ProjectA.WeaponSkills"
OWNER = "WeaponSkills.v1"
SPECS = [
    {"name": "DA_MeleeAttack", "id": "WeaponMeleeAttack", "label": "근접 공격", "source": "/Game/User_JeHoon/Blueprint/DataAsset/Skills/BPDA_swoard_attack", "tags": ["Attack.Close", "Skill.Element.Physical", "Skill.Effect.Damage"]},
    {"name": "DA_CrossbowAttack", "id": "WeaponCrossbowAttack", "label": "석궁 공격", "source": "/Game/User_JeHoon/Blueprint/DataAsset/Skills/DrGame/ProjectileHitVFX/DA_DrGame_ProjectileHitVFX_Arrow", "tags": ["Attack.Ranged", "Skill.Element.Physical", "Skill.Effect.Damage", "Skill.Shape.Projectile"]},
]


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def source_hash(package):
    return hashlib.sha256((ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")).read_bytes()).hexdigest()


def resolve_profile(asset):
    result = asset.resolve_round_skill()
    values = result if isinstance(result, tuple) else [result]
    return require(next((value for value in values if isinstance(value, unreal.CombatRoundSkill)), None), "Skill profile failed to resolve: " + asset.get_path_name())


def main():
    before = {spec["source"]: source_hash(spec["source"]) for spec in SPECS}
    records = []
    for spec in SPECS:
        source = require(unreal.load_asset(spec["source"]), "Missing source skill")
        profile = resolve_profile(source)
        names = set(re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', profile.get_editor_property("effect_tags").export_text())) | set(spec["tags"])
        tags = unreal.GameplayTagContainer()
        require(tags.import_text("(GameplayTags=(" + ",".join('(TagName="' + name + '")' for name in sorted(names)) + "))"), "Tag import failed")
        require(set(re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', tags.export_text())) == names, "Unregistered native tags")
        profile.set_editor_property("effect_tags", tags)
        if spec["name"] == "DA_CrossbowAttack":
            # The projectile profile is shared, but the bow draw animation does not belong to a crossbow.
            # 투사체 프로필을 공유하더라도 활 당기기 애니메이션은 석궁에 적용하지 않습니다.
            profile.set_editor_property("cast_montage", None)
        if spec["name"] == "DA_MeleeAttack":
            # Use the shared melee collision path for all close weapons rather than the fixed sword trace proxy.
            # 모든 근접 무기의 공통 근접 충돌 경로를 사용하며 고정된 검 판정 프록시는 사용하지 않습니다.
            profile.set_editor_property("use_weapon_trace", False)
            profile.set_editor_property("use_melee_area_collision", False)
            for field in ["weapon_component_name", "weapon_base_socket", "weapon_tip_socket", "weapon_montage_slot"]:
                profile.set_editor_property(field, unreal.Name("None"))
        folder = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons"
        path = folder + "/" + spec["name"]
        asset = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
        if asset:
            require(isinstance(asset, unreal.SkillDefinitionDataAsset) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Destination belongs to another author")
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.SkillDefinitionDataAsset)
            asset = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(spec["name"], folder, unreal.SkillDefinitionDataAsset, factory), "Asset creation failed")
        asset.set_editor_property("use_round_definition", True)
        asset.set_editor_property("round_definition", profile)
        asset.set_editor_property("ability_class", source.get_editor_property("ability_class"))
        asset.set_editor_property("skill_id", unreal.Name(spec["id"]))
        asset.set_editor_property("skill_name", unreal.Text(spec["label"]))
        asset.set_editor_property("skill_description", unreal.Text("무기에 부여된 기본 공격입니다."))
        asset.set_editor_property("action_point_cost", profile.get_editor_property("action_point_cost"))
        ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
        require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Asset save failed")
        authored = resolve_profile(asset)
        records.append({"path": asset.get_path_name(), "source": spec["source"], "profile": authored.export_text()})
    require(before == {package: source_hash(package) for package in before}, "Source assets changed")
    rules_folder = "/Game/User_JeHoon/Blueprint/DataAsset/Weapons"
    rules_path = rules_folder + "/DA_WeaponSkillRules"
    rules_asset = unreal.load_asset(rules_path) if ASSETS.does_asset_exist(rules_path) else None
    if rules_asset:
        require(isinstance(rules_asset, unreal.RunWeaponSkillRulesDataAsset) and ASSETS.get_metadata_tag(rules_asset, OWNER_KEY) == OWNER, "Rules destination belongs to another author")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.RunWeaponSkillRulesDataAsset)
        rules_asset = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_WeaponSkillRules", rules_folder, unreal.RunWeaponSkillRulesDataAsset, factory), "Rules creation failed")
    resolved = rules_asset.build_state()
    values = resolved if isinstance(resolved, tuple) else [resolved]
    rules = require(next((value for value in values if isinstance(value, unreal.RunWeaponSkillRulesState)), None), "Rules failed to resolve")
    rules_asset.set_editor_property("rules", rules)
    ASSETS.set_metadata_tag(rules_asset, OWNER_KEY, OWNER)
    require(ASSETS.save_loaded_asset(rules_asset, only_if_is_dirty=False), "Rules save failed")
    party = require(unreal.load_asset("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty"), "Project party missing")
    previous_rules = party.get_editor_property("weapon_skill_rules")
    require(previous_rules is None or previous_rules == rules_asset, "Party has another custom rule asset")
    party.set_editor_property("weapon_skill_rules", rules_asset)
    require(ASSETS.save_loaded_asset(party, only_if_is_dirty=False), "Party reference save failed")
    output = ROOT / "Saved/Automation/WeaponSkills"
    output.mkdir(parents=True, exist_ok=True)
    (output / "Author.json").write_text(json.dumps({"assets": records, "rules": rules_asset.get_path_name(), "candidate_count": len(rules.get_editor_property("candidates")), "rarity_count": len(rules.get_editor_property("rarities")), "source_hashes": before}, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[WeaponSkills] Authored two weapon definitions and a rule asset; source skills unchanged")


main()
