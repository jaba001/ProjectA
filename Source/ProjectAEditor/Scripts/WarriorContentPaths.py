import unreal

ROOT = "/Game/User_JeHoon"
WARRIOR_SOURCE = "/Game/GKnight/Meshes/SK_GothicKnight_VA"
ENEMY_SOURCE = "/Game/Skeleton_Guard/Mesh_UE4/Full/SKM_Skeleton_Guard_Body"
WEAPON_SOURCE = "/Game/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Sword"
SWING_SOURCE = "/Game/BossyEnemy/Animations/InPlace/Attacks/Boss_Attack_Swing_InP"
UNARMED_SOURCE = "/Game/Characters/Mannequins/Anims/Unarmed"
WARRIOR_RIGS = ROOT + "/GKnight/Rigs"
ENEMY_RIGS = ROOT + "/Skeleton_Guard/Rigs"
SWING_FOLDER = ROOT + "/BossyEnemy/Animations/InPlace/Attacks"
SWORD_FOLDER = ROOT + "/ParagonAnimationsRetargetedToManny/KwangManny/Attack"
SWORD_SOURCE = SWORD_FOLDER + "/PrimaryAttack_A_Slow"
SWORD_RECOVERY_SOURCE = SWORD_FOLDER + "/PrimaryAttack_A_Slow_Recovery"
SWORD_SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
SWORD_SUFFIX = "_KwangSword"
WARRIOR_MONTAGE = SWORD_FOLDER + "/AM_SwordAttack"


def mirrored_path(source, suffix=""):
    package = source.split(".")[0]
    if not package.startswith("/Game/"):
        raise RuntimeError("Expected a game content source: " + source)
    return (package if package.startswith(ROOT + "/") else ROOT + package[len("/Game"):]) + suffix


def animation_sources():
    paths = [UNARMED_SOURCE + "/" + name for name in ["ABP_Unarmed", "BS_Idle_Walk_Run", "MM_Idle"]]
    paths += [UNARMED_SOURCE + "/Jump/" + name for name in ["MM_Jump", "MM_Fall_Loop", "MM_Land"]]
    paths += [UNARMED_SOURCE + "/Attack/MM_Attack_" + number for number in ["01", "03"]]
    paths += [ROOT + "/Blueprint/Unit/Animation/Montage/MM_Attack_" + number + "_Montage" for number in ["01", "03"]]
    for gait in ["Walk", "Jog"]:
        paths += [UNARMED_SOURCE + "/" + gait + "/MF_Unarmed_" + gait + "_" + direction for direction in ["Bwd", "Bwd_Left", "Bwd_Right", "Fwd", "Fwd_Left", "Fwd_Right", "Left", "Right"]]
    return {path.rsplit("/", 1)[1]: path for path in paths}


def legacy_moves():
    moves = {}
    for old_folder, source, mesh_name, skeleton_name, rigs, suffixes, animation_suffix in [
        (ROOT + "/Characters/Warrior", WARRIOR_SOURCE, "SK_Warrior", "SKEL_Warrior", WARRIOR_RIGS, ["_Warrior", "_SwordAttack"], "_Warrior"),
        (ROOT + "/Characters/SwordEnemy", ENEMY_SOURCE, "SK_SwordEnemy", "SKEL_SwordEnemy", ENEMY_RIGS, ["_SwordEnemy", "_Sword"], "_SwordEnemy"),
    ]:
        mesh = unreal.load_asset(source)
        if not mesh:
            raise RuntimeError("Missing source mesh: " + source)
        moves[old_folder + "/" + mesh_name] = source
        moves[old_folder + "/" + skeleton_name] = mesh.get_editor_property("skeleton").get_path_name().split(".")[0]
        for suffix in suffixes:
            for prefix in ["IK_Source", "IK_Target", "RTG"]:
                moves[old_folder + "/" + prefix + suffix] = rigs + "/" + prefix + suffix
        for name, source_path in animation_sources().items():
            moves[old_folder + "/" + name + animation_suffix] = mirrored_path(source_path, animation_suffix)
    moves[ROOT + "/Characters/Warrior/Boss_Attack_Swing_InP_SwordAttack"] = mirrored_path(SWING_SOURCE, "_SwordAttack")
    moves[ROOT + "/Characters/Warrior/AM_SwordAttack"] = SWING_FOLDER + "/AM_SwordAttack"
    moves[ROOT + "/Characters/SwordEnemy/Boss_Attack_Swing_InP_SwordAttack_Sword"] = mirrored_path(SWING_SOURCE, "_SwordAttack_Sword")
    moves[ROOT + "/Characters/SwordEnemy/AM_SwordAttack_Sword"] = SWING_FOLDER + "/AM_SwordAttack_Sword"
    moves[ROOT + "/Weapons/SM_Sword"] = mirrored_path(WEAPON_SOURCE)
    return moves


def migrate_legacy_assets():
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    moves = legacy_moves()
    pending = []
    for old, new in moves.items():
        entries = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(old)
        if not entries or all(str(data.asset_class_path.asset_name) == "ObjectRedirector" for data in entries):
            continue
        if assets.does_asset_exist(new):
            raise RuntimeError("Both old and new working copies exist: " + old + " -> " + new)
        pending.append(unreal.AssetRenameData(unreal.load_asset(old), new.rsplit("/", 1)[0], new.rsplit("/", 1)[1]))
    if pending and not tools.rename_assets(pending):
        raise RuntimeError("Could not move working copies with Unreal AssetTools")
    return moves


def retarget_output_path(asset_name, suffix, inputs):
    if not asset_name.endswith(suffix):
        raise RuntimeError("Unexpected retarget output name: " + asset_name)
    original_name = asset_name[:-len(suffix)]
    sources = animation_sources()
    sources[SWING_SOURCE.rsplit("/", 1)[1]] = SWING_SOURCE
    for asset in inputs:
        sources[asset.get_name()] = asset.get_path_name().split(".")[0]
    sources["Boss_Attack_Swing_InP_SwordAttack"] = mirrored_path(SWING_SOURCE, "_SwordAttack")
    for source in [SWORD_SOURCE, SWORD_RECOVERY_SOURCE]:
        sources[source.rsplit("/", 1)[1] + SWORD_SUFFIX] = mirrored_path(source, SWORD_SUFFIX)
    if original_name not in sources:
        raise RuntimeError("Unknown source folder for retarget output: " + asset_name)
    return mirrored_path(sources[original_name], suffix)
