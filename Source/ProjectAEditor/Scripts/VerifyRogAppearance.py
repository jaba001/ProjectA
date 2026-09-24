import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureRogAppearance import ASSETS, CATALOG_PATH, LEGACY_BODY_MATERIAL_PATH, MANNY, PROFESSIONS, ROOT, SLOTS, UNARMED_SOURCE, HELPER, components, load, require, topdown_mesh


def accepts(catalog, names):
    selection = unreal.CharacterAppearanceSelection()
    selection.set_editor_property("item_ids", names)
    result = catalog.validate_selection(selection)
    return result[0] if isinstance(result, tuple) else result


def verify():
    catalog = load(CATALOG_PATH)
    if catalog.get_editor_property("body_variants"):
        from VerifyPrimitiveAppearance import verify as verify_primitive
        return verify_primitive()
    slots = list(catalog.get_editor_property("slots"))
    items = list(catalog.get_editor_property("items"))
    parts = list(catalog.get_editor_property("body_parts"))
    require(len(slots) == len(SLOTS) and len(parts) == 6 and len(items) == 103, "Incomplete ROG catalogue")
    require(accepts(catalog, []), "Default appearance rejected")
    require(not accepts(catalog, ["UnknownCosmetic"]), "Unknown outfit ID accepted")
    require(not accepts(catalog, [items[0].item_id, items[0].item_id]), "Duplicate outfit ID accepted")
    by_slot = {}
    for item in items:
        require(accepts(catalog, [item.item_id]), "Invalid catalogue selection: " + str(item.item_id))
        by_slot.setdefault(item.slot_tag.export_text(), []).append(item)
    for slot_items in by_slot.values():
        if len(slot_items) > 1:
            require(not accepts(catalog, [slot_items[0].item_id, slot_items[1].item_id]), "Two outfits for the same slot accepted")
    require(accepts(catalog, [slot_items[0].item_id for slot_items in by_slot.values()]), "Full outfit rejected")
    leader = load(MANNY)
    original = topdown_mesh()
    require(original.get_editor_property("skeletal_mesh_asset") == leader, "Base mesh differs from TopDown")
    original_materials = [original.get_material(index) for index in range(original.get_num_materials())]
    require(not ASSETS.does_asset_exist(LEGACY_BODY_MATERIAL_PATH), "Obsolete untextured body material remains")
    body_materials = {}
    for part in parts:
        material_slots = list(unreal.CharacterAppearanceAssetLibrary.validate_body_material_slots(leader, part.mesh))
        require(material_slots and list(part.material_overrides) == [original_materials[index] for index in material_slots], "Missing original textured body materials: " + part.mesh.get_name())
        body_materials[part.mesh.get_name()] = [original_materials[index].get_path_name() for index in material_slots]
    reference = unreal.AnimPoseExtensions.get_reference_pose(leader.get_editor_property("skeleton"))
    names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(reference)}
    meshes = {part.mesh.get_path_name(): part.mesh for part in parts}
    for item in items:
        for mesh in item.meshes:
            meshes[mesh.get_path_name()] = mesh
    skeletons = {mesh.get_editor_property("skeleton") for mesh in meshes.values()}
    for skeleton in skeletons:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        for name in unreal.AnimPoseExtensions.get_bone_names(pose):
            require(str(name) in names, "Missing leader bone " + str(name))
            first = unreal.AnimPoseExtensions.get_bone_pose(reference, name, unreal.AnimPoseSpaces.WORLD)
            second = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
            require((first.translation - second.translation).length() < 0.01 and (first.scale3d - second.scale3d).length() < 0.001, "Follower bind pose differs: " + str(name))
    require(all(path.startswith("/Game/ROG_Modular_Armor/") for path in meshes), "Cosmetic mesh was copied outside its source pack")
    source_defaults = unreal.get_default_object(load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class())
    source_mesh = source_defaults.get_editor_property("mesh")
    sword = load(ROOT + "/Blueprint/DataAsset/Skills/BPDA_swoard_attack")
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    snapshots = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/MainMenu")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    stage = next(actor for actor in actors if isinstance(actor, unreal.MainMenuPreviewStage))
    for actor in actors:
        if actor.get_actor_label() in ["SM_Staff_02", "SM_Staff_03", "SM_Staff_04"]:
            require(actor.get_editor_property("hidden") and not actor.get_actor_enable_collision(), "Unused stage staff must remain hidden")
    samples = 0
    staff_blueprints = []
    for profession, unit_name, preview_name in PROFESSIONS:
        units = [load(ROOT + "/Blueprint/Unit/" + unit_name), load(ROOT + "/Blueprint/Unit/BP_" + profession + "SnapshotOpponent")]
        for blueprint in units:
            defaults = unreal.get_default_object(blueprint.generated_class())
            mesh = defaults.get_editor_property("mesh")
            require(mesh.get_editor_property("skeletal_mesh_asset") == leader, "Profession must use the common base mesh: " + profession)
            require(mesh.get_editor_property("anim_class") == source_mesh.get_editor_property("anim_class"), "Profession must reuse the common animation: " + profession)
            require([mesh.get_material(index) for index in range(mesh.get_num_materials())] == original_materials, "Unit materials differ from TopDown: " + profession)
            require((mesh.get_editor_property("relative_scale3d") - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Unit mesh scale differs")
            require(mesh.get_editor_property("visible"), "Fallback leader must begin visible")
            require(mesh.get_editor_property("physics_asset_override") is None, "Profession physics override remains")
            appearance = defaults.get_editor_property("character_appearance")
            require(appearance.get_editor_property("appearance_catalog") == catalog and not appearance.get_editor_property("hidden_mesh_bones"), "Missing native appearance catalogue or hidden base bones")
            require(dict(defaults.get_editor_property("round_montage_overrides")) == dict(source_defaults.get_editor_property("round_montage_overrides")), "Common montage overrides differ")
            for index in range(41):
                endpoints = list(HELPER.sample_weapon_blade(blueprint, sword, 0.23 + index * 0.005))
                require(len(endpoints) == 2 and all(math.isfinite(value) for point in endpoints for value in [point.x, point.y, point.z]), "Invalid common sword blade")
                require(90.0 < (endpoints[1] - endpoints[0]).length() < 100.0, "Common sword scale differs")
                samples += 1
        definition = party.get_editor_property("professions")[unreal.Name(profession)]
        require(definition.get_editor_property("appearance_catalog") == catalog, "Profession catalogue missing: " + profession)
        require(definition.get_editor_property("combat_class") in [None, units[0].generated_class()], "Profession combat class differs: " + profession)
        require(party.get_editor_property("player_unit_classes")[unreal.Name(profession)] == units[0].generated_class(), "Party class changed")
        require(snapshots.get_editor_property("enemy_classes")[unreal.Name(profession)] == units[1].generated_class(), "Snapshot outfit class missing")
        if profession in ["Warrior", "Archer"]:
            require(snapshots.get_editor_property("legacy_enemy_classes")[unreal.Name(profession)] == load(ROOT + "/Blueprint/Unit/BP_SnapshotOpponent").generated_class(), "Historical Snapshot class compatibility missing")
        preview_blueprint = load(ROOT + "/Blueprint/UI/" + preview_name)
        require(stage.get_editor_property("preview_actor_classes")[unreal.Name(profession)] == preview_blueprint.generated_class(), "Menu preview mapping differs")
        preview = unreal.get_default_object(preview_blueprint.generated_class()).get_editor_property("skeletal_mesh_component")
        require(preview.get_editor_property("skeletal_mesh_asset") == leader, "Menu base mesh differs")
        require([preview.get_material(index) for index in range(preview.get_num_materials())] == original_materials, "Preview materials differ from TopDown")
        require((preview.get_editor_property("relative_scale3d") - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Menu mesh scale differs")
        require(preview.get_editor_property("animation_data").get_editor_property("anim_to_play") == load(UNARMED_SOURCE + "/MM_Idle"), "Menu idle differs")
        if profession == "Mage":
            staff_blueprints = units + [preview_blueprint]
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", leader)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(load(UNARMED_SOURCE + "/MM_Idle"), 0.0, options)
    hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD)
    palm = (unreal.AnimPoseExtensions.get_bone_pose(pose, "thumb_02_l", unreal.AnimPoseSpaces.WORLD).translation + unreal.AnimPoseExtensions.get_bone_pose(pose, "pinky_02_l", unreal.AnimPoseSpaces.WORLD).translation) * 0.5
    for blueprint in staff_blueprints:
        staff = next(component for handle, component, name in components(blueprint) if name == "Staff")
        require(staff.get_editor_property("static_mesh") == load("/Game/MageStaff_FreeWeapons/SM_Staff_01"), "Unexpected staff mesh")
        require(str(HELPER.get_weapon_attachment(blueprint, "Staff")) == "hand_l", "Staff must attach to the Manny left hand")
        require(staff.get_editor_property("visible") and not staff.get_editor_property("hidden_in_game") and staff.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Invalid staff visibility or collision")
        relative = unreal.Transform(location=staff.get_editor_property("relative_location"), rotation=staff.get_editor_property("relative_rotation"), scale=staff.get_editor_property("relative_scale3d"))
        require((relative.scale3d - unreal.Vector(1.0, 1.0, 1.0)).length() < 0.001, "Staff scale differs")
        shaft = unreal.MathLibrary.transform_location(hand, unreal.MathLibrary.transform_location(relative, unreal.Vector(-0.476373, 0.000402, 0.0)))
        require((shaft - palm).length() < 0.1, "Staff shaft is outside the authored palm grip")
    require(HELPER.validate_character_physics(leader), "Common ragdoll structure invalid")
    report = {"professions": [entry[0] for entry in PROFESSIONS], "base_mesh": leader.get_path_name(), "body_materials": body_materials, "slots": len(slots), "items": len(items), "original_meshes": len(meshes), "matching_bones": len(names), "blade_samples": samples, "staff_grips": len(staff_blueprints), "gameplay_test": "not run", "ui_visual_test": "user pending"}
    Path(unreal.Paths.project_saved_dir(), "Automation/RogAppearanceReload.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("ROG_APPEARANCE_RELOAD_VERIFIED")


if __name__ == "__main__":
    verify()
