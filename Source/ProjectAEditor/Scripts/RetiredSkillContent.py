import json
from pathlib import Path


# Match reviewed retired identities without excluding future authored skills.
# 향후 작성할 스킬을 제외하지 않고 검토된 폐기 식별자만 대조합니다.
SPEC = json.loads(Path(__file__).with_suffix(".json").read_text(encoding="utf-8"))
REMOVED_SKILL_IDS = {entry["skill_id"] for entry in SPEC["assets"] if entry["asset_class"] == "SkillDefinitionDataAsset"} | {"SweepingStrike", "AOE", "RangedAttack"}
REMOVED_SKILL_PATHS = {entry["package"] for entry in SPEC["assets"] if entry["asset_class"] == "SkillDefinitionDataAsset"}
REMOVED_SKILL_PATHS.update("/Game/User_JeHoon/Blueprint/DataAsset/Skills/" + name for name in ["BPDA_SweepingStrike", "DA_SweepingStrike", "BPDA_AreaAttack", "BPDA_RangedAttack"])
REMOVED_SKILL_PATHS.update("/Game/User_JeHoon/Blueprint/DataAsset/" + name for name in ["DA_SweepingStrike", "BPDA_AreaAttack", "BPDA_RangedAttack"])
REMOVED_SKILL_IDS.update(SPEC.get("historical_skill_ids", []))
REMOVED_SKILL_PATHS.update(path.split(".")[0] for path in SPEC.get("historical_skill_paths", []))


def is_removed_skill(skill):
    return bool(skill and (str(skill.get_editor_property("skill_id")) in REMOVED_SKILL_IDS or skill.get_path_name().split(".")[0] in REMOVED_SKILL_PATHS))
