import unreal

# Create trusted content for the opt-in local Snapshot exercise.
# 선택적으로 실행하는 로컬 Snapshot 검증용 신뢰 콘텐츠를 생성합니다.
root = "/Game/User_JeHoon/Blueprint"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
enemy_path = root + "/Unit/BP_SnapshotOpponent"
enemy = unreal.load_asset(enemy_path) if assets.does_asset_exist(enemy_path) else None
if not enemy:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.EnemyUnit)
    enemy = tools.create_asset("BP_SnapshotOpponent", root + "/Unit", unreal.Blueprint, factory)
    player_class = assets.load_blueprint_class(root + "/Unit/BP_PlayerUnit")
    player_mesh = unreal.get_default_object(player_class).get_editor_property("mesh")
    enemy_mesh = unreal.get_default_object(enemy.generated_class()).get_editor_property("mesh")
    for prop in ["skeletal_mesh_asset", "anim_class", "relative_location", "relative_rotation", "relative_scale3d"]:
        enemy_mesh.set_editor_property(prop, player_mesh.get_editor_property(prop))
    unreal.BlueprintEditorLibrary.compile_blueprint(enemy)
    if not assets.save_loaded_asset(enemy):
        raise RuntimeError("Could not save Snapshot opponent Blueprint")

catalog_path = root + "/DataAsset/DA_OpponentSnapshotCatalog"
catalog = unreal.load_asset(catalog_path) if assets.does_asset_exist(catalog_path) else None
if not catalog:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.OpponentSnapshotCatalogDataAsset)
    catalog = tools.create_asset("DA_OpponentSnapshotCatalog", root + "/DataAsset", unreal.OpponentSnapshotCatalogDataAsset, factory)
    catalog.set_editor_property("enemy_classes", {unreal.Name(name): enemy.generated_class() for name in ["StableHand", "Scholar", "Herbalist", "Hunter"]})
    catalog.set_editor_property("skills", {
        unreal.Name("DefaultAttack"): unreal.load_asset(root + "/DataAsset/BPDA_DefaulatAttack"),
        unreal.Name("SweepingStrike"): unreal.load_asset(root + "/DataAsset/DA_SweepingStrike"),
    })
    if not assets.save_loaded_asset(catalog):
        raise RuntimeError("Could not save Snapshot catalog")

mode = unreal.load_asset(root + "/Game/BP_GameplayGameMode")
defaults = unreal.get_default_object(mode.generated_class())
if not defaults.get_editor_property("local_opponent_catalog"):
    defaults.set_editor_property("local_opponent_catalog", catalog)
    unreal.BlueprintEditorLibrary.compile_blueprint(mode)
    if not assets.save_loaded_asset(mode):
        raise RuntimeError("Could not save Snapshot catalog binding")

# Preserve an existing user-edited sample slot; corrupt data remains visible to validation.
# 사용자가 편집한 기존 샘플 슬롯을 보존하며 손상 데이터는 검증 오류로 드러나게 둡니다.
slot_id = "SampleOpponent"
slot_name = unreal.PartySnapshotLibrary.get_save_slot_name(slot_id)
if not unreal.GameplayStatics.does_save_game_exist(slot_name, 0):
    stats = unreal.PartySnapshotStats(max_hp=140.0, current_hp=120.0, max_action_points=2, max_sub_action_points=1, move_range=1)
    member = unreal.PartySnapshotMember(member_id="SampleHunter", class_id="Hunter", character_name="Snapshot Hunter", stats=stats, skill_ids=["DefaultAttack", "SweepingStrike"], formation_slot=0)
    snapshot = unreal.PartySnapshot(schema_version=1, content_version=1, snapshot_id="SampleOpponentV1", members=[member])
    result = unreal.PartySnapshotLibrary.save_snapshot(slot_id, snapshot)
    unreal.log("Snapshot sample save: " + str(result))
    if not unreal.GameplayStatics.does_save_game_exist(slot_name, 0):
        raise RuntimeError("Could not save sample opponent")
unreal.log("Snapshot assets ready; launch with -ProjectAOpponentSnapshot=SampleOpponent; the default Run checkpoint is ProjectA_SnapshotRun_SampleOpponent")
