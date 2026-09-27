import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SKILLS_ROOT = "/Game/User_JeHoon/Blueprint/DataAsset/Skills"
PARTY_PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty"
POOL_PATH = "/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_RunEncounterPool_TestSkills"
VERIFY_ONLY = "-TestSkillShopVerifyOnly" in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
PARTY_PROPERTIES = ["unarmed_starting_skill", "encounter_skill_pool", "professions", "player_unit_classes", "fallback_player_unit_class"]
POOL_PRESERVED_PROPERTIES = ["starting_gold", "fixed_skill_offers", "recovery", "gold_reward_min", "gold_reward_max"]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def file_hash(filename):
    result = hashlib.sha256()
    with filename.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def package_file(package):
    require(package.startswith("/Game/"), "Unexpected package root: " + package)
    return ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")


def normalized(value):
    # Compare product text by content because saving assigns package-specific localization identities.
    # 저장 시 패키지별 현지화 식별자가 부여되므로 상품 문구는 실제 내용으로 비교합니다.
    if isinstance(value, unreal.RunSkillShopOffer):
        return {name: normalized(value.get_editor_property(name)) for name in ["offer_id", "skill", "display_name", "description", "price"]}
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if hasattr(value, "items"):
        return {str(key): normalized(item) for key, item in sorted(value.items(), key=lambda pair: str(pair[0]))}
    if isinstance(value, (list, tuple, unreal.Array)):
        return [normalized(item) for item in value]
    return value


def properties(asset, names):
    return {name: normalized(asset.get_editor_property(name)) for name in names}


def validate(asset, validator, warnings):
    result, errors, messages = validator.is_object_valid(asset, unreal.DataValidationUsecase.SCRIPT)
    require(result == unreal.DataValidationResult.VALID and not errors, "Data validation failed: " + asset.get_path_name() + " / " + "; ".join(str(error) for error in errors))
    warnings.extend({"asset": asset.get_path_name(), "message": str(message)} for message in messages)


def test_tag():
    tag = unreal.GameplayTag()
    require(tag.import_text('(TagName="Encounter.Shop.Skill.Test")'), "Could not import the test shop GameplayTag")
    require("Encounter.Shop.Skill.Test" in tag.export_text(), "Test shop GameplayTag was not preserved")
    return tag


def main():
    party = require(unreal.load_asset(PARTY_PATH), "Missing active party definition")
    require(isinstance(party, unreal.PartyDefinitionDataAsset), "Unexpected party asset class")
    current_pool = party.get_editor_property("run_encounter_pool")
    require(not current_pool or current_pool.get_path_name().split(".", 1)[0] == POOL_PATH, "Party references another authored RunEncounterPool; preserve it and review before continuing")
    party_before = properties(party, PARTY_PROPERTIES)
    party_hash_before = file_hash(package_file(PARTY_PATH))
    require("BPDA_DefaulatAttack" in str(party_before["unarmed_starting_skill"]), "The expected unarmed starting skill changed; review its existing ownership")
    defaults = unreal.get_default_object(unreal.RunEncounterPoolDataAsset)
    baseline = properties(defaults, POOL_PRESERVED_PROPERTIES)
    require(baseline["starting_gold"] == 10 and baseline["gold_reward_min"] == 5 and baseline["gold_reward_max"] == 15, "Native gold defaults changed; review before creating the test pool")
    native_products = defaults.get_editor_property("fixed_skill_offers")
    require(len(native_products) == 4 and all(offer.get_editor_property("price") == 1 for offer in native_products), "Native normal skill shop products changed")
    require(defaults.get_editor_property("recovery").get_editor_property("price") == 1, "Native recovery price changed")
    fixed_offers = []
    changed_slots = 0
    for original in defaults.get_editor_property("fixed_offers"):
        offer = unreal.RunEncounterOffer()
        require(offer.import_text(original.export_text()), "Could not copy an existing encounter offer")
        if str(offer.get_editor_property("encounter_id")) == "Shop_03":
            offer.set_editor_property("encounter_tag", test_tag())
            offer.set_editor_property("display_name", "테스트 스킬상점")
            changed_slots += 1
        fixed_offers.append(offer)
    require(len(fixed_offers) == 3 and changed_slots == 1, "Expected exactly one Shop_03 among three existing encounters")
    skill_paths = sorted({path.split(".", 1)[0] for path in ASSETS.list_assets(SKILLS_ROOT, False, False)}, key=str.casefold)
    require(len(skill_paths) == 181, "Expected all 181 authored skills directly under Skills")
    require(all(path.rsplit("/", 1)[0] == SKILLS_ROOT for path in skill_paths), "Unexpected nested skill path")
    require(len({path.rsplit("/", 1)[-1].casefold() for path in skill_paths}) == 181, "Duplicate test offer or primary asset names")
    skill_hashes = {path: file_hash(package_file(path)) for path in skill_paths}
    validator = require(unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem), "Missing editor validation subsystem")
    warnings = []
    test_offers = []
    skill_records = []
    primary_ids = set()
    for package in skill_paths:
        skill = require(unreal.load_asset(package), "Missing skill: " + package)
        require(isinstance(skill, unreal.SkillDefinitionDataAsset), "Unexpected skill asset class: " + package)
        validate(skill, validator, warnings)
        # Native data assets use class and object names; pool validation also checks the resolved runtime IDs.
        # 네이티브 DataAsset은 클래스·오브젝트 이름을 사용하며 풀 검증도 실제 실행 식별자를 검사합니다.
        require(skill.get_class().get_path_name() == "/Script/ProjectA.SkillDefinitionDataAsset", "Review the identity rule for a derived skill class: " + package)
        primary_id = skill.get_class().get_name() + ":" + skill.get_name()
        require(primary_id.casefold() not in primary_ids, "Duplicate primary skill identity: " + package)
        primary_ids.add(primary_id.casefold())
        offer = unreal.RunSkillShopOffer()
        offer.set_editor_property("offer_id", "Test_" + skill.get_name())
        offer.set_editor_property("skill", unreal.SoftObjectPath(skill.get_path_name()))
        offer.set_editor_property("display_name", skill.get_editor_property("skill_name"))
        offer.set_editor_property("description", skill.get_editor_property("skill_description"))
        offer.set_editor_property("price", 0)
        test_offers.append(offer)
        skill_records.append({"asset": skill.get_path_name(), "class": skill.get_class().get_path_name(), "primary_asset_id": primary_id, "skill_id": str(skill.get_editor_property("skill_id")), "name": str(skill.get_editor_property("skill_name")), "use_round_definition": bool(skill.get_editor_property("use_round_definition")), "catalog_source": str(ASSETS.get_metadata_tag(skill, "CatalogSource")), "offer_id": str(offer.get_editor_property("offer_id")), "price": 0})

    # Reuse only a matching authored pool; never overwrite user changes during repeated runs.
    # 재실행 시 일치하는 제작 풀만 재사용하며 사용자 변경을 덮어쓰지 않습니다.
    expected = {name: defaults.get_editor_property(name) for name in POOL_PRESERVED_PROPERTIES}
    expected["fixed_offers"] = fixed_offers
    expected["fixed_test_skill_offers"] = test_offers
    exists = ASSETS.does_asset_exist(POOL_PATH)
    require(exists or not VERIFY_ONLY, "Test skill shop pool is missing in verify-only mode")
    pool = unreal.load_asset(POOL_PATH) if exists else None
    if exists:
        require(isinstance(pool, unreal.RunEncounterPoolDataAsset), "Unexpected test pool asset class")
        for name, value in expected.items():
            require(normalized(pool.get_editor_property(name)) == normalized(value), "Existing test shop settings differ; review without overwriting: " + name)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.RunEncounterPoolDataAsset)
        directory, name = POOL_PATH.rsplit("/", 1)
        pool = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, unreal.RunEncounterPoolDataAsset, factory), "Could not create the test skill shop pool")
        for name, value in expected.items():
            pool.set_editor_property(name, value)
        ASSETS.set_metadata_tag(pool, "AuthoringScript", "ConfigureTestSkillShop.py")
        ASSETS.set_metadata_tag(pool, "TestSkillCount", "181")
    require(properties(pool, POOL_PRESERVED_PROPERTIES) == baseline, "Normal shop, recovery or gold defaults changed")
    validate(pool, validator, warnings)
    if VERIFY_ONLY:
        require(current_pool == pool, "Party is not linked to the saved test skill shop pool")
    elif not current_pool:
        party.set_editor_property("run_encounter_pool", pool)
    validate(party, validator, warnings)
    require(properties(party, PARTY_PROPERTIES) == party_before, "Unrelated party definitions changed")
    saved_packages = []
    if not VERIFY_ONLY:
        if not exists:
            require(ASSETS.save_loaded_asset(pool), "Could not save the test skill shop pool")
            saved_packages.append(POOL_PATH)
        if not current_pool:
            require(ASSETS.save_loaded_asset(party), "Could not save the party pool reference")
            saved_packages.append(PARTY_PATH)
    require(skill_hashes == {path: file_hash(package_file(path)) for path in skill_paths}, "A source skill package changed")
    require(properties(party, PARTY_PROPERTIES) == party_before, "Unrelated party properties changed while saving")
    if VERIFY_ONLY:
        require(file_hash(package_file(PARTY_PATH)) == party_hash_before, "Party package changed in verify-only mode")
    report = {"mode": "reload" if VERIFY_ONLY else "configure", "pool": pool.get_path_name(), "party": party.get_path_name(), "previous_pool": current_pool.get_path_name() if current_pool else None, "skills": len(skill_records), "data_validation": "passed", "test_price": 0, "test_encounter_id": "Shop_03", "test_encounter_tag": "Encounter.Shop.Skill.Test", "normal_settings_preserved": baseline, "party_properties_preserved": party_before, "source_skill_hashes": skill_hashes, "source_skills_unchanged": True, "unarmed_starting_skill": party_before["unarmed_starting_skill"], "unarmed_initial_ownership": "Existing CreateInitialSaveData/ResolveStartingSkills adds the starting skill; gameplay was not executed", "saved_packages": saved_packages, "warnings": warnings, "assets": skill_records, "gameplay_test": "not run"}
    output = ROOT / "Saved/Automation/TestSkillShop" / ("Reload.json" if VERIFY_ONLY else "Configuration.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("TEST_SKILL_SHOP_AUTHORING_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
