import json
from pathlib import Path

import unreal


# Author and inspect editor assets only; this tool never creates a game or PIE world.
# 에디터 에셋만 작성하고 검사하며 게임·PIE 월드를 생성하지 않습니다.
ROOT = Path(unreal.Paths.project_dir()).resolve()
GAMEPLAY = "/Game/User_JeHoon/LEVEL/Core/Gameplay"
FOLDER = "/Game/User_JeHoon/Blueprint/Environment/"
NAMES = ["MeadowBloom", "DungeonStone", "IceCitadel"]
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSETS = unreal.EditorAssetLibrary
OVERWRITE = "-ArenaEnvironmentOverwrite" in unreal.SystemLibrary.get_command_line()
VERIFY_ONLY = "-ArenaEnvironmentVerifyOnly" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def core_state():
    result = {}
    for actor in ACTORS.get_all_level_actors():
        if actor.get_actor_label() not in ["Floor", "GameplayCamera"] and not isinstance(actor, (unreal.CombatArena, unreal.CombatGridManager)):
            continue
        record = {"transform": actor.get_actor_transform().export_text(), "collision": actor.get_actor_enable_collision()}
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            record[component.get_name()] = {
                "mesh": component.get_editor_property("static_mesh").get_path_name(),
                "collision": str(component.get_collision_enabled()),
                "visible": component.get_editor_property("visible"),
                "materials": [component.get_material(index).get_path_name() for index in range(component.get_num_materials())]
            }
        result[actor.get_name()] = record
    return result


def vector_values(value, digits):
    return [round(value.x, digits), round(value.y, digits), round(value.z, digits)]


def floor_records(components):
    result = []
    for component in components:
        mesh = component.get_editor_property("static_mesh")
        if mesh and mesh.get_path_name().endswith("SM_Floor_Flagstone_01.SM_Floor_Flagstone_01"):
            require(not isinstance(component, unreal.InstancedStaticMeshComponent), "Flagstone must retain ordinary mesh material usage")
            rotation = component.get_world_rotation()
            result.append({
                "mesh": mesh.get_path_name(),
                "location": vector_values(component.get_world_location(), 3),
                "scale": vector_values(component.get_world_scale(), 5),
                "rotation": [round(rotation.pitch, 3), round(rotation.yaw, 3), round(rotation.roll, 3)],
                "materials": [component.get_material(index).get_path_name() for index in range(component.get_num_materials())]
            })
    return sorted(result, key=lambda value: str(value))


def main():
    require(unreal.EditorLoadingAndSavingUtils.load_map(GAMEPLAY), "Cannot load Gameplay")
    baseline = core_state()
    arenas = [actor for actor in ACTORS.get_all_level_actors() if isinstance(actor, unreal.CombatArena)]
    require(len(arenas) == 1, "Expected one arena")
    arena = arenas[0]
    floor = require(next((actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == "Floor"), None), "Physical floor missing")
    if not VERIFY_ONLY:
        report = unreal.CombatArenaBakeLibrary.bake_combat_arena_environments(OVERWRITE)
        require(report is not None, "Environment bake failed; inspect the editor log")
        unreal.log(report)
        occluders = list(arena.get_editor_property("environment_occluders"))
        require(not occluders or occluders == [floor], "Preserve custom environment occluders; configure this map explicitly")
        if not occluders:
            arena.modify()
            arena.set_editor_property("environment_occluders", [floor])
            require(unreal.EditorLoadingAndSavingUtils.save_map(arena.get_world(), GAMEPLAY), "Cannot save Gameplay occluder reference")
    require(list(arena.get_editor_property("environment_occluders")) == [floor], "Physical floor is not configured as a visual occluder")
    templates = arena.get_editor_property("environment_templates")
    records = {}
    for name in NAMES:
        path = FOLDER + "BP_CombatArena_" + name
        require(unreal.load_asset(path), "Baked environment missing: " + name)
        klass = require(ASSETS.load_blueprint_class(path), "Baked environment class missing")
        require(templates[unreal.Name(name)] == klass, "Arena template points elsewhere")
        actor = require(ACTORS.spawn_actor_from_class(klass, unreal.Vector()), "Cannot inspect authored Blueprint")
        try:
            components = actor.get_components_by_class(unreal.StaticMeshComponent)
            require(components, "Blueprint has no serialized meshes")
            for component in components:
                collision_disabled = component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
                require(collision_disabled and not component.get_editor_property("can_ever_affect_navigation"), "Decoration changes collision/navigation")
                require(component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC, "Decoration lost authored static mobility")
            records[name] = {"path": path, "components": len(components), "flagstone": floor_records(components)}
        finally:
            require(ACTORS.destroy_actor(actor), "Cannot clean inspection actor")
    require(core_state() == baseline, "Core transform, camera, collision or physical floor material changed")
    # Compare persisted stone geometry and material assignments against the independent authored map.
    # 독립 작성 맵과 저장된 석재 지오메트리·재질 할당을 대조합니다.
    require(unreal.EditorLoadingAndSavingUtils.load_map("/Game/User_JeHoon/LEVEL/Environment/Dungeon/DungeonStone"), "Cannot load independent stone map")
    original = floor_records([component for actor in ACTORS.get_all_level_actors() for component in actor.get_components_by_class(unreal.StaticMeshComponent)])
    require(len(original) == 36 and records["DungeonStone"]["flagstone"] == original, "Baked stone floor differs from independent DungeonStone")
    output = ROOT / "Saved/Automation/CombatArenaBake.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("COMBAT_ARENA_ASSETS_VERIFIED")


if __name__ == "__main__":
    main()
