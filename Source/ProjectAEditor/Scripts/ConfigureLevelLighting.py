import hashlib
import json
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import unreal


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))

import ConfigureDungeonLevels as dungeon
import ConfigureEnvironmentLevels as environment


ROOT = Path(unreal.Paths.project_dir()).resolve()
DIRECTORY = ROOT / "Saved/Automation/Lighting"
VERIFY_ONLY = "-LightingVerifyOnly" in unreal.SystemLibrary.get_command_line().split()
CAPTURE_BASELINE = "-LightingCaptureBaseline" in unreal.SystemLibrary.get_command_line().split()
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EXPECTED_CVARS = {"r.DynamicGlobalIlluminationMethod": 0, "r.ReflectionMethod": 2, "r.Nanite": 0, "r.Nanite.ProjectEnabled": 0, "r.Lumen.Supported": 0, "r.Nanite.ProxyRenderMode": 0, "r.Shadow.Virtual.Enable": 0}
LIGHTING_CLASSES = (unreal.Light, unreal.SkyLight, unreal.PostProcessVolume, unreal.ExponentialHeightFog, unreal.SkyAtmosphere, unreal.VolumetricCloud)
require = dungeon.require


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def map_file(package):
    require(package.startswith(environment.OUTPUT_ROOT) and "/" not in package.removeprefix(environment.OUTPUT_ROOT), "Lighting target must be an Environment world")
    return "Content/" + package.removeprefix("/Game/") + ".umap"


def content_hashes(excluded):
    paths = sorted(path for path in (ROOT / "Content").rglob("*") if path.is_file() and path.relative_to(ROOT).as_posix() not in excluded)

    def inspect(path):
        return path.relative_to(ROOT).as_posix(), {"bytes": path.stat().st_size, "sha256": digest(path)}

    with ThreadPoolExecutor(max_workers=4) as pool:
        return dict(pool.map(inspect, paths))


def snapshot_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")).hexdigest()


def asset_path(asset):
    return asset.get_path_name() if asset else None


def instance_transform(component, index):
    value = component.get_instance_transform(index, True)
    if isinstance(value, tuple):
        value = next((item for item in value if isinstance(item, unreal.Transform)), None)
    require(isinstance(value, unreal.Transform), "Could not inspect an existing mesh instance")
    return value.export_text()


def component_state(component):
    state = {"name": component.get_name(), "class": component.get_class().get_path_name(), "navigation": component.get_editor_property("can_ever_affect_navigation")}
    if isinstance(component, unreal.SceneComponent):
        state.update(world_transform=component.get_world_transform().export_text(), relative_transform=component.get_relative_transform().export_text(), mobility=str(component.get_editor_property("mobility")))
    if isinstance(component, unreal.PrimitiveComponent):
        state.update(collision=str(component.get_collision_enabled()), collision_profile=str(component.get_collision_profile_name()), overlap=component.get_editor_property("generate_overlap_events"), visible=component.get_editor_property("visible"), cast_shadows=component.get_editor_property("cast_shadow"))
    if isinstance(component, unreal.MeshComponent):
        state["materials"] = [asset_path(component.get_material(index)) for index in range(component.get_num_materials())]
    if isinstance(component, unreal.StaticMeshComponent):
        state.update(mesh=asset_path(component.get_editor_property("static_mesh")), draw_distance=component.get_editor_property("ld_max_draw_distance"), distance_field_lighting=component.get_editor_property("affect_distance_field_lighting"))
    if isinstance(component, unreal.SkeletalMeshComponent):
        state["mesh"] = asset_path(component.get_editor_property("skeletal_mesh_asset"))
    if isinstance(component, unreal.InstancedStaticMeshComponent):
        state.update(instances=[instance_transform(component, index) for index in range(component.get_instance_count())], cull_start=component.get_editor_property("instance_start_cull_distance"), cull_end=component.get_editor_property("instance_end_cull_distance"))
    if isinstance(component, unreal.CameraComponent):
        state["camera"] = {name: str(component.get_editor_property(name)) for name in ["field_of_view", "aspect_ratio", "constrain_aspect_ratio", "override_aspect_ratio_axis_constraint", "aspect_ratio_axis_constraint", "post_process_blend_weight"]}
        state["camera"]["post_process_settings"] = component.get_editor_property("post_process_settings").export_text()
    if isinstance(component, unreal.NiagaraComponent):
        state["effect"] = asset_path(component.get_asset())
    if isinstance(component, unreal.ParticleSystemComponent):
        state["effect"] = asset_path(component.get_editor_property("template"))
    return state


def nonlighting_state():
    # Preserve actual scene data, including camera post process and every saved ISM instance.
    # 카메라 후처리와 저장된 모든 ISM 인스턴스를 포함한 실제 씬 데이터를 보존합니다.
    states = []
    for actor in ACTORS.get_all_level_actors():
        if isinstance(actor, LIGHTING_CLASSES):
            continue
        components = [component_state(component) for component in actor.get_components_by_class(unreal.ActorComponent)]
        states.append({"name": actor.get_name(), "label": actor.get_actor_label(), "class": actor.get_class().get_path_name(), "transform": actor.get_actor_transform().export_text(), "collision": actor.get_actor_enable_collision(), "tags": sorted(str(tag) for tag in actor.get_editor_property("tags")), "components": sorted(components, key=lambda value: value["name"])})
    return sorted(states, key=lambda value: value["name"])


def renderer_settings():
    configured = {}
    for line in (ROOT / "Config/DefaultEngine.ini").read_text(encoding="utf-8-sig").splitlines():
        name, separator, value = line.strip().partition("=")
        if separator and name in EXPECTED_CVARS:
            configured[name] = value.strip()
    result = {}
    for name, expected in EXPECTED_CVARS.items():
        value = configured.get(name)
        require(value is not None and value.lower() in (["0", "false"] if expected == 0 else [str(expected)]), "Renderer configuration differs: " + name)
        actual = unreal.SystemLibrary.get_console_variable_int_value(name)
        require(actual == expected, "Active renderer CVar differs: " + name + "=" + str(actual))
        result[name] = {"configured": value, "runtime": actual}
    return result


def read_targets():
    outdoor = environment.read_specs()
    require(len(outdoor) == 12, "Lighting authoring requires all twelve environment specifications")
    indoor = [json.loads((SCRIPT_DIRECTORY / (name + "Spec.json")).read_text(encoding="utf-8")) for name in dungeon.LEVELS]
    require(len(indoor) == 2, "Lighting authoring requires both dungeon specifications")
    targets = [("environment", environment.level_path(spec), spec) for spec in outdoor]
    for name, spec in zip(dungeon.LEVELS, indoor):
        require(spec["level"] == environment.OUTPUT_ROOT + name, "Dungeon world must use its moved Environment path")
        targets.append(("dungeon", spec["level"], spec))
    require(len({package for _, package, _ in targets}) == 14, "Lighting worlds must be unique")
    for _, package, _ in targets:
        require((ROOT / map_file(package)).is_file(), "Moved world file is missing: " + package)
    return targets


def expected_layouts():
    _, source = environment.preserved_layout(dungeon.SOURCE_MAP)
    mode = require(unreal.load_asset(dungeon.DUNGEON_MODE), "Standalone combat mode is missing")
    require(isinstance(mode, unreal.Blueprint), "Standalone combat mode must be a Blueprint")
    defaults = unreal.get_default_object(mode.generated_class())
    require(isinstance(defaults, unreal.CombatDebugGameMode) and defaults.get_editor_property("party_definition") and defaults.get_editor_property("enemy_definition"), "Standalone combat definitions are missing")
    expected = dict(source, game_mode=mode.generated_class().get_path_name())
    indoor = dict(expected)
    del indoor["camera_aspect_ratio"]
    return {"environment": expected, "dungeon": indoor}


def write_report(name, report):
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    temporary = DIRECTORY / (name + ".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(DIRECTORY / name)


def configure_target(kind, package, spec, expected, previous):
    loader = environment.preserved_layout if kind == "environment" else dungeon.core_layout
    world, layout = loader(package)
    require(world.get_path_name() == package + "." + package.rsplit("/", 1)[1] and layout == expected, "Lighting world or combat layout differs: " + package)
    before = nonlighting_state()
    before_hash = snapshot_hash(before)
    if VERIFY_ONLY:
        require(previous.get(package) == before_hash, "Saved nonlighting scene differs from the authored baseline: " + package)
    else:
        # Apply absolute specification values; never rebuild decoration or multiply saved light values.
        # 저장된 광원 값을 반복 곱하거나 장식을 재생성하지 않고 명세의 절대값만 적용합니다.
        (environment.apply_lighting if kind == "environment" else dungeon.apply_lighting)(spec)
        require(nonlighting_state() == before, "Lighting changed nonlighting scene data before saving: " + package)
        require(unreal.EditorLoadingAndSavingUtils.save_map(world, package), "Could not save the relit world: " + package)
    validation = environment.verify_environment(spec, expected, environment.quality_budget(spec)) if kind == "environment" else dungeon.verify_decoration(spec, expected)
    require(validation["layout"] == layout and nonlighting_state() == before, "Saved lighting changed the combat layout or nonlighting scene: " + package)
    return {"level": package, "kind": kind, "nonlighting_sha256": before_hash, "nonlighting_actor_count": len(before), "nonlighting_component_count": sum(len(actor["components"]) for actor in before), "layout": layout, "lighting": spec["lighting"] if kind == "environment" else spec["palette"], "validation": {key: value for key, value in validation.items() if key not in ["meshes", "lights", "effects", "layout"]}, "nonlighting_preserved": True}


def main():
    require(not (VERIFY_ONLY and CAPTURE_BASELINE), "CaptureBaseline and VerifyOnly cannot be combined")
    targets = read_targets()
    if CAPTURE_BASELINE:
        # Capture a reviewed starting state explicitly; never replace it automatically during Apply or Verify.
        # 검토한 시작 상태를 명시적으로 기록하며 적용이나 검증 도중 자동 교체하지 않습니다.
        write_report("ContentBefore.json", content_hashes(set()))
        unreal.log("LIGHTING_BASELINE_CAPTURED " + str(DIRECTORY / "ContentBefore.json"))
        return
    settings = renderer_settings()
    target_files = {map_file(package) for _, package, _ in targets}
    excluded = set() if VERIFY_ONLY else target_files
    unreal.log("LIGHTING_PROTECTED_HASHES_START")
    before = content_hashes(excluded)
    baseline = json.loads((DIRECTORY / "ContentBefore.json").read_text(encoding="utf-8"))
    require({path: value for path, value in before.items() if path not in target_files} == {path: value for path, value in baseline.items() if path not in target_files}, "Protected Content differs from the pre-lighting baseline")
    previous = {}
    if VERIFY_ONLY:
        authored = json.loads((DIRECTORY / "Configuration.json").read_text(encoding="utf-8"))
        require(authored.get("completed") and len(authored.get("levels", [])) == 14, "Complete lighting authoring report is required")
        previous = {level["level"]: level["nonlighting_sha256"] for level in authored["levels"]}
    layouts = expected_layouts()
    report = {"mode": "verify" if VERIFY_ONLY else "configure", "completed": False, "renderer": settings, "levels": [], "protected_content_files": len(before), "gameplay_test": "not run", "render_test": "not run", "pie_test": "not run"}
    output = "Reload.json" if VERIFY_ONLY else "Configuration.json"
    try:
        for kind, package, spec in targets:
            unreal.log("LIGHTING_AUTHORING " + package)
            report["levels"].append(configure_target(kind, package, spec, layouts[kind], previous))
            write_report(output, report)
        require(content_hashes(excluded) == before, "Lighting changed protected Content files or a read-only reload wrote assets")
        require(renderer_settings() == settings, "Renderer settings changed during lighting authoring")
        report.update(completed=True, protected_content_unchanged=True, nonlighting_preserved=True)
        write_report(output, report)
        unreal.log("LIGHTING_COMPLETE " + str(DIRECTORY / output))
    except Exception as error:
        report["error"] = str(error)
        write_report(output, report)
        raise


if __name__ == "__main__":
    main()
