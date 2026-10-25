import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "Content/ParagonAnimationsRetargetedToManny"
DESTINATION = "/Game/User_JeHoon/Assets/ParagonAnimationsRetargetedToManny"
MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SKELETON = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
ASSETS = unreal.EditorAssetLibrary
SKILL = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_DoubleAttack"
BASE = "/Game/User_JeHoon/Blueprint/DataAsset/Skills/Weapons/DA_MeleeAttack"
MONTAGE = DESTINATION + "/CountessManny/Attack/AM_DoubleAttack"
SEQUENCES = ["CountessManny/Attack/Primary_Attack_A_Normal", "CountessManny/Attack/Primary_Attack_B_Normal"]
OWNER_KEY = "ProjectA.DoubleAttack"
OWNER = "DoubleAttack.v1"
VERIFY = "-DoubleAttackVerifyOnly" in unreal.SystemLibrary.get_command_line()
OVERWRITE = "-DoubleAttackOverwrite" in unreal.SystemLibrary.get_command_line()


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def existing(path, asset_type):
    asset = unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if asset:
        require(isinstance(asset, asset_type) and ASSETS.get_metadata_tag(asset, OWNER_KEY) == OWNER, "Preserve independently authored destination: " + path)
    else:
        require(not VERIFY, "Missing baked asset: " + path)
    return asset


def save(asset):
    require(not VERIFY, "Read-only mode cannot save")
    ASSETS.set_metadata_tag(asset, OWNER_KEY, OWNER)
    require(ASSETS.save_loaded_asset(asset, only_if_is_dirty=False), "Save failed: " + asset.get_path_name())


def import_animation(source, destination, skeleton):
    options = unreal.FbxImportUI()
    settings = {"automated_import_should_detect_type": False, "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION, "original_import_type": unreal.FBXImportType.FBXIT_ANIMATION,
                "import_mesh": False, "import_as_skeletal": True, "import_animations": True, "import_materials": False, "import_textures": False, "create_physics_asset": False, "skeleton": skeleton}
    for name, value in settings.items():
        options.set_editor_property(name, value)
    data = options.get_editor_property("anim_sequence_import_data")
    settings = {"animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME, "use_default_sample_rate": False, "custom_sample_rate": 0, "snap_to_closest_frame_boundary": True,
                "import_bone_tracks": True, "import_custom_attribute": True, "add_curve_metadata_to_skeleton": False, "import_translation": unreal.Vector(), "import_rotation": unreal.Rotator(),
                "import_uniform_scale": 1.0, "convert_scene": True, "force_front_x_axis": False, "convert_scene_unit": True, "preserve_local_transform": False}
    for name, value in settings.items():
        data.set_editor_property(name, value)
    task = unreal.AssetImportTask()
    settings = {"filename": str(source), "destination_path": destination.rsplit("/", 1)[0], "destination_name": source.stem, "automated": True, "async_": False,
                "replace_existing": False, "save": False, "factory": unreal.FbxFactory(), "options": options}
    for name, value in settings.items():
        task.set_editor_property(name, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    require(len(paths) == 1 and paths[0].split(".")[0] == destination, "Unexpected import outputs: " + str(paths))
    sequence = require(unreal.load_asset(destination), "Missing selected import")
    sequence.set_preview_skeletal_mesh(require(unreal.load_asset(MESH), "Missing original Manny preview mesh"))
    save(sequence)


def main():
    require(not (VERIFY and OVERWRITE), "Use verification or explicit overwrite separately")
    protected = [ROOT / "Content" / (BASE.removeprefix("/Game/") + ".uasset")]
    protected += [SOURCE / (relative + ".FBX") for relative in SEQUENCES]
    before = {str(path): file_hash(path) for path in protected}
    skeleton = require(unreal.load_asset(SKELETON), "Missing original Manny skeleton")
    require(unreal.load_asset(MESH).get_editor_property("skeleton") == skeleton, "Original Manny mesh and skeleton differ")
    sequences = []
    for relative in SEQUENCES:
        path = DESTINATION + "/" + relative
        if not ASSETS.does_asset_exist(path):
            require(not VERIFY, "Missing selected import: " + path)
            import_animation(SOURCE / (relative + ".FBX"), path, skeleton)
        sequence = require(unreal.load_asset(path), "Selected animation missing")
        require(isinstance(sequence, unreal.AnimSequence) and sequence.get_editor_property("skeleton") == skeleton, "Invalid selected skeleton")
        require(sequence.get_play_length() >= 0.8999, "Selected source is shorter than its authored trim")
        sequences.append(sequence)
    montage = existing(MONTAGE, unreal.AnimMontage)
    create_montage = montage is None
    if create_montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", skeleton)
        factory.set_editor_property("source_animation", sequences[0])
        folder, name = MONTAGE.rsplit("/", 1)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory), "Montage factory failed")
    arguments = [sequences[0], sequences[1], 0.0, 0.4, 0.0, sequences[1].get_play_length(), 0.4]
    if not VERIFY and (create_montage or OVERWRITE):
        require(unreal.WarriorAssetLibrary.configure_timed_attack_montage(montage, *arguments), "Montage authoring failed")
        save(montage)
    require(unreal.WarriorAssetLibrary.validate_timed_attack_montage(montage, *arguments), "Saved montage differs from its two authored gestures")
    skill = existing(SKILL, unreal.SkillDefinitionDataAsset)
    create_skill = skill is None
    if create_skill:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.SkillDefinitionDataAsset)
        folder, name = SKILL.rsplit("/", 1)
        skill = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.SkillDefinitionDataAsset, factory), "Skill factory failed")
    base = require(unreal.load_asset(BASE), "Missing basic attack")
    if not VERIFY and (create_skill or OVERWRITE):
        require(unreal.EquipmentSkillAssetLibrary.configure_double_attack(skill, base, montage, 0.2, 0.4), "Skill authoring failed")
        save(skill)
    result = unreal.EquipmentSkillAssetLibrary.validate_double_attack(skill, base)
    contract = result if isinstance(result, str) else result[1] if isinstance(result, tuple) and result[0] else None
    require(contract and contract.startswith("Dual daggers required;"), "Double attack static contract failed: " + str(result))
    require(before == {str(path): file_hash(path) for path in protected}, "Protected source files changed")
    report = {"mode": "verify" if VERIFY else "author", "skill": skill.get_path_name(), "montage": montage.get_path_name(), "sequences": [sequence.get_path_name() for sequence in sequences], "static_contract": contract, "source_hashes": before, "runtime_review": "User review pending; no gameplay or automation tests executed."}
    output = ROOT / "Saved/DoubleAttack" / ("Verify.json" if VERIFY else "Author.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[DoubleAttack] " + contract)


if __name__ == "__main__":
    main()
