import hashlib
import json
import math

import unreal


OWNER_KEY = "ProjectA.CombatAnimationGrip"
VERSION = "RightHandRotation.v1"
ALLOWED_SEQUENCES = {
    "/Game/User_JeHoon/ParagonAnimationsRetargetedToManny/GideonManny/Attack/Primary_Attack_A_Medium",
    "/Game/User_JeHoon/ParagonAnimationsRetargetedToManny/MurielManny/ConsecratedGround_Cast",
}
FINGERS = tuple(sorted([finger + "_" + joint + "_r" for finger in ["index", "middle", "ring", "pinky"] for joint in ["metacarpal", "01", "02", "03"]] + ["thumb_" + joint + "_r" for joint in ["01", "02", "03"]]))


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def components(value, names):
    result = [float(getattr(value, name)) for name in names]
    require(all(math.isfinite(number) for number in result), "Nonfinite animation key")
    return result


def quaternion(value):
    result = components(value, "xyzw")
    length = math.sqrt(sum(number * number for number in result))
    require(abs(length - 1.0) < 0.001, "Animation rotation is not normalized")
    result = [number / length for number in result]
    sign = next((number for number in reversed(result) if abs(number) > 0.0000001), 1.0)
    return result if sign >= 0.0 else [-number for number in result]


def snapshot(sequence):
    require(isinstance(sequence, unreal.AnimSequence), "Grip input must be an animation sequence")
    model = require(sequence.get_editor_property("data_model_interface"), "Animation has no data model")
    names = sorted(str(name) for name in model.get_bone_track_names())
    require(len(names) == len(set(names)) and set(FINGERS).issubset(names), "Animation must contain every allowed right finger track exactly once")
    count = model.get_number_of_keys()
    require(count > 0, "Animation has no source keys")
    tracks = {}
    for name in names:
        keys = unreal.WarriorAssetLibrary.get_animation_bone_track_transforms(sequence, name)
        require(len(keys) == count, "Unexpected animation track key count: " + name)
        tracks[name] = [[components(key.translation, "xyz"), quaternion(key.rotation), components(key.scale3d, "xyz")] for key in keys]
    rate = model.get_frame_rate()
    return {"key_count": count, "frame_rate": [rate.numerator, rate.denominator], "seconds": sequence.get_play_length(), "skeleton": sequence.get_editor_property("skeleton").get_path_name(), "tracks": tracks}


def reference_rotations(reference, skeleton):
    require(isinstance(reference, unreal.AnimSequence) and reference.get_editor_property("skeleton").get_path_name() == skeleton, "Grip reference must use the same exact skeleton")
    rotations = {}
    for name in FINGERS:
        keys = unreal.WarriorAssetLibrary.get_animation_bone_track_transforms(reference, name)
        require(keys, "Grip reference has no finger track: " + name)
        rotations[name] = quaternion(keys[0].rotation)
    return rotations


def hash_data(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()


def preserved_hash(state):
    # Store a stable read-back fingerprint for the untouched channels, excluding only the nineteen replaced rotations.
    # 교체한 열아홉 회전만 제외하고 보존 채널의 재로드 확인용 지문을 저장합니다.
    data = {key: value for key, value in state.items() if key != "tracks"}
    data["tracks"] = {name: [[[round(number, 6) for number in vector] for index, vector in enumerate(key) if name not in FINGERS or index != 1] for key in keys] for name, keys in state["tracks"].items()}
    return hash_data(data)


def rotation_distance(a, b):
    return math.degrees(2.0 * math.acos(min(1.0, abs(sum(x * y for x, y in zip(a, b))))))


def check_preserved(before, after):
    require({key: value for key, value in before.items() if key != "tracks"} == {key: value for key, value in after.items() if key != "tracks"} and set(before["tracks"]) == set(after["tracks"]), "Grip authoring changed skeleton, timing or track membership")
    maximum_position = 0.0
    maximum_scale = 0.0
    maximum_rotation = 0.0
    for name, keys in before["tracks"].items():
        require(len(keys) == len(after["tracks"][name]), "Grip authoring changed track key count")
        for original, actual in zip(keys, after["tracks"][name]):
            maximum_position = max(maximum_position, math.dist(original[0], actual[0]))
            maximum_scale = max(maximum_scale, math.dist(original[2], actual[2]))
            if name not in FINGERS:
                maximum_rotation = max(maximum_rotation, rotation_distance(original[1], actual[1]))
    require(maximum_position <= 0.0001 and maximum_scale <= 0.00001 and maximum_rotation <= 0.001, "Grip authoring changed a preserved animation channel")
    return {"max_preserved_position_difference_cm": maximum_position, "max_preserved_scale_difference": maximum_scale, "max_preserved_rotation_difference_degrees": maximum_rotation}


def check_rotations(state, expected):
    maximum = max(rotation_distance(key[1], expected[name]) for name in FINGERS for key in state["tracks"][name])
    require(maximum <= 0.001, "Right finger rotation differs from the fixed grip reference")
    return maximum


def validate(sequence, reference):
    require(sequence.get_path_name().split(".")[0] in ALLOWED_SEQUENCES, "Grip validation is limited to the two project magic sequences")
    state = snapshot(sequence)
    rotations = reference_rotations(reference, state["skeleton"])
    text = require(unreal.EditorAssetLibrary.get_metadata_tag(sequence, OWNER_KEY), "Missing authored grip metadata")
    metadata = json.loads(text)
    require(metadata.get("version") == VERSION and metadata.get("reference") == reference.get_path_name() and metadata.get("fingers") == list(FINGERS) and metadata.get("reference_rotations") == hash_data(rotations), "Authored grip reference or allowed fingers differ")
    require(metadata.get("preserved_channels") == preserved_hash(state), "A preserved grip animation channel changed after authoring")
    return {"status": "passed", "sequence": sequence.get_path_name(), "reference": reference.get_path_name(), "finger_tracks": len(FINGERS), "validated_keys_per_track": state["key_count"], "preserved_channels_sha256": metadata["preserved_channels"], "maximum_finger_rotation_difference_degrees": check_rotations(state, rotations)}


def write_track(controller, name, keys, rotation=None):
    positions = [unreal.Vector(*key[0]) for key in keys]
    rotations = [unreal.Quat(*(rotation if rotation is not None else key[1])) for key in keys]
    scales = [unreal.Vector(*key[2]) for key in keys]
    require(controller.set_bone_track_keys(unreal.Name(name), positions, rotations, scales, False), "Could not write allowed right finger track: " + name)


def configure(sequence, reference):
    require(sequence.get_path_name().split(".")[0] in ALLOWED_SEQUENCES, "Grip authoring is limited to the two project magic sequences")
    if unreal.EditorAssetLibrary.get_metadata_tag(sequence, OWNER_KEY):
        previous = json.loads(unreal.EditorAssetLibrary.get_metadata_tag(sequence, OWNER_KEY))
        previous_reference = require(unreal.load_asset(previous.get("reference", "")), "Missing previous grip reference")
        previous_result = validate(sequence, previous_reference)
        if previous_reference == reference:
            return previous_result
        require(previous_reference.get_path_name() == "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle" and reference.get_path_name() == "/Game/User_JeHoon/ParagonAnimationsRetargetedToManny/GreystoneManny/Attack/Attack_A_Med.Attack_A_Med", "Preserve an independently authored grip reference")
    before = snapshot(sequence)
    rotations = reference_rotations(reference, before["skeleton"])
    controller = require(sequence.get_editor_property("controller"), "Animation has no controller")
    require(all(hasattr(controller, method) for method in ["set_bone_track_keys", "open_bracket", "close_bracket"]), "Animation controller is missing the public key authoring API")
    # Keep the caller responsible for saving the declared project output; originals, wrists and other tracks are never written.
    # 선언된 프로젝트 출력 저장은 호출자가 담당하며 원본·손목·다른 트랙은 쓰지 않습니다.
    sequence.modify()
    controller.open_bracket("Fix right finger grip / 오른손 손가락 쥐기 보정", False)
    try:
        for name in FINGERS:
            write_track(controller, name, before["tracks"][name], rotations[name])
    except Exception:
        for name in FINGERS:
            write_track(controller, name, before["tracks"][name])
        raise
    finally:
        controller.close_bracket(False)
    after = snapshot(sequence)
    changes = check_preserved(before, after)
    check_rotations(after, rotations)
    metadata = {"version": VERSION, "reference": reference.get_path_name(), "fingers": list(FINGERS), "reference_rotations": hash_data(rotations), "preserved_channels": preserved_hash(after)}
    unreal.EditorAssetLibrary.set_metadata_tag(sequence, OWNER_KEY, json.dumps(metadata, sort_keys=True, separators=(",", ":")))
    result = validate(sequence, reference)
    result.update(changes)
    return result
