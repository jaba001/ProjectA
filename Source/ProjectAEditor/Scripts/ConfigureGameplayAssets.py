import json
import os
import unreal


# Run through the PythonScript commandlet with PythonScriptPlugin temporarily enabled.
# PythonScriptPlugin을 명령줄에서 일시 활성화한 PythonScript commandlet으로 실행합니다.
ROOT = "/Game/User_JeHoon"
OUTPUTS = {
    "party": ROOT + "/Blueprint/DataAsset/DA_VerticalSliceParty",
    "encounter": ROOT + "/Blueprint/DataAsset/DA_DefaultEncounter",
    "controller": ROOT + "/Blueprint/Controller/BP_GameplayPlayerController",
    "game_mode": ROOT + "/Blueprint/Game/BP_GameplayGameMode",
    "map": ROOT + "/LEVEL/Gameplay",
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets_to_save = []


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load_class(path):
    return require(unreal.load_class(None, path), "Class not found: " + path)


def create_blueprint(path, parent):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset = require(tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], unreal.Blueprint, factory), "Blueprint creation failed: " + path)
    assets_to_save.append(asset)
    return asset


def create_data_asset(path, asset_class):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = require(tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], asset_class, factory), "Data asset creation failed: " + path)
    assets_to_save.append(asset)
    return asset


def widget_class(name, native_name):
    path = ROOT + "/UI/Gameplay/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return load_class(path + "." + name + "_C")
    return load_class("/Script/ProjectA." + native_name)


# Refuse to overwrite previously generated or manually edited gameplay content.
# 이전 생성본이나 사용자가 편집한 Gameplay 콘텐츠를 덮어쓰지 않습니다.
existing = [path for path in OUTPUTS.values() if unreal.EditorAssetLibrary.does_asset_exist(path)]
require(not existing, "Gameplay assets already exist; inspect and edit them in the editor instead of overwriting: " + ", ".join(existing))
player_class = load_class(ROOT + "/Blueprint/Unit/BP_PlayerUnit.BP_PlayerUnit_C")
enemy_class = load_class(ROOT + "/Blueprint/Unit/BP_EnemyUnit.BP_EnemyUnit_C")
party_class = load_class("/Script/ProjectA.PartyDefinitionDataAsset")
encounter_class = load_class("/Script/ProjectA.EncounterDefinitionDataAsset")
mode_class = load_class("/Script/ProjectA.GameplayGameModeBase")
controller_class = load_class("/Script/ProjectA.GameplayPlayerController")
arena_class = load_class("/Script/ProjectA.CombatArena")
source_world = require(unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/TestMap"), "TestMap could not be loaded.")
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
source_actors = actor_subsystem.get_all_level_actors()
source_grids = [actor for actor in source_actors if isinstance(actor, unreal.CombatGridManager)]
require(len(source_grids) == 1, "TestMap must contain exactly one combat grid.")
require(not any(isinstance(actor, unreal.UnitBase) or isinstance(actor, unreal.CombatManager) for actor in source_actors), "TestMap contains placed combat units or managers; inspect before copying.")
require(any(actor.get_class().get_path_name() == "/Script/NavigationSystem.NavMeshBoundsVolume" for actor in source_actors), "TestMap has no navigation bounds volume.")

party = create_data_asset(OUTPUTS["party"], party_class)
party.set_editor_property("fallback_player_unit_class", player_class)
party.set_editor_property("player_unit_classes", {unreal.Name(name): player_class for name in ["StableHand", "Scholar", "Herbalist", "Hunter"]})
encounter = create_data_asset(OUTPUTS["encounter"], encounter_class)
encounter.set_editor_property("enemy_unit_classes", [enemy_class])

# Designer widgets are optional; native fallbacks remain usable without generated WBP assets.
# Designer 위젯은 선택 사항이며 WBP 에셋이 없어도 native fallback을 사용할 수 있습니다.
root_widget_class = widget_class("WBP_GameplayRootWidget", "GameplayRootWidget")
root_widget_path = ROOT + "/UI/Gameplay/WBP_GameplayRootWidget"
if unreal.EditorAssetLibrary.does_asset_exist(root_widget_path):
    root_widget_asset = unreal.load_asset(root_widget_path)
    root_defaults = unreal.get_default_object(root_widget_class)
    root_defaults.set_editor_property("run_map_widget_class", widget_class("WBP_RunMapWidget", "RunMapWidget"))
    root_defaults.set_editor_property("combat_hud_widget_class", widget_class("WBP_CombatHUDWidget", "CombatHUDWidget"))
    root_defaults.set_editor_property("result_widget_class", widget_class("WBP_EncounterResultWidget", "EncounterResultWidget"))
    unreal.BlueprintEditorLibrary.compile_blueprint(root_widget_asset)
    assets_to_save.append(root_widget_asset)

controller = create_blueprint(OUTPUTS["controller"], controller_class)
unreal.get_default_object(controller.generated_class()).set_editor_property("gameplay_root_widget_class", root_widget_class)
unreal.BlueprintEditorLibrary.compile_blueprint(controller)
mode = create_blueprint(OUTPUTS["game_mode"], mode_class)
mode_defaults = unreal.get_default_object(mode.generated_class())
mode_defaults.set_editor_property("party_definition", party)
mode_defaults.set_editor_property("encounter_definitions", {unreal.Name("DefaultEncounter"): encounter})
mode_defaults.set_editor_property("player_controller_class", controller.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(mode)

# Duplicate through Unreal asset APIs so TestMap and its serialized references remain untouched.
# TestMap과 직렬화된 참조를 보존하도록 Unreal 에셋 API로 복제합니다.
gameplay_asset = require(unreal.EditorAssetLibrary.duplicate_asset(ROOT + "/LEVEL/TestMap", OUTPUTS["map"]), "Gameplay map duplication failed.")
world = require(unreal.EditorLoadingAndSavingUtils.load_map(OUTPUTS["map"]), "Gameplay could not be loaded after duplication.")
require(world.get_path_name() == OUTPUTS["map"] + ".Gameplay", "Refusing to save an unexpected world: " + world.get_path_name())
world.get_world_settings().set_editor_property("default_game_mode", mode.generated_class())
grid = require(next((actor for actor in actor_subsystem.get_all_level_actors() if isinstance(actor, unreal.CombatGridManager)), None), "Copied Gameplay grid not found.")
# Keep sprites above the reused floor surface to avoid coplanar depth fighting.
# 재사용한 바닥과 같은 평면의 깊이 충돌을 피하도록 스프라이트를 위에 둡니다.
grid_location = grid.get_actor_location()
grid_location.z += 5.0
grid.set_actor_location(grid_location, False, True)
camera = require(actor_subsystem.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(-300.0, -1000.0, 1500.0), unreal.Rotator(pitch=-46.97, yaw=90.0, roll=0.0)), "Camera could not be created.")
camera.set_actor_label("GameplayCamera")
camera.get_editor_property("camera_component").set_editor_property("field_of_view", 55.0)
arena = require(actor_subsystem.spawn_actor_from_class(arena_class, unreal.Vector(0.0, 0.0, 0.0)), "Arena could not be created.")
arena.set_actor_label("GameplayCombatArena")
arena.set_editor_property("grid", grid)
arena.set_editor_property("camera_anchor", camera)
arena.set_editor_property("player_coords", [unreal.IntPoint(index, 1) for index in range(4)])
arena.set_editor_property("enemy_coords", [unreal.IntPoint(index, 2) for index in range(4)])

for asset in assets_to_save:
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Asset save failed: " + asset.get_path_name())
require(unreal.EditorLoadingAndSavingUtils.save_map(world, OUTPUTS["map"]), "Gameplay map save failed.")
report = {"created": OUTPUTS, "source_map": ROOT + "/LEVEL/TestMap", "game_mode": mode.generated_class().get_path_name(), "controller": controller.generated_class().get_path_name(), "root_widget": root_widget_class.get_path_name(), "grid": grid.get_path_name(), "arena": arena.get_path_name(), "camera": camera.get_path_name(), "camera_location": str(camera.get_actor_location()), "camera_rotation": str(camera.get_actor_rotation()), "party_fallback": player_class.get_path_name(), "enemy": enemy_class.get_path_name(), "validation": "Asset configuration and package save only; PIE gameplay is verified separately."}
output = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "GameplayProvisioning.json")
os.makedirs(os.path.dirname(output), exist_ok=True)
with open(output, "w", encoding="utf-8") as handle:
    json.dump(report, handle, ensure_ascii=False, indent=2)
unreal.log("GAMEPLAY_PROVISIONING_COMPLETE: " + output)
unreal.log("Run ResavePackages -Package=/Game/User_JeHoon/LEVEL/Gameplay -BuildNavigationData before ValidateGameplayAssets.py or PIE.")
