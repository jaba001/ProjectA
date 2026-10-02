import hashlib
import json
import math
import sys
from pathlib import Path

import unreal

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

from ConfigureCombatDebugLevel import inspect_level, require


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_MAP = "/Game/User_JeHoon/LEVEL/Gameplay"
DUNGEON_MODE = "/Game/User_JeHoon/Blueprint/Game/BP_CombatDebugGameMode"
LEVELS = ["DungeonFantasy", "DungeonStone"]
DECORATION_TAG = unreal.Name("ProjectADungeonDecoration")
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSETS = unreal.EditorAssetLibrary
COMMAND_LINE = unreal.SystemLibrary.get_command_line()
VERIFY_ONLY = "-DungeonVerifyOnly" in COMMAND_LINE
REBUILD = "-DungeonRebuild" in COMMAND_LINE


def digest(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def protected_hashes(specs):
    paths = set((ROOT / "Content/User_JeHoon").rglob("*.uasset"))
    paths.update((ROOT / "Content/User_JeHoon").rglob("*.umap"))
    for spec in specs:
        directory = ROOT / "Content" / spec["pack_root"].removeprefix("/Game/")
        paths.update(directory.rglob("*.uasset"))
        paths.update(directory.rglob("*.umap"))
    for category in ["__ExternalActors__", "__ExternalObjects__"]:
        directory = ROOT / "Content" / category / "User_JeHoon"
        if directory.exists():
            paths.update(path for path in directory.rglob("*") if path.is_file())
    return {str(path.relative_to(ROOT)): digest(path) for path in sorted(paths) if not any(level in path.parts or path.stem.startswith(level) for level in LEVELS)}


def vector(value):
    return [value.x, value.y, value.z]


def color(value):
    return unreal.LinearColor(*(channel / 255.0 for channel in value), 1.0)


def core_layout(package):
    world, layout = inspect_level(package)
    del layout["actor_classes"]
    layout["game_mode"] = world.get_world_settings().get_editor_property("default_game_mode").get_path_name()
    floor = require(next((actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == "Floor"), None), "Original physical floor is missing")
    component = floor.get_component_by_class(unreal.StaticMeshComponent)
    layout["physical_floor"] = {"transform": floor.get_actor_transform().export_text(), "mesh": component.get_editor_property("static_mesh").get_path_name(), "collision": str(component.get_collision_enabled()), "navigation": component.get_editor_property("can_ever_affect_navigation")}
    layout["navigation_bounds"] = sorted(actor.get_actor_transform().export_text() for actor in ACTORS.get_all_level_actors() if actor.get_class().get_path_name() == "/Script/NavigationSystem.NavMeshBoundsVolume")
    return world, layout


def mark(actor, label, category):
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_enable_collision(False)
    actor.set_actor_label("Dungeon_" + label)
    actor.set_editor_property("tags", [DECORATION_TAG, unreal.Name("Dungeon" + category.capitalize())])
    actor.set_folder_path("Dungeon/" + category.capitalize())
    return actor


def mesh_placement(mesh, item):
    bounds = mesh.get_bounds()
    origin, extent = vector(bounds.origin), vector(bounds.box_extent)
    scale = item.get("scale", [1.0, 1.0, 1.0])
    if "size" in item:
        scale = [size / (axis * 2.0) if axis > 0.001 else 1.0 for size, axis in zip(item["size"], extent)]
    require(len(scale) == 3 and all(value > 0.0 for value in scale), "Invalid mesh scale")
    # Compensate for the source pivot so placement refers to geometry centre and floor contact.
    # 배치를 메시 중심과 바닥 접점 기준으로 해석하도록 원본 피벗을 보정합니다.
    angle = math.radians(item.get("yaw", 0.0))
    pivot_x, pivot_y = origin[0] * scale[0], origin[1] * scale[1]
    reference_z = (origin[2] + extent[2] * (1.0 if item.get("align") == "top" else -1.0)) * scale[2]
    position = item["position"]
    location = unreal.Vector(position[0] - pivot_x * math.cos(angle) + pivot_y * math.sin(angle), position[1] - pivot_x * math.sin(angle) - pivot_y * math.cos(angle), position[2] - reference_z)
    return location, scale


def place_mesh(item):
    mesh = require(unreal.load_asset(item["asset"]), "Missing source mesh: " + item["asset"])
    require(isinstance(mesh, unreal.StaticMesh), "Decoration must reference an original StaticMesh")
    location, scale = mesh_placement(mesh, item)
    actor = require(ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(yaw=item.get("yaw", 0.0))), "Could not place decoration")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    require(component.set_static_mesh(mesh), "Could not assign source mesh")
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    if item.get("material"):
        component.set_material(0, require(unreal.load_asset(item["material"]), "Missing original material"))
    return mark(actor, item["label"], item["category"])


def place_light(item):
    actor = require(ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*item["position"])), "Could not place light")
    component = actor.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity_units(unreal.LightUnits.LUMENS)
    component.set_intensity(item["intensity"])
    component.set_light_color(color(item["color"]), True)
    component.set_attenuation_radius(item["radius"])
    component.set_cast_shadows(item.get("shadows", True))
    component.set_editor_property("source_radius", item.get("source_radius", 25.0))
    component.set_editor_property("can_ever_affect_navigation", False)
    return mark(actor, item["label"], "lighting")


def place_effect(item):
    system = require(unreal.load_asset(item["asset"]), "Missing original ambient effect")
    require(isinstance(system, (unreal.NiagaraSystem, unreal.ParticleSystem)), "Expected an original Niagara or Cascade system")
    actor_class = unreal.NiagaraActor if isinstance(system, unreal.NiagaraSystem) else unreal.Emitter
    actor = require(ACTORS.spawn_actor_from_class(actor_class, unreal.Vector(*item["position"])), "Could not place ambient effect")
    component = actor.get_component_by_class(unreal.NiagaraComponent if isinstance(system, unreal.NiagaraSystem) else unreal.ParticleSystemComponent)
    if isinstance(system, unreal.NiagaraSystem):
        component.set_asset(system)
    else:
        component.set_template(system)
    component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_scale3d(unreal.Vector(*item.get("scale", [1.0, 1.0, 1.0])))
    actor.set_actor_enable_collision(False)
    return mark(actor, item["label"], "effects")


def configure_atmosphere(spec):
    for actor in list(ACTORS.get_all_level_actors()):
        name = actor.get_class().get_path_name()
        if name in ["/Script/Engine.DirectionalLight", "/Script/Engine.SkyAtmosphere", "/Script/Engine.VolumetricCloud", "/Script/Engine.SkyLight"] or actor.get_actor_label() == "SM_SkySphere":
            require(ACTORS.destroy_actor(actor), "Could not remove copied outdoor lighting")
        elif actor.get_actor_label() == "Floor":
            actor.get_component_by_class(unreal.StaticMeshComponent).set_visibility(False)
        elif isinstance(actor, unreal.ExponentialHeightFog):
            actor.set_actor_location(unreal.Vector(-300.0, 400.0, 0.0), False, True)
            component = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            component.set_editor_property("fog_density", spec["palette"]["fog_density"])
            component.set_editor_property("fog_height_falloff", 0.35)
            component.set_editor_property("fog_max_opacity", 0.3)
            component.set_editor_property("start_distance", 300.0)
            component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.035, 0.06, 1.0))
    actor = require(ACTORS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(-300.0, 400.0, 0.0)), "Could not create indoor post process")
    actor.set_editor_property("unbound", True)
    actor.set_editor_property("priority", 10.0)
    settings = actor.get_editor_property("settings")
    for name, value in {"override_auto_exposure_method": True, "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL, "override_auto_exposure_bias": True, "auto_exposure_bias": 0.0, "override_auto_exposure_apply_physical_camera_exposure": True, "auto_exposure_apply_physical_camera_exposure": False, "override_bloom_intensity": True, "bloom_intensity": 0.25, "override_vignette_intensity": True, "vignette_intensity": 0.18, "override_color_saturation": True, "color_saturation": unreal.Vector4(spec["palette"]["saturation"], spec["palette"]["saturation"], spec["palette"]["saturation"], 1.0), "override_motion_blur_amount": True, "motion_blur_amount": 0.0}.items():
        settings.set_editor_property(name, value)
    actor.set_editor_property("settings", settings)
    actor.set_actor_enable_collision(False)
    mark(actor, "IndoorPostProcess", "lighting")


def segment_intersects_box(start, end, minimum, maximum):
    lower, upper = 0.0, 0.999
    for axis in range(3):
        direction = end[axis] - start[axis]
        if abs(direction) < 0.000001:
            if start[axis] < minimum[axis] or start[axis] > maximum[axis]:
                return False
            continue
        entry, exit_value = sorted([(minimum[axis] - start[axis]) / direction, (maximum[axis] - start[axis]) / direction])
        lower, upper = max(lower, entry), min(upper, exit_value)
        if lower > upper:
            return False
    return True


def verify_transform(actor, position, scale, yaw=0.0):
    require(all(abs(actual - expected) < 0.01 for actual, expected in zip(vector(actor.get_actor_location()), position)), "Saved decoration position differs: " + actor.get_actor_label())
    require(all(abs(actual - expected) < 0.0001 for actual, expected in zip(vector(actor.get_actor_scale3d()), scale)), "Saved decoration scale differs")
    rotation = actor.get_actor_rotation()
    require(abs((rotation.yaw - yaw + 180.0) % 360.0 - 180.0) < 0.01 and abs(rotation.pitch) < 0.01 and abs(rotation.roll) < 0.01, "Saved decoration rotation differs")


def verify_decoration(spec, source_layout):
    world, layout = core_layout(spec["level"])
    require(layout == source_layout, "Dungeon changed the source combat or physical floor layout")
    actors = ACTORS.get_all_level_actors()
    decoration = [actor for actor in actors if DECORATION_TAG in actor.get_editor_property("tags")]
    expected = {"Dungeon_" + item["label"] for item in spec["meshes"] + spec["lights"] + spec.get("effects", [])} | {"Dungeon_IndoorPostProcess"}
    require({actor.get_actor_label() for actor in decoration} == expected and len(decoration) == len(expected), "Missing or duplicate authored decoration")
    meshes = []
    camera = next(actor for actor in actors if isinstance(actor, unreal.CameraActor))
    camera_position = vector(camera.get_actor_location())
    samples = [[-row * 200.0, column * 200.0 + (200.0 if column >= 2 else 0.0), height] for row in range(4) for column in range(4) for height in [5.0, 100.0, 200.0]]
    mesh_specs = {"Dungeon_" + item["label"]: item for item in spec["meshes"]}
    for actor in decoration:
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            require(component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Decoration blocks collision: " + actor.get_actor_label())
            require(not component.get_editor_property("can_ever_affect_navigation"), "Decoration affects navigation")
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        item = mesh_specs[actor.get_actor_label()]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        mesh = component.get_editor_property("static_mesh")
        require(mesh.get_path_name().split(".")[0] == item["asset"] and item["asset"].startswith(spec["pack_root"] + "/"), "Decoration no longer references its source pack")
        location, scale = mesh_placement(mesh, item)
        verify_transform(actor, vector(location), scale, item.get("yaw", 0.0))
        require(component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC, "Decoration lost static mobility")
        if item.get("material"):
            require(component.get_material(0).get_path_name().split(".")[0] == item["material"], "Saved material override differs")
        origin, extent = actor.get_actor_bounds(False)
        minimum = [value - half for value, half in zip(vector(origin), vector(extent))]
        maximum = [value + half for value, half in zip(vector(origin), vector(extent))]
        if item["category"] == "floor":
            require(maximum[2] < 4.0, "Decorative floor overlaps the combat grid")
        else:
            require(maximum[0] <= -790.0 or minimum[0] >= 190.0 or maximum[1] <= -190.0 or minimum[1] >= 990.0, "Decoration enters the combat clearance")
            require(not any(segment_intersects_box(camera_position, sample, minimum, maximum) for sample in samples), "Decoration obscures the combat camera")
        meshes.append({"label": actor.get_actor_label(), "mesh": mesh.get_path_name(), "transform": actor.get_actor_transform().export_text(), "bounds_min": minimum, "bounds_max": maximum, "collision": "NoCollision", "navigation": False})
    require(not next(actor for actor in actors if actor.get_actor_label() == "Floor").get_component_by_class(unreal.StaticMeshComponent).get_editor_property("visible"), "Template physical floor must not obscure dungeon floor")
    by_label = {actor.get_actor_label(): actor for actor in decoration}
    lights, effects = [], []
    for item in spec["lights"]:
        actor = by_label["Dungeon_" + item["label"]]
        component = require(actor.get_component_by_class(unreal.PointLightComponent), "Expected a point light")
        verify_transform(actor, item["position"], [1.0, 1.0, 1.0])
        for name, value in {"intensity": item["intensity"], "attenuation_radius": item["radius"], "source_radius": item.get("source_radius", 25.0)}.items():
            require(abs(component.get_editor_property(name) - value) < 0.01, "Saved light setting differs: " + name)
        require(component.get_editor_property("intensity_units") == unreal.LightUnits.LUMENS and component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE, "Saved light units or mobility differs")
        require(component.get_editor_property("cast_shadows") == item.get("shadows", True), "Saved shadow setting differs")
        actual_color, expected_color = component.get_light_color(), color(item["color"])
        require(all(abs(getattr(actual_color, channel) - getattr(expected_color, channel)) < 0.01 for channel in ["r", "g", "b"]), "Saved light color differs")
        lights.append({"label": actor.get_actor_label(), "position": vector(actor.get_actor_location()), "lumens": component.get_editor_property("intensity"), "color": [actual_color.r, actual_color.g, actual_color.b]})
    for item in spec.get("effects", []):
        actor = by_label["Dungeon_" + item["label"]]
        component = actor.get_component_by_class(unreal.NiagaraComponent) or actor.get_component_by_class(unreal.ParticleSystemComponent)
        require(component, "Expected an ambient effect component")
        asset = component.get_asset() if isinstance(component, unreal.NiagaraComponent) else component.get_editor_property("template")
        require(asset.get_path_name().split(".")[0] == item["asset"] and item["asset"].startswith(spec["pack_root"] + "/"), "Saved effect does not reference its original source")
        verify_transform(actor, item["position"], item.get("scale", [1.0, 1.0, 1.0]))
        effects.append({"label": actor.get_actor_label(), "asset": asset.get_path_name(), "transform": actor.get_actor_transform().export_text()})
    post = by_label["Dungeon_IndoorPostProcess"]
    settings = post.get_editor_property("settings")
    require(post.get_editor_property("unbound") and settings.get_editor_property("auto_exposure_method") == unreal.AutoExposureMethod.AEM_MANUAL and not settings.get_editor_property("auto_exposure_apply_physical_camera_exposure"), "Indoor exposure differs")
    fog = require(next((actor for actor in actors if isinstance(actor, unreal.ExponentialHeightFog)), None), "Indoor fog is missing")
    require(abs(fog.get_component_by_class(unreal.ExponentialHeightFogComponent).get_editor_property("fog_density") - spec["palette"]["fog_density"]) < 0.0001, "Saved indoor fog differs")
    return {"level": spec["level"], "pack_root": spec["pack_root"], "layout": layout, "decoration_count": len(decoration), "mesh_count": len(meshes), "light_count": len(lights), "effect_count": len(effects), "meshes": meshes, "lights": lights, "effects": effects, "camera_samples": len(samples), "gameplay_test": "not run"}


def main():
    require(not (VERIFY_ONLY and REBUILD), "Verify and rebuild are mutually exclusive")
    specs = [json.loads((SCRIPT_DIRECTORY / (name + "Spec.json")).read_text(encoding="utf-8")) for name in LEVELS]
    for name, spec in zip(LEVELS, specs):
        require(spec["level"] == "/Game/User_JeHoon/LEVEL/" + name, "Unexpected output level")
        require((ROOT / "Content" / spec["pack_root"].removeprefix("/Game/")).is_dir(), "Download the owned source pack before authoring")
        if not VERIFY_ONLY:
            require(REBUILD or not ASSETS.does_asset_exist(spec["level"]), "Level already exists; inspect or explicitly use -DungeonRebuild")
        else:
            require(ASSETS.does_asset_exist(spec["level"]), "Dungeon level has not been authored")
    # Hash the original project and imported packs; save only the two new worlds.
    # 기존 프로젝트와 임포트한 원본 팩의 해시를 확인하고 새 월드 두 개만 저장합니다.
    before = protected_hashes(specs)
    _, source_layout = core_layout(SOURCE_MAP)
    mode = require(unreal.load_asset(DUNGEON_MODE), "Existing standalone combat GameMode is missing")
    require(isinstance(mode, unreal.Blueprint), "Expected the existing combat GameMode Blueprint")
    mode_class = mode.generated_class()
    defaults = unreal.get_default_object(mode_class)
    require(isinstance(defaults, unreal.CombatDebugGameMode) and defaults.get_editor_property("party_definition") and defaults.get_editor_property("enemy_definition"), "Standalone combat GameMode definitions are missing")
    require(defaults.get_editor_property("player_controller_class") == unreal.load_class(None, "/Script/ProjectA.CombatDebugPlayerController"), "Standalone combat controller differs")
    dungeon_layout = dict(source_layout, game_mode=mode_class.get_path_name())
    reports = []
    for spec in specs:
        unreal.log("DUNGEON_AUTHORING " + spec["level"])
        if not VERIFY_ONLY:
            if not ASSETS.does_asset_exist(spec["level"]):
                require(ASSETS.duplicate_asset(SOURCE_MAP, spec["level"]), "Could not duplicate the combat layout through Unreal")
            world, layout = core_layout(spec["level"])
            require(dict(layout, game_mode=source_layout["game_mode"]) == source_layout and layout["game_mode"] in [source_layout["game_mode"], dungeon_layout["game_mode"]], "Existing dungeon combat layout differs from Gameplay")
            # Reuse the isolated combat mode so each comparison map can be played directly without a Run.
            # Run 없이 각 비교 맵을 직접 실행할 수 있도록 기존 독립 전투 모드를 사용합니다.
            world.get_world_settings().set_editor_property("default_game_mode", mode_class)
            for actor in list(ACTORS.get_all_level_actors()):
                if DECORATION_TAG in actor.get_editor_property("tags"):
                    require(ACTORS.destroy_actor(actor), "Could not rebuild authored decoration")
            configure_atmosphere(spec)
            for item in spec["meshes"]:
                place_mesh(item)
            for item in spec["lights"]:
                place_light(item)
            for item in spec.get("effects", []):
                place_effect(item)
            require(unreal.EditorLoadingAndSavingUtils.save_map(world, spec["level"]), "Could not save authored dungeon")
        reports.append(verify_decoration(spec, dungeon_layout))
    require(protected_hashes(specs) == before, "Original project or source pack assets changed during dungeon authoring")
    output = ROOT / "Saved/Automation/Dungeons" / ("Reload.json" if VERIFY_ONLY else "Configuration.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"mode": "verify" if VERIFY_ONLY else "configure", "levels": reports, "protected_project_files": len(before), "source_preserved": True, "gameplay_test": "not run"}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("DUNGEON_AUTHORING_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
