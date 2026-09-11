# ProjectA

Unreal Engine 5.7 기반의 파티·Grid 턴제 전투 프로젝트입니다. 파티 빌드 성장, 직접 조작 전술 전투, 상대 Snapshot을 사용하는 Async PvP와 아군 Co-op을 목표로 합니다. 물리적인 월드맵 탐험 대신 UI에서 선택·강화·전투·보상을 이어갑니다.

현재 기본 Run은 싱글플레이이며 같은 Gameplay 레벨에서 두 순차 전투를 진행합니다.

```text
MainMenu → CharacterCreation (1~4명) → Gameplay → Run Map UI
→ Combat → Victory → Result Continue → 다음 노드
         → Defeat → 종료 화면
```

`URunStateSubsystem`이 파티·이름·직업·HP·노드 진행을 보존하고, `AEncounterManager`가 기존 CombatManager/TurnManager/GAS를 사용해 전투를 준비·정리합니다. 일반 Continue는 지원되는 오프라인 저장을 복구하며 협동·계정 기반 저장은 필요한 세션 연결을 안내합니다.

T14는 로컬 상대 Snapshot, Listen Server 상태 동기화, 확정 턴 복구, 아군 AI와 관리 Run v4의 명시적 Host 승계를 구현했습니다. 최대 4인·원래 소유자만 조작·최초 참가 번호순 승계가 기준입니다. Host가 사전 동의 없이 AI 전환을 확정하고 해당 Run에서는 인간 조작으로 돌아오지 않습니다. 현재 Host만 노드·Continue를 결정합니다.

6번까지 로컬 개발 범위는 완료했습니다. 7번 최신 승계 확장은 사용자 작동 검증 대기이며, 8번 실제 Steam/PlayFab 인증·P2P·공유 저장·MMR은 미구현입니다. 2026-09-11 사용자 확인에 따라 자체 App ID·PlayFab Title 준비부터 진행하며 [서비스 준비 절차](Docs/MULTIPLAYER.md#t14-8-서비스-준비)와 [준비 보고서](Docs/TEST_REPORT.md#d-t14-8-서비스-준비-보고서)에 현재 상태를 기록했습니다. 관리 Run의 **싱글로 전환하기** 메뉴는 C++ 개발 fixture가 호출자·저장 대상을 설정해야 표시됩니다. 기본 새 게임이 협동 Run을 만들거나 Steam 계정을 인증하는 기능은 아직 없습니다.

2026-09-11 사용자 요청으로 현재 코드의 2인 Listen Server/Client 전투 자동화 1건을 직접 실행해 통과했습니다(오류 0·경고 4). 조작권·이동·스킬·상태 동기화·두 전투의 완료 흐름을 확인했으며 Steam 접속이나 수동 조작 검증은 포함하지 않습니다. 상세 결과는 [테스트 보고서 F절](Docs/TEST_REPORT.md#f-2026-09-11-codex-직접-실행-2인-전투-동기화)에 있습니다.

## 문서 안내

Docs는 다음 6개 문서로 관리합니다. 작업마다 새 문서를 추가하지 않고 해당 문서를 갱신합니다.

| 문서 | 찾을 내용 |
|---|---|
| [GAME_DESIGN](Docs/GAME_DESIGN.md) | 게임 목표·Run 흐름·협동 확정 정책·콘텐츠 방향 |
| [PROJECT_PLAN](Docs/PROJECT_PLAN.md) | 현재 구조·실행 흐름·에셋·설정 방법 |
| [TODO](Docs/TODO.md) | 다음 작업·T14 1~8번 상태·남은 결정 |
| [MULTIPLAYER](Docs/MULTIPLAYER.md) | Snapshot·서버 권위·저장 버전·AI·승계·온라인 연결 계약 |
| [TEST_REPORT](Docs/TEST_REPORT.md) | 사용자가 실행할 테스트 절차·기대 결과·결과 기록 |
| [HISTORY](Docs/HISTORY.md) | 완료 작업·커밋·당시 검증 근거와 실패 수정 이력 |

## Codex 작업 규칙

사용자와 Codex가 만드는 엔진 에셋은 모두 `Content/User_JeHoon/` (`/Game/User_JeHoon/`) 아래에 저장합니다. 검증용 생성본도 동일하며 메뉴 검증 위젯 3종은 `Validation/T12`에 있습니다. 외부 리소스·템플릿 원본은 기존 위치를 유지하고 직접 수정할 사본을 작업 폴더에 만듭니다. UI 생성 도구도 이 경로를 검사합니다.

[AGENTS.md](AGENTS.md)에 코드·문서·Git 규칙을 정의합니다. **작동 테스트는 사용자가 수행합니다.** Codex는 컴파일·정적 검사와 README 확인 후 테스트 보고서를 갱신하고, 사용자 검증 대기를 명시해 해당 변경을 커밋·push합니다. 이후 사용자가 특정 실행을 명시적으로 요청하지 않는 한 PIE·게임·Unreal 자동화·패키지 작동 테스트를 시작하지 않습니다. Visual Studio도 자동으로 열지 않습니다.

커밋 제목은 `[codex] `로 시작하고 본문에는 변경 내용·이유·실제 검증 결과를 한글로 기록합니다.

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
- USaveGame v1 상대 Snapshot 저장·검증과 DataAsset 카탈로그 기반 Encounter 생성
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

타일 액터는 클릭을 `PartyPlayerController::HandleTileClicked`로 전달합니다. 이동·스킬·회복약·턴 종료는 Actor 없는 `FCombatActionRequest`로 공통 서버 검증을 거칩니다. 서버는 연결에 바인딩한 원래 참가자, 캐릭터 소유권, Run·Host 세대·전투 실행 ID·연결 바인딩 ID·턴 번호·요청 순번을 검사합니다. Host도 타인 캐릭터를 조작할 수 없으며 이미 처리한 요청과 이전 전투/연결/턴의 명령을 거절합니다. 이동 범위는 서버에서 다시 계산하고, 스킬은 실제 장착 목록의 `FPrimaryAssetId`로 유일하게 해석해 서버의 비용·GAS·대상 규칙을 적용합니다.

UI의 턴 종료는 `PartyPlayerController::RequestEndTurn`, 회복약은 `RequestHealingItem`, C++ AI의 내부 종료는 `CombatManager::RequestEndTurnForUnit(this)`를 사용합니다. 기존 CombatManager의 인자 없는 Blueprint 종료 함수는 deprecated 상태이며 플레이어 입력 검사를 거칩니다. `Accepted` 응답은 실행 진입을 뜻하고 이동·스킬의 최종 성공은 기존 행동 완료 이벤트로 확인합니다. 늦은 응답은 새 전투나 최신 요청·새 선택을 덮지 않습니다. 턴 전환과 전투 종료 시 이전 선택·하이라이트를 정리합니다.

순차 3번은 서로 다른 NetDriver를 가진 Listen Server/Client 두 PIE 월드에서 실제 Server/Client Reliable RPC를 검증했습니다. 서버 전용 TurnManager와 CombatManager의 복제 뷰, GAS HP/MaxHP RepNotify, 유닛 AP/SubAP·팀·장착·이동·턴/행동/사망 상태, Grid 점유·전열 보호·결과/HUD 복제를 연결했습니다. `AGameplayGameState`는 Run 단계·파티·노드·결과의 읽기 전용 표시 값을 전달하며 클라이언트의 RunState를 권위 상태로 사용하지 않습니다. Development Editor / Win64 빌드, 2인 PIE를 포함한 전체 자동화 42건(성공 26·경고 동반 성공 16·실패 0), 별도 Snapshot 상대 PIE 1건을 통과했습니다.

네트워크 참가자는 `AGameplayGameModeBase::AssignRunParticipant`와 `ApplyCombatParticipantBindings`에서 신뢰된 서버 C++ 코드로 배정합니다. 현재 자동화는 알려진 연결에 계정을 명시적으로 연결하며 실제 로그인을 제공하지 않습니다. 실제 인증은 8번입니다. 기존 미식별 Run/TestMap의 입력 호환은 Standalone에만 적용하고 손상된 식별 Run으로 우회하지 않습니다. 노드 선택·Continue는 현재 Host의 로컬 서버 연결과 신뢰된 계정 배정을 확인해 허용하며 Client는 진행 상태를 표시합니다. 네트워크 최종 유닛 상태는 결과 화면에서 유지하고 명시적인 Continue 또는 월드 종료에서 정리합니다. 상세 경계와 검증 명령은 [Listen Server 안내](Docs/MULTIPLAYER.md)를 참고하세요.

캐릭터 생성의 Edit는 선택한 슬롯의 이름(1~32자)과 직업을 편집합니다. 저장 전에는 파티 데이터가 바뀌지 않으며 취소하면 기존 값이 유지됩니다. ClassInfo는 같은 직업 정의의 실제 HP/AP/보조 AP와 시작 스킬을 읽기 전용으로 표시합니다. 저장한 이름·직업과 사용한 직업 목록은 Gameplay로 전달되고, Encounter 스폰은 동일한 설정을 적용한 뒤 이전 전투의 HP를 복원합니다.

직업 설정의 `bUseUnitClassDefaults`가 켜져 있으면 기존 클래스의 스탯·스킬을 읽고, 끄면 `MaxHP/ActionPoints/SubActionPoints/StartingSkills`를 사용합니다. `CombatClass`가 없을 때만 기존 직업 클래스 매핑과 명시적 fallback을 사용합니다. 알 수 없는 직업, 잘못된 수치·스킬 및 중복 Ability 항목은 거절합니다. 아이콘은 각 정의의 `Icon`에 지정하며 새 직업 일러스트나 밸런스 차별화는 이번 변경에 포함하지 않습니다.

스폰 공격은 몽타주 종료와 스킬 액터의 impact가 모두 끝난 뒤 GAS를 종료합니다. 몽타주가 없으면 impact까지 기다립니다. 스폰 클래스는 `SkillActorBase` 계열이어야 하며, `RequestFinish`를 impact 전에 호출하거나 액터가 파괴되면 실패로 종료합니다. 미충돌은 Ability의 `SpawnedActorTimeout`(기본 10초) 후 실패로 정리합니다. 취소·시전자 사망 시 대기 액터를 파괴해 늦은 피해를 차단하고 이미 소비한 AP는 환불하지 않습니다. 플레이어는 AP와 보조 AP가 모두 0이면 행동 완료 다음 틱에 자동으로 턴을 종료하며, 보조 AP가 남으면 이동/아이템을 계속 사용할 수 있습니다.

T13 전투 콘텐츠: 각 전투의 새 유닛은 기본 회복약 1개를 가집니다. T14의 Snapshot 상대는 적 아이템 AI가 미구현이므로 회복약을 지급하지 않습니다. HUD의 회복약 버튼은 자신을 HP 40만큼 회복하고 수량 1개와 SubAP 1을 소모합니다. 최대 HP를 넘지 않으며, 만피·수량 부족·잘못된 대상은 소모 없이 거절합니다. `HealingItemAmount/HealingItemCount`는 유닛 클래스에서 조정할 수 있고 C++/Blueprint의 `StartItemAction`은 생존 아군도 대상으로 받습니다. HUD는 자기 회복만 제공합니다.

`PartyDefinition.EncounterSkillPool`에서 직업 설정 후 스킬 하나를 가중 추첨해 즉시 GAS에 부여하고 추가 슬롯에 장착합니다. 보유 Ability·잘못된 데이터·0 이하 가중치는 추첨에서 제외하며 추가 슬롯은 최대 4개입니다. 현재 풀은 `DA_SweepingStrike`(휩쓸기) 하나이며 대상 주변 체비셰프 반경 1의 적에게 피해 10, AP 1을 적용합니다. 시작 직업 스킬은 유지합니다. 회복약과 추가 스킬은 전투 한정 지급으로 다음 전투에서 새로 지급하며 저장된 HP만 이어집니다. 영구 인벤토리·보상 선택 UI는 구현 범위에 포함하지 않습니다. 에셋 작성 스크립트는 `Source/ProjectAEditor/Scripts/ConfigureCombatContent.py`입니다.

적 AI는 유효 대상 수/AP와 거리·HP 선호도로 스킬을 비교합니다. 기본 공격의 임시 `+100000` 가중치는 제거했고 스킬 간 동점은 기존 순서를 유지합니다. 이동은 CombatManager의 같은 영역·점유·이동 범위 후보 중 가장 가까운 상대와의 거리가 줄어드는 타일만 평가합니다. 거리 점수는 200 Unreal 단위를 기준으로 정규화합니다. 이동 성공은 다음 틱에 재판단하고 실패는 턴 종료로 복구합니다. 유효한 스킬이나 전진 후보가 없으면 대기합니다.

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
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"
```

실제 생성:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Overwrite -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"
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

새 게임 시작, 확정 턴 경계, 전투 결과 확정, Continue 시 기본 `ProjectA_Run` 슬롯에 진행을 저장합니다. 식별된 Run은 전투 중 종료해도 마지막 확정 턴부터 이어가며, 처리 중이던 행동은 저장하지 않습니다. 결과 화면에서 종료했다면 결과 화면으로 복원하며, 패배하거나 모든 노드를 완료한 기록은 이어할 수 없습니다. 새 파티로 게임을 시작하면 선택된 슬롯의 기존 저장을 교체합니다.

T14 로컬 상대 검증은 [Snapshot 설정 스크립트](Source/ProjectAEditor/Scripts/ConfigureSnapshotContent.py) 실행 후 `-ProjectAOpponentSnapshot=SampleOpponent`로 선택합니다. 상대 데이터는 `ProjectA_Opponent_SampleOpponent`, Run 체크포인트는 `ProjectA_SnapshotRun_SampleOpponent`로 분리됩니다. 명시적인 `-ProjectASaveSlot=...`은 우선하며, 이어하기에는 같은 상대 실행 인자를 사용합니다. 저장값과 에셋 설정은 [로컬 Snapshot 안내](Docs/MULTIPLAYER.md)를 참고하세요.

메인메뉴 Continue는 저장이 없거나 손상/버전 불일치/직업 데이터 누락/종료된 진행이면 비활성화하고 이유를 표시합니다. 신규 Run의 전투 밖 저장은 v2, 확정 턴 저장은 v3이며 Run·원래 참가자·캐릭터 소유자·Host/세대를 유지합니다. 실제 식별 정보가 없는 v1은 `LegacyOffline`으로 이어가며 전투 밖 복구만 지원합니다. 손상된 v2/v3를 이전 형식으로 우회하지 않습니다.

v3는 턴 순서·다음 유닛·HP/AP/SubAP·재고·사망·점유·실제 장착·고정된 상대 Snapshot을 저장합니다. 저장에 실패하면 이전 파일을 보존하고 다음 턴을 멈추며 Host의 **저장 다시 시도** 버튼으로 재시도합니다. 결과와 Continue도 저장 성공 후 확정합니다. 복구할 때 새 Actor와 명령 실행 ID를 만들며, 처리 중 이동/발사체·활성 GAS 능력·지속 효과·쿨다운 등 표현하지 못하는 상태는 거절합니다. 일반 v3 협동 복구는 기존 Host와 원래 참가자 전원의 서버 연결 배정 후 실행하고, 관리 v4는 승인된 현재 인간 참가자만 연결합니다. 실제 로그인·재접속 UI는 후속 범위입니다. 자세한 계약은 [확정 턴 저장·복구](Docs/MULTIPLAYER.md)를 참고하세요.

아군 AI는 `APlayerUnit`의 서버 컴포넌트로 자기 회복약·아군 피해 없는 공격·접근 이동·턴 종료를 실행합니다. 원래 소유자와 Host의 인간 입력은 서버와 HUD에서 차단하며 캐릭터·소유권·팀은 유지합니다. 전투 시작 전 신뢰된 서버 API에서 원래 소유권을 확인해 모드를 지정하고 전투 본문 schema 2에 저장·복원합니다. 개인 사전 동의는 요구하지 않습니다. 기본 Run은 인간 조작이며 연결 끊김만으로 AI가 켜지지 않습니다. 관리 Run v4의 명시적 재개는 전투 밖에 보존한 인간 참가 목록으로 모드를 결정하며 기존 AI 계정을 다시 인간 목록에 추가할 수 없습니다. [아군 AI 안내](Docs/MULTIPLAYER.md)에 API와 검증 방법을 정리합니다.

관리 Run v4는 Run별 공유 개발 저장소에 전체 본문·revision·HostEpoch·실행 세션을 함께 확정합니다. 모든 자동 저장과 턴 저장은 배타적 실행 권한을 검사하며 일반 `ProjectA_Run` 슬롯에 대신 쓰지 않습니다. 재개는 최신 기록의 원래 소유권·번호·확정 턴을 유지하고 새 Host와 영속 인간 목록만 승인해 적용합니다. 기존 v1/v2/v3를 자동 이관하지 않으며, 같은 PC의 개발 저장소 검증을 원격 중앙 저장이나 위변조 방지로 취급하지 않습니다.

현재 새 싱글플레이는 Run마다 임시 개발용 참가자 한 명을 생성하고 생성한 캐릭터들을 해당 참가자에게 연결합니다. Identity schema 2의 최초 참가 번호는 1이며 레벨 이동·다음 노드·저장/복원 동안 유지합니다. 새 게임은 새 식별값을 만듭니다. 이전 동의 필드는 호환용 `Unknown`·버전 0으로 보존하며 AI 승인 조건으로 사용하지 않습니다. 기존 schema 1의 번호는 0(미상)으로 유지하고 배열 순서로 추측하지 않습니다. 개발용 ID와 번호/소유권 조회는 실제 계정 인증이나 Co-op 접속·Host 승계를 제공하지 않습니다.

Options에서 그래픽 품질과 수직 동기화를 선택하고 **적용 및 저장**으로 반영합니다. 적용 전 닫기는 변경을 버리며, 설정은 Unreal `GameUserSettings.ini`에 유지됩니다. Quit는 실제 게임 종료를 요청합니다. 테스트는 `-ProjectASaveSlot=T11_PIE`처럼 별도 슬롯을 지정해 플레이 저장을 보호합니다.

## 메뉴 프리뷰와 UI 구조 (T12)

메뉴 화면 구조의 기준(D08)은 기존 Designer WBP입니다. JSON 명세는 생성·바인딩 검증용 scaffold이며 기존 WBP를 자동 재생성하지 않습니다. `GenerateUiScaffold -Spec=... -AddMissing`은 기존 위젯의 속성과 계층을 유지하고 누락된 위젯만 추가합니다. `-Overwrite`와 함께 사용할 수 없으며, 현재 프로젝트 명세는 `generateNativeSource=false`를 유지합니다. 생성 화면에는 누락됐던 `Text_StartGameStatus`를 추가해 시작/저장 실패 안내가 보이도록 연결했습니다.

MainMenu는 배치된 `MainMenuPreviewStage` 카메라와 표시 전용 `BP_PartyMenuPreview`를 사용합니다. 네 ClassId 모두 기존 캐릭터 메시를 재사용하며 직업별 새 모델·애니메이션은 제작하지 않았습니다. GameMode는 전투 Pawn을 생성하지 않습니다. 생성 화면 Back/X는 프리뷰를 정리하고 초안을 버리며, 재진입하면 네 슬롯이 빈 상태로 시작합니다. 상세 편집 패널이 열려 있으면 Back은 먼저 패널만 닫습니다. 슬롯 패널은 최소 높이를 유지하면서 내용에 맞게 늘어나 ClassInfo 버튼이 잘리지 않도록 합니다.

기존 메뉴 WBP 3종과 별도 경로의 JSON 생성본은 `ProjectA.Menu.AssetContracts`에서 바인딩 이름·타입을 확인합니다. 생성본 검증은 `/Game/User_JeHoon/Validation/T12`에 임시 생성한 뒤 `-T12GeneratedAssets`를 지정하며 원본 에셋에 덮어쓰지 않습니다. 맵 설정 스크립트는 `Source/ProjectAEditor/Scripts/ConfigureMenuPreview.py`입니다.
