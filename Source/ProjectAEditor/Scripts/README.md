# Gameplay 에셋 도구

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

6. 저장된 메뉴 맵에서 시작하는 PIE 통합 테스트를 실행한다. 이 환경의 UE 5.7에서는 `-NullRHI` 상태의 PIE travel이 `GenericWindow::GetRestoredDimensions` fatal을 일으켜, 실제 렌더러의 `-RenderOffscreen`을 사용한다.

```powershell
& $editorExecutable $projectFile -unattended -nop4 -RenderOffscreen -nosound -Windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=0 '-ExecCmds=Automation RunTests ProjectA.VerticalSlice.SavedMapsPIELoop' '-TestExit=Automation Test Queue Empty' ("-ReportExportPath=$projectDirectory/Saved/Automation/VerticalSlicePIE")
```

검증 범위:

- 캐릭터 생성·노드·Continue는 위젯 delegate, 첫 Move/Skill은 Slate 합성 마우스로 HUD와 타일 입력 검사.
- 기본 공격·Enemy AI로 첫 Victory 확인, 메모리 SkillData로 제자리 몽타주 실행 확인, 두 번째 Defeat는 치명 피해로 유도.
- 디스크 에셋·밸런스는 변경하지 않는다. 실제 마우스 사용성·육안 확인은 별도다.
- 오프스크린 클릭은 위 명령의 창 위치·크기를 유지한다. 기본 창 설정의 과거 hit-test 실패는 같은 빌드에서 창 조건 지정 후 통과했다. 클릭 검사를 함수 호출로 대체하지 않는다.

종료 코드와 함께 JSON의 테스트 상태·오류 및 `Test Completed. Result={Success}`를 확인한다. 화면 캡처 경로는 `Saved/Automation/VerticalSliceScreenshots`다. 작동 테스트 실행 원칙과 결과 기록은 [TEST_REPORT](../../../Docs/TEST_REPORT.md)를 따른다.
