import json
import os
import unreal


# Validate saved packages in a separate editor process after generation.
# 생성 후 별도 에디터 프로세스에서 저장된 패키지를 검증합니다.
ROOT = "/Game/User_JeHoon"
checks = []


def check(value, message):
    if not value:
        raise RuntimeError(message)
    checks.append(message)
    return value


world = check(unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/Gameplay"), "Gameplay map loads")
mode_class = check(world.get_world_settings().get_editor_property("default_game_mode"), "Gameplay has a GameMode override")
mode = unreal.get_default_object(mode_class)
check(isinstance(mode, unreal.GameplayGameModeBase), "Gameplay GameMode derives from GameplayGameModeBase")
party = check(mode.get_editor_property("party_definition"), "Gameplay GameMode references saved party definition")
check(party.get_editor_property("fallback_player_unit_class"), "Party fallback PlayerUnit class is assigned")
player_classes = party.get_editor_property("player_unit_classes")
check(set(str(key) for key in player_classes) == {"StableHand", "Scholar", "Herbalist", "Hunter"}, "All four ClassIds are mapped")
check(all(value for value in player_classes.values()), "Every ClassId has a PlayerUnit class")
encounters = mode.get_editor_property("encounter_definitions")
check(unreal.Name("DefaultEncounter") in encounters, "DefaultEncounter is mapped")
check(len(encounters[unreal.Name("DefaultEncounter")].get_editor_property("enemy_unit_classes")) == 1, "DefaultEncounter has one configured enemy")
controller = unreal.get_default_object(mode.get_editor_property("player_controller_class"))
check(isinstance(controller, unreal.GameplayPlayerController), "Gameplay controller derives from GameplayPlayerController")
root_class = check(controller.get_editor_property("gameplay_root_widget_class"), "Gameplay root widget class is assigned")
root = unreal.get_default_object(root_class)
check(isinstance(root, unreal.GameplayRootWidget), "Gameplay root derives from GameplayRootWidget")
for property_name, expected_class in [("run_map_widget_class", unreal.RunMapWidget), ("combat_hud_widget_class", unreal.CombatHUDWidget), ("result_widget_class", unreal.EncounterResultWidget)]:
    widget_class = check(root.get_editor_property(property_name), property_name + " is assigned")
    check(isinstance(unreal.get_default_object(widget_class), expected_class), property_name + " has the expected native parent")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
arenas = [actor for actor in actors if isinstance(actor, unreal.CombatArena)]
grids = [actor for actor in actors if isinstance(actor, unreal.CombatGridManager)]
check(len(arenas) == 1 and len(grids) == 1, "Gameplay contains exactly one Arena and one Grid")
arena = arenas[0]
grid = grids[0]
check(arena.get_editor_property("grid") == grid, "Arena references the Gameplay grid")
check(isinstance(arena.get_editor_property("camera_anchor"), unreal.CameraActor), "Arena references a placed CameraActor")
check(len(arena.get_editor_property("player_coords")) == 4, "Arena supports four party slots")
check(grid.get_editor_property("tile_class"), "Grid tile class is assigned")
check(grid.get_editor_property("row_count") == 4 and grid.get_editor_property("col_count") == 4, "Grid has the expected 4 x 4 dimensions")
check(any(actor.get_class().get_path_name() == "/Script/NavigationSystem.NavMeshBoundsVolume" for actor in actors), "Gameplay retains navigation bounds")
check(any(actor.get_class().get_path_name() == "/Script/NavigationSystem.RecastNavMesh" for actor in actors), "Gameplay retains a Recast navigation data actor")
check(not any(isinstance(actor, unreal.UnitBase) or isinstance(actor, unreal.CombatManager) for actor in actors), "Gameplay has no placed combat units or combat managers")

menu_class = unreal.load_class(None, ROOT + "/Blueprint/Controller/BP_MainMenuPlayerController.BP_MainMenuPlayerController_C")
menu_defaults = unreal.get_default_object(check(menu_class, "Existing MainMenu controller Blueprint loads"))
check(str(menu_defaults.get_editor_property("gameplay_level_name")) == ROOT + "/LEVEL/Gameplay", "Existing MainMenu controller resolves GameplayLevelName to Gameplay")
report = {"passed": len(checks), "checks": checks, "gameplay_world": world.get_path_name(), "game_mode": mode_class.get_path_name(), "root_widget": root_class.get_path_name(), "scope": "Loaded asset/class references and configured level actors; this is not a PIE gameplay test."}
output = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "GameplayAssetValidation.json")
os.makedirs(os.path.dirname(output), exist_ok=True)
with open(output, "w", encoding="utf-8") as handle:
    json.dump(report, handle, ensure_ascii=False, indent=2)
unreal.log("GAMEPLAY_ASSET_VALIDATION_PASSED: " + str(len(checks)))
