# ProjectA 완료 작업과 검증 기록

정리일: 2026-09-10. 완료 이력의 기준 커밋은 `122f756`이며 `origin/main` push까지 확인된 범위다.
현재 작업 상태와 다음 할 일은 [TODO](TODO.md), 사용 방법은 [README](../README.md)를 따른다.

이 문서는 이전 `CODE_REVIEW.md`, `VERTICAL_SLICE_REPORT.md`, `T14_QUEUE.md`와 TODO의 중복 기록을 요약한다.
과거 시점의 미구현·정책 대기 문구를 현재 결함이나 현재 정책으로 옮기지 않는다.
아래 작동 검증은 당시 수행한 기록이다. 이번 정리에서는 남아 있던 테스트 소스의 컴파일만 확인했으며 새 작동 테스트는 실행하지 않았다.
`122f756` 이후 4→3인 승계 테스트 변경은 사용자 작동 검증 대기이며, 이전 성공 기록으로 완료 처리하지 않는다.

## 2026-09-11 콘텐츠 위치 규칙 후속

- 사용자·Codex 제작 에셋의 기준 경로를 `Content/User_JeHoon`으로 확정했다. 외부 리소스·템플릿 원본은 유지하고 편집 사본을 작업 폴더에 만든다.
- 별도 T12Validation의 메뉴 검증 위젯 3종을 `User_JeHoon/Validation/T12`로 이동하고 Git 추적 대상에 포함했다. UI 생성 도구의 경로 제한과 검증 코드·문서·로컬 생성 명세를 갱신했다.
- Unreal AssetTools 이동·저장과 별도 프로세스의 패키지 조사를 완료했다. 이전 경로 에셋·Redirector 0개, 이동된 위젯 로드 3개, Development Editor / Win64 컴파일 성공. 작동 테스트는 [TEST_REPORT E](TEST_REPORT.md#e-엔진-콘텐츠-경로-통일)에서 사용자 확인 대기다.

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

### Snapshot과 명령의 검증 경계

- Snapshot의 장비·전술 ID는 저장할 수 있지만 실행은 빈 값만 지원했다. 미지원 버전·카탈로그 불일치·알 수 없는 ID·실행할 HP 0은 명시적으로 거절했다.
- Snapshot 상대는 기존 PvE 유닛/전투를 사용하며 회복약과 무작위 추가 스킬을 새로 지급하지 않았다. 기본 PvE와 명령줄 상대별 Run 슬롯을 분리했다.
- 서버는 요청 payload의 계정·비용·Actor를 신뢰하지 않는다. 실제 장착 스킬, 연결 바인딩, Run/Host/전투 ID와 순번을 검사했다.
- 같은 문맥에서 거절된 요청도 순번을 소비해 나중에 자원·점유·입력 잠금이 바뀌어 재실행되지 않게 했다. 재접속 바인딩과 전투 실행 ID는 새로 발급했다.
- `Accepted`는 행동 전달 승인이다. 이동·스킬의 비동기 성공은 행동 완료 결과에서 별도로 확인했다.

### 턴 저장·AI·승계의 검증 경계

- 확정 경계는 이전 턴 종료 → 다음 유닛 선택 → 파일 교체 성공 → 다음 턴 시작이다. 저장 실패 시 같은 후보·기존 기록을 유지하고 인간/AI 입력을 막았다.
- 복구는 저장된 턴·HP/AP/SubAP·재고·사망·타일·장착·상대 본문을 사용했다. 사망 이벤트와 완료 콜백을 다시 실행하지 않았다.
- 지원하지 않는 활성 GAS 능력·지속 효과·쿨다운과 잘못된 실제 XY/타일 배치는 저장 또는 스폰 전에 거절했다. 임의 효과/난수 복구 완료를 주장하지 않는다.
- 아군 AI는 자기 회복약 → 아군 피해 없는 공격 → 접근 이동 → 턴 종료를 판단했다. 원래 소유자의 인간 요청도 AI 모드에서는 거절했다.
- 최초 AI 구현의 사전 동의 정책은 이후 사용자 답변으로 폐기했다. 기존 동의 필드는 직렬화 호환용이며 Unknown/Declined도 AI 실행·저장/복구를 허용하도록 다시 검증했다.
- 최초 Host는 1번, 최초 합류 번호는 고정한다. 명시적 재개의 현재 인간 중 가장 작은 번호가 Host이며 다른 캐릭터의 소유권을 얻지 않는다.
- 개인 사전 동의 없이 Host가 AI 전환을 결정하고 전환한 캐릭터는 해당 Run 종료까지 AI를 유지한다. 현재 Host만 노드·Continue를 결정한다.
- 관리 v4는 Human 목록을 결과/Map/다음 전투까지 보존하며 목록의 재확대를 거절한다. 일반 v1/v2/v3에는 자동 승격이나 관리 저장 우회를 넣지 않았다.
- 관리 종료는 행동·콜백 중단 후 lease를 반환한다. 직접 lease를 닫은 뒤 살아 있는 옛 Authority의 인간/AI 요청·재바인딩·모드 변경도 거절했다.
- 실제 3→2인 검증은 원래 2번 Host·3번 Client·1번 AI로 새 세션을 복구하고 결과→Continue→다음 전투를 확인했다.
- 실제 4→1인 검증은 4번이 메뉴 버튼으로 싱글 전환하고 나머지를 AI로 유지했다. 비맵 travel 실패→새 메뉴 재시도→정상 전투도 확인했다.
- 위 관리 저장소는 같은 PC의 개발용 공유 파일·신원 주입이다. 원격 중앙 저장·Steam 로그인·P2P 초대·MMR 결과 신뢰의 증거가 아니다.

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

## 6. 이력과 현재 대기의 구분

- `122f756`까지의 6번은 로컬 개발 범위의 구현·과거 작동 검증·커밋·push가 끝났다.
- 이후 7번의 원래 4명 → 현재 인간 3명/불참 AI 1명 테스트 일반화와 각 Client의 독립 계정·바인딩·턴 종료 Accepted RPC 검사를 추가했다.
- 이 7번 후속 변경은 사용자 작동 검증 대기다. 이전 3·4인 기존 Host 복구나 3→2인 승계의 성공을 새 시나리오 성공으로 대체하지 않는다.
- 온라인 준비 문서는 완료됐지만 실제 Steam/PlayFab 연결·중앙 Run/Snapshot 저장·결과 중복 방지·MMR은 완료되지 않았다. 무료 개발 범위가 영구 운영비 0원을 보장하지 않는다.
- 전체 물리 마우스 플레이, 실제 이동 중 사용자의 취소, 후속 변경의 패키지/네트워크 손실·지연 검증은 기존 합성 입력·phase 주입·과거 T11 패키지 검증과 구분한다.
- 앞으로의 작동 테스트는 사용자가 직접 수행한다. 새 결과는 실행한 사람이 확인한 범위만 기록하며 이력 정리 때문에 이전 검증을 반복하지 않는다.

## 7. 사용자 테스트 인계와 문서 통합

- 2026-09-10 사용자 요청에 따라 PIE·게임·Unreal 자동화·패키지 작동 테스트를 Codex가 시작하지 않도록 AGENTS와 README를 갱신했다. 컴파일·정적 검사와 사용자 테스트 보고서를 구분한다.
- Docs의 14개 Markdown을 GAME_DESIGN·PROJECT_PLAN·TODO·MULTIPLAYER·TEST_REPORT·HISTORY의 6개로 통합했다. 각 세부 작업마다 새 문서를 만드는 대신 기존 문서를 갱신한다. 통합 전 원문은 Git 이력에 남아 있다.
- 직전 지시 이전에 시작된 `Saved/Automation/T14ManagedScaleFull1/index.json`은 76건(성공 49·경고 동반 성공 27·실패 0)이었다. 이 결과에는 전용 인자로 실행한 관리 PIE 3건이 포함되지만, 실행 뒤 추가한 최신 바인딩 복제 대기 조건은 포함하지 않는다. 최신 소스 작동 성공으로 계산하지 않는다.
- Unity 빌드의 보조 함수명 충돌과 바인딩 준비 대기 변경을 포함한 Development Editor / Win64 컴파일은 `-DisableAdaptiveUnity`로 성공했다. 이번 로그는 `Saved/Automation/T14ManualHandoffBuild1.log`다.
- 이번 인계에서는 새 작동 테스트를 실행하지 않았다. 최신 7번은 [TEST_REPORT](TEST_REPORT.md)의 사용자 결과 대기로 유지한다.

## 8. 2026-09-11 재개 준비

- 사용자가 7번 작동 테스트 미실행과 Steam App ID·PlayFab Title 미준비를 확인하여 검증 대기와 서비스 준비 상태를 유지했다.
- UE 5.7 설치 소스에서 OnlineSubsystemSteam·SteamSockets와 IP 전용 SocketSubsystemSteamIP의 차이를 확인했다. 프로젝트 서비스 설정·PlayFab 플러그인은 없는 상태다.
- 공식 안내를 대조해 공용 Steam ID 480 개발 연결과 자체 앱 등록 수수료, PlayFab Foundation 전환·Xbox 관련 자격 조건, 계정/Title 준비 절차를 MULTIPLAYER와 TEST_REPORT에 추가했다.
- 이번 변경은 기존 문서 갱신뿐이다. 가입·결제·SDK 설치·컴파일·작동 테스트·기본 실행 설정 변경은 수행하지 않았다. 온라인 구현 완료로 기록하지 않는다.
