import unreal

# Preserve Designer widgets; configure only the missing menu stage and a visual-only preview.
# Designer 위젯은 보존하고 누락된 메뉴 스테이지와 표시 전용 프리뷰만 설정합니다.
root = "/Game/User_JeHoon"
world = unreal.EditorLoadingAndSavingUtils.load_map(root + "/LEVEL/MainMenu")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
stages = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage)]
if len(stages) > 1:
    raise RuntimeError("MainMenu has multiple preview stages; resolve ownership first.")

preview_path = root + "/Blueprint/UI/BP_PartyMenuPreview"
preview = unreal.load_asset(preview_path) if unreal.EditorAssetLibrary.does_asset_exist(preview_path) else None
if not preview:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.SkeletalMeshActor)
    preview = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_PartyMenuPreview", root + "/Blueprint/UI", unreal.Blueprint, factory)
    player = unreal.EditorAssetLibrary.load_blueprint_class(root + "/Blueprint/Unit/BP_PlayerUnit")
    mesh = unreal.get_default_object(player).get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
    defaults = unreal.get_default_object(preview.generated_class())
    defaults.get_editor_property("skeletal_mesh_component").set_editor_property("skeletal_mesh_asset", mesh)
    defaults.set_actor_enable_collision(False)
    unreal.BlueprintEditorLibrary.compile_blueprint(preview)
    if not unreal.EditorAssetLibrary.save_loaded_asset(preview):
        raise RuntimeError("Could not save menu preview Blueprint.")

stage = stages[0] if stages else actors.spawn_actor_from_class(unreal.MainMenuPreviewStage, unreal.Vector(0, 0, 0))
stage.set_actor_label("MainMenuPreviewStage")
classes = dict(stage.get_editor_property("preview_actor_classes"))
for profession in ["StableHand", "Scholar", "Herbalist", "Hunter"]:
    if not classes.get(unreal.Name(profession)):
        classes[unreal.Name(profession)] = preview.generated_class()
stage.set_editor_property("preview_actor_classes", classes)

# Menu camera and previews do not need a combat pawn spawned by GameMode.
# 메뉴 카메라와 프리뷰에는 GameMode가 생성하는 전투 Pawn이 필요하지 않습니다.
mode_class = world.get_world_settings().get_editor_property("default_game_mode")
mode_asset = unreal.load_asset(mode_class.get_path_name().split(".")[0])
unreal.get_default_object(mode_class).set_editor_property("default_pawn_class", None)
unreal.BlueprintEditorLibrary.compile_blueprint(mode_asset)
if not unreal.EditorAssetLibrary.save_loaded_asset(mode_asset):
    raise RuntimeError("Could not save menu GameMode pawn removal.")

for index, offset in enumerate([-675.0, -225.0, 225.0, 675.0]):
    stage.get_editor_property("slot%d_anchor" % index).set_editor_property("relative_location", unreal.Vector(0, offset, 0))
mesh_component = unreal.get_default_object(preview.generated_class()).get_editor_property("skeletal_mesh_component")
mesh_component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
unreal.BlueprintEditorLibrary.compile_blueprint(preview)
if not unreal.EditorAssetLibrary.save_loaded_asset(preview):
    raise RuntimeError("Could not save menu preview orientation.")
if not unreal.EditorLoadingAndSavingUtils.save_map(world, root + "/LEVEL/MainMenu"):
    raise RuntimeError("Could not save MainMenu stage placement.")
unreal.log("T12: MainMenu stage and four visual-only profession previews configured.")
