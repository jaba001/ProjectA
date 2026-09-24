import json
import re
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureWarriorContent import ASSETS, HELPER, TOOLS, load, require, save
from ConfigureWitchAssassin import attach_weapon, components
from WarriorContentPaths import ROOT, UNARMED_SOURCE

PACK = "/Game/ROG_Modular_Armor"
CATALOG_PATH = ROOT + "/ROG_Modular_Armor/DA_MannyAppearance"
BODY_MATERIAL_PATH = ROOT + "/ROG_Modular_Armor/Common/Materials/MI_MannyNeutral"
BODY_MATERIAL_PARENT = PACK + "/Common/Materials/M_Character_DEMO"
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
TOPDOWN = "/Game/TopDown/Blueprints/BP_TopDownCharacter"
BODY = PACK + "/Characters/UE5_Mannequins/Manny"
TABLES = PACK + "/Accessories_Robes/Blueprints/DataTables/UE5_Mannequins/Manny/DT_Manny_"
SLOTS = [("Head", "투구"), ("Torso", "상의 / 로브"), ("Legs", "바지"), ("Feet", "신발"), ("Hands", "장갑"), ("Shoulders", "어깨 장식"), ("Bracers", "손목 보호대"), ("Back", "망토")]
PROFESSIONS = [("Warrior", "BP_WarriorUnit", "BP_WarriorMenuPreview"), ("Mage", "BP_MageUnit", "BP_MageMenuPreview"), ("Archer", "BP_PlayerUnit", "BP_PartyMenuPreview"), ("Rogue", "BP_RogueUnit", "BP_RogueMenuPreview")]


def topdown_mesh():
    component = unreal.get_default_object(load(TOPDOWN).generated_class()).get_editor_property("mesh")
    # Slot names wait for skeletal mesh material compilation; component getters can otherwise return None.
    # 슬롯 이름 조회로 메시 재질 컴파일을 기다려 컴포넌트 조회가 None을 반환하는 것을 방지합니다.
    require(component.get_material_slot_names(), "TopDown body materials are unavailable")
    return component


def stored_vector(material, name):
    # Read authored values before asynchronous material parameter caches are ready.
    # 비동기 재질 파라미터 캐시 준비 전에도 저장된 설정값을 직접 읽습니다.
    values = [entry.get_editor_property("parameter_value") for entry in material.get_editor_property("vector_parameter_values") if str(entry.get_editor_property("parameter_info").get_editor_property("name")) == name]
    require(len(values) == 1, "Missing or ambiguous material vector: " + material.get_path_name() + " / " + name)
    return values[0]


def tag(name):
    result = unreal.GameplayTag()
    result.import_text('(TagName="' + name + '")')
    return result


def tags(names):
    result = unreal.GameplayTagContainer()
    result.import_text("(GameplayTags=(" + ",".join('(TagName="Appearance.Body.' + name + '")' for name in names) + "))")
    return result


def table(name):
    return json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(load(TABLES + name)))


def mesh_paths(value):
    return list(dict.fromkeys(re.findall(r"/Game/[^\"'\s)]+", value)))


def label(path, category):
    name = path.split(".")[0].rsplit("/", 1)[1]
    style = next((text for token, text in [("Heavy", "중갑"), ("Medium", "가죽"), ("Light", "천"), ("Hunt", "사냥꾼"), ("Barb", "야만 전사"), ("Necr", "강령술사")] if token in name), "")
    variant = re.search(r"_T(\d+)(?:_(\d+))?", name)
    number = " ".join(part for part in variant.groups() if part) if variant else ""
    color = next((text for token, text in [("dark_grey", "진회색"), ("black", "검정"), ("green", "초록"), ("white", "흰색"), ("yellow", "노랑"), ("red", "빨강"), ("rich", "문양"), ("crow", "까마귀"), ("fox", "여우")] if token in name.lower()), "")
    shape = re.search(r"(?:Cloak|Chest|Gloves)(\d+)", name)
    design = (shape.group(1) + "-" if shape else "") + number
    return " ".join(part for part in [style, category, design] if part) + (" · " + color if color else "")


def create_body_material():
    if ASSETS.does_asset_exist(BODY_MATERIAL_PATH):
        material = load(BODY_MATERIAL_PATH)
    else:
        material = require(TOOLS.create_asset(BODY_MATERIAL_PATH.rsplit("/", 1)[1], BODY_MATERIAL_PATH.rsplit("/", 1)[0], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew()), "Could not create neutral body material")
    editor = unreal.MaterialEditingLibrary
    editor.set_material_instance_parent(material, load(BODY_MATERIAL_PARENT))
    color = stored_vector(topdown_mesh().get_material(0), "Paint Tint")
    # UE 5.7 setters return false even after updating; verify the stored values instead.
    # UE 5.7 설정 함수는 갱신 후에도 false를 반환하므로 저장된 값으로 확인합니다.
    editor.set_material_instance_vector_parameter_value(material, "SolidColor", color)
    editor.set_material_instance_scalar_parameter_value(material, "Roughness", 0.4)
    actual = stored_vector(material, "SolidColor")
    require(all(abs(getattr(actual, channel) - getattr(color, channel)) < 0.00001 for channel in ["r", "g", "b", "a"]), "Could not set the neutral body color")
    require(abs(editor.get_material_instance_scalar_parameter_value(material, "Roughness") - 0.4) < 0.00001, "Could not set the body roughness")
    editor.update_material_instance(material)
    save(material)
    return material


def create_catalog():
    if ASSETS.does_asset_exist(CATALOG_PATH):
        catalog = load(CATALOG_PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.CharacterAppearanceCatalog)
        catalog = require(TOOLS.create_asset(CATALOG_PATH.rsplit("/", 1)[1], CATALOG_PATH.rsplit("/", 1)[0], unreal.CharacterAppearanceCatalog, factory), "Could not create appearance catalogue")
    slots = []
    for name, title in SLOTS:
        slot = unreal.CharacterAppearanceSlot()
        slot.set_editor_property("slot_tag", tag("Appearance.Slot." + name))
        slot.set_editor_property("display_name", title)
        slots.append(slot)
    body_parts = []
    body_material = create_body_material()
    for name, suffix in [("Head", "Head"), ("Torso", "Chest"), ("Arms", "Arms"), ("Hands", "Hands"), ("Legs", "Legs"), ("Feet", "Feet")]:
        part = unreal.CharacterAppearanceBodyPart()
        part.set_editor_property("part_tag", tag("Appearance.Body." + name))
        body_mesh = load(BODY + "/Body_parts/SK_Manny_" + suffix)
        part.set_editor_property("mesh", body_mesh)
        part.set_editor_property("material_overrides", [body_material])
        body_parts.append(part)
    expected_skeleton = load(BODY + "/SK_Manny").get_editor_property("skeleton")
    items = []
    excluded = []
    titles = {}

    def add(slot, category, paths, hidden):
        if not paths:
            return
        meshes = [load(path) for path in paths]
        if any(mesh.get_editor_property("skeleton") != expected_skeleton for mesh in meshes):
            excluded.extend(paths)
            return
        item = unreal.CharacterAppearanceItem()
        item_id = "ROG_Manny_" + meshes[0].get_name().removeprefix("SK_").removeprefix("L_")
        title = label(paths[0], category)
        count = titles.get((slot, title), 0) + 1
        titles[(slot, title)] = count
        item.set_editor_property("item_id", item_id)
        item.set_editor_property("display_name", title + (" (" + str(count) + ")" if count > 1 else ""))
        item.set_editor_property("slot_tag", tag("Appearance.Slot." + slot))
        item.set_editor_property("meshes", meshes)
        item.set_editor_property("hidden_body_parts", tags(hidden))
        items.append(item)

    armor = table("AdditionalArmor")[0]
    for key, slot, title, hidden in [("Helmet", "Head", "투구", ["Head"]), ("Chest", "Torso", "갑옷", ["Torso", "Arms"]), ("Pants", "Legs", "바지", ["Legs"]), ("Boots", "Feet", "장화", ["Feet"])]:
        add(slot, title, mesh_paths(armor[key]), hidden)
    for name, slot, field, title in [("Robes", "Torso", "RobesMesh", "로브"), ("Gloves", "Hands", "GlovesMesh", "장갑"), ("Shoulders", "Shoulders", "ShouldersMesh", "어깨 장식"), ("Cloaks", "Back", "CloaksMesh", "망토"), ("Bracers", "Bracers", "", "손목 보호대")]:
        for row in table(name):
            paths = mesh_paths(row[field]) if field else mesh_paths(row["LeftBracer"]) + mesh_paths(row["RightBracer"])
            hidden = []
            if name == "Robes":
                hidden = ["Torso"] + (["Arms"] if row["RobeType"] == "LongSleeve" else [])
            elif name == "Gloves" and row["GlovesType"] != "Open":
                hidden = ["Hands"]
            add(slot, title, paths, hidden)
    catalog.set_editor_property("slots", slots)
    catalog.set_editor_property("items", items)
    catalog.set_editor_property("body_parts", body_parts)
    save(catalog)
    return catalog, excluded


def staff_grip():
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", load(MANNY))
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(load(UNARMED_SOURCE + "/MM_Idle"), 0.0, options)
    hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD)
    thumb = unreal.AnimPoseExtensions.get_bone_pose(pose, "thumb_02_l", unreal.AnimPoseSpaces.WORLD).translation
    pinky = unreal.AnimPoseExtensions.get_bone_pose(pose, "pinky_02_l", unreal.AnimPoseSpaces.WORLD).translation
    middle = unreal.AnimPoseExtensions.get_bone_pose(pose, "middle_01_l", unreal.AnimPoseSpaces.WORLD).translation
    axis = thumb - pinky
    if axis.z < 0.0:
        axis *= -1.0
    desired = unreal.Transform(location=(thumb + pinky) * 0.5, rotation=unreal.MathLibrary.make_rot_from_zx(axis, middle - hand.translation))
    desired.translation -= unreal.MathLibrary.transform_direction(desired, unreal.Vector(-0.476373, 0.000402, 0.0))
    return unreal.MathLibrary.make_relative_transform(desired, hand)


def configure_profession(profession, unit_name, preview_name, catalog):
    source_blueprint = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit")
    source = unreal.get_default_object(source_blueprint.generated_class())
    source_mesh = source.get_editor_property("mesh")
    original = topdown_mesh()
    source_sword = next(component for handle, component, name in components(source_blueprint) if name == "Sword")
    grip = unreal.Transform(location=source_sword.get_editor_property("relative_location"), rotation=source_sword.get_editor_property("relative_rotation"), scale=source_sword.get_editor_property("relative_scale3d"))
    sword_mesh = source_sword.get_editor_property("static_mesh")
    sword_socket = str(HELPER.get_weapon_attachment(source_blueprint, "Sword"))
    common_mesh_settings = {field: source_mesh.get_editor_property(field) for field in ["anim_class", "relative_location", "relative_rotation", "relative_scale3d", "physics_asset_override"]}
    common_montages = dict(source.get_editor_property("round_montage_overrides"))
    for blueprint_name in [unit_name, "BP_" + profession + "SnapshotOpponent"]:
        blueprint = load(ROOT + "/Blueprint/Unit/" + blueprint_name)
        defaults = unreal.get_default_object(blueprint.generated_class())
        skills = list(defaults.get_editor_property("equipped_skill_data_assets"))
        mesh = defaults.get_editor_property("mesh")
        for field, value in common_mesh_settings.items():
            mesh.set_editor_property(field, value)
        mesh.set_editor_property("skeletal_mesh_asset", original.get_editor_property("skeletal_mesh_asset"))
        mesh.set_editor_property("override_materials", [original.get_material(index) for index in range(original.get_num_materials())])
        defaults.set_editor_property("round_montage_overrides", common_montages)
        appearance = defaults.get_editor_property("character_appearance")
        appearance.set_editor_property("appearance_catalog", catalog)
        appearance.set_editor_property("selection", unreal.CharacterAppearanceSelection())
        appearance.set_editor_property("hidden_mesh_bones", [])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        attach_weapon(blueprint, "Sword", sword_mesh, sword_socket, grip)
        if profession == "Mage":
            attach_weapon(blueprint, "Staff", load("/Game/MageStaff_FreeWeapons/SM_Staff_01"), "hand_l", staff_grip())
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
        require(list(unreal.get_default_object(blueprint.generated_class()).get_editor_property("equipped_skill_data_assets")) == skills, "Existing profession skills changed: " + profession)
    preview = load(ROOT + "/Blueprint/UI/" + preview_name)
    mesh = unreal.get_default_object(preview.generated_class()).get_editor_property("skeletal_mesh_component")
    mesh.set_editor_property("skeletal_mesh_asset", load(MANNY))
    mesh.set_editor_property("override_materials", [original.get_material(index) for index in range(original.get_num_materials())])
    mesh.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    mesh.override_animation_data(load(UNARMED_SOURCE + "/MM_Idle"), True, True, 0.0, 1.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(preview)
    if profession == "Mage":
        attach_weapon(preview, "Staff", load("/Game/MageStaff_FreeWeapons/SM_Staff_01"), "hand_l", staff_grip())
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
    save(preview)
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    professions = dict(party.get_editor_property("professions"))
    definition = professions[unreal.Name(profession)]
    definition.set_editor_property("appearance_catalog", catalog)
    professions[unreal.Name(profession)] = definition
    party.set_editor_property("professions", professions)
    save(party)
    snapshots = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    classes = dict(snapshots.get_editor_property("enemy_classes"))
    replacement = load(ROOT + "/Blueprint/Unit/BP_" + profession + "SnapshotOpponent").generated_class()
    legacy = dict(snapshots.get_editor_property("legacy_enemy_classes"))
    if classes.get(unreal.Name(profession)) and classes[unreal.Name(profession)] != replacement:
        legacy[unreal.Name(profession)] = classes[unreal.Name(profession)]
    snapshots.set_editor_property("legacy_enemy_classes", legacy)
    classes[unreal.Name(profession)] = replacement
    snapshots.set_editor_property("enemy_classes", classes)
    save(snapshots)


def configure():
    catalog, excluded = create_catalog()
    for profession, unit_name, preview_name in PROFESSIONS:
        configure_profession(profession, unit_name, preview_name, catalog)
    report = {"catalog": catalog.get_path_name(), "professions": [entry[0] for entry in PROFESSIONS], "base_model": topdown_mesh().get_editor_property("skeletal_mesh_asset").get_path_name(), "slots": len(catalog.get_editor_property("slots")), "items": len(catalog.get_editor_property("items")), "excluded_incompatible_meshes": excluded, "source_assets_copied": 0, "gameplay_test": "not run"}
    Path(unreal.Paths.project_saved_dir(), "Automation/RogAppearanceConfigure.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("ROG_APPEARANCE_CONFIGURED")


if __name__ == "__main__":
    configure()
