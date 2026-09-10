# ProjectA

Unreal Engine 기반 Grid Turn-Based Combat System 프로젝트입니다.
전투 구조는 grid, unit, turn, GAS 중심으로 구성되어 있고, CommonUI 기반 메뉴와 노드 진행 UI를 하나의 persistent Gameplay 레벨의 전투에 연결합니다.

```text
MainMenu → CharacterCreation (1~4명) → Gameplay → Run Map UI
→ Combat Node → Encounter → 기존 Grid Combat → Victory → Result Continue → Run Map UI
                                             → Defeat → 종료 화면
```

`URunStateSubsystem`이 슬롯·이름·ClassId·HP·노드 진행을 레벨 전환 동안 보존합니다. `AEncounterManager`는 아레나 준비, 파티/적 스폰, 결과 추출과 정리를 맡고 기존 CombatManager/TurnManager/GAS를 재사용합니다. 두 개의 순차 전투 노드가 같은 Gameplay 레벨에서 실행됩니다. 디스크 저장은 포함하지 않습니다.

실행/에셋 설정과 검증 경계는 [Vertical Slice 설정](Docs/VERTICAL_SLICE_SETUP.md)과 [작업 보고](Docs/VERTICAL_SLICE_REPORT.md)를 확인하세요. 네 직업은 현재 기존 `BP_PlayerUnit`을 공통 임시 전투 클래스로 사용합니다.

## 작업 재개 문서

- [TODO와 작업 기록](Docs/TODO.md): 다음 작업, 우선순위, 완료 조건, 중단 지점 기록
- [기획 초안과 구현 현황](Docs/PROJECT_PLAN.md): 이미 작성된 기능, 현재 규칙, 결정할 기획, 단계별 목표
- [코드 리뷰](Docs/CODE_REVIEW.md): P1/P2 문제의 근거와 검증 시나리오

문서는 2026-09-10 현재 작업 트리를 기준으로 정리합니다. 코드 구현, 정식 빌드, 에셋 설정 확인, PIE 검증은 별도로 기록합니다.

## Codex 작업 완료 규칙

[AGENTS.md](AGENTS.md)에 코드 스타일과 작업 완료 규칙을 정의합니다. Codex는 파일 수정 후 변경에 맞는 검증과 README 확인을 마치면 해당 작업의 변경을 한글 메시지로 커밋하고 현재 브랜치의 upstream으로 push합니다.

Codex가 만드는 커밋은 Summary(제목)에 `[codex] ` 접두사를 붙이고, Description(본문)에 변경 내용과 이유 및 검증 결과를 한글로 기록합니다.

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
- 스킬 DataAsset의 AP 비용으로 HUD 표시·사용 가능 판정·실제 차감을 통합
- 공통 스킬 타겟 검증으로 플레이어·적 AI·실행 진입의 진영·생존·전열 보호 규칙 통합
- Tile 기반 이동 및 타겟 선택
- Unit AI Controller
- Combat HUD Widget
- MainMenu / CharacterCreation CommonUI 화면 흐름
- Persistent Gameplay의 Run Map → Encounter → Result 진행과 파티 HP 이관
- 전투 결과 1회 통지, 종료 후 입력/턴 잠금, 유닛·AI·스킬 액터·그리드 점유 정리
- 제자리/접근 스킬의 공통 완료 결과와 실패·취소 후 적 AI 복구
- Blueprint Designer 위젯이 부족해도 동작하는 native fallback UI

## UI

UI 관련 상세 메모는 아래 파일에 정리되어 있습니다.

`Source/ProjectA/UI/UI_README.txt`

현재 UI 쪽 핵심 구조:

- `UMainMenuRootWidget`
- `UMainMenuScreenWidget`
- `UCharacterCreationWidget`
- `AMainMenuPreviewStage`
- `UGameplayRootWidget`: Run / Combat / Modal CommonUI 스택
- `URunMapWidget`: 노드 정의 기반 진행과 선택 요청
- `UCombatHUDWidget`: 기존 Move / Skill / End Turn 입력 연결
- Gameplay 입력 모드는 CommonUI 화면이 관리한다. CombatHUD는 게임·UI 입력을 함께 허용하고, Run Map/Result는 월드 입력을 차단한다.
- `UEncounterResultWidget`: Victory Continue / Defeat

스킬 AP 비용은 `USkillDefinitionDataAsset::ActionPointCost`에서만 설정하며 1 이상이어야 합니다. HUD는 비용을 표시하고, 0·음수 비용 또는 AP 부족 시 선택과 실행을 거절합니다. 공격 컨텍스트 검증과 GAS 커밋이 성공한 뒤 AP를 한 번 차감하며, 실행 시작 후 소비한 AP는 취소 시 환불하지 않습니다. `GA_AttackBase`의 기존 비용 필드는 에셋 참조 호환용으로만 유지하고 실행에는 사용하지 않습니다.

스킬 대상은 `CombatTargetingLibrary::IsValidSkillTarget`으로 검사합니다. 기존 플레이어 규칙대로 전열 보호는 `EnemyUnit` 선택에 적용하고 `bIgnoreFront`로 무시할 수 있습니다. 제자리 `AllyUnit`/`AnyUnit` 스킬은 자기 자신도 선택할 수 있으며, 타일 스킬은 영역에 맞는 빈 타일도 선택합니다. 접근 스킬은 다른 생존 유닛이 필요합니다. 실행 직전에 대상을 다시 검사하며, 유닛 대상이 이동하거나 다른 유닛으로 교체되면 실패 처리합니다. 범위 공격의 피해 대상 계산 통합은 T06에서 진행합니다.

기존 MainMenu와 CharacterCreation은 런타임 fallback을 유지합니다.
Scaffold generator는 기존 native class를 부모로 사용하는 WBP의 Designer tree를 JSON spec 기준으로 생성합니다.

관리 대상 asset:

- `/Game/User_JeHoon/UI/MainMenu/WBP_MainMenuRootWidget`
- `/Game/User_JeHoon/UI/MainMenu/WBP_MainMenuScreenWidget`
- `/Game/User_JeHoon/UI/MainMenu/WBP_CharacterCreationWidget`

CharacterCreation은 하단 4개 파티 슬롯과 L_MainMenu의 실제 월드 캐릭터 프리뷰를 함께 사용합니다. 각 슬롯은 캐릭터 생성하기 버튼으로 시작하고, 생성 중인 슬롯만 편집 패널과 프리뷰 actor를 표시합니다.
`AMainMenuPreviewStage`는 메뉴 카메라, 4개 스폰 앵커, ClassId별 프리뷰 actor class 맵을 관리합니다.

## UI Scaffold Generator

JSON spec 위치:

- `Source/ProjectAEditor/UiScaffoldSpecs/MainMenuRootWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/CharacterCreationWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/GameplayRootWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/RunMapWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/EncounterResultWidget.json`
- `Source/ProjectAEditor/UiScaffoldSpecs/CombatHUDWidget.json`

기본 commandlet:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"
```

실제 생성:

```powershell
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Overwrite -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"
```

현재 지원하는 widget type:

- CanvasPanel
- Overlay
- Button
- TextBlock
- HorizontalBox
- VerticalBox
- Border
- EditableTextBox
- Image
- CommonActivatableWidgetStack

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
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

C++ 파일을 생성, 삭제, 이름 변경한 뒤 프로젝트 파일 재생성:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine
```

## Goal

게임 한 판은 `Gameplay.umap`에 머물며 노드 UI와 전투 Arena를 전환합니다. `TestMap`은 원본 전투 테스트 환경으로 보존하며, `WorldMap.umap`과 `AWorldMapGameModeBase`는 기존 직렬화 참조 호환을 위한 deprecated/미사용 항목입니다. 새로운 월드 탐험이나 전투별 OpenLevel 경로로 사용하지 않습니다.
