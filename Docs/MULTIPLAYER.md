# 멀티플레이 설계·개발 계약

[프로젝트 안내](../README.md) · [과거 구현·검증 이력](HISTORY.md) · [남은 확인](TODO.md#1-사용자-작동-확인)

## 현재 상태

기본 Combat는 라운드 계획·시간차 실행으로 교체했다. `ACombatRoundCoordinator`가 서버 계획·시각·위치·충돌·피해를 소유하며 기존 Listen Server·원래 캐릭터 소유권·관리 lease를 연결한다. 순차 턴·연속 AI 실행과 턴 저장 복원은 폐기했다. 새 2/4인 전투·UI의 작동 검증은 미실행이다.

이전 ff22940의 승계 PIE·관리 계약 성공은 순차 전투 이력이다. 현재 비전투 관리 재개는 유지하지만 저장된 Phase Combat는 본문·Host·참가자·lease 변경 전에 거절한다. 새 전투의 라운드 중간 복구는 미지원이다. 상세 경계는 [12절](#12-시간차-자동-전투의-확장-경계)을 따른다.

Steam/PlayFab 인증·인터넷 P2P·공유 저장·결과 검증·MMR은 미구현이다. 협동의 추가 확인은 [TODO](TODO.md#1-사용자-작동-확인)에 기록하며 2인 → 4인 순서로 진행한다.

## 확정 정책

| 항목 | 규칙 |
|---|---|
| 인원 | 최대 4명. 기본 `GameSession.MaxPlayers=4`는 Listen Host를 포함한다 |
| 인간 조작 | 각 플레이어는 원래 자기 캐릭터만 조작한다. Host도 타인 캐릭터를 직접 조작하지 못한다 |
| 소유권 | Host·연결·AI 모드가 바뀌어도 `CharacterId`와 원래 소유자는 유지한다 |
| 참가자 | 원래 Run 참가자 외 대체 참가자는 허용하지 않는다 |
| 최초 번호 | 최초 Host는 1번, 최초 합류 순서대로 2·3·4번. 재접속 순서나 파티 슬롯으로 다시 부여하지 않는다 |
| 중단 | 정상 종료·연결 끊김만으로 Host나 AI 모드를 바꾸지 않는다 |
| 명시적 재개 | 현재 Human으로 재개할 원래 참가자 중 최초 번호가 가장 작은 사람이 Host가 된다 |
| 단독 전환 | 2·3·4번이 혼자 전환하면 본인이 Host, 나머지 원래 캐릭터는 AI가 된다 |
| AI 결정 | 재개 Host가 단독 확정한다. 개인별 사전 동의는 요구하지 않는다 |
| 영구 AI | 해당 Run 종료까지 AI를 유지한다. 이후 Human 목록이나 Host 후보에 다시 넣지 않는다 |
| 진행 결정 | 현재 Host만 노드 선택·승리 Continue를 실행한다 |
| 현재 복구 지점 | 마지막 전투 외 확정 저장. 새 라운드 중간 복구와 이전 순차 Combat 저장 재개는 미지원 |
| 경쟁 목표 | 현재 Human 참여자에게만 MMR을 반영하고 불참자에게 추가 변동을 적용하지 않는다. 실제 계산·연동은 후속이다 |

명시적 재개는 확정 데이터로 새 서버를 여는 기능이다. 실행 중인 Listen Server를 즉시 옮기는 자동 Host Migration은 아니다.
기존 실행이 살아 있으면 다른 참가자가 같은 기록을 획득할 수 없다. 로컬 파일 복사나 계정 입력을 승계 승인으로 사용하지 않는다.
한 사람이 가질 캐릭터 수, 관전·화면 분할, AI 캐릭터의 비전투 장비·보상 결정권은 미정이다.

## 식별자와 저장 버전

ID·버전은 아래 계층별로 구분한다. 영속 DTO에 Actor·Controller·Ability 인스턴스·실행 핸들을 저장하지 않는다.

| 식별자 | 수명과 의미 |
|---|---|
| `RunId` | 같은 Run 전체를 식별하며 재개 후에도 유지 |
| `FRunAccountId` | 공급자와 불투명 Subject. 표시명·PIE PlayerId·접속 순서가 아니다 |
| `JoinOrdinal` | 최초 합류 번호. 승계 후보 순서를 결정 |
| `CharacterId` / `OwnerAccountId` | 원래 캐릭터와 소유자. AI·Host 변경으로 이전하지 않음 |
| `HostAccountId` / `HostEpoch` | 현재 Host와 실행 세대. 명시적 관리 재개 시 epoch 증가 |
| 이전 `AttemptId` | 과거 순차 전투 저장을 읽어 식별하는 필드. 현재 실행/복구에 사용하지 않음 |
| 이전 전투 체크포인트 `Revision` | 과거 본문 검증용. 새 라운드 저장 순번으로 재해석하지 않음 |
| 저장소 stamp | RunId·저장소 revision·HostEpoch·SessionId. 저장소 revision은 지도/결과/재개를 포함한 모든 저장 순번 |
| Runtime ID·수정 번호 | 새 CombatId·RoundNumber·PlanRevision, 서버 유닛 ID·소유 연결. Run 영속 ID와 구분 |

| 저장 계층 | 지원과 호환 |
|---|---|
| Opponent Snapshot | schema 1, 카탈로그와 일치하는 ContentVersion. Run 저장과 별개 |
| Run SaveGame v1 | 식별 정보 없는 `LegacyOffline`. 소유자·Host를 추정 이관하지 않음 |
| Run SaveGame v2 | 일반 Run의 전투 밖 진행과 식별/소유권 |
| Run SaveGame v3 | 이전 순차 Combat 본문. 현재 Continue/로드 거절, 파일 보존 |
| Run SaveGame v4 | 비전투 관리 진행·영속 Human 목록·lease 유지. 이전 Combat 재개 거절. LocalDevelopment 원래 참가자 2~4명 |
| Identity schema 1 | 최초 번호 0을 유지하며 번호순 승계를 거절 |
| Identity schema 2 | 1~참가자 수의 고유하고 연속된 최초 번호. 최초 epoch 1의 Host는 1번 |
| CombatCheckpoint schema 1 / 2 | 과거 Human/ServerAI 저장의 구조 검증만 유지. 현재 라운드 저장/복구에는 사용하지 않음 |
| Run EncounterProgress schema 0 / 1 | 0은 기존 상점 없는 경로. 1은 고정 상점 3개의 제시 목록·선택 ID·퇴장 상태. 새 일반/관리 Run에 적용 |

새 일반 싱글 Run은 임시 개발 참가자 한 명과 Identity schema 2를 만든다. 실제 계정 인증을 의미하지 않는다.

인카운터 선택·상점 퇴장은 기존 노드·Continue와 같이 현재 Host만 결정한다. Client는 GameState의 직렬화 가능한 표시 뷰를 수신하며 별도 구매·소유권 변경은 없다. 선택·퇴장을 저장한 뒤 상태를 공개하고 실패 시 이전 상태로 복원한다. 관리 Run은 기존 lease·저장소 revision으로 전이를 기록하며 `EncounterChoice`·`Shop`도 전투 밖 재개를 허용한다. 일반 싱글 이어하기는 선택 화면 또는 선택한 상점을 복원한다. 기존 저장의 누락 필드는 schema 0으로 읽어 원래 경로를 유지한다. 새 저장을 이전 실행 파일로 여는 역호환은 지원 대상으로 보지 않는다.
기존 `AIConsent`·`ConsentPolicyVersion`은 구버전 구조 검증용으로 읽으며 AI 승인 조건으로 사용하지 않는다.
v1~3을 관리 v4로 자동 이관하지 않는다. 전투별 AI 플래그에서 영속 Human 이력을 추정하지 않는다.
스킬·클래스의 동작 계약을 바꾸면 콘텐츠 버전도 검토해야 한다. 저장은 실행 코드 자체를 동결하지 않는다.

## 서버 실행과 화면

| 구성 | 책임 |
|---|---|
| GameMode / Encounter | 서버 Arena·파티·참가자·Run 시작/종료, Host 진행 권한 |
| RunState / GameState | 영속 진행·관리 lease와 읽기 전용 진행/결과 표시 뷰 |
| CombatManager / RoundCoordinator | 전투 등록·계획·준비·서버 시각표·실제 이동·공격·최종 결과 |
| CombatActionAuthority | 원래 캐릭터 소유권·서버 연결·관리 SessionId/lease·현재 Human/AI 검증 |
| CombatRoundPlayerController | 소유 연결의 Reliable 계획/준비 RPC, 서버 응답·최신 복제 대기 |
| Unit / Projectile | 서버 HP·실제 위치·사망·공격 피격체/투사체와 복제 표현 |

`AssignRunParticipant`는 신뢰 C++가 서버 연결에 계정을 배정한다. 계정 입력·표시 이름으로 소유권을 주장할 수 없다. 일반 협동은 원래 참가자 전원, 관리 Run은 승인된 현재 Human 전원이 필요하다. 새 Host도 다른 캐릭터의 인간 조작권을 얻지 않는다.

계획 요청은 CombatId·RoundNumber·PlanRevision·UnitId·SkillId·대상 ID/좌표·목적지만 전달한다. 서버가 소유권·등록·스킬 부여·비용·목표·최종 배치를 검증하고 피해량/시각은 서버 데이터에서 결정한다. 해석이 끝난 이전 라운드나 낡은 수정 번호, 해결 중 편집, 대체 연결의 요청은 거절한다. UI는 서버 응답과 복제된 최신 상태를 기다린다.

현재 계획 변경 시 인간 팀원의 Ready를 모두 해제하고 전체 준비 완료에서 명령을 잠근다. 이 정책과 복귀 칸 충돌·동시 타격 순서는 사용자 확인 대기 중인 구현 기본값이다. Client 프레임·충돌·몽타주가 독립 피해를 확정하지 않는다. 실제 네트워크의 지연·손실·4인 표시 일치는 미검증이다.

## Async PvP 상대 Snapshot

`UPartySnapshotSaveGame`에 `FPartySnapshot`을 담고 Unreal SaveGame API로 저장한다. 초기 구현은 작은 파티의 동기 I/O다.
Snapshot은 실시간 상대 접속 없이 상대 빌드 하나로 Encounter를 만든다. 원래 아군 소유권·Host와 라운드 실행 상태는 별개다.

- 필드: SnapshotId·MemberId·ClassId·이름·Stats·순서 있는 SkillIds·EquipmentIds·TacticsId·FormationSlot·버전.
- 카탈로그가 고정 ID를 신뢰된 Enemy 클래스/스킬 에셋으로 해석한다. 상대 데이터의 임의 클래스 경로를 실행하지 않는다.
- 1~4명, FormationSlot 0~3, 순서 있는 스킬 1~5개. ID/배치/스킬 중복과 서로 다른 카탈로그 ID가 같은 스킬 에셋을 가리키는 별칭 중복을 거절한다. `ResolveRoundSkill`로 검증하며 명시 프로필은 GAS 클래스 없이 허용한다. 서로 다른 스킬 에셋의 동일 AbilityClass 공유는 허용한다.
- 수치 상한은 HP 1,000,000, AP/SubAP 100, 이동 범위 32로 검증용 한계이며 밸런스 수치가 아니다.
- EquipmentIds/TacticsId는 저장 가능하지만 현재 전투에서는 빈 값만 지원한다.
- HP는 저장값 그대로 적용한다. HP 0은 저장 가능하나 현재 Snapshot Encounter에서는 거절한다.
- FormationSlot을 `Arena.EnemyCoords`로 해석하고 중복되지 않는 빈 Enemy 영역 타일인지 전체 검증한 뒤 스폰한다.
- Snapshot 적의 장착을 무작위로 바꾸거나 회복약을 자동 지급하지 않는다. 적 아이템 AI는 미구현이다.
- 실패하면 상세 오류와 함께 준비를 정리하며 PvE 적으로 조용히 대체하지 않는다. 잘못된 저장/로드는 기존 파일/출력 값을 보존한다.

생성한 적은 기존 `ConfigureProfession → RegisterUnits → StartCombat → Result → Cleanup` 흐름을 사용한다.
새 Encounter마다 상대 슬롯을 읽어 유닛·장착을 준비한다. 현재 진행 중 전투는 저장/복구하지 않는다. 마지막 비전투 저장에서 Encounter를 다시 시작하면 상대 슬롯을 다시 읽는다. 상대 변경/삭제와 경쟁 재시도 정책은 별도 설계 대상이다.
Run 전체 상대 이력, 경쟁 HP 정규화, 사망한 상대 포함 규칙, 매칭·서버 데이터/결과 검증은 후속이다.

## 확정 턴과 파일 저장

기존 확정 턴 실행·저장/복원 경로는 폐기했다. `CompletedTurnSerial`·`NextTurnIndex`·Checkpoint schema는 이전 파일을 인식하고 검증하는 데만 남기며 새 라운드 번호로 재해석하지 않는다. `CommitCombatCheckpoint`·`RestoreSavedCombat`은 명시적으로 거절한다. 기존 파일은 삭제·다운그레이드하지 않는다.

현재 저장은 Map·Result·EncounterChoice·Shop·종료 결과 등 비전투 Run 전이만 수행한다. Preparing/Combat는 자동 저장하지 않는다. 새 전투 중 종료하면 마지막 비전투 확정 기록에서 해당 Encounter를 다시 시작한다. 라운드 경계·진행 중 시전/투사체·난수 상태의 저장과 복구는 미구현이다.

일반 Continue/로드는 저장된 Phase Combat를 메모리 적용 전에 거절한다. 관리 Resume는 이전 Combat를 참가자·HostEpoch·lease·기준 저장 변경 전에 거절한다. 원래 소유권·버전 검증과 비전투 저장의 지원 범위는 유지한다.

최종 승패와 Continue는 저장 성공 뒤 공개하며 실패 시 대기 결과·기존 단계·파일을 보존하고 재시도한다. Unreal USaveGame 직렬화와 같은 디렉터리 임시 파일의 flush·바이트 검사·Win64 파일 교체를 유지한다. 기존 파일을 먼저 지우지 않는다. 로컬 어댑터는 온라인 정본 권위나 절대 내구성을 보장하지 않는다.

## 관리 v4 재개와 lease

`FRunParticipationData.HumanParticipants`는 원래 소유자 중 지금 Human인 계정을 전투 밖에도 보존한다.
현재 목록에 없는 원래 캐릭터는 AI다. 사망만으로 목록을 바꾸지 않으며 결과에서 전투 본문을 비워도 목록은 남는다.
새 Encounter는 이 목록으로 모드를 결정한다. 이전 Combat 본문의 조작 모드는 구조 검증 후 재개 거절 대상으로만 사용한다.

| API | 계약 |
|---|---|
| `ConfigureLocalDevelopmentCaller` | 신뢰 C++가 Development 계정·namespace를 주입. 같은 GameInstance에서 변경 불가 |
| `CreateManagedRun` | 최초 Host 1번·epoch 1·원래 2~4명 전원 Human. 기록과 lease 생성 성공 뒤 메모리에 적용 |
| `ReadManagedRun` | 원래 개발 참가자가 최신 본문/stamp를 조회. 조회만으로 권한·현재 Run을 바꾸지 않음 |
| `ResumeManagedRun` | 저장된 Combat는 변경 전에 거절. 비전투 기록은 닫힌 실행·최신 stamp·Human 부분집합·번호순 Host 검증 후 새 본문/세대/lease 획득 |
| `ConfirmManagedResumeStarted` | Gameplay 복구 성공 뒤 pending 해제. 실패하면 lease·pending·오류를 유지 |
| `BeginManagedMenuTravel` | 같은 실행 SessionId·WorldContext의 TravelFailure를 GameInstance에서 감시 |
| `CloseManagedRun` | 전투/콜백 중단 후 lease·활성 상태 해제. 호출자·재개 대상은 유지 |

로컬 기준 저장소는 같은 PC의 공유 경로에서 배타적인 OS 실행 파일 핸들을 유지한다.
짧은 별도 트랜잭션 잠금으로 읽기·stamp 비교·본문 교체를 직렬화한다. Create/Acquire/Commit 실패는 기존 기록·lease·출력을 보존한다.
Run별 유효한 저장소 revision·HostEpoch·SessionId로 갱신하며 충돌 후 일반 SaveSlot으로 우회하지 않는다.
프로세스가 종료되어 핸들이 풀려도 Host는 자동 변경되지 않는다. 명시적 Resume에서만 새 epoch·SessionId를 만든다.
이는 동일 PC 개발 대역이며 다른 PC의 중앙 저장·계정 인증·네트워크 장애 판정이 아니다.

현재 재개는 비전투 Run Identity·영속 Human 목록만 갱신하며 원래 소유권·번호를 유지한다. 이전 CombatCheckpoint Identity/모드 갱신 코드는 제거했다.
현재 Human이 본인 Host 한 명이면 Standalone으로 복구한다. Listen Server에서는 현재 Human 전원이 배정될 때까지 멈추며 자동 AI 전환하지 않는다.
pending 중 Human 명령·노드·Continue를 막고 비전투 Gameplay 진입 확인 후 해제한다. 순차 턴 체크포인트 저장은 허용하지 않는다.
정상 월드 종료는 `ShutdownGameplay → CloseManagedRun` 순서로 Actor·AI·타이머·능력 콜백을 먼저 중단한다.

Authority는 관리 실행 여부를 `Reset` 후에도 유지하며 현재 lease·전체 Identity·호출자 Host·SessionId를 재검사한다.
관리 설정을 일반 Run으로 낮추거나 직접 Close 후 남은 Authority로 Human/AI 명령을 실행하지 못한다.
Human 바인딩과 서버 AI 모드 설정도 영속 목록에 맞아야 한다. 이 방어가 정상 Actor 정리를 대신하지 않는다.

## 메뉴와 아군 AI

일반 MainMenu 이어하기는 v1 오프라인 또는 단일 참가자 LocalDevelopment v2의 비전투 상태만 허용한다. 실제 클릭에서 방금 읽은 본문을 다시 검증한다. 일반 `LoadCheckpoint` 역시 이전 v3/Combat를 거절한다. 협동·AccountProvider·v4를 Standalone 경로로 우회하지 않는다.

관리 패널은 신뢰 개발 경로가 호출자·재개 대상을 설정했을 때만 표시한다. 비전투 기록에 대해 번호순 Host·영구 AI·lease를 유지한다. Combat 기록은 명시적인 미지원 메시지와 함께 거절한다. 메뉴 이동 실패는 확정 Host/AI 기록을 되돌리지 않고 lease를 해제하여 재시도를 허용한다. 온라인 로그인·관리 저장 검색·수동 협동 재개 UI는 미구현이다.

개발용 협동은 새 비관리 Listen 방을 생성한다. 주소 참가가 원래 소유자 인증을 대신하지 않으며 이탈한 방은 새 방으로 시작한다. 자동 Host 변경·대체 참가·자동 AI 전환은 없다.

이전 `UPartyAutoCombatComponent`와 EnemyUnit의 순차 재판단 실행은 제거했다. RoundCoordinator가 인간 초안 이전에 적·ServerAI 아군의 명령을 한 번 고정하고 준비 상태로 만든다. 클래스·진영·원래 소유자는 유지한다. 초기 AI는 가까운 적과 단일 공격을 선택하며 타일 접근·지원·이동 전술은 후속이다.

## Steam·PlayFab와 남은 서비스 정책

연동 기준은 Steam+PlayFab, 전투 연결은 Unreal Listen Server·Steam P2P/SDR이다. 운영비 회피 조건을 적용한다.
별도 전투 서버 임대가 없어도 공동 Run 저장·승계 승인·결과 검증·MMR의 비용과 권위는 별도다.
Steam Cloud는 기본적으로 같은 사용자의 PC 사이 저장 동기화이며 참가자 간 단일 최신 Run을 원자적으로 확정하는 기능이 아니다.
Steam Leaderboards는 전투 정당성을 검증하지 않는다. Trusted 점수 제출의 publisher key를 Listen Host 클라이언트나 저장소에 넣지 않는다.

2026-09-11 재확인한 [PlayFab Development 문서](https://learn.microsoft.com/en-us/xbox/playfab/pricing/development-mode)는 Foundation으로의 전환을 안내한다. 아래 준비 절차처럼 현재 계정에 제공되는 모드·무료 자격을 확인한다. 과거 개발 한도를 신규 Title의 무료 보장으로 사용하지 않는다.
개발 무료 한도를 영구 무료 운영으로 취급하지 않는다. Foundation 자격·별도 계산 리소스 비용도 확인한다.
유료 리소스·과금 전환을 임의 활성화하지 않는다. 비용 조건을 랭크 삭제·무검증 점수·오프라인 진행 분기 승인으로 해석하지 않는다.
공식 참고: [Steam 인증](https://partner.steamgames.com/doc/features/auth?l=english), [Steam Cloud](https://partner.steamgames.com/doc/features/cloud?l=english), [PlayFab 개발 모드](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode).

준비물은 Steamworks AppID·테스트 계정/권한·PlayFab Title·승인된 개발 환경이다. 공급자는 기존 결정을 유지한다.
Unreal Online Subsystem·공식 SDK·엔진 비동기 delegate를 우선하며 선택한 UE 5.7 플러그인/NetDriver 코드를 확인해 설정한다.
로그인 성공·표시명·PIE ID·Client Verified 플래그를 서버 인증 증거로 쓰지 않는다. 서버가 검증한 티켓 계정을 실제 연결에 대응한다.
온라인 실패를 Development 계정이나 로컬 저장 성공으로 대체하지 않으며 이전 연결의 늦은 인증 완료도 현재 Run을 바꾸지 못하게 해야 한다.

중앙 서비스는 정확한 이전 revision/epoch의 한 갱신만 승인하고 후임자에게 원래 Host 파일 없이 최신 본문을 제공해야 한다.
같은 요청 ID·같은 본문은 기존 결과를 조회하고 같은 ID·다른 본문은 거절한다. 타임아웃 뒤 새 ID로 같은 동작을 다시 확정하지 않는다.
전투 Attempt의 결과·MMR은 다른 요청 ID로 재제출해도 한 번만 반영해야 한다. 서명·TLS만으로 전투 정당성을 증명하지 않는다.
MMR 판정 시점·계산/정산 단위, 이탈·미확정 턴 반복 악용 대응, 결과 재현/서버 계산 등 검증 방식은 미정이다.
비전투 AI 편집/보상·일시 끊김의 대기/시간 제한·AI 소유자의 관전 정책도 미정이며 필요 시 구체적인 선택지로 결정한다.

## T14-8 서비스 준비

2026-09-11 기준 자체 Steam App ID·PlayFab Title 미준비, T14-7 로컬 승계 검증 완료. 아래 절차는 서비스 준비 범위이며 실제 연동은 미구현이다.

### 비용과 준비 순서

1. [PlayFab Game Manager](https://developer.playfab.com/)에 Microsoft 계정으로 로그인하고 Studio를 준비한다. 생성 화면에서 Title 이름은 개발 환경임을 구별할 수 있게 `ProjectA-Dev` 등을 사용한다. Title이 생성되면 게임 이름 아래의 Title ID와 선택된 서비스 모드를 확인한다. 계정·Studio·Title 생성 순서는 [공식 시작 안내](https://learn.microsoft.com/en-us/xbox/playfab/live-service-management/gamemanager/quickstart)를 따른다.
2. Title을 확정하기 전에 무료로 제공되는 모드와 사용량 조건을 확인한다. [Foundation 온보딩](https://learn.microsoft.com/en-us/xbox/playfab/get-started/foundation-onboarding)은 Entra ID·Xbox 출시/출시 계획·Partner Center·미리보기 승인을 전제로 안내한다. Steam 우선이라는 기존 결정을 Xbox 출시 계획으로 바꾸거나 자격을 추정하지 않는다. 무료 모드가 없거나 유료 전환만 보이면 모드 이름·표시된 비용을 확인한 뒤 다음 방향을 결정한다. Launch/Live 전환과 유료 계산·호스팅 리소스 생성은 현재 준비 범위에 없다.
3. Steam은 정식 앱 등록과 결제 전 연결 시제품을 구분한다. [Epic의 App ID 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine)는 모든 개발자가 공유하는 테스트 ID `480`을 허용하며 출시에 자체 ID가 필요하다고 명시한다. 480은 별도 개발 연결에서만 사용할 후보이며 ProjectA의 소유권·배포 권한이나 PlayFab 정식 Steam 인증 준비 완료를 뜻하지 않는다.
4. 자체 ID를 만들 시점에는 [Steamworks 온보딩](https://partner.steamgames.com/doc/gettingstarted/onboarding)의 계약·신원·은행/세금 절차와 앱 등록을 사용자가 진행한다. [Steam Direct 수수료](https://partner.steamgames.com/doc/gettingstarted/appfee)는 앱당 USD 100 상당이며 지역 세금이 적용될 수 있다. 이는 전투 서버의 월 운영비와 별도다. 현재 결제를 요청하거나 대신 실행하는 단계는 아니다.
5. 자체 App ID·Title이 준비되면 PlayFab의 Steam 연동 설정에서 해당 앱 ID와 권한이 있는 Web API Key를 연결한다. 키는 공급자 관리 화면에서만 취급하고 게임/Listen Host/Git/채팅에 넣지 않는다. Steam 티켓으로 로그인하는 계약과 필요한 설정은 [공식 Steam 인증 API](https://learn.microsoft.com/en-us/xbox/playfab/api-references/c/pfauthentication/functions/pfauthenticationloginwithsteamasync)를 따른다. 단순 CustomID 로그인이나 로그인 성공 자체를 전투 결과 검증으로 대신하지 않는다.

위 Epic 웹 예제의 기존 SteamNetDriver 설정은 프로젝트에 그대로 복사하지 않는다. 설치된 UE 5.7 소스와 플러그인 구성을 아래처럼 대조했다.

### 로컬 엔진 점검과 다음 구현

| 대상 | 정적 확인 결과 / 적용 방향 |
|---|---|
| 프로젝트 | `ProjectA.uproject`에 Steam/PlayFab 활성화 없음. Build.cs의 OnlineSubsystem은 주석뿐이며 Config에 Steam 서비스/NetDriver 설정 없음 |
| Steam API | 엔진 `Plugins/Online/OnlineSubsystemSteam/OnlineSubsystemSteam.uplugin` 존재, 기본 비활성 |
| P2P transport | 엔진 `Plugins/Runtime/Steam/SteamSockets/SteamSockets.uplugin`과 `USteamSocketsNetDriver` 존재. `/Script/SteamSockets.SteamSocketsNetDriver`를 사용할 경로 |
| IP 전용 경로 | `SocketSubsystemSteamIP.uplugin`은 NAT punchthrough를 제공하지 않으며 P2P에 SteamSockets를 쓰도록 명시. 두 플러그인을 같은 transport로 취급하지 않음 |
| PlayFab | 프로젝트와 설치된 엔진에서 `.uplugin`을 찾지 못함. SDK 도입 시 UE 5.7 지원 버전·인증 API를 먼저 고정 |
| 관리 저장 | 현재 `FLocalRunAuthorityStore`는 같은 PC 동기 파일/OS lease, 개발 호출자만 지원. Steam 계정 ID만 주입해 온라인 관리 Run으로 승격하지 않음 |

연결 구현 순서는 **명시적인 개발 온라인 설정 → 엔진 OnlineSubsystem 세션·SteamSockets 연결 → 검증된 원래 계정 배정 → 별도 PC의 사용자 접속 확인 → 공유 저장/승계 원자 처리 → 확정된 MMR 정책**으로 둔다. 온라인 초기화 실패를 개발 계정 성공으로 대체하지 않는다. 기본 싱글플레이 진입과 기존 로컬 저장은 계속 지원한다.

사용자가 준비 결과로 전달할 것은 `Steam: 자체 ID 준비/480 개발 연결 희망/보류`, `PlayFab: Title ID 또는 미생성 이유와 표시 모드`, `서로 다른 계정의 테스트 PC 2대 준비 여부`다. 비밀번호·인증 티켓·Web API/Secret Key·은행/세금 정보는 전달 대상이 아니다. 최초 온라인 작동 확인은 서비스 준비와 구현 후 [남은 확인](TODO.md#1-사용자-작동-확인)에 추가한다.

## 개발 실행 참조

프로젝트 루트 PowerShell·UE 5.7 기준 명령이다. 실행 대상은 [TODO](TODO.md), 완료 결과는 [HISTORY](HISTORY.md)를 따른다. 샘플 생성은 로컬 에셋/저장을 작성하므로 최초 준비 시에만 실행한다.

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

아래 순차 전투·턴 복구용 명령/옵션은 과거 재현 기록이다. 현재 기본 전투의 실행 지침이나 통과 근거로 사용하지 않으며 새 확인 항목은 TODO에 간단히 기록한다.

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
| `ProjectA.ManagedRunPIE.HostSuccession4Players` | 원래 4명 → 2번 Host·3/4번 Client·1번 AI. 당시 실행 결과는 HISTORY의 검증 이력 참조 |
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

## 12 시간차 자동 전투의 확장 경계

### 12-1 서버 실행과 조작권

기본 Combat는 [GAME_DESIGN 8절](GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투)의 라운드 계획/시간차 실행이다. 기존 Run 식별·소유 연결·관리 lease를 사용하되 순차 행동 실행은 제거한다. 서버가 계획·전체 잠금·0.01초 실행·실제 좌표·근접/지점/투사체 충돌·피해·사망·잔여 공격 종료를 확정하고 Client에 상태를 복제한다. 공격은 [8-5절의 충돌 계약](GAME_DESIGN.md#8-5-발동과-피격)을 따르며 Client 충돌 통지나 별도 명중 확률로 서버 결과를 대체하지 않는다. 서버의 기존 시전·발사 시각을 유지하며 클라이언트 애니메이션 알림에 피해 권위를 넘기지 않는다.

동일 예정 시각의 타격은 안정적인 서버 순서로 즉시 처리하는 초기값이다. 전체 라운드의 원자적 동시 성립이나 물리 충돌까지 포함한 결정성을 보장하지 않는다. 준비 해제·최종 목적지 충돌 정책도 사용자 확정과 구분한다. 2026-09-18 동일 PC의 별도 NetDriver로 2인 → 4인 PIE를 실행하여 원래 소유자별 계획·HP/계획 복제·원격 AP/몽타주/보행·두 전투와 Host 전용 결과/상점 진행을 확인했다. 실제 인증은 시험 계정 바인딩으로 대체했으며 Steam·다중 PC·지연/손실·체감 품질은 검증하지 않았다.

### 12-2 Snapshot과 복구

상대 Party Snapshot과 실행 중 라운드/명령은 별도 데이터다. 기존 카탈로그로 스폰한 장착 스킬은 명시 RoundDefinition 또는 초기 변환을 사용한다. 원래 GAS 효과·EquipmentIds/TacticsId·Snapshot Speed가 모두 지원되는 것으로 해석하지 않는다. 적 계획은 인간 초안 이전에 고정한다.

이전 Phase Combat 저장은 로드/관리 재개를 거절하며 파일·현재 상태·lease를 바꾸지 않는다. 새 전투는 마지막 비전투 확정 기록만 재개할 수 있다. 진행 중 라운드의 시전·실제 좌표·투사체·지연 피해·난수·이미 적용된 결과 복구는 미구현이다. 중단만으로 자동 Host 승계나 AI 전환하지 않는다. 상세 결정은 [TODO 3-1절](TODO.md#3-1-전투-규칙), 사용자 확인은 [남은 확인](TODO.md#1-사용자-작동-확인)에 둔다.
