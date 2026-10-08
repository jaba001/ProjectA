import hashlib
import json
import math
import re
import sys
from pathlib import Path

import unreal


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

# These modules guard their authoring entry points; only their placement helpers are reused.
# 이 모듈은 작성 진입점을 보호하므로 배치 보조 함수만 재사용합니다.
import ConfigureEnvironmentLevels as environment
from ConfigureCombatDebugLevel import coordinates, require
from ConfigureDungeonLevels import segment_intersects_box, vector
from ProjectLevelPaths import project_level_path


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / "Saved/Automation/UnifiedGameplay_20261008"
ASSETS = unreal.EditorAssetLibrary
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
OWNER_TAG = unreal.Name("ProjectAUnifiedGameplayVisual")
OWNER_KEY = "ProjectA.UnifiedGameplay"
OWNER_VALUE = "UnifiedGameplay.v1"
OVERVIEW_TAG = unreal.Name("GameplayEncounterOverview")
VERIFY_ONLY = "-UnifiedGameplayVerifyOnly" in unreal.SystemLibrary.get_command_line()


def digest(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def package_file(package, suffix):
    return ROOT / "Content" / (package.removeprefix("/Game/") + suffix)


def protected_hashes(spec):
    allowed = {package_file(spec["level"], ".umap"), package_file(spec["material"], ".uasset")}
    directories = [ROOT / "Content", ROOT / "Config", ROOT / "Saved/Config", ROOT / "Saved/SaveGames"]
    return {str(path.relative_to(ROOT)): digest(path) for directory in directories if directory.exists() for path in sorted(directory.rglob("*")) if path.is_file() and path not in allowed}


def normalized(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    if isinstance(value, (bool, int, float, str)) or value is None:
        return value
    return str(value)


def properties(obj, names):
    return {name: normalized(obj.get_editor_property(name)) for name in names}


def preserved_state():
    # Inspect the current world without reloading and discarding unsaved authored actors.
    # 저장 전 작성 Actor를 버리는 재로드 없이 현재 월드를 검사합니다.
    world = require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "Editor world is unavailable")
    require(world.get_path_name() == project_level_path("Gameplay") + ".Gameplay", "Unexpected loaded Gameplay world")
    current_actors = list(ACTORS.get_all_level_actors())
    arenas = [actor for actor in current_actors if isinstance(actor, unreal.CombatArena)]
    grids = [actor for actor in current_actors if isinstance(actor, unreal.CombatGridManager)]
    require(len(arenas) == 1 and len(grids) == 1, "Gameplay must retain exactly one arena and grid")
    arena, grid = arenas[0], grids[0]
    require(arena.get_editor_property("grid") == grid and arena.get_editor_property("camera_anchor") in current_actors, "Arena grid or camera reference is broken")
    require(not any(isinstance(actor, (unreal.UnitBase, unreal.CombatManager)) for actor in current_actors), "Unexpected placed combat unit or manager")
    require(any(actor.get_actor_label() == "Floor" for actor in current_actors), "Original physical floor is missing")
    for class_path in ["/Script/NavigationSystem.NavMeshBoundsVolume", "/Script/NavigationSystem.RecastNavMesh"]:
        require(any(actor.get_class().get_path_name() == class_path for actor in current_actors), "Original navigation actor is missing")
    layout = {"game_mode": normalized(world.get_world_settings().get_editor_property("default_game_mode")), "grid": properties(grid, ["row_count", "col_count", "tile_class"]), "arena": properties(arena, ["grid", "camera_anchor"]), "player_coords": [list(value) for value in coordinates(arena.get_editor_property("player_coords"))], "enemy_coords": [list(value) for value in coordinates(arena.get_editor_property("enemy_coords"))]}
    require(layout["grid"]["row_count"] == 4 and layout["grid"]["col_count"] == 4 and len(layout["player_coords"]) == 4 and len(layout["enemy_coords"]) == 4, "Gameplay grid layout differs")
    actors = {}
    for actor in current_actors:
        if OWNER_TAG in actor.get_editor_property("tags"):
            continue
        record = {"class": actor.get_class().get_path_name(), "label": actor.get_actor_label(), "transform": actor.get_actor_transform().export_text(), "tags": sorted(str(tag) for tag in actor.get_editor_property("tags")), "collision": actor.get_actor_enable_collision(), "components": {}}
        for component in actor.get_components_by_class(unreal.SceneComponent):
            details = properties(component, ["relative_location", "relative_rotation", "relative_scale3d", "mobility", "visible"])
            if isinstance(component, unreal.PrimitiveComponent):
                details.update(properties(component, ["can_ever_affect_navigation", "cast_shadow"]))
                details["collision"] = str(component.get_collision_enabled())
            if isinstance(component, unreal.StaticMeshComponent):
                details["mesh"] = normalized(component.get_editor_property("static_mesh"))
                details["materials"] = [normalized(component.get_material(index)) for index in range(component.get_num_materials())]
            if isinstance(component, unreal.LightComponentBase):
                details.update(properties(component, ["intensity", "light_color"]))
            if isinstance(component, unreal.SkyLightComponent):
                details.update(properties(component, ["source_type", "cubemap", "real_time_capture"]))
            if isinstance(component, unreal.ExponentialHeightFogComponent):
                details.update(properties(component, ["fog_density", "fog_height_falloff", "fog_max_opacity", "start_distance", "enable_volumetric_fog"]))
            if isinstance(component, unreal.CameraComponent):
                details.update(properties(component, ["field_of_view", "aspect_ratio", "constrain_aspect_ratio", "projection_mode", "post_process_settings", "post_process_blend_weight"]))
            record["components"][component.get_name()] = details
        if isinstance(actor, unreal.PostProcessVolume):
            record["post_process"] = properties(actor, ["settings", "unbound", "priority", "blend_weight", "blend_radius"])
        actors[actor.get_name()] = record
    return world, {"layout": layout, "actors": dict(sorted(actors.items()))}


def tag_container(names):
    result = unreal.GameplayTagContainer()
    require(result.import_text("(GameplayTags=(" + ",".join('(TagName="' + name + '")' for name in names) + "))"), "Could not import encounter tags")
    require(sorted(re.findall(r'TagName="?([A-Za-z0-9_.]+)"?', result.export_text())) == sorted(names), "Encounter tag is not registered")
    return result


def read_specs():
    spec = json.loads((SCRIPT_DIRECTORY / "UnifiedGameplaySpecs.json").read_text(encoding="utf-8"))
    require(spec["schema_version"] == 1 and spec["level"] == project_level_path("Gameplay"), "Unsupported unified world specification")
    require(spec["material"] == "/Game/User_JeHoon/Materials/Prototype/M_EncounterPrototype", "Unexpected prototype material output")
    require("-EnvironmentNames=" not in unreal.SystemLibrary.get_command_line(), "Unified authoring always includes all comparison regions")
    sources = {item["name"]: item for item in environment.read_specs()}
    require(len(sources) == 12, "Expected the twelve existing environment specifications")
    for name in ["DungeonFantasy", "DungeonStone"]:
        item = json.loads((SCRIPT_DIRECTORY / (name + "Spec.json")).read_text(encoding="utf-8"))
        item.update(name=name, source_roots=[item["pack_root"]])
        sources[name] = item
    require(len(spec["regions"]) == 14 and {region["name"] for region in spec["regions"]} == set(sources), "Unified regions must include each existing comparison map once")
    offsets = [tuple(region["offset"]) for region in spec["regions"]]
    require(len(set(offsets)) == len(offsets) and all(len(offset) == 3 and all(math.isfinite(value) and value % 12000 == 0 for value in offset) and offset[2] == 0 for offset in offsets), "Regions must occupy distinct 12000cm grid positions")
    require(next(region for region in spec["regions"] if region["name"] == "PineRidge")["offset"] == [0, 0, 0], "PineRidge must preserve the central arena origin")
    require(len(spec["stages"]) == 5 and len({stage["id"] for stage in spec["stages"]}) == 5, "Expected five unique encounter stages")
    for stage in spec["stages"]:
        require(len(stage["position"]) == 3 and all(math.isfinite(value) for value in stage["position"]), "Invalid stage position")
        require(3500 <= math.hypot(*stage["position"][:2]) <= 5000 and stage["style"] in range(5), "Stage must stay outside combat at the specified radius")
        tag_container(stage["required"])
        tag_container(stage["excluded"])
    require(unreal.load_class(None, "/Script/ProjectA.EncounterPrototypeStage"), "Compile EncounterPrototypeStage before authoring")
    for name, source in sources.items():
        require(source["level"] == project_level_path(name) and ASSETS.does_asset_exist(source["level"]), "A preserved comparison map is missing: " + name)
    return spec, sources


def is_reserved(minimum, maximum, spec):
    for stage in spec["stages"]:
        low = [stage["position"][axis] + spec["stage_clearance_min"][axis] for axis in range(3)]
        high = [stage["position"][axis] + spec["stage_clearance_max"][axis] for axis in range(3)]
        if all(maximum[axis] >= low[axis] and minimum[axis] <= high[axis] for axis in range(3)):
            return True
    return False


def prepare_regions(spec, sources):
    camera = next(actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == "GameplayCamera")
    camera_position = vector(camera.get_actor_location())
    combat_samples = [[-row * 200.0, column * 200.0 + (200.0 if column >= 2 else 0.0), height] for row in range(4) for column in range(4) for height in [5.0, 100.0, 200.0]]
    prepared = []
    for region in spec["regions"]:
        source = sources[region["name"]]
        result = {"name": region["name"], "source_level": source["level"], "offset": region["offset"], "meshes": [], "excluded": []}
        require(len({item["label"] for item in source["meshes"]}) == len(source["meshes"]), "Duplicate source mesh labels")
        for original in source["meshes"]:
            item = dict(original)
            environment.original_path(item["asset"], source)
            require(item["category"] in ["floor", "architecture", "prop"], "Unexpected source mesh category")
            require(len(item["position"]) == 3 and all(math.isfinite(value) for value in item["position"]), "Invalid source position")
            for field in ["scale", "size"]:
                require(field not in item or (len(item[field]) == 3 and all(math.isfinite(value) and (value >= 0 if field == "size" else value > 0) for value in item[field])), "Invalid source mesh " + field + ": " + region["name"] + "/" + item["label"])
            for path in environment.material_paths(item):
                environment.material_path(path, source)
            mesh = require(unreal.load_asset(item["asset"]), "Missing original mesh: " + item["asset"])
            require(isinstance(mesh, unreal.StaticMesh) and mesh.get_path_name().split(".")[0] == item["asset"], "Unexpected or redirected source mesh")
            # Original planar walls keep unit scale on their zero-width axis, matching the shared placement helper.
            # 원본 평면 벽은 공통 배치 함수와 동일하게 두께가 0인 축의 배율 1을 유지합니다.
            if "size" in item:
                require(all(size > 0 or extent <= 0.001 for size, extent in zip(item["size"], vector(mesh.get_bounds().box_extent))), "Zero target size requires a planar source axis: " + region["name"] + "/" + item["label"])
            item["position"] = [value + offset for value, offset in zip(item["position"], region["offset"])]
            item["cull_distance"] = spec["central_cull_distance"] if region["name"] == "PineRidge" else min(item.get("cull_distance", spec["region_cull_distance"]), spec["region_cull_distance"])
            transform = environment.item_transform(mesh, item)
            minimum, maximum = environment.world_bounds(mesh, transform)
            if item["category"] != "floor":
                overlaps_combat = not (maximum[0] <= -790 or minimum[0] >= 190 or maximum[1] <= -190 or minimum[1] >= 990)
                padded_minimum = [minimum[axis] - (60 if axis < 2 else 20) for axis in range(3)]
                padded_maximum = [maximum[axis] + (60 if axis < 2 else 20) for axis in range(3)]
                if is_reserved(minimum, maximum, spec) or overlaps_combat or any(segment_intersects_box(camera_position, sample, padded_minimum, padded_maximum) for sample in combat_samples):
                    result["excluded"].append(item["label"])
                    continue
            overrides = environment.material_paths(item)
            slots = mesh.get_editor_property("static_materials")
            require(len(overrides) <= len(slots), "Material override exceeds source slots")
            materials = [require(unreal.load_asset(overrides[index]) if index < len(overrides) else slot.get_editor_property("material_interface"), "Source material is missing") for index, slot in enumerate(slots)]
            # Unsupported source materials use ordinary mesh actors; source usage flags are never modified.
            # 원본 재질이 ISM을 지원하지 않으면 일반 메시 Actor를 사용하며 원본 usage 설정은 변경하지 않습니다.
            item["instance"] = item.get("instance", True) and all(unreal.MaterialEditingLibrary.has_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES) for material in materials)
            result["meshes"].append(item)
        require(result["meshes"], "Region has no retained decoration")
        prepared.append(result)
    return prepared


def mark(actor, label, category):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [OWNER_TAG, unreal.Name(category)])
    actor.set_folder_path("UnifiedGameplay/" + category)
    actor.set_actor_enable_collision(False)
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)


def labels(region):
    groups, singles = environment.batching(region)
    return [("Unified_" + region["name"] + "_" + label.removeprefix("Environment_"), key, items) for label, key, items in groups], [("Unified_" + region["name"] + "_" + item["label"], item) for item in singles]


def place_regions(regions):
    subsystem = require(unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem), "SubobjectDataSubsystem is unavailable")
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for region in regions:
        groups, singles = labels(region)
        for label, key, items in groups:
            actor = require(ACTORS.spawn_actor_from_class(unreal.Actor, unreal.Vector(*region["offset"])), "Could not spawn instancing actor")
            handles = subsystem.k2_gather_subobject_data_for_instance(actor)
            require(handles and library.is_handle_valid(handles[0]), "Could not gather actor subobjects")
            if actor.get_editor_property("root_component"):
                actor.get_editor_property("root_component").set_mobility(unreal.ComponentMobility.STATIC)
            handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.InstancedStaticMeshComponent))
            require(library.is_handle_valid(handle), "Could not create a serialized instance component: " + str(reason))
            component = library.get_associated_object(library.get_data(handle))
            mesh = require(unreal.load_asset(key[0]), "Missing prepared source mesh")
            environment.configure_mesh(component, mesh, items[0])
            component.set_cull_distances(int(key[3] * 0.85), key[3])
            require(list(component.add_instances([environment.item_transform(mesh, item) for item in items], True, True, False)) == list(range(len(items))), "Saved instance indices differ")
            actor.modify()
            mark(actor, label, region["name"])
        for label, item in singles:
            mesh = unreal.load_asset(item["asset"])
            transform = environment.item_transform(mesh, item)
            actor = require(ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, transform.translation, transform.rotation.rotator()), "Could not spawn source decoration")
            actor.set_actor_scale3d(transform.scale3d)
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            environment.configure_mesh(component, mesh, item)
            component.set_editor_property("ld_max_draw_distance", float(item["cull_distance"]))
            mark(actor, label, region["name"])


def prototype_material(spec):
    editing = unreal.MaterialEditingLibrary
    exists = ASSETS.does_asset_exist(spec["material"])
    if exists:
        material = require(unreal.load_asset(spec["material"]), "Prototype material failed to load")
        require(isinstance(material, unreal.Material) and ASSETS.get_metadata_tag(material, OWNER_KEY) == OWNER_VALUE, "Prototype material belongs to another author")
    else:
        require(not VERIFY_ONLY, "Prototype material has not been authored")
        folder, name = spec["material"].rsplit("/", 1)
        material = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew()), "Could not create prototype material")
        tint = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -300, 0)
        tint.set_editor_property("parameter_name", "Tint")
        tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
        roughness = editing.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 180)
        roughness.set_editor_property("r", 0.85)
        require(editing.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR), "Could not connect prototype tint")
        require(editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS), "Could not connect prototype roughness")
        ASSETS.set_metadata_tag(material, OWNER_KEY, OWNER_VALUE)
        editing.recompile_material(material)
        require(ASSETS.save_loaded_asset(material, only_if_is_dirty=False), "Could not save prototype material")
    expressions = list(editing.get_material_expressions(material))
    require(len(expressions) == 2 and {str(name) for name in editing.get_vector_parameter_names(material)} == {"Tint"}, "Prototype material graph differs")
    require(material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_DEFAULT_LIT, "Prototype material must use the preserved scene lighting")
    require(isinstance(editing.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR), unreal.MaterialExpressionVectorParameter), "Prototype base color is disconnected")
    roughness = editing.get_material_property_input_node(material, unreal.MaterialProperty.MP_ROUGHNESS)
    require(isinstance(roughness, unreal.MaterialExpressionConstant) and abs(roughness.get_editor_property("r") - 0.85) < 0.0001, "Prototype roughness differs")
    return material


def stage_properties(stage, material):
    return {"stage_id": unreal.Name(stage["id"]), "stage_title": unreal.Text(stage["title"]), "required_tags": tag_container(stage["required"]), "excluded_tags": tag_container(stage["excluded"]), "priority": stage["priority"], "visual_style": stage["style"], "tint": unreal.LinearColor(*stage["tint"]), "prototype_material": material}


def place_stages(spec, material):
    stage_class = unreal.load_class(None, "/Script/ProjectA.EncounterPrototypeStage")
    for stage in spec["stages"]:
        actor = require(ACTORS.spawn_actor_from_class(stage_class, unreal.Vector(*stage["position"])), "Could not spawn encounter stage")
        for name, value in stage_properties(stage, material).items():
            actor.set_editor_property(name, value)
        actor.refresh_prototype()
        mark(actor, "Unified_Stage_" + stage["id"], "Stages")
    overview = spec["overview"]
    position, target = unreal.Vector(*overview["position"]), unreal.Vector(*overview["target"])
    actor = require(ACTORS.spawn_actor_from_class(unreal.CameraActor, position, unreal.MathLibrary.find_look_at_rotation(position, target)), "Could not create encounter overview camera")
    actor.get_editor_property("camera_component").set_editor_property("field_of_view", float(overview["fov"]))
    mark(actor, "Unified_EncounterOverview", "Overview")
    actor.set_editor_property("tags", [OWNER_TAG, OVERVIEW_TAG])


def verify(spec, regions, baseline, material):
    _, current = preserved_state()
    require(current == baseline, "Unified decoration changed the original core, camera, floor, navigation, GameMode or lighting")
    owned = [actor for actor in ACTORS.get_all_level_actors() if OWNER_TAG in actor.get_editor_property("tags")]
    by_label = {actor.get_actor_label(): actor for actor in owned}
    expected = {"Unified_Stage_" + stage["id"] for stage in spec["stages"]} | {"Unified_EncounterOverview"}
    region_reports = []
    for region in regions:
        groups, singles = labels(region)
        expected.update(label for label, key, items in groups)
        expected.update(label for label, item in singles)
        for label, key, items in groups:
            actor = require(by_label.get(label), "Missing unified mesh actor: " + label)
            components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
            require(len(components) == 1 and components[0].get_instance_count() == len(items), "Unified instance count differs")
            component = components[0]
            mesh = component.get_editor_property("static_mesh")
            require(component.get_editor_property("instance_start_cull_distance") == int(key[3] * 0.85) and component.get_editor_property("instance_end_cull_distance") == key[3], "Unified instance culling differs")
            environment.check_mesh_component(component, mesh, items[0])
            for index, item in enumerate(items):
                actual = component.get_instance_transform(index, True)
                if isinstance(actual, tuple):
                    actual = next((value for value in actual if isinstance(value, unreal.Transform)), None)
                require(isinstance(actual, unreal.Transform), "Could not read unified instance transform")
                environment.check_transform(actual, environment.item_transform(mesh, item), item["label"])
        for label, item in singles:
            actor = require(by_label.get(label), "Missing unified mesh actor: " + label)
            component = actor.get_component_by_class(unreal.StaticMeshComponent)
            mesh = component.get_editor_property("static_mesh")
            environment.check_mesh_component(component, mesh, item)
            environment.check_transform(actor.get_actor_transform(), environment.item_transform(mesh, item), item["label"])
            require(abs(component.get_editor_property("ld_max_draw_distance") - item["cull_distance"]) < 0.01, "Unified mesh culling differs")
        region_reports.append({"name": region["name"], "source_level": region["source_level"], "offset": region["offset"], "mesh_count": len(region["meshes"]), "ism_actors": len(groups), "single_actors": len(singles), "excluded_for_clearance": region["excluded"]})
    for stage in spec["stages"]:
        actor = require(by_label.get("Unified_Stage_" + stage["id"]), "Missing encounter stage")
        require(isinstance(actor, unreal.EncounterPrototypeStage), "Unexpected stage actor class")
        environment.check_transform(actor.get_actor_transform(), unreal.Transform(location=unreal.Vector(*stage["position"])), stage["id"])
        for name, value in stage_properties(stage, material).items():
            require(normalized(actor.get_editor_property(name)) == normalized(value), "Saved stage setting differs: " + stage["id"] + "." + name)
    overview = require(by_label.get("Unified_EncounterOverview"), "Missing overview camera")
    require(isinstance(overview, unreal.CameraActor) and OVERVIEW_TAG in overview.get_editor_property("tags"), "Invalid overview camera")
    position, target = unreal.Vector(*spec["overview"]["position"]), unreal.Vector(*spec["overview"]["target"])
    environment.check_transform(overview.get_actor_transform(), unreal.Transform(location=position, rotation=unreal.MathLibrary.find_look_at_rotation(position, target)), "overview")
    require(abs(overview.get_editor_property("camera_component").get_editor_property("field_of_view") - spec["overview"]["fov"]) < 0.01, "Overview field of view differs")
    require(len(owned) == len(expected) and set(by_label) == expected, "Missing, duplicate or unexpected owned actors")
    for actor in owned:
        require(not actor.get_actor_enable_collision(), "Owned decoration blocks gameplay collision")
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            require(component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION and not component.get_editor_property("can_ever_affect_navigation"), "Owned decoration affects collision or navigation")
    return {"regions": region_reports, "stage_count": len(spec["stages"]), "owned_actor_count": len(owned), "original_actor_count": len(baseline["actors"]), "core_preserved": True, "comparison_maps_preserved": 14}


def main():
    spec, sources = read_specs()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    unreal.log("UNIFIED_GAMEPLAY_PROTECTED_HASHES_START")
    before = protected_hashes(spec)
    require(unreal.EditorLoadingAndSavingUtils.load_map(spec["level"]), "Could not load Gameplay")
    world, baseline = preserved_state()
    baseline_path = OUTPUT / "PreservedLayout.json"
    if VERIFY_ONLY:
        require(baseline_path.exists(), "Authoring baseline is required for independent verification")
        require(baseline == json.loads(baseline_path.read_text(encoding="utf-8")), "Saved world differs from its protected authoring baseline")
    regions = prepare_regions(spec, sources)
    prior_material = digest(package_file(spec["material"], ".uasset")) if package_file(spec["material"], ".uasset").exists() else None
    target_before = digest(package_file(spec["level"], ".umap"))
    material = prototype_material(spec)
    if not VERIFY_ONLY:
        # Only this tool's tagged presentation actors are replaced; comparison worlds and gameplay actors are untouched.
        # 이 도구의 태그가 있는 연출 Actor만 교체하고 비교 월드와 게임플레이 Actor는 보존합니다.
        for actor in list(ACTORS.get_all_level_actors()):
            if OWNER_TAG in actor.get_editor_property("tags"):
                require(actor.get_actor_label().startswith("Unified_") and actor.get_class().get_path_name() in ["/Script/Engine.Actor", "/Script/Engine.StaticMeshActor", "/Script/Engine.CameraActor", "/Script/ProjectA.EncounterPrototypeStage"], "A nonvisual actor carries the ownership tag")
                require(ACTORS.destroy_actor(actor), "Could not rebuild owned visual decoration")
        place_regions(regions)
        place_stages(spec, material)
        report = verify(spec, regions, baseline, material)
        require(ASSETS.get_metadata_tag(world, OWNER_KEY) in ["", OWNER_VALUE], "Gameplay world carries conflicting authoring ownership")
        ASSETS.set_metadata_tag(world, OWNER_KEY, OWNER_VALUE)
        require(unreal.EditorLoadingAndSavingUtils.save_map(world, spec["level"]), "Could not save unified Gameplay")
        baseline_path.write_text(json.dumps(baseline, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    else:
        require(ASSETS.get_metadata_tag(world, OWNER_KEY) == OWNER_VALUE, "Unified world ownership is missing")
        report = verify(spec, regions, baseline, material)
        require(digest(package_file(spec["level"], ".umap")) == target_before, "Read-only verification changed Gameplay")
    if prior_material:
        require(digest(package_file(spec["material"], ".uasset")) == prior_material, "Existing prototype material was modified")
    after = protected_hashes(spec)
    (OUTPUT / ("ProtectedReload.json" if VERIFY_ONLY else "ProtectedAuthor.json")).write_text(json.dumps({"before": before, "after": after}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    require(after == before, "Original Content, configuration or existing saves changed")
    report.update(mode="verify" if VERIFY_ONLY else "author", level=spec["level"], protected_files=len(before), source_preserved=True, gameplay_test="not run", visual_performance_test="not run", original_lighting_preserved=True, source_lights_and_effects_copied=False)
    output = OUTPUT / ("Reload.json" if VERIFY_ONLY else "Author.json")
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("UNIFIED_GAMEPLAY_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
