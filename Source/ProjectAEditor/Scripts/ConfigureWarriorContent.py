import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureSweepingStrike import configure_sweeping_strike
from RetargetContentLibrary import retarget, verify_retarget_motion
from WarriorContentPaths import ENEMY_RIGS, ENEMY_SOURCE, ROOT, SWORD_FOLDER, SWORD_RECOVERY_SOURCE, SWORD_SOURCE, SWORD_SOURCE_MESH, SWORD_SUFFIX, UNARMED_SOURCE, WARRIOR_MONTAGE, WARRIOR_RIGS, WARRIOR_SOURCE, WEAPON_SOURCE, animation_sources, legacy_moves, migrate_legacy_assets, mirrored_path, retarget_output_path

SKILLS = ROOT + "/Blueprint/DataAsset/Skills"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
HELPER = unreal.WarriorAssetLibrary
REPORT = {"gameplay_test": "not run", "retargeted": [], "units": []}
SWORD_WINDUP_SECONDS = 0.23
SWORD_TRACE_SECONDS = 0.2
SWORD_TRACE_RADIUS = 4.0
SWORD_BLADE_BASE = (0.0, 0.0, -22.0)
SWORD_BLADE_TIP = (0.0, 0.191992, -118.28656)
SWORD_WARRIOR_GRIP = (-11.095651, 5.605028, -10.0)
SWORD_ENEMY_GRIP = (-8.5, 5.0, -10.0)
SWORD_RECOVERY_START_SECONDS = 0.2
SWORD_MONTAGE_SECONDS = 1.933333


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


def project_mesh(source_path):
    mesh = load(source_path)
    skeleton = require(mesh.get_editor_property("skeleton"), "Source mesh has no skeleton: " + source_path)
    return mesh, skeleton


def configure_weapon(blueprint, weapon, grip):
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
    # The pivot is the handle center; place the guard ahead of the thumb with the blade along hand +Z.
    # 피벗은 손잡이 중앙이므로 검막이를 엄지 앞에 놓고 칼날을 손의 +Z 방향으로 정렬합니다.
    component.set_editor_property("relative_location", unreal.Vector(*grip))
    component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=0.0, roll=180.0))
    component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    require(HELPER.set_weapon_attachment(blueprint, "Sword", "hand_r"), "Could not persist sword attachment")


def configure_blade_sockets(weapon):
    for name, location in [("BladeBase", SWORD_BLADE_BASE), ("BladeTip", SWORD_BLADE_TIP)]:
        socket = weapon.find_socket(name)
        if not socket:
            socket = unreal.new_object(unreal.StaticMeshSocket, outer=weapon)
            socket.set_editor_property("socket_name", name)
            weapon.add_socket(socket)
        socket.set_editor_property("relative_location", unreal.Vector(*location))
        socket.set_editor_property("relative_rotation", unreal.Rotator())
        socket.set_editor_property("relative_scale", unreal.Vector(1.0, 1.0, 1.0))
    save(weapon)


def configure_unit(blueprint, mesh, animation, skills, overrides, weapon):
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    component.set_editor_property("skeletal_mesh_asset", mesh)
    component.set_editor_property("anim_class", animation.generated_class())
    defaults.set_editor_property("equipped_skill_data_assets", skills)
    defaults.set_editor_property("round_montage_overrides", overrides)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    configure_weapon(blueprint, weapon, SWORD_WARRIOR_GRIP if mesh == load(WARRIOR_SOURCE) else SWORD_ENEMY_GRIP)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    REPORT["units"].append(blueprint.get_path_name())


def configure():
    REPORT["folder_moves"] = migrate_legacy_assets()
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
    warrior_mesh, warrior_skeleton = project_mesh(WARRIOR_SOURCE)
    enemy_mesh, enemy_skeleton = project_mesh(ENEMY_SOURCE)
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    source_abp = load(UNARMED_SOURCE + "/ABP_Unarmed")
    warrior_retargets = retarget(source_mesh, warrior_mesh, WARRIOR_RIGS, "_Warrior", [source_abp] + montages, report=REPORT, force_rebuild="-WarriorRebuildRetargets" in unreal.SystemLibrary.get_command_line(), anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path, output_path=retarget_output_path, save_asset=save)
    enemy_retargets = retarget(source_mesh, enemy_mesh, ENEMY_RIGS, "_SwordEnemy", [source_abp] + montages, report=REPORT, force_rebuild="-WarriorRebuildRetargets" in unreal.SystemLibrary.get_command_line(), anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path, output_path=retarget_output_path, save_asset=save)
    for animation in [warrior_retargets[source_abp], enemy_retargets[source_abp]]:
        require(HELPER.remove_legacy_foot_ik(animation), "Could not remove the incompatible Manny foot rig")
        require(HELPER.ensure_output_slot(animation, "DefaultSlot"), "Montage output slot is missing")
        save(animation)
    source_attack = load(SWORD_SOURCE)
    source_recovery = load(SWORD_RECOVERY_SOURCE)
    sword_retargets = retarget(load(SWORD_SOURCE_MESH), warrior_mesh, WARRIOR_RIGS, SWORD_SUFFIX, [source_attack, source_recovery], report=REPORT, force_rebuild="-WarriorRebuildRetargets" in unreal.SystemLibrary.get_command_line(), anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path, output_path=retarget_output_path, save_asset=save)
    attack = sword_retargets[source_attack]
    recovery = sword_retargets[source_recovery]
    montage_path = WARRIOR_MONTAGE
    if ASSETS.does_asset_exist(montage_path):
        montage = load(montage_path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", warrior_skeleton)
        factory.set_editor_property("source_animation", attack)
        montage = require(TOOLS.create_asset("AM_SwordAttack", SWORD_FOLDER, unreal.AnimMontage, factory), "Could not create sword montage")
    require(HELPER.configure_sword_montage(montage, attack, recovery, SWORD_RECOVERY_START_SECONDS), "Could not configure Kwang attack and recovery")
    require(list(HELPER.get_montage_animations(montage)) == [attack, recovery], "Sword montage must play Kwang attack and recovery")
    save(montage)
    enemy_sword = retarget(warrior_mesh, enemy_mesh, ENEMY_RIGS, "_Sword", [montage], report=REPORT, force_rebuild="-WarriorRebuildRetargets" in unreal.SystemLibrary.get_command_line(), anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path, output_path=retarget_output_path, save_asset=save)[montage]
    enemy_attack = load(mirrored_path(SWORD_SOURCE, SWORD_SUFFIX + "_Sword"))
    enemy_recovery = load(mirrored_path(SWORD_RECOVERY_SOURCE, SWORD_SUFFIX + "_Sword"))
    require(HELPER.configure_sword_montage(enemy_sword, enemy_attack, enemy_recovery, SWORD_RECOVERY_START_SECONDS), "Could not configure enemy Kwang attack and recovery")
    save(enemy_sword)
    sword = duplicate(basic.get_path_name(), SKILLS + "/BPDA_swoard_attack")
    sword.set_editor_property("skill_id", "SwordAttack")
    sword.set_editor_property("skill_name", "검 공격")
    profile = unreal.CombatRoundSkill()
    profile.set_editor_property("cast_montage", montage)
    profile.set_editor_property("kind", unreal.CombatRoundSkillKind.MELEE)
    profile.set_editor_property("approach", unreal.CombatRoundApproach.UNIT)
    # Kwang's main swing peaks near 0.23 seconds; recovery skips its duplicated first 0.2 seconds.
    # Kwang의 주 타격 구간은 약 0.23초이며 회복 동작의 처음 0.2초는 중복 구간이므로 생략합니다.
    profile.set_editor_property("windup_seconds", SWORD_WINDUP_SECONDS)
    profile.set_editor_property("use_weapon_trace", True)
    profile.set_editor_property("weapon_component_name", "Sword")
    profile.set_editor_property("weapon_base_socket", "BladeBase")
    profile.set_editor_property("weapon_tip_socket", "BladeTip")
    profile.set_editor_property("weapon_montage_slot", "DefaultSlot")
    profile.set_editor_property("weapon_trace_duration", SWORD_TRACE_SECONDS)
    profile.set_editor_property("weapon_trace_radius", SWORD_TRACE_RADIUS)
    profile.set_editor_property("power", unreal.get_default_object(basic.get_editor_property("ability_class")).get_editor_property("damage_amount"))
    profile.set_editor_property("action_point_cost", basic.get_editor_property("action_point_cost"))
    sword.set_editor_property("round_definition", profile)
    sword.set_editor_property("use_round_definition", True)
    sword.set_editor_property("move_to_target", True)
    sword.set_editor_property("target_rule", unreal.SkillTargetRule.ENEMY_UNIT)
    sword.set_editor_property("area_type", unreal.SkillAreaType.SINGLE)
    sword.set_editor_property("area_radius", 0)
    sword.set_editor_property("skill_description", "대상에게 접근해 칼날이 닿은 첫 적을 한 번 타격한 뒤 원래 자리로 복귀합니다.")
    save(sword)
    weapon = duplicate(WEAPON_SOURCE, mirrored_path(WEAPON_SOURCE))
    configure_blade_sockets(weapon)
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
    REPORT["sword_source"] = SWORD_SOURCE
    REPORT["sword_recovery_source"] = SWORD_RECOVERY_SOURCE
    REPORT["sword_seconds"] = montage.get_editor_property("sequence_length")
    dirty_packages = [package for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() if package.get_name() not in REPORT["folder_moves"]]
    for package in dirty_packages:
        require(package.get_name().startswith(ROOT + "/"), "Unexpected dirty external package: " + package.get_name())
    require(not dirty_packages or unreal.EditorLoadingAndSavingUtils.save_packages(dirty_packages, True), "Could not save moved asset references")


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
    require(abs(profile.get_editor_property("windup_seconds") - SWORD_WINDUP_SECONDS) < 0.001, "Sword release does not match the Kwang swing")
    require(profile.get_editor_property("use_weapon_trace") and not profile.get_editor_property("use_melee_area_collision"), "Sword must use blade contact instead of forward area collision")
    require(abs(profile.get_editor_property("weapon_trace_duration") - SWORD_TRACE_SECONDS) < 0.001 and abs(profile.get_editor_property("weapon_trace_radius") - SWORD_TRACE_RADIUS) < 0.001, "Sword trace window or thickness changed")
    for field, expected_name in [("weapon_component_name", "Sword"), ("weapon_base_socket", "BladeBase"), ("weapon_tip_socket", "BladeTip"), ("weapon_montage_slot", "DefaultSlot")]:
        require(str(profile.get_editor_property(field)) == expected_name, "Sword blade binding mismatch: " + field)
    weapon = load(mirrored_path(WEAPON_SOURCE))
    for socket_name, expected_location in [("BladeBase", SWORD_BLADE_BASE), ("BladeTip", SWORD_BLADE_TIP)]:
        socket = require(weapon.find_socket(socket_name), "Missing blade socket")
        location = socket.get_editor_property("relative_location")
        require(math.dist((location.x, location.y, location.z), expected_location) < 0.001, "Blade socket does not match sword geometry")
    require(abs(montage.get_editor_property("sequence_length") - SWORD_MONTAGE_SECONDS) < 0.001, "Kwang attack and recovery length changed")
    require(profile.get_editor_property("action_point_cost") == basic.get_editor_property("action_point_cost"), "Sword AP was not preserved")
    expected = [basic, load(SKILLS + "/BPDA_RangedAttack"), load(SKILLS + "/BPDA_AreaAttack"), sweep, sword]
    require(montage == load(WARRIOR_MONTAGE), "Sword montage path does not preserve the Kwang folder")
    require(list(HELPER.get_montage_animations(montage)) == [load(mirrored_path(source, SWORD_SUFFIX)) for source in [SWORD_SOURCE, SWORD_RECOVERY_SOURCE]], "Warrior sword montage is not the Kwang attack and recovery")
    require(HELPER.validate_sword_montage(montage, load(mirrored_path(SWORD_SOURCE, SWORD_SUFFIX)), load(mirrored_path(SWORD_RECOVERY_SOURCE, SWORD_SUFFIX)), SWORD_RECOVERY_START_SECONDS), "Saved warrior Kwang segment timing or blending mismatch")
    for name, suffix, skills in [("BP_WarriorUnit", "_Warrior", expected), ("BP_EnemyUnit", "_SwordEnemy", [sword]), ("BP_SnapshotOpponent", "_SwordEnemy", [sword])]:
        blueprint = load(ROOT + "/Blueprint/Unit/" + name)
        defaults = unreal.get_default_object(blueprint.generated_class())
        mesh_component = defaults.get_editor_property("mesh")
        mesh = mesh_component.get_editor_property("skeletal_mesh_asset")
        skeleton = mesh.get_editor_property("skeleton")
        expected_mesh = load(WARRIOR_SOURCE if name == "BP_WarriorUnit" else ENEMY_SOURCE)
        require(mesh == expected_mesh, "Mesh does not reference its original pack asset")
        animation = load(mirrored_path(UNARMED_SOURCE + "/ABP_Unarmed", suffix))
        require(animation.get_editor_property("target_skeleton") == skeleton, "AnimBP skeleton mismatch")
        require(mesh_component.get_editor_property("anim_class") == animation.generated_class(), "Unit AnimBP mismatch")
        require(HELPER.is_output_slot_connected(animation, "DefaultSlot"), "Output montage slot disconnected")
        require(list(defaults.get_editor_property("equipped_skill_data_assets")) == skills, "Unexpected loadout for " + name)
        require(str(HELPER.get_weapon_attachment(blueprint, "Sword")) == "hand_r", "Sword socket was not saved")
        overrides = defaults.get_editor_property("round_montage_overrides")
        if name != "BP_WarriorUnit":
            require(list(HELPER.get_montage_animations(overrides[montage])) == [load(mirrored_path(source, SWORD_SUFFIX + "_Sword")) for source in [SWORD_SOURCE, SWORD_RECOVERY_SOURCE]], "Enemy sword montage is not the Kwang attack and recovery")
            require(abs(overrides[montage].get_editor_property("sequence_length") - SWORD_MONTAGE_SECONDS) < 0.001, "Enemy Kwang montage length changed")
            require(HELPER.validate_sword_montage(overrides[montage], load(mirrored_path(SWORD_SOURCE, SWORD_SUFFIX + "_Sword")), load(mirrored_path(SWORD_RECOVERY_SOURCE, SWORD_SUFFIX + "_Sword")), SWORD_RECOVERY_START_SECONDS), "Saved enemy Kwang segment timing or blending mismatch")
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
                grip = component.get_editor_property("relative_location")
                expected_grip = SWORD_WARRIOR_GRIP if name == "BP_WarriorUnit" else SWORD_ENEMY_GRIP
                require(math.dist((grip.x, grip.y, grip.z), expected_grip) < 0.001, "Sword grip was not saved")
                rotation = component.get_editor_property("relative_rotation")
                require(abs(rotation.pitch) < 0.001 and abs(rotation.yaw) < 0.001 and abs(abs(rotation.roll) - 180.0) < 0.001, "Sword blade must face the thumb side")
        require(weapons == [load(mirrored_path(WEAPON_SOURCE)).get_path_name()], "Unexpected weapon mesh list: " + name)
        blade_samples = []
        for sample_index in range(math.ceil(SWORD_TRACE_SECONDS / 0.005) + 1):
            seconds = SWORD_WINDUP_SECONDS + min(sample_index * 0.005, SWORD_TRACE_SECONDS)
            endpoints = list(HELPER.sample_weapon_blade(blueprint, sword, seconds))
            require(len(endpoints) == 2 and all(math.isfinite(value) for point in endpoints for value in [point.x, point.y, point.z]), "Runtime blade sampling failed on the saved Blueprint")
            require(90.0 < math.dist((endpoints[0].x, endpoints[0].y, endpoints[0].z), (endpoints[1].x, endpoints[1].y, endpoints[1].z)) < 100.0, "Blade sample length does not match mesh sockets")
            blade_samples.append({"seconds": seconds, "base": [endpoints[0].x, endpoints[0].y, endpoints[0].z], "tip": [endpoints[1].x, endpoints[1].y, endpoints[1].z]})
        REPORT.setdefault("weapon_blade_samples", {})[name] = blade_samples
        REPORT["units"].append({"blueprint": blueprint.get_path_name(), "mesh": mesh.get_path_name(), "animation": animation.get_path_name(), "skills": [skill.get_path_name() for skill in skills], "weapon": weapons[0]})
    party = load(ROOT + "/Blueprint/DataAsset/Parties/DA_VerticalSliceParty")
    warrior_class = load(ROOT + "/Blueprint/Unit/BP_WarriorUnit").generated_class()
    require(party.get_editor_property("professions")[unreal.Name("Warrior")].get_editor_property("combat_class") == warrior_class, "Warrior profession not bound")
    classes = party.get_editor_property("player_unit_classes")
    require(classes[unreal.Name("Warrior")] == warrior_class, "Warrior class map not bound")
    player_class = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit").generated_class()
    for profession, blueprint_name in [("Mage", "BP_MageUnit"), ("Archer", "BP_PlayerUnit"), ("Rogue", "BP_RogueUnit")]:
        require(classes[unreal.Name(profession)] == load(ROOT + "/Blueprint/Unit/" + blueprint_name).generated_class(), "Another profession class changed: " + profession)
    require(len(unreal.get_default_object(player_class).get_editor_property("equipped_skill_data_assets")) == 4, "Shared player loadout changed")
    for path in [SKILLS + "/DA_SweepingStrike", ROOT + "/Blueprint/DataAsset/DA_SweepingStrike"]:
        old_reference = unreal.SoftObjectPath(path + ".DA_SweepingStrike")
        require(HELPER.load_saved_asset_reference(old_reference) == sweep, "Historical Sweeping Strike reference does not redirect")
    REPORT["sword_profile"] = profile.export_text()
    REPORT["folder_moves"] = legacy_moves()
    for old, new in REPORT["folder_moves"].items():
        require(ASSETS.does_asset_exist(new), "Missing mirrored working copy: " + new)
        entries = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(old)
        require(not entries, "Old flat folder still contains an asset or redirector: " + old)
        old_reference = unreal.SoftObjectPath(old + "." + old.rsplit("/", 1)[1])
        require(HELPER.load_saved_asset_reference(old_reference) == load(new), "Moved working copy reference does not redirect: " + old)
    REPORT["legacy_references_verified"] = len(REPORT["folder_moves"])
    REPORT["sword_source"] = SWORD_SOURCE
    REPORT["sword_recovery_source"] = SWORD_RECOVERY_SOURCE
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    warrior_mesh = load(WARRIOR_SOURCE)
    enemy_mesh = load(ENEMY_SOURCE)
    source_abp = load(UNARMED_SOURCE + "/ABP_Unarmed")
    verify_retarget_motion(source_mesh, warrior_mesh, WARRIOR_RIGS, "_Warrior", [source_abp], report=REPORT, anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path)
    verify_retarget_motion(source_mesh, enemy_mesh, ENEMY_RIGS, "_SwordEnemy", [source_abp], report=REPORT, anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path)
    verify_retarget_motion(load(SWORD_SOURCE_MESH), warrior_mesh, WARRIOR_RIGS, SWORD_SUFFIX, [load(SWORD_SOURCE), load(SWORD_RECOVERY_SOURCE)], report=REPORT, anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path)
    verify_retarget_motion(warrior_mesh, enemy_mesh, ENEMY_RIGS, "_Sword", [montage], report=REPORT, anim_blueprint_sources=animation_sources().values(), mirror_path=mirrored_path)
    REPORT["verification"] = "Saved asset reload, original folder structure, Kwang segments, retarget op stack and pelvis travel, skeleton, loadout, socket, slot, profession and historical reference checks passed"


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
