import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUTPUT = ROOT / 'Saved/Automation/MazeTextures_20261011'
MATERIAL_ROOT = '/Game/User_JeHoon/Materials/Environment/Dungeon_Modular_V1/Materials'
SOURCE_ROOT = '/Game/Dungeon_Modular_V1/Textures/'
OWNER_TAG = 'ProjectAEncounterSurface'
EDITING = unreal.MaterialEditingLibrary
LIBRARY = unreal.EditorAssetLibrary
SPECS = [('M_EncounterStoneWall', 'T_Stone_Wall_01_Master', 160.0), ('M_EncounterStoneFloor', 'T_Floor_Flagstone_01', 240.0)]


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


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    protected = [SOURCE_ROOT + prefix + suffix for name, prefix, size in SPECS for suffix in ('_D', '_N', '_R', '_M')]
    protected += ['/Game/Dungeon_Modular_V1/Materials/' + name for name in ('M_Master_TwoSided_01', 'M_Stone_Wall_Master_01', 'M_Floor_Flagstone_01')]
    hashes = {str(package_file(path).relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(package_file(path).read_bytes()).hexdigest() for path in protected}
    (OUTPUT / 'SourceAssetsBefore.json').write_text(json.dumps(hashes, indent=2) + '\n', encoding='utf-8')
    report = []
    for name, prefix, size in SPECS:
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
        EDITING.set_base_material_usage(material, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        for expression in list(EDITING.get_material_expressions(material)):
            EDITING.delete_material_expression(material, expression)
        scale = node(material, unreal.MaterialExpressionVectorParameter, 'WorldTileSize', -1200, -300)
        scale.set_editor_property('parameter_name', 'WorldTileSize')
        scale.set_editor_property('default_value', unreal.LinearColor(size, size, size, 1.0))
        tint = node(material, unreal.MaterialExpressionVectorParameter, 'Tint', -450, -250)
        tint.set_editor_property('parameter_name', 'Tint')
        tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        diffuse = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_D'), 'Missing diffuse'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture', scale, 'Diffuse', -1200, 0)
        normal = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_N'), 'Missing normal'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal', scale, 'Normal', -1200, 350)
        roughness = projection(material, require(unreal.load_asset(SOURCE_ROOT + prefix + '_R'), 'Missing roughness'), '/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture', scale, 'Roughness', -1200, 700)
        base = node(material, unreal.MaterialExpressionMultiply, 'BaseColor', -200, 0)
        connect(diffuse, base, 'A', 'XYZ Texture')
        connect(tint, base, 'B')
        mask = node(material, unreal.MaterialExpressionComponentMask, 'RoughnessR', -400, 700)
        mask.set_editor_property('r', True)
        mask.set_editor_property('g', False)
        mask.set_editor_property('b', False)
        mask.set_editor_property('a', False)
        connect(roughness, mask, '', 'XYZ Texture')
        require(EDITING.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR), 'Base color connection failed')
        require(EDITING.connect_material_property(normal, 'XYZ Texture', unreal.MaterialProperty.MP_NORMAL), 'Normal connection failed')
        require(EDITING.connect_material_property(mask, '', unreal.MaterialProperty.MP_ROUGHNESS), 'Roughness connection failed')
        compile_errors = EDITING.recompile_material(material)
        require(not compile_errors, 'Material compilation failed: ' + str(compile_errors))
        require(material.get_editor_property('used_with_instanced_static_meshes'), 'Instanced mesh usage was not enabled')
        require(LIBRARY.save_loaded_asset(material, only_if_is_dirty=False), 'Cannot save owned material')
        report.append({'path': path, 'instanced_static_meshes': True, 'world_tile_size_cm': size, 'world_space_normal': True, 'texture_paths': [SOURCE_ROOT + prefix + suffix for suffix in ('_D', '_N', '_R')], 'expressions': len(EDITING.get_material_expressions(material)), 'compile_errors': list(compile_errors)})
    # The project-owned prototype still supplies mortar and NPC colors; persist its instancing support as well.
    # 프로젝트 소유 프로토타입은 줄눈·NPC 색상에 계속 사용하므로 인스턴싱 지원도 저장합니다.
    prototype_path = '/Game/User_JeHoon/Materials/Prototype/M_EncounterPrototype'
    prototype = require(unreal.load_asset(prototype_path), 'Missing project prototype')
    EDITING.set_base_material_usage(prototype, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    compile_errors = EDITING.recompile_material(prototype)
    require(not compile_errors, 'Prototype compilation failed: ' + str(compile_errors))
    require(prototype.get_editor_property('used_with_instanced_static_meshes'), 'Prototype usage was not enabled')
    require(LIBRARY.save_loaded_asset(prototype, only_if_is_dirty=False), 'Cannot save project prototype')
    require(all(hashlib.sha256((ROOT / name).read_bytes()).hexdigest() == value for name, value in hashes.items()), 'Original content changed')
    (OUTPUT / 'Author.json').write_text(json.dumps({'materials': report, 'prototype_instanced_static_meshes': True, 'original_hashes_preserved': len(hashes), 'gameplay_executed': False}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    unreal.log('Encounter surface authoring completed; original textures preserved.')


main()
