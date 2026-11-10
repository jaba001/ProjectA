# 멀티플레이 설계·개발 계약

식별·소유권·저장·네트워크 계약만 적는다. 게임 규칙은 [GAME_STATUS](GAME_STATUS.md), 구현 구조는 [PROJECT_PLAN](PROJECT_PLAN.md), 남은 작업은 [TODO 4절](TODO.md#4-온라인-협동과-경쟁)을 따른다.

## 1 현재 상태

| 범위 | 상태 |
|---|---|
| 전투 | 라운드 계획·시간차 실행. `ACombatRoundCoordinator`가 서버 계획·시각·위치·충돌·피해를 소유 |
| 같은 PC 협동 | Listen Server 2·4인 개발 협동(10전투·9상점 경로) |
| 복구 | 비전투 관리 재개와 schema 3 Ready 경계 복구. 이전 순차 Combat 저장은 거절 |
| 새 싱글 Run | 60인카운터·10PvE·10로컬 Snapshot. 기존 2/10전투 저장과 개발 협동은 원래 경로 |
| Steam | 480 개발 설정의 세션·SteamSockets·SteamAuth 관측 시제품. 기본 실행은 Steam 비활성 |
| 미구현 | PlayFab, 공유 저장, 온라인 Run 참가/재개, 관전, 결과 검증, MMR, 실제 2PC 확인 |

- 같은 PC 개발 협동 결과는 시험 계정 수동 배정과 HP 보정 fixture 기준이다. 정상 난이도·Steam 다중 PC·온라인 저장의 확인으로 확대하지 않는다.
- 진행 중 시전·투사체의 임의 시점 복원은 지원하지 않는다. [확장 경계](#12-시간차-자동-전투의-확장-경계)

## 2 확정 정책

| 항목 | 규칙 |
|---|---|
| 인원 | 최대 4명. `GameSession.MaxPlayers=4`는 Listen Host 포함 |
| 캐릭터 배정 | 협동은 인간 1인 1캐릭터, 부족 슬롯은 비움. 일반 싱글은 직접 조작 1명 + 동료 AI |
| 인간 조작 | 원래 자기 캐릭터만 조작. Host도 타인 캐릭터를 조작하지 못함 |
| 소유권 | Host·연결·AI 모드가 바뀌어도 `CharacterId`와 원래 소유자 유지 |
| 참가자 | 원래 Run 참가자 외 대체 참가 불가 |
| 최초 번호 | 최초 Host 1번, 합류 순서대로 2·3·4번. 재접속·슬롯으로 재부여하지 않음 |
| 중단 | 정상 종료·끊김만으로 Host나 AI 모드를 바꾸지 않음 |
| 명시적 재개 | 재개하는 원래 참가자 중 번호가 가장 작은 사람이 Host |
| 단독 전환 | 혼자 전환하면 본인이 Host, 나머지 원래 캐릭터는 AI |
| AI 결정 | 재개 Host가 단독 확정. 사전 동의 없음 |
| 영구 AI | 해당 Run 종료까지 AI. Human 목록·Host 후보에 다시 넣지 않음 |
| 초대·관전(목표) | 비공개 Steam 친구 초대로 새 Run 참가, 원래 참가자는 관리 Run 목록에서 재접속. AI 전환된 원래 소유자는 읽기 전용 관전 |
| 진행 결정 | 현재 Host만 노드 선택·인카운터 선택·상점 퇴장·Continue |
| 구매 | 본인 생존 Human만 구매·리롤. Host도 타인·AI에게 지급하지 못함 |
| 회복 | 누적 HP·사망 유지. 본인만 소모품 사용·부활. AI 소모품 사용과 타인 서비스 구매는 없음 |
| 복구 지점 | 마지막 저장 계획·Ready 경계(첫·다음 라운드 Planning 포함) |
| MMR(목표) | Run 종료 시 현재 Human에게만 반영 |
| Run 간 이월 | MMR만 반영. 성장·골드·장비·스킬·소모품은 이월하지 않음 |

- 명시적 재개는 확정 데이터로 새 서버를 여는 기능이며 자동 Host Migration이 아니다.
- 기존 실행이 살아 있으면 다른 참가자가 같은 기록을 획득할 수 없다. 파일 복사나 계정 입력을 승계 승인으로 쓰지 않는다.
- 구매·판매 RPC는 캐릭터 ID·상품(또는 사본) ID만 받고 서버가 연결에서 계정을 조회한다. 서버는 Human 참여·소유권·단계·잔액·Revision을 검증하고 원자 저장한다. 클라이언트는 응답 GUID와 복제 Revision을 확인한 뒤 거래 잠금을 푼다.
- `ValidateNewRunRoster`(`InitializeIdentifiedRun`·`CreateManagedRun` 공통): 새 협동 생성 시 참가자마다 한 캐릭터만 허용한다. 기존 저장의 로드·재개에는 소급하지 않는다.
- 미정: 화면 분할, AI 캐릭터의 비전투 장비·보상 결정권, 일시 끊김의 대기·시간 제한.

## 3 식별자와 저장 버전

영속 DTO에 Actor·Controller·Ability 인스턴스·실행 핸들을 저장하지 않는다.

| 식별자 | 수명과 의미 |
|---|---|
| `RunId` | Run 전체. 재개 후에도 유지 |
| `FRunAccountId` | 공급자와 불투명 Subject. 표시명·PIE PlayerId·접속 순서가 아님 |
| `JoinOrdinal` | 최초 합류 번호. 승계 순서 결정 |
| `CharacterId` / `OwnerAccountId` | 원래 캐릭터와 소유자. 이전하지 않음 |
| `HostAccountId` / `HostEpoch` | 현재 Host와 실행 세대. 관리 재개 시 epoch 증가 |
| `AttemptId` | 전투 저장 묶음. schema 3의 Ready 경계 갱신은 같은 값 |
| 체크포인트 `Revision` | 저장 성공마다 증가. `RoundNumber`·`PlanRevision`과 구분 |
| 저장소 stamp | RunId·저장소 revision·HostEpoch·SessionId |
| Runtime ID | CombatId·RoundNumber·PlanRevision·서버 유닛 ID. Run 영속 ID와 구분 |

| 저장 계층 | 지원과 호환 |
|---|---|
| Opponent Snapshot | schema 1, 카탈로그와 일치하는 ContentVersion |
| Run SaveGame v1 | 식별 정보 없는 `LegacyOffline`. 소유자·Host를 추정 이관하지 않음 |
| Run SaveGame v2 | 일반 Run의 전투 밖 진행과 식별/소유권 |
| Run SaveGame v3 | 이전 순차 Combat. 로드 거절, 파일 보존 |
| Run SaveGame v4 | 관리 진행·Human 목록·lease·Ready 경계. LocalDevelopment 참가자 2~4명 |
| Run SaveGame v5 | 일반 Run의 CombatCheckpoint schema 3 Ready 경계 |
| Run SaveGame v6 | LegacyOffline의 schema 3 Ready 경계. Standalone 복구, 식별자 생성 없음 |
| Identity schema 1 / 2 | 1은 번호 0 유지·번호순 승계 거절. 2는 1~N의 고유·연속 번호 |
| CombatCheckpoint schema 1 / 2 | 구조 검증만. 저장·복구에 쓰지 않음 |
| CombatCheckpoint schema 3 | Actor 없는 양 팀 유닛 값·라운드/수정 번호·스킬/대상·Ready·SAP 예약·차감 전 자원 |
| EncounterProgress schema 0 / 1 / 2 | 0은 상점 없는 경로, 1은 2/10전투·개발 협동, 2는 목표 Run(전투 전 인카운터 3회) |
| DungeonState schema 0 / 1 | 0은 빈 계획, 1은 지형 시드·통로 변형·후보 ID 3개 |
| TargetRun schema 0 / 1 | 1은 싱글 20전투·60선택의 고정 편성·성장·보상·로컬 Snapshot |
| GoldReward schema 0 / 1 / 2 | 0 무보상, 1 골드 3택1, 2 아이템 3택1 + 공통 골드 |
| Recovery schema 0 / 1 | 1은 회복 시험값·서비스 revision·캐릭터별 소모품 수량 |

**무기 스킬 획득 버전 1**

- 서버가 사본 GUID·등급·확정 스킬·생성 버전과 저장된 후보/태그 조건·소유권을 검증한다. 새 사본의 등급은 저장된 `CatalogRarityTag`와 일치해야 한다.
- Client는 장착 사본 인덱스·슬롯·Revision만 요청하며 스킬이나 등급을 지정하지 않는다.
- 생성 결과와 규칙을 Run에 저장해 구매·재장착·Continue로 재추첨하지 않는다. `SchemaVersion=1`·`BalanceVersion=1`·사본의 `SkillBalanceVersion/GrantedSkillBalances`는 서버가 확정한 값이다.
- 등급 확률은 `FRunItemShopState.RarityProbabilities` 스키마 1로 고정한다. 스키마 0은 균등 추첨을 유지한다.
- 버전 0과 기존 저장은 당시 카탈로그·등급·스킬·추첨 규칙을 유지하며 CSV 변경을 소급하지 않는다.
- 개별 장비 GUID·등급의 Snapshot 공개와 경쟁 서버 검증은 미구현이다.

**저장 처리**

- 식별 Run 초기화는 첫 저장과 상태 공개를 `CommitSaveCandidate`로 묶는다. 저장 실패는 생성 실패다.
- 인카운터 선택·상점 퇴장은 저장한 뒤 공개하고 실패하면 이전 상태로 되돌린다. 관리 Run은 `EncounterChoice`·`Shop`도 전투 밖 재개를 허용한다.
- `DungeonState` 계획은 읽기 전용 GameState 뷰로 복제하며 클라이언트는 추첨하지 않는다.
- `AIConsent`·`ConsentPolicyVersion`은 구버전 검증용으로만 읽는다.
- v1~3을 v4로 자동 이관하지 않는다. 전투별 AI 플래그에서 영속 Human 이력을 추정하지 않는다.
- 스킬·클래스의 동작 계약을 바꾸면 콘텐츠 버전도 검토한다. 새 저장을 이전 실행 파일로 여는 역호환은 지원하지 않는다.

## 4 서버 실행과 화면

| 구성 | 책임 |
|---|---|
| GameMode / Encounter | 서버 Arena·파티·참가자·Run 시작/종료, Host 진행 권한 |
| RunState / GameState | 영속 진행·관리 lease, 읽기 전용 진행/결과 뷰 |
| CombatManager / RoundCoordinator | 전투 등록·계획·준비·시각표·이동·공격·결과 |
| CombatActionAuthority | 캐릭터 소유권·연결·SessionId/lease·Human/AI 검증 |
| CombatRoundPlayerController | 소유 연결의 Reliable 계획/준비 RPC, 서버 응답·복제 대기 |
| Unit / Projectile | 서버 HP·위치·사망·피격체/투사체와 복제 표현 |

- `AssignRunParticipant`는 신뢰 C++가 서버 연결에 계정을 배정한다. 계정 입력·표시 이름으로 소유권을 주장할 수 없다.
- 일반 협동은 원래 참가자 전원, 관리 Run은 승인된 현재 Human 전원이 필요하다.
- 계획 요청은 CombatId·RoundNumber·PlanRevision·UnitId·SkillId·대상 ID/좌표·목적지만 전달한다. 피해량·시각은 서버 데이터에서 결정한다.
- 이전 라운드, 낡은 수정 번호, 해결 중 편집, 대체 연결의 요청은 거절한다.
- 계획 변경은 해당 소유자의 Ready만 해제한다. 전체 준비와 저장 성공 뒤 Ready Phase를 끝내고 AP/SAP를 한 번 차감한다.
- 피해·사망은 서버 순서대로 즉시 반영한다. Client의 프레임·충돌·몽타주는 피해를 확정하지 않는다.

## 5 Async PvP 상대 Snapshot

`UPartySnapshotSaveGame`에 `FPartySnapshot`을 담아 SaveGame API로 저장한다(동기 I/O). 실시간 상대 접속 없이 상대 빌드 하나로 Encounter를 만든다.

- 필드: SnapshotId·MemberId·ClassId·이름·Stats·SkillIds·EquipmentIds·TacticsId·FormationSlot·버전.
- 카탈로그가 고정 ID를 신뢰된 Enemy 클래스/스킬로 해석한다. 임의 클래스 경로는 실행하지 않는다.
- 1~4명, FormationSlot 0~3, 스킬 1개 이상(상한 없음). ID·배치·스킬 중복과 별칭 중복을 거절한다.
- `ResolveSkills`는 `RunRecoveryRules::IsConsumable`로 `Item.Consumable` 태그 스킬을 거절한다.
- 검증 상한: HP 1,000,000, AP/SubAP 100, 이동 범위 32.
- EquipmentIds·TacticsId는 빈 값만 지원한다. HP 0은 저장 가능하나 Encounter에서는 거절한다.
- FormationSlot을 `Arena.EnemyCoords`로 해석해 전체 검증한 뒤 스폰한다.
- 실패하면 오류와 함께 준비를 정리하며 PvE 적으로 대체하지 않는다. 잘못된 저장/로드는 기존 파일을 보존한다.
- 적 아이템 AI는 없다. 장착을 무작위로 바꾸거나 회복약을 지급하지 않는다.
- 목표 Run은 생성 시 10개 상대 값을 Run 본문에 고정한다. 외부 `.sav` 변경으로 상대가 바뀌지 않는다.

**상대 선정 API (로컬 개발, 기본 Run 미연결)**

- `UPartySnapshotSelectionLibrary::SelectOpponent`: 진행 단계·콘텐츠 버전·GameplayTagQuery와 생존/배치/스킬 검증을 통과한 후보 중 가중치 1로 추첨한다. 중복 Snapshot ID는 거절한다. 실패 시 출력·난수 상태를 보존한다.
- `LoadAndSelectOpponent`: `FPartySnapshotSlotCandidate`(슬롯 ID·진행 단계·태그)를 받아 조건에 맞는 파일만 읽는다. 누락·손상이면 실패하며 후보를 빼고 다시 추첨하지 않는다. 원본 슬롯은 쓰지 않는다.
- 호출자가 준 진행·태그는 인증된 게시 정보가 아니다.

**온라인(제안 12 선택 1, 미구현)**

- 지원 범위의 생존 상대·저장 HP·스킬·배치를 게시하고, 같은 콘텐츠 버전·진행 단계의 태그 조건 후보 중 균등 추첨한다.
- 게시 시점·보존 기간·중복 상대·버전 변경·소모 반영은 미정이다.

## 6 준비 완료 경계와 파일 저장

- 저장 시점: Map·Result·EncounterChoice·Shop·종료 결과, 첫·다음 라운드 Planning, 준비 완료·수정·취소.
- 서버가 저장 성공을 확인한 뒤 Ready를 공개한다. 실패하면 이전 파일·계획·준비를 유지하고 전투를 시작하지 않는다.
- 전원 준비 경계는 비용 차감 전 상태로 저장하고 Ready Phase 종료 시 비용을 한 번 차감한다. 강제 종료 후 이 경계에서 다시 실행한다.
- 소모품도 같은 경계를 따른다. 회복 발동 성공 시 수량을 한 번 차감하고 다음 계획 경계와 결과에서 HP·수량을 함께 저장한다. 서비스 구매는 서비스 revision으로 중복 요청을 거절한다.
- 일반 Continue는 v5 또는 LegacyOffline v6, 관리 Resume는 v4의 schema 3 Ready 경계를 복구한다. LegacyOffline은 Standalone에서만 복원한다.
- 이전 순차 Combat는 참가자·HostEpoch·lease·기준 저장 변경 전에 거절한다. `CompletedTurnSerial`·`NextTurnIndex`는 호환 필드다.
- 승패와 Continue는 저장 성공 뒤 공개한다.
- 파일 쓰기: USaveGame 직렬화 → 같은 디렉터리 임시 파일 flush·바이트 검사 → Win64 파일 교체. 기존 파일을 먼저 지우지 않는다. 로컬 어댑터는 온라인 정본 권위를 보장하지 않는다.

## 7 관리 v4 재개와 lease

- `FRunParticipationData.HumanParticipants`가 현재 Human인 원래 소유자를 전투 밖에도 보존한다. 목록에 없는 원래 캐릭터는 AI다. 사망만으로 목록을 바꾸지 않는다.

| API | 계약 |
|---|---|
| `ConfigureLocalDevelopmentCaller` | 신뢰 C++가 Development 계정·namespace 주입. 같은 GameInstance에서 변경 불가 |
| `CreateManagedRun` | Host 1번·epoch 1·원래 2~4명 전원 Human. 기록·lease 생성 성공 뒤 적용 |
| `ReadManagedRun` | 최신 본문/stamp 조회. 권한·현재 Run을 바꾸지 않음 |
| `ResumeManagedRun` | 닫힌 실행·최신 stamp·Human 부분집합·번호순 Host 검증 후 새 본문/세대/lease 획득 |
| `ConfirmManagedResumeStarted` | Gameplay 복구 성공 뒤 pending 해제. 실패하면 유지 |
| `BeginManagedMenuTravel` | 같은 SessionId·WorldContext의 TravelFailure 감시 |
| `CloseManagedRun` | 전투/콜백 중단 후 lease·활성 상태 해제 |

- 로컬 저장소는 같은 PC의 공유 경로에서 배타적인 OS 파일 핸들을 유지하고, 별도 트랜잭션 잠금으로 읽기·stamp 비교·본문 교체를 직렬화한다.
- Create/Acquire/Commit 실패는 기존 기록·lease를 유지한다. 충돌 후 일반 SaveSlot으로 우회하지 않는다.
- 프로세스가 종료되어도 Host는 자동으로 바뀌지 않는다. 명시적 Resume에서만 새 epoch·SessionId를 만든다.
- 현재 Human이 Host 한 명이면 Standalone으로 복구한다. Listen Server는 현재 Human 전원이 배정될 때까지 멈추며 자동 AI 전환하지 않는다.
- pending 중에는 Human 명령·노드·Continue를 막는다.
- 정상 종료 순서: `ShutdownGameplay → CloseManagedRun`.
- Authority는 관리 실행 여부를 `Reset` 후에도 유지하고 lease·Identity·Host·SessionId를 재검사한다.
- 이 저장소는 같은 PC 개발용이며 중앙 저장·계정 인증·네트워크 장애 판정이 아니다.

## 8 메뉴와 아군 AI

- 일반 이어하기: v1 오프라인, 단일 참가자 LocalDevelopment v2의 비전투 상태, v5/v6의 Ready 경계를 허용한다. 클릭 시 본문을 다시 검증한다. v3와 협동·AccountProvider·v4는 Standalone으로 우회하지 않는다.
- 새 싱글은 목표 Run의 첫 인카운터 선택으로 진입한다. `-ProjectAPrototypeRun`(비 Shipping/Test)은 기존 10전투 경로를 쓴다.
- 관리 패널은 신뢰 개발 경로가 호출자·재개 대상을 설정했을 때만 표시한다. 메뉴 이동 실패는 확정 Host/AI 기록을 되돌리지 않고 lease를 해제해 재시도를 허용한다.
- 로컬 개발 협동은 새 비관리 Listen 방을 만들고 참가자별 캐릭터 한 명만 만든다. 주소 참가는 소유자 인증이 아니다.
- AI: RoundCoordinator가 인간 초안 이전에 적·ServerAI 아군의 명령을 한 번 고정한다. 가까운 적과 단일 공격을 고르며 타일 접근·지원·이동 전술은 후속이다.
- 온라인 로그인·관리 저장 검색·수동 협동 재개 UI는 미구현이다.

## 9 Steam·PlayFab와 남은 서비스 정책

- 기준: Steam + PlayFab, 전투 연결은 Unreal Listen Server·Steam P2P/SDR. 운영비 회피가 조건이다.
- 전투 서버를 임대하지 않아도 공동 Run 저장·승계 승인·결과 검증·MMR의 비용과 권위는 별도다.
- Steam Cloud는 같은 사용자의 PC 간 동기화이며 참가자 간 단일 최신 Run을 확정하지 않는다. Steam Leaderboards는 전투 정당성을 검증하지 않는다.
- publisher key·Web API Key·Secret Key를 Listen Host 클라이언트·저장소·Git·채팅에 넣지 않는다.
- 개발 무료 한도를 영구 무료 운영으로 보지 않는다. 유료 리소스·과금 전환을 임의로 켜지 않는다.
- 로그인 성공·표시명·PIE ID·Client Verified 플래그는 서버 인증 증거가 아니다. 서버가 검증한 티켓 계정을 연결에 대응한다.
- 온라인 실패를 Development 계정이나 로컬 저장 성공으로 대체하지 않는다.
- 참고: [Steam 인증](https://partner.steamgames.com/doc/features/auth?l=english), [Steam Cloud](https://partner.steamgames.com/doc/features/cloud?l=english), [PlayFab 개발 모드](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode).

| 제안 | 채택 방향 | 미정 |
|---|---|---|
| 11 정본 저장 | PlayFab을 쓰는 신뢰 서비스가 Run 본문·소유권·revision/epoch·lease를 함께 승인. 정확한 이전 revision/epoch의 한 갱신만 승인 | 조건부 갱신·무료 범위 |
| 13 MMR | 공격 Run의 PvP 승패를 합산해 파티 공통 변동량을 현재 Human에게 한 번 적용. Snapshot 방어는 통계만 | 계산식·초기 평점·동률·미확정 결과·이탈 악용 |
| 14 결과 검증 | 서비스가 확정 명령·시드·콘텐츠 버전·판정 입력을 저장하고 전투 종료 후 재검증 | 재현 가능한 판정 범위·실패/이탈 처리 |

- 같은 요청 ID·같은 본문은 기존 결과를 조회하고, 같은 ID·다른 본문은 거절한다. 전투 Attempt 결과와 MMR은 각각 한 번만 반영한다.
- 로컬 전투의 물리 판정까지 결정성이 확보된 것으로 보지 않는다.

## 10 T14-8 서비스 준비

제안 9 선택 2(Steam 480 개발 연결)와 제안 10 선택 1(친구 초대)을 채택했다. 서로 다른 계정의 PC 2대와 PlayFab Title은 미준비다.

### 10-1 비용과 준비 순서

1. [PlayFab Game Manager](https://developer.playfab.com/)에서 Studio와 Title(`ProjectA-Dev` 등)을 만들고 Title ID와 서비스 모드를 확인한다. [시작 안내](https://learn.microsoft.com/en-us/xbox/playfab/live-service-management/gamemanager/quickstart)
2. 무료 모드와 사용량 조건을 확인한다. [Foundation 온보딩](https://learn.microsoft.com/en-us/xbox/playfab/get-started/foundation-onboarding)은 Xbox 출시 계획 등을 전제하므로 자격을 추정하지 않는다. 유료 전환만 보이면 비용을 확인한 뒤 방향을 정한다.
3. 개발 연결은 공유 테스트 ID `480`을 쓴다([Epic App ID 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine), [SpaceWar 예제](https://partner.steamgames.com/doc/sdk/api/example)). 출시는 자체 App ID가 필요하다.
4. 자체 ID는 [Steamworks 온보딩](https://partner.steamgames.com/doc/gettingstarted/onboarding)을 사용자가 진행한다. [Steam Direct 수수료](https://partner.steamgames.com/doc/gettingstarted/appfee)는 앱당 USD 100 상당이다.
5. App ID·Title이 준비되면 PlayFab의 Steam 연동에 앱 ID와 Web API Key를 연결한다. Steam 티켓 로그인은 [공식 API](https://learn.microsoft.com/en-us/xbox/playfab/api-references/c/pfauthentication/functions/pfauthenticationloginwithsteamasync)를 따른다. CustomID 로그인이나 로그인 성공을 결과 검증으로 대신하지 않는다.

### 10-2 로컬 엔진 점검과 다음 구현

| 대상 | 현재 설정 |
|---|---|
| 프로젝트 | `ProjectA.uproject`·Build.cs에 OnlineSubsystemSteam/SteamSockets를 Win64·비 Shipping/Test로 명시. PlayFab은 비활성 |
| 기본 실행 | `Config/DefaultEngine.ini`의 Steam `bEnabled=false` |
| 개발 옵션 | `Config/Custom/SteamDev/DefaultEngine.ini`가 Steam·480·SteamAuth·SteamSockets 설정. `-CustomConfig=SteamDev -ProjectASteamDev`로 활성화 |
| P2P transport | `/Script/SteamSockets.SteamSocketsNetDriver`를 기본·fallback으로 요구 |
| IP 전용 경로 | `SocketSubsystemSteamIP`는 NAT punchthrough를 제공하지 않음. SteamSockets와 구분 |
| PlayFab | 엔진의 `PlayFabParty` 플러그인은 기본 비활성. SDK 도입 시 UE 5.8 지원 버전을 먼저 고정 |
| 관리 저장 | `FLocalRunAuthorityStore`는 같은 PC 파일/OS lease만 지원. Steam 계정 ID만으로 온라인 관리 Run으로 승격하지 않음 |

- `USteamDevelopmentSubsystem`: 세션 생성·검색·참가·종료와 친구 초대 수락. 2~4명, `PROJECTA_PROBE` 표식·빌드 ID·lobby 설정을 검사하고 서버가 친구 관계를 확인한다. `Core/MainMenu`를 연결 전용 GameMode로 연다. Run 생성·저장 조회·승계·관전 배정은 하지 않는다.
- 친구 전용 로비는 `ReadFriendsList(InGamePlayers)` 후 친구마다 `FindFriendSession`을 순차 호출해 찾는다. 요청 번호로 오래되거나 중복된 완료를 거절한다.
- 인증은 공식 `FOnlineAuthUtilsSteam::OnAuthenticationResultDelegate`의 성공 판정만 인정하고 기본 인증 실패 추방을 유지한다. Steam 로그인·UniqueNetId 문자열은 인증 완료가 아니다. [SteamAuth 계약](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine#steam-online-authentication)
- 실패는 명시적 실패로 남기며 Development 계정으로 치환하지 않는다.
- 다음 단계: 서로 다른 PC·Steam 친구 계정으로 접속·인증 실패·초대·종료 확인 → 검증된 계정을 Run 소유권에 연결 → 승인된 PlayFab 환경에서 정본·lease·결과 검증 구현.

## 11 개발 실행 참조

프로젝트 루트 PowerShell 기준이다.

```powershell
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$editorCmd = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = "$PWD/ProjectA.uproject"
& $editorCmd $project -run=pythonscript -EnablePlugins=PythonScriptPlugin "-script=$PWD/Source/ProjectAEditor/Scripts/ConfigureSnapshotContent.py" -unattended -nullrhi -nop4
& $editor $project -ProjectAPrototypeRun -ProjectAOpponentSnapshot=SampleOpponent
```

- 샘플 생성은 로컬 에셋·저장을 쓰므로 최초 준비 때만 실행한다. 샘플은 Hunter 1명·최대 HP 140/현재 120·AP 2/SubAP 1·이동 1·배치 0·DefaultAttack이다.
- SlotId는 영문·숫자·밑줄 1~64자, 저장 이름은 `ProjectA_Opponent_<SlotId>`. 샘플 `.sav`는 Git·패키지에 넣지 않는다.
- Snapshot Run은 `ProjectA_SnapshotRun_<SlotId>`, PvE는 `ProjectA_Run`. `-ProjectASaveSlot=...`이 우선한다.
- 이 명령은 개발 프로토타입이다. 기본 메뉴의 목표 Run은 Run 본문에 고정된 로컬 Snapshot을 쓴다.

Steam 시제품(Steam 친구인 서로 다른 계정·PC 2대에서 각각 실행):

```powershell
& $editor $project /Game/User_JeHoon/LEVEL/Core/MainMenu -game -CustomConfig=SteamDev -ProjectASteamDev -windowed -ResX=1280 -ResY=720
```

- 메뉴의 Steam 연결 확인 패널에서 방 생성, 검색 참가 또는 초대 수락, 연결/인증 수, 나가기를 확인한다.
- 패널의 성공은 연결·인증 범위에 한정한다. 일반 작업에서는 두 옵션을 뺀다.

이전 순차 전투용 자동화(현재 전투의 통과 근거로 쓰지 않는다):

```powershell
& $editorCmd $project -unattended -nop4 -RenderOffscreen -nosound '-ExecCmds=Automation RunTests ProjectA.Coop.' '-TestExit=Automation Test Queue Empty'
& $editorCmd $project -unattended -nop4 -RenderOffscreen -nosound -T14ManagedRunPIE '-ExecCmds=Automation RunTests ProjectA.ManagedRunPIE.' '-TestExit=Automation Test Queue Empty'
```

| 필터/옵션 | 범위 |
|---|---|
| `ProjectA.Coop.ListenServer{Client,ThreePlayer,FourPlayer}Combat` | 2/3/4인 소유자 RPC·전투 상태·Host 진행 권한 |
| `ProjectA.Coop.CheckpointSessionRestart[3Players\|4Players]` | 끊김 후 원래 참가자 전원의 새 세션 복구 |
| + `-T14CheckpointAI` | 소유자도 연결된 AI 모드 복구 |
| + `-T14CheckpointOpponent=Replace\|Delete` | 원본 상대 변경/삭제 후 고정 본문 복구 |
| `ProjectA.ManagedRunPIE.HostSuccession[4Players]` | 2번 Host 승계·1번 AI |
| `ProjectA.ManagedRunPIE.SoloMenuConversion` | 4번의 메뉴 단독 전환·영구 AI |
| `ProjectA.Run.Managed` / `ProjectA.Checkpoint` | 관리 저장/전이/호환, 확정 경계/손상/거절 |
| `ProjectA.Persistence.CombatProcessRestart` | Writer `-T14WriteCombatCheckpoint` → Reader `-T14ReadCombatCheckpoint`, 공통 `-T14CheckpointSlot=...` |
| `ProjectA.RunAuthority.ProcessProbe` | `-T14AuthorityProbe=Holder\|BusyReader\|ResumeReader`, 공통 `-T14AuthorityNamespace`·`-T14AuthorityRunId`·`-T14AuthorityProbeId` |

- 관리 PIE는 `-T14ManagedRunPIE`가 없으면 안내만 출력한다.
- 정원 검사의 `ApproveLogin` 호출은 실제 다섯 번째 클라이언트 접속 시험이 아니다.

## 12 시간차 자동 전투의 확장 경계

### 12-1 서버 실행과 조작권

- 서버가 계획·전체 잠금·0.01초 실행·좌표·충돌·피해·사망·잔여 공격 종료를 확정하고 Client에 복제한다. 규칙은 [GAME_STATUS 6절](GAME_STATUS.md#6-라운드-전투).
- Client의 충돌 통지·애니메이션 알림·명중 확률로 서버 결과를 대체하지 않는다.
- 같은 예정 시각도 서버 순서대로 처리한다. 라운드 전체의 원자적 동시 성립이나 물리 충돌을 포함한 결정성은 보장하지 않는다.
- 수정한 소유자만 Ready를 해제하며 다른 유닛의 복귀·예약 칸 이동과 자리 교환은 금지한다.
- 같은 PC 개발 경로의 결과를 Ready 강제 종료 후 Actor 복구, Steam 다중 PC, 지연/손실의 통과 근거로 쓰지 않는다.

### 12-2 Snapshot과 복구

- 상대 Party Snapshot과 실행 중 라운드/명령은 별도 데이터다. 적 계획은 인간 초안 이전에 고정한다.
- 원래 GAS 효과·EquipmentIds/TacticsId·Snapshot Speed가 모두 지원되는 것으로 해석하지 않는다.
- 이전 순차 Combat 저장은 로드·관리 재개를 거절하고 파일·상태·lease를 바꾸지 않는다.
- 새 전투는 마지막 저장 계획·Ready·양 팀 유닛·자원·배치·적 계획을 복구해 비용 차감 전 경계에서 다시 실행한다.
- 중단만으로 Host 승계나 AI 전환을 하지 않는다. 온라인 정본 저장·경쟁 재실행 방지는 미구현이다.
