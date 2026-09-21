import json
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureSweepingStrike import configure_sweeping_strike

ROOT = "/Game/User_JeHoon"
SKILLS = ROOT + "/Blueprint/DataAsset/Skills"
WARRIOR = ROOT + "/Characters/Warrior"
ENEMY = ROOT + "/Characters/SwordEnemy"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
HELPER = unreal.WarriorAssetLibrary
REPORT = {"gameplay_test": "not run", "retargeted": [], "units": []}


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def save(asset):
    require(asset.get_path_name().startswith(ROOT + "/"), "Refusing to save external source " + asset.get_path_name())
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save " + asset.get_path_name())


def duplicate(source, destination):
    require(destination.startswith(ROOT + "/"), "Copies must be project-owned")
    return load(destination) if ASSETS.does_asset_exist(destination) else require(ASSETS.duplicate_asset(source, destination), "Could not duplicate " + source)


def rig(mesh, directory, name):
    path = directory + "/" + name
    if ASSETS.does_asset_exist(path):
        return load(path)
    result = require(unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(directory, name), "Could not create IK Rig")
    controller = unreal.IKRigController.get_controller(result)
    require(controller.set_skeletal_mesh(mesh), "IK preview mesh rejected")
    require(controller.apply_auto_generated_retarget_definition(), "No humanoid retarget template")
    require(controller.apply_auto_fbik(), "No humanoid FBIK template")
    save(result)
    return result


def retarget(source_mesh, target_mesh, directory, suffix, inputs):
    source_rig = rig(source_mesh, directory, "IK_Source" + suffix)
    target_rig = rig(target_mesh, directory, "IK_Target" + suffix)
    path = directory + "/RTG" + suffix
    if ASSETS.does_asset_exist(path):
        retargeter = load(path)
    else:
        retargeter = require(TOOLS.create_asset("RTG" + suffix, directory, unreal.IKRetargeter, unreal.IKRetargetFactory()), "Could not create retargeter")
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        for side, selected_rig, mesh in [(unreal.RetargetSourceOrTarget.SOURCE, source_rig, source_mesh), (unreal.RetargetSourceOrTarget.TARGET, target_rig, target_mesh)]:
            controller.set_ik_rig(side, selected_rig)
            controller.set_preview_mesh(side, mesh)
        controller.add_default_ops()
        controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
        save(retargeter)
    pending = [asset for asset in inputs if not ASSETS.does_asset_exist(directory + "/" + asset.get_name() + suffix)]
    if pending:
        previous_paths = set(ASSETS.list_assets(directory, recursive=False, include_folder=False))
        require(HELPER.retarget_animations(pending, source_mesh, target_mesh, retargeter, directory, suffix), "IK retarget failed")
        for path in ASSETS.list_assets(directory, recursive=False, include_folder=False):
            asset = load(path)
            if path in previous_paths:
                continue
            require(asset.get_path_name().startswith(directory + "/"), "Unexpected retarget output path")
            save(asset)
            REPORT["retargeted"].append(asset.get_path_name())
    return {asset: load(directory + "/" + asset.get_name() + suffix) for asset in inputs}


def project_mesh(source_path, directory, mesh_name, skeleton_name):
    source = load(source_path)
    skeleton = duplicate(source.get_editor_property("skeleton").get_path_name(), directory + "/" + skeleton_name)
    mesh = duplicate(source_path, directory + "/" + mesh_name)
    require(HELPER.assign_mesh_skeleton(mesh, skeleton), "Could not assign copied skeleton")
    save(mesh)
    save(skeleton)
    return mesh, skeleton


def configure_weapon(blueprint, weapon):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    parent = None
    component = None
    for handle in handles:
        data = library.get_data(handle)
        obj = library.get_object_for_blueprint(data, blueprint)
        if isinstance(obj, unreal.SkeletalMeshComponent):
            parent = handle
        if str(library.get_variable_name(data)) == "Sword":
            component = obj
    require(parent is not None, "No character mesh for sword")
    if component is None:
        handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.StaticMeshComponent, blueprint_context=blueprint))
        require(not str(reason), "Could not add sword: " + str(reason))
        require(subsystem.rename_subobject(handle, "Sword"), "Could not name sword component")
        component = library.get_object_for_blueprint(library.get_data(handle), blueprint)
    component.set_static_mesh(weapon)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_editor_property("can_ever_affect_navigation", False)
    # The sword mesh points down its local Z axis; align its grip across the right palm.
    # 검 메시의 날은 로컬 Z 음의 방향이며 오른손 손바닥을 가로지르도록 손잡이를 정렬합니다.
    component.set_editor_property("relative_location", unreal.Vector(-11.095651, 5.605028, 0.592877))
    component.set_editor_property("relative_rotation", unreal.Rotator(0.0, 0.0, 180.0))
    component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    require(HELPER.set_weapon_attachment(blueprint, "Sword", "hand_r"), "Could not persist sword attachment")


def configure_unit(blueprint, mesh, animation, skills, overrides, weapon):
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    component.set_editor_property("skeletal_mesh_asset", mesh)
    component.set_editor_property("anim_class", animation.generated_class())
    defaults.set_editor_property("equipped_skill_data_assets", skills)
    defaults.set_editor_property("round_montage_overrides", overrides)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    configure_weapon(blueprint, weapon)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    REPORT["units"].append(blueprint.get_path_name())


def configure():
    old_path = SKILLS + "/DA_SweepingStrike"
    new_path = SKILLS + "/BPDA_SweepingStrike"
    if not ASSETS.does_asset_exist(new_path):
        sweep = load(old_path)
        require(TOOLS.rename_assets([unreal.AssetRenameData(sweep, SKILLS, "BPDA_SweepingStrike")]), "Sweeping Strike rename failed")
    sweep = load(new_path)
    configure_sweeping_strike(sweep)
    save(sweep)
    basic = load(SKILLS + "/BPDA_DefaulatAttack")
    basic.set_editor_property("skill_name", "비무장 공격")
    save(basic)
    previous = [basic, load(SKILLS + "/BPDA_RangedAttack"), load(SKILLS + "/BPDA_AreaAttack"), sweep]
    montages = []
    for skill in previous:
        montage = skill.get_editor_property("round_definition").get_editor_property("cast_montage") if skill.get_editor_property("use_round_definition") else None
        if not montage:
            montage = unreal.get_default_object(skill.get_editor_property("ability_class")).get_editor_property("attack_montage")
        if montage and montage not in montages:
            montages.append(montage)
    warrior_mesh, warrior_skeleton = project_mesh("/Game/GKnight/Meshes/SK_GothicKnight_VA", WARRIOR, "SK_Warrior", "SKEL_Warrior")
    enemy_mesh, enemy_skeleton = project_mesh("/Game/Skeleton_Guard/Mesh_UE4/Full/SKM_Skeleton_Guard_Body", ENEMY, "SK_SwordEnemy", "SKEL_SwordEnemy")
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    source_abp = load("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")
    warrior_retargets = retarget(source_mesh, warrior_mesh, WARRIOR, "_Warrior", [source_abp] + montages)
    enemy_retargets = retarget(source_mesh, enemy_mesh, ENEMY, "_SwordEnemy", [source_abp] + montages)
    for animation, skeleton in [(warrior_retargets[source_abp], warrior_skeleton), (enemy_retargets[source_abp], enemy_skeleton)]:
        require(HELPER.remove_legacy_foot_ik(animation), "Could not remove the incompatible Manny foot rig")
        require(HELPER.ensure_output_slot(animation, "DefaultSlot"), "Montage output slot is missing")
        save(animation)
        save(skeleton)
    source_attack = load("/Game/BossyEnemy/Animations/InPlace/Attacks/Boss_Attack_Swing_InP")
    attack = retarget(load("/Game/BossyEnemy/SkeletalMesh/SK_Mannequin_UE4_WithWeapon"), warrior_mesh, WARRIOR, "_SwordAttack", [source_attack])[source_attack]
    montage_path = WARRIOR + "/AM_SwordAttack"
    if ASSETS.does_asset_exist(montage_path):
        montage = load(montage_path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", warrior_skeleton)
        factory.set_editor_property("source_animation", attack)
        montage = require(TOOLS.create_asset("AM_SwordAttack", WARRIOR, unreal.AnimMontage, factory), "Could not create sword montage")
    save(montage)
    enemy_sword = retarget(warrior_mesh, enemy_mesh, ENEMY, "_Sword", [montage])[montage]
    sword = duplicate(basic.get_path_name(), SKILLS + "/BPDA_swoard_attack")
    sword.set_editor_property("skill_id", "SwordAttack")
    sword.set_editor_property("skill_name", "검 공격")
    profile = unreal.CombatRoundSkill()
    profile.set_editor_property("cast_montage", montage)
    profile.set_editor_property("kind", unreal.CombatRoundSkillKind.MELEE)
    profile.set_editor_property("approach", unreal.CombatRoundApproach.UNIT)
    # Source pose samples place the main sword swing at 2.15 seconds without changing playback speed.
    # 원본 포즈 표본의 주 타격 구간인 2.15초를 사용하며 재생 속도는 변경하지 않습니다.
    profile.set_editor_property("windup_seconds", 2.15)
    profile.set_editor_property("power", unreal.get_default_object(basic.get_editor_property("ability_class")).get_editor_property("damage_amount"))
    profile.set_editor_property("action_point_cost", basic.get_editor_property("action_point_cost"))
    sword.set_editor_property("round_definition", profile)
    sword.set_editor_property("use_round_definition", True)
    sword.set_editor_property("move_to_target", True)
    sword.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
    sword.set_editor_property("area_type", unreal.SkillAreaType.SINGLE)
    sword.set_editor_property("area_radius", 0)
    sword.set_editor_property("skill_description", "대상에게 접근해 검으로 공격한 뒤 원래 자리로 복귀합니다.")
    save(sword)
    weapon = duplicate("/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Sword", ROOT + "/Weapons/SM_Sword")
    save(weapon)
    warrior = duplicate(ROOT + "/Blueprint/Unit/BP_PlayerUnit", ROOT + "/Blueprint/Unit/BP_WarriorUnit")
    configure_unit(warrior, warrior_mesh, warrior_retargets[source_abp], previous + [sword], {key: warrior_retargets[key] for key in montages}, weapon)
    enemy_overrides = {key: enemy_retargets[key] for key in montages}
    enemy_overrides[montage] = enemy_sword
    configure_unit(load(ROOT + "/Blueprint/Unit/BP_EnemyUnit"), enemy_mesh, enemy_retargets[source_abp], [sword], enemy_overrides, weapon)
    # Snapshot commands continue to use their saved loadout; only the visual/default unit setup changes.
    # Snapshot 명령은 저장된 장착을 유지하며 유닛의 표현과 기본 구성만 변경합니다.
    configure_unit(load(ROOT + "/Blueprint/Unit/BP_SnapshotOpponent"), enemy_mesh, enemy_retargets[source_abp], [sword], enemy_overrides, weapon)
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    professions = dict(party.get_editor_property("professions"))
    warrior_definition = professions[unreal.Name("Warrior")]
    warrior_definition.set_editor_property("combat_class", warrior.generated_class())
    if not warrior_definition.get_editor_property("use_unit_class_defaults"):
        warrior_definition.set_editor_property("starting_skills", previous + [sword])
    professions[unreal.Name("Warrior")] = warrior_definition
    party.set_editor_property("professions", professions)
    classes = dict(party.get_editor_property("player_unit_classes"))
    classes[unreal.Name("Warrior")] = warrior.generated_class()
    party.set_editor_property("player_unit_classes", classes)
    save(party)
    catalog = load(ROOT + "/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog")
    catalog_skills = dict(catalog.get_editor_property("skills"))
    catalog_skills[unreal.Name("SwordAttack")] = sword
    catalog.set_editor_property("skills", catalog_skills)
    save(catalog)
    REPORT["sword_profile"] = profile.export_text()
    REPORT["sword_source"] = "/Game/BossyEnemy/Animations/InPlace/Attacks/Boss_Attack_Swing_InP"
    REPORT["sword_seconds"] = attack.get_editor_property("sequence_length")


def verify():
    sword = load(SKILLS + "/BPDA_swoard_attack")
    basic = load(SKILLS + "/BPDA_DefaulatAttack")
    sweep = load(SKILLS + "/BPDA_SweepingStrike")
    require(str(basic.get_editor_property("skill_name")) == "비무장 공격", "Unarmed display name mismatch")
    require(str(basic.get_editor_property("skill_id")) == "DeafaultAttack", "Existing basic attack identity changed")
    require(str(sweep.get_editor_property("skill_name")) == "휩쓸기", "Sweeping Strike display changed")
    require(sweep.get_editor_property("round_definition").get_editor_property("use_melee_area_collision"), "Sweeping Strike collision is not enabled")
    profile = sword.get_editor_property("round_definition")
    montage = profile.get_editor_property("cast_montage")
    require(sword.get_editor_property("use_round_definition") and profile.get_editor_property("kind") == unreal.CombatRoundSkillKind.MELEE, "Sword attack must use a melee profile")
    require(abs(profile.get_editor_property("windup_seconds") - 2.15) < 0.001, "Sword release does not match the authored swing")
    require(abs(montage.get_editor_property("sequence_length") - 5.866667) < 0.001, "Source attack length changed")
    require(profile.get_editor_property("action_point_cost") == basic.get_editor_property("action_point_cost"), "Sword AP was not preserved")
    expected = [basic, load(SKILLS + "/BPDA_RangedAttack"), load(SKILLS + "/BPDA_AreaAttack"), sweep, sword]
    for name, directory, skills in [("BP_WarriorUnit", WARRIOR, expected), ("BP_EnemyUnit", ENEMY, [sword]), ("BP_SnapshotOpponent", ENEMY, [sword])]:
        blueprint = load(ROOT + "/Blueprint/Unit/" + name)
        defaults = unreal.get_default_object(blueprint.generated_class())
        mesh_component = defaults.get_editor_property("mesh")
        mesh = mesh_component.get_editor_property("skeletal_mesh_asset")
        skeleton = mesh.get_editor_property("skeleton")
        animation = load(directory + ("/ABP_Unarmed_Warrior" if name == "BP_WarriorUnit" else "/ABP_Unarmed_SwordEnemy"))
        require(animation.get_editor_property("target_skeleton") == skeleton, "AnimBP skeleton mismatch")
        require(mesh_component.get_editor_property("anim_class") == animation.generated_class(), "Unit AnimBP mismatch")
        require(HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Output montage slot disconnected")
        require(list(defaults.get_editor_property("equipped_skill_data_assets")) == skills, "Unexpected loadout for " + name)
        require(str(HELPER.get_weapon_attachment(blueprint, "Sword")) == "hand_r", "Sword socket was not saved")
        overrides = defaults.get_editor_property("round_montage_overrides")
        for skill in skills:
            skill_montage = skill.get_editor_property("round_definition").get_editor_property("cast_montage") if skill.get_editor_property("use_round_definition") else None
            if not skill_montage:
                skill_montage = unreal.get_default_object(skill.get_editor_property("ability_class")).get_editor_property("attack_montage")
            resolved = overrides.get(skill_montage, skill_montage)
            require(resolved and resolved.get_editor_property("skeleton") == skeleton, "Skill montage skeleton mismatch: " + skill.get_name())
            require("DefaultSlot" in [str(slot) for slot in unreal.AnimationLibrary.get_montage_slot_names(resolved)], "Unexpected montage slot")
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        weapons = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
            data = library.get_data(handle)
            component = library.get_object_for_blueprint(data, blueprint)
            if isinstance(component, unreal.StaticMeshComponent) and component.get_editor_property("static_mesh"):
                weapons.append(component.get_editor_property("static_mesh").get_path_name())
        require(weapons == [ROOT + "/Weapons/SM_Sword.SM_Sword"], "Unexpected weapon mesh list: " + name)
        REPORT["units"].append({"blueprint": blueprint.get_path_name(), "mesh": mesh.get_path_name(), "animation": animation.get_path_name(), "skills": [skill.get_path_name() for skill in skills], "weapon": weapons[0]})
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    warrior_class = load(ROOT + "/Blueprint/Unit/BP_WarriorUnit").generated_class()
    require(party.get_editor_property("professions")[unreal.Name("Warrior")].get_editor_property("combat_class") == warrior_class, "Warrior profession not bound")
    classes = party.get_editor_property("player_unit_classes")
    require(classes[unreal.Name("Warrior")] == warrior_class, "Warrior class map not bound")
    player_class = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class()
    require(all(classes[unreal.Name(name)] == player_class for name in ["Mage", "Archer", "Rogue"]), "Another profession class changed")
    require(len(unreal.get_default_object(player_class).get_editor_property("equipped_skill_data_assets")) == 4, "Shared player loadout changed")
    for path in [SKILLS + "/DA_SweepingStrike", ROOT + "/Blueprint/DataAsset/DA_SweepingStrike"]:
        old_reference = unreal.SoftObjectPath(path + ".DA_SweepingStrike")
        require(HELPER.load_saved_asset_reference(old_reference) == sweep, "Historical Sweeping Strike reference does not redirect")
    REPORT["sword_profile"] = profile.export_text()
    REPORT["verification"] = "Saved asset reload, skeleton, loadout, socket, slot, profession and historical reference checks passed"


if __name__ == "__main__":
    verify_only = "-WarriorVerifyOnly" in unreal.SystemLibrary.get_command_line()
    if verify_only:
        verify()
    else:
        configure()
    output = Path(unreal.Paths.project_saved_dir(), "Automation", "WarriorContentReload.json" if verify_only else "WarriorContentConfigure.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("WARRIOR_CONTENT_VERIFIED " + str(output) if verify_only else "WARRIOR_CONTENT_CONFIGURED " + str(output))
