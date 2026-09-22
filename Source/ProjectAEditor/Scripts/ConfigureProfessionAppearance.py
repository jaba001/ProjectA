import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureWarriorContent import ASSETS, HELPER, SKILLS, TOOLS, configure_weapon, duplicate, load, require, retarget, save
from ImportMageStaff import MESH_PATH as STAFF_PATH, configure as import_staff, describe_bounds
from WarriorContentPaths import ROOT, SWORD_FOLDER, SWORD_SOURCE, SWORD_RECOVERY_SOURCE, UNARMED_SOURCE, WARRIOR_MONTAGE, WEAPON_SOURCE, mirrored_path


PROFESSIONS = [("Warrior", "Kwang", "Kwang_GDC"), ("Mage", "Gideon", "Gideon"), ("Archer", "Sparrow", "Sparrow"), ("Rogue", "Countess", "SM_Countess")]
PARTY_PATH = ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty"
SNAPSHOT_CATALOG = ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog"
MENU_PATH = ROOT + "/LEVEL/MainMenu"
SWORD_MANNY = SWORD_FOLDER + "/AM_SwordAttack_Manny"
REPORT = {"gameplay_test": "not run", "original_meshes_referenced_directly": True, "professions": []}
HIDDEN_WEAPON_BONES = {"Kwang": ["weapon_r"], "Sparrow": ["bow_base", "arrow_nock"], "Gideon": [], "Countess": ["weapon_l", "weapon_r"]}


def hero_root(hero):
    return "/Game/Paragon" + hero + "/Characters/Heroes/" + hero


def unit_path(profession):
    return ROOT + "/Blueprint/Unit/BP_" + profession + "Unit"


def preview_path(profession):
    return ROOT + "/Blueprint/UI/BP_" + profession + "MenuPreview"


def snapshot_path(profession):
    return ROOT + "/Blueprint/Unit/BP_" + profession + "SnapshotOpponent"


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    return [(handle, library.get_object_for_blueprint(library.get_data(handle), blueprint), str(library.get_variable_name(library.get_data(handle)))) for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint)]


def configure_embedded_weapons(blueprint, hero):
    templates = component_templates(blueprint)
    component = next((obj for handle, obj, name in templates if isinstance(obj, unreal.CharacterAppearanceComponent)), None)
    if component is None and HIDDEN_WEAPON_BONES[hero]:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=templates[0][0], new_class=unreal.CharacterAppearanceComponent, blueprint_context=blueprint))
        require(not str(reason), "Could not add appearance settings: " + str(reason))
        require(subsystem.rename_subobject(handle, "CharacterAppearance"), "Could not name appearance component")
        component = library.get_object_for_blueprint(library.get_data(handle), blueprint)
    if component:
        component.set_editor_property("hidden_mesh_bones", [unreal.Name(bone) for bone in HIDDEN_WEAPON_BONES[hero]])


def configure_staff(blueprint, mesh, idle, staff):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    templates = component_templates(blueprint)
    parent = next(handle for handle, obj, name in templates if isinstance(obj, unreal.SkeletalMeshComponent))
    component = next((obj for handle, obj, name in templates if name == "Staff"), None)
    if component is None:
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint))
        require(not str(reason), "Could not create staff component: " + str(reason))
        require(subsystem.rename_subobject(handle, "Staff"), "Could not name staff component")
        component = library.get_object_for_blueprint(library.get_data(handle), blueprint)
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("optional_skeletal_mesh", mesh)
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, options)
    require(unreal.AnimPoseExtensions.is_valid(pose), "Could not evaluate mage idle hand pose")
    hand = unreal.AnimPoseExtensions.get_bone_pose(pose, "hand_l", unreal.AnimPoseSpaces.WORLD)
    finger = unreal.AnimPoseExtensions.get_bone_pose(pose, "middle_01_l", unreal.AnimPoseSpaces.WORLD)
    palm = (hand.translation + finger.translation) * 0.5
    # Align the staff upright at the closed left palm, leaving the right hand available for purchased sword skills.
    # 지팡이를 왼손 손바닥에서 수직으로 정렬하고 오른손은 구매한 검 스킬에 사용합니다.
    desired = unreal.Transform(location=palm, rotation=unreal.Rotator(), scale=unreal.Vector(1.0, 1.0, 1.0))
    relative = unreal.MathLibrary.make_relative_transform(desired, hand)
    component.set_static_mesh(staff)
    component.set_editor_property("relative_location", relative.translation)
    component.set_editor_property("relative_rotation", relative.rotation.rotator())
    component.set_editor_property("relative_scale3d", relative.scale3d)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    require(HELPER.set_weapon_attachment(blueprint, "Staff", "hand_l"), "Could not save staff hand attachment")


def skill_montage(skill):
    montage = skill.get_editor_property("round_definition").get_editor_property("cast_montage") if skill.get_editor_property("use_round_definition") else None
    return montage or unreal.get_default_object(skill.get_editor_property("ability_class")).get_editor_property("attack_montage")


def configure_combat_blueprint(blueprint, mesh, animation, overrides, hero, idle, staff, weapon):
    defaults = unreal.get_default_object(blueprint.generated_class())
    previous_skills = list(defaults.get_editor_property("equipped_skill_data_assets"))
    component = defaults.get_editor_property("mesh")
    component.set_editor_property("skeletal_mesh_asset", mesh)
    component.set_editor_property("override_materials", [])
    component.set_editor_property("anim_class", animation.generated_class())
    defaults.set_editor_property("round_montage_overrides", overrides)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    configure_weapon(blueprint, weapon, (-11.095651, 5.605028, -10.0))
    configure_embedded_weapons(blueprint, hero)
    if hero == "Gideon":
        configure_staff(blueprint, mesh, idle, staff)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    require(list(unreal.get_default_object(blueprint.generated_class()).get_editor_property("equipped_skill_data_assets")) == previous_skills, "Visual changes altered existing skill defaults")


def configure():
    staff = import_staff()
    REPORT["staff"] = describe_bounds(staff)
    source_unit = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit")
    source_defaults = unreal.get_default_object(source_unit.generated_class())
    skills = list(source_defaults.get_editor_property("equipped_skill_data_assets"))
    montages = list(dict.fromkeys(skill_montage(skill) for skill in skills))
    require(all(montages), "Every existing skill needs its authored montage")
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    source_animation = load(UNARMED_SOURCE + "/ABP_Unarmed")
    sword_manny = load(SWORD_MANNY)
    weapon = load(mirrored_path(WEAPON_SOURCE))
    party = load(PARTY_PATH)
    definitions = dict(party.get_editor_property("professions"))
    classes = dict(party.get_editor_property("player_unit_classes"))
    catalog = load(SNAPSHOT_CATALOG)
    snapshot_classes = dict(catalog.get_editor_property("enemy_classes"))
    previews = {}
    for profession, hero, mesh_name in PROFESSIONS:
        unreal.log("PROFESSION_APPEARANCE_BEGIN " + profession)
        mesh = load(hero_root(hero) + "/Meshes/" + mesh_name)
        suffix = "_" + hero
        rigs = mirrored_path(hero_root(hero) + "/Rigs")
        inputs = [source_animation] + montages + [sword_manny, load(SWORD_SOURCE), load(SWORD_RECOVERY_SOURCE)]
        converted = retarget(source_mesh, mesh, rigs, suffix, inputs)
        animation = converted[source_animation]
        require(HELPER.remove_legacy_foot_ik(animation), "Could not remove Manny-only foot rig")
        require(HELPER.ensure_output_slot(animation, "DefaultSlot"), "Could not connect montage output slot")
        save(animation)
        idle = load(mirrored_path(UNARMED_SOURCE + "/MM_Idle", suffix))
        unit = duplicate(source_unit.get_path_name(), unit_path(profession))
        overrides = {montage: converted[montage] for montage in montages}
        overrides[load(WARRIOR_MONTAGE)] = converted[sword_manny]
        configure_combat_blueprint(unit, mesh, animation, overrides, hero, idle, staff, weapon)
        snapshot = duplicate(ROOT + "/Blueprint/Unit/BP_SnapshotOpponent", snapshot_path(profession))
        configure_combat_blueprint(snapshot, mesh, animation, overrides, hero, idle, staff, weapon)
        snapshot_classes[unreal.Name(profession)] = snapshot.generated_class()
        if ASSETS.does_asset_exist(preview_path(profession)):
            preview = load(preview_path(profession))
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", unreal.SkeletalMeshActor)
            preview = require(TOOLS.create_asset("BP_" + profession + "MenuPreview", ROOT + "/Blueprint/UI", unreal.Blueprint, factory), "Could not create profession menu preview")
        preview_defaults = unreal.get_default_object(preview.generated_class())
        preview_defaults.set_actor_enable_collision(False)
        preview_mesh = preview_defaults.get_editor_property("skeletal_mesh_component")
        preview_mesh.set_editor_property("skeletal_mesh_asset", mesh)
        preview_mesh.set_editor_property("override_materials", [])
        preview_mesh.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
        preview_mesh.override_animation_data(idle, True, True, 0.0, 1.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
        configure_embedded_weapons(preview, hero)
        if profession == "Mage":
            configure_staff(preview, mesh, idle, staff)
        unreal.BlueprintEditorLibrary.compile_blueprint(preview)
        save(preview)
        previews[unreal.Name(profession)] = preview.generated_class()
        definition = definitions[unreal.Name(profession)]
        definition.set_editor_property("combat_class", unit.generated_class())
        definitions[unreal.Name(profession)] = definition
        classes[unreal.Name(profession)] = unit.generated_class()
    party.set_editor_property("professions", definitions)
    party.set_editor_property("player_unit_classes", classes)
    save(party)
    catalog.set_editor_property("enemy_classes", snapshot_classes)
    save(catalog)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MENU_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stages = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage)]
    require(len(stages) == 1, "MainMenu requires exactly one existing preview stage")
    stages[0].set_editor_property("preview_actor_classes", previews)
    require(unreal.EditorLoadingAndSavingUtils.save_map(world, MENU_PATH), "Could not save profession preview mapping")


def verify():
    party = load(PARTY_PATH)
    world = unreal.EditorLoadingAndSavingUtils.load_map(MENU_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    stages = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MainMenuPreviewStage)]
    require(len(stages) == 1, "Expected one preview stage")
    sword = load(SKILLS + "/BPDA_swoard_attack")
    source_skills = list(unreal.get_default_object(load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class()).get_editor_property("equipped_skill_data_assets"))
    catalog = load(SNAPSHOT_CATALOG)
    for profession, hero, mesh_name in PROFESSIONS:
        mesh = load(hero_root(hero) + "/Meshes/" + mesh_name)
        skeleton = mesh.get_editor_property("skeleton")
        unit = load(unit_path(profession))
        defaults = unreal.get_default_object(unit.generated_class())
        component = defaults.get_editor_property("mesh")
        animation = load(mirrored_path(UNARMED_SOURCE + "/ABP_Unarmed", "_" + hero))
        require(component.get_editor_property("skeletal_mesh_asset") == mesh, "Profession does not reference original mesh")
        require(animation.get_editor_property("target_skeleton") == skeleton and component.get_editor_property("anim_class") == animation.generated_class(), "Profession animation skeleton mismatch")
        require(HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Missing output montage slot")
        overrides = defaults.get_editor_property("round_montage_overrides")
        for skill in source_skills + [sword]:
            original = skill_montage(skill)
            montage = overrides.get(original, original)
            require(montage and montage.get_editor_property("skeleton") == skeleton, "Skill montage does not use hero skeleton: " + skill.get_name())
            require("DefaultSlot" in [str(slot) for slot in unreal.AnimationLibrary.get_montage_slot_names(montage)], "Missing skill montage slot")
        sword_montage = overrides[load(WARRIOR_MONTAGE)]
        require(abs(sword_montage.get_editor_property("sequence_length") - load(SWORD_MANNY).get_editor_property("sequence_length")) < 0.001, "Sword timing changed")
        for index in range(41):
            endpoints = list(HELPER.sample_weapon_blade(unit, sword, 0.23 + index * 0.005))
            require(len(endpoints) == 2 and all(math.isfinite(value) for point in endpoints for value in [point.x, point.y, point.z]), "Sword blade pose sampling failed")
            require(90.0 < (endpoints[1] - endpoints[0]).length() < 100.0, "Sword blade length changed")
        require(party.get_editor_property("professions")[unreal.Name(profession)].get_editor_property("combat_class") == unit.generated_class(), "Profession combat class missing")
        require(party.get_editor_property("player_unit_classes")[unreal.Name(profession)] == unit.generated_class(), "Profession fallback class missing")
        snapshot = load(snapshot_path(profession))
        snapshot_defaults = unreal.get_default_object(snapshot.generated_class())
        snapshot_mesh = snapshot_defaults.get_editor_property("mesh")
        require(catalog.get_editor_property("enemy_classes")[unreal.Name(profession)] == snapshot.generated_class(), "Snapshot profession class missing")
        require(snapshot_mesh.get_editor_property("skeletal_mesh_asset") == mesh and snapshot_mesh.get_editor_property("anim_class") == animation.generated_class(), "Snapshot appearance differs from player")
        require(dict(snapshot_defaults.get_editor_property("round_montage_overrides")) == dict(overrides), "Snapshot montage mapping differs from player")
        for combat_blueprint in [unit, snapshot]:
            combat_defaults = unreal.get_default_object(combat_blueprint.generated_class())
            references = combat_defaults.get_editor_property("weapon_presentation_skills")
            reference_paths = [reference.get_path_name() if isinstance(reference, unreal.Object) else str(reference) for reference in references]
            require(reference_paths == [sword.get_path_name()], "Inherited sword presentation skill missing: " + str(reference_paths))
            for index in range(41):
                endpoints = list(HELPER.sample_weapon_blade(combat_blueprint, sword, 0.23 + index * 0.005))
                require(len(endpoints) == 2 and all(math.isfinite(value) for point in endpoints for value in [point.x, point.y, point.z]), "Combat blade pose sampling failed")
        preview = load(preview_path(profession))
        require(stages[0].get_editor_property("preview_actor_classes")[unreal.Name(profession)] == preview.generated_class(), "Profession preview not mapped")
        preview_defaults = unreal.get_default_object(preview.generated_class())
        preview_mesh = preview_defaults.get_editor_property("skeletal_mesh_component")
        require(preview_mesh.get_editor_property("skeletal_mesh_asset") == mesh, "Preview mesh mismatch")
        playback = preview_mesh.get_editor_property("animation_data")
        idle = playback.get_editor_property("anim_to_play")
        require(idle and idle.get_editor_property("skeleton") == skeleton and playback.get_editor_property("saved_looping") and playback.get_editor_property("saved_playing"), "Preview idle playback mismatch")
        for blueprint in [unit, preview, snapshot]:
            appearance = next((obj for handle, obj, name in component_templates(blueprint) if isinstance(obj, unreal.CharacterAppearanceComponent)), None)
            hidden_bones = [str(bone) for bone in appearance.get_editor_property("hidden_mesh_bones")] if appearance else []
            require(hidden_bones == HIDDEN_WEAPON_BONES[hero], "Embedded weapon visibility differs from unarmed start")
            require(all(component.get_bone_index(bone) >= 0 for bone in hidden_bones), "An embedded weapon bone does not exist: " + hero)
        if profession == "Mage":
            for blueprint in [unit, preview, snapshot]:
                staff = next((obj for handle, obj, name in component_templates(blueprint) if name == "Staff"), None)
                require(staff and staff.get_editor_property("static_mesh") == load(STAFF_PATH) and str(HELPER.get_weapon_attachment(blueprint, "Staff")) == "hand_l", "Mage staff attachment mismatch")
                require(staff.get_editor_property("visible") and staff.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Staff must stay visible without combat collision")
        REPORT["professions"].append({"profession": profession, "hero": hero, "mesh": mesh.get_path_name(), "unit": unit.get_path_name(), "snapshot": snapshot.get_path_name(), "preview": preview.get_path_name(), "animation": animation.get_path_name(), "sword_samples_per_unit": 41})


if __name__ == "__main__":
    verify_only = "-ProfessionAppearanceVerifyOnly" in unreal.SystemLibrary.get_command_line()
    try:
        if not verify_only:
            configure()
        verify()
        report_path = Path(unreal.Paths.project_saved_dir(), "Automation", "ProfessionAppearanceReload.json" if verify_only else "ProfessionAppearanceConfigure.json")
        report_path.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log("PROFESSION_APPEARANCE_COMPLETE " + str(report_path))
    finally:
        if not verify_only:
            unreal.SystemLibrary.quit_editor()
