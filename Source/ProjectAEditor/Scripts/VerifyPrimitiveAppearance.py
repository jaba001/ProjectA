import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigurePrimitiveAppearance import BODIES, CATALOG_PATH, MANNY, PROFESSIONS, ROOT, UNARMED_SOURCE, load, require
from ConfigureWitchAssassin import components
from ConfigureWarriorContent import HELPER
from WarriorContentPaths import SWORD_SOURCE, animation_sources


def accepts(catalog, body_id, item_ids=()):
    selection = unreal.CharacterAppearanceSelection()
    selection.set_editor_property("body_id", body_id)
    selection.set_editor_property("item_ids", list(item_ids))
    result = catalog.validate_selection(selection)
    return result[0] if isinstance(result, tuple) else result


def verify_default_staff(blueprint):
    for handle, component, name in components(blueprint):
        if isinstance(component, unreal.StaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            require(mesh is None or not mesh.get_path_name().startswith("/Game/MageStaff_FreeWeapons/"), "Mage must not carry a default staff")
        if name == "Staff":
            require(component.get_editor_property("static_mesh") is None and not component.get_editor_property("visible") and component.get_editor_property("hidden_in_game"), "Default staff attachment must be empty and hidden")
            require(component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Empty staff attachment must not collide")


def verify_mage_defaults():
    paths = [ROOT + "/Blueprint/Unit/BP_MageUnit", ROOT + "/Blueprint/Unit/BP_MageSnapshotOpponent", ROOT + "/Blueprint/UI/BP_MageMenuPreview"]
    for path in paths:
        verify_default_staff(load(path))
    require(load("/Game/MageStaff_FreeWeapons/SM_Staff_01"), "Original staff asset must remain available")
    report = {"blueprints": paths, "default_staffs": 0, "original_staff_preserved": True, "gameplay_test": "not run", "ui_visual_test": "user pending"}
    Path(unreal.Paths.project_saved_dir(), "Automation/MageDefaultStaffReload.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MAGE_DEFAULT_STAFF_RELOAD_VERIFIED")


def verify():
    catalog = load(CATALOG_PATH)
    require(not catalog.get_editor_property("enable_outfits"), "Outfits must remain disabled")
    require(str(catalog.get_editor_property("default_body_id")) == "Male", "Missing default male body")
    variants = {str(entry.body_id): entry for entry in catalog.get_editor_property("body_variants")}
    require(len(variants) >= len(BODIES), "Missing body variants")
    items = list(catalog.get_editor_property("items"))
    require(len(items) == 103 and len(catalog.get_editor_property("slots")) == 8, "Preserved outfit catalogue changed")
    require(accepts(catalog, "None") and not accepts(catalog, "UnknownBody"), "Invalid default or unknown body validation")
    require(not accepts(catalog, "Female", ["UnknownOutfit"]), "Unknown legacy outfit was accepted")
    require(not accepts(catalog, "Male", [items[0].item_id, items[0].item_id]), "Duplicate legacy outfit was accepted")
    source_skeleton = load(MANNY).get_editor_property("skeleton")
    animation = load(UNARMED_SOURCE + "/ABP_Unarmed").generated_class()
    idle = load(UNARMED_SOURCE + "/MM_Idle")
    sequences = list(dict.fromkeys([load(path) for path in animation_sources().values()] + [load(SWORD_SOURCE)]))
    sequences = [asset for asset in sequences if isinstance(asset, unreal.AnimSequence)]
    require(sequences, "Shared animation sequences are missing")
    report = {"bodies": [], "professions": [], "preserved_outfits": len(items), "pose_samples": 0, "unarmed_mage_blueprints": 0, "gameplay_test": "not run", "ui_visual_test": "user pending"}
    for body_id, title, path in BODIES:
        variant = variants[body_id]
        mesh = load(path)
        skeleton = mesh.get_editor_property("skeleton")
        require(variant.mesh == mesh and str(variant.display_name) == title, "Wrong body mapping: " + body_id)
        require(variant.animation_class == animation and variant.preview_animation == idle, "Body must use shared original animations")
        require(source_skeleton in list(skeleton.get_editor_property("compatible_skeletons")), "Missing compatible skeleton registration")
        require(skeleton.get_editor_property("use_retarget_modes_from_compatible_skeleton"), "Missing per-body translation retarget policy")
        require(unreal.CharacterAppearanceAssetLibrary.validate_body_animation_skeleton(mesh, source_skeleton), "Incompatible body skeleton")
        require(HELPER.validate_character_physics(mesh), "Invalid original ragdoll")
        require(accepts(catalog, body_id) and all(accepts(catalog, body_id, [item.item_id]) for item in items), "Body or preserved outfit data rejected")
        selection = unreal.CharacterAppearanceSelection()
        selection.set_editor_property("body_id", body_id)
        selection.set_editor_property("item_ids", [items[0].item_id])
        restored = unreal.CharacterAppearanceSelection()
        restored.import_text(selection.export_text())
        require(restored.body_id == selection.body_id and list(restored.item_ids) == list(selection.item_ids), "Body identifier did not survive reflected serialization")
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("optional_skeletal_mesh", mesh)
        for sequence in sequences:
            for seconds in [0.0, sequence.get_play_length() * 0.5, sequence.get_play_length()]:
                pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, seconds, options)
                require(unreal.AnimPoseExtensions.is_valid(pose), "Invalid body animation pose")
                for bone in ["root", "pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r"]:
                    transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
                    require(all(math.isfinite(value) for value in transform.translation.to_tuple()) and (transform.scale3d - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Invalid body pose or scale: " + body_id + "/" + bone)
                report["pose_samples"] += 1
        report["bodies"].append({"id": body_id, "mesh": path, "skeleton": skeleton.get_path_name(), "physics": mesh.get_editor_property("physics_asset").get_path_name(), "height_cm": mesh.get_bounds().box_extent.z * 2.0})
    male = load(BODIES[0][2])
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    snapshots = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/MainMenu")
    stage = next(actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage))
    source_defaults = unreal.get_default_object(load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class())
    for profession, unit_name, preview_name in PROFESSIONS:
        units = [load(ROOT + "/Blueprint/Unit/" + unit_name), load(ROOT + "/Blueprint/Unit/BP_" + profession + "SnapshotOpponent")]
        for blueprint in units:
            defaults = unreal.get_default_object(blueprint.generated_class())
            mesh = defaults.get_editor_property("mesh")
            require(mesh.get_editor_property("skeletal_mesh_asset") == male and mesh.get_editor_property("anim_class") == animation, "Incorrect default unit body")
            require(not mesh.get_editor_property("override_materials") and mesh.get_editor_property("physics_asset_override") is None, "Unit must use original body materials and physics")
            require(defaults.get_editor_property("character_appearance").get_editor_property("appearance_catalog") == catalog, "Unit catalogue missing")
            require(dict(defaults.get_editor_property("round_montage_overrides")) == dict(source_defaults.get_editor_property("round_montage_overrides")), "Original montage mapping changed")
        definition = party.get_editor_property("professions")[unreal.Name(profession)]
        require(definition.get_editor_property("appearance_catalog") == catalog and definition.get_editor_property("combat_class") in [None, units[0].generated_class()], "Profession definition changed")
        require(party.get_editor_property("player_unit_classes")[unreal.Name(profession)] == units[0].generated_class(), "Party class changed")
        require(snapshots.get_editor_property("enemy_classes")[unreal.Name(profession)] == units[1].generated_class(), "Snapshot class changed")
        preview_blueprint = load(ROOT + "/Blueprint/UI/" + preview_name)
        require(stage.get_editor_property("preview_actor_classes")[unreal.Name(profession)] == preview_blueprint.generated_class(), "Preview class mapping changed")
        preview = unreal.get_default_object(preview_blueprint.generated_class()).get_editor_property("skeletal_mesh_component")
        require(preview.get_editor_property("skeletal_mesh_asset") == male and not preview.get_editor_property("override_materials"), "Incorrect default preview body")
        require(preview.get_editor_property("animation_data").get_editor_property("anim_to_play") == idle, "Incorrect preview animation")
        if profession == "Mage":
            for blueprint in units + [preview_blueprint]:
                verify_default_staff(blueprint)
                report["unarmed_mage_blueprints"] += 1
        report["professions"].append(profession)
    Path(unreal.Paths.project_saved_dir(), "Automation/PrimitiveAppearanceReload.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("PRIMITIVE_APPEARANCE_RELOAD_VERIFIED")


if __name__ == "__main__":
    if "-PrimitiveMageDefaultsOnly" in unreal.SystemLibrary.get_command_line():
        verify_mage_defaults()
    else:
        verify()
