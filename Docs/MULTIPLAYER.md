# ProjectA 멀티플레이 계약과 개발 안내

[프로젝트 안내](../README.md) · [과거 구현·검증 이력](HISTORY.md) · [사용자 테스트와 결과](TEST_REPORT.md)

## 현재 상태

- 기본 Run은 싱글플레이이며 Async PvP의 로컬 상대 Snapshot과 Listen Server Co-op 전투를 지원한다.
- T14 1~6번의 로컬 개발 범위는 구현·검증을 마쳤다. 6번은 관리 v4, 3→2인 Host 승계와 4번 참가자의 싱글 전환을 포함한다.
- 7번의 기존 2·3·4인 전투/기존 Host 복구 이력과 현재 추가한 승계 확장을 구분한다. 최신 변경은 컴파일을 확인했으며 최종 사용자 작동 검증 대기다.
- 8번은 Steam/PlayFab 개발 환경·접근 권한이 필요하다. 실제 로그인, 인터넷 P2P, 공동 저장, 결과 검증과 MMR은 미구현이다.
- 과거 실행 횟수·실패 수정·로그는 HISTORY에서 관리한다. 앞으로 작동 테스트는 사용자가 담당하며 Codex가 UE·PIE·자동화를 임의 실행하지 않는다.

## 확정 정책

| 항목 | 규칙 |
|---|---|
| 인원 | 최대 4명. 기본 `GameSession.MaxPlayers=4`는 Listen Host를 포함한다 |
| 인간 조작 | 각 플레이어는 원래 자기 캐릭터만 조작한다. Host도 타인 캐릭터를 직접 조작하지 못한다 |
| 소유권 | Host·연결·AI 모드가 바뀌어도 `CharacterId`와 원래 소유자는 유지한다 |
| 참가자 | 원래 Run 참가자 외 대체 참가자는 허용하지 않는다 |
| 최초 번호 | 최초 Host는 1번, 최초 합류 순서대로 2·3·4번. 재접속 순서나 파티 슬롯으로 다시 부여하지 않는다 |
| 중단 | 정상 종료·연결 끊김만으로 Host나 AI 모드를 바꾸지 않는다 |
| 명시적 재개 | 이번에 Human으로 재개할 원래 참가자 중 최초 번호가 가장 작은 사람이 Host가 된다 |
| 단독 전환 | 2·3·4번이 혼자 전환하면 본인이 Host, 나머지 원래 캐릭터는 AI가 된다 |
| AI 결정 | 재개 Host가 단독 확정한다. 개인별 사전 동의는 요구하지 않는다 |
| 영구 AI | 해당 Run 종료까지 AI를 유지한다(3C). 이후 Human 목록이나 Host 후보에 다시 넣지 않는다 |
| 진행 결정 | 현재 Host만 노드 선택·승리 Continue를 실행한다(5A) |
| 복구 지점 | 마지막으로 저장까지 완료된 확정 턴 경계 |
| 경쟁 목표 | 현재 Human 참여자에게만 MMR을 반영하고 불참자에게 추가 변동을 적용하지 않는다. 실제 계산·연동은 후속이다 |

명시적 재개는 확정 데이터로 새 서버를 여는 기능이다. 실행 중인 Listen Server를 즉시 옮기는 자동 Host Migration은 아니다.
기존 실행이 살아 있으면 다른 참가자가 같은 기록을 획득할 수 없다. 로컬 파일 복사나 계정 입력을 승계 승인으로 사용하지 않는다.
한 사람이 가질 캐릭터 수, 관전·화면 분할, AI 캐릭터의 비전투 장비·보상 결정권은 미정이다.

## 식별자와 저장 버전

서로 다른 목적의 ID와 버전을 섞지 않는다. 영속 DTO는 Actor·Controller·Ability 인스턴스나 실행 핸들을 저장하지 않는다.

| 식별자 | 수명과 의미 |
|---|---|
| `RunId` | 같은 Run 전체를 식별하며 재개 후에도 유지 |
| `FRunAccountId` | 공급자와 불투명 Subject. 표시명·PIE PlayerId·접속 순서가 아니다 |
| `JoinOrdinal` | 최초 합류 번호. 승계 후보 순서를 결정 |
| `CharacterId` / `OwnerAccountId` | 원래 캐릭터와 소유자. AI·Host 변경으로 이전하지 않음 |
| `HostAccountId` / `HostEpoch` | 현재 Host와 실행 세대. 명시적 관리 재개 시 epoch 증가 |
| `AttemptId` | 확정 전투 시도. 같은 전투 복구·승계에서 유지 |
| 전투 체크포인트 `Revision` | 해당 Attempt의 확정 턴 저장 순번 |
| 저장소 stamp | RunId·저장소 revision·HostEpoch·SessionId. 저장소 revision은 지도/결과/재개를 포함한 모든 저장 순번 |
| Runtime ID·nonce | 전투/유닛 실행 ID, 연결 바인딩 ID, AI 조작 세션. 복구 시 새로 생성 |

| 저장 계층 | 지원과 호환 |
|---|---|
| Opponent Snapshot | schema 1, 카탈로그와 일치하는 ContentVersion. Run 저장과 별개 |
| Run SaveGame v1 | 식별 정보 없는 `LegacyOffline`. 소유자·Host를 추정 이관하지 않음 |
| Run SaveGame v2 | 일반 Run의 전투 밖 진행과 식별/소유권 |
| Run SaveGame v3 | 일반 Run의 확정 전투 본문. 협동 복구는 기존 Host·원래 참가자 전원 연결 |
| Run SaveGame v4 | 관리 Run의 전 단계·영속 Human 목록과 확정 전투. LocalDevelopment 원래 참가자 2~4명만 허용 |
| Identity schema 1 | 최초 번호 0을 유지하며 번호순 승계를 거절 |
| Identity schema 2 | 1~참가자 수의 고유하고 연속된 최초 번호. 최초 epoch 1의 Host는 1번 |
| CombatCheckpoint schema 1 / 2 | 1은 Human 호환, 2는 Human/ServerAI 모드 포함. 현재 콘텐츠 버전 1 |

새 일반 싱글 Run은 임시 개발 참가자 한 명과 Identity schema 2를 만든다. 실제 계정 인증을 의미하지 않는다.
기존 `AIConsent`·`ConsentPolicyVersion`은 구버전 구조 검증용으로 읽으며 AI 승인 조건으로 사용하지 않는다.
v1~3을 관리 v4로 자동 이관하지 않는다. 전투별 AI 플래그에서 영속 Human 이력을 추정하지 않는다.
스킬·클래스의 동작 계약을 바꾸면 콘텐츠 버전도 검토해야 한다. 저장은 실행 코드 자체를 동결하지 않는다.

## 서버 실행과 화면

| 구성 | 책임 |
|---|---|
| GameMode | 서버 Arena/Encounter 준비, 신뢰된 참가자 배정, Host 진행 권한, 종료 정리 |
| RunStateSubsystem | 서버 GameInstance의 영속 진행·참여 목록·관리 lease. Client 공유 상태 원본이 아님 |
| GameState | Phase·파티·노드·결과·확정 revision·오류의 읽기 전용 표시 뷰 |
| CombatManager / TurnManager | 서버 턴·전투 결과·등록 유닛·Grid 검증. Client는 TurnManager를 생성하지 않음 |
| CombatActionAuthority | 값 명령의 실행 문맥·연결·소유권·턴·장착·대상·자원 검증 |
| PartyPlayerController | 소유 연결의 Server Reliable RPC와 응답, 로컬 선택/UI |
| Unit / Grid | 서버 상태를 Replication·RepNotify로 전달. HP/MaxHP는 GAS, 이동은 Character Movement |

`AssignRunParticipant`는 알려진 서버 연결에 계정을 배정하는 신뢰된 C++ 진입점이다. 같은 연결의 계정 변경·같은 계정의 중복 연결을 거절한다.
일반 협동 전투는 원래 참가자 전원을 요구한다. 관리 v4는 승인된 현재 Human 전원만 요구하고 AI 소유자의 Human 배정을 거절한다.
정원 검사는 Unreal `PreLogin/Login → ApproveLogin`을 사용하며 소유권 검증과 별개다. `?MaxPlayers`·디버그 override는 엔진 규칙상 정원을 바꿀 수 있다.
현재 fixture의 명시적 배정은 로그인 인증이 아니다. 서버는 향후 검증한 공급자 계정을 연결에 대응해야 한다.

`FCombatActionRequest`에는 Run/Host 세대·전투/바인딩 ID·턴·순번·유닛 ID·행동·스킬 PrimaryAssetId·대상 유닛/좌표를 담는다.
Client가 주장한 계정·비용·Actor reference를 받지 않는다. 서버가 현재 장착 AbilitySpec, AP/SubAP, 생존·busy·턴·대상을 다시 확인한다.
명령 순번을 재사용하거나 오래된 전투·턴·바인딩으로 요청하면 거절한다. `Accepted`는 실행 요청 승인으로 비동기 이동/스킬 성공과 구분한다.
늦은 응답은 새 선택을 지우지 않으며 처리한 응답의 재전송이 UI를 Pending에 남기지 않는다.

Unit의 팀·AP/SubAP·장착·턴/행동/사망·타일, Grid의 점유·영역·전열 보호를 서버에서 복제한다.
GAS는 서버 전용 능력 실행과 Minimal 복제를 사용한다. 유닛의 AIController 이동을 인간 PlayerController 소유권에 의존시키지 않는다.
HUD는 같은 서버 상태를 읽되 자기 Human 캐릭터 차례에만 행동 버튼을 활성화한다. AI 차례에는 `(AI)`를 표시한다.
네트워크 결과 화면은 최종 유닛을 유지하고 Host Continue 또는 월드 종료에서 정리한다. 일반 Standalone은 결과 시 즉시 정리한다.
Host의 진행 권한은 버튼과 실제 요청 모두 검사한다. 미배정·다른 Host·Client·서버의 원격 Controller는 노드/Continue를 확정하지 못한다.

## Async PvP 상대 Snapshot

`UPartySnapshotSaveGame`에 `FPartySnapshot`을 담고 Unreal SaveGame API로 저장한다. 초기 구현은 작은 파티의 동기 I/O다.
Snapshot은 실시간 상대 접속 없이 상대 빌드 하나로 Encounter를 만든다. 원래 아군 소유권·Host·턴 복구 데이터와 별개다.

- 필드: SnapshotId·MemberId·ClassId·이름·Stats·순서 있는 SkillIds·EquipmentIds·TacticsId·FormationSlot·버전.
- 카탈로그가 고정 ID를 신뢰된 Enemy 클래스/스킬 에셋으로 해석한다. 상대 데이터의 임의 클래스 경로를 실행하지 않는다.
- 1~4명, FormationSlot 0~3, 스킬 1~5개. 첫 스킬이 기본 공격이며 ID/배치/스킬 및 동일 AbilityClass 중복을 거절한다.
- 수치 상한은 HP 1,000,000, AP/SubAP 100, 이동 범위 32로 검증용 한계이며 밸런스 수치가 아니다.
- EquipmentIds/TacticsId는 저장 가능하지만 현재 전투에서는 빈 값만 지원한다.
- HP는 저장값 그대로 적용한다. HP 0은 저장 가능하나 현재 Snapshot Encounter에서는 거절한다.
- FormationSlot을 `Arena.EnemyCoords`로 해석하고 중복되지 않는 빈 Enemy 영역 타일인지 전체 검증한 뒤 스폰한다.
- Snapshot 적의 장착을 무작위로 바꾸거나 회복약을 자동 지급하지 않는다. 적 아이템 AI는 미구현이다.
- 실패하면 상세 오류와 함께 준비를 정리하며 PvE 적으로 조용히 대체하지 않는다. 잘못된 저장/로드는 기존 파일/출력 값을 보존한다.

생성한 적은 기존 `ConfigureProfession → RegisterUnits → StartCombat → Result → Cleanup` 흐름을 사용한다.
새 Encounter마다 상대 슬롯을 읽지만, 전투 체크포인트는 읽은 본문·카탈로그·실제 장착을 고정한다.
원본 슬롯을 변경·삭제해도 진행 중이던 전투는 고정 본문으로 복구한다. 다음 새 전투는 상대 슬롯을 다시 읽는다.
Run 전체 상대 이력, 경쟁 HP 정규화, 사망한 상대 포함 규칙, 매칭·서버 데이터/결과 검증은 후속이다.

## 확정 턴과 파일 저장

저장 순서는 `OnTurnEnd 완료 → 다음 생존 유닛 선택 → 저장 완료 → OnTurnStart`다.
첫 전투는 완료 턴 0 경계를 저장한다. 저장 시 모든 유닛은 비활성이고 다음 턴 AP/SubAP 초기화 전 상태다.
`CompletedTurnSerial`과 `NextTurnIndex`로 복원하며 다음 활성 유닛의 정상 턴 초기화는 한 번만 실행한다.
다른 유닛의 자원 잔량은 저장값을 유지한다. 이미 AI 판단이 시작된 뒤의 `OnTurnChanged`를 저장 훅으로 쓰지 않는다.

확정 본문은 다음을 담는다.

- Run Identity, 노드/Encounter, AttemptId·revision, 완료 턴과 다음 유닛 인덱스.
- 턴 순서대로 Unit 체크포인트 ID·원래 캐릭터/소유자·파티 슬롯·팀·클래스·이름.
- HP/MaxHP·AP/SubAP와 상한, 이동 범위·회복약, 사망·타일 좌표·Transform.
- 순서 있는 실제 스킬·기본 공격·Human/ServerAI 모드와 고정 상대 Snapshot.

저장 실패는 같은 다음 유닛·턴 번호·후보 본문을 유지한 채 전투를 멈춘다. Human·Enemy AI·자동 턴 종료가 앞서 진행하지 않는다.
현재 로컬 Host의 **저장 다시 시도**가 같은 본문을 재시도하며 성공 후 다음 턴을 한 번만 연다. Client는 오류와 중단 상태만 수신한다.
최종 승패와 Continue도 저장 성공 뒤 공개한다. 실패하면 대기 결과·기존 단계·확정 파일을 보존한다.

Unreal `USaveGame`/`SaveGameToMemory`로 직렬화하고 같은 디렉터리의 임시 파일을 flush·바이트 재검증한 뒤 Win64 파일 교체로 확정한다.
기존 파일을 먼저 지우지 않으며 미완료 `.tmp`는 로드 대상이 아니다. 현재 동기 저장은 Win64 로컬 어댑터로 다른 플랫폼/중앙 저장과 구분한다.
전원 상실·저장 장치 고장에 대한 절대 내구성이나 온라인 위변조 방지를 주장하지 않는다.

버전·Identity·중복 ID/점유·수치·생존/HP·다음 유닛·클래스/스킬·직업/아레나 적합성을 검증한다.
생존 유닛의 XY는 점유 타일 중심에서 5cm 이내여야 한다. 고정 상대의 클래스·장착·최대 스탯도 대조한다.
손상된 전투를 구버전 저장으로 낮춰 읽지 않으며 실패한 복원은 부분 Actor를 정리하고 확정 기록을 유지한다.
활성 Ability·이동/스킬 액터·지속/주기 효과·쿨다운·상태 태그 등 표현하지 못하는 상태는 저장 시 거절한다.
범용 GAS 상태·게임플레이 난수 스트림 복구는 미구현이다. 전열 보호는 복원한 진영·점유로 재계산한다.

## 관리 v4 재개와 lease

`FRunParticipationData.HumanParticipants`는 원래 소유자 중 지금 Human인 계정을 전투 밖에도 보존한다.
현재 목록에 없는 원래 캐릭터는 AI다. 사망만으로 목록을 바꾸지 않으며 결과에서 전투 본문을 비워도 목록은 남는다.
새 Encounter와 복원은 이 목록으로 모드를 결정한다. 저장된 모드가 다르면 스폰 전에 거절한다.

| API | 계약 |
|---|---|
| `ConfigureLocalDevelopmentCaller` | 신뢰 C++가 Development 계정·namespace를 주입. 같은 GameInstance에서 변경 불가 |
| `CreateManagedRun` | 최초 Host 1번·epoch 1·원래 2~4명 전원 Human. 기록과 lease 생성 성공 뒤 메모리에 적용 |
| `ReadManagedRun` | 원래 개발 참가자가 최신 본문/stamp를 조회. 조회만으로 권한·현재 Run을 바꾸지 않음 |
| `ResumeManagedRun` | 닫힌 실행·최신 stamp·이전 Human의 부분집합·번호순 Host를 검증하고 새 본문/세대/lease를 원자적으로 획득 |
| `ConfirmManagedResumeStarted` | Gameplay 복구 성공 뒤 pending 해제. 실패하면 lease·pending·오류를 유지 |
| `BeginManagedMenuTravel` | 같은 실행 SessionId·WorldContext의 TravelFailure를 GameInstance에서 감시 |
| `CloseManagedRun` | 전투/콜백 중단 후 lease·활성 상태 해제. 호출자·재개 대상은 유지 |

로컬 기준 저장소는 같은 PC의 공유 경로에서 배타적인 OS 실행 파일 핸들을 유지한다.
짧은 별도 트랜잭션 잠금으로 읽기·stamp 비교·본문 교체를 직렬화한다. Create/Acquire/Commit 실패는 기존 기록·lease·출력을 보존한다.
Run별 유효한 저장소 revision·HostEpoch·SessionId로 갱신하며 충돌 후 일반 SaveSlot으로 우회하지 않는다.
프로세스가 종료되어 핸들이 풀려도 Host는 자동 변경되지 않는다. 명시적 Resume에서만 새 epoch·SessionId를 만든다.
이는 동일 PC 개발 대역이며 다른 PC의 중앙 저장·계정 인증·네트워크 장애 판정이 아니다.

재개는 Run과 CombatCheckpoint Identity/모드를 함께 바꾸되 원래 소유권·번호·AttemptId·확정 턴을 보존한다.
현재 Human이 본인 Host 한 명이면 Standalone으로 복구한다. Listen Server에서는 현재 Human 전원이 배정될 때까지 멈추며 자동 AI 전환하지 않는다.
pending 중 Human 명령·노드·Continue를 막고 유효 lease의 서버 복원 준비·확정 턴 저장은 허용한다.
정상 월드 종료는 `ShutdownGameplay → CloseManagedRun` 순서로 Actor·AI·타이머·능력 콜백을 먼저 중단한다.

Authority는 관리 실행 여부를 `Reset` 후에도 유지하며 현재 lease·전체 Identity·호출자 Host·SessionId를 재검사한다.
관리 설정을 일반 Run으로 낮추거나 직접 Close 후 남은 Authority로 Human/AI 명령을 실행하지 못한다.
Human 바인딩과 서버 AI 모드 설정도 영속 목록에 맞아야 한다. 이 방어가 정상 Actor 정리를 대신하지 않는다.

## 메뉴와 아군 AI

일반 MainMenu **이어하기**는 v1 오프라인 또는 단일 참가자 LocalDevelopment v2/v3만 허용한다.
클릭 시 실제 로드 본문을 다시 검증한다. 협동·AccountProvider·v4 저장은 일반 Standalone 진입으로 우회하지 않는다.
일반 `LoadCheckpoint`의 협동 v3 지원은 유지하지만 기존 Host와 원래 참가자 전원의 서버 배정이 필요하다.

관리 패널은 신뢰 개발 경로가 호출자와 `SetManagedResumeTarget`을 설정했을 때만 표시한다. 사용자 계정 입력·자동 계정 추정·저장 검색은 없다.
**싱글로 전환하기**는 여러 Human 중 본인만 남기는 새 실행을 획득하며 타 캐릭터의 영구 AI를 안내한다.
**싱글 진행 이어하기**는 이미 본인만 Human인 관리 Run을 새 실행으로 연다. 이미 AI인 본인·종료/손상/오래된 기록은 거절한다.
메뉴 TravelFailure는 확정 Host/AI를 되돌리지 않고 해당 lease를 해제한다. Controller가 사라져도 같은 GameInstance의 새 메뉴에서 다시 이어간다.
Gameplay 도착 후 전투 복원 실패는 lease·pending을 유지하는 별도 재시도다. 계정 공급자 연동 전에는 fixture를 온라인 재개 서비스로 노출하지 않는다.

`UPartyAutoCombatComponent`는 PlayerUnit의 클래스·팀·원래 소유자를 유지하고 서버에서만 판단한다. 프레임 Tick이나 가짜 PlayerController를 만들지 않는다.
판단 순서는 유효 자기 회복약 → 장착·부여된 공격 스킬 → 접근이 나아지는 도달 가능 이동 → 턴 종료다.
적 피해 대상이 없는 스킬과 아군 피해가 포함되는 공격은 제외한다. 동점은 장착 순서·타일 좌표로 처리한다.
`ExecuteServerAI`는 별도 조작 세션·순번으로 인간과 같은 턴/자원/대상 검증을 이용한다. Client용 AI 우회 RPC는 없다.
행동 전 대기를 설정하고 완료 뒤 다음 틱에 판단한다. 실패는 무한 재시도하지 않고 턴 종료로 정리한다.
사망·턴 종료·전투/월드 정지는 전투·턴·AI 세션·generation이 지난 콜백을 무효화한다. 인간용 자원 소진 타이머와 중복 실행하지 않는다.

## Steam·PlayFab와 남은 서비스 정책

선택은 Steam+PlayFab(4B), 전투는 Unreal Listen Server와 Steam P2P/SDR 우선이다. 운영비를 피하려는 조건을 함께 유지한다.
별도 전투 서버 임대가 없어도 공동 Run 저장·승계 승인·결과 검증·MMR의 비용과 권위는 별도다.
Steam Cloud는 기본적으로 같은 사용자의 PC 사이 저장 동기화이며 참가자 간 단일 최신 Run을 원자적으로 확정하는 기능이 아니다.
Steam Leaderboards는 전투 정당성을 검증하지 않는다. Trusted 점수 제출의 publisher key를 Listen Host 클라이언트나 저장소에 넣지 않는다.

과거 조사에서 PlayFab 개발 모드는 타이틀/누적 계정/서비스별 한도를 안내했다. 현행 한도·무료 자격·출시 가격은 연동 전에 다시 확인해야 한다.
개발 무료 한도를 영구 무료 운영으로 취급하지 않는다. Foundation 자격·Azure Functions 계산 비용도 별도로 확인한다.
유료 리소스·과금 전환을 임의 활성화하지 않는다. 비용 조건을 랭크 삭제·무검증 점수·오프라인 진행 분기 승인으로 해석하지 않는다.
공식 참고: [Steam 인증](https://partner.steamgames.com/doc/features/auth?l=english), [Steam Cloud](https://partner.steamgames.com/doc/features/cloud?l=english), [PlayFab 개발 모드](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode).

8번에는 Steamworks AppID·테스트 계정/권한, PlayFab Title과 허용된 개발 환경이 필요하다. 공급자 선택은 다시 결정하지 않는다.
Unreal Online Subsystem·공식 SDK·엔진 비동기 delegate를 우선하며 선택한 UE 5.7 플러그인/NetDriver 코드를 확인해 설정한다.
로그인 성공·표시명·PIE ID·Client Verified 플래그를 서버 인증 증거로 쓰지 않는다. 서버가 검증한 티켓 계정을 실제 연결에 대응한다.
온라인 실패를 Development 계정이나 로컬 저장 성공으로 대체하지 않으며 이전 연결의 늦은 인증 완료도 현재 Run을 바꾸지 못하게 해야 한다.

중앙 서비스는 정확한 이전 revision/epoch의 한 갱신만 승인하고 후임자에게 원래 Host 파일 없이 최신 본문을 제공해야 한다.
같은 요청 ID·같은 본문은 기존 결과를 조회하고 같은 ID·다른 본문은 거절한다. 타임아웃 뒤 새 ID로 같은 동작을 다시 확정하지 않는다.
전투 Attempt의 결과·MMR은 다른 요청 ID로 재제출해도 한 번만 반영해야 한다. 서명·TLS만으로 전투 정당성을 증명하지 않는다.
MMR 판정 시점·계산/정산 단위, 이탈·미확정 턴 반복 악용 대응, 결과 재현/서버 계산 등 검증 방식은 미정이다.
비전투 AI 편집/보상·일시 끊김의 대기/시간 제한·AI 소유자의 관전 정책도 미정이며 필요 시 구체적인 선택지로 결정한다.

## 개발 실행 참조

아래는 사용자가 필요할 때 선택할 명령과 fixture다. 이 문서 통합에서 실행하지 않았으며 테스트 순서·결과 작성은 [TEST_REPORT](TEST_REPORT.md)를 따른다.
프로젝트 루트 PowerShell, 로컬 UE 5.7 설치 경로를 기준으로 한다. 샘플 생성은 로컬 에셋/저장을 만들므로 최초 준비 때만 사용한다.

```powershell
$editor = 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe'
$editorCmd = 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = "$PWD/ProjectA.uproject"
& $editorCmd $project -run=pythonscript -EnablePlugins=PythonScriptPlugin "-script=$PWD/Source/ProjectAEditor/Scripts/ConfigureSnapshotContent.py" -unattended -nullrhi -nop4
& $editor $project -ProjectAOpponentSnapshot=SampleOpponent
```

샘플은 Hunter 1명·최대 HP 140/현재 HP 120·AP 2/SubAP 1·이동 1·배치 0·DefaultAttack/SweepingStrike다. 기존 샘플 슬롯은 덮어쓰지 않는다.
SlotId는 영문·숫자·밑줄 1~64자이며 `ProjectA_Opponent_<SlotId>`로 저장한다. 샘플 `.sav`는 Git·패키지에 포함하지 않는다.
Snapshot Run은 `ProjectA_SnapshotRun_<SlotId>`, PvE는 `ProjectA_Run`이다. `-ProjectASaveSlot=...`이 우선하며 이어하기는 같은 상대 인자를 사용한다.
패키지에서 샘플을 쓰려면 그 실행 환경의 저장 API로 먼저 생성해야 한다. 실행 인자를 제거하면 기본 PvE다.

```powershell
& $editorCmd $project -unattended -nop4 -RenderOffscreen -nosound '-ExecCmds=Automation RunTests ProjectA.Coop.' '-TestExit=Automation Test Queue Empty'
& $editorCmd $project -unattended -nop4 -RenderOffscreen -nosound -T14ManagedRunPIE '-ExecCmds=Automation RunTests ProjectA.ManagedRunPIE.' '-TestExit=Automation Test Queue Empty'
```

| 필터/옵션 | 실제 범위 |
|---|---|
| `ProjectA.Coop.ListenServerClientCombat` / `ListenServerThreePlayerCombat` / `ListenServerFourPlayerCombat` | 2/3/4인 각 소유자의 RPC, 전투 상태, Host 진행 권한 |
| `ProjectA.Coop.CheckpointSessionRestart` / `CheckpointSessionRestart3Players` / `CheckpointSessionRestart4Players` | 끊김 후 기존 Host·원래 참가자 전원의 새 세션 복구 |
| 위 복구 + `-T14CheckpointAI` | 소유자도 연결된 AI 모드 복구. 불참 승계와 구분 |
| 위 복구 + `-T14CheckpointOpponent=Replace` 또는 `Delete` | 원본 상대 변경/삭제 후 고정 본문 복구 |
| `ProjectA.ManagedRunPIE.HostSuccession` | 원래 3명 → 2번 Host·3번 Client·1번 AI |
| `ProjectA.ManagedRunPIE.HostSuccession4Players` | 원래 4명 → 2번 Host·3/4번 Client·1번 AI. 최신 사용자 검증 대상 |
| `ProjectA.ManagedRunPIE.SoloMenuConversion` | 4번의 실제 메뉴 단독 전환·영구 AI·이동 실패 후 재개 |
| `ProjectA.Run.Managed` / `ProjectA.Checkpoint` | 관리 저장/전이/호환과 확정 경계/손상/거절 |

관리 PIE는 `-T14ManagedRunPIE`가 없으면 안내만 출력한다. 프로세스 재시작도 전용 인자 없는 성공을 실제 재시작 검증으로 계산하지 않는다.
PIE fixture는 고유 저장 namespace/슬롯과 명시적 계정 매핑을 사용하고 사용자 Play 설정을 transient 복제본으로 보존한다.
맵 PIE 복제 전에 편집기 NavMesh 정상 생성, 이동 전에 서버 경로·바닥 안착을 기다린다. 고정 sleep·강제 rebuild로 이동 조건을 완화하지 않는다.
정원 검사의 `ApproveLogin` 호출은 실제 다섯 번째 클라이언트 접속 시험과 구분한다. 인터넷 지연/손실·패키지·원격 서비스 결과로 확대하지 않는다.

독립 전투 저장은 `ProjectA.Persistence.CombatProcessRestart`의 Writer `-T14WriteCombatCheckpoint`, Reader `-T14ReadCombatCheckpoint` 순서다.
두 프로세스는 같은 `-T14CheckpointSlot=T14_CombatProcess_<고유값>`을 사용하며 AI는 양쪽에 `-T14CheckpointAI`를 추가한다.
로컬 lease 프로세스 fixture는 `ProjectA.RunAuthority.ProcessProbe`와 `-T14AuthorityProbe=Holder|BusyReader|ResumeReader`를 사용한다.
공통 `-T14AuthorityNamespace`, `-T14AuthorityRunId`, `-T14AuthorityProbeId`를 지정하고 Holder의 ready 파일 뒤 BusyReader, 해당 테스트 Holder 종료, ResumeReader 순서다.
이는 테스트 프로세스의 잠금 해제 검증용이며 실제 사용자 프로세스를 종료하는 작업이 아니다. 상세 과거 결과는 [HISTORY](HISTORY.md)에만 보존한다.
