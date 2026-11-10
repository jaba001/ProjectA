# Gameplay 에셋 도구

에셋 작성·검사 도구의 실행 명령과 옵션만 적는다. UI 생성 옵션·JSON 필드는 [UI_README](../../../Docs/UI_README.md), 콘텐츠 구성 기준은 [PROJECT_PLAN](../../../Docs/PROJECT_PLAN.md)을 따른다.

- UE 5.8 Development Editor / Win64 빌드를 쓴다. 제작 경로는 `/Game/User_JeHoon`이다.
- 도구는 에셋 작성·정적 검사만 한다. PIE·게임·자동화 테스트를 시작하지 않는다(6번·34번 제외).
- 번호는 다른 문서가 참조하므로 바꾸지 않는다.

## 공통 명령

```powershell
$editorExecutable = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectFile = 'C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject'
$projectDirectory = Split-Path $projectFile -Parent
$scriptDirectory = Join-Path $projectDirectory 'Source/ProjectAEditor/Scripts'
```

기본형(이하 표의 `스크립트`와 `옵션`만 바꿔 쓴다):

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/<스크립트>.py") <옵션> -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

- 19~29·31~33번은 끝에 `-NoTraceServer -AssetGatherAll=false`를 붙인다. 31·33번은 `-nowrite`도 붙인다(INI 저장만 막고 에셋 저장은 허용).
- 24번은 `'-EnablePlugins=PythonScriptPlugin,MovieRenderPipeline'`을 쓴다.
- 30번은 `-EnablePlugins` 없이 `-unattended -nop4 -NullRHI -nosound -NoTraceServer`를 쓴다.
- 옵션이 없는 기본 실행이 작성인지 검사인지는 도구마다 다르다. 아래 표의 "기본 실행"을 확인한다.
- 작성 후에는 별도 프로세스에서 검사 옵션으로 다시 읽는다.

Navigation 빌드(맵 하나 또는 폴더):

```powershell
& $editorExecutable $projectFile -run=ResavePackages '-Package=<맵 경로>' -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=ResavePackages ("-PackageFolder=" + (Join-Path (Split-Path $projectFile) 'Content/User_JeHoon/LEVEL/Environment')) -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

`-ini`는 해당 프로세스에서만 비동기 로딩 대기를 끄며 프로젝트 설정을 저장하지 않는다.

## 1~9 Gameplay 기본 구성

| 번호 | 도구 | 기본 실행 | 옵션·결과 |
|---|---|---|---|
| 1 | `AuditGameplayAssets.py` | 읽기 전용 감사 | 결과 `Saved/Automation/GameplayAssetAudit.json` |
| 2 | `GenerateUiScaffold` commandlet | WBP 생성 | `GameplayRootWidget`·`RunMapWidget`·`EncounterResultWidget`·`CombatHUDWidget` 명세 |
| 3 | `ConfigureGameplayAssets.py` | 신규 Gameplay·GameMode·Controller·Party/Encounter DA 생성 | 기존 TestMap이 필요하다. 대상이 하나라도 있으면 중단 |
| 4 | Navigation 빌드 | `-Package=/Game/User_JeHoon/LEVEL/Core/Gameplay` | 저장된 경로 데이터가 없으면 이동 요청이 실패한다 |
| 5 | `ValidateGameplayAssets.py` | 읽기 전용 검사 | 결과 `Saved/Automation/GameplayAssetValidation.json` |
| 6 | 스킬 목록 PIE 테스트 | 아래 명령 | 실행마다 빈 전용 저장 슬롯 지정 |
| 7 | `CreateRangedAttack.py` | 폐기 안내만 출력 | 테스트 원거리 제거 |
| 8 | `ConfigureSweepingStrike.py` | 폐기 안내만 출력 | 휩쓸기 제거 |
| 9 | `ConfigureTestEnemies.py` | Arena의 적 배치 작성 | `-TestEnemiesVerifyOnly`. 에디터를 닫고 실행한다 |

2번:

```powershell
& $editorExecutable $projectFile -run=GenerateUiScaffold '-Spec=Source/ProjectAEditor/UiScaffoldSpecs/GameplayRootWidget.json' -unattended -nop4 -NullRHI
```

6번:

```powershell
$skillTestSlot = 'ProjectA_Automation_SkillLoadout_' + [Guid]::NewGuid().ToString('N')
& $editorExecutable $projectFile -unattended -nop4 -RenderOffscreen -nosound -Windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=0 ("-ProjectASaveSlot=$skillTestSlot") '-ExecCmds=Automation RunTests ProjectA.VerticalSlice.SavedSkillLoadout' '-TestExit=Automation Test Queue Empty' ("-ReportExportPath=$projectDirectory/Saved/Automation/SkillLoadoutPIE")
```

- 종료 코드와 함께 JSON의 테스트 상태·오류, `Test Completed. Result={Success}`를 확인한다. 캡처는 `Saved/Automation/SkillLoadoutScreenshots`.
- 관련 필터: 메뉴 `ProjectA.VerticalSlice.SavedMenuLifecycle`, 항복 `ProjectA.Menu.GameMenuSurrender`(`-ProjectASaveSlot=ProjectA_Automation_Surrender_<고유값>`).
- 별도 프로세스 이어하기: `-T11CheckpointSlot=ProjectA_Automation_Restart_<고유값> -T11WriteCheckpoint`로 `ProjectA.Persistence.ProcessRestart` 실행 → 같은 슬롯을 `-ProjectASaveSlot`으로 준 `-game` 프로세스에서 `ProjectA.Menu.PackagedContinue`. `UnrealEditor-Cmd -game`은 패키징 검증이 아니다.
- `ProjectA.Combat.Round.MeleeAreaPhysicalContacts`·`MeleeTargetAndSides`는 공통 기능 회귀로 유지한다.

## 10~18 캐릭터·외형·애니메이션

| 번호 | 도구 | 기본 실행 | 옵션·결과 |
|---|---|---|---|
| 10 | `ConfigureWarriorContent.py` | 이전 GKnight 전사·Skeleton_Guard 적 작성(현행 외형 카탈로그가 있으면 차단) | `-WarriorVerifyOnly`, `-WarriorRebuildRetargets`(시퀀스 48개 재작성) |
| 11 | `ImportParagonAnimations.py` | Kwang 공격·복귀 FBX 2개 임포트 | `-ParagonVerifyOnly`, `-ParagonImportAll`, `-ParagonAnimationPaths=<상대경로,...>`, `-ParagonImportLimit=<양수>` |
| 12 | `ConfigureShopSkillPresentation.py` | `BP_PlayerUnit`에 Manny 메시·검 부착·대체 몽타주 연결 | 결과 `Saved/Automation/ShopSkillPresentationConfigure.json` |
| 13 | (제거) | 파라곤 외형·사망 애니메이션 도구는 삭제 | `ImportMageStaff.py`는 원본 FBX가 있을 때만 쓰는 수동 임포트 |
| 14 | `ConsolidateCopiedAssets.py` | 읽기 전용 사전검사 | `-CopiedAssetsApply`, `-CopiedAssetsVerifyOnly`(적용 기록 필요) |
| 15 | `ConfigureWitchAssassin.py` · `VerifyWitchAssassin.py` | 이전 마녀·Assassin 구성(현행 구성에서는 차단) | `-WitchRebuildRetargets` |
| 16 | `ConfigureRogAppearance.py` · `VerifyRogAppearance.py` | 이전 Manny 의상 구성(현행 몸체 선택에서는 차단) | `-RogAppearanceMaterialsOnly` |
| 17 | `ConfigurePrimitiveAppearance.py` · `VerifyPrimitiveAppearance.py` | `DA_MannyAppearance.BodyVariants`에 Male·Female 등록, 네 직업 기본 메시 연결 | `-PrimitiveMageDefaultsOnly`, `-PrimitivePreviewFacingOnly` |
| 18 | `ConfigureSkillVfxDirection.py` · `CreateCatalogSkills.py` | 폐기 안내만 출력 | 삭제 이력은 [RetiredSkillContent.json](RetiredSkillContent.json) |

- 10번 작성은 `("-ExecutePythonScript=$scriptDirectory/ConfigureWarriorContent.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -RenderOffscreen -nosplash` 형태로 실행한다. 재실행하면 수동 장착·부착·직업 연결을 다시 설정한다.
- 10번 관련 공통 코드: `WarriorContentPaths.py`(파생 결과 경로), `RetargetContentLibrary.py`(Rig·리타깃·골반 이동 검증).
- 검 수치: `unreal.Rotator(pitch=0, yaw=0, roll=180)`, `BladeBase`/`BladeTip` 소켓, 활성 0.23~0.43초·반경 4cm. 몽타주는 공격 1.2초 + 복귀(첫 0.2초 제외) = 1.933333초, 블렌드 0.08/0.12초.
- 11번: `-ParagonImportAll`과 `-ParagonAnimationPaths`는 함께 쓸 수 없다. 기존 결과는 재사용하고 누락분만 가져온다. 원본 FBX 5,385개는 보존한다. 결과 `Saved/Automation/ParagonAnimationsImport.json`·`ParagonAnimationsReload.json`.
- 14번: `User_JeHoon`에 복사한 Manny·GKnight·Skeleton_Guard 메시·뼈대 6개만 원본으로 통합하고 옛 경로에 Redirector를 남긴다. 결과 `Saved/Automation/CopiedAssetsMigration.json`·`CopiedAssetsReload.json`.
- 17번: 남자 `SKM_Primitive_Charater_01_Body`, 여자 `SKM_Primitive_02_Body`. 재실행해도 Male/Female 외 항목은 보존한다. `BodyId` 누락 저장은 `DefaultBodyId=Male`.
  - `-PrimitiveMageDefaultsOnly`: 마법사 Blueprint 3개의 기본 스태프만 처리. 결과 `MageDefaultStaffConfigure.json`·`MageDefaultStaffReload.json`.
  - `-PrimitivePreviewFacingOnly`: 프리뷰 회전 `0°`, MainMenu 카메라 X `-500`, 앵커 Yaw `90°`. 결과 `PreviewFacingReload.json`.
  - 전체 결과 `Saved/Automation/PrimitiveAppearanceConfigure.json`·`PrimitiveAppearanceReload.json`.

## 19~23 레벨·환경·정리

| 번호 | 도구 | 기본 실행 | 옵션·결과 |
|---|---|---|---|
| 19 | `ConfigureDungeonLevels.py` | 던전 비교 맵 2개 신규 작성 | `-DungeonVerifyOnly`, `-DungeonRebuild`(수동 수정 덮어씀). 결과 `Saved/Automation/Dungeons/` |
| 20 | `ConfigureEnvironmentSurfaces.py` → `ConfigureEnvironmentLevels.py` | 재질 작성 후 환경 맵 12개 작성 | 아래 참조 |
| 21 | `ConfigureMonsterContent.py` | 몬스터 Blueprint·애니메이션·스킬 DA와 `DA_DefaultEncounter` 작성 | `-MonsterVerifyOnly`. 결과 `Saved/Automation/Monsters/` |
| 22 | `CleanupUnusedProjectAssets.py` | 삭제 계획 감사 | `-CleanupUnusedApply`, `-CleanupUnusedVerifyOnly`(함께 쓸 수 없음) |
| 23 | `ConfigureLevelLighting.py` | 14맵의 조명·노출만 수정 | `-LightingCaptureBaseline`(작업 시작 시 1회), `-LightingVerifyOnly`. 결과 `Saved/Automation/Lighting/` |

- 19번 순서: 작성 → 두 맵 Navigation 빌드 → `-DungeonVerifyOnly`. 명세 `DungeonFantasySpec.json`·`DungeonStoneSpec.json`. [구성](../../../Docs/PROJECT_PLAN.md#4-3-지하-던전-비교-레벨)
- 20번 순서: 재질 작성 → 맵 작성 → 맵별 Navigation 빌드 → 두 도구의 검사. 명세 `EnvironmentLevelSpecs.json`. [구성](../../../Docs/PROJECT_PLAN.md#4-4-환경-비교-레벨)

```powershell
$environmentSpecs = Get-Content (Join-Path $scriptDirectory 'EnvironmentLevelSpecs.json') -Raw | ConvertFrom-Json
foreach ($environmentSpec in $environmentSpecs.levels)
{
    & $editorExecutable $projectFile -run=ResavePackages ("-Package=" + $environmentSpec.level) -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
}
```

| 20번 옵션 | 동작 |
|---|---|
| `-EnvironmentSurfacesVerifyOnly` / `-EnvironmentVerifyOnly` | 재질 / 맵 읽기 전용 검사 |
| `-EnvironmentNames=MeadowBloom,PineRidge` | 맵 작성·검사 대상 선택 |
| `-EnvironmentSurfacesOnly` | surface 9개만 처리(MI 44개 보존) |
| `-EnvironmentSurfacesRebuild` / `-EnvironmentRebuild` | 기존 결과 재작성(수동 수정 덮어씀) |
| `-EnvironmentSurfacesReportDir=Saved/Automation/<폴더>` | 재질 도구 보고서 위치 |

- 20번 지면 명세 키: `color_low`·`color_high`(색상 범위), `macro_texture`·`macro_uv_scale`, `detail_weight`(색상 범위 모드), `macro_strength`(원본 RGB 모드), `normal_strength`. 생략하면 이전 그래프를 유지한다. 기본 보고서 `Saved/Automation/Environments/`.
- 21번: 소유 정보가 없는 기존 대상은 변경하지 않고 중단한다. 명세 [MonsterContentSpecs.json](MonsterContentSpecs.json). [구성](../../../Docs/PROJECT_PLAN.md#4-5-몬스터-콘텐츠)
- 22번: 검토한 `Saved/Automation/AssetCleanup/DeletionPlan.json`만 처리한다(`schema_version=1`, `base_commit`, `retained_roots`, 대상별 `package`·`asset_class`·`file`·`bytes`·`sha256`). 계획을 자동 생성하지 않으며 `/Game/User_JeHoon/`의 AnimSequence·몽타주·AnimBlueprint·BlendSpace만 허용한다. 계획과 HEAD가 일치해야 하므로 검증은 커밋 전에 실행한다. [기준](../../../Docs/PROJECT_PLAN.md#4-6-미사용-프로젝트-에셋-정리)
- 23번 순서: 기준 해시 기록 → 조명 적용 → 폴더 Navigation 빌드(폴더에 비교 맵 14개만 있는지 확인) → `-LightingVerifyOnly`. 보호 검사 실패를 피하려고 기준을 덮어쓰지 않는다. [기준](../../../Docs/PROJECT_PLAN.md#4-7-렌더링-설정과-비교-레벨-밝기)

## 24~28 스킬·VFX

| 번호 | 도구 | 기본 실행 | 옵션·결과 |
|---|---|---|---|
| 24 | `ResetSkillContent.py` | 퇴역 명세 감사 | `-SkillResetApply`, `-SkillResetVerifyOnly`, `-SkillResetResume`(중단된 적용 재개). 결과 `Saved/Automation/SkillReset/` |
| 25 | `AuditDrGameVfx.py` | 구입 VFX/SFX 6팩 읽기 전용 감사 | 결과 `Saved/Automation/DrGameSkills/Inventory.json` |
| 26 | `CreateDrGameSkills.py` | 스킬 60개·상점 풀·Run 풀 작성, 기본 파티 연결 | 아래 참조 |
| 27 | `AuditSkillVfxDirections.py` | 스킬 DA의 체인·Niagara 방향 값 감사 | `-ChainSkillsAudit`. 결과 `Saved/Automation/SkillVfxDirection/AssetAudit.json` |
| 28 | `CreateSkillVfxDirectionDerivatives.py` | 기존 파생본 읽기 전용 검사 | `-SkillVfxDirectionAuthor`(가시엄니 파생본 1개 작성) |

- 24번: 적용·검증은 커밋 전, 명세의 기준 HEAD에서 한다. 커밋 후 다시 적용하지 않는다. [기준](../../../Docs/PROJECT_PLAN.md#4-8-기본-공격-외-스킬-정리)
- 26번 명세 [DrGameSkillSpecs.json](DrGameSkillSpecs.json). 재실행으로 수동 수정을 덮어쓰지 않는다. 각 옵션에 `-DrGameSkillsVerifyOnly`를 함께 주면 읽기 전용 검사다. 마이그레이션 옵션끼리는 함께 쓸 수 없다. [기준](../../../Docs/PROJECT_PLAN.md#4-9-구입-vfx와-sfx-도입)

| 26번 옵션 | 동작 | 결과 폴더(`Saved/Automation/`) |
|---|---|---|
| (없음) | 신규 작성 | `DrGameSkills/` |
| `-DrGameSkillsAddClassificationTags` | 선언된 분류 태그만 추가(`link` 5종에 `Skill.Shape.Chain`) | `ChainSkillFilter/` |
| `-DrGameSkillsUpdateVfxDirections` | 끝점 좌표 공간과 Niagara 참조 쌍 전환 | `SkillVfxDirection/` |
| `-DrGameSkillsEnableChain` | `link` 5종에 체인 설정 적용 | `ChainSkills/` |
| `-DrGameSkillsUpdateChainTiming` | 체인 간격 0.15 → 0.4초 | `ChainTiming/` |

- 체인 명세 `profiles.link.chain`의 필수 키: `max_targets`·`jump_distance`·`jump_interval_seconds`·`damage_multiplier_per_jump`.
- 방향 수정 순서: 28번 작성 → 26번 `-DrGameSkillsUpdateVfxDirections` → 별도 프로세스에서 28번 기본 검사·26번 방향 VerifyOnly·27번 감사.

## 29~33 구조 정리와 전투 애니메이션

| 번호 | 도구 | 기본 실행 | 옵션·결과 |
|---|---|---|---|
| 29 | `OrganizeLevelFolders.py` | 사전 감사 | `-LevelFoldersApply`, `-LevelFoldersVerifyOnly`. 기록 `Saved/Automation/LevelFolders` |
| 30 | `CreateRecoverySkill.py` | `DA_HealthPotion` 작성 | `-RecoverySkillVerifyOnly`. 결과 `Saved/Automation/RecoverySkill/` |
| 31 | `ConfigureUnifiedGameplay.py` | Core/Gameplay에 환경 12·던전 2의 장식 합성 | `-UnifiedGameplayVerifyOnly`. 결과 `Saved/Automation/UnifiedGameplay_20261008/` |
| 32 | `ConfigureBowAnimation.py` | 활 몽타주 작성, 정밀 화살·화염 화살비의 `CastMontage` 갱신 | 결과 `Saved/Automation/BowAnimation_20261009/` |
| 33 | `ConfigureCombatAnimations.py` | 읽기 전용 검사 | `-CombatAnimationsAuthor`(작성). 결과 `Saved/Automation/CombatAnimations_20261009/` |

- 29번은 18맵 이동 전용의 제한된 도구다. 완료된 작업에 Audit/Apply를 다시 실행하거나 옛 Baseline을 재생성하지 않는다. 공통 경로는 `ProjectLevelPaths.py`, 명세는 `LevelFolderLayout.json`. [구성](../../../Docs/PROJECT_PLAN.md#4-10-레벨-폴더와-이전-경로-호환)
- 30번: `Item.Consumable.Healing`·Instant Heal·HP 25·AP 1. 기존 목적지는 소유 메타데이터와 프로필이 일치해야 하며 다른 값은 덮어쓰지 않는다.
- 31번 명세 `UnifiedGameplaySpecs.json`. 전용 소유 태그의 연출만 재작성하고 전투 코어·바닥·navigation·GameMode·조명은 보존한다. native `EncounterPrototypeStage` 컴파일 후 실행한다.
- 32번 명세 [BowAnimationSpecs.json](BowAnimationSpecs.json). `WarriorAssetLibrary.ConfigureTimedAttackMontage`를 먼저 컴파일한다. 선딜 명세·스킬 DA·CSV가 준비 시간과 다르면 중단한다.
- 33번 명세 [CombatAnimationSpecs.json](CombatAnimationSpecs.json). 일반 근접 1·참격 4·전방 마법 27·광역/지원 마법 27개의 `CastMontage`만 연결한다. `CombatAnimationGrip.py`가 `grip_reference`가 있는 마법 시퀀스 2개에 오른손 손가락 19개 회전을 적용한다.
- 32·33번은 원자적 rollback을 제공하지 않는다. 작성 전 출력은 결과 폴더의 `BeforeAssets`에 백업한다.

## 34 밸런스 측정 commandlet

실제 Run을 실행하므로 사용자가 직접 실행한다. 프로젝트 루트의 PowerShell에서 실행한다.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\ProjectA.uproject" -run=BalanceSim -nullrhi -nosound -unattended -nop4 -NoTraceServer -nowrite -Players=1 -Difficulty=Medium -Repeats=2 -Seed=31001 -TimeScale=20 -MaxSteps=80
```

| 인자 | 범위 | 기본값 |
|---|---|---|
| `Players` | 1..4 | 1 |
| `Difficulty` | `Low`·`Medium`·`High`·`Random` | `Medium` |
| `Repeats` | 1..100000 (시드는 1씩 증가) | 1 |
| `Seed` | int32 | 1 |
| `TimeScale` | 1..1000 | 20 |
| `MaxSteps` | 1..80 | 80 |
| `MaxSeconds` | > 0 (Run당 실제 시간) | 7200 |

- 시간 배율은 1/60초 월드 tick 사이의 실제 대기만 줄인다. time dilation·스킬 타이밍·판정 간격은 그대로다.
- 정책 v1: 생성 UI와 같은 직업 순서·기본 장비로 새 기본 Run을 만들고 전원을 서버 동료 AI에 배정한다. 인카운터·난이도·보상·상점 구매는 시드로 고른다. 골드·구매 권한은 싱글플레이의 선택 캐릭터 규칙을 따른다.
- 결과: `Saved/BalanceSim/<UTC 타임스탬프_UUID>/runs.csv`·`combats.csv`(UTF-8).
- Run 결과: `Complete`·`Defeat`는 완주·자연 패배, `StepLimit`·`Timeout`·`Error`는 미완료.
- 종료 코드: 0 전체 기록 성공, 1 인자 오류, 2 미완료 포함.
- 열: `progress_steps`는 완료 선택 + 승리 전투 수, `final_gold`는 파티 골드 합, `gold_earned`는 전투와 보상 선택의 골드 순증. Snapshot 난이도는 `N/A`. 중단된 전투는 행을 만들지 않는다.
- 저장은 메모리에만 게시하고 Run 저장 파일의 읽기·쓰기·삭제를 막는다. CSV·콘텐츠는 바꾸지 않는다.

## 35 스킬 공격 분류 태그

`TagSkillAttackCategories.py`(`USkillAttackTagLibrary`)가 `Skills` 폴더의 RoundDefinition에 `Skill.Attack.Melee`·`Skill.Attack.Magic`을 추가한다. 회복·보호막·활·석궁은 제외하고 기존 태그는 지우지 않는다. `-SkillAttackTagDryRun`은 저장하지 않는다.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\ProjectA.uproject" -run=pythonscript -script="$PWD\Source\ProjectAEditor\Scripts\TagSkillAttackCategories.py" -unattended -nop4 -nosplash -NoSound
```

## 36 단검 쌍수 이중타격

`CreateDoubleStrike.py`(기본형 명령):

- `GreystoneManny/Attack/AM_MeleeAttack`을 `AS_DoubleStrike_Right`로 굽고 `MirrorPose`로 `AS_DoubleStrike_Left`(0.5초로 단축)를 만든다. `AM_DoubleStrike`은 왼손 → 오른손 순서다.
- 저장 길이 2.1초를 1.25배속으로 재생한다(1.68초). 타격 시점 0.184초·0.584초.
- `Skills/Weapons/DA_DoubleStrike`에 타격 시점·표시명·쌍수 단검 조건을 작성한다.
- 기본 실행은 기존 결과를 보존한다. `-DoubleStrikeOverwrite`는 덮어쓰기, `-DoubleStrikeVerifyOnly`는 읽기 전용 검사. 결과 `Saved/DoubleStrike`.

## 37 방패 돌진

`CreateShieldCharge.py`(기본형 명령):

- 먼저 `ImportParagonAnimations.py -ParagonAnimationPaths=steelmanny/Ability/Steel_Ability_CowCatcher_Charge,steelmanny/Ability/Steel_Ability_CowCatcher_End`로 Steel 원본을 들여온다.
- `AS_ShieldCharge_Loop`(1.0초, 루트 이동 0cm)와 2구간 `AM_ShieldCharge`(0.5초)를 만든다.
- `Skills/Weapons/DA_ShieldCharge`: `DA_MeleeAttack` 복제, 접근 애니메이션 0.6배속, 이동 속도 2000, 선딜 0.17초, 임시 위력 50.
- 기본 실행은 기존 결과를 보존한다. `-ShieldChargeOverwrite`는 덮어쓰기, `-ShieldChargeVerifyOnly`는 읽기 전용 검사. 결과 `Saved/ShieldCharge`.

## 38 프로젝트 폴더 정리 도구

완료된 이동을 위한 제한된 유지보수 도구다. 부분 실행 뒤에는 기록에 맞는 모드만 재개한다.

| 도구 | 기본 실행 | 옵션 | 기록 |
|---|---|---|---|
| `OrganizeReferencedAssetFolders.py` | 이동 전 감사 | `-AssetFoldersApply`, `-AssetFoldersCleanup`, `-AssetFoldersVerifyOnly` | `Saved/AssetFolderOrganization` |
| `OrganizeValidationWidgets.py` | 참조·바이트 감사와 백업 | `-ValidationWidgetsApply`, `-ValidationWidgetsVerifyOnly` | `Saved/ValidationWidgetOrganization` |

- 앞의 도구는 `AssetFolderLayout.json`의 14개 리소스 폴더를 `Assets` 아래로 옮긴다.
- 뒤의 도구는 `-T12GeneratedAssets` 검사용 메뉴 위젯 3종을 `UI/Validation/T12`로 옮긴다.
