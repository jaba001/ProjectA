import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from RetiredSkillContent import is_removed_skill

# Remove reviewed retired skills while preserving future authored entries and bindings.
# 검토된 폐기 스킬을 제거하고 향후 작성할 항목과 연결은 보존합니다.
root = "/Game/User_JeHoon/Blueprint/DataAsset"


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
