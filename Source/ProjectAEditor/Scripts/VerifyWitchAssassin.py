import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureWitchAssassin import HELPER, PROFESSIONS, ROOT, STAFF_PATH, UNARMED_SOURCE, WARRIOR_MONTAGE, components, load, mirrored_path, require, skill_montage
from RetargetContentLibrary import pelvis_motion_span, valid_retarget_ops
from WarriorContentPaths import SWORD_SOURCE, SWORD_RECOVERY_SOURCE, animation_sources


def point(vector):
    return [vector.x, vector.y, vector.z]


def verify():
    report = {"gameplay_test": "not run", "professions": [], "poses": {}}
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    if all(party.get_editor_property("professions")[unreal.Name(name)].get_editor_property("appearance_catalog") is not None for name in ["Mage", "Rogue"]):
        from VerifyRogAppearance import verify as verify_current_appearance
        verify_current_appearance()
        unreal.log("WITCH_ASSASSIN_LEGACY_CHECK_DELEGATED_TO_CURRENT_APPEARANCE")
        return
    catalog = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    world = unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/MainMenu")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    stage = next(actor for actor in actors if isinstance(actor, unreal.MainMenuPreviewStage))
    for actor in actors:
        if actor.get_actor_label() in ["SM_Staff_02", "SM_Staff_03", "SM_Staff_04"]:
            require(actor.get_editor_property("hidden") and not actor.get_actor_enable_collision(), "Unused stage staff must remain hidden")
    preview_classes = stage.get_editor_property("preview_actor_classes")
    require(party.get_editor_property("player_unit_classes")[unreal.Name("Warrior")] == load(ROOT + "/Blueprint/Unit/BP_WarriorUnit").generated_class(), "Warrior mapping changed")
    require(party.get_editor_property("player_unit_classes")[unreal.Name("Archer")] == load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class(), "Archer mapping changed")
    require(preview_classes[unreal.Name("Warrior")] == load(ROOT + "/Blueprint/UI/BP_WarriorMenuPreview").generated_class(), "Warrior preview changed")
    require(preview_classes[unreal.Name("Archer")] == load(ROOT + "/Blueprint/UI/BP_PartyMenuPreview").generated_class(), "Archer preview changed")
    source_skills = list(unreal.get_default_object(load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class()).get_editor_property("equipped_skill_data_assets"))
    sword = load(ROOT + "/Blueprint/DataAsset/Skills/BPDA_swoard_attack")
    for profession, mesh_path, suffix, directory, scale in PROFESSIONS:
        mesh = load(mesh_path)
        skeleton = mesh.get_editor_property("skeleton")
        require(HELPER.validate_character_physics(mesh), "Disconnected or invalid ragdoll structure: " + profession)
        bounds = mesh.get_bounds()
        require(160.0 < bounds.box_extent.z * 2.0 < 205.0, "Unexpected character height")
        reference = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        for bone in unreal.AnimPoseExtensions.get_bone_names(reference):
            transform = unreal.AnimPoseExtensions.get_bone_pose(reference, bone, unreal.AnimPoseSpaces.WORLD)
            require(max(abs(value - 1.0) for value in point(transform.scale3d)) < 0.001, "Nonunit reference bone: " + str(bone))
        animation = load(mirrored_path(UNARMED_SOURCE + "/ABP_Unarmed", suffix))
        require(animation.get_editor_property("target_skeleton") == skeleton and HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Invalid animation blueprint")
        controller = unreal.IKRetargeterController.get_controller(load(directory + "/RTG" + suffix))
        require(valid_retarget_ops(controller, "rig" if profession == "Mage" else "root", "DEF-spine" if profession == "Mage" else "pelvis"), "Invalid retarget stack")
        units = []
        for ending in ["Unit", "SnapshotOpponent"]:
            blueprint = load(ROOT + "/Blueprint/Unit/BP_" + profession + ending)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            defaults = unreal.get_default_object(blueprint.generated_class())
            component = defaults.get_editor_property("mesh")
            require(component.get_editor_property("skeletal_mesh_asset") == mesh and component.get_editor_property("anim_class") == animation.generated_class(), "Combat mesh or animation mismatch")
            require(point(component.get_editor_property("relative_scale3d")) == [1.0, 1.0, 1.0], "Component must use unit scale")
            require(component.get_editor_property("physics_asset_override") is None, "Unexpected physics override")
            overrides = defaults.get_editor_property("round_montage_overrides")
            for skill in source_skills + [sword]:
                original = skill_montage(skill)
                montage = overrides.get(original, original)
                require(montage and montage.get_editor_property("skeleton") == skeleton, "Skill montage uses another skeleton")
            for index in range(41):
                endpoints = list(HELPER.sample_weapon_blade(blueprint, sword, 0.23 + index * 0.005))
                require(len(endpoints) == 2 and all(math.isfinite(value) for endpoint in endpoints for value in point(endpoint)), "Invalid blade pose")
                require(90.0 < (endpoints[1] - endpoints[0]).length() < 100.0, "Weapon inherited incorrect scale")
            units.append(blueprint)
        require(party.get_editor_property("player_unit_classes")[unreal.Name(profession)] == units[0].generated_class(), "Missing party class")
        require(party.get_editor_property("professions")[unreal.Name(profession)].get_editor_property("combat_class") == units[0].generated_class(), "Missing profession combat class")
        require(catalog.get_editor_property("enemy_classes")[unreal.Name(profession)] == units[1].generated_class(), "Missing Snapshot class")
        preview = load(ROOT + "/Blueprint/UI/BP_" + profession + "MenuPreview")
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
        require(preview_classes[unreal.Name(profession)] == preview.generated_class(), "Missing menu preview")
        component = unreal.get_default_object(preview.generated_class()).get_editor_property("skeletal_mesh_component")
        require(component.get_editor_property("skeletal_mesh_asset") == mesh and point(component.get_editor_property("relative_scale3d")) == [1.0, 1.0, 1.0], "Preview mesh mismatch")
        idle = load(mirrored_path(UNARMED_SOURCE + "/MM_Idle", suffix))
        require(component.get_editor_property("animation_data").get_editor_property("anim_to_play") == idle, "Wrong preview idle")
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("optional_skeletal_mesh", mesh)
        sequences_checked = 0
        for path in list(animation_sources().values()) + [SWORD_SOURCE, SWORD_RECOVERY_SOURCE]:
            source = load(path)
            if not isinstance(source, unreal.AnimSequence):
                continue
            sequence = load(mirrored_path(path, suffix))
            require(sequence.get_editor_property("skeleton") == skeleton, "Sequence skeleton mismatch")
            require(abs(sequence.get_editor_property("sequence_length") - source.get_editor_property("sequence_length")) < 0.001, "Animation timing changed")
            require(all(sequence.get_editor_property(flag) == source.get_editor_property(flag) for flag in ["enable_root_motion", "force_root_lock", "root_motion_root_lock"]), "Root motion policy changed")
            source_span = pelvis_motion_span(source, load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"))
            target_span = pelvis_motion_span(sequence, mesh, "DEF-spine" if profession == "Mage" else "pelvis")
            require(target_span <= source_span * 2.0 + 20.0, "Excessive pelvis movement")
            for fraction in [0.0, 0.25, 0.5, 0.75, 1.0]:
                sample = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, sequence.get_editor_property("sequence_length") * fraction, options)
                for bone in unreal.AnimPoseExtensions.get_bone_names(sample):
                    transform = unreal.AnimPoseExtensions.get_bone_pose(sample, bone, unreal.AnimPoseSpaces.WORLD)
                    require(all(math.isfinite(value) for value in point(transform.translation)), "Nonfinite pose")
                    require(max(abs(value - 1.0) for value in point(transform.scale3d)) < 0.01, "Animated bone scale differs from one: " + str(bone))
            sequences_checked += 1
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, options)
        report["poses"][profession] = {str(bone): point(unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation) for bone in unreal.AnimPoseExtensions.get_bone_names(pose)}
        if profession == "Mage":
            for blueprint in units + [preview]:
                staff = next(obj for handle, obj, name in components(blueprint) if name == "Staff")
                require(staff.get_editor_property("static_mesh") == load(STAFF_PATH), "Unexpected staff mesh")
                require(str(HELPER.get_weapon_attachment(blueprint, "Staff")) == "DEF-hand_L", "Staff must attach to the left hand")
                require(staff.get_editor_property("visible") and staff.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Invalid staff visibility or collision")
                require(not staff.get_editor_property("hidden_in_game"), "Equipped staff is hidden")
                require(max(abs(value - 1.0) for value in point(staff.get_editor_property("relative_scale3d"))) < 0.001, "Incorrect staff scale")
            staff = next(obj for handle, obj, name in components(preview) if name == "Staff")
            hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-hand_L", unreal.AnimPoseSpaces.WORLD)
            relative = unreal.Transform(location=staff.get_editor_property("relative_location"), rotation=staff.get_editor_property("relative_rotation"), scale=staff.get_editor_property("relative_scale3d"))
            shaft = unreal.MathLibrary.transform_location(hand, unreal.MathLibrary.transform_location(relative, unreal.Vector(-0.476373, 0.000402, 0.0)))
            thumb = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-thumb_02_L", unreal.AnimPoseSpaces.WORLD).translation
            pinky = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-f_pinky_02_L", unreal.AnimPoseSpaces.WORLD).translation
            require((shaft - (thumb + pinky) * 0.5).length() < 0.1, "Staff shaft is outside the authored palm grip")
            report["staff"] = {"bounds": str(load(STAFF_PATH).get_bounds()), "location": point(staff.get_editor_property("relative_location")), "rotation": str(staff.get_editor_property("relative_rotation"))}
        report["professions"].append({"name": profession, "mesh": mesh_path, "height_cm": bounds.box_extent.z * 2.0, "bone_count": len(unreal.AnimPoseExtensions.get_bone_names(reference)), "blade_samples": 82, "sequences_checked": sequences_checked})
    Path(unreal.Paths.project_saved_dir(), "Automation/WitchAssassinReload.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("WITCH_ASSASSIN_RELOAD_VERIFIED")


if __name__ == "__main__":
    verify()
