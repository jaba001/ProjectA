# Gameplay 에셋 도구

UI 구조·생성 옵션·JSON 필드는 [UI_README](../../../Docs/UI_README.md)를 따른다.

Development Editor / Win64 빌드를 사용한다. Python은 `-EnablePlugins=PythonScriptPlugin`으로 해당 프로세스에서만 활성화한다. 제작 경로는 `/Game/User_JeHoon`이다. 최초 생성·Audit는 기존 TestMap을 요구하므로 현재 사용자 삭제 상태에서 실행 전 원본 가용성을 확인한다.

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

- 실제 메뉴·캐릭터 생성·전투 노드에서 시작하며 저장된 DA 4개의 목록·계획·피해 적용을 확인한다.
- 위젯 버튼과 타일 선택 delegate로 검사한다. 운영체제 마우스 hit-test·첫 클릭 포커스 검증은 포함하지 않는다.
- 전용 시험 저장만 생성·정리하며 기존 저장과 디스크 에셋·밸런스는 변경하지 않는다.

종료 코드와 함께 JSON의 테스트 상태·오류 및 `Test Completed. Result={Success}`를 확인한다. 화면 캡처 경로는 `Saved/Automation/SkillLoadoutScreenshots`다. 메뉴·설정 검사는 별도 `ProjectA.VerticalSlice.SavedMenuLifecycle`을 사용한다. 작동 테스트는 [작업 규칙](../../../AGENTS.md#작동-테스트와-보고서)을 따르며 남은 확인은 [TODO](../../../Docs/TODO.md), 완료 결과는 [HISTORY](../../../Docs/HISTORY.md)에 짧게 기록한다.

7. `CreateRangedAttack.py`: 기본 공격 DA·GA를 `BPDA_RangedAttack`·`BPGA_RangedAttack`으로 복제하고 기존 단일 대상 투사체 변환을 연결한다. 캐릭터 장착과 기존 에셋은 유지하며 대상이 이미 있으면 덮어쓰지 않는다. `-RangedAttackVerifyOnly`는 제작 직후 저장된 연결·복제 수치만 다시 읽는다. 두 명령 모두 게임을 실행하지 않는다.

```powershell
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRangedAttack.py") -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=pythonscript ("-script=$scriptDirectory/CreateRangedAttack.py") -RangedAttackVerifyOnly -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI
```
