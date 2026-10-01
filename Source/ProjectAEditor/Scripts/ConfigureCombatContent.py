import unreal

# Remove only deleted prototype skills from the existing encounter pool; preserve other authored entries and bindings.
# 기존 인카운터 풀에서 삭제된 시험 스킬만 제거하고 다른 작성 항목과 연결은 보존합니다.
root = "/Game/User_JeHoon/Blueprint/DataAsset"
removed_ids = {"SweepingStrike", "AOE", "RangedAttack"}
removed_paths = {root + "/Skills/" + name for name in ["BPDA_SweepingStrike", "DA_SweepingStrike", "BPDA_AreaAttack", "BPDA_RangedAttack"]}
removed_paths.update({root + "/" + name for name in ["DA_SweepingStrike", "BPDA_AreaAttack", "BPDA_RangedAttack"]})


def is_removed_skill(skill):
    return bool(skill and (str(skill.get_editor_property("skill_id")) in removed_ids or skill.get_path_name().split(".")[0] in removed_paths))


pool_path = root + "/SkillPools/DA_EncounterSkillPool"
if unreal.EditorAssetLibrary.does_asset_exist(pool_path):
    pool = unreal.load_asset(pool_path)
    if not pool:
        raise RuntimeError("Could not load " + pool_path)
    entries = list(pool.get_editor_property("entries"))
    retained = [entry for entry in entries if not is_removed_skill(entry.get_editor_property("skill"))]
    if len(retained) != len(entries):
        pool.set_editor_property("entries", retained)
        if not unreal.EditorAssetLibrary.save_loaded_asset(pool):
            raise RuntimeError("Could not save " + pool.get_path_name())
unreal.log("Encounter content cleanup complete; deleted prototype skills are no longer authored")
