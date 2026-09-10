# ProjectA 작업 보드와 재개 메모

최근 정리: 2026-09-10 · `main` · T06 범위 계산 통합과 전체 자동화 18건 검증 완료.

[기획·구현 현황](PROJECT_PLAN.md) · [코드 리뷰와 검증 시나리오](CODE_REVIEW.md)

## 다음에 켜면 여기부터

**Vertical Slice와 T03·T04·T05·T06 검증을 완료했다. 다음 작업은 T07 발사체 완료 책임과 자원 소진 정책 설계다.** 기존 slice 실행 결과는 [Vertical Slice 보고](VERTICAL_SLICE_REPORT.md), 후속 검증은 각 작업 카드, 에디터 연결은 [설정 안내](VERTICAL_SLICE_SETUP.md)를 기준으로 한다. 다음 콘텐츠 작업은 네 직업의 공통 전투 fallback 교체다.

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

### T07 · 스킬 액터·자원 소진 시 완료 책임 확정 — 설계 대기

- [ ] 완료
- 선행 결정: D04·D05. 위치: [SkillActorBase.cpp](../Source/ProjectA/Combat/SkillActor/SkillActorBase.cpp), [GA_AttackBase.cpp](../Source/ProjectA/GAS/Ability/GA_AttackBase.cpp), [PlayerUnit.cpp](../Source/ProjectA/Unit/PlayerUnit.cpp).
- 작업: 몽타주 종료/impact/실패 중 무엇이 행동 완료인지 정한다. 발사체 미충돌·시전자 사망 등에도 종료 경로를 보장하고 플레이어 AP 소진 처리와 연결한다.
- 완료 조건: 느린 발사체·미충돌·중단에서 정해진 시점까지 턴을 유지하고 완료는 한 번 발생. AP 0/보조 AP 잔여 조합이 확정 규칙대로 처리됨.

## M1. 생성한 파티를 실제 게임에 연결

### T08 · 파티 데이터 유지와 Gameplay 전투 스폰 연결 — 코드/자동화/PIE 검증 완료

- [x] 4슬롯 runtime data, GameInstanceSubsystem 수명, 이름/ClassId/생성 여부/HP
- [x] 생성된 캐릭터 1명 이상 검증 → Run 초기화 → Gameplay OpenLevel
- [x] 빈 슬롯 스폰 제외, 슬롯 순서/좌표 유지, 단일 DataAsset 직업 매핑
- [x] 기존 BP_PlayerUnit 명시적 임시 fallback
- VERTICAL-01/02/08의 실제 검증은 최종 보고 참조. 디스크 저장은 T11이며 이번 작업에 포함하지 않는다.

### T09 · 직업 정의와 캐릭터 상세 UI — 부분 구현

- [ ] 완료
- 작업: 현재 네 ClassId의 표시명·아이콘·전투 클래스·기본 스탯·시작 스킬을 같은 정의에 연결한다. Warrior/Archer/Mage 임시 표시를 정리하고 Edit/ClassInfo 버튼에 실제 기능을 연결한다.
- 완료 조건: 네 직업의 UI 수치와 스폰 스탯이 일치. 각 슬롯 편집 결과가 T08 데이터에 반영되고 설명을 확인할 수 있음.

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

## M3. 반복 플레이와 콘텐츠

### T11 · 저장/이어하기·옵션·게임 종료 — 미구현

- [ ] 완료
- 작업: 저장 범위를 D07에서 정하고 저장/로드 경로를 연결한다. 저장이 없을 때 Continue 상태와 오류 처리를 정한다. Options와 Quit 로그를 실제 기능으로 바꾼다.
- 완료 조건: 재실행 후 파티/진행 복원, 저장이 없을 때 안내, 옵션 변경 반영, 패키징 실행 파일에서 종료 확인.

### T12 · UI 에셋·프리뷰·생성 도구 검증 — 검증 대기

- [ ] 완료
- 작업: `MainMenu.umap`의 PreviewStage 배치, 네 ClassId의 프리뷰 클래스, 메뉴 WBP 3종의 스택/슬롯/버튼 바인딩 확인. Back/X/재진입 시 프리뷰 정리·유지 정책을 적용한다. JSON과 Designer 중 구조 기준을 D08에서 정한다.
- 완료 조건: V07·V08·V09 통과. C++의 BindWidget TODO는 실제 에셋과 대조한 후 완료 여부를 판정한다. JSON에 이름이 있다고 바인딩 검증 완료로 보지 않는다.

### T13 · 아이템·적 이동·추가 스킬 콘텐츠 — 후속 작업

- [ ] 완료
- 작업: 아이템 효과/소모 데이터, 적 이동 후보 점수와 완료 전이, 기본 공격의 임시 `+100000` 점수를 대체할 정책을 구현한다. SkillPool의 획득/장착 연결도 정한다.
- 완료 조건: 아이템이 실제 효과와 소모를 일으키고 적이 유효한 이동/스킬/대기를 선택. 스킬 획득 결과가 실제 장착과 실행에 반영됨.

### T14 · 네트워크 범위 결정과 후속 구현 — 결정 대기

- [ ] 완료
- 선행 결정: D01. 싱글플레이를 선택하면 이 카드를 `보류`로 표시한다.
- 네트워크를 선택한 경우 작업: 전투 상태 소유권, 요청 RPC, HP/AP/사망/점유/턴 정보 복제, HUD 갱신을 설계한다.
- 완료 조건: 지원할 서버 방식과 클라이언트 수를 정하고 양측 동일 상태·권한 검사를 실제 다중 PIE에서 확인. `bReplicates=true`만으로 완료 처리하지 않는다.

## 검증을 실행할 때 확인할 설정

- 현재 기본/에디터 시작 맵은 [DefaultEngine.ini](../Config/DefaultEngine.ini)의 `MainMenu`다. 메뉴 작업은 실제 존재하는 `Content/User_JeHoon/LEVEL/MainMenu.umap`을 열어 시작한다. 기존 UI 설명의 `L_MainMenu`와 파일 이름이 다르므로 혼동하지 않는다.
- 전투 맵: Grid의 TileClass, GameMode의 CombatManagerClass·유닛 클래스·좌표, PartyPlayerController와 HUD 연결, 이동 가능한 NavMesh를 확인한다.
- 메뉴 맵: 메뉴 GameMode/Controller, WBP 3종, PreviewStage와 ClassId 맵을 확인한다.
- Gameplay: BP_GameplayGameMode, GameplayCombatArena의 Grid/CameraAnchor, PartyDefinition과 DefaultEncounter를 확인한다. WorldMap은 deprecated이며 gameplay 경로에 사용하지 않는다.
- UI spec DryRun은 구조 검사다. WBP 실제 생성은 기존 Designer 수정에 영향을 주므로 기존 [UI 사용 설명](../Source/ProjectA/UI/UI_README.txt)의 백업·Overwrite 절차를 따른다.

## 작업 기록

| 날짜 | 작업 | 완료/검증 | 다음 시작점 |
|---|---|---|---|
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
