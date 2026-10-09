import json
import re
import sys
import time
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from WarriorContentPaths import SWORD_RECOVERY_SOURCE, SWORD_SOURCE


PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = (PROJECT / "Content/ParagonAnimationsRetargetedToManny").resolve()
DESTINATION = "/Game/User_JeHoon/ParagonAnimationsRetargetedToManny"
MESH_SOURCE = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MESH_PATH = MESH_SOURCE
SKELETON_PATH = "/Game/Characters/Mannequins/Meshes/SK_Mannequin"
LIBRARY = unreal.EditorAssetLibrary
DEFAULT_ANIMATION_PATHS = [path.removeprefix(DESTINATION + "/") for path in [SWORD_SOURCE, SWORD_RECOVERY_SOURCE]]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def option_value(command_line, name):
    matches = list(re.finditer(r"(?:^|\s)-" + re.escape(name) + r"(?==|\s|$)", command_line))
    require(len(matches) <= 1, "Duplicate option: -" + name)
    if not matches:
        return None
    value = re.match(r'=(?:"([^"]*)"|([^\s"]*))(?=\s|$)', command_line[matches[0].end():])
    require(value, "Expected -" + name + "=<value>")
    return value.group(1) if value.group(1) is not None else value.group(2)


def option_flag(command_line, name):
    matches = list(re.finditer(r"(?:^|\s)-" + re.escape(name) + r"(?==|\s|$)", command_line))
    require(len(matches) <= 1, "Duplicate option: -" + name)
    require(not matches or matches[0].end() == len(command_line) or command_line[matches[0].end()].isspace(), "The -" + name + " flag does not take a value")
    return bool(matches)


def select_sources(command_line, source_root, default_paths):
    import_all = option_flag(command_line, "ParagonImportAll")
    requested = option_value(command_line, "ParagonAnimationPaths")
    limit_value = option_value(command_line, "ParagonImportLimit")
    require(not import_all or requested is None, "Use either -ParagonImportAll or -ParagonAnimationPaths")
    source_root = source_root.resolve()
    if import_all:
        sources = sorted(source_root.rglob("*.FBX"))
    else:
        # Import only active sword sources unless an explicit selection is supplied.
        # 명시적 선택이 없으면 현행 검 공격 소스만 임포트합니다.
        names = requested.split(",") if requested is not None else list(default_paths)
        require(names and len(names) == len(set(names)), "Animation selection must contain unique relative paths")
        sources = []
        for name in names:
            require(name and name == name.strip() and "\\" not in name and ":" not in name, "Expected a relative animation path without an extension: " + name)
            require(all(part not in ["", ".", ".."] for part in name.split("/")) and not Path(name).suffix, "Invalid animation selection path: " + name)
            source = (source_root / (name + ".FBX")).resolve()
            require(source.is_relative_to(source_root) and source.is_file(), "Missing selected source or path outside the Paragon folder: " + name)
            require(source not in sources, "Duplicate selected source: " + name)
            sources.append(source)
    require(sources, "Extract the selected Paragon FBX sources under Content first")
    if limit_value is not None:
        require(re.fullmatch(r"[0-9]+", limit_value) and int(limit_value) > 0, "ParagonImportLimit must be a positive integer")
        sources = sources[:int(limit_value)]
    return sources


def save(asset):
    require(asset.get_path_name().startswith(DESTINATION + "/"), "Refusing to save external source " + asset.get_path_name())
    require(LIBRARY.save_loaded_asset(asset, only_if_is_dirty=False), "Could not save " + asset.get_path_name())


def load_manny():
    mesh = require(unreal.load_asset(MESH_PATH), "Missing source Manny mesh")
    skeleton = require(unreal.load_asset(SKELETON_PATH), "Missing source Manny skeleton")
    require(mesh.get_editor_property("skeleton") == skeleton, "Source Manny mesh and skeleton do not match")
    return skeleton


def import_animation(source, destination, skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_ANIMATION)
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property("skeleton", skeleton)
    data = options.get_editor_property("anim_sequence_import_data")
    data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property("use_default_sample_rate", False)
    data.set_editor_property("custom_sample_rate", 0)
    # Align fractional exported end times to the source sampling frame boundary.
    # 소수점으로 내보낸 종료 시간을 소스 샘플링 프레임 경계에 맞춥니다.
    data.set_editor_property("snap_to_closest_frame_boundary", True)
    data.set_editor_property("import_bone_tracks", True)
    data.set_editor_property("import_custom_attribute", True)
    data.set_editor_property("add_curve_metadata_to_skeleton", False)
    data.set_editor_property("import_translation", unreal.Vector(0.0, 0.0, 0.0))
    data.set_editor_property("import_rotation", unreal.Rotator(0.0, 0.0, 0.0))
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("convert_scene_unit", True)
    data.set_editor_property("preserve_local_transform", False)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination.rsplit("/", 1)[0])
    task.set_editor_property("destination_name", source.stem)
    task.set_editor_property("automated", True)
    task.set_editor_property("async_", False)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = task.get_editor_property("imported_object_paths")
    require(len(imported) == 1 and imported[0].split(".")[0] == destination, "Unexpected imported assets: " + str(imported))
    asset = require(unreal.load_asset(destination), "Missing imported animation " + destination)
    # Store the preview on the imported animation without changing the original skeleton or mesh.
    # 원본 스켈레톤이나 메시를 변경하지 않고 임포트한 애니메이션에 프리뷰를 저장합니다.
    asset.set_preview_skeletal_mesh(require(unreal.load_asset(MESH_PATH), "Missing source Manny preview mesh"))
    save(asset)
    return asset


def verify_animation(asset, source, skeleton, preview_mesh):
    require(isinstance(asset, unreal.AnimSequence), "Not an animation: " + source.name)
    require(asset.get_editor_property("skeleton") == skeleton, "Skeleton mismatch: " + source.name)
    preview_tag = LIBRARY.find_asset_data(asset.get_path_name()).get_tag_value("PreviewSkeletalMesh")
    require(preview_tag and preview_mesh.get_path_name() in str(preview_tag), "Preview mesh mismatch: " + source.name)
    require(asset.get_editor_property("sequence_length") > 0.0, "Empty animation: " + source.name)
    tracks = unreal.AnimationLibrary.get_animation_track_names(asset)
    require(tracks, "Missing bone animation tracks: " + source.name)
    filenames = asset.get_editor_property("asset_import_data").extract_filenames()
    require(len(filenames) == 1 and Path(filenames[0]).resolve() == source.resolve(), "Source file mismatch: " + source.name)
    return {"asset": asset.get_path_name(), "seconds": asset.get_editor_property("sequence_length"), "bone_tracks": len(tracks)}


def main():
    command_line = unreal.SystemLibrary.get_command_line()
    verify_only = option_flag(command_line, "ParagonVerifyOnly")
    sources = select_sources(command_line, SOURCE, DEFAULT_ANIMATION_PATHS)
    selected_paths = [source.relative_to(SOURCE).with_suffix("").as_posix() for source in sources]
    skeleton = load_manny()
    preview_mesh = require(unreal.load_asset(MESH_PATH), "Missing Manny preview mesh")
    require(preview_mesh.get_editor_property("skeleton") == skeleton, "Invalid Manny preview mesh")
    results = []
    failures = []
    started = time.time()
    imported = 0
    report_path = PROJECT / ("Saved/Automation/ParagonAnimationsReload.json" if verify_only else "Saved/Automation/ParagonAnimationsImport.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    for index, source in enumerate(sources, 1):
        relative = source.relative_to(SOURCE).with_suffix("")
        destination = DESTINATION + "/" + relative.as_posix()
        try:
            if LIBRARY.does_asset_exist(destination):
                asset = unreal.load_asset(destination)
            else:
                require(not verify_only, "Missing imported animation: " + destination)
                asset = import_animation(source, destination, skeleton)
                imported += 1
            results.append(verify_animation(asset, source, skeleton, preview_mesh))
        except Exception as error:
            failures.append({"source": str(source), "error": str(error)})
            unreal.log_error("PARAGON_IMPORT_FAILURE " + str(source) + " " + str(error))
        if index % 50 == 0 or index == len(sources) or failures:
            report = {"sources": len(sources), "selected_paths": selected_paths, "processed": index, "imported": imported, "verified": len(results), "failures": failures, "verify_only": verify_only, "elapsed_seconds": round(time.time() - started, 2), "assets": results}
            report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
            unreal.log("PARAGON_IMPORT_PROGRESS " + str(index) + "/" + str(len(sources)) + " failures=" + str(len(failures)))
            unreal.SystemLibrary.collect_garbage()
        if failures:
            break
    require(not failures, "Paragon import failures: " + str(len(failures)))
    unreal.log("PARAGON_IMPORT_COMPLETE " + str(len(results)))


if __name__ == "__main__":
    main()
