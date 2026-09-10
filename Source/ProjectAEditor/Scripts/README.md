# Gameplay asset 도구

Development Editor | Win64 정식 빌드 후 실행한다. Python은 실행한 에디터 프로세스에서만 `-EnablePlugins=PythonScriptPlugin`으로 활성화하며 `.uproject`나 프로젝트 Config를 변경하지 않는다.

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

테스트는 위젯 버튼 델리게이트로 캐릭터 생성·노드 선택·Continue를 실행한다. 첫 Move/Skill은 실제 Slate 합성 마우스 이벤트로 HUD 버튼과 월드 타일을 클릭한다. 히트 테스트·CommonUI·게임 뷰포트 전달 및 이동/피해 적용을 검사한 뒤 기존 기본 공격과 Enemy AI로 Victory까지 진행한다. 기존 몽타주의 제자리 실행은 메모리 내 SkillData 복사본으로 한 번 검사하고, 두 번째 Encounter의 Defeat는 테스트용 치명 피해로 유도한다. 디스크 에셋이나 전투 수치를 바꾸지 않는다. 물리적 마우스로 전체 흐름을 조작한 검증과 육안 검수는 별도다.

오프스크린 Slate 클릭 검증은 창 위치와 크기를 위 명령처럼 명시한다. 2026-09-10 기본 창 설정의 첫 실행에서는 Move 버튼 hit-test가 실패했으며, 창 조건을 명시한 동일 빌드의 PIE 재실행은 통과했다. 클릭 검사를 직접 함수 호출로 대체하지 않는다.

자동화 프로세스가 exit 0이어도 테스트 자체는 실패할 수 있으므로 JSON 보고서의 테스트 결과와 `Test Completed. Result={Success}` 로그를 확인한다. 화면 캡처는 `Saved/Automation/VerticalSliceScreenshots`에 요청한다.
