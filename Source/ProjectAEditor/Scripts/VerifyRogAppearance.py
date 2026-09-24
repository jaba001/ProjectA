import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureRogAppearance import BODY, CATALOG_PATH, MANNY, ROOT, SLOTS, UNARMED_SOURCE, HELPER, load, require


def accepts(catalog, names):
    selection = unreal.CharacterAppearanceSelection()
    selection.set_editor_property("item_ids", names)
    result = catalog.validate_selection(selection)
    return result[0] if isinstance(result, tuple) else result


def verify():
    catalog = load(CATALOG_PATH)
    slots = list(catalog.get_editor_property("slots"))
    items = list(catalog.get_editor_property("items"))
    parts = list(catalog.get_editor_property("body_parts"))
    require(len(slots) == len(SLOTS) and len(parts) == 6 and len(items) > 90, "Incomplete ROG catalogue")
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
    samples = 0
    for ending in ["Unit", "SnapshotOpponent"]:
        blueprint = load(ROOT + "/Blueprint/Unit/BP_Warrior" + ending)
        defaults = unreal.get_default_object(blueprint.generated_class())
        mesh = defaults.get_editor_property("mesh")
        require(mesh.get_editor_property("skeletal_mesh_asset") == leader, "Warrior must use the common base mesh")
        require(mesh.get_editor_property("anim_class") == source_mesh.get_editor_property("anim_class"), "Warrior must reuse the common animation")
        require(mesh.get_editor_property("visible"), "Fallback leader must begin visible")
        require(mesh.get_editor_property("physics_asset_override") is None, "Warrior physics override remains")
        require(defaults.get_editor_property("character_appearance").get_editor_property("appearance_catalog") == catalog, "Missing native appearance catalogue")
        require(dict(defaults.get_editor_property("round_montage_overrides")) == dict(source_defaults.get_editor_property("round_montage_overrides")), "Common montage overrides differ")
        for index in range(41):
            endpoints = list(HELPER.sample_weapon_blade(blueprint, sword, 0.23 + index * 0.005))
            require(len(endpoints) == 2 and all(math.isfinite(value) for point in endpoints for value in [point.x, point.y, point.z]), "Invalid common sword blade")
            require(90.0 < (endpoints[1] - endpoints[0]).length() < 100.0, "Common sword scale differs")
            samples += 1
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    require(party.get_editor_property("professions")[unreal.Name("Warrior")].get_editor_property("appearance_catalog") == catalog, "Profession catalogue missing")
    snapshots = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    require(snapshots.get_editor_property("enemy_classes")[unreal.Name("Warrior")] == load(ROOT + "/Blueprint/Unit/BP_WarriorSnapshotOpponent").generated_class(), "Snapshot outfit class missing")
    require(snapshots.get_editor_property("legacy_enemy_classes")[unreal.Name("Warrior")] == load(ROOT + "/Blueprint/Unit/BP_SnapshotOpponent").generated_class(), "Historical Snapshot class compatibility missing")
    preview = unreal.get_default_object(load(ROOT + "/Blueprint/UI/BP_WarriorMenuPreview").generated_class()).get_editor_property("skeletal_mesh_component")
    require(preview.get_editor_property("skeletal_mesh_asset") == leader, "Menu base mesh differs")
    require(preview.get_editor_property("animation_data").get_editor_property("anim_to_play") == load(UNARMED_SOURCE + "/MM_Idle"), "Menu idle differs")
    for profession in ["Mage", "Archer", "Rogue"]:
        require(party.get_editor_property("professions")[unreal.Name(profession)].get_editor_property("appearance_catalog") is None, "Other profession unexpectedly customized")
    require(HELPER.validate_character_physics(leader), "Common ragdoll structure invalid")
    report = {"slots": len(slots), "items": len(items), "original_meshes": len(meshes), "matching_bones": len(names), "blade_samples": samples, "gameplay_test": "not run", "ui_visual_test": "user pending"}
    Path(unreal.Paths.project_saved_dir(), "Automation/RogAppearanceReload.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("ROG_APPEARANCE_RELOAD_VERIFIED")


if __name__ == "__main__":
    verify()
