import math
import uuid

import unreal

# Shared retarget operations receive recipe policy explicitly and never inspect another recipe's globals.
# 공용 리타깃 연산은 제작 정책을 명시적으로 전달받으며 다른 제작 스크립트의 전역 상태를 참조하지 않습니다.
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
HELPER = unreal.WarriorAssetLibrary
RETARGET_SETUP_VERSION = "2"
RETARGET_OP_CLASSES = ["IKRetargetPelvisMotionController", "IKRetargetFKChainsController", "IKRetargetIKChainsController", "IKRetargetRunIKRigController", "IKRetargetRootMotionController", "IKRetargetCurveRemapController"]


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def rig(mesh, directory, name, *, save_asset):
    path = directory + "/" + name
    if ASSETS.does_asset_exist(path):
        return load(path)
    result = require(unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(directory, name), "Could not create IK Rig")
    controller = unreal.IKRigController.get_controller(result)
    require(controller.set_skeletal_mesh(mesh), "IK preview mesh rejected")
    require(controller.apply_auto_generated_retarget_definition(), "No humanoid retarget template")
    require(controller.apply_auto_fbik(), "No humanoid FBIK template")
    save_asset(result)
    return result


def valid_retarget_ops(controller, target_root="root", target_pelvis="pelvis"):
    if controller.get_num_retarget_ops() != len(RETARGET_OP_CLASSES):
        return False
    for index, expected in enumerate(RETARGET_OP_CLASSES):
        op = controller.get_op_controller(index)
        if not op or op.get_class().get_name() != expected or not controller.get_retarget_op_enabled(index):
            return False
        if isinstance(op, unreal.IKRetargetRootMotionController):
            if str(op.get_source_root_bone()) != "root" or str(op.get_target_root_bone()) != target_root or str(op.get_target_pelvis_bone()) != target_pelvis:
                return False
            if op.get_settings().get_editor_property("root_motion_source") != unreal.RootMotionSource.COPY_FROM_SOURCE_ROOT:
                return False
    return True


def retarget_sequences(inputs, anim_blueprint_sources):
    sequences = {}
    for asset in inputs:
        if isinstance(asset, unreal.AnimBlueprint):
            referenced = [load(path) for path in anim_blueprint_sources]
        elif isinstance(asset, unreal.AnimMontage):
            referenced = list(HELPER.get_montage_animations(asset))
        else:
            referenced = [asset]
        for sequence in referenced:
            if isinstance(sequence, unreal.AnimSequence):
                sequences[sequence.get_path_name()] = sequence
    return list(sequences.values())


def retarget(source_mesh, target_mesh, directory, suffix, inputs, *, report, force_rebuild, anim_blueprint_sources, mirror_path, output_path, save_asset, target_root="root", target_pelvis="pelvis"):
    source_rig = rig(source_mesh, directory, "IK_Source" + suffix, save_asset=save_asset)
    target_rig = rig(target_mesh, directory, "IK_Target" + suffix, save_asset=save_asset)
    path = directory + "/RTG" + suffix
    created = not ASSETS.does_asset_exist(path)
    if not created:
        retargeter = load(path)
    else:
        retargeter = require(TOOLS.create_asset("RTG" + suffix, directory, unreal.IKRetargeter, unreal.IKRetargetFactory()), "Could not create retargeter")
    controller = unreal.IKRetargeterController.get_controller(retargeter)
    for side, selected_rig, mesh in [(unreal.RetargetSourceOrTarget.SOURCE, source_rig, source_mesh), (unreal.RetargetSourceOrTarget.TARGET, target_rig, target_mesh)]:
        controller.set_ik_rig(side, selected_rig)
        controller.set_preview_mesh(side, mesh)
    reset_ops = not valid_retarget_ops(controller, target_root, target_pelvis)
    if reset_ops:
        # The factory already creates ops; rebuild once after assigning rigs to avoid uninitialized duplicate pelvis/root ops.
        # 팩토리가 이미 연산을 생성하므로 Rig 지정 후 한 번만 재구성하여 초기화되지 않은 골반/루트 연산 중복을 막습니다.
        controller.remove_all_ops()
        controller.add_default_ops()
        controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        for index in range(controller.get_num_retarget_ops()):
            op = controller.get_op_controller(index)
            if isinstance(op, unreal.IKRetargetRootMotionController):
                op.set_target_root_bone(target_root)
                op.set_target_pelvis_bone(target_pelvis)
    if created:
        controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    require(valid_retarget_ops(controller, target_root, target_pelvis), "Retarget operations are duplicated or have unassigned root bones")
    rebuild = reset_ops or ASSETS.get_metadata_tag(retargeter, "ProjectA.RetargetSetupVersion") != RETARGET_SETUP_VERSION or force_rebuild
    if rebuild:
        groups = {}
        for sequence in retarget_sequences(inputs, anim_blueprint_sources):
            destination = mirror_path(sequence.get_path_name(), suffix)
            if ASSETS.does_asset_exist(destination):
                groups.setdefault(destination.rsplit("/", 1)[0], []).append(sequence)
        for destination, sequences in groups.items():
            require(HELPER.retarget_animations(sequences, source_mesh, target_mesh, retargeter, destination, suffix, True, False), "Could not rebuild retargeted sequences with the corrected op stack")
            for sequence in sequences:
                rebuilt = load(mirror_path(sequence.get_path_name(), suffix))
                save_asset(rebuilt)
                report.setdefault("rebuilt", []).append(rebuilt.get_path_name())
    pending = [asset for asset in inputs if not ASSETS.does_asset_exist(mirror_path(asset.get_path_name(), suffix))]
    if pending:
        batch_folder = directory + "/RetargetBatch_" + uuid.uuid4().hex
        require(HELPER.retarget_animations(pending, source_mesh, target_mesh, retargeter, batch_folder, suffix), "IK retarget failed")
        moves = []
        reused = []
        for path in ASSETS.list_assets(batch_folder, recursive=False, include_folder=False):
            asset = load(path)
            if isinstance(asset, unreal.Class):
                continue
            require(asset.get_path_name().startswith(batch_folder + "/"), "Unexpected retarget output path")
            destination = output_path(asset.get_name(), suffix, inputs)
            if ASSETS.does_asset_exist(destination):
                reused.append((load(destination), asset))
            else:
                moves.append(unreal.AssetRenameData(asset, destination.rsplit("/", 1)[0], destination.rsplit("/", 1)[1]))
        for existing, generated in reused:
            require(ASSETS.consolidate_assets(existing, [generated]), "Could not reuse an existing retargeted dependency")
        require(not moves or TOOLS.rename_assets(moves), "Could not preserve source folders for retarget output")
        for move in moves:
            save_asset(move.asset)
            report.setdefault("retargeted", []).append(move.asset.get_path_name())
    ASSETS.set_metadata_tag(retargeter, "ProjectA.RetargetSetupVersion", RETARGET_SETUP_VERSION)
    save_asset(retargeter)
    return {asset: load(mirror_path(asset.get_path_name(), suffix)) for asset in inputs}


def pelvis_motion_span(sequence, mesh, bone_name="pelvis"):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.COMPRESSED)
    options.set_editor_property("optional_skeletal_mesh", mesh)
    options.set_editor_property("incorporate_root_motion_into_pose", False)
    options.set_editor_property("extract_root_motion", sequence.get_editor_property("enable_root_motion"))
    length = sequence.get_editor_property("sequence_length")
    points = []
    for index in range(math.ceil(length * 30) + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, min(index / 30.0, length), options)
        require(unreal.AnimPoseExtensions.is_valid(pose) and unreal.Name(bone_name) in unreal.AnimPoseExtensions.get_bone_names(pose), "Could not evaluate the pelvis pose: " + sequence.get_path_name())
        location = unreal.AnimPoseExtensions.get_bone_pose(pose, bone_name, unreal.AnimPoseSpaces.WORLD).translation
        require(all(math.isfinite(value) for value in [location.x, location.y, location.z]), "Invalid pelvis transform: " + sequence.get_path_name())
        points.append((location.x, location.y, location.z))
    return max(math.dist(point, points[0]) for point in points)


def verify_retarget_motion(source_mesh, target_mesh, directory, suffix, inputs, *, report, anim_blueprint_sources, mirror_path):
    retargeter = load(directory + "/RTG" + suffix)
    require(valid_retarget_ops(unreal.IKRetargeterController.get_controller(retargeter)), "Invalid retarget op stack: " + retargeter.get_path_name())
    require(ASSETS.get_metadata_tag(retargeter, "ProjectA.RetargetSetupVersion") == RETARGET_SETUP_VERSION, "Retarget sequence rebuild is incomplete")
    for source in retarget_sequences(inputs, anim_blueprint_sources):
        target = load(mirror_path(source.get_path_name(), suffix))
        require(all(target.get_editor_property(flag) == source.get_editor_property(flag) for flag in ["enable_root_motion", "force_root_lock", "root_motion_root_lock"]), "Retargeted root motion policy changed")
        source_span = pelvis_motion_span(source, source_mesh)
        target_span = pelvis_motion_span(target, target_mesh)
        # Allow body proportions and normal pose differences, while rejecting root travel baked into the pelvis.
        # 체형 및 정상 자세 차이는 허용하면서 골반에 베이크된 루트 이동 궤적은 거부합니다.
        require(target_span <= source_span * 2.0 + 20.0, "Excessive retargeted pelvis travel: " + target.get_path_name())
        require(abs(target.get_editor_property("sequence_length") - source.get_editor_property("sequence_length")) < 0.001, "Retargeted animation length changed")
        report.setdefault("retarget_motion", []).append({"asset": target.get_path_name(), "source_pelvis_span_cm": source_span, "target_pelvis_span_cm": target_span})
