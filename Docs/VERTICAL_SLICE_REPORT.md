# ProjectA Vertical Slice 작업 보고

2026-09-08. 작업 시작 시 존재한 미커밋 메뉴/캐릭터 생성/UI 생성 도구/에셋/문서를 보존했다. 커밋과 push는 수행하지 않았다.

## Gameplay 클릭 입력 후속 수정

사용자 플레이에서 발견한 CommonUI 타일 클릭 차단을 수정했다. CombatHUD의 입력 설정 누락으로 Router가 Menu 모드를 유지했으며, 기존 자동화의 `StartMoveAction`/`StartSkill` 직접 호출은 이 입력 경로를 우회했다.

- CombatHUD `GetDesiredInputConfig`: All / CaptureDuringMouseDown / 커서 유지. RunMap과 Result: Menu / NoCapture.
- GameplayController의 화면별 수동 SetInputMode 제거. MainMenu의 UIOnly travel 잔여 IgnoreInput 해제 및 해당 로컬 플레이어의 최초 뷰포트 포커스 복원.
- 첫 이동과 접근 스킬은 실제 Slate 합성 마우스 hover/down/up으로 HUD 버튼과 월드 타일을 클릭한다. 히트 테스트, CommonUI 이후 게임 뷰포트 클릭 전달 1회, 타일 선택·점유·이동 완료·GAS 피해까지 검증한다.
- 정식 빌드 성공, 보강한 전체 자동화 8건 성공(101.88초, 테스트 errors 0). Map → Combat → Result → 두 번째 Combat → Defeat의 입력 모드와 차단 상태도 검증했다.
- 이번 후속 변경은 C++ 8파일과 Editor Build.cs 및 문서다. Blueprint/맵/JSON은 수정하지 않았다. 변경 C++ 8개를 VS에 다시 열었고 `.vcxproj`/`.filters` 반영을 확인했다.
- [후속 빌드](../Saved/Automation/GameplayInputFixBuild.log), [후속 전체 테스트](../Saved/Logs/GameplayInputFix2.log), [후속 JSON 결과](../Saved/Automation/GameplayInputFix2/index.json), [VS 열기 결과](../Saved/Automation/GameplayInputFixOpenedFiles.json).

[설계]

- 최종 흐름: MainMenu → CharacterCreation → Start Game → Gameplay persistent level → Run Map → Combat Node → Arena/Party/Enemy → 기존 Grid Combat → Victory → cleanup → Result Continue → Run Map. Defeat는 cleanup 후 입력이 잠긴 종료 화면이다.
- `URunStateSubsystem`은 GameInstance 수명의 메모리 상태만 소유한다. 파티 4슬롯, 이름, ClassId, 생성 여부, HP, 현재/완료 노드, 현재 Encounter, Phase, Result를 보존한다. Actor나 UI를 저장하지 않는다.
- `AGameplayGameModeBase`는 레벨의 Arena와 매니저/Controller를 연결한다. `AEncounterManager`는 준비·스폰·전투 시작·HP 추출·정리·진행 복귀를 담당한다.
- `ACombatArena`가 배치 Grid, 슬롯별 PlayerCoords/EnemyCoords, CameraAnchor 및 활성화/정리를 제공한다. 한 개 Arena만 구현했고 Streaming은 추가하지 않았다.
- 카메라는 같은 Gameplay 레벨의 `GameplayCamera`를 ViewTarget으로 설정한다. 전투마다 OpenLevel하지 않는다.
- 기존 CombatManager, TurnManager, Grid, Unit/AI, GAS, PlayerController 입력을 재사용한다. UI가 Unit spawn이나 전투 데이터 소유를 맡지 않는다.

[추가한 파일]

아래 신규 C++ 27개와 에디터 Python 3개, UI JSON 4개, 설정/보고 문서 2개, 에셋 도구 README를 추가했다. 전체 C++ 경로 목록은 문서 끝의 파일 목록을 참조한다.

- GameplayPlayerController `.h/.cpp`
- EncounterDefinitionDataAsset `.h`, PartyDefinitionDataAsset `.h/.cpp`
- CombatArena, EncounterManager, GameplayGameModeBase `.h/.cpp`
- RunTypes `.h`, RunStateSubsystem `.h/.cpp`
- CombatResult, UnitActionTypes `.h`
- GameplayActionButton, GameplayRootWidget, RunMapWidget, EncounterResultWidget `.h/.cpp`
- UnitActionLifecycleTests, RunStateTests, VerticalSliceFlowTests `.cpp`
- AuditGameplayAssets.py, ConfigureGameplayAssets.py, ValidateGameplayAssets.py
- Source/ProjectAEditor/Scripts/README.md
- GameplayRootWidget.json, RunMapWidget.json, CombatHUDWidget.json, EncounterResultWidget.json
- Docs/VERTICAL_SLICE_SETUP.md, Docs/VERTICAL_SLICE_REPORT.md

[수정한 파일]

- 전투: CombatManager, TurnManager, UnitBase, EnemyUnit, UnitAIController, GA_AttackBase `.h/.cpp`; CombatGridManager `.h/.cpp`; CombatGridTile `.cpp`.
- UI/입력: PartyPlayerController, MainMenuPlayerController, CharacterCreationWidget, CombatHUDWidget `.h/.cpp`; UI_README.txt; CharacterCreationWidget.json.
- WorldMapGameModeBase `.h`: deprecated 보존 이유 주석만 추가.
- ProjectAEditor.Build.cs, DefaultEngine.ini, DefaultGame.ini.
- README.md, Docs/PROJECT_PLAN.md, Docs/TODO.md, Docs/CODE_REVIEW.md.
- 작업 시작 전부터 변경된 MainMenuRootWidget/PreviewStage와 GenerateUiScaffoldCommandlet, 기존 메뉴 JSON/에셋 변경은 이번 신규 구현 목록으로 집계하지 않는다.

[삭제한 파일]

- 없음.

[이름 변경한 파일]

- 없음. MainMenu의 옛 `StartGameLevelName` 프로퍼티 대신 새 `GameplayLevelName`을 사용하며, 기존 Blueprint에 직렬화된 WorldMap 값이 새 기본값을 덮지 않도록 분리했다. 기존 단일 이름/ClassId 시작 함수는 deprecated forwarding wrapper로 남겼다.

[Asset 변경]

- 새 `Content/User_JeHoon/LEVEL/Gameplay.umap`: 원본 TestMap 복제 + 새 GM/Arena/Camera. 최초 복제 후 NavMesh 데이터가 비어 실제 경로 조회가 실패하여 Gameplay 패키지만 `ResavePackages -BuildNavigationData`로 빌드·저장했다.
- 신규 Gameplay의 배치 Grid Z를 5로 설정하여 바닥과 타일 표면의 겹침을 방지했다. HUD의 하단 조작 패널에는 native/Designer 공통 `CommandPanelBackground`를 추가했다.
- 새 `Blueprint/Game/BP_GameplayGameMode.uasset`, `Blueprint/Controller/BP_GameplayPlayerController.uasset`.
- 새 `Blueprint/DataAsset/DA_VerticalSliceParty.uasset`, `DA_DefaultEncounter.uasset`.
- 새 `UI/Gameplay/WBP_GameplayRootWidget.uasset`, `WBP_RunMapWidget.uasset`, `WBP_CombatHUDWidget.uasset`, `WBP_EncounterResultWidget.uasset`.
- WBP 4종은 기존 commandlet으로 실제 Designer tree 생성, Blueprint 컴파일, 패키지 저장까지 완료했다. 마지막 HUD 배경 수정 후 해당 신규 WBP만 재생성했다. 별도 UE 프로세스에서 저장된 레벨/클래스/위젯 설정 29개를 모두 재검증했다.
- 원본 TestMap/MainMenu/WorldMap SHA256을 전후 비교해 동일함을 확인했다. 기존 메뉴/전투 Blueprint를 임의로 재저장하지 않았다.

[WorldMap 처리]

- `WorldMap.umap`과 `AWorldMapGameModeBase`는 deprecated / 미사용 상태로 보존.
- AssetRegistry 검색에서 WorldMap 패키지 외부 incoming reference는 0개. 해당 맵의 WorldSettings가 native WorldMapGameModeBase를 실제 참조한다.
- 활성 MainMenu C++ 경로는 GameplayLevelName을 사용한다. 옛 BP_PC의 직렬화된 WorldMap 프로퍼티는 새 필드에 적용되지 않는다.
- 클래스 삭제/이름 변경을 하지 않았으므로 CoreRedirect를 추가하지 않았다.

[Build.cs]

- Runtime `ProjectA.Build.cs`: 변경 없음. Editor dependency를 넣지 않았다.
- `ProjectAEditor.Build.cs`: PIE 통합 테스트의 실제 GAS HP 접근과 내비게이션 경로 검사를 위한 private `GameplayAbilities`, `NavigationSystem`, 후속 Slate 마우스 이벤트의 EKeys 링크를 위한 private `InputCore`를 추가했다.

[Config]

- DefaultEngine.ini: GameDefaultMap/EditorStartupMap을 MainMenu로 변경.
- DefaultGame.ini: MapsToCook에 MainMenu와 Gameplay를 명시해 FName OpenLevel 대상의 패키징 누락을 방지.
- PythonScriptPlugin은 commandlet 실행 인자로 일시 활성화했다. uproject/plugin 영구 설정은 변경하지 않았다.

[Blueprint / Asset 수동 작업]

- 필수 수동 연결은 없음. 신규 Gameplay/Blueprint/DataAsset/WBP와 Arena·카메라 참조를 생성·저장했다.
- Play 시작점은 `Content/User_JeHoon/LEVEL/MainMenu`다.
- 클래스·변수·Widget 이름·GameMode Override·카메라 좌표와 복구 클릭 순서는 [VERTICAL_SLICE_SETUP.md](VERTICAL_SLICE_SETUP.md)에 모두 기재했다.
- 직업별 실제 콘텐츠를 연결하려면 DA_VerticalSliceParty의 네 ClassId 매핑을 편집한다. 현재 공통 BP_PlayerUnit은 명시적 임시 fallback이다.

[P1 수정]

- R01: 이동과 독립된 행동 소유 상태, GAS 활성화 전 종료 delegate 연결, 공통 성공/실패/취소 결과, context 정리 후 완료 1회. 제자리형은 복귀 이동을 요구하지 않는다. 동기/no-montage 및 activation reject도 처리한다.
- R02: AI Wait를 StartSkill 전에 설정하고 다음 tick에 재판단/안전한 EndTurn으로 전이. 실패/취소는 원타일과 시작 transform 복구. controller request ID 및 snap 행동 번호로 오래된 콜백을 무시한다.
- 전투 정리는 활성 턴을 먼저 끄고 행동/ASC·이동·latent callback/AI continuation을 취소한다. 이미 소비한 AP/보조 AP는 환불하지 않는다.
- 실제 no-montage GAS 피해/AP, busy·중복 입력 차단, 실패/취소·완료 1회, AI 동기 종료, 초기 경로 거절, 파티/노드 전이 회귀 테스트 7건 통과.
- 접근/복귀 실패 테스트는 이동 phase를 주입하여 위치·점유·AI 회복을 확인한다. 실제 NavMesh 경로에서의 취소 마우스 재현과 구분한다.

[Visual Studio]

- UnrealBuildTool -ProjectFiles 재생성 성공.
- 변경 소스 51개 모두 `.vcxproj` / `.vcxproj.filters` 반영 확인. 누락 0개.
- Visual Studio 2022 Community에서 ProjectA.sln과 51개 파일을 자동으로 열었다. 실패 0개, EncounterManager.cpp를 마지막 검수 탭으로 열었다.
- 목록: `Saved/Automation/VisualStudioReviewFiles.txt`. 실제 열기 결과: `VisualStudioOpenedFiles.json`. 반영 검사: `VisualStudioProjectFiles.json`.

[Build]

- Development Editor | Win64 정식 UBT/UHT 빌드 성공. Live Coding으로 판정하지 않았다.
- 중간 C4458 지역 변수 충돌과 Editor 테스트의 GAS 링크 의존성을 수정했다. 최종 로그는 `Saved/Automation/DevelopmentEditorBuild.log`.

[검증]

후속 수정까지 포함한 최종 한 프로세스에서 자동화 8건 모두 통과: ProjectA.Combat.Actions 5건 + ProjectA.VerticalSlice.Run 2건 + SavedMapsPIELoop 1건. 총 101.88초, 실패 0건, 테스트 errors 0이다. 첫 Move/Skill은 실제 Slate 클릭 경로로 실행했고, 이후 기본 공격과 적 AI를 반복해 자연 Victory를 얻었다. 두 번째 전투 Defeat만 테스트용 lethal GAS 피해를 사용했다. 화면 캡처의 타일·HUD·결과 표시도 확인했다.

- VERTICAL-01: **통과(PIE)**. 메뉴 → 생성 슬롯 버튼 delegate → 이름/4슬롯 데이터 → Gameplay OpenLevel → Run Map 활성화.
- VERTICAL-02: **통과(PIE)**. Node delegate, Map 비활성화, Party/Enemy 각 1명, 16 tiles 생성/점유.
- VERTICAL-03: **통과(PIE, Slate 클릭 경로)**. HUD Move/Skill 버튼 → 타일 합성 마우스 클릭 → CommonUI → 게임 뷰포트 → 실제 NavMesh 이동/위치·점유 및 접근 스킬 GAS 피해/복귀 검증. 턴 전환과 Enemy AI 반복 실행도 통과.
- VERTICAL-04: **통과(PIE + 회귀)**. 실제 기본 공격의 몽타주를 사용하는 transient 제자리 스킬: busy/입력 차단, 완료 1회, 원위치 유지, 이후 기본 공격 정상 실행. 별도 자동화는 no-montage 동기 완료/적 AI 복구 검증.
- VERTICAL-05: **통과(회귀, 이동 phase 주입)**. 접근/복귀 실패·취소의 AI 대기 탈출/완료 1회/위치·점유 복구와 실제 no-nav 요청 즉시 거절 확인. 실제 플레이 중 NavMesh 이동을 마우스로 취소하는 시나리오는 미실행.
- VERTICAL-06: **통과(PIE)**. 기본 공격 반복으로 마지막 Enemy 사망 → Victory 1회, 전투/턴/입력 잠금, Result 활성화.
- VERTICAL-07: **통과(PIE)**. Unit/Combat/Turn 등록·타일 점유 정리 확인 → Result Continue 버튼 delegate → Run Map 복귀.
- VERTICAL-08: **통과(PIE)**. 이전 Unit weak reference 무효화/등록 0 확인 후 같은 UWorld에서 두 번째 node 실행.
- VERTICAL-09: **통과(PIE, lethal GAS 피해 주입)**. Defeat 1회, 전투 정지/정리, Defeat UI, Continue 거절, 추가 결과 없음.
- VERTICAL-10: **통과**. Development Editor | Win64 정식 UHT/UBT 빌드.

[미구현 / 임시 처리]

- 네 직업 모두 BP_PlayerUnit 공통 테스트 전투 클래스, 기본 직업 이름+슬롯 번호. 이름 설정 API는 제공하지만 슬롯별 상세 편집 화면은 미완성.
- 두 순차 Combat 노드는 같은 DefaultEncounter를 사용한다. 랜덤 맵/다른 노드 종류/Streaming은 미구현.
- Victory는 Continue만 제공. 실제 카드/아이템 보상, 회복, 인벤토리, SaveGame/Continue, Options, 직업별 스킬 완성/밸런싱은 범위 밖.
- R03/R04/R06 비용·타겟·범위 P2 통합은 남겼다. 스킬 액터 impact 완료 책임도 후속 항목이다.
- 패키징 실행과 실제 사용자 마우스로 전 과정을 조작한 검증은 별도다.

[기존 정상 기능 중 변경한 것]

- 행동 종료 규약과 이동 실패/취소 회복: R01/R02 및 반복 Encounter 안정성에 필요.
- Player 팀/활성 전투 입력 검사와 AI 내부 EndTurn 분리: persistent HUD가 적 또는 종료된 전투를 조작하는 것을 막기 위한 R05 최소 수정.
- Grid 재생성 시 이전 타일 Destroy, 점유/표시 reset: 반복 전투에서 유령 타일/등록 잔존 방지.
- 현재 턴 유닛 사망 후 다른 유닛이 살아 있으면 다음 tick 턴 진행: 입력 불가능한 사망 유닛에서 멈추는 상황 방지.
- CombatHUD를 CommonActivatableWidget으로 연결하며 기존 명령 함수를 재사용. 기존 피해량·스킬 세트·AI 점수/타겟·비용 정책은 변경하지 않았다.

[다음 추천 작업]

- DA_VerticalSliceParty의 공통 BP_PlayerUnit fallback을 네 직업별 PlayerUnit 정의로 교체.

## 검증 근거

- [최종 전체 자동화 8건과 PIE 로그](../Saved/Logs/GameplayInputFix2.log), [최종 JSON 결과](../Saved/Automation/GameplayInputFix2/index.json)
- [저장 에셋 29항목 검증](../Saved/Automation/GameplayAssetValidation.json)
- [Gameplay NavMesh 최종 빌드 로그](../Saved/Automation/BuildGameplayNavigationFinal.stdout.txt)
- [Run Map 캡처](../Saved/Automation/VerticalSliceScreenshots/01-RunMap.png), [Combat 캡처](../Saved/Automation/VerticalSliceScreenshots/02-Combat.png), [Victory 캡처](../Saved/Automation/VerticalSliceScreenshots/03-Victory.png), [Defeat 캡처](../Saved/Automation/VerticalSliceScreenshots/04-Defeat.png)
- [정식 빌드 로그](../Saved/Automation/DevelopmentEditorBuild.log), [VS 프로젝트 재생성 로그](../Saved/Automation/VisualStudioProjectGeneration.log)
- [에디터 설정과 실행 순서](VERTICAL_SLICE_SETUP.md)

초기 NullRHI PIE는 엔진 GenericWindow에서 실패하여 실제 renderer와 RenderOffscreen으로 전환했다. 초기 복제 맵의 경로 실패는 Gameplay navigation data를 빌드·저장해 해결했다. transient 회귀 테스트의 AttributeSet 등록 누락과 정리된 TurnManager를 읽던 테스트의 null 가정도 테스트 코드에서 수정했다. 최종 테스트 통과 기록은 위 로그를 기준으로 한다. 엔진 시작 시의 LogAutomationTest 자체 진단 메시지는 요청 테스트 결과와 별도로 발생했다. 최종 테스트 4건에 경고 8개가 남으며, 이동 실패/AP 부족 주입 경고와 기존 GameplayCue 경로·PreviewStage/미지정 spawn·회귀 World의 Recast 경고다. 요청한 테스트 결과는 8건 모두 Success다.

## C++ 파일 전체 목록

- 수정: [Source/ProjectA/Combat/CombatManager.cpp](../Source/ProjectA/Combat/CombatManager.cpp)
- 수정: [Source/ProjectA/Combat/CombatManager.h](../Source/ProjectA/Combat/CombatManager.h)
- 추가: [Source/ProjectA/Controller/GameplayPlayerController.cpp](../Source/ProjectA/Controller/GameplayPlayerController.cpp)
- 추가: [Source/ProjectA/Controller/GameplayPlayerController.h](../Source/ProjectA/Controller/GameplayPlayerController.h)
- 수정: [Source/ProjectA/Controller/MainMenuPlayerController.cpp](../Source/ProjectA/Controller/MainMenuPlayerController.cpp)
- 수정: [Source/ProjectA/Controller/MainMenuPlayerController.h](../Source/ProjectA/Controller/MainMenuPlayerController.h)
- 수정: [Source/ProjectA/Controller/PartyPlayerController.cpp](../Source/ProjectA/Controller/PartyPlayerController.cpp)
- 수정: [Source/ProjectA/Controller/PartyPlayerController.h](../Source/ProjectA/Controller/PartyPlayerController.h)
- 수정: [Source/ProjectA/Controller/UnitAIController.cpp](../Source/ProjectA/Controller/UnitAIController.cpp)
- 수정: [Source/ProjectA/Controller/UnitAIController.h](../Source/ProjectA/Controller/UnitAIController.h)
- 추가: [Source/ProjectA/DataAsset/EncounterDefinitionDataAsset.h](../Source/ProjectA/DataAsset/EncounterDefinitionDataAsset.h)
- 추가: [Source/ProjectA/DataAsset/PartyDefinitionDataAsset.cpp](../Source/ProjectA/DataAsset/PartyDefinitionDataAsset.cpp)
- 추가: [Source/ProjectA/DataAsset/PartyDefinitionDataAsset.h](../Source/ProjectA/DataAsset/PartyDefinitionDataAsset.h)
- 추가: [Source/ProjectA/Game/Encounter/CombatArena.cpp](../Source/ProjectA/Game/Encounter/CombatArena.cpp)
- 추가: [Source/ProjectA/Game/Encounter/CombatArena.h](../Source/ProjectA/Game/Encounter/CombatArena.h)
- 추가: [Source/ProjectA/Game/Encounter/EncounterManager.cpp](../Source/ProjectA/Game/Encounter/EncounterManager.cpp)
- 추가: [Source/ProjectA/Game/Encounter/EncounterManager.h](../Source/ProjectA/Game/Encounter/EncounterManager.h)
- 추가: [Source/ProjectA/Game/GameModes/GameplayGameModeBase.cpp](../Source/ProjectA/Game/GameModes/GameplayGameModeBase.cpp)
- 추가: [Source/ProjectA/Game/GameModes/GameplayGameModeBase.h](../Source/ProjectA/Game/GameModes/GameplayGameModeBase.h)
- 수정: [Source/ProjectA/Game/GameModes/WorldMapGameModeBase.h](../Source/ProjectA/Game/GameModes/WorldMapGameModeBase.h)
- 추가: [Source/ProjectA/Game/Run/RunStateSubsystem.cpp](../Source/ProjectA/Game/Run/RunStateSubsystem.cpp)
- 추가: [Source/ProjectA/Game/Run/RunStateSubsystem.h](../Source/ProjectA/Game/Run/RunStateSubsystem.h)
- 추가: [Source/ProjectA/Game/Run/RunTypes.h](../Source/ProjectA/Game/Run/RunTypes.h)
- 수정: [Source/ProjectA/Game/Turn/TurnManager.cpp](../Source/ProjectA/Game/Turn/TurnManager.cpp)
- 수정: [Source/ProjectA/Game/Turn/TurnManager.h](../Source/ProjectA/Game/Turn/TurnManager.h)
- 수정: [Source/ProjectA/GAS/Ability/GA_AttackBase.cpp](../Source/ProjectA/GAS/Ability/GA_AttackBase.cpp)
- 수정: [Source/ProjectA/GAS/Ability/GA_AttackBase.h](../Source/ProjectA/GAS/Ability/GA_AttackBase.h)
- 수정: [Source/ProjectA/Grid/Combat/CombatGridManager.cpp](../Source/ProjectA/Grid/Combat/CombatGridManager.cpp)
- 수정: [Source/ProjectA/Grid/Combat/CombatGridManager.h](../Source/ProjectA/Grid/Combat/CombatGridManager.h)
- 수정: [Source/ProjectA/Grid/Combat/CombatGridTile.cpp](../Source/ProjectA/Grid/Combat/CombatGridTile.cpp)
- 추가: [Source/ProjectA/Tests/RunStateTests.cpp](../Source/ProjectA/Tests/RunStateTests.cpp)
- 추가: [Source/ProjectA/Tests/UnitActionLifecycleTests.cpp](../Source/ProjectA/Tests/UnitActionLifecycleTests.cpp)
- 추가: [Source/ProjectA/Types/CombatResult.h](../Source/ProjectA/Types/CombatResult.h)
- 추가: [Source/ProjectA/Types/UnitActionTypes.h](../Source/ProjectA/Types/UnitActionTypes.h)
- 수정: [Source/ProjectA/UI/Combat/CombatHUDWidget.cpp](../Source/ProjectA/UI/Combat/CombatHUDWidget.cpp)
- 수정: [Source/ProjectA/UI/Combat/CombatHUDWidget.h](../Source/ProjectA/UI/Combat/CombatHUDWidget.h)
- 추가: [Source/ProjectA/UI/Gameplay/EncounterResultWidget.cpp](../Source/ProjectA/UI/Gameplay/EncounterResultWidget.cpp)
- 추가: [Source/ProjectA/UI/Gameplay/EncounterResultWidget.h](../Source/ProjectA/UI/Gameplay/EncounterResultWidget.h)
- 추가: [Source/ProjectA/UI/Gameplay/GameplayActionButton.cpp](../Source/ProjectA/UI/Gameplay/GameplayActionButton.cpp)
- 추가: [Source/ProjectA/UI/Gameplay/GameplayActionButton.h](../Source/ProjectA/UI/Gameplay/GameplayActionButton.h)
- 추가: [Source/ProjectA/UI/Gameplay/GameplayRootWidget.cpp](../Source/ProjectA/UI/Gameplay/GameplayRootWidget.cpp)
- 추가: [Source/ProjectA/UI/Gameplay/GameplayRootWidget.h](../Source/ProjectA/UI/Gameplay/GameplayRootWidget.h)
- 추가: [Source/ProjectA/UI/Gameplay/RunMapWidget.cpp](../Source/ProjectA/UI/Gameplay/RunMapWidget.cpp)
- 추가: [Source/ProjectA/UI/Gameplay/RunMapWidget.h](../Source/ProjectA/UI/Gameplay/RunMapWidget.h)
- 수정: [Source/ProjectA/UI/MainMenu/CharacterCreationWidget.cpp](../Source/ProjectA/UI/MainMenu/CharacterCreationWidget.cpp)
- 수정: [Source/ProjectA/UI/MainMenu/CharacterCreationWidget.h](../Source/ProjectA/UI/MainMenu/CharacterCreationWidget.h)
- 수정: [Source/ProjectA/Unit/EnemyUnit.cpp](../Source/ProjectA/Unit/EnemyUnit.cpp)
- 수정: [Source/ProjectA/Unit/EnemyUnit.h](../Source/ProjectA/Unit/EnemyUnit.h)
- 수정: [Source/ProjectA/Unit/UnitBase.cpp](../Source/ProjectA/Unit/UnitBase.cpp)
- 수정: [Source/ProjectA/Unit/UnitBase.h](../Source/ProjectA/Unit/UnitBase.h)
- 추가: [Source/ProjectAEditor/Tests/VerticalSliceFlowTests.cpp](../Source/ProjectAEditor/Tests/VerticalSliceFlowTests.cpp)
