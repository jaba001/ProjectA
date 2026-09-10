# ProjectA 기획과 구현 현황

게임의 목표와 이후 기능 판단 기준은 [게임 기획 방향](GAME_DESIGN.md)을 따른다. 이 문서는 현재 Vertical Slice의 구현 규약과 검증 경계를 기록하며, 목표 기능이나 후속 제안을 구현 완료로 취급하지 않는다.

기준일: 2026-09-10. 기본 싱글플레이와 T14 순차 1~5번, 7번 독립 범위인 2·3·4인 전투·끊김·기존 Host 복구를 검증했다. 후속 답변으로 최초 합류 번호순 승계, 사전 동의 없는 Host 단독 AI 전환, 해당 Run의 AI 유지, Steam 우선과 Host의 노드·Continue 결정권을 확정했다. 6번은 관리 v4 저장·영속 Human 목록·싱글 전환 메뉴·새 Host 복구를 로컬 개발 저장소에 연결하고 실제 관리 PIE 2건·전체 회귀 75건·별도 기존 AI 복구 3건·Snapshot 전투 1건을 통과했다. 6번은 로컬 개발 범위 완료이고 7번의 원래 4인→현재 3인 승계를 추가 검증한다. 8번의 실제 Steam/PlayFab 서비스·MMR은 미구현이다. 기존 slice 결과는 [작업 보고](VERTICAL_SLICE_REPORT.md), T14 번호별 검증은 [순차 대기열](T14_QUEUE.md)에 기록한다.

## 1. 확정한 게임 흐름

아래는 기본 싱글플레이 흐름이다. 네트워크 전투는 결과 화면에서 최종 유닛 상태를 유지하고 명시적인 Continue 또는 월드 종료에서 정리한다.

```mermaid
flowchart LR
    A[MainMenu] --> B[CharacterCreation 1~4명]
    B -->|OpenLevel 1회| C[Gameplay persistent level]
    C --> D[Run Map UI]
    D --> E[Encounter 준비]
    E --> F[Grid Combat]
    F -->|Victory| G[정리 + Result]
    G -->|Continue| D
    F -->|Defeat| H[정리 + 패배 화면]
```

월드 진행은 CommonUI 노드 화면으로 구현한다. `WorldMap` 물리 탐험과 전투별 CombatMap 전환 계획은 폐기한다. 두 개의 순차 Combat 노드가 `DefaultEncounter` 정의를 재사용하며 두 번째 Victory 이후 Run Map에 완료 상태를 표시한다. 패배 후에는 입력과 턴이 잠긴 결과 화면을 유지한다.

관리 Run의 메뉴 진입은 별도다. 신뢰하는 C++ 개발 경로가 본인 호출자 문맥과 재개 대상을 설정하면 `싱글로 전환하기` 또는 `싱글 진행 이어하기`를 표시한다. 최신 기준 기록을 읽고 본인 한 명을 Human으로 남기는 새 lease·Host 세대를 원자적으로 획득한 뒤 Gameplay로 이동한다. 복구가 성공할 때까지 Human 명령·노드·Continue를 막는다. 일반 `이어하기`는 관리 v4를 읽지 않는다. 기본 메뉴에 계정 입력이나 개발 계정 자동 생성·저장 검색을 추가하지 않는다.

## 2. 책임과 수명

| 구성 | 역할 / 수명 |
|---|---|
| `URunStateSubsystem` | GameInstance 수명. 파티·노드·결과·HP와 Run/참가자/캐릭터 소유권/Host 보존. 일반 v1~3 저장과 관리 v4의 Human 목록·실행 lease·재개 대기를 분리하며 Actor 참조 없이 기록 |
| `AGameplayGameModeBase` | 서버에서 Arena·EncounterManager·CombatManager 준비, 신뢰된 C++ 참가자 배정. 일반 v3는 원래 참가자 전원, 관리 v4는 현재 Human 전원의 연결 요구. 정상 종료 시 전투 중단 후 관리 lease 해제 |
| `AGameplayGameState` | Run 단계·파티·노드·결과의 읽기 전용 표시 뷰와 전투/아레나 참조 복제. 클라이언트 RunState를 권위 상태로 사용하지 않음 |
| `AGameplayPlayerController` | PartyPlayerController 상속. Root UI와 서버 전투 문맥 수신, 소유 연결의 전투 RPC. 일반 Standalone 또는 신뢰 배정된 현재 Listen Host만 노드/Continue 실행. 관리 Run은 Standalone도 lease·Host·복구 완료 확인 |
| `UGameplayRootWidget` | CommonUI Run / Combat / Modal 스택 관리 |
| `URunMapWidget` | 노드 정의 표시, 선택 요청. Spawn 수행 안 함 |
| `AEncounterManager` | 준비/스폰/전투 연결/HP 추출/정리. RunState의 유효 전이를 요청. 관리 v4의 영속 소유자별 모드를 새 전투/복구에 적용하고 복구 성공 후 재개 대기 해제 |
| `ACombatArena` | Grid origin·슬롯별 좌표·카메라·타일 활성화. 향후 다른 Arena 구현으로 교체 가능 |
| `ACombatManager` / `UTurnManager` | 서버만 턴·승패 확정. CombatManager가 전투 ID·턴·결과·유닛 식별/원래 소유권의 복제 뷰 전달 |
| `UCombatActionAuthority` | CombatManager 소유 서버 객체. 인간 연결과 별도 AI 세션·공통 행동 검증. 관리 실행 여부를 Reset 후에도 유지하며 lease·전체 Identity·Host 호출자·SessionId와 영속 모드를 확인. AI 캐릭터와 복구 대기에는 인간 명령 거절 |
| `UPartyAutoCombatComponent` | PlayerUnit의 서버 전용 판단·행동 대기. 매 프레임 Tick 없이 턴 시작과 완료 통지에서 회복약·스킬·이동·턴 종료 Command를 선택 |
| `UCombatHUDWidget` | CommonActivatableWidget, 기존 Move/Skill/End Turn 명령 연결 |
| `UEncounterResultWidget` | Victory Continue / Defeat. 향후 보상 선택을 넣을 위치 |

Streaming, Level Instance, 인벤토리와 장비, 여러 Act, 즉시 전체 Replication 리팩터링은 현재 Vertical Slice 범위 밖이다. T14 순차 4번은 다음 턴 시작 전 경계를 v3 값 데이터로 저장하고 새 Actor로 복구한다. 6번 관리 v4는 같은 확정 경계와 전투 밖 Human 목록을 별도 로컬 기준 저장소에 보존한다. GameInstance에 전투/UI를 몰아넣지 않는다.

D01 / T14: ProjectA는 최종적으로 Async PvP와 실시간 Co-op을 지원한다. 첫 Vertical Slice와 기본 Run은 싱글플레이를 유지하며, 네트워크 기획 확정과 구현 완료를 구분한다. 상세 범위·미결정 항목·완료 조건은 [T14 작업 카드](TODO.md)를 기준으로 한다.

Async PvP는 서버에 저장된 상대 Party/Build Snapshot으로 Encounter를 구성하고 기존 PvE Unit/Combat 흐름을 재사용한다. Snapshot은 파티 구성·Class·Stats·Skills·Equipment·Formation·데이터 버전을 표현해야 한다. 초기 로컬 Snapshot 전투와 경쟁 콘텐츠의 서버 결과 검증은 별도 단계다.

T14의 첫 구현은 사용자 선택 1A+2B에 따라 로컬 Snapshot 전투와 Unreal `USaveGame` v1을 사용한다. 파티 값은 Actor 참조 없는 USTRUCT로 저장하고, 고정 ID를 DataAsset 카탈로그로 해석해 기존 적 AI/Combat을 실행한다. 저장 구조 버전은 1만 허용하고 콘텐츠 버전은 카탈로그와 일치해야 한다. 로컬 전투 한 사이클 검증 이후 Listen Server Co-op 동기화를 진행한다. 실행 방법과 현재 계약은 [T14 Snapshot 안내](T14_SNAPSHOT.md)를 따른다.

상대 Snapshot은 AI가 조작하며 아군은 직접 Grid 전투를 조작한다. 향후 플레이어가 설정한 Tactics를 Snapshot에 포함할 수 있는 구조를 고려하되 전술 편집 기능은 후속 기획으로 둔다.

Co-op은 Listen Server의 Host-authoritative 구조를 우선한다. 최대 4인으로 설계하고 첫 동기화는 2인으로 검증한다. 원래 캐릭터 소유자만 직접 조작하고 다른 사람이 같은 Run에 대체 참가하지 못한다. Host가 바뀌어도 타인 캐릭터의 인간 조작권은 얻지 않는다. Client의 Action Request는 서버가 검증·실행하며 CombatManager·TurnManager·Grid Occupancy·Unit State·HP/AP·사망·Combat Result의 최종 권위는 서버에 있다.

순차 2번에서 이동·스킬·회복약·턴 종료를 값 Command와 PlayerController의 소유 연결 RPC 진입점으로 통합했다. 순차 3번은 실제 두 PIE 월드 사이의 RPC와 소유권 검증, Turn·HP/AP/SubAP·이동·Grid 점유/전열 보호·사망·결과·장착/HUD 복제를 구현하고 실제 2인 PIE에서 검증했다. 서버만 유닛 행동과 GAS 능력을 실행하며 HP/MaxHP는 Attribute RepNotify로 전달한다. GameState의 Run 표시 뷰와 CombatManager의 실행 중 복제 뷰를 영속 Run/Command 데이터와 분리한다.

`AssignRunParticipant`와 `ApplyCombatParticipantBindings`는 신뢰된 서버 C++ 연결 배정이며 실제 로그인은 아니다. 네트워크 자동화는 알려진 연결에 원래 참가자 계정을 명시적으로 배정하고, 일반 자동 연결은 Standalone의 개발용 단일 참가자를 유지한다. 관리 v4는 원래 인원이 여러 명이어도 현재 Human이 본인 Host 한 명이면 유효 lease로 Standalone 복구·바인딩을 허용한다. Listen 복구는 현재 Human만 연결하고 영구 AI 소유자의 Human 배정을 거절한다. 노드 선택·Continue는 5A에 따라 현재 Host만 실행하며 버튼과 요청에서 로컬 서버 연결·신뢰 배정·현재 Host 계정을 함께 검사한다. Client는 공유 진행을 표시한다. [Listen Server 안내](T14_NETWORK.md), [확정 턴 저장·복구](T14_CHECKPOINT.md), [명시적 재개 안내](T14_RESUME.md)에 구현 경계를 기록하고 번호별 검증은 [대기열](T14_QUEUE.md)을 따른다.

5번은 `APlayerUnit`에 Human/ServerAI 조작 모드와 `UPartyAutoCombatComponent`를 연결했다. 신뢰된 서버 C++는 전투 시작 전에 원래 소유권을 확인해 모드를 설정한다. 후속 확정으로 개인 사전 동의 검사는 제거했다. AI는 별도 실행 세션·요청 순번으로 공통 행동 검증을 이용하며 인간 입력과 중복 실행되지 않는다. Run 저장 v3의 전투 본문 schema 2에 모드를 저장하고 schema 1은 Human으로 호환한다. 복원 시 AI 실행 세션은 새로 만들며 소유권·Host·팀을 바꾸지 않는다. 최초 구현의 전체 56건·실제 AI Co-op·독립 프로세스 복구와 이후 검증은 [대기열](T14_QUEUE.md)을 따른다. 이 일반 v3 경로는 원래 참가자 전원의 연결을 유지한다. 6번의 관리 v4만 영속 Human 목록에 따른 불참 AI 예외를 적용한다.

관리 v4의 `CreateManagedRun`은 번호가 명시된 원래 개발 참가자 2~4명과 최초 Host·Human 전체 목록을 저장한다. `ResumeManagedRun`은 기존 실행이 닫힌 뒤 최신 stamp와 이전 Human 목록의 부분집합을 검사하고, 남을 인간 중 가장 작은 최초 번호를 새 Host로 정한다. 새 HostEpoch·참여 목록·CombatCheckpoint Identity/모드를 함께 획득하고 성공 후에만 메모리에 적용한다. 원래 참가자·번호·캐릭터 ID·소유자·AttemptId·확정 턴은 보존한다. 확정 턴·결과·Continue 저장도 관리 lease를 요구하며 쓰기 실패 시 일반 SaveSlot으로 우회하지 않는다.

Gameplay는 복구 성공 후 `ConfirmManagedResumeStarted`로 대기를 해제한다. 복구 실패는 lease·대기와 오류 안내를 유지하며 현재 로컬 Host만 재시도한다. 결과에서 전투 본문을 비운 뒤에도 Human 목록은 남아 다음 전투까지 AI를 유지한다. 정상 월드 종료는 Encounter/AI/능력 콜백을 중단한 뒤 `CloseManagedRun`으로 lease와 활성 상태를 비우고, 호출자 문맥과 재개 대상은 GameInstance에 남긴다. 옛 전투 Authority도 관리 여부와 실행 SessionId를 기억해 직접 Close 후 명령이 일반 경로로 우회하지 못하게 한다.

정상 종료·갑작스러운 끊김만으로 Host를 자동 변경하지 않는다. 4번은 연결 끊김 시 전투를 중단하고 기존 Host와 원래 참가자가 새 세션에서 마지막 확정 턴을 복구하도록 한다. 6번의 명시적 재개는 참여하는 인간 중 최초 번호가 가장 작은 사람이 Host가 되고 그 Host가 사전 동의 없이 AI 전환을 확정한다. 혼자 싱글로 전환하면 본인이 Host가 되고 나머지는 해당 Run 종료까지 AI를 유지한다. 이후 MMR은 현재 인간 참가자에게만 반영하고 불참자에게 추가 변동을 주지 않는 기획이며 실제 판정·계산·연동은 후속 범위다.

MMR 랭크를 목표로 하므로 원래 참가자·캐릭터 소유자·현재 Host를 구분하고 동일 Run의 동시 승계·진행 분기·결과 중복 반영을 방지해야 한다. Steam P2P와 Unreal Listen Server를 우선하며 선택한 Steam+PlayFab 조합에는 운영비를 피하려는 사용자 조건이 있다. 무료 개발 한도와 중앙 저장·결과 검증의 비용을 구분하고 유료 리소스를 임의로 활성화하지 않는다. 고의 이탈 방지·MMR 세부 정책은 아직 결정하지 않았다. [Co-op 기획](T14_COOP_DESIGN.md)과 [온라인 안내](T14_ONLINE.md)를 따른다.

새 Run/Party/Encounter/Combat 기능은 직렬화 가능한 Runtime Data와 Command를 우선하고 Actor reference 및 로컬 PlayerController에 강하게 결합하지 않는다. 실행 중 복제 뷰의 유닛 조회에만 Actor 참조를 사용한다. 미결정 정책이 구현에 영향을 주면 사용자에게 선택지와 영향을 설명해 결정한다. 기본 Run은 싱글플레이를 유지한다. 관리 v4는 변경 불가능한 개발 호출자를 C++ fixture가 주입하는 로컬 실행이며 `AccountProvider`를 승격하거나 실제 로그인을 제공하지 않는다. 7번 독립 3·4인 검증은 마쳤으나 관리 승계 PIE의 최종 결과와 8번 실제 서비스 연동 전 T14 전체를 완료로 표시하지 않는다.

## 3. 파티 규칙

- CharacterCreation은 기존 네 슬롯을 사용하며 1명 이상 생성하면 시작한다. 반드시 4명 규칙은 없다.
- 각 슬롯은 `SlotIndex`, `CharacterName`, `ClassId`, `bCreated`, `CurrentHP`를 전달한다. 빈 슬롯은 스폰하지 않고 슬롯 번호에 해당하는 PlayerCoords를 사용한다.
- 생성 UI에서 Edit로 슬롯 이름·직업을 편집한다. 수정하지 않은 슬롯은 직업 표시명과 슬롯 번호로 기본 이름을 갖는다. `SetSlotCharacterName`으로 개별 이름을 지정할 수 있다.
- `StableHand`, `Scholar`, `Herbalist`, `Hunter` → `UPartyDefinitionDataAsset::Professions`에서 직업 설정을 찾는다. CombatClass 미지정 시 기존 PlayerUnitClasses와 명시적인 FallbackPlayerUnitClass를 사용한다.
- 네 직업 콘텐츠가 아직 없으므로 첫 데이터 에셋은 기존 `BP_PlayerUnit`을 공통 임시 클래스로 사용한다. 직업별 스킬/스탯 완성으로 취급하지 않는다.
- 첫 스폰은 직업 정의 HP(기본은 클래스 HP), 이후 스폰은 이전 결과 HP를 복원한다. HP 0인 파티 멤버는 다음 전투에서 스폰하지 않는다. 부활/회복 보상은 미구현이다.

## 4. Encounter와 전투 규약

Gameplay의 입력 모드는 활성 CommonUI 화면이 소유한다. CombatHUD는 `All / CaptureDuringMouseDown`으로 버튼과 타일 클릭을 함께 허용하고 커서를 유지한다. RunMap과 Result는 `Menu / NoCapture`로 월드 입력을 차단한다. GameplayController는 별도로 `SetInputMode`를 호출하지 않으며, MainMenu의 기존 UIOnly 설정에서 travel 후 남는 viewport `IgnoreInput`을 해제하고 해당 로컬 플레이어의 최초 뷰포트 포커스를 복원한다.

`CanStartNode → BeginEncounter → PrepareArena → SpawnParty/Enemies → MarkCombatStarted → StartCombat` 순서다. 누락 클래스/잘못된 좌표/중복 점유/빈 편성은 부분 스폰을 정리하고 Run Map 오류 메시지로 돌아간다.

`OnUnitDied → EvaluateCombatResult → StopCombat`에서 결과를 1회 확정한다. 먼저 모든 유닛의 활성 턴을 끄고 행동을 취소하며 추가 입력을 막는다. 사망 콜백이 반환된 다음 틱에 HP를 저장한다. Standalone은 TurnOrder·CombatUnits·ASC/AI 이동·타이머·컨트롤러·스킬 액터·Unit·타일 점유를 정리한 뒤 결과를 표시한다. 네트워크는 최종 HP·사망·점유 해제가 복제되도록 결과 화면에서 유닛을 유지하고 명시적인 Continue에서 정리한 뒤 다음 노드를 연다.

전체 팀이 전멸하지 않은 상태에서 현재 턴 유닛이 죽으면 다음 틱에 다음 생존 유닛으로 진행한다. 같은 사망으로 중복 전이하지 않는다.

R01/R02는 `EUnitActionResult`와 공통 행동 완료 경로로 수정한다. GAS 종료 델리게이트는 활성화 전에 연결하여 동기 완료도 받는다. 제자리 스킬은 복귀를 요구하지 않는다. 실패/취소는 원래 타일과 위치로 복구하며 AI는 다음 틱에 안전한 턴 종료로 전이한다. 이미 소비한 AP는 환불하지 않는다.

## 5. 기존 기반과 남은 P2

기존 4×4 grid, 타일 선택/이동·타겟 표시, GAS 피해/몽타주/스킬 액터, 적 스킬 판단, AP·보조 AP 규칙을 재사용한다. `TestMap`은 원본을 보존하고 그 geometry/NavMesh/Grid를 복제한 `Gameplay`를 기준 레벨로 사용한다.

| 항목 | 현재 경계 |
|---|---|
| R03 / T03 AP 비용 | DataAsset 비용으로 표시·판정·차감 통합 완료. 비용 1 이상, 컨텍스트 검증/GAS 커밋 후 차감. 2026-09-10 빌드 및 자동화 11건 통과 |
| R04 / T04 타겟 | 공통 진영·생존·전열 보호 검사와 실행 직전 재검증 완료. 양 진영·6개 규칙 및 AI/실행/저장 맵 PIE를 포함한 전체 자동화 14건 통과 |
| R05 / T05 입력 | PlayerController로 타일 명령 집중, 요청 유닛을 확인하는 AI 내부 종료 분리, 턴 전환/종료 시 선택 정리 완료. 정식 빌드 및 입력·AI·저장 맵 PIE 포함 전체 자동화 16건 통과 |
| R06 / T06 범위 | Single/AroundTarget/AroundSelf 공통 계산, 기존 시전자 제외 유지. 미지원 타입/음수 반경은 에셋 검증·실행 전 거절. 전체 자동화 18건 통과 |
| T07 발사체 완료 | GAS가 몽타주+impact 완료를 기다림. 미충돌 시간 제한·취소/사망 정리 및 플레이어 AP·보조 AP 모두 소진 시 자동 종료 완료. 빌드 및 전체 자동화 20건 통과 |
| T09 직업/편집 | 공통 직업 정의로 UI/스폰 연결, 이름·직업 편집과 읽기 전용 ClassInfo 구현. 기존 클래스 밸런스 유지, 직업별 신규 콘텐츠는 별도 |
| T11 메뉴 기능 | 체크포인트 자동 저장/이어하기, 품질·수직 동기화 옵션, 실제 Quit 연결. T11 당시 v1 빌드·자동화 23건 및 패키지 저장/Continue/Quit 검증 완료. T14에서 새 Run은 v2 소유권 메타데이터, 기존 v1은 LegacyOffline 호환 |
| T12 메뉴 에셋 | Designer 기준 유지, 표시 전용 프리뷰·재진입 정리, 누락 상태 안내 바인딩 보완. 빌드·전체 24건 및 최종 PIE·생성본 검증 완료 |
| T13 콘텐츠 | 전투 한정 회복약·HUD, SkillPool 휩쓸기 획득/GAS 장착, 적 전진 후보와 AP당 대상 점수 구현. 빌드·자동화 26건 및 저장 맵 PIE 통과 |
| T14 네트워크 | 로컬 Snapshot과 순차 1~5번, 7번 독립 3·4인 전투/기존 Host 복구 당시 전체 65건·별도 2·3·4인 AI 복구 3건 통과. 일반 v3는 신뢰된 C++ 배정·원래 참가자 전원 연결 유지. 관리 v4의 현재 Human 연결·3→2인 승계/4번 싱글 전환은 별도 실제 PIE 2건·전체 회귀 75건 통과, 8번 실제 서비스 미구현 |

## 6. 검증과 다음 단계

검증 ID는 VERTICAL-01~10을 사용한다. 빌드 성공만으로 PIE 통과로 표시하지 않으며, 강제 GAS 피해를 이용한 종료 검증과 실제 스킬/이동/AI 검증도 구분한다. [설정 안내](VERTICAL_SLICE_SETUP.md), [리뷰](CODE_REVIEW.md), [작업 보드](TODO.md), [최종 보고](VERTICAL_SLICE_REPORT.md)를 함께 확인한다.

관리 통합의 네이티브 `ProjectA.Run.Managed`는 생성·번호순 재개·2/3/4번 단독 전환·실패 원자성·영구 AI·일반 저장 호환을 검사한다. 실제 `ProjectA.ManagedRunPIE.HostSuccession`은 원래 3명에서 2번 Host·3번 Client·1번 AI로 재개하고, `SoloMenuConversion`은 원래 4번의 메뉴 버튼을 통해 Standalone으로 복구한다. 실제 세션에는 `-T14ManagedRunPIE`가 필요하며 인자 없는 안내 종료를 실제 PIE 통과로 기록하지 않는다. 전투/HUD·AI 행동·새 저장·결과 Continue·다음 전투 및 종료된 lease의 옛 명령 거절이 검증 대상이다. 전체 75건·실제 관리 PIE 2건 통과와 최종 보고서는 [재개 안내](T14_RESUME.md)와 [대기열](T14_QUEUE.md)에 별도로 기록한다.

다음 콘텐츠 작업은 공통 `BP_PlayerUnit` fallback을 실제 네 직업 정의로 교체하는 것이다.
