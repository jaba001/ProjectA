import json
from pathlib import Path

import unreal

SOURCE = "/Game/Primitive_Characters_Pack/Materials/Primitive_02/M_Primitive_02_01"
TARGET = "/Game/User_JeHoon/Assets/Primitive_Characters_Pack/Materials/Primitive_02/M_Primitive_02_Clothed"
OWNER_KEY = "ProjectA.RobeBodyMask"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)
    return condition


def main():
    library = unreal.EditorAssetLibrary
    if library.does_asset_exist(TARGET):
        material = library.load_asset(TARGET)
        require(library.get_metadata_tag(material, OWNER_KEY) == "1", "Refuse to overwrite an unowned body material")
        require(material.get_editor_property("blend_mode") == unreal.BlendMode.BLEND_MASKED, "Existing body mask changed blend mode")
        require(unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK) is not None, "Existing body mask has no opacity input")
        action = "verified_existing"
        if "-RobeBodyMaskUpdateCoverage" in unreal.SystemLibrary.get_command_line():
            mask = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_OPACITY_MASK)
            for expression in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, mask):
                if not isinstance(expression, unreal.MaterialExpressionVectorParameter):
                    continue
                name = str(expression.get_editor_property("parameter_name"))
                if name == "CoveredBodyMin":
                    expression.set_editor_property("default_value", unreal.LinearColor(-28.0, -40.0, 85.0, 1.0))
                elif name == "CoveredBodyMax":
                    expression.set_editor_property("default_value", unreal.LinearColor(28.0, 40.0, 152.0, 1.0))
            unreal.MaterialEditingLibrary.recompile_material(material)
            require(library.save_loaded_asset(material), "Cannot save the explicitly requested coverage adjustment")
            action = "updated_coverage"
    else:
        material = require(library.duplicate_asset(SOURCE, TARGET), "Cannot derive the original body shader")
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        material.set_editor_property("opacity_mask_clip_value", 0.5)
        edit = unreal.MaterialEditingLibrary
        position = edit.create_material_expression(material, unreal.MaterialExpressionPreSkinnedPosition, -800, 900)
        minimum = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, 1080)
        minimum.set_editor_property("parameter_name", "CoveredBodyMin")
        minimum.set_editor_property("default_value", unreal.LinearColor(-28.0, -40.0, 85.0, 1.0))
        maximum = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, 1260)
        maximum.set_editor_property("parameter_name", "CoveredBodyMax")
        maximum.set_editor_property("default_value", unreal.LinearColor(28.0, 40.0, 152.0, 1.0))
        mask = edit.create_material_expression(material, unreal.MaterialExpressionCustom, -400, 1000)
        mask.set_editor_property("description", "Hide the covered torso in bind-pose space / 바인드 포즈 공간에서 의상 아래 몸통 숨김")
        mask.set_editor_property("code", "float3 Inside = step(BoundsMin.xyz, Position) * step(Position, BoundsMax.xyz); return 1.0 - Inside.x * Inside.y * Inside.z;")
        mask.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inputs = []
        for name in ["Position", "BoundsMin", "BoundsMax"]:
            item = unreal.CustomInput()
            item.set_editor_property("input_name", name)
            inputs.append(item)
        mask.set_editor_property("inputs", inputs)
        for expression, name in [(position, "Position"), (minimum, "BoundsMin"), (maximum, "BoundsMax")]:
            require(edit.connect_material_expressions(expression, "", mask, name), "Cannot connect body mask input: " + name)
        require(edit.connect_material_property(mask, "", unreal.MaterialProperty.MP_OPACITY_MASK), "Cannot connect the authored opacity mask")
        library.set_metadata_tag(material, OWNER_KEY, "1")
        edit.recompile_material(material)
        require(library.save_loaded_asset(material), "Cannot save the project-owned body mask")
        action = "created"
    output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "RobeBodyMask"
    output.mkdir(parents=True, exist_ok=True)
    (output / "Author.json").write_text(json.dumps({"action": action, "source": SOURCE, "target": TARGET, "body_mesh_copied": False, "source_modified": False}, indent=2), encoding="utf-8")


main()
