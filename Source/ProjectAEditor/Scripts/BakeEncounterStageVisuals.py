import json
from pathlib import Path

import unreal


ASSET = '/Game/User_JeHoon/Blueprint/DataAsset/Encounters/DA_EncounterStageVisuals'
OVERWRITE = '-EncounterVisualsOverwrite' in unreal.SystemLibrary.get_command_line()
assets = unreal.EditorAssetLibrary
catalog = assets.load_asset(ASSET) if assets.does_asset_exist(ASSET) else None
if catalog and not OVERWRITE:
    unreal.log('Preserving the authored encounter catalog; use -EncounterVisualsOverwrite to update the shield grip.')
else:
    if not catalog:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.EncounterStageVisualCatalog)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET.rsplit('/', 1)[1], ASSET.rsplit('/', 1)[0], None, factory)
        if not catalog:
            raise RuntimeError('Cannot create the authored encounter visual catalog.')
    profiles = list(catalog.get_editor_property('profiles'))
    indices = [index for index, profile in enumerate(profiles) if str(profile.profile_id) == 'Warden']
    if len(indices) != 1:
        raise RuntimeError('The authored catalog must contain exactly one Warden profile.')
    index = indices[0]
    profile = profiles[index]
    props = list(profile.props)
    held = [index for index, prop in enumerate(props) if str(prop.attach_bone) == 'hand_l']
    if len(held) != 1:
        raise RuntimeError('The Warden must contain exactly one left-hand attachment.')
    prop = props[held[0]]
    shield = prop.mesh
    idle = profile.idle_animation
    if not shield or not idle:
        raise RuntimeError('The original shield and idle animation must resolve.')
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(idle, 0.0, unreal.AnimPoseEvaluationOptions())
    hand = unreal.AnimPoseExtensions.get_bone_pose(pose, 'hand_l', unreal.AnimPoseSpaces.WORLD)
    arm = unreal.AnimPoseExtensions.get_bone_pose(pose, 'lowerarm_l', unreal.AnimPoseSpaces.WORLD)
    # The mesh's rear grip is left of its center; its other strap follows the forearm toward the elbow.
    # 메시 뒤쪽 손잡이는 중심 왼쪽에 있으며 반대편 끈은 팔꿈치 방향의 전완을 따라갑니다.
    grip = unreal.Vector(-15.5, -6.3, 0.0)
    desired = unreal.MathLibrary.make_rot_from_xy(arm.translation - hand.translation, unreal.Vector(1.0, 0.0, 0.0)).quaternion()
    relative = hand.rotation.inversed() * desired
    bounds = shield.get_bounding_box()
    size = bounds.max - bounds.min
    center = (bounds.min + bounds.max) * 0.5
    scale = min(prop.max_size.x / size.x, prop.max_size.y / size.y, prop.max_size.z / size.z)
    prop.set_editor_property('rotation', relative.rotator())
    prop.set_editor_property('location', relative.rotate_vector((center - grip) * scale))
    props[held[0]] = prop
    profile.set_editor_property('props', props)
    profiles[index] = profile
    catalog.set_editor_property('profiles', profiles)
    if not assets.save_loaded_asset(catalog, only_if_is_dirty=False):
        raise RuntimeError('Cannot save the authored encounter visual catalog.')
    output = Path(unreal.Paths.project_dir()).resolve() / 'Saved/Automation/ShieldNpcFix'
    output.mkdir(parents=True, exist_ok=True)
    report = {'asset': ASSET, 'profiles': len(profiles), 'grip': str(grip), 'rotation': str(prop.rotation), 'location': str(prop.location)}
    (output / 'AuthoredGrip.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('ENCOUNTER_VISUAL_CATALOG_SAVED ' + ASSET)
