import json
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureRogAppearance import CATALOG_PATH, MANNY, PROFESSIONS, ROOT, UNARMED_SOURCE, load, require, save
from ConfigureWarriorContent import ASSETS, HELPER
from ConfigureWitchAssassin import components

BODIES = [("Male", "남자", "/Game/Primitive_Characters_Pack/Mesh/Primitive_01/Mesh_UE5/Separate/SKM_Primitive_Charater_01_Body"), ("Female", "여자", "/Game/Primitive_Characters_Pack/Mesh/Primitive_02/Mesh_UE5/Separate/SKM_Primitive_02_Body")]


def mesh_transform(component):
    return unreal.Transform(location=component.get_editor_property("relative_location"), rotation=component.get_editor_property("relative_rotation"), scale=unreal.Vector(1.0, 1.0, 1.0))


def clear_default_staff(blueprint):
    # Keep the attachment point and original equipment assets for future item use.
    # 향후 아이템 사용을 위해 부착 지점과 원본 장비 에셋은 보존합니다.
    for handle, component, name in components(blueprint):
        if name == "Staff":
            require(isinstance(component, unreal.StaticMeshComponent), "Unexpected Staff component type")
            component.set_static_mesh(None)
            component.set_editor_property("override_materials", [])
            component.set_visibility(False)
            component.set_hidden_in_game(True)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def configure_mage_defaults():
    paths = [ROOT + "/Blueprint/Unit/BP_MageUnit", ROOT + "/Blueprint/Unit/BP_MageSnapshotOpponent", ROOT + "/Blueprint/UI/BP_MageMenuPreview"]
    for path in paths:
        blueprint = load(path)
        clear_default_staff(blueprint)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
    report = {"blueprints": paths, "default_staffs": 0, "source_assets_copied": 0, "gameplay_test": "not run"}
    Path(unreal.Paths.project_saved_dir(), "Automation/MageDefaultStaffConfigure.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MAGE_DEFAULT_STAFF_REMOVED")


def configure_preview_facing():
    catalog = load(CATALOG_PATH)
    variants = list(catalog.get_editor_property("body_variants"))
    for variant in variants:
        if str(variant.body_id) in {body[0] for body in BODIES}:
            transform = variant.preview_mesh_transform
            transform.rotation = unreal.Rotator().quaternion()
            variant.set_editor_property("preview_mesh_transform", transform)
    catalog.set_editor_property("body_variants", variants)
    save(catalog)
    # Slot anchors own the front-facing yaw; root meshes must not apply the same correction twice.
    # 정면 Yaw는 슬롯 앵커가 담당하므로 루트 메시에서 같은 보정을 중복 적용하지 않습니다.
    for profession, unit_name, preview_name in PROFESSIONS:
        blueprint = load(ROOT + "/Blueprint/UI/" + preview_name)
        mesh = unreal.get_default_object(blueprint.generated_class()).get_editor_property("skeletal_mesh_component")
        mesh.set_editor_property("relative_rotation", unreal.Rotator())
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
    world = require(unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/MainMenu"), "Missing MainMenu map")
    stages = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage)]
    require(len(stages) == 1, "MainMenu requires one preview stage")
    stage = stages[0]
    camera = stage.get_editor_property("preview_camera")
    location = camera.get_editor_property("relative_location")
    location.x = -500.0
    camera.set_editor_property("relative_location", location)
    for index in range(4):
        stage.get_editor_property("slot%d_anchor" % index).set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT + "/LEVEL/MainMenu"), "Could not save preview facing")
    unreal.log("PRIMITIVE_PREVIEW_FACING_CONFIGURED")


def configure():
    catalog = load(CATALOG_PATH)
    preserved = {field: [entry.export_text() for entry in catalog.get_editor_property(field)] for field in ["slots", "items", "body_parts"]}
    source_skeleton = load(MANNY).get_editor_property("skeleton")
    animation = load(UNARMED_SOURCE + "/ABP_Unarmed").generated_class()
    idle = load(UNARMED_SOURCE + "/MM_Idle")
    unit = unreal.get_default_object(load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class()).get_editor_property("mesh")
    preview = unreal.get_default_object(load(ROOT + "/Blueprint/UI/BP_PartyMenuPreview").generated_class()).get_editor_property("skeletal_mesh_component")
    preview_transform = mesh_transform(preview)
    preview_transform.rotation = unreal.Rotator().quaternion()
    variants = []
    for body_id, title, path in BODIES:
        mesh = load(path)
        require(unreal.CharacterAppearanceAssetLibrary.configure_body_animation_skeleton(mesh, source_skeleton), "Could not configure compatible body animations: " + path)
        require(HELPER.validate_character_physics(mesh), "Invalid original body physics: " + path)
        skeleton = mesh.get_editor_property("skeleton")
        # Register Unreal's animation compatibility without copying meshes, skeletons, or animation assets.
        # 메시·뼈대·애니메이션을 복제하지 않고 Unreal의 애니메이션 호환 설정을 등록합니다.
        require(ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=True), "Could not save the original skeleton compatibility")
        variant = unreal.CharacterAppearanceBodyVariant()
        for field, value in {"body_id": body_id, "display_name": title, "mesh": mesh, "animation_class": animation, "preview_animation": idle, "mesh_transform": mesh_transform(unit), "preview_mesh_transform": preview_transform}.items():
            variant.set_editor_property(field, value)
        variants.append(variant)
    known_ids = {entry[0] for entry in BODIES}
    variants.extend(entry for entry in catalog.get_editor_property("body_variants") if str(entry.body_id) not in known_ids)
    catalog.set_editor_property("body_variants", variants)
    catalog.set_editor_property("default_body_id", "Male")
    catalog.set_editor_property("enable_outfits", False)
    save(catalog)
    require(preserved == {field: [entry.export_text() for entry in catalog.get_editor_property(field)] for field in preserved}, "Preserved outfit catalogue definitions changed")
    male = load(BODIES[0][2])
    for profession, unit_name, preview_name in PROFESSIONS:
        for name in [unit_name, "BP_" + profession + "SnapshotOpponent"]:
            blueprint = load(ROOT + "/Blueprint/Unit/" + name)
            defaults = unreal.get_default_object(blueprint.generated_class())
            mesh = defaults.get_editor_property("mesh")
            mesh.set_editor_property("skeletal_mesh_asset", male)
            mesh.set_editor_property("override_materials", [])
            mesh.set_editor_property("physics_asset_override", None)
            mesh.set_editor_property("anim_class", animation)
            appearance = defaults.get_editor_property("character_appearance")
            appearance.set_editor_property("appearance_catalog", catalog)
            appearance.set_editor_property("hidden_mesh_bones", [])
            if profession == "Mage":
                clear_default_staff(blueprint)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint)
        blueprint = load(ROOT + "/Blueprint/UI/" + preview_name)
        mesh = unreal.get_default_object(blueprint.generated_class()).get_editor_property("skeletal_mesh_component")
        mesh.set_editor_property("skeletal_mesh_asset", male)
        mesh.set_editor_property("override_materials", [])
        mesh.set_editor_property("physics_asset_override", None)
        mesh.set_editor_property("relative_rotation", unreal.Rotator())
        mesh.override_animation_data(idle, True, True, 0.0, 1.0)
        if profession == "Mage":
            clear_default_staff(blueprint)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
    report = {"catalog": catalog.get_path_name(), "bodies": [{"id": str(entry.body_id), "mesh": entry.mesh.get_path_name()} for entry in variants], "outfits_enabled": False, "preserved_outfit_items": len(preserved["items"]), "source_assets_copied": 0, "retargeted_assets": 0, "gameplay_test": "not run"}
    Path(unreal.Paths.project_saved_dir(), "Automation/PrimitiveAppearanceConfigure.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("PRIMITIVE_APPEARANCE_CONFIGURED")


if __name__ == "__main__":
    if "-PrimitiveMageDefaultsOnly" in unreal.SystemLibrary.get_command_line():
        configure_mage_defaults()
    elif "-PrimitivePreviewFacingOnly" in unreal.SystemLibrary.get_command_line():
        configure_preview_facing()
    else:
        configure()
