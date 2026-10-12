<!-- author: Claude · owner: Claude · Claude 제안 6 선택 1 · 2026-10-12 -->
# 외부 에셋 복원 목록

이 프로젝트는 외부 팩을 받은 그대로 원본 경로에서 참조합니다. Git은 `Content/User_JeHoon`과, 원본 위에 덮어쓰는 몇 개 파일만 추적합니다. 다른 PC에서 새로 시작할 때 이 문서를 기준으로 복원합니다. 구입처와 버전은 사용자가 채워 주세요(빈 칸).

## 1 복원 순서

1. UE 5.8을 설치하고 아래 2절의 팩을 Fab·Epic 런처에서 **같은 폴더 이름**으로 프로젝트 `Content/`에 추가합니다.
2. `git clone` 또는 `git pull`을 실행합니다. 3절의 덮어쓰기 파일이 팩 원본 위에 복원됩니다.
3. README의 빌드 명령으로 컴파일한 뒤 에디터를 엽니다. 참조 오류가 나면 2절의 "참조" 열이 0이 아닌 팩부터 확인합니다.

Paragon 애니메이션은 따로 받을 필요가 없습니다. 스크립트가 임포트한 결과는 Git이 추적하는 `Content/User_JeHoon/ParagonAnimationsRetargetedToManny`에 있으며, 원본 FBX 폴더(`Content/ParagonAnimationsRetargetedToManny`, 약 10GB)는 다시 임포트할 때만 필요합니다.

## 2 외부 팩

2026-10-12 기준으로 측정했습니다. "참조"는 `User_JeHoon`의 uasset/umap 중 해당 팩 경로를 포함한 파일 수이고, "코드"는 Source·Config·DataCatalogs에서 해당 경로를 언급한 파일 수입니다. 0/0인 팩은 현재 쓰지 않는 것으로 보이며, 정리 여부는 사용자가 판단합니다.

| 폴더 | 크기(MB) | 참조 | 코드 | 구입처·버전 |
| --- | ---: | ---: | ---: | --- |
| Characters | 125 | 50 | 15 | UE 템플릿(Manny) |
| Skeleton_Guard | 318 | 45 | 7 | |
| Orasot_Bundle | 989 | 44 | 2 | |
| GKnight | 196 | 33 | 4 | |
| StylizedCreaturesBundle | 661 | 26 | 2 | |
| InfinityBladeIceLands | 1877 | 22 | 2 | Epic 무료 |
| Fantasy_Pack | 541 | 22 | 3 | |
| ProjectileHitVFX | 313 | 15 | 4 | |
| __GroundAttackVFX | 217 | 15 | 5 | |
| TurnBasedStrategyRPGTemplate | 50 | 15 | 0 | |
| Primitive_Characters_Pack | 2871 | 13 | 6 | |
| __AoeVFX | 275 | 12 | 5 | |
| _LevelUpSpawn | 186 | 11 | 5 | |
| Dungeon_Modular_V1 | 3307 | 5 | 3 | |
| ___LinkChainVFX | 147 | 5 | 4 | |
| SlashHitVFX | 154 | 4 | 6 | |
| Kobo_Nature | 631 | 4 | 1 | |
| Assassin | 1057 | 2 | 5 | |
| BossyEnemy | 32 | 2 | 1 | |
| Fantastic_Dungeon_Pack | 817 | 2 | 3 | |
| stylized_dark_witch_fbx__extracted | 179 | 2 | 1 | 외부 FBX 추출본(3절 파일 추적) |
| Weapon_Pack | 1175 | 1 | 8 | |
| ROG_Modular_Armor | 6190 | 1 | 2 | |
| DemonicUI | 297 | 0 | 2 | 코드에서 직접 참조(공통 UI 테마) |
| MageStaff_FreeWeapons | 1 | 0 | 8 | 사용 파일 전체를 Git이 추적 |
| PurePoly | 2 | 0 | 6 | |
| TopDown | 1 | 0 | 5 | UE 템플릿 |
| CR | 5 | 0 | 3 | |
| Adventures_Pack | 3148 | 0 | 2 | |
| LevelPrototyping | 3 | 0 | 2 | UE 템플릿 |
| Cursor | 0 | 0 | 1 | |
| InfinityBladeWeapons | 215 | 0 | 0 | Epic 무료 |
| CraftResourcesIcons | 56 | 0 | 0 | |
| ParagonAnimationsRetargetedToManny | 10149 | 0 | 0 | 재임포트할 때만 필요 |

## 3 Git이 추적하는 덮어쓰기 파일

팩 원본을 받은 뒤 Git이 아래 파일을 프로젝트 수정본으로 덮어씁니다. 스켈레톤 호환·리타깃을 위해 수정한 파일입니다.

- `Assassin/Mesh/SK_Assassin_Skeleton`
- `GKnight/Meshes/SK_GothicKnight_Skeleton`
- `Primitive_Characters_Pack/Demoscene_UE5/Mesh/SKM_Manny_Skeleton`
- `Skeleton_Guard/Demoscene_UE4/Mesh/UE4_Mannequin_Skeleton`
- `ROG_Modular_Armor/Characters/UE5_Mannequins/Manny/Body_parts/SK_Manny_{Arms,Head,Legs}`
- `stylized_dark_witch_fbx__extracted/Stylized_Dark_Witch{,_Skeleton,_PhysicsAsset}`
- `MageStaff_FreeWeapons/` 중 사용 파일 5개

## 4 다시 측정하기

팩을 추가하거나 정리한 뒤에는 이 표를 다시 만듭니다. 측정 방법은 `User_JeHoon`의 uasset/umap에서 `/Game/<폴더>/` 문자열을 검색해 파일 수를 세는 것입니다. 바이너리 검색이라 소프트 참조나 이름 일부가 겹치는 경우 오차가 있을 수 있습니다.
