"""Add Skill.Attack.Melee/Magic tags to skill DataAssets. 스킬 DataAsset에 근접·마법 공격 분류 태그를 추가합니다."""

import sys
import unreal

save = "-SkillAttackTagDryRun" not in sys.argv
result = unreal.SkillAttackTagLibrary.tag_skill_attack_categories(save)
# Python returns the out report alone or with the success flag depending on the binding.
# 바인딩에 따라 출력 보고서만 또는 성공 여부와 함께 반환합니다.
ok, report = result if isinstance(result, tuple) else (True, result)
unreal.log(("SKILL_ATTACK_TAGS_OK " if ok else "SKILL_ATTACK_TAGS_FAILED ") + str(report))
