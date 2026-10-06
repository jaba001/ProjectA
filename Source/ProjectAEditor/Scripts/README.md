# Gameplay 에셋 도구

UI 구조·생성 옵션·JSON 필드는 [UI_README](../../../Docs/UI_README.md)를 따른다.

2026-10-04 사용자가 위임한 UE 5.8.3 엔진 자동화·실제 렌더링·입력 검수의 최신 범위는 [검증 이력](../../../Docs/HISTORY.md#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록한다. 아래 도구별 작성·재로드 이력과 최신 작동 검수는 구분하며 남은 확인은 [TODO](../../../Docs/TODO.md)를 따른다.

UE 5.8 Development Editor / Win64 빌드를 사용한다. 기존 Gameplay·Run 도구의 실행·재로드 결과는 UE 5.7 이력이며 19~20번 비교 레벨 도구의 작성·검사 결과는 UE 5.8 기준이다. UE 5.8 작동 확인은 [TODO 7절](../../../Docs/TODO.md#7-ue-58-전환-확인)을 따른다. Python은 `-EnablePlugins=PythonScriptPlugin`으로 해당 프로세스에서만 활성화한다. 제작 경로는 `/Game/User_JeHoon`이며 사용자 요청에 따른 지팡이 직접 임포트는 `/Game/MageStaff_FreeWeapons`를 사용한다. 최초 생성 도구 `ConfigureGameplayAssets.py`만 기존 TestMap을 요구하며 입력이 없으면 작성 전에 중단한다. `AuditGameplayAssets.py`는 현재 역할 4맵의 새 경로를 검사하므로 TestMap을 요구하지 않는다.

```powershell
$editorExecutable = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
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
& $editorExecutable $projectFile -run=ResavePackages '-Package=/Game/User_JeHoon/LEVEL/Core/Gameplay' -BuildNavigationData -ProjectOnly -unattended -nop4 -NullRHI
```

5. 별도 프로세스에서 저장된 연결과 navigation data Actor를 검사한다. 결과는 `Saved/Automation/GameplayAssetValidation.json`이며 실제 경로와 이동은 아래 PIE 테스트에서 검사한다. 에디터 맵 로드 직후의 비동기 navigation 초기화는 tick 없는 Python commandlet의 경로 검사와 구분한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ValidateGameplayAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

6. 저장된 메뉴 맵에서 시작하는 스킬 목록 PIE 통합 테스트의 실행 명령이다. 사용자 요청에 따라 에셋 교체 전 추가 스킬 실행은 보류한다. 이 환경의 UE 5.7 실행에서 `-NullRHI` 상태의 PIE travel이 `GenericWindow::GetRestoredDimensions` fatal을 일으킨 이력에 따라 실제 렌더러의 `-RenderOffscreen`을 유지한다. UE 5.8에서 해당 오류의 재현 여부는 확인하지 않았다. 실행마다 비어 있는 전용 저장 슬롯을 지정한다.

```powershell
$skillTestSlot = 'ProjectA_Automation_SkillLoadout_' + [Guid]::NewGuid().ToString('N')
& $editorExecutable $projectFile -unattended -nop4 -RenderOffscreen -nosound -Windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=0 ("-ProjectASaveSlot=$skillTestSlot") '-ExecCmds=Automation RunTests ProjectA.VerticalSlice.SavedSkillLoadout' '-TestExit=Automation Test Queue Empty' ("-ReportExportPath=$projectDirectory/Saved/Automation/SkillLoadoutPIE")
```

시험 성공 시 확인할 범위(UE 5.7 당시 `SavedSkillLoadout` 수정본은 컴파일만 확인하고 재실행하지 않음):

- 실제 메뉴·전사 생성·전투 노드에서 시작하며 저장된 검·비무장 DA 2개의 목록·계획·피해 적용을 확인한다. 삭제 대상인 휩쓸기·테스트 원거리·AOE 3종은 제외한다.
- 화면 전환 후 Slate 마우스 누름/해제 한 번을 뷰포트 hit-test·컨트롤러 입력에 전달한다. 메뉴·스킬 버튼은 delegate를 사용하며 물리 마우스 하드웨어 검사는 아니다.
- 전용 시험 저장만 생성·정리하며 기존 저장과 디스크 에셋·밸런스는 변경하지 않는다.

종료 코드와 함께 JSON의 테스트 상태·오류 및 `Test Completed. Result={Success}`를 확인한다. 화면 캡처 경로는 `Saved/Automation/SkillLoadoutScreenshots`다. 메뉴·설정 검사는 별도 `ProjectA.VerticalSlice.SavedMenuLifecycle`을 사용한다. 작동 테스트는 [작업 규칙](../../../AGENTS.md#작동-테스트와-보고서)을 따르며 현행 시험 구현의 실행 결과와 제외 범위는 [재검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관), 향후 목표 Run·온라인·새 에셋 도입은 [TODO](../../../Docs/TODO.md)를 따른다.

추가 위임 실행 명령은 같은 엔진 인자로 `Automation RunTests ProjectA.RunRoundPIE.1Players+ProjectA.RunRoundPIE.2Players+ProjectA.RunRoundPIE.4Players`를 사용한다. 각 시험이 새 전용 저장을 만들고 정리한다. 현행 fixture는 각 10전투·9상점 진행, 결과·HP/골드 저장 재로드와 원격 AP/몽타주/SAP를 검사한다. 전투 진행을 위해 인간 HP를 높인 fixture여서 정상 난이도 검증은 아니며 서비스 인증은 검사하지 않고 개발용 로컬 계정 문맥을 사용한다.

실제 창 설정은 `-game /Game/User_JeHoon/LEVEL/Core/MainMenu`와 `Automation RunTests ProjectA.Menu.GameWindowOptions`로 검사한다. 항복 UI는 같은 맵에서 새 `-ProjectASaveSlot=ProjectA_Automation_Surrender_<고유값>`과 `ProjectA.Menu.GameMenuSurrender`를 사용한다. 별도 프로세스 이어하기는 새 `-T11CheckpointSlot=ProjectA_Automation_Restart_<고유값> -T11WriteCheckpoint`로 `ProjectA.Persistence.ProcessRestart`를 먼저 실행한 뒤, 해당 슬롯을 `-ProjectASaveSlot`으로 지정한 `-game` 프로세스에서 `ProjectA.Menu.PackagedContinue`를 실행한다. 시험 이름과 달리 `UnrealEditor-Cmd -game` 실행은 패키징 검증이 아니다.

7. `CreateRangedAttack.py`는 폐기 안내 도구로 유지한다. 테스트 원거리 `BPDA_RangedAttack` 제거에 따라 에셋을 작성하거나 재생성하지 않으며 실행 시 폐기 안내만 반환한다. 이전 작성·검증 결과는 당시 이력이다.

8. `ConfigureSweepingStrike.py`는 폐기 안내 도구로 유지한다. 휩쓸기 제거에 따라 에셋을 작성하거나 재생성하지 않으며 실행 시 폐기 안내만 반환한다. `ConfigureCombatContent.py`의 현재 생성 대상에서도 휩쓸기·테스트 원거리·AOE 3종을 제외한다. 신규 후보·기존 저장 보유 제거의 확인은 [구현·검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다.

`ProjectA.Combat.Round.MeleeAreaPhysicalContacts`와 `MeleeTargetAndSides`는 콘텐츠 삭제와 독립적인 근접 범위·타일형 공통 기능 회귀로 보존한다. 두 기본 공격·몬스터 전용 공격 12종을 보존하고 새 DrGame 스킬 60종을 별도 명세로 작성하며 범위·투사체 공통 C++·GAS·FX는 신규 콘텐츠 연결을 위해 유지한다. 기존 작성·검증 결과는 [당시 이력](../../../Docs/HISTORY.md#최근-변경)으로 구분한다.

9. `ConfigureTestEnemies.py`: 기본 PvE 인카운터의 유효한 적 클래스 4개와 순서를 보존하고 Gameplay Arena만 앞열 `(1,2)`, `(2,2)`·뒷열 `(0,3)`, `(3,3)`으로 배치한다. 혼합 편성을 같은 클래스로 덮어쓰지 않으며 유닛 능력치·스킬·Snapshot 정의는 변경하지 않는다. 열린 에디터가 패키지를 잠글 수 있으므로 저장 후 종료하고 실행한다. `-TestEnemiesVerifyOnly`는 저장된 클래스 수·순서·배치만 읽는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureTestEnemies.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureTestEnemies.py") -TestEnemiesVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

10. `ConfigureWarriorContent.py`: 이전 GKnight 전사와 기존 Skeleton_Guard 적의 콘텐츠 작성·검사 도구. 현행 네 직업 의상 구성은 16번 도구를 사용한다. 공통 리타깃은 `RetargetContentLibrary.py`에서 제공한다. 메시·뼈대는 원본을 직접 참조하며, 검은 타격 소켓을 추가한 Weapon_Pack 수정본을 유지한다.

IK batch 작성은 Slate 의존성을 Null Renderer로 초기화하는 commandlet에서도 지원한다. 아래 기존 전사 제작 명령은 의상 카탈로그가 활성화되어 있으면 재작성을 차단한다. `-WarriorVerifyOnly`는 공통 외형 검사와 기존 적·직업 매핑을 확인한다. 현행 네 직업의 상세 검사는 `VerifyRogAppearance.py`를 사용한다. 두 도구 모두 PIE·게임 플레이를 시작하지 않는다.

```powershell
& $editorExecutable $projectFile ("-ExecutePythonScript=$scriptDirectory/ConfigureWarriorContent.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -RenderOffscreen -nosplash
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureWarriorContent.py") -WarriorVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`WarriorContentPaths.py`는 필수 파생 결과의 원본 하위 구조를 `/Game/User_JeHoon/` 아래에 유지한다. Manny·GKnight·Skeleton_Guard 메시·뼈대는 원본을 직접 참조하고 검 수정본은 `Weapon_Pack/Mesh/Weapons/Weapons_Kit`에 둔다. 검 시퀀스·몽타주는 `ParagonAnimationsRetargetedToManny/KwangManny/Attack`에 작성하고 기존 `BossyEnemy/Animations/InPlace/Attacks` 결과는 보존한다. Manny 리타깃은 `Characters/Mannequins/Anims/Unarmed`의 Walk/Jog/Jump/Attack·ABP/BS 구조를 유지하며 프로젝트 몽타주는 `Blueprint/Unit/Animation/Montage`에서 유닛별 접미사로 구분한다. 이전 GKnight·기본 적 IK 도구는 `GKnight/Rigs`·`Skeleton_Guard/Rigs`를 사용한다. 과거에 이동한 경로 73개의 참조 호환은 유지하며 메시·뼈대 경로는 원본으로 해석한다.

재실행은 작성 구성을 다시 적용하므로 수동 장착·부착·전사 직업 연결을 재설정한다. 부분 누락 시 고유 임시 폴더에서 리타깃하고 엔진의 에셋 통합으로 기존 의존 참조를 보존한다. `RoundMontageOverrides`와 기존 보행·DefaultSlot을 유지하고 맞지 않는 Manny Foot IK만 제거한다. Retargeter의 기본 연산을 중복 추가하지 않으며 Rig 지정 후 유효한 6개 연산을 한 번 구성한다. 설정 버전 변경이나 `-WarriorRebuildRetargets`는 관련 시퀀스 48개를 기존 경로에 다시 작성하고 원본 Root Motion 설정·참조를 보존한다.

`-WarriorVerifyOnly`는 연산 구성·48개 시퀀스의 길이/포즈/유한 좌표/골반 이동 범위, 원본 폴더 구조·이전 참조 73개, 전사/적 몽타주의 실제 Kwang 공격·복귀 세그먼트와 전사의 검·비무장 DA 2개 장착 저장본을 검사한다. 공격 1.2초에 복귀 0.933333초의 첫 중복 포즈 0.2초를 제외해 총 1.933333초로 연결하며 블렌드 인 0.08초/아웃 0.12초를 사용한다. 검은 축 순서 혼동을 방지하는 `unreal.Rotator(pitch=0, yaw=0, roll=180)`과 손잡이 부착 위치·`BladeBase`/`BladeTip` 소켓·검 전용 `bUseWeaponTrace`로 작성한다. 활성 0.23~0.43초·반경 4cm와 서버 에셋 포즈 기반 칼날 표본 123개를 검사하며 이 도구의 정적 검사는 실제 접촉 화면을 포함하지 않는다. 서버 칼날 추적의 회귀 범위는 [재검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다.

11. `ImportParagonAnimations.py`: 기본 작성·검사는 `Content/ParagonAnimationsRetargetedToManny/KwangManny/Attack`의 `PrimaryAttack_A_Slow`·`PrimaryAttack_A_Slow_Recovery` FBX 2개만 선택한다. 결과는 `/Game/User_JeHoon/ParagonAnimationsRetargetedToManny` 아래 원본 하위 구조를 유지한다. Manny 뼈대·프리뷰 원본을 직접 참조하며 게임 스킬 연결은 변경하지 않는다. 미사용 결과를 자동 재생성하지 않고 원본 FBX 5,385개는 보존한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") -ParagonVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") -ParagonImportAll -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ImportParagonAnimations.py") '-ParagonAnimationPaths=KwangManny/Attack/PrimaryAttack_A_Slow,KwangManny/Attack/PrimaryAttack_A_Slow_Recovery' -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-ParagonImportAll`은 전체 FBX를 선택하고 `-ParagonAnimationPaths=<상대경로,...>`는 확장자 없는 경로를 명시한다. 두 옵션은 함께 사용할 수 없으며 경로 이탈·누락 파일·중복 선택을 거절한다. `-ParagonImportLimit=<양수>`는 선택 후 처리 수를 제한한다. `-ParagonVerifyOnly`도 같은 선택 기준을 사용하며 기존 결과만 읽는다.

기존 목적지 에셋은 검증 후 재사용하고 누락된 에셋만 가져온다. 원본 샘플링률·프레임 경계와 Manny 뼈대·프리뷰·AnimSequence 길이·본 트랙·원본 FBX를 검사하며 선택 경로와 결과는 `Saved/Automation/ParagonAnimationsImport.json`·`ParagonAnimationsReload.json`에 기록한다. 2026-09-21 전체 5,385개 저장·별도 재로드 성공은 당시 이력이다. Additive/MSA 파일명만으로 가산 설정을 추정하지 않으며 에디터 재생·PIE·게임은 실행하지 않는다. 새 채택 범위는 [에셋 도입 계획](../../../Docs/TODO.md#6-신규-에셋-선정과-도입)을 따른다.

12. `ConfigureShopSkillPresentation.py`: 공용 `BP_PlayerUnit`에 원본 Manny 메시·검 부착·`AM_SwordAttack_Manny` 대체 몽타주를 연결한다. 현재 Blueprint의 기본 장착 목록은 보존하고 새 Run의 비무장 시작·구매 장착은 C++ Run 데이터에서 적용한다. 원본 뼈대 참조 이전이 완료되어 있어야 하며 호환 뼈대 추가나 원본 저장은 하지 않는다. 제작 결과는 `Saved/Automation/ShopSkillPresentationConfigure.json`에 기록한다. 2026-10-01 스킬별 구매·표현 실행은 사용자 지시로 제외했다. 진행·저장 회귀 범위는 [재검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureShopSkillPresentation.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

13. 파라곤 캐릭터 외형과 사망 애니메이션 작성 도구는 롤백에 따라 제거했다. 당시 GKnight 전사와 이후 마녀·Assassin 구성은 이전 이력이며 현재 네 직업은 17번 공통 몸체 구성을 사용한다. 기본 적 Skeleton_Guard·기존 Blueprint 경로·활성 검 공격은 보존하며 미사용 외형 결과는 22번 도구로 정리했다. `ImportMageStaff.py`는 원본 FBX가 있을 때만 사용하는 수동 임포트 도구다.

`RetargetContentLibrary.py`는 Rig·리타깃·골반 이동 검증을 공통 제공한다. 호출 도구가 보고서·재작성 여부·출력 경로 함수를 전달하여 다른 도구의 전역 설정을 참조하지 않는다. 기존 전사 콘텐츠의 강제 재작성은 `-WarriorRebuildRetargets`를 사용한다.

14. `ConsolidateCopiedAssets.py`: `User_JeHoon`에 복사한 Manny·GKnight·Skeleton_Guard 메시·뼈대 6개만 원본으로 통합한다. 외부 팩끼리는 비교하지 않는다. 원본 형상·기준 포즈·애니메이션 데이터를 검사하고 GKnight·Skeleton_Guard의 필요한 몽타주 슬롯만 원본에 보존한다. 애니메이션·AnimBP·메시 참조를 갱신하고 옛 경로에는 작은 Redirector를 남긴다. 검 소켓 수정본과 필수 임포트·리타깃 결과는 유지한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -CopiedAssetsApply -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConsolidateCopiedAssets.py") -CopiedAssetsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

기본 실행은 읽기 전용 사전검사다. 적용 결과는 `Saved/Automation/CopiedAssetsMigration.json`, 별도 프로세스 재로드는 `CopiedAssetsReload.json`에 기록한다. VerifyOnly는 적용 기록이 필요하다. 기존 원본 팩의 설치 상태를 유지하며 두 원본 뼈대의 슬롯 설정만 예외적으로 Git에서 추적한다. PIE·게임 플레이를 실행하지 않는다.

15. `ConfigureWitchAssassin.py`: 이전 마법사 Stylized Dark Witch·도적 Assassin Skin1 구성 도구. 현행 의상 카탈로그가 연결된 직업의 재작성을 차단한다. 당시 마녀 임포트의 본 배율·계층을 정리하고 PhysicsAsset을 재생성했다. 원본 FBX·뼈대 설정·마녀 임포트 3개·스태프 5개와 제작용 Rig는 보존하며 미사용 `_DarkWitch`·`_Assassin` 애니메이션 체인은 22번 도구로 정리했다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureWitchAssassin.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/VerifyWitchAssassin.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-WitchRebuildRetargets`는 이전 마녀 구성을 사용할 때 FBX 재임포트 후 정규화·시퀀스를 재작성하는 옵션이다. 현재 네 직업 구성에 적용하지 않는다. 검증 도구는 의상 카탈로그 활성 시 현행 공통 외형 검사로 연결하며, 이전 구성 검사는 [당시 이력](../../../Docs/HISTORY.md#9-13-마녀와-assassin-외형)으로 구분한다. 에셋 작성·정적 포즈 검사만 수행하고 에디터 창·PIE·게임·자동화 테스트를 실행하지 않는다.

16. `ConfigureRogAppearance.py` (이전 Manny 의상 구성): 네 직업을 TopDown과 같은 `SKM_Manny_Simple`·`MI_Manny_01_New`·`MI_Manny_02_New`로 연결하고 `/Game/User_JeHoon/ROG_Modular_Armor/DA_MannyAppearance`에 공통 8부위·103개 외형 항목을 작성한다. 신체를 가리는 의상이 없으면 원본 몸체를 표시하며 가릴 때도 ROG 신체 파츠 6개에 원본 Manny 재질·텍스처를 직접 참조한다. `UCharacterAppearanceAssetLibrary`는 `Head`·`Arms`·`Legs`의 한 슬롯에 합쳐진 재질 영역을 원본 경로에서 두 슬롯으로 복원하고, 정점 위치·UV·스킨 가중치·뼈대·물리를 보존한다. 두 슬롯의 기본 재질은 기존 ROG MI를 유지하고 카탈로그만 Manny MI로 덮어쓴다. `Chest`·`Hands`·`Feet`는 메시를 수정하지 않고 기존 슬롯에 해당 Manny MI를 연결한다. ROG 원본 메시·데이터 테이블을 직접 참조하고 다른 뼈대의 망토 4개를 제외한다. 모델·텍스처 복제·추가 리타깃은 하지 않는다.

궁수의 실제 클래스·메뉴는 기존 `BP_PlayerUnit`·`BP_PartyMenuPreview`를 유지한다. 네 직업의 전투·Snapshot·프리뷰와 의상 카탈로그를 연결하고 마법사 스태프를 Manny `hand_l`에 맞춘다. 이전 전사·궁수 Skeleton_Guard Snapshot 클래스는 호환 맵에 보존한다. 의상 UI는 기존 WBP의 C++ 공통 편집창을 사용한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureRogAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureRogAppearance.py") -RogAppearanceMaterialsOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/VerifyRogAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

`-RogAppearanceMaterialsOnly`는 이전 Manny 의상 구성에서 기존 직업 매핑·Blueprint·스태프 설정을 유지하고 카탈로그와 신체 파츠 재질 영역만 갱신한다. 현행 몸체 선택이 활성화된 경우 이 작성기는 재실행을 차단한다. `VerifyRogAppearance.py`는 현재 몸체 구성 검사로 연결한다. 미사용 `MI_MannyNeutral`은 이전 작업에서 참조 확인 후 엔진 기능으로 제거했다.

이전 Manny 구성의 재로드 검사는 네 직업의 기본·개별·전체 부위 선택과 잘못된 ID/중복 거부, 원본 참조·Manny 재질 영역·텍스처·본 자세·공통 애니메이션·검 표본·스태프 부착·물리 연결을 확인한다. 결과는 `Saved/Automation/RogAppearanceConfigure.json`, `RogAppearanceReload.json`에 저장한다. 현행 몸체 선택에서 호출하면 17번 Primitive 검사를 수행하고 `PrimitiveAppearanceReload.json`에 저장한다. 화면/플레이 실행은 하지 않으며 [구현·검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다. 카탈로그 활성 시 이전 GKnight·마녀·Assassin 구성의 재작성을 차단하고 `-WarriorVerifyOnly`는 현재 공통 외형과 기존 적/이전 참조 검사를 함께 수행한다.

17. `ConfigurePrimitiveAppearance.py`: 기존 `DA_MannyAppearance`의 `BodyVariants`에 남자 `Male`·여자 `Female`을 등록한다. 남자는 원본 `SKM_Primitive_Charater_01_Body`, 여자는 실제 에셋 이름인 `SKM_Primitive_02_Body`를 직접 참조한다. 원본 공통 뼈대의 Compatible Skeleton·본별 이동 리타기팅·DefaultSlot 설정으로 Manny 애니메이션을 공유하며 메시·텍스처·애니메이션을 복제하지 않는다. 네 직업의 전투·Snapshot·프리뷰 기본 메시를 남자로 연결하고 의상 표시를 비활성화한다. 마법사 기본 `Staff` 메시를 비우고 표시·충돌을 끈다. 103개 의상 항목·스태프 원본·기존 저장 ID는 향후 아이템 작업을 위해 보존한다.

캐릭터 생성/수정의 몸체 이름과 좌우 화살표는 카탈로그 순서를 순환한다. 몸체 추가 시 `BodyVariants`에 고유 `BodyId`·이름·메시·애니메이션·전투/프리뷰 변환을 등록한다. 기존 ID는 저장 호환을 위해 유지하며, 작성기를 다시 실행해도 Male/Female 이외 항목은 보존한다. `BodyId`가 없는 이전 저장은 `DefaultBodyId=Male`을 사용한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigurePrimitiveAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/VerifyPrimitiveAppearance.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```

두 명령에 `-PrimitiveMageDefaultsOnly`를 추가하면 마법사 전투·Snapshot·프리뷰 Blueprint 3개의 기본 스태프만 작성하거나 읽기 전용으로 검사한다. 결과는 `Saved/Automation/MageDefaultStaffConfigure.json`, `MageDefaultStaffReload.json`에 기록하며 스태프 원본 보존도 검사한다.

`-PrimitivePreviewFacingOnly`는 네 직업 메뉴 프리뷰 Blueprint와 현재 Male/Female의 `PreviewMeshTransform` 회전을 `0°`, 저장된 MainMenu 카메라 X를 `-500`, 네 슬롯 앵커 Yaw를 `90°`로 맞춘다. 나머지 몸체 설정·슬롯 위치·상세 거리 배율은 보존한다. 같은 옵션의 읽기 전용 검사는 Blueprint 4개·몸체 변환 2개·앵커 4개·카메라를 재로드하여 `Saved/Automation/PreviewFacingReload.json`에 기록한다. 화면·드래그 실행 검증은 포함하지 않는다.

전체 검사는 네 직업의 기본 메시·몸체 ID와 이전 의상 선택 검증·원본 애니메이션 포즈·마법사 기본 스태프 제거·래그돌 구조를 읽기 전용으로 확인한다. 결과는 `Saved/Automation/PrimitiveAppearanceConfigure.json`, `PrimitiveAppearanceReload.json`에 기록한다. 실제 메뉴 저장·선택·수정·삭제는 확인했으나 프리뷰 idle 루프 경계 4건은 통과하지 못했고 애니메이션 캡처는 제외했다. [재검증 이력](../../../Docs/HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)

18. `ConfigureSkillVfxDirection.py`와 `CreateCatalogSkills.py`: 2026-10-03 폐기된 VFX 스킬 생성의 이전 진입점이다. 실행 시 폐기 안내만 표시하고 에셋·명세를 작성하지 않는다. `CatalogSkillSpecs.json`은 폐기 상태와 [RetiredSkillContent.json](RetiredSkillContent.json)의 삭제 경로 이력만 연결한다. 신규 VFX는 [도입 기준](../../../Docs/TODO.md#6-신규-에셋-선정과-도입)에 따라 별도 명세를 작성한다. [현재 정리 범위](../../../Docs/PROJECT_PLAN.md#4-8-기본-공격-외-스킬-정리)

19. `ConfigureDungeonLevels.py`: `DungeonFantasySpec.json`·`DungeonStoneSpec.json`에 따라 Gameplay를 Unreal 기능으로 복제하여 `/Game/User_JeHoon/LEVEL/Environment/Dungeon/DungeonFantasy`·`DungeonStone`을 작성한다. FANTASTIC의 `/Game/Fantastic_Dungeon_Pack`·Modular Dungeon Collection의 `/Game/Dungeon_Modular_V1`을 직접 참조하고 전장·Grid·GameplayCamera를 보존한다. 두 맵만 기존 `BP_CombatDebugGameMode`를 지정하며 원본 Blueprint·Gameplay·DebugCombat과 기본 Run 전환은 유지한다. 최초 생성·navigation 저장·독립 재로드 정적 검사 결과는 폴더 이동 전 이력이다. 현행 밝기는 23번 도구와 같은 명세를 사용한다. [구성 기준](../../../Docs/PROJECT_PLAN.md#4-3-지하-던전-비교-레벨)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureDungeonLevels.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=ResavePackages '-Package=/Game/User_JeHoon/LEVEL/Environment/Dungeon/DungeonFantasy' -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=ResavePackages '-Package=/Game/User_JeHoon/LEVEL/Environment/Dungeon/DungeonStone' -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureDungeonLevels.py") -DungeonVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 실행은 신규 맵만 생성·저장한다. `-DungeonVerifyOnly`는 저장본의 원본 에셋 참조·전장 연결을 읽기 전용으로 검사하고 `Saved/Automation/Dungeons/Reload.json`에 기록한다. 제작 결과는 같은 폴더의 `Configuration.json`에 기록한다. `-DungeonRebuild`는 기존 두 비교 맵의 장식을 재구성하여 수동 장식 수정을 덮어쓰는 명시적 옵션이다. navigation 명령의 `-ini`는 자동 빌드의 비동기 로딩 대기를 해당 프로세스에서만 해제하며 프로젝트 설정을 저장하지 않는다. 정적 검사는 실제 화면·클릭·이동을 포함하지 않는다. 별도 실행 검수 이력은 [TODO 10절](../../../Docs/TODO.md#10-지하-던전-비교-레벨-확인), 폴더 이동 후 확인은 [TODO 24절](../../../Docs/TODO.md#24-레벨-폴더-정리-후-확인)을 따른다.

20. `ConfigureEnvironmentSurfaces.py`·`ConfigureEnvironmentLevels.py`: `EnvironmentLevelSpecs.json`의 `surfaces`·`instances`를 먼저 작성하고 `levels`의 `level` 경로에 따라 비교 맵을 `/Game/User_JeHoon/LEVEL/Environment/<테마>/`에 생성한다. Orasot·Infinity Blade Ice Lands·Kobo Nature의 원본을 직접 참조하며, 평면 지면·RVT 해제·얼음 팩의 ISM 지원에 필요한 새 Material·자식 MI만 `/Game/User_JeHoon/Materials/Environment/`에 작성한다. 얼음 자식 MI는 UE 5.8 Material usage override를 사용하여 원본 Material의 flag를 보존한다. 기존 전장·물리 바닥·카메라와 독립 전투 모드를 보존하고 반복 장식을 ISM으로 묶는다. UE 5.8에서 12개 맵·표면 Material 9개·자식 MI 44개를 생성·저장하고 navigation 저장·맵과 재질의 최종 독립 재로드 정적 검사를 통과했다. 원본 팩은 Git에 포함하지 않으므로 다른 PC에서도 해당 팩 설치가 필요하다. [맵 목록·구성 기준](../../../Docs/PROJECT_PLAN.md#4-4-환경-비교-레벨)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureEnvironmentSurfaces.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureEnvironmentLevels.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
$environmentSpecs = Get-Content (Join-Path $scriptDirectory 'EnvironmentLevelSpecs.json') -Raw | ConvertFrom-Json
foreach ($environmentSpec in $environmentSpecs.levels)
{
    & $editorExecutable $projectFile -run=ResavePackages ("-Package=" + $environmentSpec.level) -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
}
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureEnvironmentSurfaces.py") -EnvironmentSurfacesVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureEnvironmentLevels.py") -EnvironmentVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 실행은 새 에셋만 작성한다. `-EnvironmentNames=MeadowBloom,PineRidge`로 맵 작성·검사만 선택할 수 있으며 재질 도구는 명세의 재질 전체를 처리한다. 기존 결과를 재작성하려면 각각 `-EnvironmentSurfacesRebuild`·`-EnvironmentRebuild`를 명시하며 작성된 재질 설정·수동 장식 수정을 덮어쓴다. ISM 배치 전 `has_material_usage` 사전검사를 통과해야 하며 원본 기본 재질의 자동 수정에 의존하지 않는다. 원본 팩과 기존 프로젝트 에셋의 해시를 보호하고, 검사는 저장된 재질 그래프·RVT switch·MI usage override·부모/텍스처 참조와 ISM 변환·예산·지면 빈틈·카메라 여백을 확인한다. 보고서는 `Saved/Automation/Environments/{SurfacesConfiguration,SurfacesReload,Configuration,Reload}.json`이며 별도 화면·이동·실제 FPS 검수 범위는 [TODO 11절](../../../Docs/TODO.md#11-환경-비교-레벨-확인), 폴더 이동 후 확인은 [TODO 24절](../../../Docs/TODO.md#24-레벨-폴더-정리-후-확인)을 따른다.

21. `ConfigureMonsterContent.py`: [MonsterContentSpecs.json](MonsterContentSpecs.json)의 10종·설원 재질 변형 2개에 프로젝트 전용 Enemy Blueprint·BlendSpace·native GroundSpeed AnimBlueprint·단일 DefaultSlot 공격 몽타주와 Skill DataAsset을 작성한다. `Fantasy_Pack`·`StylizedCreaturesBundle` 원본을 직접 참조하며 늑대인간·골렘의 Manny 공격 2개만 원본 하위 경로를 유지하여 리타깃한다. 설원 늑대/곰은 기존 종의 애니메이션과 원본 재질을 공유한다. 기본 편성을 오크·트롤·늑대·골렘 4개로 구성하고 기존 `BP_EnemyUnit`을 포함한 소프트 디버그 카탈로그 13개를 연결한다. [구성 기준](../../../Docs/PROJECT_PLAN.md#4-5-몬스터-콘텐츠)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureMonsterContent.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureMonsterContent.py") -MonsterVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 명령은 명세의 프로젝트 전용 에셋과 `DA_DefaultEncounter`를 작성·저장하며 소유 정보가 없는 기존 생성 대상은 변경하지 않고 중단한다. `-MonsterVerifyOnly`는 저장하지 않고 원본 참조·Skeleton/PhysicsAsset·애니메이션 그래프·몽타주·GAS 계약·저장된 CDO와 편성을 검사한다. 기존 기본 비무장 공격의 피해·AP·태그/Query·GAS 효과와 전체 래그돌을 유지하고 원본 패키지를 저장하지 않는다. Development Editor / Win64 컴파일·62개 신규 에셋 작성·독립 재로드 정적 검사 통과, 원본 1,574파일과 재로드 전후 신규/변경 63개 SHA 보존을 확인했다. 결과는 `Saved/Automation/Monsters/Configuration.json`·`Reload.json`·`FinalFileAudit.json`에 기록한다. 게임·PIE·자동화 테스트는 실행하지 않으며 실제 크기·움직임·공격·사망·디버그 추가는 [TODO 12절](../../../Docs/TODO.md#12-몬스터-콘텐츠-확인)에서 사용자가 확인한다.

22. `CleanupUnusedProjectAssets.py`: 참조 그래프·Source/Config·생성 명세·저장 호환을 검토한 `Saved/Automation/AssetCleanup/DeletionPlan.json`만 처리한다. 계획은 `schema_version=1`, 현재 전체 `base_commit`, `retained_roots`와 삭제 대상의 `package`·`asset_class`·`file`·`bytes`·`sha256`을 기록한다. 자동 미사용 탐색·계획 생성 도구가 아니며 `/Game/User_JeHoon/`의 AnimSequence·몽타주·AnimBlueprint·BlendSpace만 허용한다. [정리 기준](../../../Docs/PROJECT_PLAN.md#4-6-미사용-프로젝트-에셋-정리)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CleanupUnusedProjectAssets.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CleanupUnusedProjectAssets.py") -CleanupUnusedApply -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CleanupUnusedProjectAssets.py") -CleanupUnusedVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 명령은 계획·파일 SHA·크기·클래스·모든 참조 분류를 감사한다. `-CleanupUnusedApply`는 참조하는 에셋부터 50개씩 엔진 기능으로 삭제하고 결과를 검사하며 `-CleanupUnusedVerifyOnly`는 삭제 후 읽기 전용으로 검사한다. 적용·검증 옵션은 함께 사용할 수 없다. 모든 모드에서 계획과 HEAD가 일치해야 하므로 별도 프로세스 검증은 커밋 전에 실행한다. 커밋 이후에는 현재 HEAD의 새 계획을 검토하며 계획의 기준만 임의 변경하지 않는다. 경로 이탈·파일 변경·외부 참조·후보 간 순환 참조를 거절하고 잔존 파일/Registry·생존 패키지 의존·보호 루트 폐쇄를 검사한다. 결과는 같은 폴더의 `Audit.json`·`Apply.json`·`Verify.json`에 기록하며 실제 새 Run·Continue·디버그 확인은 [TODO 13절](../../../Docs/TODO.md#13-에셋-정리-후-확인)을 따른다.

23. `ConfigureLevelLighting.py`: `EnvironmentLevelSpecs.json`과 두 던전 명세의 현행 조명·노출을 기존 `/Game/User_JeHoon/LEVEL/Environment/<테마>/` 14맵에 적용한다. 기본 명령은 조명만 부분 수정하며 전장·장식·재질·navigation 설정을 보존한다. 이전 맵 경로 호환은 공통 폴더 명세와 엔진 리디렉션 설정으로 관리한다. 전체 맵의 장식을 재작성하는 `-EnvironmentRebuild`·`-DungeonRebuild` 없이 밝기를 갱신한다. [렌더링·밝기 기준](../../../Docs/PROJECT_PLAN.md#4-7-렌더링-설정과-비교-레벨-밝기)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureLevelLighting.py") -LightingCaptureBaseline -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureLevelLighting.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=ResavePackages ("-PackageFolder=" + (Join-Path (Split-Path $projectFile) 'Content/User_JeHoon/LEVEL/Environment')) -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ConfigureLevelLighting.py") -LightingVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

`-LightingCaptureBaseline`은 원본 검토 후 작업 시작 시 1회 기준 해시만 `Saved/Automation/Lighting/`에 기록하고 종료하는 독립 모드다. 보호 검사 실패를 회피하기 위해 기준을 덮어쓰지 않는다. 조명 저장 후 navigation 캐시 재생성이 필요하므로 폴더에 대상 비교 맵 14개만 있는지 확인하고 navigation 빌드·저장 후 별도 프로세스의 `-LightingVerifyOnly`로 재검사한다. 검사는 완료된 작성 보고서와 저장본의 조명·노출·렌더링 CVar·원본 파일 해시·비조명 설정 보존을 읽기 전용으로 확인하며 결과는 같은 폴더의 `Configuration.json`·`Reload.json`에 기록한다. 14맵 조명·navigation 저장·최종 독립 재로드와 실제 CVar 7개 검사는 통과했으며 보호 대상 20,643파일·재로드 전후 전체 Content 20,657파일 SHA를 보존했다. 설정 반영에는 에디터 재시작·셰이더 재컴파일이 필요하고 화면·게임·PIE·FPS 확인은 [TODO 14절](../../../Docs/TODO.md#14-렌더링과-밝기-확인)을 따른다.

24. `ResetSkillContent.py`: [RetiredSkillContent.json](RetiredSkillContent.json)의 검토된 VFX 스킬·풀·VFX 에셋만 처리한다. 몬스터 전용 공격·원본 공격 애니메이션은 보존하며 UI·환경·무기 공유 리소스는 참조 감사로 보호한다. 기본 감사와 `-SkillResetApply` 적용·`-SkillResetVerifyOnly` 독립 읽기 전용 재로드를 구분한다. 패키지는 엔진 기능으로 삭제하고 대상 17루트의 잔존 파일이 없음을 확인한 뒤 빈 하위 폴더만 제거한다. MoviePipelinePrimaryConfig 로드를 위해 MovieRenderPipeline을 해당 명령에만 활성화한다. 로그·기준·결과는 `Saved/Automation/SkillReset/`에 보관하며 게임·PIE를 시작하지 않는다. [정리 기준](../../../Docs/PROJECT_PLAN.md#4-8-기본-공격-외-스킬-정리)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ResetSkillContent.py") '-EnablePlugins=PythonScriptPlugin,MovieRenderPipeline' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ResetSkillContent.py") -SkillResetApply '-EnablePlugins=PythonScriptPlugin,MovieRenderPipeline' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/ResetSkillContent.py") -SkillResetVerifyOnly '-EnablePlugins=PythonScriptPlugin,MovieRenderPipeline' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

적용·검증은 커밋 전 명세의 기준 HEAD에서 수행한다. 중단된 적용을 재개할 때만 같은 명세·기준 해시와 기존 Apply 보고서를 사용하여 적용 명령에 `-SkillResetResume`를 추가한다. 기준 해시를 바꾸거나 보고서 없이 부분 삭제를 성공으로 처리하지 않는다. 커밋 후 삭제 명세를 다시 적용하지 않는다. 실제 새 Run·Continue·디버그와 몬스터 공격 확인은 [TODO 18절](../../../Docs/TODO.md#18-기본-공격-외-스킬-정리-확인)을 따른다.

25. `AuditDrGameVfx.py`: 구입 VFX/SFX 6팩의 원본 Niagara·오디오 클래스·패키지 의존·사용자 파라미터·AudioPlayer 바인딩/플래그를 읽고 `Saved/Automation/DrGameSkills/Inventory.json`에 기록한다. 원본을 저장하지 않으며 NullRHI의 IsReadyToRun 값을 실제 재생 확인으로 해석하지 않는다. 원본 데모의 InputAction 경고는 별도 기록한다. [필수 원본 팩·루트](../../../Docs/PROJECT_PLAN.md#4-9-구입-vfx와-sfx-도입)

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/AuditDrGameVfx.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

26. `CreateDrGameSkills.py`: [DrGameSkillSpecs.json](DrGameSkillSpecs.json)의 주효과 60개·상점 풀·Run 풀을 `/Game/User_JeHoon/`에 작성하고 기본 파티의 Run 풀을 연결한다. 독립 스킬 미생성 보조 63개·구입 원본·두 기본 공격·몬스터 전용 공격 12개·닫힌 퇴역 명세는 보존한다. 목적지 소유 정보·저장 설정 불일치를 거절하며 재실행으로 수동 수정을 덮어쓰지 않는다. 시험 위력25/AP1/가중치1과 판정 시점·범위는 명세를 따른다. NiagaraFluids·ChaosNiagara는 프로젝트 설정에서 활성화한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsAddClassificationTags -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsAddClassificationTags -DrGameSkillsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsUpdateVfxDirections -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsUpdateVfxDirections -DrGameSkillsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsEnableChain -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsEnableChain -DrGameSkillsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsUpdateChainTiming -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateDrGameSkills.py") -DrGameSkillsUpdateChainTiming -DrGameSkillsVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 명령은 신규 소유 에셋을 작성·저장하고 기존 기본 파티의 Run 풀 참조 변경도 저장한다. `-DrGameSkillsVerifyOnly`는 별도 프로세스에서 저장본·태그·GAS 효과·Niagara 사용자 파라미터·상점 후보·파티 연결과 보호 해시를 읽기 전용으로 검사한다. 작성·재로드 결과는 `Author.json`·`Reload.json`에 기록하고 CSV는 이 결과와 원본 참조를 목록으로 정리한다. 일반 VFX는 원본 Niagara 내부 SFX를 사용한다. PoisonCarousel은 내부 `User.AudioOn=false`와 검증된 동일 원본 Cue의 외부 단일 발동을 사용하며 중복 재생을 추가하지 않는다. 실제 화면·전투·신호의 검수 결과는 [최신 실행 이력](../../../Docs/HISTORY.md#9-17-2026-10-04-ue-58-todo-실행-검수)을 따르고 개별 청감은 [TODO 19절](../../../Docs/TODO.md#19-구입-vfxsfx-스킬-확인)에 남긴다.

`-DrGameSkillsAddClassificationTags`는 기존 소유 DA의 다른 저장 필드가 명세와 모두 일치할 때 선언된 분류 태그만 추가한다. `link` 5종에 기존 `Skill.Shape.Beam`을 유지하여 `Skill.Shape.Chain`을 추가하며 피해·대상·VFX·SFX·타이밍·풀·파티는 변경하지 않는다. `-DrGameSkillsVerifyOnly`를 함께 지정하면 읽기 전용이며, 이 두 명령의 결과는 기존 도입 이력을 덮어쓰지 않고 `Saved/Automation/ChainSkillFilter/Author.json`·`Reload.json`에 기록한다. 이 옵션의 분류 추가는 다중 연쇄 설정을 변경하지 않는다. [체인 분류 사용자 확인](../../../Docs/TODO.md#21-체인-스킬-방식-필터-확인)

`-DrGameSkillsUpdateVfxDirections`는 다른 저장 필드가 명세와 모두 일치할 때 기존 `ParameterType` 끝점 공간과 명세에 선언된 원본/필수 파생본 Niagara 참조 쌍만 전환한다. 체인 5종의 끝점은 `World`, 화염 화살비·우박 폭격 2종의 시작점은 `ComponentLocal`이며 가시엄니 1종은 28번 도구로 작성한 방향 파생본을 참조한다. 원본 Niagara 자료형·에셋·상대 변환·오프셋·기존 판정·태그·GAS·SFX·시점·풀·파티는 보존한다. 다른 마이그레이션 옵션과 동시에 사용할 수 없다. `-DrGameSkillsVerifyOnly`를 함께 지정하면 저장하지 않고 독립 재로드하며 결과는 `Saved/Automation/SkillVfxDirection/Author.json`·`Reload.json`에 기록한다. [방향 사용자 확인](../../../Docs/TODO.md#22-스킬-vfx-방향-확인)

`-DrGameSkillsEnableChain`은 소유 정보가 확인된 `link` 5종에서 기본 `Chain` 값(대상 1·거리 0·간격 0·배율 1)과 기존 단일 연결 설명이 일치할 때 명세의 체인 설정과 생성 설명만 함께 변경한다. 다른 전체 라운드 필드·DA 속성이 일치해야 하며 누락 패키지 생성·파티 연결 변경·다른 마이그레이션 옵션 병행은 거절한다. `profiles.link.chain`의 필수 키는 `max_targets`·`jump_distance`·`jump_interval_seconds`·`damage_multiplier_per_jump`다. 최대 수는 첫 대상을 포함하고 점프 거리 단위는 cm이며 배율은 점프마다 누적 적용한다. 비체인 기본 대상 1은 기존 동작을 유지한다. VerifyOnly는 에셋 수정·저장 없이 비교하며 결과는 `Saved/Automation/ChainSkills/Author.json`·`Reload.json`에 기록한다. UE 5.8.3 Development Editor / Win64 컴파일을 통과했다. 기존 74스킬의 읽기 전용 해석·VFX 참조와 원본 1,900파일 보존 검사를 통과했다. 이후 채택한 초기 시험값과 작성·재로드 결과는 TODO에 구분하며, 현행 0.4초 간격 이식은 다음 옵션을 사용한다. [사용자 확인](../../../Docs/TODO.md#23-다중-체인-공격-확인)은 별도로 수행한다.

`-DrGameSkillsUpdateChainTiming`은 기존 `link` 5종의 4명·600cm·0.15초·피해 배율 0.8과 생성 설명이 일치할 때 간격을 0.4초로 조정하고 설명을 갱신한다. 나머지 저장 필드는 명세와 일치해야 하며 전체 60종 검증·원본/비대상 해시 보존·5개 DA 저장 제한을 적용한다. 다른 마이그레이션 옵션과 병행할 수 없으며 재실행 시 변경하지 않는다. `-DrGameSkillsVerifyOnly`는 독립 재로드용 읽기 전용 검사이며 결과는 `Saved/Automation/ChainTiming/Author.json`·`Reload.json`에 기록한다. 실제 체감 속도 확인은 [TODO 23절](../../../Docs/TODO.md#23-다중-체인-공격-확인)을 따른다.

27. `AuditSkillVfxDirections.py`: 전체 스킬 DA 74개를 기존 해석기로 읽고 해석된 `Chain` 4필드·Niagara 사용자 파라미터·이미터 공간·방향 관련 스크립트 값과 Cascade 방향 모듈·분포를 감사한다. 원본 1,900파일과 스킬 74파일의 전후 해시를 비교하며 저장·효과 활성화·게임 실행을 하지 않는다. 결과는 `Saved/Automation/SkillVfxDirection/AssetAudit.json`에 기록하며 `-ChainSkillsAudit` 지정 시 `Saved/Automation/ChainSkills/AssetAudit.json`을 사용한다. 원본 HLSL·런타임 좌표 변환 정적 검토와 실제 화면 확인은 구분한다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/AuditSkillVfxDirections.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

28. `CreateSkillVfxDirectionDerivatives.py`: 명세의 가시엄니 Niagara 1개만 원본 팩/하위 구조의 프로젝트 전용 경로에 작성한다. 원본 `GPU_Fang.AddVelocity`는 월드 이미터의 Simulation(0) 입력으로 고정 월드 +X를 사용하며 해당 입력은 외부 User 파라미터로 공개되지 않았다. 대상 방향 표현에 필요한 파생본 1개(약 12.1MiB)에서 실제 enum 입력 하나만 Local(2)로 변경하고 이미터 공간·속도 수치·회전·렌더러·SFX·원본 모듈 참조를 보존한다. 편의 복제를 허용하는 도구가 아니다. 실제 입력·소유권·예상 공간·다른 필드·원본/스킬 해시·컴파일/검증이 일치하지 않으면 저장하지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateSkillVfxDirectionDerivatives.py") -SkillVfxDirectionAuthor -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateSkillVfxDirectionDerivatives.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

기본 모드는 기존 파생본의 독립 읽기 전용 검사다. 신규 작성은 명시적 `-SkillVfxDirectionAuthor`만 허용하며 소유 정보가 없는 기존 결과를 덮어쓰지 않는다. 28번 작성 → 26번 방향 참조 갱신 → 별도 프로세스의 28번 기본 검사·26번 방향 VerifyOnly·27번 전체 감사 순서로 확인한다. 결과는 `Saved/Automation/SkillVfxDirection/DerivativeAuthor.json`·`DerivativeReload.json`에 기록하며 화면·게임·PIE·자동화 테스트를 시작하지 않는다.

29. `OrganizeLevelFolders.py`: `LevelFolderLayout.json`을 기준으로 18개 맵을 Unreal 기능으로 이동한다. 기본 실행은 변경 없는 사전 감사, `-LevelFoldersApply`는 이동·참조 갱신·Redirector 정리, `-LevelFoldersVerifyOnly`는 독립 프로세스의 읽기 전용 검증이다. `ProjectLevelPaths.py`는 작성 도구의 기존 맵 이름과 새 용도·테마 경로를 공통으로 관리한다. `EnvironmentLevelSpecs.json` 각 항목의 `level`과 두 던전 명세의 `level`은 이 기준과 일치해야 한다. 기존 맵 이동·Redirector 정리는 Unreal 기능으로 수행하며 탐색기에서 `.umap`만 이동하지 않는다. 원본 팩·고유 맵 18개·필수 파생 자료를 유지하고 이전 Package/Object 경로를 새 맵으로 연결한다. [폴더 기준](../../../Docs/PROJECT_PLAN.md#4-10-레벨-폴더와-이전-경로-호환)·[이동 후 검증](../../../Docs/TODO.md#24-레벨-폴더-정리-후-확인)

이 도구는 이번 18맵 이동만을 위한 제한된 유지보수 명령이며 `Saved/Automation/LevelFolders`의 사전 SHA·바이트 백업(`ProtectedBefore.json`·`Before/`)과 감사 기준(`Baseline.json`)을 사용한다. 완료된 작업에 Audit/Apply를 다시 실행하거나 현재 상태로 옛 Baseline을 재생성하지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/OrganizeLevelFolders.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/OrganizeLevelFolders.py") -LevelFoldersApply -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=ResavePackages ("-PackageFolder=" + (Join-Path (Split-Path $projectFile) 'Content/User_JeHoon/LEVEL/Environment')) -BuildNavigationData -ProjectOnly '-ini:Engine:[/Script/NavigationSystem.NavigationSystemV1]:bWaitForAsyncLoadingBeforeBuildingNavigationAutomatically=False' -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/OrganizeLevelFolders.py") -LevelFoldersVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false
```

적용 전 기본 시작·쿠킹 맵과 로컬 에디터의 최근 맵/맵별 뷰 키를 새 경로로 갱신한다. 로컬 설정은 원본 바이트를 백업하고 카메라 좌표·뷰 설정을 보존한다. CoreRedirect는 맵 이동과 Redirector 정리를 마친 뒤 추가한다. 이동 후 환경 14맵의 navigation을 공식 `ResavePackages -BuildNavigationData`로 재빌드·저장하고 새 프로세스의 `-LevelFoldersVerifyOnly`를 실행한다. 기존 던전 별칭 2개와 이동하는 맵의 옛 경로 18개를 구분한다. 엔진이 이동 중 Redirector를 생성하지 않을 수도 있으므로 삭제 개수를 고정하지 않는다. 최종 검증은 새 World 18개와 옛 물리 경로 20개의 부재, 옛 SoftObjectPath의 정확한 새 맵 해석을 확인한다. 2026-10-04 적용·navigation 재빌드·최종 독립 검증 종료 0과 Development Editor / Win64 38.89초 컴파일을 통과했다. 환경 14맵은 각 64타일과 native Recast payload SHA가 원본과 같고 모든 18맵에서 삭제된 export는 없다. 근거는 `Saved/Automation/LevelFolders/{Audit,Apply,Verify,ProtectedFinal,EditorMapPaths.Final,Payloads.AfterNavigationRepair}.json`과 `Build.Editor.log`·`Navigation.Build.log`이며 이번 작업에서 PIE·게임·자동화 테스트는 실행하지 않았다. 현재 확인 범위는 [TODO 24절](../../../Docs/TODO.md#24-레벨-폴더-정리-후-확인)을 따른다.


30. `CreateRecoverySkill.py`: 목표 Run 전용 `DA_HealthPotion` 한 개를 Unreal AssetTools로 작성한다. `Item.Consumable.Healing`·GAS Instant Heal·HP 25·AP 1을 사용하며 원본 VFX/몬스터 에셋을 수정하지 않는다. 기존 목적지는 소유 메타데이터와 전체 작성 프로필이 일치해야 한다. 다른 제작자 에셋이나 다른 값은 덮어쓰지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRecoverySkill.py") -unattended -nop4 -NullRHI -nosound -NoTraceServer
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRecoverySkill.py") -RecoverySkillVerifyOnly -unattended -nop4 -NullRHI -nosound -NoTraceServer
```

작성 후 별도 프로세스에서 `-RecoverySkillVerifyOnly`를 실행한다. 결과는 `Saved/Automation/RecoverySkill/Author.json`·`Verify.json`에 기록한다. 회복량·AP 변경은 기존 결과를 자동 덮어쓰는 용도로 지원하지 않으며 정식 데이터 변경과 저장 호환 검토가 필요하다. 이 스크립트는 PIE나 게임을 실행하지 않는다.
