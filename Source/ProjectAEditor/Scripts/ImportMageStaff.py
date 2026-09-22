import json
from pathlib import Path

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = PROJECT / "Content/Mage Staff - Free Weapons"
DESTINATION = "/Game/MageStaff_FreeWeapons"
MESH_PATH = DESTINATION + "/SM_Staff_01"
MATERIAL_PATH = DESTINATION + "/Materials/M_Staff_01"
TEXTURE_SOURCE = SOURCE / "Textures/Unreal packes"
TEXTURE_DESTINATION = DESTINATION + "/Textures/Unreal_packes"
LIBRARY = unreal.EditorAssetLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def save_owned(asset):
    require(asset.get_path_name().startswith(DESTINATION + "/"), "Refusing to save outside staff import folder: " + asset.get_path_name())
    require(LIBRARY.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save " + asset.get_path_name())


def import_asset(source, destination, asset_type, factory, options=None):
    if LIBRARY.does_asset_exist(destination):
        asset = require(unreal.load_asset(destination), "Could not load " + destination)
    else:
        require(source.is_file(), "Missing staff source: " + str(source))
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", destination.rsplit("/", 1)[0])
        task.set_editor_property("destination_name", destination.rsplit("/", 1)[1])
        task.set_editor_property("automated", True)
        task.set_editor_property("async_", False)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        task.set_editor_property("factory", factory)
        if options:
            task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        imported = task.get_editor_property("imported_object_paths")
        require(len(imported) == 1 and imported[0].split(".")[0] == destination, "Unexpected staff import outputs: " + str(imported))
        asset = require(unreal.load_asset(destination), "Missing imported staff asset: " + destination)
    require(isinstance(asset, asset_type), "Unexpected staff asset type: " + destination)
    filenames = asset.get_editor_property("asset_import_data").extract_filenames()
    require(len(filenames) == 1 and Path(filenames[0]).resolve() == source.resolve(), "Staff source file mismatch: " + destination)
    return asset


def import_mesh():
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_translation", unreal.Vector(0.0, 0.0, 0.0))
    data.set_editor_property("import_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("convert_scene_unit", True)
    return import_asset(SOURCE / "SM_Staff_01.fbx", MESH_PATH, unreal.StaticMesh, unreal.FbxFactory(), options)


def import_texture(suffix, compression, srgb):
    name = "Elven_staff_DefaultMaterial_" + suffix
    texture = import_asset(TEXTURE_SOURCE / (name + ".png"), TEXTURE_DESTINATION + "/" + name, unreal.Texture2D, unreal.TextureFactory())
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("srgb", srgb)
    save_owned(texture)
    return texture


def configure_material(base_color, normal, orm):
    material = unreal.load_asset(MATERIAL_PATH) if LIBRARY.does_asset_exist(MATERIAL_PATH) else unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Staff_01", DESTINATION + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    require(isinstance(material, unreal.Material), "Could not load or create the staff material")
    editing = unreal.MaterialEditingLibrary
    editing.delete_all_material_expressions(material)
    samples = []
    for index, (texture, sampler_type) in enumerate([(base_color, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR), (normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL), (orm, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]):
        sample = require(editing.create_material_expression(material, unreal.MaterialExpressionTextureSample, -450, index * 300), "Could not create a staff texture sample")
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", sampler_type)
        samples.append(sample)
    require(editing.connect_material_property(samples[0], "RGB", unreal.MaterialProperty.MP_BASE_COLOR), "Could not connect staff base color")
    require(editing.connect_material_property(samples[1], "RGB", unreal.MaterialProperty.MP_NORMAL), "Could not connect staff normal")
    # The supplied Unreal ORM texture packs occlusion, roughness, and metallic into R, G, and B.
    # 제공된 Unreal ORM 텍스처는 R, G, B에 오클루전, 거칠기, 금속성을 저장합니다.
    for output, material_property in [("R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("G", unreal.MaterialProperty.MP_ROUGHNESS), ("B", unreal.MaterialProperty.MP_METALLIC)]:
        require(editing.connect_material_property(samples[2], output, material_property), "Could not connect staff ORM channel " + output)
    editing.recompile_material(material)
    save_owned(material)
    return material


def describe_bounds(mesh):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    dimensions = [extent.x * 2.0, extent.y * 2.0, extent.z * 2.0]
    require(max(dimensions) > 0.0, "Imported staff has empty bounds")
    # Bounds describe the imported local axes; hand attachment transforms are authored separately.
    # 경계는 임포트된 로컬 축을 나타내며 손 부착 Transform은 별도로 설정합니다.
    return {"asset": mesh.get_path_name(), "source": str(SOURCE / "SM_Staff_01.fbx"), "local_bounds_min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z], "local_bounds_max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z], "dimensions_cm": dimensions, "longest_local_axis": "XYZ"[dimensions.index(max(dimensions))], "import_rotation_degrees": [0.0, 0.0, 0.0], "convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False}


def configure():
    # Import source files directly; package names sanitize spaces without duplicating the source pack.
    # 원본 파일을 직접 임포트하며 팩을 복제하지 않고 패키지 이름의 공백만 정리합니다.
    mesh = import_mesh()
    base_color = import_texture("BaseColor", unreal.TextureCompressionSettings.TC_DEFAULT, True)
    normal = import_texture("Normal", unreal.TextureCompressionSettings.TC_NORMALMAP, False)
    orm = import_texture("OcclusionRoughnessMetallic", unreal.TextureCompressionSettings.TC_MASKS, False)
    material = configure_material(base_color, normal, orm)
    materials = mesh.get_editor_property("static_materials")
    require(materials, "Imported staff has no material slots")
    for index in range(len(materials)):
        mesh.set_material(index, material)
    save_owned(mesh)
    unreal.log("MAGE_STAFF_IMPORT " + json.dumps(describe_bounds(mesh), sort_keys=True))
    return mesh


if __name__ == "__main__":
    configure()
