import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
CEILING_ONLY = '-EncounterCeilingOnly' in unreal.SystemLibrary.get_command_line()
MASONRY_ONLY = '-EncounterMasonryOnly' in unreal.SystemLibrary.get_command_line()
STONE_COLOR_ONLY = '-EncounterStoneColorOnly' in unreal.SystemLibrary.get_command_line()
OUTPUT = ROOT / ('Saved/Automation/EncounterStoneColor_20261012' if STONE_COLOR_ONLY else 'Saved/Automation/EncounterMasonry_20261011' if MASONRY_ONLY else 'Saved/Automation/EncounterCeiling_20261011' if CEILING_ONLY else 'Saved/Automation/MazeTextures_20261011')
MATERIAL_ROOT = '/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials'
SOURCE_ROOT = '/Game/Dungeon_Modular_V1/Textures/'
OWNER_TAG = 'ProjectAEncounterSurface'
EDITING = unreal.MaterialEditingLibrary
LIBRARY = unreal.EditorAssetLibrary
SPECS = [(name, 'T_Stone_Wall_01_Master', 160.0) for name in ('M_EncounterStoneWall', 'M_EncounterStoneFloor', 'M_EncounterStoneCeiling')]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def connect(source, target, input_name, output_name=''):
    require(EDITING.connect_material_expressions(source, output_name, target, input_name), 'Connection failed: ' + str(EDITING.get_material_expression_output_names(source)) + ' -> ' + input_name + ' of ' + str(EDITING.get_material_expression_input_names(target)))


def node(material, node_class, label, x, y):
    result = require(EDITING.create_material_expression(material, node_class, x, y), 'Cannot create ' + label)
    result.set_editor_property('desc', 'EncounterSurface:' + label + ' / 미로 표면 재질')
    return result


def projection(material, texture, function_path, scale, label, x, y):
    texture_object = node(material, unreal.MaterialExpressionTextureObject, label + 'Texture', x, y)
    texture_object.set_editor_property('texture', texture)
    function = require(unreal.load_asset(function_path), 'Missing engine projection function')
    call = node(material, unreal.MaterialExpressionMaterialFunctionCall, label + 'Projection', x + 220, y)
    call.set_editor_property('material_function', function)
    connect(texture_object, call, 'TextureObject')
    connect(scale, call, 'TextureSize')
    return call


def package_file(path):
    return ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')


def stone_palette(material, diffuse, output_name=''):
    # Preserve the original stone RGB and contrast with one uniform dark warm multiplier.
    # 균일하고 어두운 따뜻한 배율만 적용하여 원본 석재 RGB와 명암 대비를 보존합니다.
    tint = node(material, unreal.MaterialExpressionVectorParameter, 'StoneTint', -300, -300)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.65, 0.60, 0.52, 1.0))
    base = node(material, unreal.MaterialExpressionMultiply, 'StoneBaseColor', 0, -100)
    connect(diffuse, base, 'A', output_name)
    connect(tint, base, 'B')
    return base


def surface_properties(material, diffuse, sampled_normal, sampled_roughness, horizontal):
    base = stone_palette(material, diffuse, '' if horizontal else 'XYZ Texture')
    flat = node(material, unreal.MaterialExpressionConstant3Vector if horizontal else unreal.MaterialExpressionVertexNormalWS, 'FlatNormal', -450, 300)
    if horizontal:
        flat.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    strength = node(material, unreal.MaterialExpressionScalarParameter, 'NormalStrength', -450, 500)
    strength.set_editor_property('parameter_name', 'NormalStrength')
    strength.set_editor_property('default_value', 0.25)
    blend = node(material, unreal.MaterialExpressionLinearInterpolate, 'NormalBlend', -200, 350)
    connect(flat, blend, 'A')
    connect(sampled_normal, blend, 'B', '' if horizontal else 'XYZ Texture')
    connect(strength, blend, 'Alpha')
    normal = node(material, unreal.MaterialExpressionNormalize, 'StoneNormal', 0, 350)
    connect(blend, normal, '')
    mask = node(material, unreal.MaterialExpressionComponentMask, 'RoughnessR', -450, 700)
    for channel, enabled in {'r': True, 'g': False, 'b': False, 'a': False}.items():
        mask.set_editor_property(channel, enabled)
    connect(sampled_roughness, mask, '', '' if horizontal else 'XYZ Texture')
    roughness = node(material, unreal.MaterialExpressionClamp, 'StoneRoughness', -200, 700)
    roughness.set_editor_property('min_default', 0.8)
    roughness.set_editor_property('max_default', 1.0)
    connect(mask, roughness, '')
    specular = node(material, unreal.MaterialExpressionConstant, 'StoneSpecular', -200, 900)
    specular.set_editor_property('r', 0.15)
    for expression, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR), (normal, unreal.MaterialProperty.MP_NORMAL), (roughness, unreal.MaterialProperty.MP_ROUGHNESS), (specular, unreal.MaterialProperty.MP_SPECULAR)]:
        require(EDITING.connect_material_property(expression, '', prop), 'Surface property connection failed')


def configure_horizontal(material, prefix, size, ceiling):
    # Keep floor and ceiling textures level and use each mesh face's tangent basis.
    # 바닥·천장 텍스처를 수평으로 투영하고 각 메시 면의 접선 기저를 사용합니다.
    material.set_editor_property('tangent_space_normal', True)
    material.set_editor_property('two_sided', ceiling)
    world = node(material, unreal.MaterialExpressionWorldPosition, 'HorizontalWorldPosition', -1400, 0)
    world.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    xy = node(material, unreal.MaterialExpressionComponentMask, 'HorizontalXY', -1200, 0)
    for channel, enabled in {'r': True, 'g': True, 'b': False, 'a': False}.items():
        xy.set_editor_property(channel, enabled)
    connect(world, xy, '', 'XYZ')
    scale = node(material, unreal.MaterialExpressionScalarParameter, 'HorizontalTileSize', -1200, -200)
    scale.set_editor_property('parameter_name', 'HorizontalTileSize')
    scale.set_editor_property('default_value', size)
    uv = node(material, unreal.MaterialExpressionDivide, 'HorizontalUV', -1000, 0)
    connect(xy, uv, 'A')
    connect(scale, uv, 'B')
    samples = {}
    for label, suffix, y in [('Diffuse', '_D', 0), ('Normal', '_N', 350), ('Roughness', '_R', 700)]:
        texture = require(unreal.load_asset(SOURCE_ROOT + prefix + suffix), 'Missing horizontal surface ' + label)
        sample = node(material, unreal.MaterialExpressionTextureSample, 'Horizontal' + label, -800, y)
        sample.set_editor_property('texture', texture)
        compression = texture.get_editor_property('compression_settings')
        sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if label == 'Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if compression == unreal.TextureCompressionSettings.TC_MASKS else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        sample.set_editor_property('sampler_type', sampler_type)
        sample.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        connect(uv, sample, '')
        samples[label] = sample
    surface_properties(material, samples['Diffuse'], samples['Normal'], samples['Roughness'], True)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    require(sum((CEILING_ONLY, MASONRY_ONLY, STONE_COLOR_ONLY)) <= 1, 'Select only one authoring mode')
    protected = [SOURCE_ROOT + prefix + suffix for prefix in ('T_Stone_Wall_01_Master', 'T_Floor_Flagstone_01', 'T_Ceiling_01') for suffix in ('_D', '_N', '_R', '_M')]
    protected += ['/Game/Dungeon_Modular_V1/Materials/' + name for name in ('M_Master_TwoSided_01', 'M_Stone_Wall_Master_01', 'M_Floor_Flagstone_01')]
    hashes = {str(package_file(path).relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(package_file(path).read_bytes()).hexdigest() for path in protected}
    (OUTPUT / 'SourceAssetsBefore.json').write_text(json.dumps(hashes, indent=2) + '\n', encoding='utf-8')
    report = []
    for name, prefix, size in SPECS:
        if CEILING_ONLY and name != 'M_EncounterStoneCeiling':
            continue
        path = MATERIAL_ROOT + '/' + name
        if LIBRARY.does_asset_exist(path):
            material = require(unreal.load_asset(path), 'Cannot load owned material')
            require(LIBRARY.get_metadata_tag(material, OWNER_TAG) == '1', 'Existing output lacks ownership marker')
        else:
            material = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()), 'Cannot create material')
            LIBRARY.set_metadata_tag(material, OWNER_TAG, '1')
        material.modify()
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
        material.set_editor_property('material_domain', unreal.MaterialDomain.MD_SURFACE)
        material.set_editor_property('tangent_space_normal', False)
        material.set_editor_property('two_sided', False)
        EDITING.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        horizontal = name != 'M_EncounterStoneWall'
        if horizontal:
            configure_horizontal(material, prefix, size, name == 'M_EncounterStoneCeiling')
            EDITING.delete_unused_expressions(material)
            compile_errors = EDITING.recompile_material(material)
            require(not compile_errors, 'Ceiling compilation failed: ' + str(compile_errors))
            require(material.get_editor_property('used_with_instanced_static_meshes'), 'Ceiling instancing support missing')
            require(material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_OPAQUE and material.get_editor_property('two_sided') == (name == 'M_EncounterStoneCeiling'), 'Invalid horizontal surface settings')
            require(LIBRARY.save_loaded_asset(material, only_if_is_dirty=False), 'Cannot save ceiling material')
            report.append({'path': path, 'instanced_static_meshes': True, 'opaque': True, 'two_sided': name == 'M_EncounterStoneCeiling', 'tangent_space_normal': True, 'normal_strength': 0.25, 'roughness_min': 0.8, 'specular': 0.15, 'diffuse_rgb_preserved': True, 'diffuse_tint': [0.65, 0.60, 0.52], 'tile_size_cm': size, 'texture_paths': [SOURCE_ROOT + prefix + suffix for suffix in ('_D', '_N', '_R')], 'compile_errors': list(compile_errors)})
            continue
        scale = node(material, unreal.MaterialExpressionVectorParameter, 'WorldTileSize', -1200, -300)
        scale.set_editor_property('parameter_name', 'WorldTileSize')
        scale.set_editor_property('default_value', unreal.LinearColor(size, size, size, 1.0))
        diffuse = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_D'), 'Missing diffuse'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture', scale, 'Diffuse', -1200, 0)
        normal = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_N'), 'Missing normal'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal', scale, 'Normal', -1200, 350)
        roughness = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_R'), 'Missing roughness'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture', scale, 'Roughness', -1200, 700)
        surface_properties(material, diffuse, normal, roughness, False)
        # Remove the disconnected old graph with the engine's reachability analysis.
        # 엔진의 도달 가능성 분석으로 연결 해제된 이전 그래프를 정리합니다.
        EDITING.delete_unused_expressions(material)
        compile_errors = EDITING.recompile_material(material)
        require(not compile_errors, 'Material compilation failed: ' + str(compile_errors))
        require(material.get_editor_property('used_with_instanced_static_meshes'), 'Instanced mesh usage was not enabled')
        require(LIBRARY.save_loaded_asset(material, only_if_is_dirty=False), 'Cannot save owned material')
        report.append({'path': path, 'instanced_static_meshes': True, 'opaque': True, 'two_sided': False, 'tangent_space_normal': False, 'normal_strength': 0.25, 'roughness_min': 0.8, 'specular': 0.15, 'diffuse_rgb_preserved': True, 'diffuse_tint': [0.65, 0.60, 0.52], 'tile_size_cm': size, 'world_space_normal': True, 'texture_paths': [SOURCE_ROOT + prefix + suffix for suffix in ('_D', '_N', '_R')], 'expressions': len(EDITING.get_material_expressions(material)), 'compile_errors': list(compile_errors)})
    # The project-owned prototype still supplies mortar and NPC colors; persist its instancing support as well.
    # 프로젝트 소유 프로토타입은 줄눈·NPC 색상에 계속 사용하므로 인스턴싱 지원도 저장합니다.
    prototype_path = '/Game/User_JeHoon/Materials/Prototype/M_EncounterPrototype'
    prototype = require(unreal.load_asset(prototype_path), 'Missing project prototype')
    if not CEILING_ONLY and not MASONRY_ONLY and not STONE_COLOR_ONLY:
        EDITING.set_base_material_usage(prototype, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        compile_errors = EDITING.recompile_material(prototype)
        require(not compile_errors, 'Prototype compilation failed: ' + str(compile_errors))
        require(LIBRARY.save_loaded_asset(prototype, only_if_is_dirty=False), 'Cannot save project prototype')
    require(prototype.get_editor_property('used_with_instanced_static_meshes'), 'Prototype usage was not enabled')
    require(all(hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == value for name, value in hashes.items()), 'Original content changed')
    (OUTPUT / 'Author.json').write_text(json.dumps({'materials': report, 'prototype_instanced_static_meshes': True, 'original_hashes_preserved': len(hashes), 'gameplay_executed': False}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    unreal.log('Encounter surface authoring completed; original textures preserved.')


main()
