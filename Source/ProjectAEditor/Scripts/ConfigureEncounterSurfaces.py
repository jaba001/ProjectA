import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
CEILING_ONLY = '-EncounterCeilingOnly' in unreal.SystemLibrary.get_command_line()
MASONRY_ONLY = '-EncounterMasonryOnly' in unreal.SystemLibrary.get_command_line()
STONE_COLOR_ONLY = '-EncounterStoneColorOnly' in unreal.SystemLibrary.get_command_line()
STONE_DETAIL_ONLY = '-EncounterStoneDetailOnly' in unreal.SystemLibrary.get_command_line()
# Stone relief: stronger normals, wider roughness, chiselled block edges and large-scale tone variation.
# 석재 질감: 더 강한 노말, 넓은 거칠기 범위, 깎인 블록 모서리와 큰 단위 명암 변화를 적용합니다.
NORMAL_STRENGTH = 1.1
ROUGHNESS_MIN = 0.6
SPECULAR = 0.25
EDGE_WIDTH = 0.07
EDGE_DARKEN = 0.5
MACRO_TILE_CM = 900.0
MACRO_RANGE = (0.85, 1.2)
OUTPUT = ROOT / ('Saved/Automation/EncounterStoneDetail_20261012' if STONE_DETAIL_ONLY else 'Saved/Automation/EncounterStoneColor_20261012' if STONE_COLOR_ONLY else 'Saved/Automation/EncounterMasonry_20261011' if MASONRY_ONLY else 'Saved/Automation/EncounterCeiling_20261011' if CEILING_ONLY else 'Saved/Automation/MazeTextures_20261011')
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


def scalar(material, name, value, x, y):
    result = node(material, unreal.MaterialExpressionScalarParameter, name, x, y)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result


def edge_wear(material, base):
    # Each instanced cube face keeps 0..1 UVs, so distance to the nearest UV border marks the block edge.
    # 인스턴스 큐브의 각 면은 0..1 UV를 유지하므로 가장 가까운 UV 경계까지의 거리로 블록 모서리를 찾습니다.
    coords = node(material, unreal.MaterialExpressionTextureCoordinate, 'EdgeUV', -1400, 1100)
    axes = []
    for channel, y in (('r', 1100), ('g', 1250)):
        mask = node(material, unreal.MaterialExpressionComponentMask, 'EdgeAxis' + channel.upper(), -1200, y)
        for name in 'rgba':
            mask.set_editor_property(name, name == channel)
        connect(coords, mask, '')
        inverse = node(material, unreal.MaterialExpressionOneMinus, 'EdgeAxisInverse' + channel.upper(), -1000, y + 60)
        connect(mask, inverse, '')
        nearest = node(material, unreal.MaterialExpressionMin, 'EdgeAxisNearest' + channel.upper(), -800, y)
        connect(mask, nearest, 'A')
        connect(inverse, nearest, 'B')
        axes.append(nearest)
    distance = node(material, unreal.MaterialExpressionMin, 'EdgeDistance', -600, 1150)
    connect(axes[0], distance, 'A')
    connect(axes[1], distance, 'B')
    width = scalar(material, 'EdgeWidth', EDGE_WIDTH, -600, 1300)
    ratio = node(material, unreal.MaterialExpressionDivide, 'EdgeRatio', -400, 1150)
    connect(distance, ratio, 'A')
    connect(width, ratio, 'B')
    inside = node(material, unreal.MaterialExpressionSaturate, 'EdgeInside', -250, 1150)
    connect(ratio, inside, '')
    darken = scalar(material, 'EdgeDarken', EDGE_DARKEN, -250, 1300)
    factor = node(material, unreal.MaterialExpressionLinearInterpolate, 'EdgeFactor', -100, 1150)
    connect(darken, factor, 'A')
    one = node(material, unreal.MaterialExpressionConstant, 'EdgeOne', -250, 1400)
    one.set_editor_property('r', 1.0)
    connect(one, factor, 'B')
    connect(inside, factor, 'Alpha')
    result = node(material, unreal.MaterialExpressionMultiply, 'EdgeWornColor', 100, 1000)
    connect(base, result, 'A')
    connect(factor, result, 'B')
    return result


def macro_variation(material, base, horizontal):
    # A large second stone sample breaks visible repetition without new textures.
    # 두 번째 석재 텍스처를 크게 샘플링하여 새 텍스처 없이 반복 무늬를 줄입니다.
    world = node(material, unreal.MaterialExpressionWorldPosition, 'MacroWorldPosition', -1400, 1550)
    world.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    if horizontal:
        plane = node(material, unreal.MaterialExpressionComponentMask, 'MacroPlane', -1200, 1550)
        for name in 'rgba':
            plane.set_editor_property(name, name in 'rg')
        connect(world, plane, '', 'XYZ')
    else:
        masks = {}
        for channel in 'rgb':
            mask = node(material, unreal.MaterialExpressionComponentMask, 'MacroAxis' + channel.upper(), -1200, 1550 + 'rgb'.index(channel) * 120)
            for name in 'rgba':
                mask.set_editor_property(name, name == channel)
            connect(world, mask, '', 'XYZ')
            masks[channel] = mask
        along = node(material, unreal.MaterialExpressionAdd, 'MacroAlongWall', -1000, 1550)
        connect(masks['r'], along, 'A')
        connect(masks['g'], along, 'B')
        plane = node(material, unreal.MaterialExpressionAppendVector, 'MacroPlane', -850, 1600)
        connect(along, plane, 'A')
        connect(masks['b'], plane, 'B')
    size = scalar(material, 'MacroTileSize', MACRO_TILE_CM, -850, 1750)
    uv = node(material, unreal.MaterialExpressionDivide, 'MacroUV', -700, 1600)
    connect(plane, uv, 'A')
    connect(size, uv, 'B')
    sample = node(material, unreal.MaterialExpressionTextureSample, 'MacroSample', -550, 1600)
    texture = require(unreal.load_asset(SOURCE_ROOT + 'T_Stone_Wall_02_Master_D'), 'Missing macro stone texture')
    sample.set_editor_property('texture', texture)
    sample.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    sample.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    connect(uv, sample, '')
    weights = node(material, unreal.MaterialExpressionConstant3Vector, 'MacroLuminanceWeights', -550, 1800)
    weights.set_editor_property('constant', unreal.LinearColor(0.3, 0.59, 0.11, 1.0))
    luminance = node(material, unreal.MaterialExpressionDotProduct, 'MacroLuminance', -350, 1650)
    connect(sample, luminance, 'A', 'RGB')
    connect(weights, luminance, 'B')
    low = scalar(material, 'MacroDark', MACRO_RANGE[0], -350, 1800)
    high = scalar(material, 'MacroLight', MACRO_RANGE[1], -350, 1900)
    factor = node(material, unreal.MaterialExpressionLinearInterpolate, 'MacroFactor', -150, 1700)
    connect(low, factor, 'A')
    connect(high, factor, 'B')
    connect(luminance, factor, 'Alpha')
    result = node(material, unreal.MaterialExpressionMultiply, 'MacroToneColor', 100, 1300)
    connect(base, result, 'A')
    connect(factor, result, 'B')
    return result


def surface_properties(material, diffuse, sampled_normal, sampled_roughness, horizontal, wall=False):
    base = macro_variation(material, edge_wear(material, stone_palette(material, diffuse, '' if horizontal else 'XYZ Texture')), horizontal and not wall)
    flat = node(material, unreal.MaterialExpressionConstant3Vector if horizontal else unreal.MaterialExpressionVertexNormalWS, 'FlatNormal', -450, 300)
    if horizontal:
        flat.set_editor_property('constant', unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    strength = node(material, unreal.MaterialExpressionScalarParameter, 'NormalStrength', -450, 500)
    strength.set_editor_property('parameter_name', 'NormalStrength')
    strength.set_editor_property('default_value', NORMAL_STRENGTH)
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
    roughness.set_editor_property('min_default', ROUGHNESS_MIN)
    roughness.set_editor_property('max_default', 1.0)
    connect(mask, roughness, '')
    specular = node(material, unreal.MaterialExpressionConstant, 'StoneSpecular', -200, 900)
    specular.set_editor_property('r', SPECULAR)
    for expression, prop in [(base, unreal.MaterialProperty.MP_BASE_COLOR), (normal, unreal.MaterialProperty.MP_NORMAL), (roughness, unreal.MaterialProperty.MP_ROUGHNESS), (specular, unreal.MaterialProperty.MP_SPECULAR)]:
        require(EDITING.connect_material_property(expression, '', prop), 'Surface property connection failed')


def planar_coordinates(material, wall, x, y):
    # Floors and ceilings project world XY; walls only face X or Y, so (X+Y, Z) runs along every wall.
    # 바닥·천장은 월드 XY를, X·Y 방향뿐인 벽은 (X+Y, Z)를 투영하여 모든 벽면을 따라갑니다.
    world = node(material, unreal.MaterialExpressionWorldPosition, 'PlanarWorldPosition', x - 200, y)
    world.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    if not wall:
        plane = node(material, unreal.MaterialExpressionComponentMask, 'PlanarXY', x, y)
        for name in 'rgba':
            plane.set_editor_property(name, name in 'rg')
        connect(world, plane, '', 'XYZ')
        return plane
    masks = {}
    for channel in 'rgb':
        mask = node(material, unreal.MaterialExpressionComponentMask, 'PlanarAxis' + channel.upper(), x, y + 'rgb'.index(channel) * 120)
        for name in 'rgba':
            mask.set_editor_property(name, name == channel)
        connect(world, mask, '', 'XYZ')
        masks[channel] = mask
    along = node(material, unreal.MaterialExpressionAdd, 'PlanarAlongWall', x + 150, y)
    connect(masks['r'], along, 'A')
    connect(masks['g'], along, 'B')
    plane = node(material, unreal.MaterialExpressionAppendVector, 'PlanarWallPlane', x + 300, y + 60)
    connect(along, plane, 'A')
    connect(masks['b'], plane, 'B')
    return plane


def configure_horizontal(material, prefix, size, ceiling, wall=False):
    # Keep floor and ceiling textures level and use each mesh face's tangent basis.
    # 바닥·천장 텍스처를 수평으로 투영하고 각 메시 면의 접선 기저를 사용합니다.
    material.set_editor_property('tangent_space_normal', True)
    material.set_editor_property('two_sided', ceiling)
    xy = planar_coordinates(material, wall, -1400, 0)
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
    surface_properties(material, samples['Diffuse'], samples['Normal'], samples['Roughness'], True, wall)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    require(sum((CEILING_ONLY, MASONRY_ONLY, STONE_COLOR_ONLY, STONE_DETAIL_ONLY)) <= 1, 'Select only one authoring mode')
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
        # Every surface now uses planar projection with tangent-space normals; the old world-aligned wall graph flipped normals once strengthened.
        # 모든 면이 평면 투영과 접선 공간 노말을 사용합니다. 이전 월드 정렬 벽 그래프는 노말을 강화하면 일부 면이 뒤집혔습니다.
        configure_horizontal(material, prefix, size, name == 'M_EncounterStoneCeiling', name == 'M_EncounterStoneWall')
        EDITING.delete_unused_expressions(material)
        compile_errors = EDITING.recompile_material(material)
        require(not compile_errors, 'Surface compilation failed: ' + str(compile_errors))
        require(material.get_editor_property('used_with_instanced_static_meshes'), 'Surface instancing support missing')
        require(material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_OPAQUE and material.get_editor_property('two_sided') == (name == 'M_EncounterStoneCeiling'), 'Invalid horizontal surface settings')
        require(LIBRARY.save_loaded_asset(material, only_if_is_dirty=False), 'Cannot save surface material')
        report.append({'path': path, 'instanced_static_meshes': True, 'opaque': True, 'two_sided': name == 'M_EncounterStoneCeiling', 'tangent_space_normal': True, 'normal_strength': NORMAL_STRENGTH, 'roughness_min': ROUGHNESS_MIN, 'specular': SPECULAR, 'edge_width_uv': EDGE_WIDTH, 'edge_darken': EDGE_DARKEN, 'macro_tile_cm': MACRO_TILE_CM, 'macro_range': list(MACRO_RANGE), 'diffuse_rgb_preserved': True, 'diffuse_tint': [0.65, 0.60, 0.52], 'tile_size_cm': size, 'texture_paths': [SOURCE_ROOT + prefix + suffix for suffix in ('_D', '_N', '_R')], 'compile_errors': list(compile_errors)})
    # The project-owned prototype still supplies mortar and NPC colors; persist its instancing support as well.
    # 프로젝트 소유 프로토타입은 줄눈·NPC 색상에 계속 사용하므로 인스턴싱 지원도 저장합니다.
    prototype_path = '/Game/User_JeHoon/Materials/Prototype/M_EncounterPrototype'
    prototype = require(unreal.load_asset(prototype_path), 'Missing project prototype')
    if not CEILING_ONLY and not MASONRY_ONLY and not STONE_COLOR_ONLY and not STONE_DETAIL_ONLY:
        EDITING.set_base_material_usage(prototype, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        compile_errors = EDITING.recompile_material(prototype)
        require(not compile_errors, 'Prototype compilation failed: ' + str(compile_errors))
        require(LIBRARY.save_loaded_asset(prototype, only_if_is_dirty=False), 'Cannot save project prototype')
    require(prototype.get_editor_property('used_with_instanced_static_meshes'), 'Prototype usage was not enabled')
    require(all(hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == value for name, value in hashes.items()), 'Original content changed')
    (OUTPUT / 'Author.json').write_text(json.dumps({'materials': report, 'prototype_instanced_static_meshes': True, 'original_hashes_preserved': len(hashes), 'gameplay_executed': False}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    unreal.log('Encounter surface authoring completed; original textures preserved.')


main()
