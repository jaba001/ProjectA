# ProjectA 코드 리뷰

기준일: 2026-09-08 · `main` / `2b0d9ea` + 현재 작업 트리.

검토 범위는 전투 초기화·턴·이동·스킬·AI·메뉴 생성/전환의 주요 C++ 경로, 스킬 데이터 정의, UI 생성 commandlet의 검증/저장 경로다. 전체 에셋 또는 모든 코드에 대한 무결성 보증은 아니다. 아래 기존 결함 근거는 수정 전 경로를 설명한다. 2026-09-08에 R01/R02와 Vertical Slice를 구현했으며, 정식 빌드·자동화·PIE 실행 결과는 [최종 보고](VERTICAL_SLICE_REPORT.md)에서 구분한다.

P1은 전투 진행 정지 또는 행동 상태 훼손을 먼저 해결할 항목, P2는 특정 데이터·입력 조건에서 규칙이 달라지는 항목이다. 추정 기획과 미구현 기능은 [기획 현황](PROJECT_PLAN.md), 실제 작업은 [TODO](TODO.md)에서 관리한다.

자동화 검증: P1/Run 7건 성공, 실제 saved maps PIE 자연 Victory/Continue/두 번째 Encounter/Defeat 1건 성공. R01 제자리 몽타주와 no-montage를 모두 실행했고, R02 접근/복귀 실패·취소는 phase 주입 방식으로 검증했다. 전체 UI 마우스 조작과 패키징은 별도다.

## P1

### Gameplay 후속. CommonUI 메뉴 입력 모드가 전투 타일 클릭을 차단 — 수정 및 Slate 입력 PIE 검증 완료

- **원인:** CombatHUD의 `GetDesiredInputConfig` 누락으로 ActionRouter가 기본 Menu 모드를 유지했다. GameplayController의 `SetInputMode(GameAndUI)`는 Router 내부 모드를 바꾸지 않아 Slate에서 도착한 월드 클릭이 차단됐다. 기존 테스트는 `StartMoveAction`/`StartSkill` 직접 호출로 이 경로를 우회했다.
- **수정:** CombatHUD는 All/CaptureDuringMouseDown, RunMap/Result는 Menu/NoCapture를 반환한다. GameplayController의 화면별 수동 입력 설정은 제거하고, MainMenu UIOnly에서 travel 후 남는 viewport IgnoreInput 해제와 로컬 플레이어의 최초 뷰포트 포커스 복원을 수행한다.
- **검증:** 실제 Slate 합성 마우스 이벤트로 Move/Skill 버튼과 타일을 클릭하여 게임 뷰포트 전달 1회, 이동·점유·GAS 피해를 검증했다. 메뉴/결과 차단 및 후속 전투 입력 복귀도 통과. 정식 빌드 및 전체 자동화 8건 성공; 로그는 GameplayInputFix2와 최종 보고 참조. 물리적 마우스로 전체 흐름을 수동 플레이한 검증과는 구분한다.

### R01. 제자리 스킬이 행동 중 상태와 완료 처리에 진입하지 않음 — 수정 및 PIE/자동화 검증

- **현재 수정:** 이동과 독립된 공통 행동 소유 상태 및 결과를 도입했다. GAS 종료 delegate를 활성화 전에 연결하고 context 정리 후 완료를 1회 통지한다. 제자리 스킬은 이동 복귀를 요구하지 않는다. 활성화 거절/동기 종료/취소 회귀 테스트 추가. 실제 실행 결과는 최종 보고 참조.

- **근거:** [UnitBase.cpp](../Source/ProjectA/Unit/UnitBase.cpp) `StartSkill`, `OnSkillFinished`. `bMoveToTarget=false` 경로는 Ability를 실행하지만 `MovePhase`를 `WaitingForSkill`로 바꾸지 않는다. 완료 함수는 그 상태가 아니면 즉시 반환한다.
- **영향:** 몽타주 실행 중에도 `IsBusy()`가 false여서 이동/다른 스킬/턴 종료 입력을 막지 못하고 Pending 컨텍스트가 덮어써질 수 있다. 적은 [EnemyUnit.cpp](../Source/ProjectA/Unit/EnemyUnit.cpp)의 `WaitSkillComplete` 이후 복귀 콜백을 기다리므로 제자리 스킬 뒤 다음 행동으로 이어지지 않는다.
- **검증 시나리오:** 유효한 Ability와 몽타주가 있는 `bMoveToTarget=false` 스킬을 플레이어와 적에게 각각 장착한다. 실행 중 이동/턴 종료를 요청하고, 종료 후 적이 다음 행동 또는 턴 종료로 진행하는지 확인한다. 몽타주 없는 즉시 종료도 별도로 확인한다.
- **수정 방향:** 이동 여부와 무관한 행동 시작/완료 규약을 만들고 제자리 실행은 이동 없이 정상 정리한다. 적의 대기 상태는 동기 완료가 발생하기 전에 설정하고, 시작 거절도 별도 결과로 처리한다.
- **연결 작업:** T01.

### R02. 스킬 이동 실패 후 적 AI가 대기 상태에서 빠져나오지 못함 — 수정 및 phase 주입 자동화 검증

- **현재 수정:** EUnitActionResult 성공/실패/취소가 AI까지 전달된다. AI는 StartSkill 이전에 Wait를 설정하며 완료 다음 틱에 재판단/안전한 EndTurn으로 전이한다. 실패/취소는 예약 원타일과 행동 시작 transform으로 복구한다. 접근 전 실패 AP 미차감, 이후 소비된 AP는 환불하지 않는다. phase 주입 테스트와 실제 NavMesh 이동 검증은 구분한다.

- **근거:** [UnitAIController.cpp](../Source/ProjectA/Controller/UnitAIController.cpp) `OnMoveCompleted`는 실패 시 `HandleMoveFailed`를 호출한다. [UnitBase.cpp](../Source/ProjectA/Unit/UnitBase.cpp)은 스킬 컨텍스트를 지우고 반환하며, [EnemyUnit.cpp](../Source/ProjectA/Unit/EnemyUnit.cpp)의 스킬 실행은 이후 `WaitSkillComplete`로 전환한다. 적의 재판단/종료는 정상 스킬 완료와 복귀 콜백에 연결되어 있다.
- **영향:** 적이 대상에게 접근하거나 원래 타일로 돌아오다가 실패·취소되면 유닛은 더 이상 busy가 아니지만 적 상태는 대기 중으로 남아 자동 진행이 멈출 수 있다. 기존 실패 분기는 성공으로 잘못 처리하는 문제는 막지만 AI 복구까지 완료하지는 않는다.
- **검증 시나리오:** 적 근접 스킬의 접근 도중과 복귀 도중 각각 이동을 취소한다. 다음 행동 또는 턴 종료가 한 번 발생하는지, 월드 위치와 타일 점유가 일치하는지 확인한다. Ability 활성화 거절 상황도 함께 확인한다.
- **수정 방향:** 성공/실패/취소 모두 공통 행동 종료 결과를 전달하고, 적이 재판단 또는 턴 종료를 선택하도록 한다. 컨텍스트를 지우기 전에 복귀 타일·실패 원인을 확보하고 자원 환불 정책도 정한다.
- **연결 작업:** T02.

## P2

### R03. 스킬 사용 가능 판정과 실제 AP 차감이 서로 다른 값을 사용함 — 수정 및 자동화·PIE 검증 완료

- **수정 전 근거:** [SkillDefinitionDataAsset.h](../Source/ProjectA/DataAsset/SkillDefinitionDataAsset.h)과 [GA_AttackBase.h](../Source/ProjectA/GAS/Ability/GA_AttackBase.h)에 각각 `ActionPointCost`가 있었다. 입력/`StartSkill`은 전자를, [GA_AttackBase.cpp](../Source/ProjectA/GAS/Ability/GA_AttackBase.cpp) `ActivateAbility`는 후자를 사용했다.
- **수정 전 발생 조건과 영향:** DataAsset 비용 1, Ability 비용 2인 스킬에서 AP 1인 유닛은 입력 검사를 통과하지만 Ability 내부에서 종료했다. 반대 조합은 표시보다 적게 차감했다.
- **검증/수정:** 두 비용을 일부러 다르게 설정해 표시·선택 가능 여부·차감량을 비교한다. 비용의 단일 기준을 정하고 실패 시 T01/T02의 완료 규약을 따른다.
- **현재 수정:** DataAsset 비용을 HUD·플레이어 입력·적 후보 평가·StartSkill·GAS 차감의 기준으로 통합했다. 과거 Ability 비용은 편집할 수 없는 deprecated 호환 필드로만 남긴다. 비용은 1 이상이어야 하며 음수 차감으로 AP를 얻는 경로도 차단한다. AP 차감은 컨텍스트 검증과 GAS 커밋 이후로 옮겼다.
- **실행 결과:** 2026-09-10 정식 빌드 성공, 전체 자동화 11건 성공(실패 0). `ProjectA.Combat.Costs.DefinitionAndInput`은 비용 1/2/3/0/-1과 AP 1/2, 표시 문자열·입력 허용·실제 피해·차감량 및 과거 비용 99 무시를 확인한다. `ActivationFailure`는 어빌리티 누락·GAS 태그 차단·빈 타일의 공격 컨텍스트 실패에서 자원 보존과 1회 완료, 정상 재시도를 확인한다. `EnemyAffordableSkill`은 비용을 지불할 수 없는 우선 스킬 대신 가능한 스킬을 선택하는지 확인하며 피해 검증은 이 테스트의 범위가 아니다. 저장 맵 PIE는 실제 HUD 비용 표시·버튼 활성화와 기존 Slate 클릭/전투 루프를 통과했다. [결과 JSON](../Saved/Automation/T03APCost1/index.json), [로그](../Saved/Logs/T03APCost1.log) 참조.
- **검증 경계:** 테스트 6건에 기존 환경/의도적인 잘못된 데이터/간소화된 그리드 fixture 경고가 남는다. GAS 커밋 자체의 실패 및 AP 차감 이후 중단을 새로 주입한 테스트는 포함하지 않는다. 이미 소비한 AP의 비환불 정책은 유지하며 범위 타입 통합은 T06으로 남긴다.
- **연결 작업:** T03.

### R04. 적 타겟 선정이 전열 보호와 스킬 대상 규칙을 적용하지 않음 — 수정 및 자동화·PIE 검증 완료

- **수정 전 근거:** [EnemyUnit.cpp](../Source/ProjectA/Unit/EnemyUnit.cpp) `FindBestSkillTarget`은 살아 있는 Player 팀만 점수 평가했다. [PartyPlayerController.cpp](../Source/ProjectA/Controller/PartyPlayerController.cpp) `IsValidTileForPendingSkill`의 TargetRule·전열 보호 검사를 공유하지 않았다.
- **발생 조건과 영향:** `bIgnoreFront=false`인 적 기본 공격이 보호된 후열을 더 높은 점수로 고르면 직접 공격할 수 있다. AllyUnit 스킬도 플레이어를 후보로 삼는다.
- **검증/수정:** 전열 생존/사망 상태와 EnemyUnit/AllyUnit 규칙을 조합해 플레이어·AI의 허용 대상 목록을 비교한다. 명시적인 시전자·스킬·타일을 받는 공통 검증을 AI 선택과 실행 진입에 적용한다.
- **현재 수정:** `CombatTargetingLibrary::IsValidSkillTarget`으로 플레이어·AI·실행 진입을 통합했다. AI는 타일을 순회해 아군/자기 자신/빈 타일도 규칙에 맞게 평가한다. 실행 직전에 상태와 점유를 재검증하여 선택한 유닛이 바뀐 요청은 AP 소비 전에 실패 처리한다. 제자리 타일 스킬은 현재 점유자를 따른다.
- **규칙 경계:** 기존 플레이어 기준을 유지하여 전열 보호는 `EnemyUnit`에만 적용한다. `AnyUnit`과 타일 규칙으로 선택한 대상에 새 보호 규칙을 추가하지 않는다. 접근형 스킬은 다른 생존 유닛을 요구한다. 범위 효과의 실제 피해 대상 필터링은 T06으로 남긴다.
- **검증:** `RulesAndPlayerSelection`의 576개 조합, `EnemySelection`의 실제 AI 선택, `ExecutionRevalidation`의 직접 거절/대기 phase 주입/재시도 검증 통과. 정식 빌드와 기존 행동·AP·Run·Slate PIE를 포함한 최종 전체 자동화 14건이 통과했다. 첫 PIE hit-test 실패와 창 조건을 명시한 재실행 등 상세 근거는 [T04](TODO.md) 참조.
- **연결 작업:** T04.

### R05. 플레이어 입력의 행동 가능 검사에 소속 팀 제한이 없음 — 처리 주체 분리 및 자동화·PIE 검증 완료

- **현재 수정:** 타일 클릭의 이동/스킬 실행을 PlayerController로 집중했다. 플레이어 진입은 권한·입력 잠금·Player 팀·활성 전투/턴·생존·busy를 검사한다. AI는 명시한 요청 유닛을 검증하는 `RequestEndTurnForUnit(this)`를 사용한다. 기존 인자 없는 Blueprint 종료 진입점도 플레이어 검사를 거치도록 변경했다. 내부 `AdvanceTurn`은 private이며 턴 전환/전투 종료에 선택 정리를 연결했다.

- **수정 전 근거:** [PartyPlayerController.cpp](../Source/ProjectA/Controller/PartyPlayerController.cpp)의 초기 `CanUseActiveUnitAction`은 생존·활성 턴·busy만 검사했다. slice에서 팀 검사는 추가했지만 [CombatGridTile.cpp](../Source/ProjectA/Grid/Combat/CombatGridTile.cpp)에 명령 처리가 남아 있었고, CombatManager의 인자 없는 종료 요청은 호출 유닛·busy를 확인하지 않았다.
- **발생 조건과 영향:** 적이 활성 턴이면서 busy가 아닌 구간에 HUD가 입력을 전달하면 플레이어가 적 이동/스킬/턴 종료를 요청할 수 있다. 특히 R01/R02의 대기 정지 상태에서 구간이 길어진다. Blueprint HUD가 별도로 차단하는지는 확인하지 못했다.
- **검증/수정:** 적 턴의 비busy 상태에서 이동·스킬·종료 버튼을 실행한다. 플레이어 입력 진입점에서 조작 가능한 팀을 검사하고, AI의 턴 종료는 CombatManager 등 별도 내부 경로로 옮긴다. 공용 함수에 Player 제한만 추가하면 AI 종료가 깨질 수 있다.
- **연결 작업:** T05.
- **검증 범위:** `PlayerAndEnemyTurnIsolation`은 실제 AEnemyUnit의 취소 후 다음 틱 전 비busy 구간에 플레이어 이동/스킬/타일/종료 및 기존 Blueprint 진입점을 호출하고, UI가 잠겨도 AI가 정상 종료하는지 확인한다. `TileCommandsAndTurnGuards`는 요청자/활성 상태/busy/사망/전투 종료 검사와 실제 타일 클릭의 AP 재검사·피해를 확인한다. 정식 빌드와 저장 맵 Slate PIE를 포함한 전체 자동화 16건 통과. 상세 실행 근거는 [T05](TODO.md) 참조. 네트워크 다중 클라이언트 검증은 T14 범위다.

### R06. 스킬 범위 계산이 직접 효과와 스폰 액터 경로에서 다름 — 공통 계산 및 검증 완료

- **근거:** [GA_AreaAttack.cpp](../Source/ProjectA/GAS/Ability/GA_AreaAttack.cpp) `ResolveCenterTile`은 AroundSelf에 시전자 타일을 사용한다. [AttackSkillActorBase.cpp](../Source/ProjectA/Combat/SkillActor/AttackSkillActorBase.cpp) `ResolveImpactTargetUnits`은 Single 외 모든 타입을 TargetTile 중심 반경으로 계산한다. [SkillTypes.h](../Source/ProjectA/Types/SkillTypes.h)에 선언된 Row/Column/AllEnemies 등의 개별 계산도 없다.
- **발생 조건과 영향:** AroundSelf에서 시전자와 다른 타일을 타겟으로 넘기면 직접 피해와 스폰 액터 피해 위치가 달라진다. Row 등의 타입은 직접 효과에서는 중심 타일을 얻지 못하고, 스폰 경로에서는 주변 사각 범위로 처리한다.
- **검증/수정:** 동일 스킬의 직접 효과/스폰 액터 실행에서 대상 집합을 비교한다. 공통 범위 계산을 사용하고 미지원 타입은 에디터 검증에서 명시적으로 거절하거나 구현한다.
- **현재 수정:** 공통 중심/대상 수집을 두 범위 효과 경로와 UnitBase에 연결했다. AroundSelf는 시전자 타일 중심이며 시전자 제외를 유지한다. 미지원 타입과 음수 반경은 에셋 IsDataValid 및 AP 소비 전 검사로 거절하고 임팩트에서도 재확인한다.
- **검증:** 72조건의 직접/실제 스폰 피해 및 AP·행동 완료 일치, 미지원 거절·정상 재시도 검증. 정식 빌드와 기존 Slate PIE를 포함한 전체 18건 통과. 실행 자료와 범위 경계는 [T06](TODO.md) 참조.
- **연결 작업:** T06.

## 완료로 오해하기 쉬운 미구현/설계 항목

| 항목 | 코드에서 확인한 경계 | 연결 작업 |
|---|---|---|
| 파티 데이터 이관 | 4슬롯 runtime data → RunStateSubsystem → Gameplay/Encounter 스폰 연결 구현, 디스크 저장 없음 | T08 |
| Continue / Options / Quit | MainMenuScreenWidget의 처리 함수가 로그만 출력 | T11 |
| 캐릭터 편집/직업 설명 | 슬롯 이름·직업 저장/취소와 읽기 전용 ClassInfo 연결 | T09 |
| 실제 스탯 미리보기 | 공통 직업 정의의 HP/AP/보조 AP·시작 스킬을 UI와 스폰에 함께 적용 | T09 |
| 아이템 효과 | `ExecuteItemAtTarget`이 효과 적용 없이 종료 | T13 |
| 적 이동 의사결정 | 스킬/대기만 평가, 이동 완료 후 AI 전이도 보완 필요 | T13 |
| 스킬 액터 완료 책임 | 몽타주+impact 완료 후 GAS 종료. 조기 종료/파괴/시간 초과 실패, 취소·사망 시 대기 액터 제거 및 1회 통지 검증 완료. 전체 자동화 20건 통과 | T07 |
| 전투 종료 | TurnManager 결과 1회 → 전투 잠금 → Encounter next tick 정리 → Result/Run Map 연결 구현 | T10 |
| 메뉴 재진입 프리뷰 | Back 경로에서 프리뷰 일괄 정리는 확인되지 않음. 화면 재진입·위젯 재사용 시 유지/정리 정책 검증 필요 | T12 |
| 멀티플레이 | 일부 복제 선언만 있으며 기능 전체의 클라이언트 동기화는 미완성 | D01 / T14 |

## 검증 기록과 남은 확인

이번에 수행한 것은 소스 호출 경로 추적, TODO 검색, Git 상태/최근 이력 확인, 문서 링크와 관련 파일 존재 확인이다. **2026-09-07 당시에는 UE 빌드, PIE, 패키징, commandlet을 실행하지 않았다. 2026-09-08의 실제 결과는 VERTICAL_SLICE_REPORT.md가 기준이다.** 기존 작업 트리의 변경 에셋과 미추적 파일을 포함해 검토했지만 바이너리 에셋 내부를 읽어 완료 판정을 하지는 않았다.

| 검증 ID | 실행할 시나리오 | 통과 기준 |
|---|---|---|
| V01 | 플레이어/적, 접근형/제자리형, 몽타주 유/무 스킬 | 실행 중 입력 차단, AP 1회 소비, 행동 종료 1회, 다음 행동 가능 |
| V02 | 접근/복귀 취소·실패, 활성화 거절 | 컨텍스트와 점유 일치, 적 무한 대기 없음, 자원 정책 일치 |
| V03 | 전열 보호, 아군/적군/타일 타겟, 적 턴의 사용자 입력 | 허용 대상과 조작 가능한 팀 일치 |
| V04 | 비용 불일치·AP 부족·지원/미지원 범위 타입 | 잘못된 데이터 조기 검출, 실패 후 행동 가능, 직접/스폰 대상 일치 |
| V05 | 마지막 적/플레이어 사망, 사망 유닛 턴 건너뛰기 | 결과 1회 확정, 종료 후 추가 행동 금지, 정상 복귀 |
| V06 | 빈 슬롯·1명·4명·삭제 후 시작·다른 직업 조합 | 확정한 생성 규칙 적용, 레벨 전환 후 동일 파티 유지 |
| V07 | 새 게임→Back/X→재진입 반복, 프리뷰 클래스 누락 | 배경 입력/카메라/프리뷰 상태가 정책과 일치 |
| V08 | Designer WBP와 native fallback 각각 실행 | 슬롯·버튼·스택 정상 작동, 실제 생성 spec과 바인딩 일치 |
| V09 | Development Editor 빌드와 JSON 3종 DryRun, 별도 생성본 검사 | 컴파일 성공, spec 검증 성공, WBP 바인딩/컴파일/저장은 별도 확인 |
| V10 | 패키징 후 MainMenu → Gameplay 이동과 동일 Gameplay의 반복 Encounter | 모든 맵 포함, 올바른 GameMode/Controller/HUD 사용 |

DryRun은 코드상 JSON 파싱·구조 검증 후 반환한다. native class 조회, WBP 컴파일, 바인딩, 패키지 저장의 검증까지 수행한 것으로 기록하지 않는다.
