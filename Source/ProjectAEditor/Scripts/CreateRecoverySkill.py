import json
import re
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
VERIFY = "-RecoverySkillVerifyOnly" in unreal.SystemLibrary.get_command_line()
PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Consumables/DA_HealthPotion"
OWNER_KEY = "ProjectA.RecoverySkill"
OWNER = "RecoverySkill.v1"
ASSETS = unreal.EditorAssetLibrary


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def normalized(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.StructBase):
        return re.sub(r"(?<=[=(,])-0(\.0+)?(?=[,)])", r"0\1", value.export_text())
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    return value


def main():
    tags = unreal.GameplayTagContainer()
    names = ["Item.Consumable.Healing", "Skill.Effect.Heal"]
    require(tags.import_text("(GameplayTags=(" + ",".join('(TagName="' + name + '")' for name in names) + "))"), "Tag import failed")
    require(sorted(re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', tags.export_text())) == sorted(names), "Native tags are not registered")
    profile = unreal.CombatRoundSkill()
    profile.set_editor_property("target_rule", unreal.SkillTargetRule.ALLY_UNIT)
    profile.set_editor_property("approach", unreal.CombatRoundApproach.NONE)
    profile.set_editor_property("target_loss", unreal.CombatRoundTargetLoss.CANCEL)
    profile.set_editor_property("effect_tags", tags)
    profile.set_editor_property("effect_class", require(unreal.load_class(None, "/Script/ProjectA.GE_Heal"), "Native Instant heal effect missing"))
    profile.set_editor_property("power", 25.0)
    profile.set_editor_property("action_point_cost", 1)
    properties = {
        "use_round_definition": True,
        "round_definition": profile,
        "skill_id": unreal.Name("HealthPotion"),
        "skill_name": unreal.Text("회복 소모품"),
        "skill_description": unreal.Text("본인 생존 캐릭터의 HP를 25 회복합니다. AP 1을 사용하며 실제 회복 발동 시 소모품 1개가 차감됩니다. 최대 HP이거나 수량이 없으면 사용할 수 없습니다."),
        "action_point_cost": 1,
        "target_rule": unreal.SkillTargetRule.ALLY_UNIT,
    }
    created = False
    asset = unreal.load_asset(PATH) if ASSETS.does_asset_exist(PATH) else None
    if asset:
        require(isinstance(asset, unreal.SkillDefinitionDataAsset) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Destination belongs to another author")
        for name, value in properties.items():
            require(normalized(asset.get_editor_property(name)) == normalized(value), "Existing authored value differs: " + name)
    else:
        require(not VERIFY, "Authored healing skill is missing")
        folder, name = PATH.rsplit("/", 1)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.SkillDefinitionDataAsset)
        asset = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.SkillDefinitionDataAsset, factory), "Create asset failed")
        for name, value in properties.items():
            asset.set_editor_property(name, value)
        ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
        require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed")
        created = True
    for name, value in properties.items():
        require(normalized(asset.get_editor_property(name)) == normalized(value), "Saved property mismatch: " + name)
    report = {"mode": "verify" if VERIFY else "author", "created": created, "path": asset.get_path_name(), "power": 25, "ap_cost": 1, "tags": names, "effect": "/Script/ProjectA.GE_Heal", "round_definition": normalized(asset.get_editor_property("round_definition"))}
    output = ROOT / "Saved/Automation/RecoverySkill"
    output.mkdir(parents=True, exist_ok=True)
    (output / ("Verify.json" if VERIFY else "Author.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[RecoverySkill] " + json.dumps(report, ensure_ascii=False))


main()
