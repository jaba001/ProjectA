# ProjectA

Unreal Engine 기반 Grid Turn-Based Combat System 프로젝트입니다.
전투 구조는 grid, unit, turn, GAS 중심으로 구성되어 있고, 메인 메뉴와 캐릭터 생성 UI는 CommonUI 기반 흐름과 native fallback UI를 함께 사용합니다.

## Engine

- Unreal Engine 5.7
- C++
- Gameplay Ability System
- CommonUI
- UMG

## Modules

### ProjectA

런타임 게임 모듈입니다.

주요 역할:

- Grid 기반 전투 타일과 전투 공간 관리
- Unit, EnemyUnit, PlayerUnit 기반 유닛 구조
- TurnManager 기반 턴 진행
- Gameplay Ability System 기반 공격/스킬/데미지 처리
- MainMenu와 CharacterCreation의 CommonUI runtime fallback

### ProjectAEditor

에디터 전용 도구 모듈입니다.

주요 역할:

- UI scaffold 생성 commandlet 제공
- JSON spec 기반 native widget class 생성
- Widget Blueprint asset 생성
- Widget Blueprint Designer에 실제 widget tree 저장

Editor-only dependency는 이 모듈에만 둡니다.
`UnrealEd`, `UMGEditor`, `AssetTools`, `BlueprintGraph`, `KismetCompiler` 같은 모듈을 runtime `ProjectA.Build.cs`에 추가하지 않습니다.

## Runtime Features

- Grid 기반 전투 시스템
- Turn Manager 기반 턴 진행 구조
- Gameplay Ability System 활용
- Tile 기반 이동 및 타겟 선택
- Unit AI Controller
- Combat HUD Widget
- MainMenu / CharacterCreation CommonUI 화면 흐름
- Blueprint Designer 위젯이 부족해도 동작하는 native fallback UI

## UI

UI 관련 상세 메모는 아래 파일에 정리되어 있습니다.

`Source/ProjectA/UI/UI_README.txt`

현재 UI 쪽 핵심 구조:

- `UMainMenuRootWidget`
- `UMainMenuScreenWidget`
- `UCharacterCreationWidget`
- `UMainMenuScaffoldTestWidget`

기존 MainMenu와 CharacterCreation은 런타임 fallback을 유지합니다.
새로운 scaffold generator는 기존 WBP를 덮어쓰지 않고 테스트 전용 WBP를 생성합니다.

기본 생성 asset:

`/Game/User_JeHoon/UI/MainMenu/Generated/WBP_MainMenuScaffoldTest`

## UI Scaffold Generator

JSON spec 위치:

`Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScaffoldTest.json`

기본 commandlet:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun
```

실제 생성:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Overwrite
```

현재 지원하는 widget type:

- CanvasPanel
- Overlay
- Button
- TextBlock
- VerticalBox
- Border
- EditableTextBox

## Project Structure

```text
Source/
  ProjectA/
    Animation/
    Combat/
    Controller/
    DataAsset/
    Game/
    GAS/
    Grid/
    Types/
    UI/
    Unit/
  ProjectAEditor/
    Commandlets/
    UiScaffoldSpecs/
```

## Build

Development Editor | Win64 빌드:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

C++ 파일을 생성, 삭제, 이름 변경한 뒤 프로젝트 파일 재생성:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine
```

## Goal

GAS를 활용한 확장 가능한 턴제 전투 아키텍처를 유지하면서, UI는 CommonUI runtime 안정성과 Blueprint Designer 편집 가능성을 함께 확보하는 것을 목표로 합니다.
