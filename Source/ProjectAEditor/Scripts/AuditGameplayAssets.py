import json
import os
import unreal

# Load assets without saving them; write evidence only under Saved.
# 에셋은 저장하지 않고 읽으며 확인 결과만 Saved 아래에 기록합니다.
report = {"assets": [], "maps": [], "world_map_references": []}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True, include_searchable_names=True, include_soft_management_references=True, include_hard_management_references=True)
report["world_map_references"] = [str(value) for value in registry.get_referencers("/Game/User_JeHoon/LEVEL/WorldMap", options)]

properties = ["default_game_mode", "player_controller_class", "default_pawn_class", "spectator_class", "hud_class", "combat_manager_class", "player_unit_classes", "enemy_unit_classes", "player_coords", "enemy_coords", "tile_class", "row_count", "col_count", "spacing", "gap_spacing", "gap_start_index", "hud_widget_class", "main_menu_root_widget_class", "main_menu_screen_widget_class", "character_creation_widget_class", "world_map_level_name", "equipped_skill_data_assets", "equipped_skill_ability_classes", "default_attack_ability_class", "ability_class", "attack_montage", "move_to_target", "damage", "base_damage", "skill_actor_class", "camera_actor", "auto_activate_for_player", "auto_possess_ai", "ai_controller_class", "main_menu_widget_class", "gameplay_level_name", "party_definition", "encounter_definitions", "fallback_player_unit_class", "gameplay_root_widget_class", "run_map_widget_class", "result_widget_class", "combat_hud_widget_class", "grid", "camera_anchor"]
properties.extend(["start_game_level_name", "damage_amount", "spawned_attack_actor_class", "empty_sprite", "player_sprite", "enemy_sprite", "movable_sprite", "active_sprite"])

def inspect_object(obj):
    result = {"path": obj.get_path_name(), "class": obj.get_class().get_path_name(), "properties": {}}
    for name in properties:
        try:
            result["properties"][name] = str(obj.get_editor_property(name))
        except Exception:
            pass
    if isinstance(obj, unreal.Actor):
        result["location"] = str(obj.get_actor_location())
        result["rotation"] = str(obj.get_actor_rotation())
        result["label"] = obj.get_actor_label()
        result["bounds"] = str(obj.get_actor_bounds(False))
    return result

for data in registry.get_assets_by_path("/Game/User_JeHoon", recursive=True):
    if str(data.asset_class_path.asset_name) == "World":
        continue
    obj = data.get_asset()
    if not obj:
        continue
    entry = inspect_object(obj)
    if isinstance(obj, unreal.Blueprint):
        try:
            generated = obj.generated_class()
            entry["defaults"] = inspect_object(unreal.get_default_object(generated))
            entry["tags"] = str(data.tags_and_values) if hasattr(data, "tags_and_values") else ""
        except Exception as error:
            entry["error"] = str(error)
    report["assets"].append(entry)

map_names = ["TestMap", "MainMenu", "WorldMap"]
if unreal.EditorAssetLibrary.does_asset_exist("/Game/User_JeHoon/LEVEL/Gameplay"):
    map_names.append("Gameplay")
for map_name in map_names:
    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/User_JeHoon/LEVEL/" + map_name)
    entry = {"name": map_name, "loaded": bool(world), "actors": []}
    if world:
        entry["world_settings"] = inspect_object(world.get_world_settings())
        for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
            entry["actors"].append(inspect_object(actor))
    report["maps"].append(entry)

output = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "GameplayAssetAudit.json")
os.makedirs(os.path.dirname(output), exist_ok=True)
with open(output, "w", encoding="utf-8") as handle:
    json.dump(report, handle, ensure_ascii=False, indent=2)
unreal.log("GAMEPLAY_ASSET_AUDIT: " + output)
