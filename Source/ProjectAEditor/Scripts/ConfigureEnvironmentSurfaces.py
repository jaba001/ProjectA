import hashlib
import json
import math
import os
import re
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC_FILE = Path(__file__).resolve().parent / "EnvironmentLevelSpecs.json"
OUTPUT_ROOT = "/Game/User_JeHoon/Materials/Environment/"
LIBRARY = unreal.EditorAssetLibrary
EDITING = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
COMMAND_LINE = unreal.SystemLibrary.get_command_line()
VERIFY_ONLY = "-EnvironmentSurfacesVerifyOnly" in COMMAND_LINE
REBUILD = "-EnvironmentSurfacesRebuild" in COMMAND_LINE
OWNER_TAG = unreal.Name("ProjectAEnvironmentSurface")
OWNER_VALUE = "1"
PACKAGE_SUFFIXES = [".uasset", ".umap", ".uexp", ".ubulk", ".uptnl"]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def package_path(path):
    require(isinstance(path, str) and re.fullmatch(r"/(?:Game|Engine)/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+", path), "Invalid package path: " + str(path))
    return path


def owned_path(path):
    package_path(path)
    require(path.startswith(OUTPUT_ROOT) and re.fullmatch(r"(?:[A-Za-z0-9_-]+/)*[A-Za-z][A-Za-z0-9_-]*", path.removeprefix(OUTPUT_ROOT)), "Environment material output must stay inside " + OUTPUT_ROOT)
    return path


def source_path(path):
    package_path(path)
    require(path != "/Game/User_JeHoon" and not path.startswith("/Game/User_JeHoon/"), "Reference original source textures and parents directly: " + path)
    return path


def source_root(path):
    return "/" + "/".join(source_path(path).split("/")[1:3])


def read_specs():
    require(not (VERIFY_ONLY and REBUILD), "Surface verify and rebuild are mutually exclusive")
    document = json.loads(SPEC_FILE.read_text(encoding="utf-8"))
    surfaces, instances = document.get("surfaces", []), document.get("instances", [])
    require(isinstance(surfaces, list) and isinstance(instances, list) and surfaces + instances, "Declare at least one surface or instance")
    outputs = [owned_path(item["asset"]) for item in surfaces + instances]
    require(len(outputs) == len(set(outputs)), "Environment surface output paths must be unique")
    for item in surfaces:
        require(set(item).issubset({"asset", "texture", "normal", "color", "uv_scale", "roughness"}), "Unknown environment surface field")
        source_path(item["texture"])
        if item.get("normal"):
            source_path(item["normal"])
        require(isinstance(item["color"], list) and len(item["color"]) == 3 and all(finite(value) and value >= 0.0 for value in item["color"]), "Surface tint must be three nonnegative linear values")
        require(finite(item["uv_scale"]) and item["uv_scale"] > 0.0 and finite(item["roughness"]) and 0.0 <= item["roughness"] <= 1.0, "Invalid surface UV scale or roughness")
    for item in instances:
        require(set(item).issubset({"asset", "parent", "static_switches", "scalar_parameters", "vector_parameters", "instanced_mesh_usage"}), "Unknown environment instance field")
        source_path(item["parent"])
        require(isinstance(item.get("instanced_mesh_usage", False), bool), "Instanced mesh usage override must be boolean")
        for field in ["static_switches", "scalar_parameters", "vector_parameters"]:
            values = item.get(field, {})
            require(isinstance(values, dict) and all(isinstance(name, str) and name and name != "None" for name in values), "Invalid instance parameter names")
            for name, value in values.items():
                if field == "static_switches":
                    require(isinstance(value, bool), "Static switch must be boolean: " + name)
                elif field == "scalar_parameters":
                    require(finite(value), "Scalar parameter must be finite: " + name)
                else:
                    require(isinstance(value, list) and len(value) in [3, 4] and all(finite(channel) for channel in value), "Vector parameter must have three or four finite channels: " + name)
    roots = {source_path(root.rstrip("/")) for level in document.get("levels", []) for root in level.get("source_roots", [])}
    roots.add("/Game/User_JeHoon")
    roots.update(source_root(item[field]) for item in surfaces for field in ["texture", "normal"] if item.get(field))
    roots.update(source_root(item["parent"]) for item in instances)
    return surfaces, instances, set(outputs), roots


def digest(path):
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def protected_hashes(outputs, roots):
    # Protect User_JeHoon and every declared or directly referenced source root; unrelated pack downloads remain independent.
    # User_JeHoon과 선언되거나 직접 참조된 원본 루트를 보호하며 관계없는 팩 다운로드는 별도로 진행할 수 있습니다.
    directories = {ROOT / "Content" / category / "User_JeHoon" for category in ["__ExternalActors__", "__ExternalObjects__"]}
    engine_content = Path(unreal.Paths.engine_content_dir()).resolve()
    directories.update(ROOT / "Content" / root.removeprefix("/Game/") if root.startswith("/Game/") else engine_content / root.removeprefix("/Engine/") for root in roots)
    require(all(directory.is_dir() for directory in directories if directory not in {ROOT / "Content/__ExternalActors__/User_JeHoon", ROOT / "Content/__ExternalObjects__/User_JeHoon"}), "A declared source or project root is missing")
    excluded = {ROOT / "Content" / (path.removeprefix("/Game/") + suffix) for path in outputs for suffix in PACKAGE_SUFFIXES if suffix != ".umap"}
    files = {path for directory in directories for path in directory.rglob("*") if path.is_file() and path.suffix.lower() in PACKAGE_SUFFIXES and path not in excluded}
    return {str(path): digest(path) for path in sorted(files)}


def load_source(path, expected_type):
    asset = require(unreal.load_asset(source_path(path)), "Missing original source asset: " + path)
    require(isinstance(asset, expected_type) and asset.get_path_name().split(".")[0] == path, "Unexpected original source class or redirected path: " + path)
    return asset


def sampler_type(texture):
    # Match UE 5.8 MaterialExpressionUtils using source compression, gamma and virtual streaming; never change the texture.
    # 원본 압축, 감마, VT 설정으로 UE 5.8 MaterialExpressionUtils와 같은 샘플러를 선택하며 텍스처는 변경하지 않습니다.
    compression = texture.get_editor_property("compression_settings")
    srgb = texture.get_editor_property("srgb")
    virtual = texture.get_editor_property("virtual_texture_streaming")
    kinds = {unreal.TextureCompressionSettings.TC_NORMALMAP: "NORMAL", unreal.TextureCompressionSettings.TC_MASKS: "MASKS", unreal.TextureCompressionSettings.TC_ALPHA: "ALPHA"}
    if compression == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        kind = "GRAYSCALE" if srgb else "LINEAR_GRAYSCALE"
    else:
        kind = kinds.get(compression, "COLOR" if srgb else "LINEAR_COLOR")
    require(compression != unreal.TextureCompressionSettings.TC_DISTANCE_FIELD_FONT, "Font texture is not a ground surface")
    require(texture.get_editor_property("lod_group") not in [unreal.TextureGroup.TEXTUREGROUP_8_BIT_DATA, unreal.TextureGroup.TEXTUREGROUP_16_BIT_DATA], "Data textures are not ground surfaces")
    require(kind not in ["NORMAL", "MASKS"] or not srgb, "Normal and mask source textures must already have sRGB disabled")
    return getattr(unreal.MaterialSamplerType, "SAMPLERTYPE_" + ("VIRTUAL_" if virtual else "") + kind)


def validate_sources(surfaces, instances):
    for item in surfaces:
        texture = load_source(item["texture"], unreal.Texture2D)
        require(texture.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_NORMALMAP, "Diffuse source cannot be a normal map")
        sampler_type(texture)
        if item.get("normal"):
            normal = load_source(item["normal"], unreal.Texture2D)
            require(normal.get_editor_property("compression_settings") == unreal.TextureCompressionSettings.TC_NORMALMAP, "Normal source must already use Normalmap compression")
            sampler_type(normal)
    for item in instances:
        parent = load_source(item["parent"], unreal.MaterialInterface)
        for field, kind in [("static_switches", "static_switch"), ("scalar_parameters", "scalar"), ("vector_parameters", "vector")]:
            names = {str(name) for name in getattr(EDITING, "get_" + kind + "_parameter_names")(parent)}
            require(set(item.get(field, {})).issubset(names), "Unknown source parent " + kind + " parameters: " + str(set(item.get(field, {})) - names))


def load_output(path, expected_type):
    asset = require(unreal.load_asset(path), "Missing environment surface: " + path)
    require(isinstance(asset, expected_type) and asset.get_path_name().split(".")[0] == path, "Environment output type or path differs: " + path)
    require(LIBRARY.get_metadata_tag(asset, OWNER_TAG) == OWNER_VALUE, "Refusing an asset without this generator's ownership marker: " + path)
    return asset


def create_output(path, expected_type, factory):
    if LIBRARY.does_asset_exist(path):
        return load_output(path, expected_type)
    name, directory = path.rsplit("/", 1)[1], path.rsplit("/", 1)[0]
    asset = require(TOOLS.create_asset(name, directory, expected_type, factory), "Could not create environment material: " + path)
    LIBRARY.set_metadata_tag(asset, OWNER_TAG, OWNER_VALUE)
    return asset


def save_output(asset, outputs):
    require(asset.get_path_name().split(".")[0] in outputs and LIBRARY.get_metadata_tag(asset, OWNER_TAG) == OWNER_VALUE, "Refusing to save an undeclared environment material")
    require(LIBRARY.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save environment material")


def make_node(material, node_class, role, x, y):
    node = require(EDITING.create_material_expression(material, node_class, x, y), "Could not create surface node: " + role)
    node.set_editor_property("desc", "ProjectAEnvironmentSurface:" + role + " / 환경 지면 재질")
    return node


def connect(source, target, input_name, output_name=""):
    source_label, target_label = source.get_editor_property("desc"), target.get_editor_property("desc")
    outputs = [str(name) for name in EDITING.get_material_expression_output_names(source)]
    inputs = [str(name) for name in EDITING.get_material_expression_input_names(target)]
    require(EDITING.connect_material_expressions(source, output_name, target, input_name), "Could not connect " + source_label + " output=" + repr(output_name) + " to " + target_label + " input=" + repr(input_name) + "; available outputs=" + str(outputs) + ", inputs=" + str(inputs))


def describe_node(node):
    description = node.get_editor_property("desc") if node else ""
    return {"class": node.get_class().get_name() if node else "None", "path": node.get_path_name() if node else "None", "desc": description, "role": description.removeprefix("ProjectAEnvironmentSurface:").split(" / ")[0]}


def clear_surface_graph(material):
    # UE 5.8's bulk delete iterates its changing source array; delete an independent snapshot to avoid skipped nodes.
    # UE 5.8 일괄 삭제는 변경 중인 원본 배열을 순회하므로 별도 목록을 개별 삭제하여 누락을 방지합니다.
    for node in list(EDITING.get_material_expressions(material)):
        EDITING.delete_material_expression(material, node)
    remaining = [describe_node(node) for node in EDITING.get_material_expressions(material)]
    require(not remaining, "Surface graph could not be completely cleared: " + json.dumps(remaining, ensure_ascii=False))


def configure_surface(item, outputs):
    material = create_output(item["asset"], unreal.Material, unreal.MaterialFactoryNew())
    material.modify()
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("two_sided", False)
    material.set_editor_property("tangent_space_normal", True)
    EDITING.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    clear_surface_graph(material)
    world = make_node(material, unreal.MaterialExpressionWorldPosition, "WorldPosition", -1100, 0)
    world.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    mask = make_node(material, unreal.MaterialExpressionComponentMask, "WorldXY", -900, 0)
    for channel, enabled in {"r": True, "g": True, "b": False, "a": False}.items():
        mask.set_editor_property(channel, enabled)
    scale = make_node(material, unreal.MaterialExpressionConstant, "UVScale", -900, 180)
    scale.set_editor_property("r", item["uv_scale"])
    uv = make_node(material, unreal.MaterialExpressionMultiply, "WorldUV", -700, 0)
    connect(world, mask, "", "XYZ")
    connect(mask, uv, "A")
    connect(scale, uv, "B")
    samples = {}
    for role, path in [("Diffuse", item["texture"])] + ([("Normal", item["normal"])] if item.get("normal") else []):
        texture = load_source(path, unreal.Texture2D)
        sample = make_node(material, unreal.MaterialExpressionTextureSample, role, -500, 0 if role == "Diffuse" else 450)
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", sampler_type(texture))
        sample.set_editor_property("sampler_source", unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        connect(uv, sample, "")
        samples[role] = sample
    tint = make_node(material, unreal.MaterialExpressionConstant3Vector, "LinearTint", -500, 220)
    tint.set_editor_property("constant", unreal.LinearColor(*item["color"], 1.0))
    base = make_node(material, unreal.MaterialExpressionMultiply, "BaseColor", -200, 0)
    # TextureSample output zero is RGB; UE 5.8's name resolver handles single channels but not an unnamed RGB output.
    # TextureSample의 출력 0은 RGB이며 UE 5.8 이름 변환기는 이름 없는 RGB 대신 단일 채널만 인식합니다.
    connect(samples["Diffuse"], base, "A")
    connect(tint, base, "B")
    require(EDITING.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR), "Could not connect surface base color")
    for role, value, material_property, y in [("Roughness", item["roughness"], unreal.MaterialProperty.MP_ROUGHNESS, 270), ("Metallic", 0.0, unreal.MaterialProperty.MP_METALLIC, 350)]:
        constant = make_node(material, unreal.MaterialExpressionConstant, role, -200, y)
        constant.set_editor_property("r", value)
        require(EDITING.connect_material_property(constant, "", material_property), "Could not connect surface " + role)
    if "Normal" in samples:
        require(EDITING.connect_material_property(samples["Normal"], "", unreal.MaterialProperty.MP_NORMAL), "Could not connect surface normal")
    errors = [str(error) for error in EDITING.recompile_material(material)]
    require(not errors, "Environment material compilation failed: " + str(errors))
    save_output(material, outputs)
    return {"compiler_errors": errors, "material_compile": "recompile requested; API reported no errors; shader map completion not verified under NullRHI"}


def rgba(value):
    return value if len(value) == 4 else value + [1.0]


def configure_instance(item, outputs):
    instance = create_output(item["asset"], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    instance.modify()
    EDITING.set_material_instance_parent(instance, load_source(item["parent"], unreal.MaterialInterface))
    EDITING.clear_all_material_instance_parameters(instance)
    # Override instancing on this new instance only; clearing the override restores its original parent's setting.
    # 새 인스턴스에만 ISM 사용을 override하며 해제하면 원본 부모의 설정을 상속합니다.
    EDITING.set_material_usage_override(instance, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, item.get("instanced_mesh_usage", False), True)
    # UE 5.8 parameter setters always return false; verify effective values and explicit overrides before saving.
    # UE 5.8 파라미터 setter는 항상 false를 반환하므로 저장 전에 실제 값과 명시적 override를 검증합니다.
    for name, value in item.get("static_switches", {}).items():
        EDITING.set_material_instance_static_switch_parameter_value(instance, unreal.Name(name), value, unreal.MaterialParameterAssociation.GLOBAL_PARAMETER, False)
    for name, value in item.get("scalar_parameters", {}).items():
        EDITING.set_material_instance_scalar_parameter_value(instance, unreal.Name(name), value)
    for name, value in item.get("vector_parameters", {}).items():
        EDITING.set_material_instance_vector_parameter_value(instance, unreal.Name(name), unreal.LinearColor(*rgba(value)))
    EDITING.update_material_instance(instance)
    verify_instance(item)
    save_output(instance, outputs)
    return {"material_compile": "instance update requested; compiler diagnostics unavailable through this API"}


def check_input(material, node, input_index, expected, expected_output=""):
    if not expected_output:
        expected_output = str(EDITING.get_material_expression_output_names(expected)[0])
    # Official graph APIs preserve expression input order without reading protected FExpressionInput properties.
    # 공식 그래프 API는 보호된 FExpressionInput 속성을 읽지 않고 표현식 입력 순서를 유지합니다.
    inputs = list(EDITING.get_inputs_for_material_expression(material, node))
    names = [str(name) for name in EDITING.get_material_expression_input_names(node)]
    label = node.get_editor_property("desc") + " input index=" + str(input_index) + "; inputs=" + str(names)
    require(len(inputs) == len(names) and 0 <= input_index < len(inputs) and inputs[input_index] == expected, "Saved surface graph input differs: " + label)
    # UE Python returns the output string on success or None on failure for bool plus one output parameter.
    # UE Python은 bool과 출력 인자 하나인 함수에서 성공 시 출력 문자열을, 실패 시 None을 반환합니다.
    output_name = EDITING.get_input_node_output_name_for_material_expression(node, expected)
    require(output_name is not None and str(output_name) == expected_output, "Saved surface graph source output differs: " + label + "; actual=" + repr(output_name) + "; expected=" + repr(expected_output))


def close(actual, expected):
    return abs(actual - expected) <= max(0.000001, abs(expected) * 0.00001)


def verify_surface(item):
    material = load_output(item["asset"], unreal.Material)
    require(material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_OPAQUE and material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_SURFACE and not material.get_editor_property("two_sided") and material.get_editor_property("tangent_space_normal"), "Saved surface material properties differ")
    classes = {"WorldPosition": unreal.MaterialExpressionWorldPosition, "WorldXY": unreal.MaterialExpressionComponentMask, "UVScale": unreal.MaterialExpressionConstant, "WorldUV": unreal.MaterialExpressionMultiply, "Diffuse": unreal.MaterialExpressionTextureSample, "LinearTint": unreal.MaterialExpressionConstant3Vector, "BaseColor": unreal.MaterialExpressionMultiply, "Roughness": unreal.MaterialExpressionConstant, "Metallic": unreal.MaterialExpressionConstant}
    if item.get("normal"):
        classes["Normal"] = unreal.MaterialExpressionTextureSample
    nodes = {}
    expressions = list(EDITING.get_material_expressions(material))
    details = [describe_node(node) for node in expressions]
    for node, detail in zip(expressions, details):
        desc, role = detail["desc"], detail["role"]
        require(desc.startswith("ProjectAEnvironmentSurface:") and role in classes and role not in nodes and isinstance(node, classes[role]), "Unknown or duplicate saved surface graph node: " + json.dumps({"offender": detail, "validated_roles": list(nodes), "expected_roles": list(classes), "all_expressions": details}, ensure_ascii=False))
        nodes[role] = node
    require(set(nodes) == set(classes) and len(expressions) == len(classes), "Saved surface graph is incomplete: " + json.dumps({"expected_roles": list(classes), "all_expressions": details}, ensure_ascii=False))
    require(EDITING.has_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES) and nodes["WorldPosition"].get_editor_property("world_position_shader_offset") == unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS, "Saved surface instancing usage or absolute world coordinates differ")
    require(all(nodes["WorldXY"].get_editor_property(channel) == value for channel, value in {"r": True, "g": True, "b": False, "a": False}.items()), "Saved world XY mask differs")
    require(close(nodes["UVScale"].get_editor_property("r"), item["uv_scale"]) and close(nodes["Roughness"].get_editor_property("r"), item["roughness"]) and nodes["Metallic"].get_editor_property("r") == 0.0, "Saved surface constants differ")
    tint = nodes["LinearTint"].get_editor_property("constant")
    require(all(close(getattr(tint, channel), value) for channel, value in zip(["r", "g", "b"], item["color"])), "Saved linear surface tint differs")
    for role, field in [("Diffuse", "texture")] + ([("Normal", "normal")] if item.get("normal") else []):
        texture = load_source(item[field], unreal.Texture2D)
        sample = nodes[role]
        require(sample.get_editor_property("texture") == texture and sample.get_editor_property("sampler_type") == sampler_type(texture) and sample.get_editor_property("sampler_source") == unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS, "Saved source texture or sampler differs")
        check_input(material, sample, 0, nodes["WorldUV"])
    for role, input_index, source, output_name in [("WorldXY", 0, "WorldPosition", "XYZ"), ("WorldUV", 0, "WorldXY", ""), ("WorldUV", 1, "UVScale", ""), ("BaseColor", 0, "Diffuse", ""), ("BaseColor", 1, "LinearTint", "")]:
        check_input(material, nodes[role], input_index, nodes[source], output_name)
    for role, material_property in [("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR), ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS), ("Metallic", unreal.MaterialProperty.MP_METALLIC)]:
        require(EDITING.get_material_property_input_node(material, material_property) == nodes[role], "Saved surface output differs: " + role)
    require(EDITING.get_material_property_input_node(material, unreal.MaterialProperty.MP_NORMAL) == nodes.get("Normal"), "Saved normal output differs")
    return {"asset": item["asset"], "texture": item["texture"], "normal": item.get("normal"), "color_linear": item["color"], "uv_scale_per_cm": item["uv_scale"], "repeat_distance_cm": 1.0 / item["uv_scale"], "roughness": item["roughness"], "graph_nodes": len(nodes), "graph_validation": "passed", "graph_validation_scope": "node classes, constants, texture references, ordered input nodes and exposed output names; indices of unnamed outputs are not exposed by this API", "gamma": "source texture sRGB retained", "sampler": str(nodes["Diffuse"].get_editor_property("sampler_type")), "visual_test": "not run"}


def verify_instance(item):
    instance = load_output(item["asset"], unreal.MaterialInstanceConstant)
    require(instance.get_editor_property("parent") == load_source(item["parent"], unreal.MaterialInterface), "Saved original parent differs")
    usage = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
    override = item.get("instanced_mesh_usage", False)
    require(EDITING.has_material_usage_override(instance, usage) == override, "Saved instance instanced mesh usage override differs")
    effective_usage = EDITING.has_material_usage(instance, usage)
    require(not override or effective_usage, "Saved instance instanced mesh usage is disabled")
    for field, kind in [("static_switches", "static_switch"), ("scalar_parameters", "scalar"), ("vector_parameters", "vector")]:
        getter = getattr(EDITING, "get_material_instance_" + kind + "_parameter_value")
        for name, expected in item.get(field, {}).items():
            require(EDITING.is_material_instance_parameter_overridden(instance, unreal.Name(name)), "Saved parameter lacks an explicit override: " + name)
            actual = getter(instance, unreal.Name(name))
            matches = actual == expected if kind == "static_switch" else close(actual, expected) if kind == "scalar" else all(close(getattr(actual, channel), value) for channel, value in zip(["r", "g", "b", "a"], rgba(expected)))
            require(matches, "Saved instance parameter differs: " + name)
    return {"asset": item["asset"], "parent": item["parent"], "static_switches": item.get("static_switches", {}), "scalar_parameters": item.get("scalar_parameters", {}), "vector_parameters": item.get("vector_parameters", {}), "instanced_mesh_usage": override, "effective_instanced_mesh_usage": effective_usage, "parameter_validation": "passed", "shader_execution": "not run", "visual_test": "not run"}


def write_report(report):
    output = ROOT / "Saved/Automation/Environments" / ("SurfacesReload.json" if VERIFY_ONLY else "SurfacesConfiguration.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(output)
    return output


def main():
    surfaces, instances, outputs, roots = read_specs()
    for item in surfaces + instances:
        exists = LIBRARY.does_asset_exist(item["asset"])
        require(exists if VERIFY_ONLY else REBUILD or not exists, "Verify an existing environment surface or explicitly use -EnvironmentSurfacesRebuild")
        if exists:
            load_output(item["asset"], unreal.Material if item in surfaces else unreal.MaterialInstanceConstant)
    unreal.log("ENVIRONMENT_SURFACES_PROTECTED_HASHES_START")
    before = protected_hashes(outputs, roots)
    report = {"mode": "verify" if VERIFY_ONLY else "configure", "process_id": os.getpid(), "status": "in progress", "surfaces": [], "instances": [], "protected_roots": sorted(roots), "protected_external_roots": ["/Game/__ExternalActors__/User_JeHoon", "/Game/__ExternalObjects__/User_JeHoon"], "protected_asset_files": len(before), "shader_execution": "not run", "visual_performance_test": "not run", "reload_validation": "loaded package graph and parameters checked; use verify-only in a fresh process to validate disk reload"}
    error = None
    try:
        validate_sources(surfaces, instances)
        for item in surfaces:
            report["current_asset"] = item["asset"]
            unreal.log("ENVIRONMENT_SURFACE_AUTHORING " + item["asset"])
            compilation = {} if VERIFY_ONLY else configure_surface(item, outputs)
            report["surfaces"].append(dict(verify_surface(item), **compilation))
        for item in instances:
            report["current_asset"] = item["asset"]
            unreal.log("ENVIRONMENT_INSTANCE_AUTHORING " + item["asset"])
            compilation = {} if VERIFY_ONLY else configure_instance(item, outputs)
            report["instances"].append(dict(verify_instance(item), **compilation))
    except Exception as failure:
        error = failure
        report["error_traceback"] = traceback.format_exc()
        unreal.log_error(report["error_traceback"])
    report["source_preserved"] = protected_hashes(outputs, roots) == before
    if not report["source_preserved"] and error is None:
        error = RuntimeError("Existing project assets or original source packs changed")
    report["status"] = "failed" if error else "passed"
    if error:
        report["error"] = str(error)
    else:
        report.pop("current_asset", None)
    output = write_report(report)
    require(error is None, "Environment surface authoring failed; see " + str(output) + ": " + str(error))
    unreal.log("ENVIRONMENT_SURFACES_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
