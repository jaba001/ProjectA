# 멀티플레이 설계·개발 계약

[프로젝트 안내](../README.md) · [과거 구현·검증 이력](HISTORY.md) · [온라인 확장 계획](TODO.md#4-온라인-협동과-경쟁)

아이템 판매는 소유 연결의 RPC를 통해 서버가 원래 소유자·Human·생존·아이템 상점·사본·두 Revision을 검증하고 원자 저장한다. 클라이언트는 응답 GUID와 장비/상점 복제 Revision을 함께 확인한 뒤 거래 잠금을 해제한다. 이 경로의 2인 실행·지연·저장 실패 검수는 미수행이며 [상점 계약](PROJECT_PLAN.md#3-2-상점-인카운터)과 [사용자 확인](TODO.md#26-재개-후-로컬-검수와-저장-보완)을 따른다.

## 1 현재 상태

엔진 기준은 UE 5.8.3이다. 레벨 폴더 이동 전 2026-10-04 실행 결과는 [UE 5.8 검증 이력](HISTORY.md#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록한다. 이동 후 새 게임·기존 저장 UUID 사본의 실제 이어하기는 사용자 위임으로 통과했다. 2026-10-04 후속 구현과 검수는 [목표 Run 검증 이력](HISTORY.md#9-18-2026-10-04-목표-run과-위임-후속-검수)을 따르며 이전 성공을 최신 코드의 작동 확인으로 대신하지 않는다.

기본 Combat는 라운드 계획·시간차 실행으로 교체했다. `ACombatRoundCoordinator`가 서버 계획·시각·위치·충돌·피해를 소유하며 기존 Listen Server·원래 캐릭터 소유권·관리 lease를 연결한다. 순차 턴·연속 AI 실행과 턴 저장 복원은 폐기했다. 2026-10-08 최신 코드의 동일 PC 1/2/4인 `RunRoundPIE`가 각각 10전투·9상점을 통과했다. 1인은 Standalone, 2/4인은 실제 Listen Server·Client NetDriver를 사용하여 원래 소유자의 계획·HP/AP·몽타주·보행 복제, 개인 골드 보상·Host 진행·결과 저장 재로드를 확인했다. 반복 `Shop_02` 방문은 각 Host/Client의 NPC 카메라·저장 제목·퇴장 후 전경 복귀도 검사했다. 시험 계정 수동 배정과 아군 HP 10000·적 HP 보정의 개발 fixture이므로 정상 난이도·메뉴 lobby·Ready 강제 종료 후 Actor 재구성·Steam 다중 PC·온라인 저장의 확인으로 확대하지 않는다. 실행 근거·경고·제한은 [HISTORY 9-33](HISTORY.md#9-33-2026-10-08-위임-실행-검수와-협동-보완), 남은 범위는 [온라인 확장 계획](TODO.md#4-온라인-협동과-경쟁)을 따른다. 이전 UE 5.7·폴더 이동 전 결과는 [당시 검증 이력](HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)과 구분한다.

이전 ff22940의 승계 PIE·관리 계약 성공은 순차 전투 이력이다. 비전투 관리 재개와 새 schema 3의 Ready 경계 복구를 지원하며 이전 순차 Combat 저장은 본문·Host·참가자·lease 변경 전에 거절한다. 강제 종료 후 마지막으로 저장한 준비 완료 상태를 복구하며 진행 중 시전·투사체의 임의 시점 복원은 지원하지 않는다. [상세 경계](#12-시간차-자동-전투의-확장-경계)

기본 새 싱글 Run은 60인카운터·10PvE·10로컬 Snapshot의 목표 경로를 사용한다. 기존 2/10전투 저장과 개발 협동은 원래 경로를 유지한다. 목표 Run과 회복 기능의 컴파일·실행 이력은 [HISTORY](HISTORY.md#9-19-2026-10-04-todo-재개-검수와-저장제목-보완), 남은 확인은 [TODO](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 구분하며 이전 10전투 시험을 80단계 정상 완주 근거로 사용하지 않는다.

Steam 480의 세션·SteamSockets·공식 SteamAuth 결과 관측 코드는 별도 개발 설정으로 구현했다. 기본 실행은 Steam 비활성이며 실제 서비스 초기화·2대 PC 인증/접속은 아직 확인하지 않았다. PlayFab·공유 저장·온라인 Run 참가/재개·관전·결과 검증·MMR은 미구현이다. 사용자 선택과 준비 조건은 [온라인 확장 계획](TODO.md#4-온라인-협동과-경쟁)을 따른다.

## 2 확정 정책

새 일반 Target Run은 제안 15~19 선택 1과 추가 결정 위임에 따라 스킬상점을 제외하고 무기 스킬 획득 버전 1을 사용한다. 무기 스킬 도입 전 Run·명시적 prototype·개발 협동은 버전 0으로 기존 상점·습득 목록을 유지한다. 총 스킬 상한은 서버 구매·유닛·Run 저장·Snapshot·체크포인트에서 제거했다. 새 카탈로그의 5색 등급은 CSV의 에셋별 값으로 고정하며 해당 등급과 무기 태그에 맞는 스킬 1개를 추첨한다. 가격 1G·비무장 기본 공격·시작 무기 생성 시 부여를 유지하며 새 일반 Run은 별도 CSV의 스킬 고유 등급·수치·장비별 가중치를 고정한다. 새 일반 Run의 아이템상점은 생성 당시 저장한 등급별 확률을 사용하고 기존 저장은 당시 카탈로그·등급·스킬·상품 추첨 규칙을 보존한다. 서버 저장/표시 경로 구현과 실제 협동·경쟁 지원 검증을 구분한다. [기획 기준](GAME_DESIGN.md#2-4-무기-랜덤-스킬과-아이템-등급) · [최신 구현 이력](HISTORY.md#9-27-2026-10-08-아이템-등급별-등장-확률)

| 항목 | 규칙 |
|---|---|
| 인원 | 최대 4명. 기본 `GameSession.MaxPlayers=4`는 Listen Host를 포함한다 |
| 캐릭터 배정 | 협동은 인간 1인 1캐릭터이며 부족 슬롯은 비운다. 일반 싱글은 직접 조작 1명과 생성한 나머지 동료 AI를 사용한다 |
| 인간 조작 | 각 플레이어는 원래 자기 캐릭터만 조작한다. Host도 타인 캐릭터를 직접 조작하지 못한다 |
| 소유권 | Host·연결·AI 모드가 바뀌어도 `CharacterId`와 원래 소유자는 유지한다 |
| 참가자 | 원래 Run 참가자 외 대체 참가자는 허용하지 않는다 |
| 최초 번호 | 최초 Host는 1번, 최초 합류 순서대로 2·3·4번. 재접속 순서나 파티 슬롯으로 다시 부여하지 않는다 |
| 중단 | 정상 종료·연결 끊김만으로 Host나 AI 모드를 바꾸지 않는다 |
| 명시적 재개 | 현재 Human으로 재개할 원래 참가자 중 최초 번호가 가장 작은 사람이 Host가 된다 |
| 단독 전환 | 2·3·4번이 혼자 전환하면 본인이 Host, 나머지 원래 캐릭터는 AI가 된다 |
| AI 결정 | 재개 Host가 단독 확정한다. 개인별 사전 동의는 요구하지 않는다 |
| 영구 AI | 해당 Run 종료까지 AI를 유지한다. 이후 Human 목록이나 Host 후보에 다시 넣지 않는다 |
| 초대·관전 목표 | 비공개 Steam 친구 초대로 새 Run에 참가하고 원래 참가자는 관리 Run 목록에서 재접속한다. AI 전환된 원래 소유자는 읽기 전용 관전만 허용하며 계획·Ready·구매·Continue 권한을 복원하지 않는다. 실제 Run 연동은 후속이다 |
| 진행 결정 | 현재 Host만 노드 선택·승리 Continue를 실행한다 |
| 현재 스킬·회복 상점 | 폐지 구현 전의 계약. 직접 조작 캐릭터별 개인 10G·기존 스킬/HP 전체 회복 각 1G 시험값. 생존한 본인 인간 캐릭터만 구매하며 Host도 타인·AI에게 지급하지 못한다. 회복·잔액은 함께 저장하고 만피 구매는 거절한다. 상점 선택·퇴장은 Host 결정 |
| 목표 Run 회복 | 누적 HP·사망을 유지한다. 생존한 본인은 전투 중 회복 소모품을 사용하며 사망한 본인은 부활 인카운터에서 복귀한다. 시험값은 시작 1개·AP 1·회복 25HP·구매/회복 각 1G, 부활은 최대 HP 25%·1G다. AI 소모품 사용과 타인·AI 서비스 구매권은 추가하지 않는다 |
| 현재 복구 지점 | 새 라운드의 마지막 저장 계획·Ready 경계. 첫 라운드·다음 라운드 Planning도 저장. 이전 순차 Combat 저장 재개 미지원 |
| 경쟁 목표 | Run 종료 시 현재 Human 참여자에게만 MMR을 반영하고 불참자에게 추가 변동을 적용하지 않는다. 실제 계산·연동은 후속이다 |
| Run 간 이월 | Run 종료 후에는 MMR만 반영하며 성장·능력치 보정·골드·장비·습득 스킬·소모품을 다음 Run에 이월하지 않는다. 진행 중 재개·상대 Snapshot·검증용 기록 보존은 별도 계약을 따른다. [기획 기준](GAME_DESIGN.md#2-1-성장경제보상과-run-종료) |

명시적 재개는 확정 데이터로 새 서버를 여는 기능이다. 실행 중인 Listen Server를 즉시 옮기는 자동 Host Migration은 아니다.
기존 실행이 살아 있으면 다른 참가자가 같은 기록을 획득할 수 없다. 로컬 파일 복사나 계정 입력을 승계 승인으로 사용하지 않는다.
스킬 구매 RPC는 캐릭터 ID·상품 ID만 받고 서버 연결에서 원래 계정을 조회한다. 서버는 Human 참여·소유권·상점 단계·잔액·중복을 검증한 뒤 골드와 습득 목록을 함께 저장한다. 관리 Run은 현재 Host lease를 사용하며 AI 전환자의 잔액·스킬은 보존하지만 구매권은 부여하지 않는다. [상점 구현](PROJECT_PLAN.md#3-2-상점-인카운터) · [검증 범위](HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)

협동 1인 1캐릭터·빈 슬롯 유지와 AI 전환된 원래 소유자의 읽기 전용 관전은 2026-10-04 채택 정책이다. 화면 분할, 스킬 상점 외 AI 캐릭터의 비전투 장비·보상 결정권은 미정이다. 관전 정책의 채택은 온라인 관전 구현 완료를 뜻하지 않는다.

새 협동 Run은 `InitializeIdentifiedRun`과 `CreateManagedRun`에서 공통 `ValidateNewRunRoster`로 원래 참가자마다 한 캐릭터만 생성되었는지 검사한다. 남는 슬롯은 비우며 한 참가자에게 추가 캐릭터를 배정한 새 생성 요청은 거절한다. 기존 저장의 소유권·파티를 수정하거나 로드·재개에 이 생성 제한을 소급 적용하지 않는다. 일반 싱글의 직접 조작 한 명과 AI 동료 규칙은 유지한다.

## 3 식별자와 저장 버전

무기 스킬 획득 버전 1에서는 서버가 사본 GUID·등급·확정 스킬·생성 버전과 저장된 후보/태그 조건·원래 소유권을 검증한다. 새 사본의 등급은 저장된 `CatalogRarityTag`와 일치해야 하며 기준 태그가 없는 기존 저장은 당시 무작위 등급 정책을 유지한다. Client는 장착 사본 인덱스·슬롯·Revision만 요청하며 스킬이나 등급을 지정하지 않는다. 생성 결과와 당시 규칙을 Run에 저장하여 구매·재장착·Continue로 재추첨하지 않는다. `InnateSkills`와 현재 장착 사본을 합치며 동일 Skill ID는 한 번만 실행 목록에 넣고 사본별 출처를 보존한다. 개별 장비 GUID/등급의 Snapshot 공개·내보내기와 경쟁 서버 검증은 미구현이며 신규 무기 규칙을 실제 온라인 협동에서 검증한 것으로 기록하지 않는다.

새 일반 Run의 무기 스킬 규칙은 `SchemaVersion=1`을 유지하고 `BalanceVersion=1`에 62후보의 스킬 고유 등급·위력·AP/SAP·선딜과 장비별 25개 등급 가중치를 고정한다. 사본의 `SkillBalanceVersion/GrantedSkillBalances`는 서버가 확정한 값이며 Client는 이를 읽어 표시한다. 기존 버전 0은 당시 풀·원본 수치를 유지하고 현재 CSV로 사본을 다시 생성하지 않는다. 전투와 체크포인트 검증·복구는 현재 Run의 고정 수치를 양 팀의 같은 SkillId에 동일하게 적용한다. Snapshot 자체의 기록된 스킬 목록·캐릭터 수치는 변경하지 않으며 상대 Run의 과거 수치별 경쟁 재현·온라인 검증 보장을 추가하지 않는다. [수치·확률 계약](GAME_DESIGN.md#2-4-6-스킬-등급과-장비별-추첨-확률) · [구현·검증 상태](HISTORY.md#9-37-2026-10-09-스킬-수치등급과-장비별-추첨)

등급별 등장 확률은 새 일반 Run 생성 시 `FRunItemShopState.RarityProbabilities`에 별도 스키마 1로 고정한다. 기본값 흰색 50%·초록색 30%·파란색 15%·보라색 4%·주황색 1%와 적격 후보에 따른 재정규화는 [기획 기준](GAME_DESIGN.md#2-4-5-아이템-등급별-등장-확률)을 따른다. 등급 확률 상태가 없는 이전 저장은 에셋별 고정 등급 도입 이후 생성분도 확률 스키마 0·빈 목록으로 당시 아이템 균등 추첨을 유지한다. 이미 확률을 저장한 Run은 그 값을 유지하며 CSV 변경을 소급 적용하지 않는다. 명시적 prototype·개발 협동과 지정된 직업별 시작 장비의 선택은 변경하지 않는다.

ID·버전은 아래 계층별로 구분한다. 영속 DTO에 Actor·Controller·Ability 인스턴스·실행 핸들을 저장하지 않는다.

| 식별자 | 수명과 의미 |
|---|---|
| `RunId` | 같은 Run 전체를 식별하며 재개 후에도 유지 |
| `FRunAccountId` | 공급자와 불투명 Subject. 표시명·PIE PlayerId·접속 순서가 아니다 |
| `JoinOrdinal` | 최초 합류 번호. 승계 후보 순서를 결정 |
| `CharacterId` / `OwnerAccountId` | 원래 캐릭터와 소유자. AI·Host 변경으로 이전하지 않음 |
| `HostAccountId` / `HostEpoch` | 현재 Host와 실행 세대. 명시적 관리 재개 시 epoch 증가 |
| `AttemptId` | 전투 저장 묶음의 식별자. schema 3의 Ready 경계 갱신은 같은 값을 유지하며 이전 schema와 구분 |
| 전투 체크포인트 `Revision` | 저장 성공마다 증가하는 본문 순번. `RoundNumber`·`PlanRevision`과 구분 |
| 저장소 stamp | RunId·저장소 revision·HostEpoch·SessionId. 저장소 revision은 지도/결과/재개를 포함한 모든 저장 순번 |
| Runtime ID·수정 번호 | 새 CombatId·RoundNumber·PlanRevision, 서버 유닛 ID·소유 연결. Run 영속 ID와 구분 |

| 저장 계층 | 지원과 호환 |
|---|---|
| Opponent Snapshot | schema 1, 카탈로그와 일치하는 ContentVersion. Run 저장과 별개 |
| Run SaveGame v1 | 식별 정보 없는 `LegacyOffline`. 소유자·Host를 추정 이관하지 않음 |
| Run SaveGame v2 | 일반 Run의 전투 밖 진행과 식별/소유권 |
| Run SaveGame v3 | 이전 순차 Combat 본문. 현재 Continue/로드 거절, 파일 보존 |
| Run SaveGame v4 | 관리 진행·영속 Human 목록·lease와 새 Ready 경계 유지. 이전 순차 Combat 재개 거절. LocalDevelopment 원래 참가자 2~4명 |
| Run SaveGame v5 | 일반 Run의 CombatCheckpoint schema 3 Ready 경계. 비용 차감 전 계획·준비·전투 상태 저장 |
| Run SaveGame v6 | Identity 없는 LegacyOffline의 Combat/schema 3 Ready 경계. 비전투는 v1 유지, 기존 파티 슬롯으로 Standalone 복구. Host·소유권·식별자 생성 없음 |
| Identity schema 1 | 최초 번호 0을 유지하며 번호순 승계를 거절 |
| Identity schema 2 | 1~참가자 수의 고유하고 연속된 최초 번호. 최초 epoch 1의 Host는 1번 |
| CombatCheckpoint schema 1 / 2 | 과거 Human/ServerAI 저장의 구조 검증만 유지. 현재 라운드 저장/복구에는 사용하지 않음 |
| CombatCheckpoint schema 3 | Actor 없는 양 팀 유닛 값·RoundUnitId·라운드/수정 번호·스킬/대상·Ready·SAP 예약·차감 전 자원. 일반/관리 Ready 경계 복구 |
| Run EncounterProgress schema 0 / 1 | 0은 기존 상점 없는 경로. 1은 기존 2/10전투·개발 협동의 상점 제시 목록·선택 ID·퇴장 상태 |
| Run EncounterProgress schema 2 | 목표 Run 전용. 각 전투 전에 인카운터 3회, 방문마다 후보 3개·선택·퇴장·방문 순서를 저장 |
| DungeonState schema 0 / 1 | 0은 기존 고정 배치·빈 계획. 1은 새 Run의 별도 지형 시드·전체 방문의 통로 변형과 순서 있는 후보 ID 3개를 저장. 바깥 저장 파일 버전은 유지 |
| TargetRun schema 0 / 1 | 0은 기존 저장의 빈 기본값. 1은 싱글 20전투와 60선택, 고정 편성·성장·보상·로컬 Snapshot 및 카탈로그 ID 매핑을 저장 |
| GoldReward schema 0 / 1 / 2 | 0은 과거 무보상 상태, 1은 골드 3택1과 개인 수령 기록. 2는 무기 스킬 획득 버전 1 일반 Target의 다음 PvE 승리부터 아이템 3택1·공통 골드·수령 기록을 저장. 이미 저장된 0/1 결과와 개발 협동의 골드 정책을 보존 |
| Recovery schema 0 / 1 | 0은 기존 저장의 빈 기본값. 1은 목표 Run 회복 시험값·서비스 revision과 캐릭터별 태그/스킬 경로/소모품 수량을 저장. 기존 저장에 소급 지급하지 않음 |

새 일반 싱글 Run은 임시 개발 참가자 한 명과 Identity schema 2를 만든다. 실제 계정 인증을 의미하지 않는다.

식별 Run의 Target·prototype·개발 협동 초기화는 새 후보의 첫 저장과 상태 공개를 `CommitSaveCandidate`로 묶는다. 체크포인트 저장이 활성화된 경우 저장 실패를 생성 실패로 반환하고 기존 메모리·저장을 보존하며 상태 변경을 통지하지 않는다. 메모리 전용 Run과 관리 생성의 기존 저장소·lease 계약은 유지한다. 새 생성 검사와 이 저장 처리 변경은 저장 schema를 올리거나 기존 파일을 일괄 이관하지 않는다. 컴파일·로컬 실행 결과는 [HISTORY](HISTORY.md)의 해당 이력으로 확인하며 온라인 생성·재개 검증과 구분한다.

일반 Target의 아이템 보상은 해당 Run에 저장된 카탈로그·등급 확률·무기 스킬 규칙을 사용하며 직전 상점의 상품 Query를 적용하지 않는다. 결과 저장 실패 시 같은 후보·공통 골드를 재사용하고 수령 시 아이템·골드·개인 수령 기록을 함께 저장한다. Snapshot·패배에는 지급하지 않으며 이 변경으로 개발 협동의 보상이나 온라인 지원 범위를 확장하지 않는다. [보상 계약과 검증](PROJECT_PLAN.md#3-2-상점-인카운터) · [최신 이력](HISTORY.md#9-29-2026-10-08-전투-아이템-선택과-공통-골드-보상)

인카운터 선택·상점 퇴장은 기존 노드·Continue와 같이 현재 Host만 결정한다. Client는 GameState의 직렬화 가능한 표시 뷰를 수신하며 별도 구매·소유권 변경은 없다. 선택·퇴장을 저장한 뒤 상태를 공개하고 실패 시 이전 상태로 복원한다. 관리 Run은 기존 lease·저장소 revision으로 전이를 기록하며 `EncounterChoice`·`Shop`도 전투 밖 재개를 허용한다. 일반 싱글 이어하기는 선택 화면 또는 선택한 상점을 복원한다. 기존 저장의 누락 필드는 schema 0으로 읽어 원래 경로를 유지한다. 새 저장을 이전 실행 파일로 여는 역호환은 지원 대상으로 보지 않는다.

새 Run의 서버는 RunId에서 만든 지형 시드와 8개 통로 변형·전체 방문의 후보 ID 순서를 `DungeonState` 버전 1로 고정한다. 일반 Target 60방문·명시적 프로토타입/개발 협동 9방문을 저장하고 이후 후보는 해당 Run의 저장된 풀에서 해석한다. 계획은 읽기 전용 GameState 뷰로 복제하며 클라이언트는 자체 추첨 없이 같은 배치를 구성한다. 상품·전투·보상 난수와 Host 권한은 유지한다. Continue·늦은 접속은 저장된 상점 도착점을 복원하고 로컬 현재·다음 구간만 최대 2개 Actor로 표시한다. 이 구현과 컴파일은 실제 온라인 배치 일치 검증을 뜻하지 않는다. [구현·검증 상태](HISTORY.md#9-36-2026-10-09-시드별-미로-계획과-구간-재사용)

기존 `AIConsent`·`ConsentPolicyVersion`은 구버전 구조 검증용으로 읽으며 AI 승인 조건으로 사용하지 않는다.
v1~3을 관리 v4로 자동 이관하지 않는다. 전투별 AI 플래그에서 영속 Human 이력을 추정하지 않는다.
스킬·클래스의 동작 계약을 바꾸면 콘텐츠 버전도 검토해야 한다. 저장은 실행 코드 자체를 동결하지 않는다.

## 4 서버 실행과 화면

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

계획·이동 예약 변경은 해당 소유자의 Ready만 해제하고 다른 팀원의 준비는 유지한다. 전체 준비와 복구 상태 저장 성공 뒤 Ready Phase를 종료하고 AP/SAP를 즉시 한 번 차감한다. 다른 유닛의 복귀·예약 칸 이동과 자리 교환을 거절하며 이동 실패 시 출발점으로 복원한다. 피해·사망은 서버 순서대로 즉시 반영하고 선행 사망자의 미발동 공격을 취소한다. Client 프레임·충돌·몽타주는 피해를 독립 확정하지 않는다. 최신 규칙·복구의 사용자 확인과 실제 지연·손실 검증은 대기다.

## 5 Async PvP 상대 Snapshot

현재 Snapshot 스킬은 최소 1개를 요구하며 개수 상한은 없다. 빈 장비 필드·카탈로그·콘텐츠 버전·중복 검증은 유지한다. 새 무기 장착 목록은 전투·체크포인트 경로에 연결한다. Snapshot은 전투용 카탈로그 해석에서 소모품 스킬을 거절하며 새 근접/석궁 스킬의 상대 카탈로그 등록, 무기 사본 GUID·등급·출처의 공개/내보내기와 경쟁 검증은 후속 작업이다. 새 Target Run은 기존 고정 Snapshot 상대를 사용한다.

`UPartySnapshotSaveGame`에 `FPartySnapshot`을 담고 Unreal SaveGame API로 저장한다. 초기 구현은 작은 파티의 동기 I/O다.
Snapshot은 실시간 상대 접속 없이 상대 빌드 하나로 Encounter를 만든다. 원래 아군 소유권·Host와 라운드 실행 상태는 별개다.

- 필드: SnapshotId·MemberId·ClassId·이름·Stats·순서 있는 SkillIds·EquipmentIds·TacticsId·FormationSlot·버전.
- 카탈로그가 고정 ID를 신뢰된 Enemy 클래스/스킬 에셋으로 해석한다. 상대 데이터의 임의 클래스 경로를 실행하지 않는다.
- 1~4명, FormationSlot 0~3, 순서 있는 스킬 1개 이상이며 개수 상한 없음. ID/배치/스킬 중복과 서로 다른 카탈로그 ID가 같은 스킬 에셋을 가리키는 별칭 중복을 거절한다. `ResolveRoundSkill`로 검증하며 명시 프로필은 GAS 클래스 없이 허용한다. 서로 다른 스킬 에셋의 동일 AbilityClass 공유는 허용한다.
- `ResolveSkills`는 해석된 스킬의 `Item.Consumable` 및 자식 태그를 공통 `RunRecoveryRules::IsConsumable`로 검사한다. 소모품 재고 없는 Snapshot의 일반 스킬로 사용할 수 없으며 별칭 ID도 이 검사를 통과해야 한다. 이는 전투·후보 검증이며 불투명 Skill ID를 저장하는 schema 1의 구조 자체를 변경하지 않는다.
- 수치 상한은 HP 1,000,000, AP/SubAP 100, 이동 범위 32로 검증용 한계이며 밸런스 수치가 아니다.
- EquipmentIds/TacticsId는 저장 가능하지만 현재 전투에서는 빈 값만 지원한다.
- HP는 저장값 그대로 적용한다. HP 0은 저장 가능하나 현재 Snapshot Encounter에서는 거절한다.
- FormationSlot을 `Arena.EnemyCoords`로 해석하고 중복되지 않는 빈 Enemy 영역 타일인지 전체 검증한 뒤 스폰한다.
- Snapshot 적의 장착을 무작위로 바꾸거나 회복약을 자동 지급하지 않는다. 적 아이템 AI는 미구현이다.
- 실패하면 상세 오류와 함께 준비를 정리하며 PvE 적으로 조용히 대체하지 않는다. 잘못된 저장/로드는 기존 파일/출력 값을 보존한다.

생성한 적은 기존 `ConfigureProfession → RegisterUnits → StartCombat → Result → Cleanup` 흐름을 사용한다.
기존 상대 슬롯 방식은 Encounter마다 슬롯을 읽고 Ready 경계 복구에서는 저장된 적 상태·장착·고정 계획을 사용한다. 목표 Run은 생성 시 10개 상대 값을 Run 본문에 고정하므로 외부 샘플 `.sav` 변경으로 상대가 교체되지 않는다. 양쪽 모두 현재 로컬 Snapshot 개발 범위이며 온라인 상대 게시·선정·경쟁 검증은 지원하지 않는다.

온라인은 제안 12의 선택 1을 채택했다. 현재 지원 범위의 생존 상대·저장 HP·스킬·배치를 게시하고 같은 콘텐츠 버전·진행 단계의 태그 조건 일치 후보 중 균등 추첨한다. 게시 시점·보존 기간·같은 상대 중복·버전 변경·소모 반영의 세부 값과 서비스 구현은 후속이며 현재 로컬 고정 편성을 온라인 매칭으로 취급하지 않는다.

`UPartySnapshotSelectionLibrary::SelectOpponent`는 값 데이터 후보의 진행 단계·카탈로그 콘텐츠 버전·GameplayTagQuery와 기존 생존/배치/스킬 검증을 적용하고 공통 후보 추첨에서 가중치 1로 한 명의 상대 파티를 선택한다. 적격 후보의 중복 Snapshot ID는 추첨을 거절하며 실패 시 출력·난수 상태, 성공 시 원본 후보를 보존한다. 결과는 저장 HP·스킬·배치를 유지한 값 사본이다. 기존 Target Run 생성·저장과 연결하지 않은 로컬 개발 API이며 게시 승인·계정 인증·중앙 매칭·이중 소모 방지의 구현을 뜻하지 않는다.

개발용 `LoadAndSelectOpponent`는 `FPartySnapshotSlotCandidate`의 슬롯 ID·진행 단계·태그를 받아 슬롯 이름·중복을 먼저 검사하고 진행 단계·GameplayTagQuery가 일치하는 파일만 `LoadSnapshot`으로 읽어 같은 선별 API에 전달한다. 일치 슬롯이 없거나 파일이 누락·손상되면 실패하며 읽지 못한 후보를 제외하여 다시 추첨하지 않는다. 실패 시 출력·난수 상태를 보존하고 원본 슬롯은 쓰거나 삭제하지 않는다. 기존 `ProjectA_Opponent_` 접두사와 Snapshot schema 1을 그대로 사용하며 로드 시 기존 메모리 이관도 원본 파일을 덮어쓰지 않는다. 호출자가 제공하는 진행·태그는 로컬 후보 메타데이터이며 인증된 게시 정보가 아니다. 이 API도 기본 Run과 온라인 공급자에는 미연결이며 실제 게임 경로·온라인 서비스 검증 완료를 뜻하지 않는다.

## 6 준비 완료 경계와 파일 저장

기존 순차 턴 실행·저장/복원 경로는 폐기했다. `CompletedTurnSerial`·`NextTurnIndex`와 schema 1/2는 이전 파일을 인식하는 호환 필드이며 새 라운드 번호로 재해석하지 않는다. 새 Ready 경계는 schema 3의 라운드·계획 필드를 사용한다. 이전 파일은 삭제·다운그레이드하지 않는다.

Map·Result·EncounterChoice·Shop·종료 결과의 저장에 더해 첫 라운드·다음 라운드 Planning과 준비 완료·수정·취소의 계획·복구 상태를 저장한다. 서버가 저장 성공을 확인한 뒤 Ready를 공개하며 저장 실패 시 이전 파일·계획·준비를 보존하고 준비 완료나 전투 시작을 확정하지 않는다. 전원 준비 경계는 비용 차감 전 상태로 저장하고 Ready Phase 종료 시 비용을 한 번 차감한다. 강제 종료 후 이 경계를 복구하여 다시 실행한다. 진행 중 시전·투사체·지연 피해의 임의 시점 복원은 지원하지 않는다.

목표 Run 소모품도 이 경계를 따른다. 서버는 생존한 원래 인간 캐릭터의 자기 대상·수량·HP·비용을 검증하고 차감 전 HP/AP/수량/명령을 저장한다. 실제 GAS 회복 발동 성공 시 수량을 한 번 차감하며 다음 계획 경계와 전투 결과에서 HP와 잔여 수량을 함께 저장한다. 회복·부활·소모품 구매는 서비스 revision으로 중복 요청을 거절하고 골드·HP·수량을 후보 저장 성공 뒤 공개한다.

일반 Continue는 식별 Run v5 또는 LegacyOffline v6의 schema 3 Ready 경계, 관리 Resume는 v4/schema 3의 Ready 경계를 검증·복구한다. LegacyOffline은 신뢰된 파티 슬롯으로 Standalone에서만 복원하며 새 Identity·Host·소유자를 추정 생성하지 않는다. 이전 순차 Combat는 참가자·HostEpoch·lease·기준 저장 변경 전에 거절한다. 원래 소유권·버전 검증과 비전투 저장의 지원 범위는 유지한다.

최종 승패와 Continue는 저장 성공 뒤 공개하며 실패 시 대기 결과·기존 단계·파일을 보존하고 재시도한다. Unreal USaveGame 직렬화와 같은 디렉터리 임시 파일의 flush·바이트 검사·Win64 파일 교체를 유지한다. 기존 파일을 먼저 지우지 않는다. 로컬 어댑터는 온라인 정본 권위나 절대 내구성을 보장하지 않는다.

## 7 관리 v4 재개와 lease

`FRunParticipationData.HumanParticipants`는 원래 소유자 중 지금 Human인 계정을 전투 밖에도 보존한다.
현재 목록에 없는 원래 캐릭터는 AI다. 사망만으로 목록을 바꾸지 않으며 결과에서 전투 본문을 비워도 목록은 남는다.
새 Encounter와 Ready 경계 복구는 이 목록으로 조작 모드를 결정한다. 이전 순차 Combat 본문의 조작 모드는 구조 검증 후 재개 거절 대상으로만 사용한다.

| API | 계약 |
|---|---|
| `ConfigureLocalDevelopmentCaller` | 신뢰 C++가 Development 계정·namespace를 주입. 같은 GameInstance에서 변경 불가 |
| `CreateManagedRun` | 최초 Host 1번·epoch 1·원래 2~4명 전원 Human. 기록과 lease 생성 성공 뒤 메모리에 적용 |
| `ReadManagedRun` | 원래 개발 참가자가 최신 본문/stamp를 조회. 조회만으로 권한·현재 Run을 바꾸지 않음 |
| `ResumeManagedRun` | 이전 순차 Combat 거절. 비전투 또는 schema 3 Ready 경계는 닫힌 실행·최신 stamp·Human 부분집합·번호순 Host 검증 후 새 본문/세대/lease 획득 |
| `ConfirmManagedResumeStarted` | Gameplay 복구 성공 뒤 pending 해제. 실패하면 lease·pending·오류를 유지 |
| `BeginManagedMenuTravel` | 같은 실행 SessionId·WorldContext의 TravelFailure를 GameInstance에서 감시 |
| `CloseManagedRun` | 전투/콜백 중단 후 lease·활성 상태 해제. 호출자·재개 대상은 유지 |

로컬 기준 저장소는 같은 PC의 공유 경로에서 배타적인 OS 실행 파일 핸들을 유지한다.
짧은 별도 트랜잭션 잠금으로 읽기·stamp 비교·본문 교체를 직렬화한다. Create/Acquire/Commit 실패는 기존 기록·lease·출력을 보존한다.
Run별 유효한 저장소 revision·HostEpoch·SessionId로 갱신하며 충돌 후 일반 SaveSlot으로 우회하지 않는다.
프로세스가 종료되어 핸들이 풀려도 Host는 자동 변경되지 않는다. 명시적 Resume에서만 새 epoch·SessionId를 만든다.
이는 동일 PC 개발 대역이며 다른 PC의 중앙 저장·계정 인증·네트워크 장애 판정이 아니다.

명시적 재개는 Run Identity·영속 Human 목록과 새 Ready 경계의 Identity/조작 모드를 일치시키며 원래 소유권·최초 번호를 유지한다. 이전 순차 체크포인트는 재개하지 않는다.
현재 Human이 본인 Host 한 명이면 Standalone으로 복구한다. Listen Server에서는 현재 Human 전원이 배정될 때까지 멈추며 자동 AI 전환하지 않는다.
pending 중 Human 명령·노드·Continue를 막고 Gameplay 진입과 필요한 Ready 경계 복구 성공 후 해제한다. 순차 턴 체크포인트 저장은 허용하지 않는다.
정상 월드 종료는 `ShutdownGameplay → CloseManagedRun` 순서로 Actor·AI·타이머·능력 콜백을 먼저 중단한다.

Authority는 관리 실행 여부를 `Reset` 후에도 유지하며 현재 lease·전체 Identity·호출자 Host·SessionId를 재검사한다.
관리 설정을 일반 Run으로 낮추거나 직접 Close 후 남은 Authority로 Human/AI 명령을 실행하지 못한다.
Human 바인딩과 서버 AI 모드 설정도 영속 목록에 맞아야 한다. 이 방어가 정상 Actor 정리를 대신하지 않는다.

## 8 메뉴와 아군 AI

일반 MainMenu 이어하기는 v1 오프라인·단일 참가자 LocalDevelopment v2의 비전투 상태와 v5/v6의 schema 3 Ready 경계를 허용한다. 실제 클릭에서 방금 읽은 본문을 다시 검증한다. 이전 v3/순차 Combat는 거절하며 협동·AccountProvider·v4를 Standalone 경로로 우회하지 않는다.

기본 새 싱글은 목표 Run의 첫 인카운터 선택으로 진입한다. 명시적인 비 Shipping/Test 개발 옵션 `-ProjectAPrototypeRun`은 기존 10전투 생성 경로를 사용한다. 기존 2/10전투 저장은 원래 노드·schema로 이어가며 새 목표 경로로 자동 확대하지 않는다.

관리 패널은 신뢰 개발 경로가 호출자·재개 대상을 설정했을 때만 표시한다. 비전투 기록과 새 schema 3 Ready 경계에 대해 번호순 Host·영구 AI·lease를 유지한다. 이전 순차 Combat 기록은 명시적인 미지원 메시지와 함께 거절한다. 메뉴 이동 실패는 확정 Host/AI 기록을 되돌리지 않고 lease를 해제하여 재시도를 허용한다. 온라인 로그인·관리 저장 검색·수동 협동 재개 UI는 미구현이다.

기존 로컬 개발 협동은 새 비관리 Listen 방을 생성하고 실제 참가자별 캐릭터 한 명만 만든다. 주소 참가가 원래 소유자 인증을 대신하지 않으며 이탈한 방은 새 방으로 시작한다. 자동 Host 변경·대체 참가·자동 AI 전환은 없다. 별도 SteamDev 설정에서는 이 개발 계정 경로로 온라인 연결을 대신하지 않는다.

이전 `UPartyAutoCombatComponent`와 EnemyUnit의 순차 재판단 실행은 제거했다. RoundCoordinator가 인간 초안 이전에 적·ServerAI 아군의 명령을 한 번 고정하고 준비 상태로 만든다. 클래스·진영·원래 소유자는 유지한다. 초기 AI는 가까운 적과 단일 공격을 선택하며 타일 접근·지원·이동 전술은 후속이다.

## 9 Steam·PlayFab와 남은 서비스 정책

연동 기준은 Steam+PlayFab, 전투 연결은 Unreal Listen Server·Steam P2P/SDR이다. 운영비 회피 조건을 적용한다.
별도 전투 서버 임대가 없어도 공동 Run 저장·승계 승인·결과 검증·MMR의 비용과 권위는 별도다.
Steam Cloud는 기본적으로 같은 사용자의 PC 사이 저장 동기화이며 참가자 간 단일 최신 Run을 원자적으로 확정하는 기능이 아니다.
Steam Leaderboards는 전투 정당성을 검증하지 않는다. Trusted 점수 제출의 publisher key를 Listen Host 클라이언트나 저장소에 넣지 않는다.

2026-09-11 재확인한 [PlayFab Development 문서](https://learn.microsoft.com/en-us/xbox/playfab/pricing/development-mode)는 Foundation으로의 전환을 안내한다. 아래 준비 절차처럼 현재 계정에 제공되는 모드·무료 자격을 확인한다. 과거 개발 한도를 신규 Title의 무료 보장으로 사용하지 않는다.
개발 무료 한도를 영구 무료 운영으로 취급하지 않는다. Foundation 자격·별도 계산 리소스 비용도 확인한다.
유료 리소스·과금 전환을 임의 활성화하지 않는다. 비용 조건을 랭크 삭제·무검증 점수·오프라인 진행 분기 승인으로 해석하지 않는다.
공식 참고: [Steam 인증](https://partner.steamgames.com/doc/features/auth?l=english), [Steam Cloud](https://partner.steamgames.com/doc/features/cloud?l=english), [PlayFab 개발 모드](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode).

준비물은 Steamworks AppID·테스트 계정/권한·PlayFab Title·승인된 개발 환경이다. 공급자는 기존 결정을 유지한다.
Unreal Online Subsystem·공식 SDK·엔진 비동기 delegate를 우선하며 선택한 UE 5.8 플러그인/NetDriver 코드를 확인해 설정한다.
로그인 성공·표시명·PIE ID·Client Verified 플래그를 서버 인증 증거로 쓰지 않는다. 서버가 검증한 티켓 계정을 실제 연결에 대응한다.
온라인 실패를 Development 계정이나 로컬 저장 성공으로 대체하지 않으며 이전 연결의 늦은 인증 완료도 현재 Run을 바꾸지 못하게 해야 한다.

제안 11의 선택 1을 채택했다. PlayFab을 사용하는 신뢰 서비스가 Run 본문·원래 소유권·revision/epoch·lease를 함께 승인하고 최신 본문을 제공한다. 중앙 서비스는 정확한 이전 revision/epoch의 한 갱신만 승인하고 후임자에게 원래 Host 파일 없이 최신 본문을 제공해야 한다.
같은 요청 ID·같은 본문은 기존 결과를 조회하고 같은 ID·다른 본문은 거절한다. 타임아웃 뒤 새 ID로 같은 동작을 다시 확정하지 않는다.
전투 Attempt의 결과와 Run 종료 시 MMR은 다른 요청 ID로 재제출해도 각각 한 번만 반영해야 한다. 서명·TLS만으로 전투 정당성을 증명하지 않는다.
제안 13의 선택 1에 따라 공격 Run의 PvP 승패를 합산하여 파티 공통 MMR 변동량을 계산하고 Run 종료 시 현재 Human에게 동일하게 한 번 적용한다. Snapshot 방어 결과는 통계만 보존한다. 계산식·초기 평점·상대 평점 기준 시점·동률·일부 결과 미확정·이탈 악용 처리는 미정이며 서비스 정산은 미구현이다.
제안 14의 선택 1에 따라 신뢰 서비스가 확정 명령·시드·콘텐츠 버전·판정 입력을 저장하고 전투 종료 후 재검증한다. 미검증 결과는 MMR 정산에서 제외한다. 서버 재현 가능한 판정 범위·실패/이탈·미확정 Attempt·무료 계산 조건은 후속이며 로컬 전투의 물리 판정까지 결정성이 확보된 것으로 간주하지 않는다.
비전투 AI 편집/보상·일시 끊김의 대기/시간 제한은 미정이다. AI 전환된 원래 소유자의 읽기 전용 관전은 채택했으나 실제 계정 인증·관전 UI와 명령 차단의 온라인 검증은 미구현이다.

## 10 T14-8 서비스 준비

2026-10-04 제안 9 선택 2의 Steam 480 개발 연결과 제안 10 선택 1의 친구 초대 방향을 채택했다. 사용자는 서로 다른 계정의 PC 2대와 PlayFab Title/무료 모드가 미준비라고 확인했다. 자체 Steam App ID의 인증·배포, PlayFab 연동, 인터넷 접속·인증 성공은 확인하지 않았으며 T14-7의 동일 PC 로컬 승계 결과와 구분한다.

### 10-1 비용과 준비 순서

1. [PlayFab Game Manager](https://developer.playfab.com/)에 Microsoft 계정으로 로그인하고 Studio를 준비한다. 생성 화면에서 Title 이름은 개발 환경임을 구별할 수 있게 `ProjectA-Dev` 등을 사용한다. Title이 생성되면 게임 이름 아래의 Title ID와 선택된 서비스 모드를 확인한다. 계정·Studio·Title 생성 순서는 [공식 시작 안내](https://learn.microsoft.com/en-us/xbox/playfab/live-service-management/gamemanager/quickstart)를 따른다.
2. Title을 확정하기 전에 무료로 제공되는 모드와 사용량 조건을 확인한다. [Foundation 온보딩](https://learn.microsoft.com/en-us/xbox/playfab/get-started/foundation-onboarding)은 Entra ID·Xbox 출시/출시 계획·Partner Center·미리보기 승인을 전제로 안내한다. Steam 우선이라는 기존 결정을 Xbox 출시 계획으로 바꾸거나 자격을 추정하지 않는다. 무료 모드가 없거나 유료 전환만 보이면 모드 이름·표시된 비용을 확인한 뒤 다음 방향을 결정한다. Launch/Live 전환과 유료 계산·호스팅 리소스 생성은 현재 준비 범위에 없다.
3. [Epic의 App ID 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine)와 [Valve SpaceWar 예제](https://partner.steamgames.com/doc/sdk/api/example)의 공유 테스트 ID `480`을 별도 개발 연결에 채택했다. 출시는 자체 App ID가 필요하며 480 연결은 ProjectA 소유권·배포 권한이나 PlayFab 정식 Steam 인증 준비 완료를 뜻하지 않는다.
4. 자체 ID를 만들 시점에는 [Steamworks 온보딩](https://partner.steamgames.com/doc/gettingstarted/onboarding)의 계약·신원·은행/세금 절차와 앱 등록을 사용자가 진행한다. [Steam Direct 수수료](https://partner.steamgames.com/doc/gettingstarted/appfee)는 앱당 USD 100 상당이며 지역 세금이 적용될 수 있다. 이는 전투 서버의 월 운영비와 별도다. 현재 결제를 요청하거나 대신 실행하는 단계는 아니다.
5. 자체 App ID·Title이 준비되면 PlayFab의 Steam 연동 설정에서 해당 앱 ID와 권한이 있는 Web API Key를 연결한다. 키는 공급자 관리 화면에서만 취급하고 게임/Listen Host/Git/채팅에 넣지 않는다. Steam 티켓으로 로그인하는 계약과 필요한 설정은 [공식 Steam 인증 API](https://learn.microsoft.com/en-us/xbox/playfab/api-references/c/pfauthentication/functions/pfauthenticationloginwithsteamasync)를 따른다. 단순 CustomID 로그인이나 로그인 성공 자체를 전투 결과 검증으로 대신하지 않는다.

2026-10-04 설치된 UE 5.8.3의 OnlineSubsystemSteam·SteamSockets·CustomConfig API와 [Epic SteamSockets 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine)를 대조했다. 연결 시제품은 설치 엔진의 SteamSockets NetDriver를 명시하며 기존 SteamNetDriver 웹 예제를 그대로 복사하지 않는다.

### 10-2 로컬 엔진 점검과 다음 구현

| 대상 | 정적 확인 결과 / 적용 방향 |
|---|---|
| 프로젝트 | `ProjectA.uproject`에 OnlineSubsystemSteam/SteamSockets를 Win64·비 Shipping/Test로 명시하고 Build.cs도 같은 조건으로 의존한다. PlayFab은 활성화하지 않음 |
| 기본 실행 | `Config/DefaultEngine.ini`의 Steam `bEnabled=false`. 일반 싱글·로컬 개발 협동의 기본 온라인 공급자/NetDriver를 Steam으로 바꾸지 않음 |
| 개발 옵션 | `Config/Custom/SteamDev/DefaultEngine.ini`가 Steam·480·SteamAuth·SteamSockets를 설정. 연결 UI는 게임 실행에서 `-CustomConfig=SteamDev -ProjectASteamDev`를 함께 지정해야 활성화 |
| P2P transport | `/Script/SteamSockets.SteamSocketsNetDriver`를 기본 및 fallback으로 요구. 다른 NetDriver나 로컬 계정으로 성공을 대체하지 않음 |
| IP 전용 경로 | 엔진 `Plugins/Online/SocketSubsystemSteamIP/SocketSubsystemSteamIP.uplugin`은 NAT punchthrough를 제공하지 않으며 P2P에 SteamSockets를 쓰도록 명시. 두 플러그인을 같은 transport로 취급하지 않음 |
| PlayFab | 엔진 `Plugins/Online/Microsoft/PlayFabParty/PlayFabParty.uplugin` 존재, 기본 비활성. Win64용 PlayFab Party Socket Subsystem이며 프로젝트 인증·Title 연동·중앙 저장·MMR 구현을 뜻하지 않음. 실제 SDK 도입 시 UE 5.8 지원 버전·인증 API를 먼저 고정 |
| 관리 저장 | 현재 `FLocalRunAuthorityStore`는 같은 PC 동기 파일/OS lease, 개발 호출자만 지원. Steam 계정 ID만 주입해 온라인 관리 Run으로 승격하지 않음 |

`USteamDevelopmentSubsystem`은 세션 생성·검색·참가·종료와 Steam 친구 초대 수락 delegate를 연결한다. 2~4명, 전용 `PROJECTA_PROBE` 표식·빌드 ID·lobby 설정을 검사하며 서버는 현재 Steam 친구 관계도 확인한다. 기존 `Core/MainMenu`를 연결 전용 C++ GameMode로 열고 연결 수와 공식 인증 성공 수를 구분한다. 새 맵 복제, Run 생성, 관리 저장 조회·승계, 대체 참가 또는 관전 권한 배정은 하지 않는다.

친구 전용 로비는 일반 공개 검색에서 제외되므로 `ReadFriendsList(InGamePlayers)` 후 친구마다 `FindFriendSession`을 순차 호출한다. 같은 세션은 한 번만 표시하고 방 소유자의 친구 관계를 다시 확인한다. 요청 번호·대기 상태로 오래되거나 중복된 완료를 거절하고 검색 중 나가기는 현재 요청 완료 후 정리한다. 생성·참가 실패도 예약된 나가기를 처리하며 Logout에서 해당 계정의 이전 SteamAuth 관측을 삭제한다. [Steam 로비 공개 범위](https://partner.steamgames.com/doc/api/isteammatchmaking#ELobbyType) · [구현·검증 이력](HISTORY.md#9-25-2026-10-07-todo-권장안의-로컬-구현)

Steam 로그인·UniqueNetId 문자열은 원격 인증 완료로 취급하지 않는다. 공식 `FOnlineAuthUtilsSteam::OnAuthenticationResultDelegate`의 성공 판정을 관측하고 기본 인증 실패 추방을 유지한다. 다른 처리기가 인증 delegate를 소유한 경우 시제품을 시작하지 않는다. 로그아웃·인증 실패·세션/이동 실패는 명시적인 실패로 남기며 Development 계정으로 치환하지 않는다. [공식 SteamAuth 계약](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine#steam-online-authentication)

다음 단계는 준비된 서로 다른 PC/Steam 친구 계정으로 실제 접속·인증 실패·초대·종료를 확인하는 것이다. 이후 검증된 계정을 Run 원래 소유권에 연결하고 승인된 PlayFab 환경에서 정본·lease·결과 검증을 구현한다. 비밀번호·인증 티켓·Web API/Secret Key는 소스·설정·채팅에 기록하지 않는다. 무료 모드/사용량 조건은 해당 계정에서 확인하며 유료 전환·호스팅·계산 리소스를 임의 활성화하지 않는다.

## 11 개발 실행 참조

프로젝트 루트 PowerShell·UE 5.8.3 기준 명령이다. 남은 실행 확인은 [TODO](TODO.md), 실제 실행 결과와 범위는 [HISTORY](HISTORY.md)를 따른다. 명령 기록은 실행 완료를 뜻하지 않는다. 샘플 생성은 로컬 에셋/저장을 작성하므로 최초 준비 시에만 실행한다.

```powershell
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$editorCmd = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = "$PWD/ProjectA.uproject"
& $editorCmd $project -run=pythonscript -EnablePlugins=PythonScriptPlugin "-script=$PWD/Source/ProjectAEditor/Scripts/ConfigureSnapshotContent.py" -unattended -nullrhi -nop4
& $editor $project -ProjectAPrototypeRun -ProjectAOpponentSnapshot=SampleOpponent
```

샘플은 Hunter 1명·최대 HP 140/현재 HP 120·AP 2/SubAP 1·이동 1·배치 0·DefaultAttack을 사용한다. 2026-10-01 확정한 휩쓸기·테스트 원거리·AOE 3종의 제거는 기존 Snapshot 입력·샘플에도 적용하며 다른 스킬·소유권은 유지한다. 기존 샘플 슬롯을 일괄 덮어쓰지 않으며 로드 후보 검증 후 정상 저장에 반영한다. 제거 후 보유 스킬이 없는 Snapshot은 오류로 거절하고 원본을 보존한다. [제거·저장 적용 범위](PROJECT_PLAN.md#3-2-상점-인카운터)
SlotId는 영문·숫자·밑줄 1~64자이며 `ProjectA_Opponent_<SlotId>`로 저장한다. 샘플 `.sav`는 Git·패키지에 포함하지 않는다.
Snapshot Run은 `ProjectA_SnapshotRun_<SlotId>`, PvE는 `ProjectA_Run`이다. `-ProjectASaveSlot=...`이 우선하며 이어하기는 같은 상대 인자를 사용한다.
기존 상대 슬롯을 쓰는 이 명령은 명시적인 개발 프로토타입이다. 기본 메뉴의 목표 Run은 Run 본문에 고정된 로컬 Snapshot을 사용하며 기존 샘플 슬롯을 읽거나 덮어쓰지 않는다.

Steam 시제품은 Steam 친구인 서로 다른 계정과 PC 2대에서 각각 아래 명령으로 게임을 실행한 뒤 메뉴의 Steam 연결 확인 패널을 사용한다. 정원 선택·방 생성, 검색 참가 또는 친구 초대 수락, 서버의 연결/공식 인증 수, 나가기·세션 정리를 확인한다. 현재 PC 2대 미준비로 이 절차는 미실행이다. 창 위치는 각 PC의 왼쪽 모니터 배치에 맞춘다.

```powershell
& $editor $project /Game/User_JeHoon/LEVEL/Core/MainMenu -game -CustomConfig=SteamDev -ProjectASteamDev -windowed -ResX=1280 -ResY=720
```

두 옵션은 Steam 서비스를 사용하는 명시적 개발 실행이다. 일반 작업에서는 제거한다. 이 패널의 성공은 연결·인증 범위에 한정하며 온라인 협동 Run·저장·승계·관전·MMR의 성공으로 확대하지 않는다.

아래 순차 전투·턴 복구용 명령/옵션은 과거 재현 기록이다. 현재 기본 전투의 실행 지침이나 통과 근거로 사용하지 않으며 온라인 서비스 구현 뒤 필요한 확인은 [온라인 확장 계획](TODO.md#4-온라인-협동과-경쟁)에 기록한다.

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

동일 예정 시각도 서버 순서대로 피해·사망을 즉시 반영하며 선행 사망자의 미발동 공격을 취소한다. 전체 라운드의 원자적 동시 성립이나 물리 충돌까지 포함한 결정성을 보장하지 않는다. 수정 소유자만 Ready 해제·복귀/예약 칸 이동과 자리 교환 금지는 2026-09-18 확정 정책이다. 당시 동일 PC 2/4인 결과와 별도로 2026-10-08 `RunRoundPIE`에서 소유권·HP/계획 복제·원격 AP/몽타주/보행·Host 결과/상점 진행을 다시 확인했다. [최신 실행 근거](HISTORY.md#9-33-2026-10-08-위임-실행-검수와-협동-보완)는 생존·적 HP를 보정한 개발 경로이며 Ready 강제 종료 후 Actor 복구·Steam 다중 PC·지연/손실·체감 품질의 통과 근거는 아니다.

### 12-2 Snapshot과 복구

상대 Party Snapshot과 실행 중 라운드/명령은 별도 데이터다. 기존 카탈로그로 스폰한 장착 스킬은 명시 RoundDefinition 또는 초기 변환을 사용한다. 원래 GAS 효과·EquipmentIds/TacticsId·Snapshot Speed가 모두 지원되는 것으로 해석하지 않는다. 적 계획은 인간 초안 이전에 고정한다.

이전 순차 Phase Combat 저장은 로드/관리 재개를 거절하며 파일·현재 상태·lease를 바꾸지 않는다. 새 전투는 마지막 저장 계획·Ready·양 팀 유닛·자원·배치·고정된 적 계획을 복구한다. 비용 차감 전 경계에서 다시 실행하므로 진행 중 시전·투사체의 임의 시점 복원과 구분한다. 중단만으로 자동 Host 승계나 AI 전환하지 않는다. 온라인 정본 저장·경쟁 재실행 방지는 별도 미구현이다. 전투 규칙은 [기획 기준](GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투), 구현 계약은 [준비 완료 경계와 파일 저장](#6-준비-완료-경계와-파일-저장), 현행 확인 범위는 [재검증 이력](HISTORY.md#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다.
