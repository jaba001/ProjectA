import hashlib
import itertools
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

from ConfigureDungeonLevels import color, core_layout, mesh_placement, segment_intersects_box, vector
from ConfigureCombatDebugLevel import require


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_MAP = "/Game/User_JeHoon/LEVEL/Gameplay"
MODE = "/Game/User_JeHoon/Blueprint/Game/BP_CombatDebugGameMode"
OUTPUT_ROOT = "/Game/User_JeHoon/LEVEL/Environment/"
SKY_CUBEMAP = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap"
FLOOR_MESH = "/Engine/BasicShapes/Plane"
MATERIAL_ROOT = "/Game/User_JeHoon/Materials/Environment/"
MATERIAL_ALLOWLIST = set()
FLOOR_MINIMUM = [-5300.0, -4100.0]
FLOOR_MAXIMUM = [4700.0, 4900.0]
DECORATION_TAG = unreal.Name("ProjectAEnvironmentDecoration")
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ASSETS = unreal.EditorAssetLibrary
COMMAND_LINE = unreal.SystemLibrary.get_command_line()
VERIFY_ONLY = "-EnvironmentVerifyOnly" in COMMAND_LINE
REBUILD = "-EnvironmentRebuild" in COMMAND_LINE
LIGHT_LABELS = {"Environment_Sun", "Environment_Sky", "Environment_Fog", "Environment_PostProcess", "Environment_Atmosphere"}


def level_path(spec):
    return OUTPUT_ROOT + spec["name"]


def original_path(path, spec):
    require(isinstance(path, str) and "." not in path and any(path.startswith(root.rstrip("/") + "/") for root in spec["source_roots"]), "Asset must reference a declared original root: " + str(path))
    require(not path.startswith("/Game/User_JeHoon/"), "Do not copy source environment assets into User_JeHoon")
    return path


def material_path(path, spec):
    return path if path in MATERIAL_ALLOWLIST else original_path(path, spec)


def preserved_layout(package):
    world, layout = core_layout(package)
    camera = next(actor for actor in ACTORS.get_all_level_actors() if actor.get_actor_label() == "GameplayCamera")
    layout["camera_aspect_ratio"] = camera.get_editor_property("camera_component").get_editor_property("aspect_ratio")
    return world, layout


def aperture(lighting):
    value = math.sqrt(2.0 ** lighting.get("exposure_ev100", 14.0) / 125.0)
    require(1.0 <= value <= 32.0, "EV100 exceeds the supported physical aperture range")
    return value


def read_specs():
    require(not (VERIFY_ONLY and REBUILD), "Verify and rebuild are mutually exclusive")
    document = json.loads((SCRIPT_DIRECTORY / "EnvironmentLevelSpecs.json").read_text(encoding="utf-8"))
    definitions = document.get("surfaces", []) + document.get("instances", [])
    paths = [definition["asset"] for definition in definitions]
    require(len(paths) == len(set(paths)) and all(path.startswith(MATERIAL_ROOT) and "." not in path for path in paths), "Derived materials must have unique declared Environment paths")
    MATERIAL_ALLOWLIST.clear()
    MATERIAL_ALLOWLIST.update(paths)
    specs = document["levels"]
    require(specs and len({spec["name"] for spec in specs}) == len(specs), "Environment names must be unique")
    for spec in specs:
        require(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", spec["name"]) and spec.get("title"), "Invalid environment identity")
        require(spec.get("source_roots") and all(root.startswith(("/Game/", "/Engine/")) and ".." not in root and not root.startswith("/Game/User_JeHoon") for root in spec["source_roots"]), "Invalid original asset roots")
        require(spec.get("meshes") and len({item["label"] for item in spec["meshes"]}) == len(spec["meshes"]), "Mesh labels must be unique")
        for item in spec["meshes"]:
            original_path(item["asset"], spec)
            require(item["category"] in ["floor", "architecture", "prop"], "Unknown mesh category")
            if item["category"] == "floor":
                require(item["asset"] == FLOOR_MESH and abs(item.get("yaw", 0.0) % 360.0) < 0.001, "Base ground must use the unrotated Engine Plane")
            require(len(item["position"]) == 3 and all(math.isfinite(value) for value in item["position"]), "Invalid mesh position")
            require(item.get("align", "bottom") in ["bottom", "top"] and math.isfinite(item.get("yaw", 0.0)), "Invalid mesh alignment")
            for field in ["size", "scale"]:
                if field in item:
                    require(len(item[field]) == 3 and all(math.isfinite(value) and value > 0.0 for value in item[field]), "Invalid mesh " + field)
            require(isinstance(item.get("instance", True), bool) and math.isfinite(item.get("cull_distance", 12000)) and item.get("cull_distance", 12000) > 0, "Invalid instancing or cull distance")
            require(isinstance(casts_shadow(item), bool), "Invalid mesh shadow setting")
            require(isinstance(item.get("affect_distance_field_lighting", True), bool), "Invalid mesh distance field lighting setting")
            for material in material_paths(item):
                material_path(material, spec)
        lighting = spec["lighting"]
        for field in ["sun_color", "sky_color", "fog_color"]:
            require(len(lighting[field]) == 3 and all(0.0 <= channel <= 255.0 for channel in lighting[field]), "Invalid lighting color")
        require(len(lighting["sun_rotation"]) == 3 and all(math.isfinite(value) for value in lighting["sun_rotation"]), "Invalid sun rotation")
        require(all(math.isfinite(lighting[field]) for field in ["sun_lux", "sky_intensity", "exposure_bias", "fog_density"]) and lighting["sun_lux"] > 0.0 and lighting["sky_intensity"] > 0.0 and 0.0 <= lighting["fog_density"] <= 0.05, "Invalid outdoor lighting")
        require(math.isfinite(lighting.get("exposure_ev100", 14.0)), "Invalid EV100")
        aperture(lighting)
    selection = re.search(r'(?:^|\s)-EnvironmentNames=(?:"([^"]*)"|([^\s]+))', COMMAND_LINE)
    if selection:
        names = [name.strip() for name in (selection.group(1) or selection.group(2) or "").split(",") if name.strip()]
        require(names and len(names) == len(set(names)) and set(names).issubset({spec["name"] for spec in specs}), "Unknown or duplicate selected environment")
        specs = [spec for spec in specs if spec["name"] in names]
    return specs


def digest(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def protected_hashes(specs):
    directories = {ROOT / "Content/User_JeHoon"}
    for root in {root for spec in specs for root in spec["source_roots"]}:
        directory = ROOT / "Content" / root.removeprefix("/Game/") if root.startswith("/Game/") else Path(unreal.Paths.engine_content_dir()).resolve() / root.removeprefix("/Engine/")
        require(directory.is_dir(), "Import the original source pack first: " + root)
        directories.add(directory)
    directories.update(ROOT / "Content" / category / "User_JeHoon" for category in ["__ExternalActors__", "__ExternalObjects__"])
    output_files = {ROOT / "Content" / (level_path(spec).removeprefix("/Game/") + suffix) for spec in specs for suffix in [".umap", "_BuiltData.uasset"]}
    output_external = [ROOT / "Content" / category / level_path(spec).removeprefix("/Game/") for spec in specs for category in ["__ExternalActors__", "__ExternalObjects__"]]
    paths = {path for directory in directories if directory.exists() for path in directory.rglob("*") if path.is_file() and path.suffix.lower() in [".uasset", ".umap", ".uexp", ".ubulk", ".uptnl"]}
    return {str(path): digest(path) for path in sorted(paths) if path not in output_files and not any(path.is_relative_to(directory) for directory in output_external)}


def material_paths(item):
    require(not (item.get("material") and "materials" in item), "Use material or materials, not both")
    return item.get("materials", [item["material"]] if item.get("material") else [])


def casts_shadow(item):
    return item.get("cast_shadow", item["category"] != "floor")


def batching(spec):
    groups, singles = {}, []
    for item in spec["meshes"]:
        if item.get("instance", True):
            key = (item["asset"], tuple(material_paths(item)), casts_shadow(item), int(item.get("cull_distance", 12000)), item.get("affect_distance_field_lighting", True))
            groups.setdefault(key, []).append(item)
        else:
            singles.append(item)
    return [("Environment_ISM_" + str(index + 1).zfill(3), key, items) for index, (key, items) in enumerate(groups.items())], singles


def quality_budget(spec):
    groups, singles = batching(spec)
    # Reject excessive full-detail geometry and shadow work before writing any map; these caps do not replace measured FPS.
    # 맵을 작성하기 전에 상세 지오메트리와 그림자 비용 초과를 거부하며 이 상한은 측정 FPS를 대체하지 않습니다.
    require(len(spec["meshes"]) <= 2500 and len(groups) <= 128 and len(singles) <= 128, "Environment exceeds the comparison-map authoring budget")
    labels = LIGHT_LABELS | {label for label, key, items in groups} | {"Environment_" + item["label"] for item in singles}
    require(len(labels) == len(LIGHT_LABELS) + len(groups) + len(singles), "Mesh labels conflict with authored environment actors")
    sources = {}
    limits = {"max_lod0_triangles": 5000000, "max_shadow_lod0_triangles": 2000000, "max_shadow_items": 256, "max_material_sections": 256, "max_material_slots_per_mesh": 8}
    require(set(spec.get("budget", {})).issubset(limits), "Unknown quality budget limit")
    limits.update(spec.get("budget", {}))
    require(all(isinstance(value, int) and not isinstance(value, bool) and value > 0 for value in limits.values()), "Quality budget limits must be positive integers")
    triangle_total, lod0_total, shadow_triangles = 0, 0, 0
    floors = []
    checked_instanced_materials = set()
    for item in spec["meshes"]:
        asset = item["asset"]
        if asset not in sources:
            mesh = require(unreal.load_asset(asset), "Missing original mesh: " + asset)
            require(isinstance(mesh, unreal.StaticMesh), "Only original StaticMesh assets are supported")
            sources[asset] = {"lod_count": mesh.get_num_lods(), "lod0_triangles": mesh.get_num_triangles(0), "nanite_triangles": mesh.get_num_nanite_triangles(), "lod0_sections": mesh.get_num_sections(0), "material_slots": len(mesh.get_editor_property("static_materials"))}
            require(sources[asset]["lod_count"] > 0 and sources[asset]["lod0_triangles"] > 0 and sources[asset]["lod0_sections"] > 0, "Source mesh geometry is not ready: " + asset)
            require(sources[asset]["material_slots"] <= limits["max_material_slots_per_mesh"], "Source mesh exceeds the material slot budget: " + asset)
        triangle_total += max(sources[asset]["lod0_triangles"], sources[asset]["nanite_triangles"])
        lod0_total += sources[asset]["lod0_triangles"]
        shadow_triangles += sources[asset]["lod0_triangles"] if casts_shadow(item) else 0
        if item.get("instance", True):
            mesh = unreal.load_asset(asset)
            overrides = material_paths(item)
            for index, slot in enumerate(mesh.get_editor_property("static_materials")):
                material = unreal.load_asset(overrides[index]) if index < len(overrides) else slot.get_editor_property("material_interface")
                require(material, "Instanced mesh material is missing: " + asset)
                if material.get_path_name() not in checked_instanced_materials:
                    require(unreal.MaterialEditingLibrary.has_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES), "Instanced mesh requires an authored material usage override: " + material.get_path_name())
                    checked_instanced_materials.add(material.get_path_name())
        if item["category"] == "floor":
            mesh = unreal.load_asset(asset)
            actual = item_transform(mesh, item)
            minimum, maximum = world_bounds(mesh, actual)
            check_floor_geometry(mesh, minimum, maximum)
            floors.append((mesh, actual, minimum, maximum))
    shadow_items = sum(casts_shadow(item) for item in spec["meshes"])
    sections = sum(sources[key[0]]["lod0_sections"] for label, key, items in groups) + sum(sources[item["asset"]]["lod0_sections"] for item in singles)
    require(lod0_total <= limits["max_lod0_triangles"], "LOD0 triangle budget exceeded")
    require(shadow_triangles <= limits["max_shadow_lod0_triangles"] and shadow_items <= limits["max_shadow_items"], "Shadow geometry budget exceeded")
    require(sections <= limits["max_material_sections"], "LOD0 material section budget exceeded")
    verify_floor_coverage(floors)
    return {"mesh_items": len(spec["meshes"]), "ism_groups": len(groups), "individual_mesh_actors": len(singles), "unique_meshes": len(sources), "shadow_mesh_items": shadow_items, "lod0_triangles": lod0_total, "shadow_lod0_triangles": shadow_triangles, "lod0_section_batches": sections, "estimated_full_detail_triangles": triangle_total, "limits": limits, "sources": sources, "dynamic_shadow_lights": 1, "realtime_sky_capture": False, "volumetric_fog": False, "performance_test": "not run"}


def mark(actor, label, category):
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [DECORATION_TAG, unreal.Name("Environment" + category.capitalize())])
    actor.set_folder_path("Environment/" + category.capitalize())
    actor.set_actor_enable_collision(False)
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
    return actor


def configure_mesh(component, mesh, item):
    component.modify()
    component.set_mobility(unreal.ComponentMobility.STATIC)
    require(component.set_static_mesh(mesh), "Could not assign original mesh")
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_cast_shadow(casts_shadow(item))
    component.set_editor_property("affect_distance_field_lighting", item.get("affect_distance_field_lighting", True))
    require(len(material_paths(item)) <= len(mesh.get_editor_property("static_materials")), "Material override exceeds original mesh slots")
    for index, path in enumerate(material_paths(item)):
        material = require(unreal.load_asset(path), "Missing original material: " + path)
        require(isinstance(material, unreal.MaterialInterface), "Expected an original material or instance")
        component.set_material(index, material)


def item_transform(mesh, item):
    location, scale = mesh_placement(mesh, item)
    return unreal.Transform(location=location, rotation=unreal.Rotator(yaw=item.get("yaw", 0.0)), scale=unreal.Vector(*scale))


def place_meshes(spec):
    groups, singles = batching(spec)
    subsystem = require(unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem), "SubobjectDataSubsystem is unavailable")
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for label, key, items in groups:
        actor = require(ACTORS.spawn_actor_from_class(unreal.Actor, unreal.Vector()), "Could not create an instancing actor")
        handles = subsystem.k2_gather_subobject_data_for_instance(actor)
        require(handles and library.is_handle_valid(handles[0]), "Could not gather actor subobjects")
        if actor.get_editor_property("root_component"):
            actor.get_editor_property("root_component").set_mobility(unreal.ComponentMobility.STATIC)
        # Use the editor subsystem so instance components are registered in the actor's serialized component array.
        # 인스턴스 컴포넌트가 Actor의 직렬화 배열에 등록되도록 에디터 서브시스템을 사용합니다.
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.InstancedStaticMeshComponent))
        require(library.is_handle_valid(handle), "Could not create an instanced mesh component: " + str(reason))
        component = library.get_associated_object(library.get_data(handle))
        require(isinstance(component, unreal.InstancedStaticMeshComponent), "Expected a saved instance component")
        mesh = require(unreal.load_asset(key[0]), "Missing original mesh")
        configure_mesh(component, mesh, items[0])
        component.set_cull_distances(int(key[3] * 0.85), key[3])
        indices = component.add_instances([item_transform(mesh, item) for item in items], True, True, False)
        require(list(indices) == list(range(len(items))), "Instanced mesh indices differ from their specification")
        actor.modify()
        mark(actor, label, "instances")
    for item in singles:
        mesh = require(unreal.load_asset(item["asset"]), "Missing original mesh")
        location, scale = mesh_placement(mesh, item)
        actor = require(ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(yaw=item.get("yaw", 0.0))), "Could not create a mesh actor")
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        configure_mesh(component, mesh, item)
        component.set_editor_property("ld_max_draw_distance", float(item.get("cull_distance", 12000)))
        actor.set_actor_scale3d(unreal.Vector(*scale))
        mark(actor, "Environment_" + item["label"], item["category"])


def configure_atmosphere(spec):
    for actor in list(ACTORS.get_all_level_actors()):
        if actor.get_class().get_path_name() in ["/Script/Engine.DirectionalLight", "/Script/Engine.SkyLight", "/Script/Engine.SkyAtmosphere", "/Script/Engine.VolumetricCloud", "/Script/Engine.ExponentialHeightFog", "/Script/Engine.PostProcessVolume"] or actor.get_actor_label() == "SM_SkySphere":
            require(ACTORS.destroy_actor(actor), "Could not replace copied atmosphere")
        elif actor.get_actor_label() == "Floor":
            actor.get_component_by_class(unreal.StaticMeshComponent).set_visibility(False)
    lighting = spec["lighting"]
    pitch, yaw, roll = lighting["sun_rotation"]
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(-300.0, 400.0, 2500.0), unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
    component = require(sun, "Could not create the sun").get_component_by_class(unreal.DirectionalLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(lighting["sun_lux"])
    component.set_light_color(color(lighting["sun_color"]), True)
    component.set_cast_shadows(True)
    component.set_atmosphere_sun_light(True)
    component.set_editor_property("can_ever_affect_navigation", False)
    mark(sun, "Environment_Sun", "lighting")
    sky = ACTORS.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(-300.0, 400.0, 2000.0))
    component = require(sky, "Could not create sky lighting").get_component_by_class(unreal.SkyLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    component.set_editor_property("real_time_capture", False)
    component.set_cubemap(require(unreal.load_asset(SKY_CUBEMAP), "Engine daylight cubemap is missing"))
    component.set_intensity(lighting["sky_intensity"])
    component.set_light_color(color(lighting["sky_color"]))
    component.set_cast_shadows(False)
    component.set_editor_property("can_ever_affect_navigation", False)
    mark(sky, "Environment_Sky", "lighting")
    atmosphere = require(ACTORS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector()), "Could not create the daytime sky")
    mark(atmosphere, "Environment_Atmosphere", "lighting")
    fog = require(ACTORS.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(-300.0, 400.0, -100.0)), "Could not create height fog")
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    component.set_editor_property("fog_density", lighting["fog_density"])
    component.set_editor_property("fog_height_falloff", 0.3)
    component.set_editor_property("fog_max_opacity", 0.25)
    component.set_editor_property("start_distance", 1400.0)
    component.set_editor_property("fog_inscattering_luminance", color(lighting["fog_color"]))
    component.set_editor_property("enable_volumetric_fog", False)
    mark(fog, "Environment_Fog", "lighting")
    post = require(ACTORS.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(-300.0, 400.0, 0.0)), "Could not create fixed exposure")
    post.set_editor_property("unbound", True)
    post.set_editor_property("priority", 10.0)
    settings = post.get_editor_property("settings")
    for name, value in {"override_auto_exposure_method": True, "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL, "override_auto_exposure_bias": True, "auto_exposure_bias": lighting["exposure_bias"], "override_auto_exposure_apply_physical_camera_exposure": True, "auto_exposure_apply_physical_camera_exposure": True, "override_camera_iso": True, "camera_iso": 100.0, "override_camera_shutter_speed": True, "camera_shutter_speed": 125.0, "override_depth_of_field_fstop": True, "depth_of_field_fstop": aperture(lighting), "override_depth_of_field_focal_distance": True, "depth_of_field_focal_distance": 0.0, "override_bloom_intensity": True, "bloom_intensity": 0.15, "override_motion_blur_amount": True, "motion_blur_amount": 0.0}.items():
        settings.set_editor_property(name, value)
    post.set_editor_property("settings", settings)
    mark(post, "Environment_PostProcess", "lighting")


def world_bounds(mesh, transform):
    bounds = mesh.get_bounds()
    origin, extent = vector(bounds.origin), vector(bounds.box_extent)
    corners = [vector(transform.transform_location(unreal.Vector(*(origin[axis] + sign[axis] * extent[axis] for axis in range(3))))) for sign in itertools.product([-1.0, 1.0], repeat=3)]
    return [min(corner[axis] for corner in corners) for axis in range(3)], [max(corner[axis] for corner in corners) for axis in range(3)]


def check_transform(actual, expected, label):
    rotation, expected_rotation = actual.rotation.rotator(), expected.rotation.rotator()
    require(all(abs(a - b) < 0.05 for a, b in zip(vector(actual.translation), vector(expected.translation))), "Saved mesh position differs: " + label)
    require(all(abs(a - b) < 0.0001 for a, b in zip(vector(actual.scale3d), vector(expected.scale3d))), "Saved mesh scale differs: " + label)
    require(all(abs((getattr(rotation, axis) - getattr(expected_rotation, axis) + 180.0) % 360.0 - 180.0) < 0.01 for axis in ["pitch", "yaw", "roll"]), "Saved mesh rotation differs: " + label)


def check_mesh_component(component, mesh, item):
    require(component.get_editor_property("mobility") == unreal.ComponentMobility.STATIC and component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION and not component.get_editor_property("can_ever_affect_navigation"), "Mesh changes mobility, collision or navigation")
    require(mesh.get_path_name().split(".")[0] == item["asset"] and component.get_editor_property("cast_shadow") == casts_shadow(item), "Saved mesh or shadow setting differs")
    require(component.get_editor_property("affect_distance_field_lighting") == item.get("affect_distance_field_lighting", True), "Saved mesh distance field lighting differs")
    for index, path in enumerate(material_paths(item)):
        require(component.get_material(index).get_path_name().split(".")[0] == path, "Saved original material differs")


def frame_ground_samples(camera):
    rotation = camera.get_actor_rotation()
    pitch, yaw = math.radians(rotation.pitch), math.radians(rotation.yaw)
    location = vector(camera.get_actor_location())
    forward = [math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch)]
    right, up = [-math.sin(yaw), math.cos(yaw), 0.0], [-math.sin(pitch) * math.cos(yaw), -math.sin(pitch) * math.sin(yaw), math.cos(pitch)]
    component = camera.get_editor_property("camera_component")
    require(component.get_editor_property("aspect_ratio") > 0.0, "Camera aspect ratio is invalid")
    # MaintainYFOV derives the vertical angle from the authored horizontal FOV and its base aspect ratio.
    # MaintainYFOV의 수직 각도는 작성된 수평 FOV와 기준 화면 비율에서 계산합니다.
    tangent = math.tan(math.radians(component.get_editor_property("field_of_view")) * 0.5) / component.get_editor_property("aspect_ratio")
    samples = []
    for aspect in [4.0 / 3.0, 16.0 / 9.0, 21.0 / 9.0]:
        for horizontal, vertical in itertools.product([-1.0, 0.0, 1.0], repeat=2):
            ray = [forward[axis] + tangent * (aspect * horizontal * right[axis] + vertical * up[axis]) for axis in range(3)]
            require(ray[2] < -0.0001, "Preserved combat camera does not face the environment floor")
            distance = (1.0 - location[2]) / ray[2]
            samples.append([location[axis] + ray[axis] * distance for axis in range(3)])
    return samples


def check_color(actual, expected):
    return all(abs(getattr(actual, channel) - value / 255.0) < 0.01 for channel, value in zip(["r", "g", "b"], expected))


def floor_covers(point, floor):
    mesh, transform, minimum, maximum = floor
    if not (minimum[0] - 0.1 <= point[0] <= maximum[0] + 0.1 and minimum[1] - 0.1 <= point[1] <= maximum[1] + 0.1):
        return False
    local = vector(transform.inverse_transform_location(unreal.Vector(*point)))
    bounds = mesh.get_bounds()
    return all(abs(local[axis] - vector(bounds.origin)[axis]) <= vector(bounds.box_extent)[axis] + 0.01 for axis in range(2))


def check_floor_geometry(mesh, minimum, maximum):
    require(mesh.get_path_name().split(".")[0] == FLOOR_MESH and 0.99 <= minimum[2] <= maximum[2] <= 1.01, "Ground must be the verified flat Plane at Z=1")


def verify_floor_coverage(floors):
    require(floors, "A complete Engine Plane ground is required")
    minimum = [min(floor[2][axis] for floor in floors) for axis in range(2)]
    maximum = [max(floor[3][axis] for floor in floors) for axis in range(2)]
    require(all(abs(a - b) <= 0.1 for a, b in zip(minimum + maximum, FLOOR_MINIMUM + FLOOR_MAXIMUM)), "Ground footprint must be 10000 x 9000 centered at (-300,400)")
    # Sweep every tile edge and union Y intervals so disconnected tiles cannot hide holes inside a combined bounding box.
    # 타일 경계마다 Y 구간을 합쳐 분리된 타일 사이 빈 곳이 합친 경계 상자로 가려지지 않도록 검사합니다.
    edges = sorted({FLOOR_MINIMUM[0], FLOOR_MAXIMUM[0]} | {floor[index][0] for floor in floors for index in [2, 3]})
    for left, right in zip(edges, edges[1:]):
        if right - left <= 0.1:
            continue
        middle = (left + right) * 0.5
        intervals = sorted((floor[2][1], floor[3][1]) for floor in floors if floor[2][0] <= middle <= floor[3][0])
        end = FLOOR_MINIMUM[1]
        for start, finish in intervals:
            require(start <= end + 0.1, "Ground Plane tiles leave a hole")
            end = max(end, finish)
        require(end >= FLOOR_MAXIMUM[1] - 0.1, "Ground Plane tiles leave an uncovered strip")


def verify_lighting(spec, by_label):
    lighting = spec["lighting"]
    sun = by_label["Environment_Sun"]
    component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    rotation = sun.get_actor_rotation()
    require(all(abs((getattr(rotation, axis) - value + 180.0) % 360.0 - 180.0) < 0.01 for axis, value in zip(["pitch", "yaw", "roll"], lighting["sun_rotation"])), "Saved sun rotation differs")
    require(abs(component.get_editor_property("intensity") - lighting["sun_lux"]) < 0.01 and component.get_editor_property("cast_shadows") and component.get_editor_property("mobility") == unreal.ComponentMobility.MOVABLE and check_color(component.get_light_color(), lighting["sun_color"]), "Saved sun differs")
    sky = by_label["Environment_Sky"].get_component_by_class(unreal.SkyLightComponent)
    require(sky.get_editor_property("source_type") == unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP and sky.get_editor_property("cubemap").get_path_name().split(".")[0] == SKY_CUBEMAP and not sky.get_editor_property("real_time_capture"), "Saved sky source differs")
    require(abs(sky.get_editor_property("intensity") - lighting["sky_intensity"]) < 0.01 and not sky.get_editor_property("cast_shadows") and check_color(sky.get_light_color(), lighting["sky_color"]), "Saved sky lighting differs")
    fog = by_label["Environment_Fog"].get_component_by_class(unreal.ExponentialHeightFogComponent)
    require(abs(fog.get_editor_property("fog_density") - lighting["fog_density"]) < 0.0001 and not fog.get_editor_property("enable_volumetric_fog") and check_color(fog.get_editor_property("fog_inscattering_luminance"), lighting["fog_color"]), "Saved fog differs")
    post = by_label["Environment_PostProcess"]
    settings = post.get_editor_property("settings")
    require(post.get_editor_property("unbound") and settings.get_editor_property("auto_exposure_method") == unreal.AutoExposureMethod.AEM_MANUAL and settings.get_editor_property("auto_exposure_apply_physical_camera_exposure"), "Saved physical manual exposure differs")
    for name, value in {"auto_exposure_bias": lighting["exposure_bias"], "camera_iso": 100.0, "camera_shutter_speed": 125.0, "depth_of_field_fstop": aperture(lighting), "depth_of_field_focal_distance": 0.0}.items():
        require(settings.get_editor_property("override_" + name) and abs(settings.get_editor_property(name) - value) < 0.0001, "Saved exposure parameter differs: " + name)
    require(settings.get_editor_property("override_auto_exposure_method") and settings.get_editor_property("override_auto_exposure_apply_physical_camera_exposure"), "Saved exposure override is disabled")
    actual_ev100 = math.log2(settings.get_editor_property("depth_of_field_fstop") ** 2 * settings.get_editor_property("camera_shutter_speed") * 100.0 / settings.get_editor_property("camera_iso"))
    require(abs(actual_ev100 - lighting.get("exposure_ev100", 14.0)) < 0.0001, "Saved physical camera EV100 differs")


def verify_environment(spec, expected_layout, budget):
    world, layout = preserved_layout(level_path(spec))
    require(layout == expected_layout, "Environment changed the preserved combat layout")
    actors = ACTORS.get_all_level_actors()
    decoration = [actor for actor in actors if DECORATION_TAG in actor.get_editor_property("tags")]
    groups, singles = batching(spec)
    expected_labels = LIGHT_LABELS | {label for label, key, items in groups} | {"Environment_" + item["label"] for item in singles}
    require({actor.get_actor_label() for actor in decoration} == expected_labels and len(decoration) == len(expected_labels), "Missing or duplicate environment decoration")
    require(sum(isinstance(actor, unreal.DirectionalLight) for actor in actors) == 1 and sum(isinstance(actor, unreal.SkyLight) for actor in actors) == 1 and not any(isinstance(actor, (unreal.PointLight, unreal.SpotLight, unreal.RectLight, unreal.VolumetricCloud, unreal.NiagaraActor, unreal.Emitter)) for actor in actors), "Unexpected dynamic lights or ambient effects")
    require(not next(actor for actor in actors if actor.get_actor_label() == "Floor").get_component_by_class(unreal.StaticMeshComponent).get_editor_property("visible"), "Template floor obscures the visual ground")
    for actor in decoration:
        require(not actor.get_actor_enable_collision(), "Environment actor enables collision")
        for component in actor.get_components_by_class(unreal.PrimitiveComponent):
            require(component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION and not component.get_editor_property("can_ever_affect_navigation"), "Environment decoration blocks input or navigation")
    by_label = {actor.get_actor_label(): actor for actor in decoration}
    camera = next(actor for actor in actors if actor.get_actor_label() == "GameplayCamera")
    camera_position = vector(camera.get_actor_location())
    samples = [[-row * 200.0, column * 200.0 + (200.0 if column >= 2 else 0.0), height] for row in range(4) for column in range(4) for height in [5.0, 100.0, 200.0]]
    placed, floors = [], []
    for label, key, items in groups:
        components = by_label[label].get_components_by_class(unreal.InstancedStaticMeshComponent)
        require(len(components) == 1 and components[0].get_instance_count() == len(items), "Saved instanced mesh count differs")
        component = components[0]
        mesh = component.get_editor_property("static_mesh")
        require(component.get_editor_property("instance_start_cull_distance") == int(key[3] * 0.85) and component.get_editor_property("instance_end_cull_distance") == key[3], "Saved instance culling differs")
        for index, item in enumerate(items):
            actual = component.get_instance_transform(index, True)
            if isinstance(actual, tuple):
                actual = next((value for value in actual if isinstance(value, unreal.Transform)), None)
            require(isinstance(actual, unreal.Transform), "Could not read the saved instance transform")
            placed.append((item, component, mesh, actual, label, index))
    for item in singles:
        actor = by_label["Environment_" + item["label"]]
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        require(abs(component.get_editor_property("ld_max_draw_distance") - item.get("cull_distance", 12000)) < 0.01, "Saved individual mesh culling differs")
        placed.append((item, component, component.get_editor_property("static_mesh"), actor.get_actor_transform(), actor.get_actor_label(), None))
    meshes = []
    for item, component, mesh, actual, label, index in placed:
        check_mesh_component(component, mesh, item)
        check_transform(actual, item_transform(mesh, item), item["label"])
        minimum, maximum = world_bounds(mesh, actual)
        if item["category"] == "floor":
            check_floor_geometry(mesh, minimum, maximum)
            floors.append((mesh, actual, minimum, maximum))
        else:
            require(maximum[0] <= -790.0 or minimum[0] >= 190.0 or maximum[1] <= -190.0 or minimum[1] >= 990.0, "Environment enters the combat clearance: " + item["label"])
            padded_minimum = [minimum[axis] - (60.0 if axis < 2 else 20.0) for axis in range(3)]
            padded_maximum = [maximum[axis] + (60.0 if axis < 2 else 20.0) for axis in range(3)]
            require(not any(segment_intersects_box(camera_position, sample, padded_minimum, padded_maximum) for sample in samples), "Environment obscures the combat camera or character margin: " + item["label"])
        meshes.append({"label": item["label"], "actor": label, "instance_index": index, "mesh": mesh.get_path_name(), "transform": actual.export_text(), "bounds_min": minimum, "bounds_max": maximum, "collision": "NoCollision", "navigation": False, "cast_shadow": casts_shadow(item)})
    frame_samples = frame_ground_samples(camera)
    verify_floor_coverage(floors)
    for point in frame_samples + [[sample[0], sample[1], 1.0] for sample in samples[::3]]:
        require(any(floor_covers(point, floor) for floor in floors), "Visual ground does not cover a combat or viewport sample")
    verify_lighting(spec, by_label)
    return {"level": level_path(spec), "title": spec["title"], "source_roots": spec["source_roots"], "layout": layout, "actor_count": len(decoration), "mesh_count": len(meshes), "camera_samples": len(samples), "occlusion_bounds_padding": [60.0, 60.0, 20.0], "ground_frame_samples": len(frame_samples), "ground_footprint": [10000.0, 9000.0], "ground_center": [-300.0, 400.0], "quality_budget": budget, "lighting": dict(spec["lighting"], exposure_ev100=spec["lighting"].get("exposure_ev100", 14.0)), "meshes": meshes, "gameplay_test": "not run", "visual_performance_test": "not run"}


def main():
    specs = read_specs()
    for spec in specs:
        exists = ASSETS.does_asset_exist(level_path(spec))
        require(exists if VERIFY_ONLY else REBUILD or not exists, "Inspect an existing environment or explicitly use -EnvironmentRebuild")
    unreal.log("ENVIRONMENT_PROTECTED_HASHES_START")
    before = protected_hashes(specs)
    _, source_layout = preserved_layout(SOURCE_MAP)
    mode = require(unreal.load_asset(MODE), "Existing standalone combat mode is missing")
    require(isinstance(mode, unreal.Blueprint), "Expected the existing combat Blueprint")
    mode_class = mode.generated_class()
    defaults = unreal.get_default_object(mode_class)
    require(isinstance(defaults, unreal.CombatDebugGameMode) and defaults.get_editor_property("party_definition") and defaults.get_editor_property("enemy_definition") and defaults.get_editor_property("player_controller_class") == unreal.load_class(None, "/Script/ProjectA.CombatDebugPlayerController"), "Standalone combat defaults differ")
    expected_layout = dict(source_layout, game_mode=mode_class.get_path_name())
    reports = []
    for spec in specs:
        unreal.log("ENVIRONMENT_AUTHORING " + level_path(spec))
        budget = quality_budget(spec)
        if not VERIFY_ONLY:
            if not ASSETS.does_asset_exist(level_path(spec)):
                require(ASSETS.duplicate_asset(SOURCE_MAP, level_path(spec)), "Could not duplicate the combat layout through Unreal")
            world, layout = preserved_layout(level_path(spec))
            require(dict(layout, game_mode=source_layout["game_mode"]) == source_layout and layout["game_mode"] in [source_layout["game_mode"], expected_layout["game_mode"]], "Existing environment has a different combat layout")
            world.get_world_settings().set_editor_property("default_game_mode", mode_class)
            for actor in list(ACTORS.get_all_level_actors()):
                if DECORATION_TAG in actor.get_editor_property("tags"):
                    require(ACTORS.destroy_actor(actor), "Could not rebuild authored environment decoration")
            configure_atmosphere(spec)
            place_meshes(spec)
            require(unreal.EditorLoadingAndSavingUtils.save_map(world, level_path(spec)), "Could not save the authored environment")
        reports.append(verify_environment(spec, expected_layout, budget))
    require(protected_hashes(specs) == before, "Existing maps, project assets or original source packs changed")
    output = ROOT / "Saved/Automation/Environments" / ("Reload.json" if VERIFY_ONLY else "Configuration.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"mode": "verify" if VERIFY_ONLY else "configure", "levels": reports, "protected_asset_files": len(before), "source_preserved": True, "gameplay_test": "not run"}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("ENVIRONMENT_AUTHORING_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
