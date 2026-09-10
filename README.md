# ProjectA

Unreal Engine 기반 Grid Turn-Based Combat System 프로젝트입니다.
전투 구조는 grid, unit, turn, GAS 중심으로 구성되어 있고, CommonUI 기반 메뉴와 노드 진행 UI를 하나의 persistent Gameplay 레벨의 전투에 연결합니다.

```text
MainMenu → CharacterCreation (1~4명) → Gameplay → Run Map UI
→ Combat Node → Encounter → 기존 Grid Combat → Victory → Result Continue → Run Map UI
                                             → Defeat → 종료 화면
```

`URunStateSubsystem`이 슬롯·이름·ClassId·HP·노드 진행을 레벨 전환 동안 보존합니다. `AEncounterManager`는 아레나 준비, 파티/적 스폰, 결과 추출과 정리를 맡고 기존 CombatManager/TurnManager/GAS를 재사용합니다. 두 개의 순차 전투 노드가 같은 Gameplay 레벨에서 실행됩니다. 진행은 전투 밖 체크포인트에서 디스크에 자동 저장합니다.

실행/에셋 설정과 검증 경계는 [Vertical Slice 설정](Docs/VERTICAL_SLICE_SETUP.md)과 [작업 보고](Docs/VERTICAL_SLICE_REPORT.md)를 확인하세요. 네 직업의 표시명·설명·아이콘·전투 클래스·스탯·시작 스킬은 `DA_VerticalSliceParty.Professions`에서 관리합니다. 기본 설정은 기존 `BP_PlayerUnit`의 전투 밸런스를 유지하며, 직업별 수치는 데이터에서 별도로 지정할 수 있습니다.

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
- 플레이어 타일 명령과 AI 내부 턴 종료 분리, 턴 전환·전투 종료 시 입력 선택 정리
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

스킬 대상은 `CombatTargetingLibrary::IsValidSkillTarget`으로 검사합니다. 기존 플레이어 규칙대로 전열 보호는 `EnemyUnit` 선택에 적용하고 `bIgnoreFront`로 무시할 수 있습니다. 제자리 `AllyUnit`/`AnyUnit` 스킬은 자기 자신도 선택할 수 있으며, 타일 스킬은 영역에 맞는 빈 타일도 선택합니다. 접근 스킬은 다른 생존 유닛이 필요합니다. 실행 직전에 대상을 다시 검사하며, 유닛 대상이 이동하거나 다른 유닛으로 교체되면 실패 처리합니다. 범위 공격의 직접 효과와 스킬 액터는 `CombatTargetingLibrary::ResolveSkillAreaTargets`로 피해 대상을 계산합니다. `Single`은 대상 타일, `AroundTarget`은 대상 타일 중심, `AroundSelf`는 효과 적용 시점의 시전자 타일 중심 체비셰프 반경을 사용합니다. 범위 효과는 기존처럼 시전자 자신을 제외하고 진영·생존·점유 상태를 확인합니다. Row/Column/LeftAndTarget/RightAndTarget/DiagonalTarget/AllEnemies와 음수 반경은 미지원으로 에셋 검증 및 실행 전 검사에서 거절하며 AP를 소비하지 않습니다. 범위 스킬은 `GA_AreaAttack` 계열을 사용하고 기본 단일 공격의 별도 효과 구현은 유지합니다.

타일 액터는 클릭을 `PartyPlayerController::HandleTileClicked`로 전달합니다. 플레이어 명령은 권한·전투 입력 잠금·Player 팀·활성 턴·생존·busy를 확인하며 클릭 시 비용과 타겟을 재검사합니다. UI의 턴 종료는 `PartyPlayerController::RequestEndTurn`, C++ AI의 내부 종료는 `CombatManager::RequestEndTurnForUnit(this)`를 사용합니다. 기존 CombatManager의 인자 없는 Blueprint 종료 함수는 deprecated 상태이며 플레이어 입력 검사를 거칩니다. 턴 전환과 전투 종료 시 이전 선택·하이라이트를 정리합니다.

캐릭터 생성의 Edit는 선택한 슬롯의 이름(1~32자)과 직업을 편집합니다. 저장 전에는 파티 데이터가 바뀌지 않으며 취소하면 기존 값이 유지됩니다. ClassInfo는 같은 직업 정의의 실제 HP/AP/보조 AP와 시작 스킬을 읽기 전용으로 표시합니다. 저장한 이름·직업과 사용한 직업 목록은 Gameplay로 전달되고, Encounter 스폰은 동일한 설정을 적용한 뒤 이전 전투의 HP를 복원합니다.

직업 설정의 `bUseUnitClassDefaults`가 켜져 있으면 기존 클래스의 스탯·스킬을 읽고, 끄면 `MaxHP/ActionPoints/SubActionPoints/StartingSkills`를 사용합니다. `CombatClass`가 없을 때만 기존 직업 클래스 매핑과 명시적 fallback을 사용합니다. 알 수 없는 직업, 잘못된 수치·스킬 및 중복 Ability 항목은 거절합니다. 아이콘은 각 정의의 `Icon`에 지정하며 새 직업 일러스트나 밸런스 차별화는 이번 변경에 포함하지 않습니다.

스폰 공격은 몽타주 종료와 스킬 액터의 impact가 모두 끝난 뒤 GAS를 종료합니다. 몽타주가 없으면 impact까지 기다립니다. 스폰 클래스는 `SkillActorBase` 계열이어야 하며, `RequestFinish`를 impact 전에 호출하거나 액터가 파괴되면 실패로 종료합니다. 미충돌은 Ability의 `SpawnedActorTimeout`(기본 10초) 후 실패로 정리합니다. 취소·시전자 사망 시 대기 액터를 파괴해 늦은 피해를 차단하고 이미 소비한 AP는 환불하지 않습니다. 플레이어는 AP와 보조 AP가 모두 0이면 행동 완료 다음 틱에 자동으로 턴을 종료하며, 보조 AP가 남으면 이동/아이템을 계속 사용할 수 있습니다.

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

## 저장·이어하기와 옵션 (T11)

새 게임 시작, 전투 결과 확정, Continue 시 단일 `ProjectA_Run` 슬롯에 파티 이름·직업·HP·노드 진행·결과와 직업 데이터 경로를 저장합니다. 전투 중 종료하면 해당 전투 시작 전 체크포인트로 돌아갑니다. 결과 화면에서 종료했다면 결과 화면으로 복원하며, 패배하거나 모든 노드를 완료한 기록은 이어할 수 없습니다. 새 파티로 게임을 시작하면 기존 저장을 교체합니다.

메인메뉴 Continue는 저장이 없거나 손상/버전 불일치/직업 데이터 누락/종료된 진행이면 비활성화하고 이유를 표시합니다. 쓰기 실패는 시작 화면 또는 Gameplay의 지도/결과 화면에 표시하며 게임 중 쓰기 실패가 이전 체크포인트까지 갱신했다는 뜻은 아닙니다. 저장 형식은 버전 1이며 이전 버전 변환과 전투 도중 액터/AP/발사체 복원은 지원하지 않습니다.

Options에서 그래픽 품질과 수직 동기화를 선택하고 **적용 및 저장**으로 반영합니다. 적용 전 닫기는 변경을 버리며, 설정은 Unreal `GameUserSettings.ini`에 유지됩니다. Quit는 실제 게임 종료를 요청합니다. 테스트는 `-ProjectASaveSlot=T11_PIE`처럼 별도 슬롯을 지정해 플레이 저장을 보호합니다.
