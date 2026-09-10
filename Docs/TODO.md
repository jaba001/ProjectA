# ProjectA 작업 보드와 재개 메모

최근 정리: 2026-09-10 · `main` · T14 순차 1~4번 완료 / 다음은 5번 아군 AI 판단·행동 / 6~8번 순차 대기.

[게임 기획 방향](GAME_DESIGN.md) · [기획·구현 현황](PROJECT_PLAN.md) · [코드 리뷰와 검증 시나리오](CODE_REVIEW.md)

## 다음에 켜면 여기부터

[T14 순차 작업 대기열](T14_QUEUE.md): 사용자 요청에 따라 1~8번을 한 번에 한 개씩 진행한다. 1~4번의 소유권·서버 명령·2인 동기화·확정 턴 저장과 기존 Host 복구 검증을 완료했다. 번호별 검증·커밋·push를 마친 뒤 다음 번호로 자동 진행한다. 다음은 5번 아군 AI 판단·행동이며 6~8번은 대기한다. 기획 결정이 필요하면 선택지별 장단점을 사용자에게 확인한다. 전투 밖 노드 선택·Continue의 협동 결정권은 답변 대기 중이다.

파티 전체 성장·UI 중심 Run·직접 조작 전투·Async PvP·아군 Co-op과 Decision Density를 [게임 기획 방향](GAME_DESIGN.md)의 기준으로 삼는다. 현재 구현과 목표를 구분하며, 보상 선택을 다음 전투에 유지하는 작은 Run은 후속 검증 제안이다. 패배 후 진행·보상·협동 재화 배분 등은 미결정 상태로 둔다.

**T14 로컬 상대 Snapshot/Unreal USaveGame v1 전투 사이클을 검증했다. 후속 Co-op은 최대 4인·본인 캐릭터 소유권 고정·기존 Host 유지와 명시적 승계/AI 이어하기로 기획을 확정했다. AI 전환은 시작 시 사전 동의, 이후 MMR은 인간 참가자만 반영, 복구는 확정 턴 경계를 기준으로 한다. 순차 1~2번의 소유권 데이터·저장·조회와 서버 명령 검증을 완료했고, 3번의 실제 2인 Listen Server 동기화까지 검증했다. 승계·AI·턴 복구는 후속 단계로 나눈다. 기본 Run은 싱글플레이를 유지하고 Unreal 기본 기능을 우선한다.** 기존 slice 실행 결과는 [Vertical Slice 보고](VERTICAL_SLICE_REPORT.md), 후속 검증은 각 작업 카드와 [Listen Server 안내](T14_NETWORK.md), 에디터 연결은 [설정 안내](VERTICAL_SLICE_SETUP.md)를 기준으로 한다.

체크박스는 작업 완료를 뜻한다. 코드를 작성했어도 완료 조건을 검증하지 못했다면 체크하지 않고 `검증 대기`로 기록한다. P1/P2는 리뷰 결함의 심각도이고 M0~M3는 개발 순서이므로 서로 구분한다.

## 먼저 보존할 현재 작업

2026-09-07 문서화 시작 시 이미 존재한 변경이다. 이번 문서화가 새로 구현한 기능으로 기록하지 않는다.

- 수정 중: MainMenuPlayerController, CharacterCreationWidget, MainMenuRootWidget, UI_README, GenerateUiScaffoldCommandlet, UI JSON spec 3개, 루트 README.
- 변경된 에셋: `Content/User_JeHoon/Blueprint/Controller/BP_MainMenuPlayerController.uasset`, `Content/User_JeHoon/UI/MainMenu/WBP_CharacterCreationWidget.uasset`.
- 미추적 작업: WorldMapGameModeBase `.h/.cpp`, MainMenuPreviewStage `.h/.cpp`, `Content/User_JeHoon/LEVEL/WorldMap.umap`.
- 기존 HEAD: `2b0d9ea` (`UI 자동화_2`). 변경 파일이 있다는 사실은 빌드·플레이 성공을 뜻하지 않는다.

## 이미 작성된 기반 코드

아래 체크는 **코드 존재 확인** 기록이다. 기능별 플레이 검증은 별도이며 세부 한계는 기획 현황 표에 있다.

- [x] Runtime/Editor 모듈과 UI 생성 도구 분리
- [x] 그리드 생성, 타일 점유, 이동 범위와 하이라이트
- [x] 전투 유닛 스폰/등록, 턴 순환과 사망 유닛 건너뛰기
- [x] AP/보조 AP, 이동·근접 접근·복귀 기본 흐름
- [x] GAS 기본/범위 공격, HP·피해·사망 처리
- [x] 스킬 정의·검색 DB·가중치 랜덤 풀
- [x] 적 스킬/대기 평가와 거리·HP 점수 계산
- [x] StartSkill 권한·활성 턴·busy·생존·AP 검사와 활성화 거절 시 일부 정리
- [x] AI 이동 완료 결과의 성공/실패 분기
- [x] CommonUI 스택, 메뉴/캐릭터 생성 native fallback
- [x] 4슬롯 생성/제거·직업 순환, 월드 프리뷰 기본 코드
- [x] JSON 기반 WBP Designer tree 생성과 DryRun/Overwrite
- [x] Gameplay persistent level 입구와 Run Map / Encounter / Result 코드 연결

## M0. 전투 안정화

### T01 · P1 · 제자리 스킬 완료 흐름 — 수정 및 PIE/자동화 검증 완료

- [x] 공통 행동 시작/busy/결과/완료 1회 구현
- GAS 종료 delegate를 활성화 전에 연결하여 동기 완료와 활성화 실패까지 수렴한다. 제자리형은 복귀하지 않는다.
- `ProjectA.Combat.Actions` 회귀 테스트와 VERTICAL-04의 범위/실행 결과는 최종 보고 참조. 몽타주 육안 검증과 주입 테스트를 구분한다.

### T02 · P1 · 실패·취소 후 적 AI 복구 — 수정 및 phase 주입 자동화 검증 완료

- [x] 성공/실패/취소 결과를 적 AI에 전달, 다음 틱 재판단 또는 안전한 턴 종료
- 접근/복귀 실패와 취소는 원타일/행동 시작 위치로 복구한다. 이미 소비된 AP·보조 AP는 환불하지 않는다.
- 이동 요청 즉시 거절, phase 주입, 실제 NavMesh 이동 테스트를 구분한다. VERTICAL-05 실행 근거는 최종 보고 참조.

### T03 · P2 · AP 비용 기준 통합 — 수정 및 자동화·PIE 검증 완료

- [x] 완료
- 근거: 리뷰 R03. 위치: [SkillDefinitionDataAsset.h](../Source/ProjectA/DataAsset/SkillDefinitionDataAsset.h), [GA_AttackBase.cpp](../Source/ProjectA/GAS/Ability/GA_AttackBase.cpp).
- 작업: 표시·사용 가능 판정·실제 차감이 같은 비용을 사용하게 한다. 잘못된 비용과 활성화 실패 시 자원 처리도 검증한다.
- 완료 조건: V04 중 비용 검증 통과. 비용 1/2, AP 부족, 잘못된 데이터에서 표시와 실행 결과가 일치.
- 구현: `SkillDefinitionDataAsset.ActionPointCost`를 단일 기준으로 사용한다. HUD에 비용을 표시하고 0·음수 비용은 입력·AI 후보·직접 실행·차감에서 거절한다. Ability의 기존 비용은 deprecated 호환 필드이며 더 이상 편집하거나 실행 비용으로 사용하지 않는다.
- 자원 처리: 공격 컨텍스트 검증 및 GAS 커밋 성공 후 AP를 차감한다. 어빌리티 누락·GAS 활성화 차단·컨텍스트 실패는 AP/보조 AP를 보존하고 완료를 1회 통지한다. 정상 실행에서 이미 차감한 AP의 비환불 규칙은 유지한다.
- 검증: 2026-09-10 Development Editor / Win64 빌드 성공. `ProjectA.Combat.Costs` 3건과 기존 행동/Run/저장 맵 PIE를 포함한 전체 11건 성공, 실패 0건. 비용 1/2/3/0/-1과 보유 AP 1/2 조합, 과거 Ability 비용 99 무시, 실패 후 재시도, 적의 사용 가능한 대안 선택, 실제 HUD 비용 표시와 Slate 클릭을 확인했다.
- 실행 근거: [빌드 로그](../Saved/Automation/T03APCostBuild.log), [자동화 결과](../Saved/Automation/T03APCost1/index.json), [PIE 로그](../Saved/Logs/T03APCost1.log). Saved 산출물은 로컬 검증 자료이며 Git 추적 대상이 아니다. V04의 범위 타입 검증은 T06으로 남긴다.

### T04 · P2 · 플레이어/적 타겟 규칙 통합 — 수정 및 자동화·PIE 검증 완료

- [x] 완료
- 근거: 리뷰 R04. 위치: [EnemyUnit.cpp](../Source/ProjectA/Unit/EnemyUnit.cpp), [PartyPlayerController.cpp](../Source/ProjectA/Controller/PartyPlayerController.cpp), [CombatTargetingLibrary.cpp](../Source/ProjectA/Combat/Library/CombatTargetingLibrary.cpp).
- 작업: 시전자·스킬·타일 기반 공통 검증으로 진영·생존·전열 보호를 검사하고 실행 진입에서도 사용한다.
- 완료 조건: V03 중 타겟 검사 통과. 같은 스킬/진영 조건에서 UI와 AI가 같은 대상 목록을 얻음.
- 구현: `IsValidSkillTarget`을 플레이어 선택·적 후보 평가/실행 진입·StartSkill·GAS 활성화 직전에 공통 적용한다. 진영·생존·타일 점유 일치·월드 일치·전열 보호를 검사한다.
- 규칙: 기존 플레이어 규칙대로 전열 보호는 `EnemyUnit`에만 적용한다. 제자리 `AllyUnit`/`AnyUnit`은 자기 자신을 허용하고, 타일 규칙은 영역에 맞는 빈 타일을 허용한다. 접근형은 자신이 아닌 생존 유닛이 필요하다. 범위 피해 대상 집합은 T06 범위다.
- 재검증: 유닛 대상의 진영·생존·전열 보호·선택한 타일의 점유자가 바뀌면 실행을 거절하고 AP를 보존한다. 제자리 타일 스킬은 해당 타일의 현재 점유 상태를 사용한다.
- 검증: Development Editor / Win64 빌드 성공. `ProjectA.Combat.Targeting` 3건과 기존 로직/저장 맵 PIE를 포함한 전체 14건 성공, 실패 0건(22.73초). 양 진영·6개 규칙·6개 타일·전열 보호·무시·접근 여부 576개 조합을 공통 함수와 플레이어 필터에서 확인했다. 적의 전열 보호/무시·자신/빈 타일 선택, 직접 실행 차단 및 실행 대기 중 진영 변경·사망·점유자 교체 후 1회 실패/자원 보존/정상 재시도를 검증했다.
- 검증 경계: 대상 상태 변경은 transient 테스트 월드와 실행 대기 phase 주입으로 재현한다. 첫 전체 실행의 저장 맵 PIE는 Move 버튼의 Slate hit-test에서 실패했으며, 창 위치·크기를 명시한 동일 빌드의 단독 PIE 및 최종 전체 실행은 통과했다. 실제 Slate Move/스킬 클릭·승리·Continue·두 번째 전투·패배 종료를 확인했다. 테스트 7건에 기존 환경/실패 주입/간소화된 월드 경고가 남는다. 사용자 수정 `BPGA_DefaultAttack.uasset`을 그대로 사용해 검증했으며 에셋 내용은 보존하고 이번 커밋에 포함하지 않는다.
- 실행 근거: [빌드 로그](../Saved/Automation/T04TargetingBuild.log), [최종 결과](../Saved/Automation/T04TargetingFinal/index.json), [최종 로그](../Saved/Logs/T04TargetingFinal.log). Saved 산출물은 로컬 검증 자료다.

### T05 · P2 · 플레이어 입력과 AI 턴 종료 분리 — 수정 및 자동화·PIE 검증 완료

- [x] 완료
- 근거: 리뷰 R05. slice의 Player 팀/활성 전투 검사와 최소 AI 종료 분리를 확장해 타일 명령의 처리 주체 및 내부 종료 요청 검증을 명확히 한다.
- 완료 조건: V03 중 적 턴 입력 검사 통과. 사용자가 적 행동을 조작하지 못하고 적은 계속 정상적으로 턴을 종료.
- 구현: 타일 액터는 PlayerController에 입력만 전달한다. 컨트롤러가 권한·입력 잠금·팀·활성 턴·생존·busy 및 클릭 시 비용/타겟/이동 범위를 확인한다. AI는 요청 유닛을 명시한 `RequestEndTurnForUnit(this)`를 사용한다.
- 종료 경계: 내부 요청도 현재 턴 유닛과의 일치·생존·비busy를 검사한다. 인자 없는 기존 Blueprint 종료 함수는 플레이어 컨트롤러로 위임하며 deprecated로 안내한다. `AdvanceTurn`은 private 내부 전환으로 제한하고, 사망 유닛을 건너뛰는 기존 경로는 유지한다. 턴 전환·전투 종료 시 선택과 하이라이트를 정리한다.
- 검증: Development Editor / Win64 빌드 성공. `ProjectA.Combat.Input` 2건에 적의 비busy 구간 입력 차단, UI 잠금과 독립적인 실제 AI 종료, 오래된 요청·busy·사망·전투 종료 요청 거절, 타일 클릭의 AP 재검사 및 실제 피해 경로를 추가했다. 기존 행동·AP·타겟·Run·저장 맵 Slate PIE를 포함한 전체 자동화 16건 통과(9건 경고 포함, 실패 0). 경고에는 축소 테스트 월드의 Grid/NavMesh 미설정, 의도한 거절 경로, 기존 콘텐츠 설정 안내가 포함된다.

- 실행 근거: [빌드 로그](../Saved/Automation/T05InputBuild.log), [전체 결과](../Saved/Automation/T05InputFinal2/index.json), [전체 로그](../Saved/Logs/T05InputFinal2.log). 첫 실행에서 새 테스트 월드의 컨트롤러 미등록을 수정했고, 중복 등록 정리 후 [입력 2건 재검증](../Saved/Automation/T05InputFocusedFinal/index.json)도 통과했다. 기존 사용자 변경 에셋을 유지한 로컬 검증이며 Saved 산출물은 커밋하지 않는다.

### T06 · P2 · 범위 계산 통합과 미지원 타입 검증 — 수정 및 자동화·PIE 검증 완료

- [x] 완료
- 근거: 리뷰 R06. 위치: [GA_AreaAttack.cpp](../Source/ProjectA/GAS/Ability/GA_AreaAttack.cpp), [AttackSkillActorBase.cpp](../Source/ProjectA/Combat/SkillActor/AttackSkillActorBase.cpp).
- 구현: `CombatTargetingLibrary`로 중심·체비셰프 범위·진영·생존·점유·중복 검사를 통합하고 GA_AreaAttack, AttackSkillActorBase, UnitBase의 대상 수집에 연결했다. Single은 그리드 매니저 없이 대상 타일만 사용한다. AroundSelf는 효과 적용 시점의 시전자 타일을 사용한다.
- D06 적용 규칙: 기존 범위 효과와 동일하게 시전자 자신을 제외한다. 아군/전체 범위 효과에도 동일하게 적용하며 기본 단일 공격의 별도 효과 구현은 변경하지 않는다. 전열 보호는 최초 EnemyUnit 선택에만 적용하고 범위 피해에 추가하지 않는다.
- 미지원: Row/Column/LeftAndTarget/RightAndTarget/DiagonalTarget/AllEnemies 및 잘못된 enum, 음수 반경은 에셋 IsDataValid와 선택/실행 진입에서 거절한다. AP 소비 전 검사하고 수동 스폰 액터의 impact도 일반 반경으로 대체하지 않는다.
- 완료 조건: V04 중 범위 검사 통과. 동일 입력에서 직접/스폰 효과의 대상 집합이 같고 미지원 타입이 조용히 다른 범위로 실행되지 않음.

- 검증: Development Editor / Win64 빌드 성공. 양 진영×6개 대상 규칙×3개 범위×반경 0/1의 72개 조건을 직접/실제 스폰 경로로 실행해 피해·AP·완료 상태 확인. 사망/시전자 제외, 대각선 경계, 중복 impact 방지, 미지원 6종과 잘못된 enum 거절 및 정상 재시도 검증. 기존 Slate 저장 맵 PIE를 포함한 전체 18건 통과(경고 포함 11건, 실패 0). 에디터 검증 추가 후 범위 2건 재검증.
- 실행 근거: [빌드](../Saved/Automation/T06AreaBuild.log), [전체 결과](../Saved/Automation/T06AreaFinal/index.json), [최종 범위 검증](../Saved/Automation/T06AreaValidation/index.json), [전체 로그](../Saved/Logs/T06AreaFinal.log). 기존 사용자 수정 공격 에셋을 유지한 로컬 검증이다. 경고는 빈 범위/의도한 거절 및 축소 월드·기존 콘텐츠 설정 안내를 포함한다. 느린 발사체의 위치 고정/행동 완료 시점 정책은 T07 범위다.

### T07 · 스킬 액터·자원 소진 시 완료 책임 확정 — 수정 및 자동화·PIE 검증 완료

- [x] 완료
- 선행 결정: D04·D05. 위치: [SkillActorBase.cpp](../Source/ProjectA/Combat/SkillActor/SkillActorBase.cpp), [GA_AttackBase.cpp](../Source/ProjectA/GAS/Ability/GA_AttackBase.cpp), [PlayerUnit.cpp](../Source/ProjectA/Unit/PlayerUnit.cpp).
- D04 적용 규칙: 직접 효과는 기존 GAS 종료 기준을 유지하고 스폰 공격은 몽타주와 impact 모두 완료 후 GAS 종료. 미충돌은 기본 10초(Ability에서 조절), impact 전 RequestFinish/파괴는 실패. 취소·사망은 대기 액터 파괴와 바인딩/타이머 정리로 늦은 피해를 차단한다. 완료 통지는 1회이며 이미 소비한 AP는 비환불이다.
- D05 적용 규칙: 플레이어 AP·보조 AP 모두 0이면 행동 완료 다음 틱에 내부 턴 종료 요청. 한쪽 자원이 남으면 턴을 유지한다. 요청 시 행동 번호와 현재 상태를 재검사하고 턴 변경 시 예약을 취소한다. 기존 MustEndTurnAfterCurrentAction 플래그도 두 자원 소진 기준으로 맞춘다.
- 경계: 스폰 클래스는 SkillActorBase 계열이어야 한다. effect 대상/중심은 T06의 impact 시점 규칙을 유지한다. 애니메이션 asset 자체의 미종료 문제와 멀티플레이 RPC는 별도 범위다.
- 완료 조건: 느린 발사체·미충돌·중단에서 정해진 시점까지 턴을 유지하고 완료는 한 번 발생. AP 0/보조 AP 잔여 조합이 확정 규칙대로 처리됨.

- 검증: Development Editor / Win64 빌드 및 전체 자동화 20건 통과(경고 포함 13건, 실패 0). ProjectileBoundaries에서 impact와 몽타주 순서, 조기 종료·파괴·시간 초과·취소·사망, 늦은 impact 차단과 1회 완료·AP 비환불 확인. PlayerResourceExhaustion에서 AP/보조 AP 0/1의 4조합, 비행 중 턴 유지와 마지막 보조 행동 후 자동 종료 확인. 기존 직접/스폰 범위 검사와 저장 맵 Slate PIE도 통과.
- 검증 경계: 몽타주 순서는 미완료 플래그 주입과 실제 완료 콜백 호출로 확인하며 새 애니메이션 에셋의 재생 테스트는 아니다. 미충돌은 격리 월드의 타이머 프레임 진행으로 확인한다. 첫 실행들의 프레임/예약 활성화 누락을 테스트에서 보완했다. 경고는 의도한 늦은 요청·축소 월드와 기존 콘텐츠 설정을 포함한다.
- 실행 근거: [빌드](../Saved/Automation/T07CompletionBuild.log), [최종 결과](../Saved/Automation/T07CompletionFinal3/index.json), [최종 로그](../Saved/Logs/T07CompletionFinal3.log). 기존 사용자 공격 Blueprint 변경을 유지해 검증하고 커밋에서 제외했다.

## M1. 생성한 파티를 실제 게임에 연결

### T08 · 파티 데이터 유지와 Gameplay 전투 스폰 연결 — 코드/자동화/PIE 검증 완료

- [x] 4슬롯 runtime data, GameInstanceSubsystem 수명, 이름/ClassId/생성 여부/HP
- [x] 생성된 캐릭터 1명 이상 검증 → Run 초기화 → Gameplay OpenLevel
- [x] 빈 슬롯 스폰 제외, 슬롯 순서/좌표 유지, 단일 DataAsset 직업 매핑
- [x] 기존 BP_PlayerUnit 명시적 임시 fallback
- VERTICAL-01/02/08의 실제 검증은 최종 보고 참조. 디스크 저장은 T11이며 이번 작업에 포함하지 않는다.

### T09 · 직업 정의와 캐릭터 상세 UI — 구현 및 검증 완료

- [x] 완료
- 구현: PartyDefinition의 Professions가 네 ClassId의 표시명·설명·아이콘·클래스·스탯·시작 스킬을 제공한다. 기존 클래스 기본값 상속 또는 명시적 직업 수치를 선택할 수 있다. UI의 가짜 Warrior/Archer/Mage 수치 제거, 슬롯 Edit의 이름/직업 저장·취소, ClassInfo의 실제 스탯/스킬 표시 구현.
- 스폰: 선택 화면의 직업 목록을 RunState에 유지하고 Encounter가 동일한 정의로 클래스와 스탯·GAS 스킬을 적용한 뒤 이전 HP를 복원한다. 잘못된 직업/수치/스킬/중복 Ability는 시작 또는 준비 단계에서 거절한다.
- 콘텐츠 경계: 기존 공통 전투 클래스와 밸런스를 기본으로 보존한다. 직업별 새 스킬 제작·밸런스 차별화·새 아이콘 원화는 별도 콘텐츠 작업이다. Icon 필드는 공통 정의에 연결했으며 미지정 이미지를 새로 생성하지 않는다.
- 완료 조건: 네 직업의 UI 수치와 스폰 스탯이 일치. 각 슬롯 편집 결과가 T08 데이터에 반영되고 설명을 확인할 수 있음.
- 검증: Development Editor / Win64 빌드와 전체 자동화 21건 통과(경고 포함 13건, 실패 0). ProfessionLoadout에서 네 직업 스탯·GAS 스킬, 명시적 수치 변경과 잘못된 설정 거절 확인. 저장 맵 PIE에서 Edit 저장/취소·빈 이름 거절·ClassInfo 표시·레벨 이동 후 직업/이름/스탯 유지와 기존 Slate 이동/스킬·승패 흐름 검증.
- 검증 경계: 상세 UI 조작은 버튼 delegate와 위젯 API로 실행했으며 키보드 실입력 검증은 별도다. 화면 캡처로 상세 정보와 기존 화면 배치를 확인했다. 중복 데이터 추가 테스트의 배열 자기 참조 오류는 복사 후 추가로 수정했다. 기존 축소 월드·의도적 실패 경로·콘텐츠 경고는 남아 있다.
- 로컬 근거: Saved/Automation/T09ProfessionFinal4/index.json, Saved/Automation/T09ProfessionBuild.log, Saved/Automation/VerticalSliceScreenshots/00-ProfessionDetails.png.

## M2. 전투 한 번을 포함하는 게임 진행 완성

### T10 · Gameplay / Run Map → Encounter → Combat → Result → Run Map — 실제 PIE 루프 검증 완료

- [x] 후속 P1: CommonUI 전투 입력 모드 수정, travel 잔여 입력 잠금/최초 포커스 복원, Slate 버튼 → 타일 클릭 경로 검증(전체 자동화 8건 성공, GameplayInputFix2)
- [x] persistent Gameplay level + CommonUI Run/Combat/Modal stack
- [x] Arena 위치/그리드/파티·적 좌표/카메라 책임 분리
- [x] 두 개 순차 Combat 노드, Victory Continue / Defeat 종료
- [x] 결과 1회, 턴/플레이어 입력 잠금, HP 반영 후 next tick cleanup
- [x] 유닛·AI controller·스킬 액터·TurnOrder·CombatUnits·타일 점유·콜백/타이머 정리
- WorldMap 탐험/전투별 OpenLevel 계획은 폐기. WorldMap asset/class는 serialized reference 보존을 위한 deprecated 상태다.
- 실제 VERTICAL-01~10 통과 여부는 [보고](VERTICAL_SLICE_REPORT.md)에 기록한다. PIE의 기존 공격 반복에 의한 자연 Victory, lethal damage로 유도한 Defeat, 실패 phase 주입, 미실행 마우스 조작 시나리오를 구분한다.
- 2026-09-10 T10 재확인: 현재 소스의 결과 1회 통지·턴/입력 잠금·HP 기록 후 다음 틱 정리·Result/Map 전환을 대조했다. 추가 런타임 수정 없이 기존 구현을 유지한다.
- 현재 코드 검증 근거: T09에서 실행한 Development Editor / Win64 빌드 성공 및 T09ProfessionFinal4 전체 자동화 21건 통과(경고 포함 13건, 실패 0). VerticalSlice의 PartyValidation / Progression / SavedMapsPIELoop 3건 모두 성공. 이후 C++·에셋 변경이 없어 동일 검증을 반복하지 않았다.
- PIE 확인 범위: 자연 승리 → Continue → 동일 Gameplay 월드의 두 번째 전투 → 피해 주입에 의한 패배, 결과 1회·입력 차단·유닛/전투 등록/턴 등록/타일 점유 정리. 두 노드 모두 승리한 최종 Complete 전이는 RunState 단위 테스트로 확인하며 실제 PIE 두 연속 승리와 구분한다.

## M3. 반복 플레이와 콘텐츠

### T11 · 저장/이어하기·옵션·게임 종료 — 구현 및 패키지 검증 완료

- [x] 완료
- D07 적용: 단일 슬롯에 새 게임/전투 결과/Continue 체크포인트 자동 저장. 파티·HP·직업 데이터 경로·노드 진행·결과 복원, 전투 중 종료 시 전투 전부터 재개. 패배/최종 완료 기록은 이어하기 불가. 새 게임 시작은 기존 슬롯 교체.
- 구현: 버전/파티/직업/진행 검증 후 복원, 저장 없음·손상·종료 안내와 Continue 비활성화, 쓰기 실패 안내. 그래픽 품질·VSync 적용/저장 및 닫기, 실제 QuitGame 연결.
- 검증: Development Editor / Win64와 Development 게임 빌드 성공, 최종 전체 자동화 23건 통과(경고 포함 13건, 실패 0). 디스크 저장/손상 버전·진행 거절/전투 전 복원/패배 종료, 실제 PIE 결과 저장과 다음 전투 HP, 옵션 적용 후 설정 재로드 확인.
- 패키지 검증: BuildCookRun 성공. 별도 실행 파일 프로세스에서 기록 생성 → 다음 프로세스의 실제 메뉴 Continue delegate → 결과 화면 → 다음 노드 해금 통과. Quit 버튼 delegate가 엔진 종료를 요청하고 정상 종료 로그·프로세스 소멸 확인. 렌더링 실행에서 Quit, NullRHI 실행에서 저장/Continue 확인. 물리 마우스 클릭 검증과 구분한다.
- 검증 경계: ProcessRestart 테스트는 플래그로 기록/복원을 선택한다(전체 기본 실행에서는 안내만 출력). 별도 프로세스 복원은 T11RestartRead 및 패키지 T11PackWrite/T11PackContinue로 검증. 쓰기 실패 안내 경로는 코드 확인이며 디스크 공간 고갈 주입은 미실행. 기존 콘텐츠·축소 월드 경고와 메뉴 PreviewStage 누락 경고는 T12 범위.
- 근거: Saved/Automation/T11EditorFinalBuild.log, T11GameBuild.log, T11Final/index.json, T11PackageFinal.log, T11PackWrite/index.json, T11PackContinue/index.json 및 Saved/Logs/T11PackQuit.log. 테스트 슬롯만 사용하며 실제 ProjectA_Run 슬롯은 변경하지 않았다.
- 완료 조건: 재실행 후 파티/진행 복원, 저장이 없을 때 안내, 옵션 변경 반영, 패키징 실행 파일에서 종료 확인.

### T12 · UI 에셋·프리뷰·생성 도구 검증 — 구현 및 검증 완료

- [x] 완료
- D08: 기존 Designer WBP를 구조 기준으로 유지. JSON 3종은 DryRun/별도 생성본 검증용. AddMissing으로 기존 구조를 보존하며 누락된 Text_StartGameStatus만 원본에 추가.
- 구현: MainMenu PreviewStage 배치와 네 직업의 표시 전용 프리뷰, 카메라 자동 Pawn 전환 차단, 메뉴 GameMode의 추가 전투 Pawn 제거. Back/X·deactivation·스테이지 EndPlay에서 프리뷰 정리, 재진입 시 빈 초안. 기존 메시 재사용이며 직업별 신규 아트는 별도.
- 검증: Development Editor / Win64 빌드 및 전체 자동화 24건 통과(경고 포함 13건, 실패 0). JSON 3종 DryRun·별도 생성/compile/save 성공, 원본/생성본 바인딩·타입 대조. AddMissing 기존 제목 보존과 신규 위젯 저장, 2회 실행 시 1개/0개 추가 확인.
- 최종 화면 보정: 추가 Pawn 제거 후 관련 3건 통과, 프리뷰 회전 보정과 슬롯 패널 최소 높이 적용 후 SavedMapsPIELoop 재통과. 네 ClassInfo 버튼이 화면 안에 들어오는지 검사하고 캡처로 프리뷰 4개·카메라·버튼 표시 확인.
- 검증 경계: Back/X·반복 활성화·생성/삭제는 delegate 및 위젯 API로 확인. 실제 이동/스킬은 기존 Slate 클릭 검증 유지. 직업별 원화/새 모델과 T12 변경의 패키지 재검증은 별도. 선택적 이전 팝업 바인딩은 완료 처리하지 않음.
- 근거: Saved/Automation/T12FinalBuild.log, T12Final/index.json, T12PreviewFinal/index.json, T12LayoutFinal/index.json, VerticalSliceScreenshots/00-FourPreviews.png와 Saved/Logs/T12_*_DryRun.log, T12_*_Generate.log, T12AddMissingValidation1/2.log.
- 완료 조건: V07·V08·V09 통과. C++의 BindWidget TODO는 실제 에셋과 대조한 후 완료 여부를 판정한다. JSON에 이름이 있다고 바인딩 검증 완료로 보지 않는다.

### T13 · 아이템·적 이동·추가 스킬 콘텐츠 — 완료

- [x] 완료
- 작업: 아이템 효과/소모 데이터, 적 이동 후보 점수와 완료 전이, 기본 공격의 임시 `+100000` 점수를 대체할 정책을 구현한다. SkillPool의 획득/장착 연결도 정한다.
- 완료 조건: 아이템이 실제 효과와 소모를 일으키고 적이 유효한 이동/스킬/대기를 선택. 스킬 획득 결과가 실제 장착과 실행에 반영됨.
- 구현: 전투 한정 회복약(기본 HP 40·1개·SubAP 1)과 HUD 자기 회복, 가중 SkillPool 추첨→중복 제외→최대 4개 추가 슬롯/GAS 장착. 현재 풀은 휩쓸기(주변 1칸·피해 10·AP 1)를 지급한다. 다음 전투에서 재지급하며 HP만 기존 체크포인트에서 복원한다.
- AI: 대상 수/AP·거리·HP 점수로 임시 기본 공격 가중치 제거. 공통 이동 후보 중 전진을 평가하고 목적지 재검증, 성공 후 재판단/실패 후 턴 종료.
- 검증: Development Editor / Win64 빌드, 전체 자동화 26건(성공 13·경고 동반 성공 13·실패 0). 아이템 효과/소모/거절, 실제 획득 스킬 피해, 이동 선택/막힘/실패 전이 및 저장 맵 PIE의 HUD 회복·두 전투 재지급 검증. `Saved/Automation/T13Final/index.json`, `Saved/Automation/T13Build.log`. 패키지 재빌드는 미실행.

### T14 · 하이브리드 멀티플레이 기반 설계 — 로컬 Snapshot·2인 동기화·턴 복구 완료 / AI·승계 후속

게임 전체의 목표 Run Loop와 우선순위는 [게임 기획 방향](GAME_DESIGN.md)을 따른다. 이 카드는 네트워크 확장 범위와 완료 조건을 기록한다.

- [ ] 완료

ProjectA는 최종적으로 멀티플레이를 지원한다.

첫 Vertical Slice와 기본 Run은 싱글플레이 구조를 유지하되, 이후 다음 두 멀티플레이 방식을 지원할 수 있도록 아키텍처를 확장한다. D01은 이 하이브리드 방향으로 갱신하며 이전 T14 보류 기록을 대체한다.

#### Async PvP

Backpack Battles / The Bazaar 계열의 비동기 PvP를 목표로 한다.

실시간으로 상대 플레이어와 연결하지 않고 서버에 저장된 상대의 파티/빌드 Snapshot을 받아 Encounter를 구성한다.

Opponent Snapshot에는 최소한 파티 구성, Class, Stats, Skills, Equipment, Formation 및 데이터 버전을 저장할 수 있어야 한다.

상대는 AI가 조작하고 아군은 직접 Grid 전투를 조작한다. 향후 플레이어가 지정한 Tactics를 Snapshot에 포함할 수 있는 구조를 고려하며, 전술 편집은 후속 기획으로 둔다.

Combat 시스템은 PvE Encounter와 Async PvP Encounter를 가능한 한 동일한 Unit/Combat 흐름으로 처리한다.

초기 구현에서는 Snapshot 기반 로컬 전투가 가능하지만, 경쟁 콘텐츠 적용 전 결과 위변조와 데이터 검증을 위한 서버 권위 정책을 별도로 설계한다.

#### Co-op Multiplayer

아군과 함께 플레이하는 실시간 협동 모드를 지원한다.

Unreal Engine의 서버 권위 구조를 유지하며 한 플레이어가 Listen Server 역할을 하는 Host-authoritative 방식을 우선한다.

네트워크 transport는 향후 Steam 또는 EOS 등의 P2P 연결 방식을 검토한다.

최대 협동 접속 인원은 Host를 포함해 4명이며 첫 동기화 검증은 2명부터 진행한다. 각 플레이어는 원래 본인 소유의 캐릭터만 직접 조작한다. 같은 Run에 다른 사람이 대체 참가하지 못하며, Host도 타인 캐릭터의 인간 조작권을 얻지 않는다. 이전 2A 선택지의 균등 배정 표현보다 사용자의 본인 캐릭터 소유권 고정 설명을 우선한다.

Client는 이동, 스킬, 타겟 선택, 턴 종료 등의 Action Request를 서버에 요청하고, 실제 행동 가능 여부와 Combat State 변경은 서버에서 검증하고 실행한다.

#### Co-op 이어하기와 랭크 확정안

정상 종료·갑작스러운 연결 끊김만으로 기존 Host를 바꾸지 않는다. 원래 인원이 다시 모여 이어하는 경로를 유지한다. Host가 꺼져도 서버가 계속 실행된다는 의미는 아니다.

원래 인원이 다시 모일 수 없을 때 기존 참가자 한 명이 **Host 승계 + 불참자 AI로 계속하기** 버튼으로 명시적으로 이어갈 수 있게 한다. 캐릭터 소유자는 그대로 유지하고 불참자의 제어 주체만 서버 AI로 전환한다. 새 Host가 타인 캐릭터를 직접 조종하거나 새로운 사람으로 교체하지 않는다. 단순 끊김만으로 자동 승계·AI 전환하지 않는다.

- AI 전환 동의: 각 참가자가 Run 시작 시 미리 동의한다.
- MMR: 랭크 시스템 도입을 목표로 한다. AI 전환 후 이어간 결과는 현재 인간 참가자에게만 반영하고 불참자에게 추가 변동을 주지 않는다.
- 복구: 마지막으로 확정된 턴 경계에서 이어한다. 현재 전투 밖 체크포인트에 이미 구현된 기능은 아니다.
- 경쟁 검증: 동일 Run의 중복 승계·진행 분기·결과 중복 반영을 막고 원래 소유자를 인증해야 한다. 불리한 상황에서 이탈하거나 턴을 반복해 MMR을 회피하는 정책은 별도 결정한다.

선택 번호가 겹치므로 최초 로컬 Snapshot 선택(1A+2B)과 이번 Co-op 선택을 혼동하지 않는다. 이번의 2A·3C는 사용자가 설명한 소유권 고정과 명시적 이어하기 방식으로 구체화했다. 데이터 경계·Unreal 적용 지점·단계별 완료 조건은 [Co-op 확정 기획](T14_COOP_DESIGN.md)에 정리한다.

#### 네트워크 권위 원칙

CombatManager, TurnManager, Grid Occupancy, Unit State, HP/AP, 사망, Combat Result는 서버가 최종 권위를 가진다.

Client가 직접 Combat State를 확정하지 않는다.

현재 일부 Actor의 `bReplicates` 설정만으로 네트워크 지원 완료로 취급하지 않는다.

#### 현재 작업 범위

기본 Vertical Slice와 Run은 싱글플레이를 유지한다. 현재는 순차 3번의 2인 Listen Server 전투/HUD 동기화에 범위를 한정해 구현·검증하며 전체 게임의 온라인 기능을 완료한 것으로 취급하지 않는다.

다만 앞으로 추가하는 Run/Party/Encounter/Combat 데이터는 Actor reference에 과도하게 의존하지 않고 직렬화 가능한 Runtime Data와 Command 형태를 우선한다.

새 기능 구현 시 향후 Async PvP Snapshot 및 Listen Server Replication을 방해하는 강한 로컬 PlayerController 의존성을 만들지 않는다.

작업 중 애매하거나 결정이 필요한 항목은 구체적인 선택지와 영향을 사용자에게 피드백한다. 미결정 정책을 임의로 확정하지 않으며, 결정에 의존하지 않는 작업은 계속 진행한다.

#### 본격 네트워크 구현 전 결정할 항목

로컬 1차 범위는 **1A Snapshot 전투 우선**, 저장은 **2B Unreal USaveGame v1**로 결정했다. 미지원 저장 구조 버전과 다른 카탈로그 콘텐츠 버전은 거절한다. Unreal 기본 기능을 우선하며 로컬 전투 사이클을 검증한 뒤 Co-op 동기화로 넘어간다.

- 1인당 본인 소유 캐릭터 수와 접속 인원보다 배정 가능한 캐릭터가 적을 때의 시작 규칙
- 전투 밖 노드 선택·Continue의 협동 결정권. 사용자 답변 전 네트워크 화면은 읽기 전용이며 자동화는 신뢰된 서버 진입점 사용
- AI 전환 뒤 원래 소유자의 복귀/수동 조작 전환 시점과 비전투 장비·스킬·Ready 처리
- 현재 인간 참가자를 MMR 정산 대상으로 확정하는 시점과 MMR 계산·정산 단위
- 고의 이탈·미확정 턴 반복·승계 후 중복 진행 방지의 구체적인 랭크 정책
- Steam / EOS 선택
- Lobby / Invite 방식
- Async PvP 온라인 Snapshot 전송 포맷과 Backend 저장 방식
- Backend 및 매칭 방식
- 전투 결과 검증 정책
- 후속 버전 Snapshot의 마이그레이션 정책(현재 v1 외 거절)

#### 완료 조건

Async PvP에서는 저장된 상대 Party Snapshot 하나로 실제 Encounter를 생성하고 기존 Combat 시스템을 통해 전투할 수 있어야 한다.

Co-op에서는 최소 2개의 PIE 인스턴스에서 Listen Server / Client로 접속하여 각 플레이어가 자신의 Unit만 조작하고 동일한 Turn, Grid, HP/AP, 사망 및 Combat Result 상태를 확인할 수 있어야 한다.

최대 4인 지원 완료 전에는 3·4인도 검증한다. 후속 승계/AI 이어하기는 원래 참가자만 재개 가능, Host가 바뀌어도 타인 인간 조작 거절, 사전 동의한 AI 제어, 마지막 확정 턴 복구, 하나의 유효 Host와 결과 1회 반영을 확인한다. 로컬 테스트용 신원·저장소 검증을 온라인 인증·랭크 보안 완료로 기록하지 않는다.

#### 현재 구현·검증 상태

- [x] 로컬 Snapshot 저장·불러오기 → 실제 Encounter → 기존 Combat 전투 한 사이클
- [x] Co-op 최대 인원·캐릭터 소유권·명시적 Host 승계/AI·MMR 반영 대상·턴 복구 기획 확정
- [x] 순차 1~2번 Run·참가자·캐릭터 소유권 값 데이터/저장과 서버 Action Request 검증
- [x] 순차 3번 Listen Server / Client 2인 PIE, 플레이어별 조작권과 전투/HUD 상태 동기화
- [ ] 턴 경계 체크포인트와 기존 Host 복구, 원래 소유권을 유지하는 승계 복구 데이터
- [ ] Host 승계 + 불참자 AI로 계속하기 버튼, 아군 AI 판단과 중복 재개 방지

`FPartySnapshot` 값 데이터를 `UPartySnapshotSaveGame`에 저장하고 `UPartySnapshotLibrary`로 검증·복원한다. 신뢰된 `UOpponentSnapshotCatalogDataAsset`이 ClassId/SkillIds를 해석하고 EncounterManager가 저장된 스탯·순서 있는 스킬·Formation으로 적을 생성한다. 장비·전술 식별자는 저장 가능하지만 실행은 빈 값만 지원하며 HP 0 상대도 현재 실행에서는 거절한다. 스냅샷 적은 회복약이나 무작위 추가 스킬을 받지 않는다.

`ConfigureSnapshotContent.py`와 샘플 Blueprint/카탈로그를 추가했다. `-ProjectAOpponentSnapshot=SampleOpponent`로 선택하며 기본 PvE와 상대별 Run 체크포인트를 분리한다. `.sav`는 로컬 생성 데이터이고 저장된 상대는 두 노드에서 매번 불러온다. 사용 방법·데이터 계약·경계는 [T14 Snapshot 안내](T14_SNAPSHOT.md)에 정리했다.

검증: Development Editor / Win64 빌드, 전체 자동화 32건(성공 18·경고 동반 성공 14·실패 0), 별도 Snapshot 모드 저장 맵 PIE 1건(경고 동반 성공). 스키마/수치/ID/스킬/배치 거절과 저장 왕복, Run 저장 분리, 준비 실패 시 Map 복귀·상세 오류·점유/유닛/턴 정리, 기존 PvE 회귀를 확인했다. Snapshot 실제 적과 저장값 일치, Slate 이동/스킬/타일 입력, 적 AI 피해, 자연 승리→Continue→다음 노드→GAS 패배, 결과 1회와 정리를 검증하고 전투 화면을 확인했다. `Saved/Automation/T14Build.log`, `Saved/Automation/T14Full/index.json`, `Saved/Automation/T14SnapshotPIE/index.json`.

이전 `ee61c84` 변경은 확정 기획과 구현 경계를 문서화한 작업이며 C++·기존 SaveGame v1·에셋 변경은 없었다. 위 32건/별도 PIE 결과는 로컬 Snapshot 구현의 기록이다. 순차 1번 소유권 데이터·v2 저장·기존 v1 호환과 2번 서버 명령·소유권·중복 검증 결과는 [대기열](T14_QUEUE.md)에 별도 기록한다.

순차 3번은 서로 다른 NetDriver를 가진 두 PIE 월드 사이의 실제 PlayerController RPC와 서버 소유권 검증을 연결했다. 서버 전용 TurnManager, CombatManager의 전투/유닛 식별 뷰, GAS HP/MaxHP RepNotify, Unit AP/SubAP·이동·장착·턴/행동/사망 상태, Grid 점유·전열 보호·결과/HUD 복제를 구현했다. GameState는 Run 단계·파티·노드·결과의 읽기 전용 표시 정보를 전달하고 클라이언트 RunState를 권위 상태로 사용하지 않는다. 상세 구조와 실행 방법은 [Listen Server 안내](T14_NETWORK.md)를 따른다.

`AssignRunParticipant`와 `ApplyCombatParticipantBindings`는 신뢰된 서버 C++ 연결 배정이며 실제 로그인 인증이 아니다. 네트워크 노드 선택·Continue 결정권은 사용자 답변 대기 중이므로 해당 UI는 읽기 전용이고 자동화는 서버 진입점을 사용한다. 네트워크 최종 유닛은 결과 화면에서 유지한 뒤 명시적인 Continue 또는 월드 종료에서 정리한다. 3번은 Development Editor / Win64 빌드·전체 자동화 42건(성공 26·경고 동반 성공 16·실패 0)·별도 Snapshot 상대 PIE 1건을 통과했다. 상세 결과와 초기 NavMesh 테스트 순서 오류의 수정 기록은 [대기열](T14_QUEUE.md)에 남긴다.

4번은 v3 확정 턴 저장·실패 재시도·기존 Host의 새 세션 복구를 구현했다. 원래 소유권·턴·HP/AP/SubAP·재고·사망·점유·실제 장착과 고정 상대를 보존하고 새 명령 실행 ID를 부여한다. Editor 빌드·전체 52건(성공 35·경고 동반 성공 17·실패 0), 독립 프로세스 Writer/Reader·원본 상대 교체/삭제 복구·별도 Snapshot 전투 PIE를 통과했다. 지원하지 않는 GAS 상태, 손상과 배치 불일치는 거절한다. [확정 턴 저장·복구](T14_CHECKPOINT.md)에 저장 경계와 현재 제한을 정리한다. 5~8번의 아군 AI·명시적 승계·3~4인·온라인 서비스는 대기하고 T14 전체 완료는 체크하지 않는다.

## 검증을 실행할 때 확인할 설정

- 현재 기본/에디터 시작 맵은 [DefaultEngine.ini](../Config/DefaultEngine.ini)의 `MainMenu`다. 메뉴 작업은 실제 존재하는 `Content/User_JeHoon/LEVEL/MainMenu.umap`을 열어 시작한다. 기존 UI 설명의 `L_MainMenu`와 파일 이름이 다르므로 혼동하지 않는다.
- 전투 맵: Grid의 TileClass, GameMode의 CombatManagerClass·유닛 클래스·좌표, PartyPlayerController와 HUD 연결, 이동 가능한 NavMesh를 확인한다.
- 메뉴 맵: 메뉴 GameMode/Controller, WBP 3종, PreviewStage와 ClassId 맵을 확인한다.
- Gameplay: BP_GameplayGameMode, GameplayCombatArena의 Grid/CameraAnchor, PartyDefinition과 DefaultEncounter를 확인한다. WorldMap은 deprecated이며 gameplay 경로에 사용하지 않는다.
- UI spec DryRun은 구조 검사다. WBP 실제 생성은 기존 Designer 수정에 영향을 주므로 기존 [UI 사용 설명](../Source/ProjectA/UI/UI_README.txt)의 백업·Overwrite 절차를 따른다.

## 작업 기록

| 날짜 | 작업 | 완료/검증 | 다음 시작점 |
|---|---|---|---|
| 2026-09-10 | T14 순차 4번 확정 턴 저장·기존 Host 복구 | Editor 빌드·전체 52건·독립 프로세스 Writer/Reader·상대 교체/삭제 복구·Snapshot PIE 통과. 저장 실패/손상/사망/GAS 거절과 실제 입력 회귀 확인 | 순차 5번 아군 AI 판단·행동, 6~8번 대기. 노드/Continue 결정권 답변 대기 |
| 2026-09-10 | T14 순차 3번 2인 Listen Server 전투/HUD 동기화 | 실제 소유 연결 RPC·본인 조작권·턴·HP/AP·Grid·사망·결과 검증. Editor 빌드·전체 42건·별도 Snapshot PIE 1건 통과. PIE 시작 전 편집기 Nav 생성 대기 보정 | 순차 4번 확정 턴 체크포인트·기존 Host 복구, 5~8번 대기. 노드/Continue 결정권 답변 대기 |
| 2026-09-10 | T14 순차 2번 서버 Action Request와 소유권 검증 | 이동·스킬·회복약·턴 종료 통합, 서버 참가자 바인딩·소유권·턴·순번·자원·대상 검증. Editor 빌드·전체 41건·별도 Snapshot PIE 1건 통과 | 순차 3번 2인 Listen Server 전투/HUD 동기화, 4~8번 대기 |
| 2026-09-10 | T14 순차 1번 Run·참가자·캐릭터 소유권 | v2 식별/동의/Host 저장·조회, 실제 메타데이터 없는 v1 호환, Editor 빌드·전체 36건·Snapshot PIE·독립 프로세스 저장/복원 통과 | 순차 2번 서버 Action Request와 소유권 검증, 3~8번 대기 |
| 2026-09-10 | T14 Co-op 소유권·승계·AI 이어하기 기획 | 최대 4인·본인 캐릭터 고정, 명시적 Host 승계/AI, 사전 동의·인간 참가자만 MMR·확정 턴 복구 결정. 현재 코드 대조·문서 정합성 검증, C++/빌드/PIE 변경 없음 | 소유권/Run 참가자 데이터와 서버 요청 검증 → 2인 동기화 → 턴 복구·승계·아군 AI |
| 2026-09-10 | T14 로컬 Snapshot 전투 | 1A+2B 확정, USaveGame v1·카탈로그·기존 Combat 연결·Run 저장 분리. Editor 빌드·전체 32건·별도 Snapshot PIE 1건 통과 | Co-op 인원·조작권·Host disconnect 정책 결정 후 Listen Server 동기화 |
| 2026-09-10 | 게임 기획 방향 문서 정리 | GAME_DESIGN에 원문 기획·현재 구현 차이·미결정 정책·검증 제안 구분, README/PROJECT_PLAN/TODO 연결, 원문 대조·문서 4개 로컬 링크 59개·코드 블록 닫힘 확인. 문서 변경으로 빌드·PIE 미실행 | 싱글플레이 Run 성장 검증의 범위와 필요한 정책 결정 |
| 2026-09-10 | T14 하이브리드 멀티플레이 기획 확정 | 사용자 확정안으로 이전 보류 방침 대체, 문서·작업 원칙 정합성 및 링크 검사 | 싱글플레이 콘텐츠 개발 지속, 네트워크 구현 전 미결정 항목 협의 |
| 2026-09-10 | T14 네트워크 범위 정리 | 기존 싱글플레이 범위 적용으로 보류, 문서·복제 선언 대조 | 범위 확장 시 서버/인원/조작권 결정 |
| 2026-09-10 | T13 전투 콘텐츠 | 빌드·전체 26건 및 저장 맵 PIE 통과 | T14 네트워크 범위 결정 |
| 2026-09-10 | T12 메뉴 에셋·프리뷰·생성 도구 | 빌드·전체 24건 및 최종 PIE 통과, JSON 생성/누락 보완·화면 검수 | T13 콘텐츠 |
| 2026-09-10 | T11 체크포인트 저장·이어하기·옵션·종료 | Editor/게임 빌드·전체 자동화 23건·패키지 저장/Continue/Quit 검증 | T12 UI 에셋·프리뷰 |
| 2026-09-10 | T10 기존 전투 진행 루프 재확인 | 현재 코드와 직전 빌드·자동화 21건 결과 대조, 추가 런타임 변경 없음 | T11 메뉴 저장/이어하기 |
| 2026-09-10 | T09 공통 직업 정의와 상세 편집 | 빌드·전체 자동화 21건 통과, 네 직업 및 저장 맵 PIE 검증·화면 확인 | T11 메뉴 저장/이어하기 |
| 2026-09-10 | T07 발사체 완료와 자원 소진 종료 | 빌드·전체 자동화 20건 통과, impact/중단/미충돌/사망 1회 완료 및 AP 조합 검증 | T09 직업별 전투 데이터 |
| 2026-09-10 | T06 범위 계산 통합과 미지원 타입 검증 | 정식 빌드 및 전체 자동화 18건 성공, 72조건의 직접/스폰 실제 피해 일치와 미지원 거절·복구 검증 | T07 완료 책임·자원 소진 정책 설계 |
| 2026-09-10 | T05 플레이어 입력과 AI 턴 종료 분리 | 정식 빌드 및 전체 자동화 16건 성공, 적 턴 입력 차단/AI 독립 종료/타일 명령/Slate PIE 검증 | T06 범위 계산 통합과 미지원 타입 검증 |
| 2026-09-10 | T04 공통 타겟 규칙과 실행 직전 재검증 | 정식 빌드 및 전체 자동화 14건 성공, 576개 타겟 조합/실제 AI 선택/실행 거절·재시도/Slate PIE 검증 | T05 입력과 AI 턴 종료 분리 마무리 |
| 2026-09-08 | P1 공통 행동 완료/AI 복구, Run State/Gameplay/Encounter/UI/결과/cleanup 연결 | 정식 빌드/에셋 29항목/자동화 8건/자연 Victory PIE 통과, HUD·결과·타일 캡처 검수, 세부 경계는 VERTICAL_SLICE_REPORT.md | 직업별 fallback 교체 |
| 2026-09-07 | 현재 코드·설정·TODO 검토, 기획/작업 보드/리뷰 문서 작성 | 정적 리뷰: P1 2건, P2 4건. 런타임 수정 없음. 빌드·PIE 미실행 | T01의 제자리 스킬 완료 규약 |

다음 작업을 끝낼 때 아래 내용을 복사해 채운다. 실행하지 않은 검증은 명시적으로 남긴다.

```text
날짜:
작업 ID / 상태: 미착수 | 진행 중 | 검증 대기 | 완료 | 보류
이번에 한 것:
변경 파일 / 커밋:
검증: 실행한 시나리오 ID, 결과, 로그 위치 / 미실행 항목
결정한 기획: D번호와 결론
남은 문제:
다음에 처음 할 한 가지:
```
