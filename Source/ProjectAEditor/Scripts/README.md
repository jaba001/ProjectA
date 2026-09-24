# Gameplay 에셋 도구

UI 구조·생성 옵션·JSON 필드는 [UI_README](../../../Docs/UI_README.md)를 따른다.

Development Editor / Win64 빌드를 사용한다. Python은 `-EnablePlugins=PythonScriptPlugin`으로 해당 프로세스에서만 활성화한다. 제작 경로는 `/Game/User_JeHoon`이며 사용자 요청에 따른 지팡이 직접 임포트는 `/Game/MageStaff_FreeWeapons`를 사용한다. 최초 생성·Audit는 기존 TestMap을 요구하므로 현재 사용자 삭제 상태에서 실행 전 원본 가용성을 확인한다.

```powershell
$editorExecutable = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectFile = 'C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject'
$projectDirectory = Split-Path $projectFile -Parent
$scriptDirectory = Join-Path $projectDirectory 'Source/ProjectAEditor/Scripts'
```

1. `AuditGameplayAssets.py`: AssetRegistry의 WorldMap 역참조, Blueprint 기본값, 실제 로드한 맵의 Actor와 GameMode를 읽고 `Saved/Automation/GameplayAssetAudit.json`에 기록한다. 패키지를 저장하지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/AuditGameplayAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

2. `GenerateUiScaffold`로 `GameplayRootWidget`, `RunMapWidget`, `EncounterResultWidget`, `CombatHUDWidget` JSON spec을 실제 생성한다. 기존 생성 파일을 덮어쓰려면 Designer 변경을 먼저 확인한다.

```powershell
& $editorExecutable $projectFile -run=GenerateUiScaffold '-Spec=Source/ProjectAEditor/UiScaffoldSpecs/GameplayRootWidget.json' -unattended -nop4 -NullRHI
```

3. `ConfigureGameplayAssets.py`: 신규 `Gameplay.umap`, GameMode/Controller Blueprint, Party/Encounter DataAsset을 생성하고 Arena·Grid·Camera·WBP를 연결한다. Gameplay 생성 대상 중 하나라도 이미 존재하면 중단한다. 실행 후에는 생성한 에셋을 에디터에서 직접 편집한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureGameplayAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

4. 복제 맵에 내비게이션 데이터를 빌드하여 저장한다. Bounds/Recast Actor가 존재해도 저장된 경로 데이터가 없으면 이동 요청이 실패한다. `-Package`는 신규 Gameplay 한 개로 제한한다.

```powershell
& $editorExecutable $projectFile -run=ResavePackages '-Package=/Game/User_JeHoon/LEVEL/Gameplay' -BuildNavigationData -ProjectOnly -unattended -nop4 -NullRHI
```

5. 별도 프로세스에서 저장된 연결과 navigation data Actor를 검사한다. 결과는 `Saved/Automation/GameplayAssetValidation.json`이며 실제 경로와 이동은 아래 PIE 테스트에서 검사한다. 에디터 맵 로드 직후의 비동기 navigation 초기화는 tick 없는 Python commandlet의 경로 검사와 구분한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ValidateGameplayAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

6. 저장된 메뉴 맵에서 시작하는 스킬 목록 PIE 통합 테스트를 실행한다. 이 환경의 UE 5.7에서는 `-NullRHI` 상태의 PIE travel이 `GenericWindow::GetRestoredDimensions` fatal을 일으켜, 실제 렌더러의 `-RenderOffscreen`을 사용한다. 실행마다 비어 있는 전용 저장 슬롯을 지정한다.

```powershell
$skillTestSlot = 'ProjectA_Automation_SkillLoadout_' + [Guid]::NewGuid().ToString('N')
& $editorExecutable $projectFile -unattended -nop4 -RenderOffscreen -nosound -Windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=0 ("-ProjectASaveSlot=$skillTestSlot") '-ExecCmds=Automation RunTests ProjectA.VerticalSlice.SavedSkillLoadout' '-TestExit=Automation Test Queue Empty' ("-ReportExportPath=$projectDirectory/Saved/Automation/SkillLoadoutPIE")
```

검증 범위:

- 실제 메뉴·전사 생성·전투 노드에서 시작하며 검 공격을 포함한 저장된 DA 5개의 목록·계획·피해 적용을 확인한다.
- 화면 전환 후 Slate 마우스 누름/해제 한 번을 뷰포트 hit-test·컨트롤러 입력에 전달한다. 메뉴·스킬 버튼은 delegate를 사용하며 물리 마우스 하드웨어 검사는 아니다.
- 전용 시험 저장만 생성·정리하며 기존 저장과 디스크 에셋·밸런스는 변경하지 않는다.

종료 코드와 함께 JSON의 테스트 상태·오류 및 `Test Completed. Result={Success}`를 확인한다. 화면 캡처 경로는 `Saved/Automation/SkillLoadoutScreenshots`다. 메뉴·설정 검사는 별도 `ProjectA.VerticalSlice.SavedMenuLifecycle`을 사용한다. 작동 테스트는 [작업 규칙](../../../AGENTS.md#작동-테스트와-보고서)을 따르며 남은 확인은 [TODO](../../../Docs/TODO.md), 완료 결과는 [HISTORY](../../../Docs/HISTORY.md)에 짧게 기록한다.

추가 위임 실행 명령은 같은 엔진 인자로 `Automation RunTests ProjectA.RunRoundPIE.1Players+ProjectA.RunRoundPIE.2Players+ProjectA.RunRoundPIE.4Players`를 사용한다. 각 시험이 새 전용 저장을 만들고 정리한다. 실제 두 전투·상점·결과 저장 복원·원격 AP/몽타주/SAP를 검사하며 서비스 인증은 시험 계정으로 대체한다.

실제 창 설정은 `-game /Game/User_JeHoon/LEVEL/MainMenu`와 `Automation RunTests ProjectA.Menu.GameWindowOptions`로 검사한다. 항복 UI는 같은 맵에서 새 `-ProjectASaveSlot=ProjectA_Automation_Surrender_<고유값>`과 `ProjectA.Menu.GameMenuSurrender`를 사용한다. 별도 프로세스 이어하기는 새 `-T11CheckpointSlot=ProjectA_Automation_Restart_<고유값> -T11WriteCheckpoint`로 `ProjectA.Persistence.ProcessRestart`를 먼저 실행한 뒤, 해당 슬롯을 `-ProjectASaveSlot`으로 지정한 `-game` 프로세스에서 `ProjectA.Menu.PackagedContinue`를 실행한다. 시험 이름과 달리 `UnrealEditor-Cmd -game` 실행은 패키징 검증이 아니다.

7. `CreateRangedAttack.py`: 기본 공격 DA·GA를 `BPDA_RangedAttack`·`BPGA_RangedAttack`으로 복제하고 기존 단일 대상 투사체 변환을 연결한다. 캐릭터 장착과 기존 에셋은 유지하며 대상이 이미 있으면 덮어쓰지 않는다. `-RangedAttackVerifyOnly`는 제작 직후 저장된 연결·복제 수치만 다시 읽는다. 두 명령 모두 게임을 실행하지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRangedAttack.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRangedAttack.py") -RangedAttackVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

8. `ConfigureSweepingStrike.py`: 기존 휩쓸기만 근접 전방 박스 충돌로 갱신한다. 타일 범위 분기는 끄고 `bUseMeleeAreaCollision=true`·`MeleeAreaHalfExtent=(75,250,100)`을 사용한다. 위력·AP·기존 몽타주는 보존하며 빈 몽타주는 기본공격에서 재사용한다. `-SweepingStrikeVerifyOnly`는 저장 결과만 읽는다. `ConfigureCombatContent.py`의 생성 경로도 같은 설정 함수를 사용한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureSweepingStrike.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureSweepingStrike.py") -SweepingStrikeVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`ProjectA.Combat.Round.MeleeAreaPhysicalContacts`는 타일과 다른 월드 위치·회전·범위 크기·차폐·복수 피격·근접 왕복을 검사한다. 기존 `MeleeTargetAndSides`는 타일형 경로 회귀로 보존한다. 작동 실행 상태는 [TODO](../../../Docs/TODO.md#2-12-휩쓸기-근접-범위-충돌)에 남긴다.

9. `ConfigureTestEnemies.py`: 기본 PvE 인카운터를 기존 적 클래스 4개로 구성하고 Gameplay Arena를 앞열 `(1,2)`, `(2,2)`·뒷열 `(0,3)`, `(3,3)`으로 배치한다. 유닛 능력치·스킬·Snapshot 정의는 변경하지 않는다. 열린 에디터가 패키지를 잠글 수 있으므로 저장 후 종료하고 실행한다. `-TestEnemiesVerifyOnly`는 저장된 클래스 수·배치만 읽는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureTestEnemies.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureTestEnemies.py") -TestEnemiesVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

10. `ConfigureWarriorContent.py`: 이전 GKnight 전사와 현재 Skeleton_Guard 적의 콘텐츠 작성·검사 도구. 현행 네 직업 의상 구성은 16번 도구를 사용한다. 공통 리타깃은 `RetargetContentLibrary.py`에서 제공한다. 메시·뼈대는 원본을 직접 참조하며, 검은 타격 소켓을 추가한 Weapon_Pack 수정본을 유지한다.

IK batch 작성은 Slate 의존성을 Null Renderer로 초기화하는 commandlet에서도 지원한다. 아래 기존 전사 제작 명령은 의상 카탈로그가 활성화되어 있으면 재작성을 차단한다. `-WarriorVerifyOnly`는 공통 외형 검사와 기존 적·직업 매핑을 확인한다. 현행 네 직업의 상세 검사는 `VerifyRogAppearance.py`를 사용한다. 두 도구 모두 PIE·게임 플레이를 시작하지 않는다.

```powershell
& $editorExecutable $projectFile ("-ExecutePythonScript=$scriptDirectory/ConfigureWarriorContent.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -RenderOffscreen -nosplash
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureWarriorContent.py") -WarriorVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`WarriorContentPaths.py`는 필수 파생 결과의 원본 하위 구조를 `/Game/User_JeHoon/` 아래에 유지한다. Manny·GKnight·Skeleton_Guard 메시·뼈대는 원본을 직접 참조하고 검 수정본은 `Weapon_Pack/Mesh/Weapons/Weapons_Kit`에 둔다. 검 시퀀스·몽타주는 `ParagonAnimationsRetargetedToManny/KwangManny/Attack`에 작성하고 기존 `BossyEnemy/Animations/InPlace/Attacks` 결과는 보존한다. Manny 리타깃은 `Characters/Mannequins/Anims/Unarmed`의 Walk/Jog/Jump/Attack·ABP/BS 구조를 유지하며 프로젝트 몽타주는 `Blueprint/Unit/Animation/Montage`에서 유닛별 접미사로 구분한다. 이전 GKnight·기본 적 IK 도구는 `GKnight/Rigs`·`Skeleton_Guard/Rigs`를 사용한다. 과거에 이동한 경로 73개의 참조 호환은 유지하며 메시·뼈대 경로는 원본으로 해석한다.

재실행은 작성 구성을 다시 적용하므로 수동 장착·부착·전사 직업 연결을 재설정한다. 부분 누락 시 고유 임시 폴더에서 리타깃하고 엔진의 에셋 통합으로 기존 의존 참조를 보존한다. `RoundMontageOverrides`와 기존 보행·DefaultSlot을 유지하고 맞지 않는 Manny Foot IK만 제거한다. Retargeter의 기본 연산을 중복 추가하지 않으며 Rig 지정 후 유효한 6개 연산을 한 번 구성한다. 설정 버전 변경이나 `-WarriorRebuildRetargets`는 관련 시퀀스 48개를 기존 경로에 다시 작성하고 원본 Root Motion 설정·참조를 보존한다.

`-WarriorVerifyOnly`는 연산 구성·48개 시퀀스의 길이/포즈/유한 좌표/골반 이동 범위, 원본 폴더 구조·이전 참조 73개, 전사/적 몽타주의 실제 Kwang 공격·복귀 세그먼트를 검사한다. 공격 1.2초에 복귀 0.933333초의 첫 중복 포즈 0.2초를 제외해 총 1.933333초로 연결하며 블렌드 인 0.08초/아웃 0.12초를 사용한다. 검은 축 순서 혼동을 방지하는 `unreal.Rotator(pitch=0, yaw=0, roll=180)`과 손잡이 부착 위치·`BladeBase`/`BladeTip` 소켓·검 전용 `bUseWeaponTrace`로 작성한다. 활성 0.23~0.43초·반경 4cm와 서버 에셋 포즈 기반 칼날 표본 123개를 검사하며 실제 접촉·피해 실행은 [TODO](../../../Docs/TODO.md#2-15-전사와-검-공격-콘텐츠)에 남긴다.

11. `ImportParagonAnimations.py`: `Content/ParagonAnimationsRetargetedToManny`의 FBX를 `/Game/User_JeHoon/ParagonAnimationsRetargetedToManny`에 원본 하위 폴더대로 가져온다. Manny 뼈대·프리뷰 메시는 `/Game/Characters/Mannequins/Meshes`의 원본을 직접 참조하며 복제하거나 저장하지 않는다. 이 스크립트는 AnimSequence만 가져오며 게임 스킬 연결은 변경하지 않는다. FBX만 압축 해제한 상태와 Content Browser에서 열 수 있는 저장 에셋을 구분한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") -ParagonVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-ParagonImportLimit=<개수>`로 처리 범위를 제한할 수 있다. 기존 목적지 에셋은 검증 후 재사용하고 누락된 에셋을 가져온다. 원본 샘플링률을 자동 판정하고 종료 시간을 프레임 경계에 맞춘다. 신규 애니메이션의 개별 프리뷰에는 원본 Manny 메시를 지정한다. 검사는 원본 Manny 메시·뼈대 연결, AnimSequence 형식·길이·본 트랙·원본 FBX 참조를 대상으로 하며 결과는 `Saved/Automation/ParagonAnimationsImport.json`·`ParagonAnimationsReload.json`에 기록한다. 2026-09-21 전체 5,385개 저장·별도 재로드 검사를 통과했다. 파일명의 Additive/MSA만으로 Unreal 전용 가산 설정을 지정하지 않으며 PIE·게임 플레이를 실행하지 않는다. Animation Editor의 실제 재생 확인은 [TODO](../../../Docs/TODO.md#2-16-paragon-fbx-애니메이션-가져오기)를 따른다.

12. `ConfigureShopSkillPresentation.py`: 공용 `BP_PlayerUnit`에 원본 Manny 메시·검 부착·`AM_SwordAttack_Manny` 대체 몽타주를 연결한다. 기존 Blueprint의 4스킬 기본값은 보존하고 새 Run의 비무장 시작·구매 장착은 C++ Run 데이터에서 적용한다. 원본 뼈대 참조 이전이 완료되어 있어야 하며 호환 뼈대 추가나 원본 저장은 하지 않는다. 제작 결과는 `Saved/Automation/ShopSkillPresentationConfigure.json`에 기록한다. 실제 구매·공격 확인은 [TODO](../../../Docs/TODO.md#2-19-비무장-시작과-스킬-상점)를 따른다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureShopSkillPresentation.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

13. 파라곤 캐릭터 외형과 사망 애니메이션 작성 도구는 롤백에 따라 제거했다. 당시 GKnight 전사와 이후 마녀·Assassin 구성은 이전 이력이며 현재 네 직업은 16번 공통 의상 구성을 사용한다. 기본 적 Skeleton_Guard·기존 Blueprint 경로·미사용 파라곤 결과는 보존한다. `ImportMageStaff.py`는 원본 FBX가 있을 때만 사용하는 수동 임포트 도구다.

`RetargetContentLibrary.py`는 Rig·리타깃·골반 이동 검증을 공통 제공한다. 호출 도구가 보고서·재작성 여부·출력 경로 함수를 전달하여 다른 도구의 전역 설정을 참조하지 않는다. 기존 전사 콘텐츠의 강제 재작성은 `-WarriorRebuildRetargets`를 사용한다.

14. `ConsolidateCopiedAssets.py`: `User_JeHoon`에 복사한 Manny·GKnight·Skeleton_Guard 메시·뼈대 6개만 원본으로 통합한다. 외부 팩끼리는 비교하지 않는다. 원본 형상·기준 포즈·애니메이션 데이터를 검사하고 GKnight·Skeleton_Guard의 필요한 몽타주 슬롯만 원본에 보존한다. 애니메이션·AnimBP·메시 참조를 갱신하고 옛 경로에는 작은 Redirector를 남긴다. 검 소켓 수정본과 임포트·리타깃 결과는 유지한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -CopiedAssetsApply -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -CopiedAssetsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

기본 실행은 읽기 전용 사전검사다. 적용 결과는 `Saved/Automation/CopiedAssetsMigration.json`, 별도 프로세스 재로드는 `CopiedAssetsReload.json`에 기록한다. VerifyOnly는 적용 기록이 필요하다. 기존 원본 팩의 설치 상태를 유지하며 두 원본 뼈대의 슬롯 설정만 예외적으로 Git에서 추적한다. PIE·게임 플레이를 실행하지 않는다.

15. `ConfigureWitchAssassin.py`: 이전 마법사 Stylized Dark Witch·도적 Assassin Skin1 구성 도구. 현행 의상 카탈로그가 연결된 직업의 재작성을 차단한다. 이전 구성에서는 원본 모델을 직접 참조하고 마녀 임포트의 본 배율 100·분리된 변형 계층을 같은 경로에서 정리해 PhysicsAsset을 재생성했다. 원본 FBX와 `_DarkWitch`·`_Assassin` 리타깃, 뼈대의 `DefaultSlot`·마녀 임포트 3개·스태프 5개를 기존 경로에 보존한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureWitchAssassin.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/VerifyWitchAssassin.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-WitchRebuildRetargets`는 이전 마녀 구성을 사용할 때 FBX 재임포트 후 정규화·시퀀스를 재작성하는 옵션이다. 현재 네 직업 구성에 적용하지 않는다. 검증 도구는 의상 카탈로그 활성 시 현행 공통 외형 검사로 연결하며, 이전 구성 검사는 [당시 이력](../../../Docs/TODO.md#2-29-마녀와-assassin-외형)으로 구분한다. 에셋 작성·정적 포즈 검사만 수행하고 에디터 창·PIE·게임·자동화 테스트를 실행하지 않는다.

16. `ConfigureRogAppearance.py`: 네 직업을 TopDown과 같은 `SKM_Manny_Simple`·`MI_Manny_01_New`·`MI_Manny_02_New`로 연결하고 `/Game/User_JeHoon/ROG_Modular_Armor/DA_MannyAppearance`에 공통 8부위·103개 외형 항목을 작성한다. 신체를 가리는 의상이 없으면 원본 몸체를 표시하며 가릴 때도 ROG 신체 파츠 6개에 원본 Manny 재질·텍스처를 직접 참조한다. `UCharacterAppearanceAssetLibrary`는 `Head`·`Arms`·`Legs`의 한 슬롯에 합쳐진 재질 영역을 원본 경로에서 두 슬롯으로 복원하고, 정점 위치·UV·스킨 가중치·뼈대·물리를 보존한다. 두 슬롯의 기본 재질은 기존 ROG MI를 유지하고 카탈로그만 Manny MI로 덮어쓴다. `Chest`·`Hands`·`Feet`는 메시를 수정하지 않고 기존 슬롯에 해당 Manny MI를 연결한다. ROG 원본 메시·데이터 테이블을 직접 참조하고 다른 뼈대의 망토 4개를 제외한다. 모델·텍스처 복제·추가 리타깃은 하지 않는다.

궁수의 실제 클래스·메뉴는 기존 `BP_PlayerUnit`·`BP_PartyMenuPreview`를 유지한다. 네 직업의 전투·Snapshot·프리뷰와 의상 카탈로그를 연결하고 마법사 스태프를 Manny `hand_l`에 맞춘다. 이전 전사·궁수 Skeleton_Guard Snapshot 클래스는 호환 맵에 보존한다. 의상 UI는 기존 WBP의 C++ 공통 편집창을 사용한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureRogAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureRogAppearance.py") -RogAppearanceMaterialsOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/VerifyRogAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-RogAppearanceMaterialsOnly`는 기존 직업 매핑·Blueprint·스태프 설정을 유지하고 카탈로그와 신체 파츠 재질 영역만 갱신한다. ROG 신체 파츠를 재임포트했다면 이 옵션으로 슬롯 복원을 다시 적용한다. 미사용 `MI_MannyNeutral`은 참조가 없음을 확인한 뒤 엔진 기능으로 제거한다.

재로드 검사는 네 직업의 기본·개별·전체 부위 선택과 잘못된 ID/중복 거부, 원본 참조·Manny 재질 영역·텍스처·본 자세·공통 애니메이션·검 표본·스태프 부착·물리 연결을 확인한다. 결과는 `Saved/Automation/RogAppearanceConfigure.json`, `RogAppearanceReload.json`에 저장한다. 화면/플레이 실행은 하지 않으며 [검증 상태와 사용자 확인](../../../Docs/TODO.md#2-30-rog-의상-커스터마이징)을 따른다. 카탈로그 활성 시 이전 GKnight·마녀·Assassin 구성의 재작성을 차단하고 `-WarriorVerifyOnly`는 현재 공통 외형과 기존 적/이전 참조 검사를 함께 수행한다.
