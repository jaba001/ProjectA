# ProjectA 완료 작업과 검증 기록

갱신일: 2026-09-11. 완료 범위·기준 커밋·실행 근거를 기록한다. 현재 상태는 [TODO](TODO.md), 최신 절차·결과는 [TEST_REPORT](TEST_REPORT.md)를 따른다. 과거 검증은 이후 변경의 통과 근거로 사용하지 않는다.

## 최근 변경

| 기준 | 변경·검증 |
|---|---|
| e90cd21 | 제작 경로를 Content/User_JeHoon으로 통일. T12 위젯 3종 이동·Git 추적. 패키지 로드 3개, 이전 경로 에셋/Redirector 0개, Editor 빌드 성공. 수동 확인 대기 |
| 640a614 | e90cd21 기준 2인 전투 자동화: 경고 동반 성공 1건, 오류 0·경고 4, 14.998초 |
| 8c29017 | 2인 경고 대응 분석·보고서 직접 요약 규칙. 게임 코드 변경·재실행 없음 |
| 2026-09-11 후속 | 8c29017 기준 4인 전투 자동화: 경고 동반 성공 1건, 오류 0·경고 6, 31.563초. Markdown 문체·중복 정리 |

2·4인 검증은 기존 사용자 삭제 에셋 3건을 보존한 작업 폴더에서 수행했다. 조작권·행동 RPC·상태·두 전투 완료를 확인했으며 Steam/P2P·수동 조작·승계·복구 검증은 포함하지 않는다. 상세는 [TEST_REPORT](TEST_REPORT.md)에 기록한다.

## 1. 초기 리뷰와 Vertical Slice

- 2026-09-07: 전투·GAS·AI·메뉴·UI 생성 경로를 정적으로 검토했다. P1 2건과 P2 4건을 기록했으며 당시 빌드·PIE·패키징은 실행하지 않았다.
- 2026-09-08: 공통 행동 완료 규약과 AI 실패 복구, 파티 데이터, Gameplay/Encounter/Result 루프를 구현했다. 초기 묶음은 `1e2f4b6`이다.
- 기본 흐름은 MainMenu → 캐릭터 생성 → Gameplay → Run Map → Combat → Result → Continue → 다음 전투다. Defeat는 진행을 종료한다.
- Gameplay는 TestMap을 바탕으로 생성했으며 Arena·Grid·카메라·파티/적 설정을 분리했다. 반복 전투마다 맵을 다시 여는 구조는 도입하지 않았다.
- 새 Gameplay WBP 4종을 실제 생성·컴파일·저장하고 별도 프로세스에서 에셋 설정 29항목을 확인했다. 원본 TestMap/MainMenu/WorldMap 보존도 비교했다.
- 최초 Gameplay NavMesh가 비어 이동하지 못한 문제는 해당 맵의 내비게이션 데이터를 빌드·저장해 해결했다.
- 사용자 플레이에서 발견한 클릭 불능은 CommonUI 입력 모드와 메뉴 travel의 잔여 입력 잠금 문제였다. HUD 입력 설정·뷰포트 잠금·최초 포커스를 보정했다.
- 직접 행동 호출만 하던 검증을 Slate 합성 마우스의 Move/Skill 버튼 → 타일 클릭으로 보강했다. 실제 이동·점유·GAS 피해와 전투 화면 전환을 확인했다.
- 최종 Editor 빌드와 자동화 8건이 통과했다. 자연 공격 반복으로 Victory, Continue 후 같은 월드의 다음 전투, 치명적 GAS 피해로 유도한 Defeat를 구분해 검증했다.
- 당시 자동으로 Visual Studio를 열었던 절차는 이후 사용자 요청과 `94e0a19`로 중단했다. 완료 안내를 위해 IDE를 열지 않는다.

## 2. T01~T13 완료 순서

T01·T02·T08의 기반은 초기 Vertical Slice에서 함께 구현했다. 표는 작업 번호순이며 별도 커밋이 없는 항목을 새 커밋으로 추정하지 않는다.

| 작업 | 완료 내용 | 핵심 검증·커밋 |
|---|---|---|
| T01 / R01 | 이동 여부와 독립된 행동 상태·GAS 종료 연결·성공/실패/취소 완료 1회. 제자리 스킬의 불필요한 복귀 제거 | 몽타주 유/무·동기 완료·활성화 거절·busy/중복 입력 검사. 초기 `1e2f4b6` |
| T02 / R02 | 적 AI에 실패·취소 결과 전달, 다음 틱 재판단/안전한 종료. 원타일·시작 위치 복구와 오래된 콜백 차단 | 접근/복귀 phase 주입·no-nav 거절·1회 완료. 소비한 AP는 비환불. 초기 `1e2f4b6` |
| T03 / R03 | DataAsset 비용을 HUD·입력·AI·GAS의 단일 AP 기준으로 통합 | 비용 1/2/3/0/-1, 과거 Ability 비용 무시, 실패 시 자원 보존. 전체 11건. `e5dcb76` |
| T04 / R04 | 플레이어·적의 타겟 규칙·전열 보호 공통화, 실행 직전 대상 재검증 | 576개 조건, 점유자/진영/생존 변경·재시도. 전체 14건. `5517601` |
| T05 / R05 | 플레이어 입력과 AI 내부 턴 종료 분리, 타일 명령의 Controller 집중 | 적 턴 입력 차단·AI 독립 종료·busy/사망/종료 문맥 거절. 전체 16건. `db987e2` |
| T06 / R06 | 직접 효과와 스폰 효과의 범위 계산 통합, 미지원 타입·잘못된 반경 거절 | 72조건의 실제 피해·AP·완료 일치와 중복 impact 차단. 전체 18건. `3414054` |
| T07 | 스폰 공격의 몽타주+impact 완료 책임, 시간 초과/취소/사망 정리, 두 자원 소진 후 턴 종료 | 늦은 피해 차단·완료 1회·AP/SubAP 조합. 전체 20건. `ac20045` |
| T08 | GameInstance 수명의 4슬롯 파티 데이터를 Gameplay 스폰으로 연결 | 빈 슬롯 제외·슬롯/좌표 유지·생성 검증·레벨 이동. 초기 `1e2f4b6` |
| T09 | 공통 직업 정의와 이름/직업 편집·ClassInfo를 실제 스탯·장착에 연결 | 네 직업·저장/취소·빈 이름 거절·travel 후 유지. 전체 21건. `4dba375` |
| T10 | 결과 1회·입력 잠금·HP 기록·정리·Continue·두 번째 전투 루프 재확인 | T09 이후 코드 변경 없이 기존 21건의 성공 근거 재사용. 기록 커밋 `d6605d4` |
| T11 | 단일 슬롯 체크포인트·메뉴 Continue·그래픽/VSync 옵션·Quit 연결 | Editor/게임 빌드·전체 23건·BuildCookRun·독립 패키지 저장/이어하기/종료. `b383e6a` |
| T12 | Designer 구조 유지, JSON 생성/검증·AddMissing·메뉴 프리뷰·카메라/배치 정리 | 전체 24건·실제 WBP compile/save·반복 추가 1개/0개·화면 확인. `78f1f81` |
| T13 | 회복약·가중 추가 스킬 획득/장착·적 이동 판단과 실패 전이 | 실제 효과/재고/SubAP·획득 스킬 피해·이동/막힘·두 전투 재지급. 전체 26건. `a97ed4d` |

각 전체 건수는 해당 변경 시점의 자동화 모음이며 서로 합산하지 않는다. 경고 동반 성공도 포함하고 위 최종 실행의 실패는 0건이었다.
T10은 이미 구현된 루프의 완료 근거 정리이며 새 전투 기능을 다시 구현한 커밋이 아니다.
T11의 당시 전투 중 종료는 전투 전부터 복구했다. T14의 확정 턴 저장은 이후 별도 스키마로 추가했다.
T12의 JSON DryRun은 구조 검사다. 실제 생성본의 클래스·바인딩·Blueprint 컴파일·저장 검증은 별도 실행으로 확인했다.
T13의 초기 회복약/추가 스킬은 전투마다 지급하고 HP만 다음 전투에 이어졌다. 이후 전투 중 복구는 T14 체크포인트가 담당한다.

## 3. T14 로컬 Snapshot과 Co-op 확장

초기 싱글플레이 유지/네트워크 보류 기록 `43e9b9e`는 이후 하이브리드 기획 `6cd7a48`로 갱신했다.
게임 방향 `18d71e3`, Co-op 소유권·복구 기획 `ee61c84`는 문서 결정이며 실제 네트워크 구현과 구분한다.
로컬 Snapshot 우선·Unreal `USaveGame` v1을 선택한 뒤 Co-op 작업을 1~8번으로 분할했다.

| 단계 | 완료된 범위 | 당시 최종 근거·커밋 |
|---|---|---|
| 로컬 Snapshot | 값 데이터 저장/검증, 신뢰된 ClassId/SkillId 카탈로그, 기존 Enemy/Combat 연결, 상대별 Run 저장 분리 | 전체 32건·별도 Snapshot 저장 맵 PIE 1건. `4092e53` |
| 1. 식별·소유권 | Run/계정/캐릭터 ID·원래 소유자·Host 값 데이터, v2 저장과 실제 식별 정보 없는 v1 호환 | 전체 36건·Snapshot PIE·독립 Writer/Reader. `66da97c` |
| 2. 서버 명령 | 값 Command와 소유 연결 RPC, 참가자 바인딩·소유권·턴·자원·대상·중복 검증 | 전체 41건·Snapshot PIE. `05843c3` |
| 3. 2인 동기화 | 서버 TurnManager, Combat/GameState 뷰, Unit/GAS/Grid·결과·HUD 복제 | 전체 42건·실제 Listen/Client RPC·Snapshot PIE. `da5aaf0` |
| 4. 턴 저장·기존 Host 복구 | v3 확정 턴 경계·고정 상대·실패 재시도·새 Actor/명령 문맥 복구 | 전체 52건·2인 새 PIE·별도 프로세스·상대 교체/삭제. `19908e2` |
| 5. 아군 AI | 원래 소유권/팀 유지, 서버 판단·공통 명령·별도 AI 세션, 전투 본문 schema 2 모드 저장 | 전체 56건·실제 AI PIE·독립 Writer/Reader. `7fa7235` |
| 6. 독립 기반 | 참여 DTO와 로컬 저장소의 단일 실행 lease·정확한 stamp 교체 | 전체 60건·프로세스 Busy/강제 종료 후 재획득. 이때는 게임 루프 미연결. `e7b35f6` |
| 7. 독립 3·4인 | 각 원격 소유자 RPC·전체 상태/HUD·끊김 정지·기존 Host 새 세션 복구 | 전체 65건·별도 2/3/4인 AI 복구 3건. 승계 검증과 구분. `152aed7` |
| 8. 온라인 준비 | 공식 UE/공급자 연동 지점·서비스 준비물·책임과 수용 기준 문서화 | 문서 검증만 수행. 실제 서비스 미연동. `f877e4d` |
| 추가 Continue 수정 | 협동/AccountProvider 저장의 잘못된 Standalone 진입 차단, 클릭 시 파일 재검증 | 전체 67건·파일 교체 후 메모리/디스크 불변. `cfbb9b9` |
| 확정 정책 반영 | 최초 참가 번호·사전 동의 gate 제거·Host의 노드/Continue 권한 | 서로 다른 69건 최종 성공·AI 복구 3건·Snapshot PIE. `04ed243` |
| 6. 로컬 통합 완료 | 관리 v4·영속 Human 목록·번호순 Host·싱글 전환 메뉴·현재 Human 복구·lease 검사 | 전체 75건·실제 관리 PIE 2건·AI 복구 3건·Snapshot PIE. `122f756` |

### 검증 범위

- Snapshot은 로컬 카탈로그·지원 수치·빈 장비/전술 ID 범위다. 온라인 상대·결과 검증은 미구현이다.
- 서버 명령은 연결·소유권·Run/Host/전투·순번을 검사한다. Accepted는 실행 승인으로 비동기 행동 성공과 구분한다.
- 확정 턴은 저장 성공 후 다음 턴을 시작한다. 지원하지 않는 GAS 상태·난수 복구는 제외한다.
- 관리 재개는 3→2인 Host 승계와 4번의 단독 전환·travel 실패 재시도를 검증했다. 이후 추가된 4→3인 승계는 최신 검증 대기다.
- AI 사전 동의 정책은 폐기되었으며 필드는 직렬화 호환용이다. 현재 정책과 저장/실패 계약은 [MULTIPLAYER](MULTIPLAYER.md)에 정의한다.

## 4. 실패에서 확인한 원인과 수정

| 시점 | 확인한 문제 | 수정과 최종 확인 |
|---|---|---|
| 초기 Gameplay | 빈 NavMesh와 CommonUI Menu 입력이 이동/타일 클릭을 차단 | Gameplay 내비게이션 저장, HUD 입력 설정·travel 잠금/포커스 수정, 실제 Slate 입력 추가 |
| T04 | 창 배치에 따른 Slate hit-test 실패 | 창 위치·크기를 명시한 동일 빌드의 PIE와 최종 전체 실행 통과 |
| T05 / 명령 경계 | 격리 fixture의 Controller 미등록·로컬 Controller 설정 누락 | 테스트 초기화 수정 후 전체/집중 실행 통과. 실제 Gameplay 경로와 구분 |
| T07 / T09 | 타이머/예약 활성화 누락과 테스트 배열 자기 참조 | 프레임 진행·예약 준비 및 복사 후 배열 추가로 테스트 자체 보정 |
| T14 3번 | PIE 시작 시 재생성 중 NavMesh가 비어 복제되어 이동 실패 | 편집기 내비게이션 완료·유효 경로를 기다림. 맵 범위나 이동 성공 조건은 완화하지 않음 |
| T14 3번 | Client BeginPlay 문맥 덮어쓰기·재전송 Pending 고착·타일 색 캐시 순서 | 복제 초기화와 응답/표시 경로 보정 후 실제 네트워크·전체 회귀 통과 |
| T14 4번 | 저장 안내 Overlay가 클릭 통과 설정을 잃음 | 루트 hit-test 처리 수정 후 기본/상대 Snapshot의 Slate 전투 루프 통과 |
| T14 5번 | fixture Host가 죽어 Host 차례를 영원히 기다림 | 로그로 AI 턴 종료 정상 확인, Host와 저장 Party HP 보정. 원래 행동 횟수·복제 조건 유지 |
| T14 7번 독립 | `UNetConnection::Close` 링크 의존성과 의도한 연결 종료 Error | Editor `NetCore` 추가, 종료 직전 해당 Error 1회만 예상. 다른 오류는 계속 실패 |
| 정책 반영 | Host 진행 UI가 활성화되기 전 버튼을 검사 | CommonUI 활성 화면/버튼 준비 대기 후 실패했던 3건 재실행 성공 |
| 관리 Run | 메뉴 travel 실패 후 lease 잔존·v4 오류 이유 소실 | GameInstance 수명의 동일 실행 travel 실패 감시, 확정 바이트 보존·lease 반환·메뉴 재시도 연결 |
| 관리 PIE | namespace가 저장소 최대 32자를 초과 | fixture를 31자로 수정 후 실제 PIE 2건 성공. production 제한 유지 |

초기 R01~R06 리뷰 결함은 T01~T06에서 모두 후속 처리했다. 오래된 리뷰의 멀티플레이 미검증 문장은 현재 상태가 아니므로 미해결 결함으로 재등록하지 않는다.
기존 경고에는 축소 월드·Spawn/GAS Cue 설정·종료 중 Nav 조회·의도한 거절/취소가 포함됐다. 경고 동반 성공을 경고 없는 실행이나 패키지 검증으로 바꾸어 기록하지 않는다.

## 5. 대표 검증 산출물

아래 경로는 당시 로컬 `Saved` 산출물이다. Git에 포함되지 않아 다른 checkout에는 없을 수 있으며, 전체 로그 복사 대신 대표 결과만 남긴다.

| 범위 | 대표 기록 |
|---|---|
| 초기 Slice·입력 | `Saved/Automation/GameplayInputFix2/index.json`, `GameplayAssetValidation.json` |
| T03~T07 | `T03APCost1`, `T04TargetingFinal`, `T05InputFinal2`, `T06AreaFinal`, `T07CompletionFinal3`의 `Saved/Automation` 결과 |
| T09~T13 | `T09ProfessionFinal4`, `T11Final`, `T12Final`, `T13Final`의 결과와 T11 패키지 `T11PackWrite`/`T11PackContinue` 로그 |
| Snapshot·소유권·명령 | `T14Full`, `T14SnapshotPIE`, `T14OwnershipFull`, `T14OwnershipRestartWrite/Read`, `T14CommandsFull2` |
| 2인·확정 턴 | `T14NetworkFull`, `T14CheckpointFull3`, `T14CheckpointRestartWrite/Read`, `T14CheckpointSnapshotReplace/Delete` |
| 아군 AI | `T14PartyAIFull2`, `T14PartyAICoop2`, `T14PartyAIRestartWrite/Read` |
| 3·4인·추가 정책 | `T14ScaleFull1`, `T14ScaleAI1`, `T14StandaloneContinueFull1`, `T14HostPolicyFull1`, `T14HostPolicyCombat2` |
| 관리 Run 통합 | `T14ManagedRunBuild3.log`, `T14ManagedRunFull1/index.json`, `T14ManagedRunPIE2/index.json`, `T14ManagedRunAI1/index.json`, `T14ManagedRunSnapshot1/index.json` |

T14의 Writer/Reader·AI·관리 PIE는 전용 인자가 없는 전체 실행에서 안내만 남기는 경우가 있다. 위 전체 건수와 별도 실제 실행 기록을 함께 읽는다.
정책 반영 69건은 최초 66건 성공과 UI 준비 보정 후 3건 성공을 합친 서로 다른 테스트 집계다. 수정 후 전체를 한 번에 다시 통과한 기록은 아니다.
관리 통합의 전체 75건은 성공 51·경고 동반 성공 24·실패 0이며, 관리 PIE 2건은 전용 인자로 별도 실행했다.
3·4인 정원 검증은 엔진 입장 검사에서 4명 이후 거절을 확인했다. 실제 다섯 번째 PIE 접속을 실행한 것으로 기록하지 않는다.

## 6. 검증 제한

T14 1~6번은 `122f756`까지 로컬 구현·검증을 완료했다. 최신 4→3인 승계의 Client별 바인딩·턴 종료 응답 검증은 대기 상태다. 기존 전투·Host 복구 성공으로 대체하지 않는다.

Steam/PlayFab·중앙 저장·결과 중복 방지·MMR은 미구현이다. 실제 마우스 조작·이동 중 취소·최신 패키지·네트워크 지연/손실 검증은 기존 자동화와 구분한다. 작동 테스트는 명시적 요청 범위에서만 Codex가 실행한다.

## 7. 테스트 인계·문서 통합

- 2026-09-10: 사용자 중심 작동 검증 원칙을 AGENTS·README에 반영했다. 이후 명시적 요청에 한해 Codex가 실행한다.
- Docs 14개를 기획·구조·작업·계약·검증·이력의 6개로 통합했다. 원문은 Git 이력에서 조회한다.
- 직전 지시 이전에 시작된 `Saved/Automation/T14ManagedScaleFull1/index.json`은 76건(성공 49·경고 동반 성공 27·실패 0)이었다. 이 결과에는 전용 인자로 실행한 관리 PIE 3건이 포함되지만, 실행 뒤 추가한 최신 바인딩 복제 대기 조건은 포함하지 않는다. 최신 소스 작동 성공으로 계산하지 않는다.
- Unity 빌드 보조 함수명 충돌·바인딩 대기 변경의 Editor 빌드는 `-DisableAdaptiveUnity`로 성공했다. 로그: `Saved/Automation/T14ManualHandoffBuild1.log`.
- 인계 당시 작동 테스트는 미실행이다. 최신 7번 승계는 [TEST_REPORT](TEST_REPORT.md)의 검증 대기 상태다.

## 8. 서비스 준비 점검

- 2026-09-11: 7번 승계 검증 대기, Steam App ID·PlayFab Title 미준비를 확인했다.
- UE 5.7 설치 소스에서 OnlineSubsystemSteam·SteamSockets와 IP 전용 SocketSubsystemSteamIP의 차이를 확인했다. 프로젝트 서비스 설정·PlayFab 플러그인은 없는 상태다.
- 공식 안내를 대조해 공용 Steam ID 480 개발 연결과 자체 앱 등록 수수료, PlayFab Foundation 전환·Xbox 관련 자격 조건, 계정/Title 준비 절차를 MULTIPLAYER와 TEST_REPORT에 추가했다.
- 점검 범위는 문서·정적 조사다. 가입·결제·SDK 설치·실행·설정 변경은 미수행이며 온라인 연동은 미구현이다.
