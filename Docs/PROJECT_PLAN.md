# ProjectA 구현 구조와 설정

기준일: 2026-09-10. 현재 코드의 책임, 기본 Run 사용법, Gameplay 에셋 연결을 한곳에서 관리한다.

기본 Run은 싱글플레이다. T14 6번은 로컬 개발 저장소 범위에서 완료했으며, 7번의 최종 작동 검증은 사용자가 직접 진행한다. 8번의 실제 Steam/PlayFab 서비스와 MMR은 미구현이다. T14 전체 완료로 취급하지 않는다.

| 찾는 내용 | 기준 문서 |
|---|---|
| 게임 목표와 콘텐츠 방향 | [GAME_DESIGN](GAME_DESIGN.md) |
| 남은 작업과 완료 조건 | [TODO](TODO.md) |
| Snapshot·Co-op·저장/재개·온라인 계약 | [MULTIPLAYER](MULTIPLAYER.md) |
| 실제 검증 결과와 사용자 확인 항목 | [TEST_REPORT](TEST_REPORT.md) |
| 변경 과정과 과거 판단 | [HISTORY](HISTORY.md) |

## 기본 실행 흐름

Unreal Engine 5.7의 `ProjectA.uproject`를 사용한다. 아래는 사용자가 직접 실행할 때의 안내다.

1. 기본 시작 맵인 `/Game/User_JeHoon/LEVEL/MainMenu`를 연다.
2. Play → New Game → CharacterCreation에서 1~4명의 캐릭터를 생성한다. 직업 화살표와 Edit로 직업·이름을 바꾼다.
3. Start Game → `/Game/User_JeHoon/LEVEL/Gameplay` → Run Map에서 첫 Combat 노드를 선택한다.
4. HUD의 스킬을 선택하고 대상 타일을 클릭한다. Move는 표시된 이동 가능 빈 아군 타일을 선택한다.
5. 행동 완료 후 End Turn으로 적 턴을 진행한다. 회복약은 전투 중 HUD에서 사용한다.
6. Victory → Continue → 두 번째 Combat 노드를 진행한다. 두 번째 Victory 뒤 Continue는 완료된 Run Map을 표시한다.
7. 파티 전멸 시 Defeat 화면을 유지하며 추가 전투 입력을 받지 않는다.

```mermaid
flowchart LR
    A[MainMenu] --> B[CharacterCreation]
    B -->|OpenLevel 1회| C[Gameplay]
    C --> D[Run Map UI]
    D --> E[Encounter 준비]
    E --> F[Grid Combat]
    F -->|Victory| G[Result]
    G -->|Continue| D
    F -->|Defeat| H[패배 화면]
```

Gameplay는 계속 유지하는 단일 레벨이며 두 Combat 노드는 `DefaultEncounter`를 재사용한다. 월드 진행은 CommonUI 노드 화면으로 표현한다. 물리적인 WorldMap 탐험과 전투별 CombatMap 전환은 현재 흐름에 없다.

## 모듈과 책임

런타임은 `Source/ProjectA`, 에셋 생성·에디터 도구·PIE 테스트는 `Source/ProjectAEditor`에 둔다. Editor 의존성을 런타임 모듈로 옮기지 않는다.

| 구성 | 책임과 수명 |
|---|---|
| `URunStateSubsystem` | GameInstance 수명. 파티·노드·결과 HP·Run/참가자/캐릭터 소유권 보존, 저장 검증과 관리 Run lease 소유. 영속 데이터에 Actor 참조를 넣지 않음 |
| `AGameplayGameModeBase` | 서버 월드의 Arena·EncounterManager·CombatManager 준비와 신뢰된 C++ 참가자 배정. 종료 시 전투를 멈춘 뒤 관리 lease 해제 |
| `AGameplayGameState` | 단계·파티·노드·결과와 전투/아레나 참조를 읽기 전용 뷰로 복제. Client RunState는 서버 권위의 대체물이 아님 |
| `AGameplayPlayerController` | `APartyPlayerController` 상속. 로컬 Root UI, 소유 연결의 전투 RPC, 현재 Host의 노드 선택·Continue 요청 |
| `UGameplayRootWidget` | 해당 플레이어 화면의 CommonUI Run/Combat/Modal 스택과 저장 실패·재시도 안내 |
| `URunMapWidget` | 노드와 진행 상태 표시, 선택 요청. 직접 Spawn하지 않음 |
| `AEncounterManager` | Encounter 준비·스폰·전투 연결·HP 추출·정리와 Run 전이. 관리 참여 목록을 새 전투/복구에 적용 |
| `ACombatArena` | 배치된 Grid, 슬롯별 좌표, 카메라, 타일 활성화 관리 |
| `ACombatManager` / `UTurnManager` | 서버의 전투·턴·승패 확정. 전투 ID·턴·결과·유닛 식별/소유권의 실행 중 복제 뷰 제공 |
| `UCombatActionAuthority` | CombatManager 소유 서버 객체. 인간 연결/AI 세션 구분, 소유권·문맥·순번·실제 행동 데이터 검증 |
| `AUnitBase` / GAS / Grid | 서버만 행동·HP/AP·사망·점유 변경. Attribute RepNotify와 Actor 복제로 Client 표시 상태 전달 |
| `UPartyAutoCombatComponent` | PlayerUnit의 서버 AI. Tick 없이 턴 시작·행동 완료에서 회복약·스킬·이동·턴 종료 Command 선택 |
| `UCombatHUDWidget` | Move·Skill·회복약·End Turn·Cancel UI와 현재 조작 가능 상태 표시 |
| `UEncounterResultWidget` | Victory Continue / Defeat. 이후 보상 선택을 연결할 위치 |

실행 중 복제 뷰와 Actor 조회는 허용하되 Run/Party/Encounter/Command는 직렬화 가능한 값 데이터가 기준이다. GameInstance에 전투 Actor나 UI 동작을 집중시키지 않는다.

## 파티와 전투 규약

### 파티

- CharacterCreation은 네 슬롯 중 하나 이상 생성하면 시작한다. 빈 슬롯은 스폰하지 않으며 원래 `SlotIndex`를 Arena의 PlayerCoords에 대응한다.
- 슬롯은 이름·`ClassId`·생성 여부·현재 HP를 전달한다. 식별된 Run은 `CharacterId`와 원래 `OwnerAccountId`도 보존한다.
- 직업은 `StableHand`, `Scholar`, `Herbalist`, `Hunter`다. `UPartyDefinitionDataAsset::Professions`에서 정의를 찾는다.
- `CombatClass`가 없으면 기존 `PlayerUnitClasses`와 명시적인 `FallbackPlayerUnitClass`를 사용한다. 현재 네 직업은 공통 `BP_PlayerUnit`을 사용하는 임시 콘텐츠다.
- 수정하지 않은 이름은 직업 표시명과 슬롯 번호를 사용한다. 개별 이름 변경은 `SetSlotCharacterName`으로 반영한다.
- 첫 스폰은 직업 정의 HP 또는 클래스 기본 HP, 이후 전투는 저장한 결과 HP를 사용한다. HP 0인 멤버는 다음 전투에 스폰하지 않는다.

### 행동과 결과

`CanStartNode → BeginEncounter → PrepareArena → SpawnParty/Enemies → MarkCombatStarted → StartCombat` 순서로 시작한다. 누락 클래스·잘못된 좌표·중복 점유·빈 편성은 부분 스폰을 정리하고 Run Map에 오류를 표시한다.

이동·스킬·회복약·턴 종료는 값 Command를 서버가 검증한다. 스킬 AP 비용은 `USkillDefinitionDataAsset::ActionPointCost` 한 곳에서 표시·판정·차감하며 1 이상이어야 한다. 컨텍스트/GAS 커밋 뒤 한 번 차감한다.

타겟과 범위는 `CombatTargetingLibrary`를 공유하고 실행 직전에 다시 검사한다. 현재 범위는 Single/AroundTarget/AroundSelf이며 미지원 타입과 음수 반경은 실행 전에 거절한다.

행동 완료는 `EUnitActionResult`로 통일한다. GAS 종료 델리게이트는 활성화 전에 연결해 동기 완료도 받는다. 제자리 스킬은 복귀 이동을 하지 않으며 실패·취소는 원래 타일/위치로 복구한다. 이미 소비한 AP는 환불하지 않는다.

발사체 스킬은 몽타주와 impact 완료를 기다리며 미충돌 시간 제한·취소·사망 때 정리한다. 적 AI의 실패 처리는 다음 틱의 안전한 턴 종료로 이어진다.

`OnUnitDied → EvaluateCombatResult → StopCombat`에서 결과를 한 번 확정한다. 먼저 턴·행동·입력을 중단하고 사망 콜백 다음 틱에 HP를 추출한다. 현재 턴 유닛만 죽고 팀이 생존하면 다음 생존 유닛으로 한 번 전이한다.

Standalone은 결과 확정 후 TurnOrder·CombatUnits·ASC/이동·타이머·컨트롤러·스킬 Actor·Unit·타일 점유를 정리한다. 네트워크는 최종 HP·사망·점유 해제가 복제되도록 Result에서 유닛을 유지하고 Continue 또는 월드 종료 시 정리한다. 저장 실패 시 결과·Continue 전이를 확정하지 않는다.

### 입력

활성 CommonUI 화면이 입력 모드를 소유한다. CombatHUD는 `All / CaptureDuringMouseDown`, RunMap/Result는 `Menu / NoCapture`를 사용한다.

GameplayController에서 별도 `SetInputMode`를 추가하지 않는다. MainMenu의 UIOnly 상태에서 travel한 뒤 남는 viewport `IgnoreInput`과 로컬 포커스는 native 진입 코드가 복구한다. 이 입력 수정에는 WBP 재생성이 필요 없다.

## 저장과 멀티플레이 연결 경계

| 저장 종류 | 현재 계약 |
|---|---|
| 일반 Run v1 | Identity 없는 `LegacyOffline` 구버전 호환. 메타데이터를 추정해 승격하지 않음 |
| 일반 Run v2 | 전투 밖 파티·진행과 Identity/캐릭터 소유권 저장 |
| 일반 Run v3 | 마지막 확정 턴의 전투 체크포인트 포함. 새 Actor로 복구 |
| 관리 Run v4 | 로컬 기준 저장소에 전체 Run 본문·참여 목록 저장. CAS revision·HostEpoch·실행 lease로 쓰기/재개 제한 |
| 상대 Snapshot v1 | 별도 `USaveGame` 슬롯과 고정 ID 카탈로그. 미지원 구조 버전 거절, 콘텐츠 버전 일치 요구 |

일반 저장 기본 슬롯은 `ProjectA_Run`, 상대 Snapshot은 `ProjectA_Opponent_` 접두사로 분리한다. Identity schema 1은 번호 0을 유지하고 schema 2는 명시적인 합류 번호 1~N을 검증한다. 전투 체크포인트 schema 1은 Human, schema 2는 저장된 Human/ServerAI 모드를 지원한다.

일반 메뉴 `이어하기`는 유효한 LegacyOffline 또는 LocalDevelopment 단일 참가자 저장만 연다. 협동 저장과 관리 v4는 별도 복구 경로를 사용하며 조회 후 파일이 바뀌어도 실제 로드에서 다시 검사한다.

관리 Run은 C++에서 변경 불가능한 개발 호출자 문맥과 재개 대상을 설정해야 한다. 메뉴의 `싱글로 전환하기`/`싱글 진행 이어하기`는 최신 기준 기록으로 명시적 재개를 요청한다. 계정 입력 UI·자동 개발 계정 생성·저장 검색·실제 로그인은 제공하지 않는다.

`CreateManagedRun`은 최초 2~4명의 Human을 기록한다. `ResumeManagedRun`은 이전 Human의 부분집합과 최소 합류 번호 Host를 검사하며 HostEpoch·참여 목록·중첩 전투 Identity/모드를 원자적으로 갱신한다. 원래 캐릭터 소유권은 바뀌지 않는다.

`ConfirmManagedResumeStarted` 전에는 인간 명령·노드·Continue를 막는다. 전투 복구 실패는 현재 Host의 재시도를 기다린다. 메뉴 travel 실패는 GameInstance에서 감지해 lease를 해제하고 확정 기록·호출자·재개 대상을 유지한다.

정상 월드 종료는 전투·AI 콜백 중단 뒤 `CloseManagedRun`을 호출한다. 결과에서 전투 본문을 비워도 참여 목록은 다음 전투에 남는다. 이미 AI로 바뀐 소유자는 같은 Run에서 Human으로 돌아오지 않는다.

일반 협동 v3는 원래 참가자 전원, 관리 v4는 현재 Human 전원의 연결을 요구한다. `AssignRunParticipant`/`ApplyCombatParticipantBindings`는 신뢰된 서버 C++ 배정이며 인증 기능이 아니다. Host도 타인의 Human 캐릭터를 직접 조작하지 못한다.

관리 저장은 Win64 로컬 개발 대역이며 `AccountProvider`를 허용하지 않는다. 로컬 잠금은 온라인 접속 확인이나 중앙 서버 권위를 대신하지 않는다. Snapshot 선택 CLI·협동 실행·저장 원자성·온라인 준비의 세부 계약은 [MULTIPLAYER](MULTIPLAYER.md)를 따른다.

## Gameplay 에셋과 배치

아래 에셋 경로는 모두 `/Game/User_JeHoon/` 기준이다. 디스크에서는 `Content/User_JeHoon/`에 대응한다. 기존 에셋에는 필수 수동 재연결 작업이 없다.

| 에셋 경로 | 클래스 / 저장된 연결 |
|---|---|
| `LEVEL/MainMenu` | 기본 시작 맵 |
| `LEVEL/Gameplay` | TestMap geometry·NavMesh·Grid를 복제한 기준 레벨, `BP_GameplayGameMode` Override |
| `Blueprint/Game/BP_GameplayGameMode` | `AGameplayGameModeBase`, PartyDefinition과 `EncounterDefinitions[DefaultEncounter]` 설정 |
| `Blueprint/Controller/BP_GameplayPlayerController` | `AGameplayPlayerController`, GameplayRootWidgetClass 설정 |
| `Blueprint/DataAsset/DA_VerticalSliceParty` | `UPartyDefinitionDataAsset`, 네 직업과 공통 `BP_PlayerUnit` fallback |
| `Blueprint/DataAsset/DA_DefaultEncounter` | `UEncounterDefinitionDataAsset`, `EnemyUnitClasses[0]=BP_EnemyUnit` |
| `UI/Gameplay/WBP_GameplayRootWidget` | `UGameplayRootWidget`, RunMap/CombatHUD/Result 화면 클래스 |
| `UI/Gameplay/WBP_RunMapWidget` | `URunMapWidget` |
| `UI/Gameplay/WBP_CombatHUDWidget` | `UCombatHUDWidget`; 기존 `UI/Combat/WBP_CombatHUDWidget`과 별도 |
| `UI/Gameplay/WBP_EncounterResultWidget` | `UEncounterResultWidget` |

클래스는 런타임 Blueprint 문자열 경로 Load 대신 DataAsset과 Blueprint 기본값 참조로 연결한다.

| Gameplay 배치 대상 | 값 |
|---|---|
| `GameplayCombatArena` | `ACombatArena`, Grid는 배치된 `BP_CombatGridManager`, CameraAnchor는 `GameplayCamera` |
| Grid | TileClass=`BP_CombatGridTile`, Rows/Cols=`4`, Location Z=`5` |
| Arena PlayerCoords | 슬롯 0~3 → `(0,1), (1,1), (2,1), (3,1)` |
| Arena EnemyCoords | `(0,2), (1,2), (2,2), (3,2)`; 기본 Encounter는 첫 좌표 사용 |
| `GameplayCamera` | `ACameraActor`, 위치 `(-300,-1000,1500)`, Pitch `-46.97`, Yaw `90`, FOV `55` |
| 여러 Arena 배치 시 | 사용할 Arena의 Actor Tags에 `GameplayArena` 지정 |

### 설정 변경 또는 연결 복구 순서

1. `DA_VerticalSliceParty`의 Professions에서 직업별 CombatClass를 설정한다. 기존 PlayerUnitClasses 매핑과 FallbackPlayerUnitClass도 확인하고 Save한다.
2. `DA_DefaultEncounter`의 EnemyUnitClasses에 `AEnemyUnit` 자식 클래스를 지정한다.
3. `BP_GameplayGameMode` Class Defaults에서 PartyDefinition, EncounterDefinitions의 `DefaultEncounter`, CombatManagerClass=`ACombatManager`, EncounterManagerClass=`AEncounterManager`를 확인한다.
4. 같은 GameMode의 PlayerControllerClass는 `BP_GameplayPlayerController`, DefaultPawnClass/HUDClass는 None으로 설정하고 Compile → Save한다.
5. `BP_GameplayPlayerController`의 GameplayRootWidgetClass와 Root WBP의 RunMapWidgetClass/CombatHUDWidgetClass/ResultWidgetClass를 위 표대로 연결하고 Compile → Save한다.
6. Gameplay의 World Settings에서 GameMode Override를 지정하고 Arena의 Grid/CameraAnchor/좌표를 위 표와 맞춘다.
7. Grid의 TileClass·크기·Z를 확인한다. P 키로 NavMesh가 바닥/스폰 위치를 덮는지 보고 필요할 때 Build → Build Paths 후 Save All한다.
8. `BP_MainMenuPlayerController`의 GameplayLevelName을 `/Game/User_JeHoon/LEVEL/Gameplay`로 지정한다. 제거된 옛 `StartGameLevelName=WorldMap` 필드는 실행에 사용하지 않는다.

### Designer 바인딩

| 화면 | 이름과 형식 |
|---|---|
| GameplayRoot | `RootOverlay`, `RunLayer`, `CombatLayer`, `ModalLayer`; 세 레이어는 `CommonActivatableWidgetStack` |
| RunMap | `Text_Progress`, `Text_Party`, `Text_FlowMessage`, `NodeList`(`VerticalBox`); 노드 버튼은 런타임 생성 |
| CombatHUD | `CommandPanelBackground`(`Border`), `Text_Turn`, `Text_Action`, `SkillList`(`HorizontalBox`), `Button_Move`, `Button_EndTurn`, `Button_Cancel` |
| Result | `Text_Result`, `Button_Continue` |

스킬 버튼은 장착 정의에서 생성한다. native fallback은 같은 `BindWidgetOptional` 이름을 사용한다. Designer를 수정한 WBP를 덮어쓰기 전에 변경 내용을 확인한다. JSON spec 변경은 실제 생성·Compile·Save를 거쳐 반영하며 DryRun만으로 완료를 기록하지 않는다.

## 에셋 도구와 CLI

도구는 Development Editor / Win64 빌드 결과를 사용한다. 아래 명령은 필요할 때 사용자가 실행할 참조다. Python은 해당 프로세스의 `-EnablePlugins=PythonScriptPlugin`으로만 활성화한다.

| 경로 | 용도 |
|---|---|
| `Source/ProjectAEditor/Scripts/AuditGameplayAssets.py` | AssetRegistry와 실제 Blueprint/맵 참조 읽기, `Saved/Automation/GameplayAssetAudit.json` 기록 |
| `Source/ProjectAEditor/Scripts/ConfigureGameplayAssets.py` | 최초 Gameplay/Blueprint/DataAsset 생성과 연결. 대상이 존재하면 중단하므로 현재 저장소에서 재실행하지 않음 |
| `Source/ProjectAEditor/Scripts/ValidateGameplayAssets.py` | 부모·클래스·위젯·스폰·카메라 참조 검사, `Saved/Automation/GameplayAssetValidation.json` 기록 |
| `Source/ProjectAEditor/UiScaffoldSpecs/` | `GameplayRootWidget.json`, `RunMapWidget.json`, `CombatHUDWidget.json`, `EncounterResultWidget.json`을 기존 GenerateUiScaffold로 관리 |

```powershell
$editorExecutable = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$projectFile = 'C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject'
& $editorExecutable $projectFile -run=PythonScript -EnablePlugins=PythonScriptPlugin -script='Source/ProjectAEditor/Scripts/AuditGameplayAssets.py' -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=GenerateUiScaffold '-Spec=Source/ProjectAEditor/UiScaffoldSpecs/GameplayRootWidget.json' -unattended -nop4 -NullRHI
& $editorExecutable $projectFile -run=PythonScript -EnablePlugins=PythonScriptPlugin -script='Source/ProjectAEditor/Scripts/ValidateGameplayAssets.py' -unattended -nop4 -NullRHI
```

초기 생성 도구의 실행 옵션은 위 Python 명령에서 script를 `ConfigureGameplayAssets.py`로 바꾼다. 생성 대상이 없는 신규 설정에만 사용한다. 복제 직후에는 Bounds/Recast Actor만으로 이동 경로가 생기지 않으므로 Gameplay 하나의 내비게이션 데이터를 빌드·저장해야 한다.

```powershell
& $editorExecutable $projectFile -run=ResavePackages -BuildNavigationData -Package=/Game/User_JeHoon/LEVEL/Gameplay -ProjectOnly -unattended -nop4 -NullRHI
```

초기 설정 순서는 UI 생성 → Gameplay 최초 생성 → Navigation Build → 저장된 연결 검사다. 실제 이동·입력·복구 작동은 별도 확인한다. 사용자 실행 절차와 확인 항목은 [TEST_REPORT](TEST_REPORT.md), 과거 실행 결과는 HISTORY에서 관리한다.

## 현재 한계와 보존 대상

- 기본 콘텐츠는 두 노드와 공통 PlayerUnit을 사용한다. 네 직업의 고유 스킬/스탯 완성, 전투 사이 회복·부활·보상, 전체 인벤토리/장비와 여러 Act는 미구현이다.
- 기존 4×4 Grid, GAS 피해·몽타주·스킬 Actor, 적 AI, AP/보조 AP를 재사용한다. Streaming/Level Instance는 현재 흐름에 없다.
- `TestMap` 원본과 기존 MainMenu를 유지한다. `WorldMap` 레벨/native class는 deprecated 상태로 보존하며 실행 흐름에 사용하지 않는다.
- WorldMap의 WorldSettings가 `WorldMapGameModeBase`를 참조하므로 클래스를 임의로 삭제하지 않는다. 삭제/이름 변경이 없어 CoreRedirect도 추가하지 않았다.
- 로컬 Snapshot·Listen Server·개발용 관리 저장의 구현을 실제 계정 인증, Steam 연결, PlayFab 운영, 경쟁 결과 검증이나 MMR 완료로 기록하지 않는다.
- 빌드·자동화 결과와 사용자의 실제 조작 검증을 구분한다. 다음 구현 우선순위와 T14 잔여 조건은 [TODO](TODO.md), 최종 작동 확인은 [TEST_REPORT](TEST_REPORT.md)를 따른다.
