import json
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureWarriorContent import ASSETS, HELPER, TOOLS, load, require, save
from RetargetContentLibrary import retarget, rig
from WarriorContentPaths import ROOT, SWORD_FOLDER, SWORD_SOURCE, SWORD_RECOVERY_SOURCE, UNARMED_SOURCE, WARRIOR_MONTAGE, WEAPON_SOURCE, animation_sources, mirrored_path, retarget_output_path

WITCH_ROOT = "/Game/stylized_dark_witch_fbx__extracted"
STAFF_PATH = "/Game/MageStaff_FreeWeapons/SM_Staff_01"
PROFESSIONS = [("Rogue", "/Game/Assassin/Mesh/SKM_Assassin_Skin1", "_Assassin", ROOT + "/Assassin/Rigs", 1.0), ("Mage", WITCH_ROOT + "/Stylized_Dark_Witch", "_DarkWitch", ROOT + "/stylized_dark_witch_fbx__extracted/Rigs", 1.0)]
REPORT = {"gameplay_test": "not run", "professions": []}


def components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    return [(handle, library.get_object_for_blueprint(library.get_data(handle), blueprint), str(library.get_variable_name(library.get_data(handle)))) for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint)]


def witch_rigs(source, target, directory, suffix):
    rig(source, directory, "IK_Source" + suffix, save_asset=save)
    path = directory + "/IK_Target" + suffix
    if ASSETS.does_asset_exist(path):
        return
    target_rig = require(unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(directory, "IK_Target" + suffix), "Could not create Witch rig")
    controller = unreal.IKRigController.get_controller(target_rig)
    require(controller.set_skeletal_mesh(target), "Could not set Witch mesh")
    require(controller.set_retarget_root("DEF-spine"), "Missing Rigify pelvis")
    # The normalized import connects weighted deform bones directly; control branches remain unanimated.
    # 정규화된 임포트는 변형 본을 직접 연결하며 제어용 계층에는 애니메이션을 입히지 않습니다.
    chains = [("Root", "rig", "rig"), ("Spine", "DEF-spine_001", "DEF-spine_003"), ("Neck", "DEF-spine_004", "DEF-spine_005"), ("Head", "DEF-spine_006", "DEF-spine_006")]
    for side, letter in [("Left", "L"), ("Right", "R")]:
        chains += [(side + "Leg", "DEF-thigh_" + letter, "DEF-toe_" + letter), (side + "Clavicle", "DEF-shoulder_" + letter, "DEF-shoulder_" + letter), (side + "Arm", "DEF-upper_arm_" + letter, "DEF-hand_" + letter)]
        for finger in ["Thumb", "Index", "Middle", "Ring", "Pinky"]:
            prefix = "DEF-thumb" if finger == "Thumb" else "DEF-f_" + finger.lower()
            chains.append((side + finger, prefix + "_01_" + letter, prefix + "_03_" + letter))
    for name, start, end in chains:
        require(str(controller.add_retarget_chain(name, start, end, "")) == name, "Invalid Witch chain: " + name)
    save(target_rig)


def attach_weapon(blueprint, name, weapon, bone, relative):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    entries = components(blueprint)
    component = next((obj for handle, obj, variable in entries if variable == name), None)
    if component is None:
        parent = next(handle for handle, obj, variable in entries if isinstance(obj, unreal.SkeletalMeshComponent))
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint))
        require(not str(reason), "Could not create weapon: " + str(reason))
        require(subsystem.rename_subobject(handle, name), "Could not name weapon")
        component = library.get_object_for_blueprint(library.get_data(handle), blueprint)
    component.set_static_mesh(weapon)
    component.set_editor_property("relative_location", relative.translation)
    component.set_editor_property("relative_rotation", relative.rotation.rotator())
    component.set_editor_property("relative_scale3d", relative.scale3d)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    require(HELPER.set_weapon_attachment(blueprint, name, bone), "Could not persist weapon attachment")


def witch_grips(mesh, idle, scale):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", mesh)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, options)
    require(unreal.AnimPoseExtensions.is_valid(pose), "Invalid Witch idle pose")
    grips = {}
    for side in ["L", "R"]:
        hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-hand_" + side, unreal.AnimPoseSpaces.WORLD)
        thumb = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-thumb_02_" + side, unreal.AnimPoseSpaces.WORLD).translation
        pinky = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-f_pinky_02_" + side, unreal.AnimPoseSpaces.WORLD).translation
        middle = unreal.AnimPoseExtensions.get_bone_pose(pose, "DEF-f_middle_01_" + side, unreal.AnimPoseSpaces.WORLD).translation
        grip = (thumb + pinky) * 0.5
        axis = thumb - pinky
        if axis.z < 0.0:
            axis *= -1.0
        rotation = unreal.MathLibrary.make_rot_from_zx(axis, middle - hand.translation)
        desired = unreal.Transform(location=grip, rotation=rotation, scale=unreal.Vector(1.0 / scale, 1.0 / scale, 1.0 / scale))
        if side == "L":
            desired.translation = grip - unreal.MathLibrary.transform_direction(desired, unreal.Vector(-0.476373 / scale, 0.000402 / scale, 0.0))
        else:
            desired.rotation = unreal.MathLibrary.make_rot_from_zx(-axis, middle - hand.translation).quaternion()
        grips[side] = unreal.MathLibrary.make_relative_transform(desired, hand)
    return grips


def skill_montage(skill):
    montage = skill.get_editor_property("round_definition").get_editor_property("cast_montage") if skill.get_editor_property("use_round_definition") else None
    return montage or unreal.get_default_object(skill.get_editor_property("ability_class")).get_editor_property("attack_montage")


def configure():
    source_unit = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit")
    source_defaults = unreal.get_default_object(source_unit.generated_class())
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    source_animation = load(UNARMED_SOURCE + "/ABP_Unarmed")
    montages = list(dict.fromkeys(skill_montage(skill) for skill in source_defaults.get_editor_property("equipped_skill_data_assets")))
    sword_manny = load(SWORD_FOLDER + "/AM_SwordAttack_Manny")
    inputs = [source_animation] + montages + [sword_manny, load(SWORD_SOURCE), load(SWORD_RECOVERY_SOURCE)]
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    definitions = dict(party.get_editor_property("professions"))
    classes = dict(party.get_editor_property("player_unit_classes"))
    catalog = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    snapshot_classes = dict(catalog.get_editor_property("enemy_classes"))
    previews = {}
    for profession, path, suffix, directory, scale in PROFESSIONS:
        mesh = load(path)
        skeleton = mesh.get_editor_property("skeleton")
        require(HELPER.ensure_selected_profession_slot(skeleton), "Could not register the shared montage slot")
        require(ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=True), "Could not save original skeleton slot")
        policy = {}
        if profession == "Mage":
            require(HELPER.normalize_witch_import(mesh), "Could not normalize Witch import")
            for asset in [mesh, mesh.get_editor_property("skeleton"), mesh.get_editor_property("physics_asset")]:
                require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save normalized Witch import")
            witch_rigs(source_mesh, mesh, directory, suffix)
            policy = {"target_root": "rig", "target_pelvis": "DEF-spine"}
        converted = retarget(source_mesh, mesh, directory, suffix, inputs, report=REPORT, force_rebuild=profession == "Mage" and "-WitchRebuildRetargets" in unreal.SystemLibrary.get_command_line(), anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path, output_path=retarget_output_path, save_asset=save, **policy)
        animation = converted[source_animation]
        require(HELPER.remove_legacy_foot_ik(animation), "Could not remove incompatible Manny foot rig")
        require(HELPER.ensure_output_slot(animation, "DefaultSlot"), "Missing montage output")
        save(animation)
        idle = load(mirrored_path(UNARMED_SOURCE + "/MM_Idle", suffix))
        grips = witch_grips(mesh, idle, scale) if profession == "Mage" else None
        overrides = {montage: converted[montage] for montage in montages}
        overrides[load(WARRIOR_MONTAGE)] = converted[sword_manny]
        for ending in ["Unit", "SnapshotOpponent"]:
            blueprint = load(ROOT + "/Blueprint/Unit/BP_" + profession + ending)
            defaults = unreal.get_default_object(blueprint.generated_class())
            skills = list(defaults.get_editor_property("equipped_skill_data_assets"))
            component = defaults.get_editor_property("mesh")
            component.set_editor_property("skeletal_mesh_asset", mesh)
            component.set_editor_property("override_materials", [])
            component.set_editor_property("physics_asset_override", None)
            component.set_editor_property("anim_class", animation.generated_class())
            component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
            defaults.set_editor_property("round_montage_overrides", overrides)
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            sword_grip = grips["R"] if grips else unreal.Transform(location=unreal.Vector(-11.095651, 5.605028, -10.0), rotation=unreal.Rotator(0.0, 0.0, 180.0))
            attach_weapon(blueprint, "Sword", load(mirrored_path(WEAPON_SOURCE)), "DEF-hand_R" if grips else "hand_r", sword_grip)
            if grips:
                attach_weapon(blueprint, "Staff", load(STAFF_PATH), "DEF-hand_L", grips["L"])
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
            save(blueprint)
            require(list(unreal.get_default_object(blueprint.generated_class()).get_editor_property("equipped_skill_data_assets")) == skills, "Existing skills changed")
            if ending == "Unit":
                classes[unreal.Name(profession)] = blueprint.generated_class()
                definition = definitions[unreal.Name(profession)]
                definition.set_editor_property("combat_class", blueprint.generated_class())
                definitions[unreal.Name(profession)] = definition
            else:
                snapshot_classes[unreal.Name(profession)] = blueprint.generated_class()
        preview = load(ROOT + "/Blueprint/UI/BP_" + profession + "MenuPreview")
        component = unreal.get_default_object(preview.generated_class()).get_editor_property("skeletal_mesh_component")
        component.set_editor_property("skeletal_mesh_asset", mesh)
        component.set_editor_property("override_materials", [])
        component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
        component.override_animation_data(idle, True, True, 0.0, 1.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
        if grips:
            attach_weapon(preview, "Staff", load(STAFF_PATH), "DEF-hand_L", grips["L"])
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
        save(preview)
        previews[unreal.Name(profession)] = preview.generated_class()
        REPORT["professions"].append({"name": profession, "mesh": path, "scale": scale, "animation": animation.get_path_name()})
    party.set_editor_property("professions", definitions)
    party.set_editor_property("player_unit_classes", classes)
    save(party)
    catalog.set_editor_property("enemy_classes", snapshot_classes)
    save(catalog)
    world = unreal.EditorLoadingAndSavingUtils.load_map(ROOT + "/LEVEL/MainMenu")
    stage = next(actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage))
    current = dict(stage.get_editor_property("preview_actor_classes"))
    current.update(previews)
    stage.set_editor_property("preview_actor_classes", current)
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT + "/LEVEL/MainMenu"), "Could not save menu mappings")
    Path(unreal.Paths.project_saved_dir(), "Automation/WitchAssassinConfigure.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("WITCH_ASSASSIN_CONFIGURED")


if __name__ == "__main__":
    try:
        configure()
    finally:
        unreal.SystemLibrary.quit_editor()
