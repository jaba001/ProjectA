import json
from pathlib import Path

import unreal


# Edit only the existing PvE encounter and its placed formation, preserving unit definitions.
# 유닛 정의를 보존하고 기존 PvE 인카운터와 배치된 진형만 수정합니다.
map_path = "/Game/User_JeHoon/LEVEL/Gameplay"
encounter_path = "/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_DefaultEncounter"
formation = [(1, 2), (2, 2), (0, 3), (3, 3)]
verify_only = "-TestEnemiesVerifyOnly" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


world = require(unreal.EditorLoadingAndSavingUtils.load_map(map_path), "Gameplay map is missing")
require(world.get_path_name() == map_path + ".Gameplay", "Unexpected world")
mode_class = require(world.get_world_settings().get_editor_property("default_game_mode"), "Gameplay mode override is missing")
mode = unreal.get_default_object(mode_class)
encounter = require(mode.get_editor_property("encounter_definitions").get(unreal.Name("DefaultEncounter")), "DefaultEncounter is not mapped")
require(encounter.get_path_name() == encounter_path + ".DA_DefaultEncounter", "Unexpected encounter asset")
require(str(encounter.get_editor_property("opponent_snapshot_slot")) == "None", "Refusing to replace a Snapshot encounter")
classes = list(encounter.get_editor_property("enemy_unit_classes"))
require(classes and all(cls and cls == classes[0] for cls in classes), "The current encounter must use one existing enemy class")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
arenas = [actor for actor in actors if isinstance(actor, unreal.CombatArena)]
require(len(arenas) == 1, "Gameplay must have one combat arena")
arena = arenas[0]
grid = require(arena.get_editor_property("grid"), "Arena grid is missing")
require(grid.get_editor_property("row_count") == 4 and grid.get_editor_property("col_count") == 4, "Unexpected grid dimensions")
if not verify_only:
    encounter.set_editor_property("enemy_unit_classes", [classes[0]] * 4)
    arena.set_editor_property("enemy_coords", [unreal.IntPoint(x, y) for x, y in formation])
    require(unreal.EditorAssetLibrary.save_loaded_asset(encounter), "Could not save encounter")
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, map_path), "Could not save Gameplay formation")
classes = list(encounter.get_editor_property("enemy_unit_classes"))
coords = [(coord.x, coord.y) for coord in arena.get_editor_property("enemy_coords")]
require(len(classes) == 4 and all(cls == classes[0] for cls in classes), "Expected four unchanged enemy classes")
require(coords == formation and len(set(coords)) == 4, "Expected two front and two rear enemies on distinct tiles")
result = {"mode": "reload" if verify_only else "configure", "encounter": encounter.get_path_name(), "enemy_count": len(classes), "enemy_class": classes[0].get_path_name(), "formation": coords, "front": 2, "rear": 2, "gameplay_test": "not run"}
output = Path(unreal.Paths.project_saved_dir(), "Automation", "TestEnemiesReload.json" if verify_only else "TestEnemiesConfigure.json")
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("TEST_ENEMIES_CONFIGURED " + str(output))
