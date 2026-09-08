# Vertical Slice 실행과 에셋 설정

2026-09-08. 코드/빌드/에셋/PIE의 실제 검증 결과는 [작업 보고](VERTICAL_SLICE_REPORT.md)에 기록한다.

## 바로 실행

1. Unreal Engine 5.7에서 `ProjectA.uproject`를 연다.
2. Content Browser → `Content/User_JeHoon/LEVEL/MainMenu`를 더블클릭한다. 기본 시작 맵도 MainMenu다.
3. Play → New Game → 하단 슬롯의 `캐릭터 생성하기`를 한 번 이상 누른다. 직업 화살표로 선택한다.
4. Start Game → `Gameplay`의 Run Map → 첫 Combat 노드를 누른다.
5. HUD에서 스킬을 선택하고 적 타일을 클릭한다. Move를 선택한 경우 표시된 빈 아군 타일을 클릭한다. 행동 완료 후 End Turn으로 적 턴을 진행한다.
6. 적을 처치하면 Victory → Continue → Run Map에서 두 번째 Combat 노드를 선택한다.
7. 두 번째 Victory 뒤 Continue는 완료된 Run Map을 표시한다. 파티 전멸 시 Defeat 화면에 머물며 추가 행동을 받지 않는다.

첫 설정은 기존 기본 공격/스탯을 그대로 사용하며 밸런싱하지 않았다. HP는 전투 사이에 유지한다. 네 직업의 완성 콘텐츠, 전투 사이 회복, 부활, 보상 아이템은 아직 없다.

## 저장된 연결

| 에셋 | 클래스 / 설정 |
|---|---|
| `LEVEL/Gameplay` | TestMap geometry·NavMesh·Grid 복제, `BP_GameplayGameMode` Override |
| `Blueprint/Game/BP_GameplayGameMode` | 부모 `AGameplayGameModeBase`, `PartyDefinition=DA_VerticalSliceParty`, `EncounterDefinitions[DefaultEncounter]=DA_DefaultEncounter`, `PlayerControllerClass=BP_GameplayPlayerController` |
| `Blueprint/Controller/BP_GameplayPlayerController` | 부모 `AGameplayPlayerController`, `GameplayRootWidgetClass=WBP_GameplayRootWidget` |
| `Blueprint/DataAsset/DA_VerticalSliceParty` | `UPartyDefinitionDataAsset`, 네 ClassId 매핑/`FallbackPlayerUnitClass=BP_PlayerUnit` |
| `Blueprint/DataAsset/DA_DefaultEncounter` | `UEncounterDefinitionDataAsset`, `EnemyUnitClasses[0]=BP_EnemyUnit` |
| `GameplayCombatArena` | `ACombatArena`, `Grid=배치된 BP_CombatGridManager`, `CameraAnchor=GameplayCamera` |
| 배치 Grid | Rows/Cols `4`, 위치 Z `5`; 바닥과 타일 표면의 겹침 방지 |
| Arena `PlayerCoords` | 슬롯 0~3 → `(0,1), (1,1), (2,1), (3,1)` |
| Arena `EnemyCoords` | `(0,2), (1,2), (2,2), (3,2)`; 현재 첫 좌표만 사용 |
| `GameplayCamera` | `ACameraActor`, 위치 `(-300,-1000,1500)`, 회전 Pitch `-46.97` / Yaw `90`, FOV `55` |
| `UI/Gameplay/WBP_GameplayRootWidget` | 부모 `UGameplayRootWidget`, 아래 세 화면 클래스 지정 |
| `UI/Gameplay/WBP_RunMapWidget` | 부모 `URunMapWidget` |
| `UI/Gameplay/WBP_CombatHUDWidget` | 부모 `UCombatHUDWidget` |
| `UI/Gameplay/WBP_EncounterResultWidget` | 부모 `UEncounterResultWidget` |

모든 경로는 `/Game/User_JeHoon/` 기준이다. 기존 `UI/Combat/WBP_CombatHUDWidget`은 변경하지 않았으며 새 Gameplay HUD가 별도 경로에 저장된다. 새 클래스들을 Blueprint asset path로 런타임 Load하지 않는다. 데이터/Blueprint 기본값의 참조를 사용한다.

## 에디터에서 연결을 수정하는 정확한 순서

이미 생성된 에셋에는 필수 수동 연결 작업이 없다. 설정 변경 또는 복구 시 아래 순서를 따른다.

1. `DA_VerticalSliceParty`를 열어 `Player Unit Classes`의 `StableHand`, `Scholar`, `Herbalist`, `Hunter`에 원하는 `APlayerUnit` 자식 클래스를 지정한다. 기본 공통 클래스는 `Fallback Player Unit Class`에 지정하고 Save한다.
2. `DA_DefaultEncounter`를 열어 `Enemy Unit Classes` 배열에 `AEnemyUnit` 자식 클래스를 지정하고 Save한다.
3. `BP_GameplayGameMode` → Class Defaults → Gameplay에서 `Party Definition`, `Encounter Definitions`의 `DefaultEncounter`, `Combat Manager Class=ACombatManager`, `Encounter Manager Class=AEncounterManager`를 확인한다. Player Controller Class는 `BP_GameplayPlayerController`, Default Pawn Class/HUD Class는 None이다. Compile → Save한다.
4. `BP_GameplayPlayerController` → Class Defaults → `Gameplay Root Widget Class=WBP_GameplayRootWidget` → Compile → Save한다.
5. `WBP_GameplayRootWidget` → Class Defaults에서 `Run Map Widget Class=WBP_RunMapWidget`, `Combat HUD Widget Class=UI/Gameplay/WBP_CombatHUDWidget`, `Result Widget Class=WBP_EncounterResultWidget` → Compile → Save한다.
6. `Gameplay`를 열어 World Settings → GameMode Override=`BP_GameplayGameMode`를 지정한다.
7. World Outliner → `GameplayCombatArena` → Details → `Grid`를 배치된 `BP_CombatGridManager`, `Camera Anchor`를 `GameplayCamera`로 지정한다. Player/Enemy Coords는 위 표 기준이다. 여러 Arena를 배치한다면 사용할 아레나 Actor Tags에 `GameplayArena`를 넣는다.
8. Grid의 `Tile Class=BP_CombatGridTile`, Rows/Cols=4, Transform Location Z=5를 확인한다. P 키로 NavMesh가 전투 바닥/스폰 위치를 덮는지 확인하고 필요 시 Build → Build Paths를 실행한다. Save All한다.
9. `BP_MainMenuPlayerController` → Class Defaults → `Gameplay Level Name=/Game/User_JeHoon/LEVEL/Gameplay`를 확인한다. 이전 에셋에 직렬화된 `StartGameLevelName=WorldMap`은 제거된 옛 필드이며 실행에 사용하지 않는다. 새 필드로 잘못된 기존 override의 이관을 방지한다.

## Designer 위젯 이름

- GameplayRoot: `RootOverlay`, `RunLayer`, `CombatLayer`, `ModalLayer` (세 레이어는 `CommonActivatableWidgetStack`).
- RunMap: `Text_Progress`, `Text_Party`, `Text_FlowMessage`, `NodeList` (`VerticalBox`). 노드 버튼은 정의 배열에서 런타임 생성한다.
- Combat HUD: `CommandPanelBackground` (`Border`), `Text_Turn`, `Text_Action`, `SkillList` (`HorizontalBox`), `Button_Move`, `Button_EndTurn`, `Button_Cancel`. 하단 조작 패널에 어두운 배경을 두며 스킬 버튼은 유닛 스킬 정의에서 런타임 생성한다.
- Result: `Text_Result`, `Button_Continue`. 보상 선택 기능은 이 화면과 Encounter Continue 앞에 추가한다.

native fallback은 같은 BindWidgetOptional 이름을 사용한다. JSON spec을 수정한 뒤 실제 생성/compile/save까지 검증해야 한다. DryRun만으로 WBP 완료로 기록하지 않는다.

입력 정책은 native `GetDesiredInputConfig`에 정의한다. CombatHUD는 All/CaptureDuringMouseDown, RunMap/Result는 Menu/NoCapture를 사용한다. GameplayController에서 별도 SetInputMode를 추가하면 CommonUI 상태와 어긋날 수 있으므로 화면별 모드 변경은 이 설정에서 관리한다. 후속 클릭 수정은 native 코드에 적용되어 WBP 재생성이나 추가 에셋 연결이 필요하지 않다.

## 재생성 도구

- `Source/ProjectAEditor/Scripts/AuditGameplayAssets.py`: AssetRegistry와 실제 로드한 Blueprint/맵의 참조 읽기, Saved에 JSON 기록.
- `Source/ProjectAEditor/Scripts/ConfigureGameplayAssets.py`: 최초 새 Gameplay 에셋 생성. 대상이 이미 있으면 덮어쓰지 않고 중단하므로 현재 저장소에서는 재실행하지 않는다.
- 최초 생성 후 반드시 아래 `ResavePackages -BuildNavigationData`를 Gameplay 패키지 하나에 실행하여 실제 NavMesh 데이터를 빌드·저장한다. Bounds/Recast actor 존재만으로 이동 경로가 생성되지는 않는다.
- `Source/ProjectAEditor/Scripts/ValidateGameplayAssets.py`: 저장된 에셋의 부모/클래스/위젯/스폰/카메라 참조 재검증.
- 신규 JSON 4종은 기존 `GenerateUiScaffold` commandlet으로 관리한다. 기존 Generator 기능 확장이나 Python plugin 영구 활성화는 하지 않았다.

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject' -run=PythonScript -EnablePlugins=PythonScriptPlugin -script='Source/ProjectAEditor/Scripts/ValidateGameplayAssets.py' -unattended -nop4 -NullRHI
```

Gameplay 최초 생성 후 내비게이션 빌드·저장:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject' -run=ResavePackages -BuildNavigationData -Package=/Game/User_JeHoon/LEVEL/Gameplay -unattended -nop4 -NullRHI
```

실제 작업 중 복제 직후에는 경로 조회가 실패했고, 이 명령으로 Gameplay의 navigation data를 저장한 뒤 같은 PIE 이동 경로 조회가 성공했다. 원본 TestMap/MainMenu/WorldMap은 변경하지 않았다. `ConfigureGameplayAssets → Gameplay Navigation Build → ValidateGameplayAssets → PIE` 순서로 검증한다.

## 검증 구분

`ProjectA.Combat.Actions`는 실제 GAS 동기 실행과 주입한 실패/취소 조건의 회귀 테스트다. `ProjectA.VerticalSlice.Run`은 데이터/진행 상태 전이 테스트다. `ProjectA.VerticalSlice.SavedMapsPIELoop`는 저장된 메뉴/WBP/Gameplay를 실제 PIE로 연다. 메뉴 생성·노드 선택·Continue는 위젯 delegate를 호출하며, 첫 Move/Skill은 실제 Slate 합성 마우스로 버튼과 타일을 클릭하여 히트 테스트·CommonUI·게임 뷰포트 전달을 포함한다. 이후 기존 공격과 적 AI를 반복하여 자연 승리하고, 두 번째 전투의 Defeat만 테스트용 lethal GAS 피해를 사용한다. 물리적 마우스로 전체 흐름을 수동 플레이한 검증과 이동 중 마우스 취소 재현은 별도 미검증 항목이다. 화면 외형은 저장된 PIE 캡처를 직접 검수한다.

WorldMap 레벨과 native class는 deprecated로 보존한다. WorldMap 패키지 외부 참조는 감사에서 없었으나 해당 맵의 WorldSettings가 `WorldMapGameModeBase`를 참조한다. 클래스 삭제/이름 변경을 하지 않았으므로 CoreRedirect를 추가하지 않는다.
