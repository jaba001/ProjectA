import json
import math
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from RetargetContentLibrary import retarget, verify_retarget_motion
from WarriorContentPaths import mirrored_path


ROOT = "/Game/User_JeHoon"
DIRECTORY = Path(unreal.Paths.project_dir()).resolve() / "Saved/Automation/Monsters"
SPEC = json.loads(Path(__file__).with_name("MonsterContentSpecs.json").read_text(encoding="utf-8-sig"))
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
HELPER = unreal.MonsterAssetLibrary
VERIFY = "-MonsterVerifyOnly" in unreal.SystemLibrary.get_command_line()
OWNER_KEY = "ProjectA.MonsterContent"
OWNER = "MonsterContent.v1"
BASE_SKILL = ROOT + "/Blueprint/DataAsset/Skills/BPDA_DefaulatAttack"
ENCOUNTER = ROOT + "/Blueprint/DataAsset/Encounters/DA_DefaultEncounter"
LEGACY_ENEMY = ROOT + "/Blueprint/Unit/BP_EnemyUnit"
REPORT = {"mode": "reload" if VERIFY else "author", "gameplay_test": "not run", "sources": SPEC["sources"], "units": [], "retargeted": []}


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def save(asset):
    require(asset.get_path_name().startswith(ROOT + "/"), "Refusing external source save")
    require(not VERIFY, "Verification must not save")
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + asset.get_path_name())


def obtain(path, cls, factory):
    require(path.startswith(ROOT + "/"), "Destination must be project-owned")
    if ASSETS.does_asset_exist(path):
        asset = load(path)
        require(isinstance(asset, cls) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Unowned destination: " + path)
        return asset
    require(not VERIFY, "Missing generated asset: " + path)
    directory, name = path.rsplit("/", 1)
    factory.set_editor_property("edit_after_new", False)
    asset = require(TOOLS.create_asset(name, directory, cls, factory), "Factory failed: " + path)
    ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
    return asset


def near(actual, expected):
    return math.isclose(float(actual), float(expected), abs_tol=0.001)


def normalized(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    return value


def check_sequence(sequence, skeleton):
    require(sequence.get_editor_property("skeleton") == skeleton, "Animation skeleton mismatch: " + sequence.get_path_name())
    require(sequence.get_play_length() > 0.0, "Empty animation")
    # Source notifies must never introduce a second damage path through an imported Blueprint.
    # 원본 Notify가 외부 Blueprint를 통해 별도 피해 경로를 만들지 않도록 확인합니다.
    events = list(unreal.AnimationLibrary.get_animation_notify_events(sequence))
    require(not events, "Inspect source notifies before use: " + sequence.get_path_name())


def attack_for(spec, mesh):
    source = load(spec["attack"])
    if not spec["retarget"]:
        return source
    suffix = "_Monster" + spec["id"]
    rigs = spec["output"].rsplit("/", 1)[0] + "/Rigs"
    source_mesh = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
    if not VERIFY:
        retarget(source_mesh, mesh, rigs, suffix, [source], report=REPORT, force_rebuild=False, anim_blueprint_sources=[], mirror_path=mirrored_path, output_path=lambda name, selected_suffix, inputs: mirrored_path(source.get_path_name(), selected_suffix), save_asset=save)
    verify_retarget_motion(source_mesh, mesh, rigs, suffix, [source], report=REPORT, anim_blueprint_sources=[], mirror_path=mirrored_path)
    return load(mirrored_path(source.get_path_name(), suffix))


def animations_for(spec, mesh, shared):
    if spec.get("share_animation"):
        return shared[spec["share_animation"]]
    skeleton = mesh.get_editor_property("skeleton")
    idle, walk, run = [load(path) for path in spec["locomotion"]]
    attack = attack_for(spec, mesh)
    for sequence in [idle, walk, run, attack]:
        check_sequence(sequence, skeleton)
    factory = unreal.BlendSpaceFactory1D()
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("preview_skeletal_mesh", mesh)
    blend = obtain(spec["animation_output"] + "/BS_Monster_" + spec["id"], unreal.BlendSpace1D, factory)
    operation = HELPER.validate_monster_blend_space if VERIFY else HELPER.configure_monster_blend_space
    require(operation(blend, idle, walk, run, 250.0, 700.0), "Invalid locomotion blend: " + spec["id"])
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MonsterAnimInstance)
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("preview_skeletal_mesh", mesh)
    blueprint = obtain(spec["animation_output"] + "/ABP_Monster_" + spec["id"], unreal.AnimBlueprint, factory)
    operation = HELPER.validate_locomotion_anim_blueprint if VERIFY else HELPER.configure_locomotion_anim_blueprint
    require(operation(blueprint, blend, "DefaultSlot"), "Invalid animation graph: " + spec["id"])
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("target_skeleton", skeleton)
    factory.set_editor_property("source_animation", attack)
    montage = obtain(spec["animation_output"] + "/AM_Monster_" + spec["id"], unreal.AnimMontage, factory)
    require(list(unreal.WarriorAssetLibrary.get_montage_animations(montage)) == [attack], "Attack montage source changed")
    require(HELPER.validate_monster_montage(montage, attack), "Attack montage must use one non-looping DefaultSlot segment")
    require(near(montage.get_play_length(), attack.get_play_length()), "Attack montage length changed")
    require(spec["windup"] < montage.get_play_length(), "Attack release exceeds montage")
    if not VERIFY:
        for asset in [blend, blueprint, montage]:
            save(asset)
    shared[spec["id"]] = (blueprint, montage, blend, attack)
    return shared[spec["id"]]


def skill_for(spec, montage, base):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.SkillDefinitionDataAsset)
    asset = obtain(ROOT + "/Blueprint/DataAsset/Skills/Monsters/DA_Monster_" + spec["id"], unreal.SkillDefinitionDataAsset, factory)
    if not VERIFY:
        # Preserve the resolved GAS contract and existing basic damage; only presentation timing differs.
        # 해석된 GAS 계약과 기존 기본 피해를 보존하고 표현 시간만 몬스터에 맞춥니다.
        profile = HELPER.get_resolved_monster_skill(base)
        profile.set_editor_property("cast_montage", montage)
        profile.set_editor_property("windup_seconds", spec["windup"])
        profile.set_editor_property("kind", unreal.CombatRoundSkillKind.MELEE)
        profile.set_editor_property("approach", unreal.CombatRoundApproach.UNIT)
        asset.set_editor_property("skill_id", "Monster" + spec["id"] + "Attack")
        asset.set_editor_property("skill_name", spec["name"] + " 공격")
        asset.set_editor_property("skill_description", "기본 근접 공격")
        asset.set_editor_property("skill_icon", base.get_editor_property("skill_icon"))
        asset.set_editor_property("ability_class", base.get_editor_property("ability_class"))
        asset.set_editor_property("action_point_cost", base.get_editor_property("action_point_cost"))
        asset.set_editor_property("move_to_target", True)
        asset.set_editor_property("use_round_definition", True)
        asset.set_editor_property("round_definition", profile)
        save(asset)
    require(HELPER.validate_monster_skill(asset), "Invalid GAS skill contract")
    profile = HELPER.get_resolved_monster_skill(asset)
    reference = HELPER.get_resolved_monster_skill(base)
    for field in ["power", "action_point_cost", "sub_action_point_cost", "effect_class", "effect_tags", "source_tag_query", "target_tag_query", "source_required_tags", "source_blocked_tags", "target_required_tags", "target_blocked_tags", "hit_range", "melee_radius", "target_rule", "kind", "approach", "melee_area", "move_speed", "remain_at_destination", "use_weapon_trace", "use_melee_area_collision", "use_effect_collision", "target_loss", "vfx", "impact_vfx"]:
        require(normalized(profile.get_editor_property(field)) == normalized(reference.get_editor_property(field)), "Basic attack contract changed: " + field)
    require(asset.get_editor_property("ability_class") == base.get_editor_property("ability_class") and asset.get_editor_property("use_round_definition"), "GAS ability contract source changed")
    require(profile.get_editor_property("cast_montage") == montage and near(profile.get_editor_property("windup_seconds"), spec["windup"]), "Attack presentation mismatch")
    return asset


def unit_for(spec, mesh, animation, skill):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.EnemyUnit)
    blueprint = obtain(spec["output"] + "/BP_Monster_" + spec["id"], unreal.Blueprint, factory)
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    capsule = defaults.get_editor_property("capsule_component")
    bounds = mesh.get_bounds()
    scale = spec.get("scale", spec.get("height_cm", 180.0) / (bounds.box_extent.z * 2.0))
    require(0.1 <= scale <= 2.0, "Invalid monster scale")
    ground = -spec["capsule_half_height"] - (bounds.origin.z - bounds.box_extent.z) * scale
    material = load(spec["material"]) if spec["material"] else None
    if not VERIFY:
        component.set_editor_property("skeletal_mesh_asset", mesh)
        component.set_editor_property("anim_class", animation.generated_class())
        component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
        component.set_editor_property("relative_location", unreal.Vector(0, 0, ground))
        component.set_editor_property("relative_rotation", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0))
        component.set_editor_property("relative_scale3d", unreal.Vector(scale, scale, scale))
        component.set_editor_property("can_ever_affect_navigation", False)
        component.set_editor_property("enable_update_rate_optimizations", True)
        component.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ONLY_TICK_MONTAGES_WHEN_NOT_RENDERED)
        component.set_editor_property("override_materials", [material] if material else [])
        capsule.set_capsule_size(spec["capsule_radius"], spec["capsule_half_height"])
        defaults.set_editor_property("runtime_character_name", spec["name"])
        defaults.set_editor_property("equipped_skill_data_assets", [skill])
        defaults.set_editor_property("equipped_skill_ability_classes", [])
        defaults.set_editor_property("default_attack_ability_class", None)
        defaults.set_editor_property("round_montage_overrides", {})
        defaults.set_editor_property("weapon_presentation_skills", [])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
    defaults = unreal.get_default_object(blueprint.generated_class())
    component = defaults.get_editor_property("mesh")
    capsule = defaults.get_editor_property("capsule_component")
    require(isinstance(defaults, unreal.EnemyUnit), "Monster must remain an EnemyUnit")
    require(component.get_editor_property("skeletal_mesh_asset") == mesh, "Original mesh reference changed")
    require(component.get_editor_property("anim_class") == animation.generated_class(), "Monster animation class changed")
    require(list(defaults.get_editor_property("equipped_skill_data_assets")) == [skill], "Monster skill connection changed")
    require(str(defaults.get_editor_property("runtime_character_name")) == spec["name"], "Monster name changed")
    require(near(capsule.get_unscaled_capsule_radius(), spec["capsule_radius"]) and near(capsule.get_unscaled_capsule_half_height(), spec["capsule_half_height"]), "Monster capsule changed")
    require(near(component.get_editor_property("relative_scale3d").x, scale) and near(component.get_editor_property("relative_location").z, ground), "Monster scale/ground placement changed")
    require(all(near(axis, scale) for axis in [component.get_editor_property("relative_scale3d").y, component.get_editor_property("relative_scale3d").z]), "Nonuniform monster scale")
    rotation = component.get_editor_property("relative_rotation")
    location = component.get_editor_property("relative_location")
    require(near(rotation.pitch, 0.0) and near(rotation.yaw, -90.0) and near(rotation.roll, 0.0) and near(location.x, 0.0) and near(location.y, 0.0), "Monster mesh orientation changed: " + rotation.export_text() + " " + location.export_text())
    require(component.get_editor_property("animation_mode") == unreal.AnimationMode.ANIMATION_BLUEPRINT and not component.get_editor_property("can_ever_affect_navigation") and component.get_editor_property("enable_update_rate_optimizations"), "Monster component policy changed")
    require(near(defaults.get_editor_property("init_max_hp"), 150.0) and defaults.get_editor_property("max_action_point") == 2, "Existing enemy balance changed")
    require(not defaults.get_editor_property("character_appearance").get_editor_property("appearance_catalog"), "Unexpected player appearance catalog")
    require(unreal.WarriorAssetLibrary.validate_character_physics(mesh), "Invalid source ragdoll")
    require(not material or component.get_material(0) == material, "Original material override changed")
    REPORT["units"].append({"id": spec["id"], "name": spec["name"], "blueprint": blueprint.get_path_name(), "mesh": mesh.get_path_name(), "skeleton": mesh.get_editor_property("skeleton").get_path_name(), "physics": mesh.get_editor_property("physics_asset").get_path_name(), "animation": animation.get_path_name(), "skill": skill.get_path_name(), "scale": scale, "mesh_ground_offset": ground, "material": material.get_path_name() if material else None, "hp": 150, "ap": 2, "source_copies": 0})
    return blueprint.generated_class()


def configure_encounter(classes):
    encounter = load(ENCOUNTER)
    legacy = load(LEGACY_ENEMY).generated_class()
    catalog = list(classes.values()) + [legacy]
    formation = [classes[key] for key in SPEC["default_formation"]]
    require(str(encounter.get_editor_property("opponent_snapshot_slot")) == "None", "Do not replace Snapshot encounters")
    if not VERIFY:
        existing = list(encounter.get_editor_property("enemy_unit_classes"))
        require(existing == [legacy] * 4 or existing == formation, "User-authored formation differs; inspect before changing")
        encounter.set_editor_property("enemy_catalog_classes", catalog)
        encounter.set_editor_property("enemy_unit_classes", formation)
        save(encounter)
    require(list(encounter.get_editor_property("enemy_catalog_classes")) == catalog, "Monster catalog mismatch")
    require(list(encounter.get_editor_property("enemy_unit_classes")) == formation, "Four-unit formation mismatch")
    REPORT["catalog_count"] = len(catalog)
    REPORT["formation"] = SPEC["default_formation"]


def main():
    DIRECTORY.mkdir(parents=True, exist_ok=True)
    require(HELPER.validate_monster_skill(load(BASE_SKILL)), "Basic source skill is invalid")
    base = load(BASE_SKILL)
    classes = {}
    shared = {}
    for spec in SPEC["units"]:
        unreal.log("MONSTER_AUTHORING " + spec["id"])
        mesh = load(spec["mesh"])
        animation, montage, blend, attack = animations_for(spec, mesh, shared)
        skill = skill_for(spec, montage, base)
        classes[spec["id"]] = unit_for(spec, mesh, animation, skill)
    configure_encounter(classes)
    output = DIRECTORY / ("Reload.json" if VERIFY else "Configuration.json")
    output.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("MONSTER_CONTENT_COMPLETE " + str(output))


if __name__ == "__main__":
    main()
