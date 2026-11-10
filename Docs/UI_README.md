# UI 구조·생성 도구

기준일: 2026-11-10. UE 5.8의 화면 구성과 JSON 기반 Widget Blueprint 생성 규칙만 적는다. 데이터·저장 계약은 [PROJECT_PLAN](PROJECT_PLAN.md), 게임 규칙은 [GAME_STATUS](GAME_STATUS.md)를 따른다.

## 1 구성과 편집 원칙

| 구성 | 위치·역할 |
|---|---|
| 생성 도구 | [GenerateUiScaffoldCommandlet.cpp](../Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.cpp), Editor 전용 |
| 명세 | [UiScaffoldSpecs](../Source/ProjectAEditor/UiScaffoldSpecs): 메뉴 3종·Gameplay 4종 |
| Native UI | [Source/ProjectA/UI](../Source/ProjectA/UI), 화면 동작·선택적 fallback |
| 공통 테마 | [DemonicUITheme](../Source/ProjectA/UI/Theme/DemonicUITheme.h) |
| WBP | `/Game/User_JeHoon/UI/MainMenu`, `/Game/User_JeHoon/UI/Gameplay` |
| 검증 사본 | `/Game/User_JeHoon/UI/Validation/T12` |

- Designer WBP가 화면 구조의 기준이다. JSON은 초기 생성·바인딩 검증·누락 보완에만 쓴다.
- 라운드 계획·상점·인벤토리·설정·백과사전 등은 native CommonUI로 생성하며 WBP 재생성이 필요 없다.
- 버튼·배경·글자색은 native 테마가 실행 시 적용하고 나머지 배치·폰트는 Designer에서 편집한다.
- Editor API·의존성은 ProjectAEditor에만 둔다.

## 2 실행과 옵션

### 지도·진행 표시

- 새 목표 Run 지도는 전투 20개와 `완료 수/20`을 표시한다. 인카운터 화면 제목은 `화면 이름 · 완료 수/80 완료`다. 기존 10전투·2전투 저장은 분모 10·2를 유지한다.
- 알려진 `RootOverlay > ContentBox > NodeList` 계층에서는 목록을 최대 높이 300의 `ScrollBox`로 감싼다. 다른 사용자 계층은 재작성하지 않는다.

### 인카운터 선택 (`URunEncounterWidget`, RunLayer)

- 통합 Gameplay: 배경·큰 패널을 숨기고 하단에 **왼쪽 문·직진·오른쪽 문** 3열 카드를 표시한다. `DungeonChoiceBar` 기준 너비 1120, 카드 최소 높이 104. 후보 0·1·2를 좌·중·우에 고정한다.
- 이동 연출 중에는 RunLayer를 숨겨 입력을 잠근다. 연출 없는 맵은 기존 세로 선택을 쓴다.
- UI와 로컬 카메라는 Host가 고정한 `DungeonState`의 후보 순서를 읽고 별도 추첨하지 않는다. 미로·NPC 연출은 [PROJECT_PLAN 4-13](PROJECT_PLAN.md#4-13-통합-gameplay와-npc-상점)을 따른다.

### 상점 화면

- 입장 연출 후 배치(UI 단위): 왼쪽 장비 300·간격 16·가방 430, 중앙 NPC용 투명 공간 480, 오른쪽 상품 620. 총 1846×840, 공통 DPI와 `DownOnly` 축소.
- 상품 카드는 분류/스킬 아이콘·이름·가격·구매 버튼이다. 긴 이름은 줄바꿈하고 목록은 스크롤한다.
- 아이템상점은 장착 지원 50개만 판매한다. 기본상점 5칸, 전문 상점은 적격 수와 5 중 작은 수. **리롤 1G**.
- 잔액 부족·사망·요청 처리 중에는 구매·리롤을 막는다. 장착 미지원 진열은 숨긴다. 전문 상점에 적격 상품이 없으면 빈 안내와 퇴장만 제공한다.
- 조회 캐릭터는 `GetInventoryCharacterId`이며 사망 후에도 장비·가방을 표시한다. 구매·드래그는 서버의 생존 Human 검증을 따른다.
- 구매·리롤 갱신은 입장 연출을 반복하지 않는다. Continue·늦은 접속은 도착점에서 시작한다.
- 서비스 버튼: **샘물 마시기 · HP 25 회복 · 1G**, **회복 소모품 1개 · 보유 N개 · 1G**, **최대 HP 25%로 부활 · 1G**. 회복·소모품은 본인 생존 Human, 부활은 본인 사망 Human만 가능하다.
- 기존 경로(버전 0·prototype·개발 협동)의 스킬상점은 61후보 중 최대 5개, **리롤 1G**(성공마다 +1G), 1G HP 전체 회복을 표시한다.
- 데이터·저장 계약: [PROJECT_PLAN 3-2](PROJECT_PLAN.md#3-2-상점-인카운터)

### 아이템 툴팁 (`UShopItemTooltipWidget`)

- 카드 전체에 연결한다. 분류 아이콘, 등급별 이름/색, 가격/구매 상태, 장착 위치/양손 여부, 부여 스킬별 효과와 수치를 표시한다.
- 등급은 복제된 상품과 `ItemRarities`, 스킬 수치는 사본의 `GrantedSkillBalances`를 쓴다. 현재 CSV나 원본 기본 위력으로 대체하지 않는다.
- 설명은 상점 갱신 시점에 구성하고 hover마다 에셋을 로드하지 않는다.
- `ConfigureInventory`·`ConfigureReward`는 가격·구매 문구 대신 장착/보관 또는 보상 상태를 안내한다.

### PvE 난이도와 Snapshot 준비 (`RunMapWidget`)

- 세 번째 방문 뒤 `Map`에서 하·중·상 3카드를 표시한다(왼쪽 문·하 / 직진·중 / 오른쪽 문·상). 난이도·전투장·등장 몬스터·배율·골드를 보여준다.
- 몬스터 이름·역할·HP/속도는 서버 View의 실제 편성에서 표시한다. 카드 전체가 NodeId와 난이도 태그를 Host 명령 경로로 보낸다.
- 선택 후 통로 이동 중 오른쪽 아래에 회전 표시와 `불러오는 중`을 표시한다. 전투 Continue는 이동을 재생하지 않는다.
- Snapshot 준비 화면은 위쪽 상대 최대 4칸·가운데 `전투 시작`·아래쪽 아군 최대 4칸이다. 이름·직업·HP·속도를 표시한다. 비Host와 상대 정보 누락은 버튼을 비활성화한다.

### 생성 도구 실행

Development Editor / Win64 빌드 후 프로젝트 루트에서 실행한다. 빌드 명령은 [README](../README.md#개발-환경과-빌드).

```powershell
$editor = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = Join-Path (Get-Location) 'ProjectA.uproject'
$spec = 'Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json'
& $editor $project -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun "-Spec=$spec"
```

| 옵션 | 동작 |
|---|---|
| `-Spec=...` | 명세 경로. 생략 시 MainMenuRootWidget.json |
| `-DryRun` | JSON 구조 검사만. 클래스 로드·컴파일·저장은 제외 |
| 변경 옵션 없음 | 신규 WBP 생성. 기존 WBP는 수정하지 않음 |
| `-AddMissing` | 기존 속성·계층 유지, 누락 위젯만 추가 후 컴파일·저장. 타입 불일치 시 거절 |
| `-Overwrite` | 기존 WidgetTree를 JSON 기준으로 재생성 |

- `-AddMissing`과 `-Overwrite`는 함께 쓸 수 없다.
- 기존 WBP의 부모가 nativeClass와 다르면 중단한다. Overwrite 전에는 Designer 수정 내용을 확인한다.

## 3 JSON 명세

```json
{
  "nativeClass": "UMainMenuScreenWidget",
  "nativeHeaderPath": "Source/ProjectA/UI/MainMenu/MainMenuScreenWidget.h",
  "nativeSourcePath": "Source/ProjectA/UI/MainMenu/MainMenuScreenWidget.cpp",
  "blueprintName": "WBP_MainMenuScreenWidget",
  "assetPath": "/Game/User_JeHoon/UI/MainMenu",
  "parentClass": "UCommonActivatableWidget",
  "generateNativeSource": false,
  "widgets": [{ "name": "RootOverlay", "type": "Overlay", "parent": "" }]
}
```

| 최상위 필드 | 규칙 |
|---|---|
| `nativeClass` | 필수. U 접두사의 Native 클래스명 |
| `nativeHeaderPath`, `nativeSourcePath` | 필수 |
| `blueprintName` | 필수. WBP 이름 |
| `assetPath` | 필수. `/Game/User_JeHoon` 또는 하위 경로 |
| `parentClass` | 필수. UUserWidget / UCommonUserWidget / UCommonActivatableWidget |
| `generateNativeSource` | 생략 시 true. 현재 명세는 모두 false |
| `widgets` | 필수. 1개 이상, 루트는 1개 |

- Native 생성은 UUserWidget 부모·PROJECTA_API만 지원하고 `bind=true` 항목의 BindWidget 속성을 만든다.
- 신규 클래스 순서: 소스 생성 → 프로젝트 파일 재생성 → Editor 빌드 → 도구 재실행.

### 3-1 위젯 필드

| 필드 | 규칙·기본값 |
|---|---|
| `name`, `type` | 필수. 이름 중복 금지 |
| `parent` | 부모 name. 루트는 빈 문자열 |
| `bind` | 기본 false. true면 Designer 변수 등록 |
| `text` | TextBlock 초기 문자열 |
| `horizontalAlignment` | Fill / Left / Center / Right, 기본 Center |
| `verticalAlignment` | Fill / Top / Center / Bottom, 기본 Center |
| `padding` | 숫자 4개(Left·Top·Right·Bottom) |
| `sizeRule`, `fill` | HorizontalBox 자식: Auto(기본) / Fill. Fill 비율은 양수, 기본 1 |
| `color` | RGBA 숫자 4개 |
| `widthOverride`, `heightOverride` | SizeBox 크기. 양수 |

### 3-2 타입·계층·바인딩

| 자식 허용 | 지원 타입 |
|---|---|
| 여러 개 | CanvasPanel, Overlay, HorizontalBox, VerticalBox |
| 최대 1개 | Button, Border, SizeBox |
| 없음 | TextBlock, EditableTextBox, Image, CommonActivatableWidgetStack |

- 부모 누락·순환 참조·복수 루트·자식 수 위반·미지원 타입·잘못된 숫자는 생성 전에 거절한다.
- JSON name과 C++의 BindWidget/BindWidgetOptional 이름·타입이 일치해야 한다. `bind=true`는 버튼 동작을 구현하지 않는다.

## 4 화면·프리뷰

| 대상 | 동작 |
|---|---|
| MainMenuRoot | MainStack·MenuStack·ModalStack. MenuStack 활성 시 MainStack을 숨기고 Back/X로 복원 |
| GameModeSelection | 싱글은 CharacterCreation, 멀티는 `UDevelopmentCoopWidget` |
| CharacterCreation | 생성 버튼으로 즉시 생성, Edit로 편집, 슬롯 X로 제거. Back/X는 초안·프리뷰 정리 |
| PreviewStage | MainMenu의 월드 Actor·카메라. SceneCapture2D·RenderTarget 미사용 |
| GameplayRoot | RunLayer·CombatLayer·ModalLayer는 CommonActivatableWidgetStack |
| 입력 | RoundPlanning은 All/CaptureDuringMouseDown, RunMap·Result는 Menu/NoCapture |

프리뷰 수치는 `/Game/User_JeHoon/LEVEL/Core/MainMenu`의 `MainMenuPreviewStage` Details에서 고친다.

| 조절 대상 | Details 위치 | 현재값 |
|---|---|---|
| 전체 기준 거리 | `PreviewCamera` → Location X | `-500` (실제 거리는 몸체 투영·화면 폭에 맞춰 보정) |
| 캐릭터 기본 방향 | `Slot0Anchor`~`Slot3Anchor` → Rotation Z | 모두 `90°` |
| 캐릭터 간격 | `Slot0Anchor`~`Slot3Anchor` → Location Y | `-450 / -150 / 150 / 450` |
| Edit 확대 크기 | Preview → Camera → `Focused Camera Distance Scale` | `1.15` (`1.0` 미만은 전신이 잘릴 수 있음) |

- 전체 보기는 실제 메시 경계를 투영해 좌우 10% 여백에 맞춘다. 화면 크기·슬롯·외형이 바뀌면 기준 카메라에서 다시 계산한다.
- 프리뷰 Blueprint: `BP_WarriorMenuPreview`·`BP_MageMenuPreview`·`BP_RogueMenuPreview`·`BP_PartyMenuPreview`(궁수). `PreviewMeshTransform` 회전은 `0°`이고 앵커가 방향을 정한다.
- Edit 편집창: ◀·▶로 몸체(`BodyVariants` 순서) 선택, 미리보기 영역 **우클릭 드래그**로 회전, **저장**·**취소**.
- `Text_StartGameStatus`는 선택 바인딩이다. 배치·바인딩은 [PROJECT_PLAN 6절](PROJECT_PLAN.md#6-gameplay-에셋과-배치).

## 5 검증·문제 해결

- 생성 결과는 `Blueprint compile result: true`, `Asset save result: true`로 확인한다. DryRun이나 종료 코드 0만으로 판단하지 않는다.
- Designer에서 부모 클래스·계층·변수·텍스트·색상·패딩·정렬을 확인한다.
- 바인딩/갱신 실패 시 Native 빌드, 이름/타입, 부모 클래스, AddMissing/Overwrite 옵션을 확인한다.
- T12 검증: 메뉴 명세 3종을 `UI/Validation/T12`로 복사 생성한다. `Text_Title` 변경과 `T12_MissingLabel`(TextBlock, parent=RootOverlay) 추가 후 AddMissing을 2회 실행해 기존 제목 보존·신규 위젯·바인딩을 확인한다. 테스트 필터 `ProjectA.Menu.AssetContracts`, 옵션 `-T12GeneratedAssets`.

## 7 기본 라운드 전투 UI

`UGameplayRootWidget`의 CombatLayer는 native `UCombatRoundPlanningWidget`을 쓴다. `UCombatHUDWidget`은 참조용 외형이다.

- 조작: **대상(또는 타일) 클릭 → 장착 스킬 버튼 → 준비 완료**. 스킬 버튼이 계획 적용 요청이다.
- 스킬 버튼은 AP/SAP 비용을 표시한다. `CanPlanCommand`가 거절하면 비활성화하고 사유를 보여준다.
- 스킬 없이 준비하면 비용 없는 턴 넘기기다. `스킬 선택 취소`는 이동 예약을 유지한다.
- 이동: **이동 예약 · SAP 1** → 강조된 아군 빈칸 클릭. `이동 예약 변경`·`이동 예약 취소`·`목적지 선택 닫기`.
- 준비·준비 취소 버튼은 패널 하단에 고정한다. 준비 완료는 저장 성공 뒤 표시한다.
- 파티 카드는 이름·조작 주체·HP/AP/SAP·계획·준비 상태를 표시하고 현재 조작 유닛을 금색 테두리로 구분한다. 초상화·확정 턴 순서는 넣지 않는다.
- **회복 소모품 · AP 1** 버튼은 스킬 목록과 분리하고 보유 수량을 표시한다.
- 화면·빈 루트는 `SelfHitTestInvisible`, 조작 패널은 hit-test를 유지한다. 패널 뒤 유닛·타일은 선택되지 않는다.
- `ACombatRoundPlayerController`는 최신 수정 번호 도착 전의 중복 요청과 이전 라운드 요청을 막는다.
- 저장 실패 시 Host의 **저장 다시 시도** 안내를 갱신한다.
- 타일 불투명도(`CombatGridTile|Presentation`): 빈칸 0.24, 점유 0.42, 이동 가능 0.65, 스킬 대상 0.78. hover +0.12(최대 0.9). 전열 보호 색은 25% 혼합.
- 패널 배치는 [10-2절](#10-2-전투-배치와-카메라).

### 7-1 개발용 유닛 HP 표시

- 코드 위치: `Source/ProjectA/UI/Debug/`.
- `UCombatUnitHealthDebugWidget`이 유닛 캡슐 상단 24cm 위에 현재/최대 HP와 체력바를 그린다. GAS AttributeSet 값을 읽고 마우스 입력은 통과시킨다. 아군 청록색, 적 주황색.
- Debug/Development에서 기본 활성화. 콘솔 `projecta.Debug.UnitHP 0`/`1`. Shipping/Test에서는 제외한다.
- 카메라 뒤·숨김·파괴·사망 유닛은 그리지 않는다.

### 7-2 전투 디버그 도구

- `UCombatDebugWidget`: DebugCombat 화면 왼쪽의 접을 수 있는 패널. 최대 920×760 UI 단위, 높이는 화면 높이 − 380으로 제한.
- 탭: **스킬 구입 / 장비 / 캐릭터·체력 / 스킬 타이밍**.
- 스킬 구입: **속성**(전체·물리·화염·냉기·번개·카오스·복합·미분류)과 **방식**(전체·투사체·범위형·체인·근접공격·지원형·미분류) 두 줄 탭 + 이름/에셋명 검색. 미보유 후보만 나열한다. 목록은 220 UI 단위 이상의 독립 스크롤이다.
  - 복합 속성은 각 속성과 복합 탭에 모두 표시한다. 치유·보호막은 지원형, `Skill.Shape.Chain`은 체인, 체인 태그 없는 빔은 범위형이다.
- 장비: 무기 1·2와 시범 방어구·망토의 획득+장착·제거. 계획 단계의 생존 아군만 편집하며 외형만 반영한다.
- 캐릭터·체력: 최대/현재 HP 입력(`1 ≤ 현재 HP ≤ 최대 HP ≤ 1,000,000`), 전부 회복, 직업·적 종류별 생성(총 8명), **아군 부활 창 열기**, **전투 초기화**.
- 스킬 타이밍: **임시 적용 / 원본 복원 / 설정값 복사**. 같은 스킬 ID의 모든 유닛에 적용하며 원본 DA·저장에는 쓰지 않는다.

| 항목 | 의미 |
|---|---|
| 시전 대기 `WindupSeconds` | 시전 시작부터 효과·발사·타격 발동까지(초) |
| 효과 피격 대기 `EffectHitDelaySeconds` | VFX 활성화 후 충돌·이동 시작까지(0~10초, 기본 0) |
| 효과 판정 기간 `EffectDuration` | 피격 대기 이후 충돌을 검사하는 기간(초) |
| 투사체 속도 `ProjectileSpeed` | 공통 배율(기본 0.5) 적용 전 작성 속도(cm/s) |
| 검 타격 기간 `WeaponTraceDuration` | 검 궤적을 검사하는 기간(초) |

- 규칙·수치는 [PROJECT_PLAN 4-2](PROJECT_PLAN.md#4-2-전투-디버그-레벨)를 따른다.

## 8 시작 메뉴 설정

MainMenu의 Options는 native `UOptionsWidget`이다. 화면은 `UGameUserSettings`, 언어는 엔진 Internationalization을 쓴다.

| 항목 | 표시·동작 |
|---|---|
| 언어 | 한국어(기본)·English |
| 화면 모드 | 전체 화면·테두리 없는 전체 화면·창 모드 |
| 해상도 | 엔진이 반환한 목록에서 중복 제거. 현재 해상도 보존 |
| 테두리 없는 전체 화면 | 게임 창이 있는 모니터의 바탕 화면 해상도로 고정. 해상도 선택 비활성화 |
| 그래픽 품질 | 낮음·중간·높음·최고·시네마틱(0~4). 사용자 지정 품질은 프리셋을 고르기 전까지 유지 |
| 수직 동기화 | 켜기·끄기 |

### 8-1 적용과 복원

- 항목 선택은 초안만 바꾼다. 적용 전에 닫으면 초안을 버린다.
- 언어·품질·VSync만 바꾸면 적용 시 바로 저장한다.
- 해상도·화면 모드가 바뀌면 `RequestResolutionChange`로 미리 적용하고 15초 동안 **유지 및 저장**·**이전 설정으로 복구**를 표시한다. 첫 포커스는 복구 버튼이다.
- 복구·시간 초과·뒤로 가기·닫기·위젯 제거·게임 종료 요청은 화면·품질·VSync를 모두 이전 값으로 되돌린다.
- 확인 중에는 `Alt+Enter`·`F11` 전체 화면 전환을 잠시 끈다.
- 확인 시간은 게임 시간 배율과 무관한 실제 시간이다.

### 8-2 Gameplay 인벤토리와 설정 단축키

**단축키**

- `I`: 전투·지도·결과에서 인벤토리 열기/닫기. 상점에서는 이미 표시된 가방으로 포커스만 옮긴다.
- `Esc`/`O`: 설정 열기/닫기. 인벤토리가 열려 있으면 설정으로 전환한다. 화면 변경 확인 중에는 먼저 이전 설정을 복구한다.
- `UGameplayRootWidget`의 `UtilityLayer`에 표시한다. 열린 동안 배경 UI와 전장 입력을 잠그지만 전투·협동 세션은 멈추지 않는다.

**인벤토리** (`UInventoryWidget` = `UCharacterEquipmentPanel` + `UCharacterInventoryPanel`)

- 가방 탭: **전체 / 무기 / 방패 / 탄약 / 기타 / 스킬**(아이콘·개수). 분류는 `Item.Weapon` 계층의 태그 조건을 쓴다.
- 미장착 사본마다 분류 아이콘·저장된 표시명·`(1)` 행을 만든다. 같은 에셋의 사본을 합치지 않고 `ItemIndex`와 장비 `Revision`을 드래그 요청에 유지한다.
- 행을 클릭하면 이름·분류·참고 가격·장착 지원·허용 슬롯을 표시한다. 상세 영역은 높이 220에서 스크롤한다.
- 아이템상점에서는 판매 금액과 판매/확정/취소 버튼을 상세 하단에 둔다. [판매 계약](PROJECT_PLAN.md#3-2-상점-인카운터)
- 스킬 탭은 같은 스킬을 한 번만 표시하고 장착 출처의 무기 이름·슬롯을 함께 보여준다.
- 사망 후에도 조회할 수 있다.

**장비창**

- 무기 2칸·투구·장갑·신발·갑옷·액세서리 각 1칸(3×3 격자, 신발 행의 좌우 칸은 비움). 양손 장비는 두 슬롯에 같은 사본으로 표시한다.
- 본인 생존 Human이 **상점 페이즈에서만** 드래그로 장착·해제·교체한다. 서버가 단계·권한·Revision·태그를 재검사한다.
- 판매 CSV의 50개는 모두 장착 프로필을 지원한다. 기존 보유 미지원 아이템은 장착 불가로 안내한다.

**표시 데이터**

- 등급 색·이름은 `RunItemPresentation`으로 저장된 값을 표시한다. 클라이언트에는 `FGameplayViewState.ItemRarities`로 전달한다.
- `SkillBalanceVersion=1` 사본은 `GrantedSkillBalances`에서 스킬 등급·위력·AP/SAP를 읽는다. UI가 CSV를 다시 읽거나 추첨하지 않는다.
- 스킬 탭·선택 상세·툴팁은 같은 `SkillDetails`를 쓴다.

### 8-3 개인 골드 상시 표시

- Gameplay 좌측 상단에 `보유 골드 10G` 형식으로 표시한다. 인벤토리와 같은 소유 캐릭터의 확정 잔액이다.
- 0G도 표시하고 캐릭터 미확정·Run 없음·협동 대기실에서는 숨긴다. 마우스 입력은 통과시킨다.
- 패널 아래에 `I` 인벤토리·`Esc/O` 설정 단축키를 표시한다.

### 8-4 시작 메뉴 백과사전

- native `UEncyclopediaWidget`을 MenuStack에 연다. 1180×760 기준 `DownOnly` 축소.
- 탭 7개: `장비`·`스킬`·`직업`·`상태이상`·`전투·공식`·`모험·상점`·`몬스터`. 선택한 탭의 이름/본문 검색, 좌측 목록, 우측 상세.
- 하위 탭:
  - 장비: `전체`·`검`·`단검`·`활`·`지팡이·완드`·`방패`·`기타·안내`
  - 스킬: `전체`·`근접·참격`·`투사체`·`범위`·`광선`·`연쇄`·`회복`·`보호막`·`기타·안내`
- 분류는 장비의 `Item.Weapon.*`, 스킬의 `EffectTags`와 GameplayTagQuery를 쓴다. 콘텐츠가 없는 종류는 숨긴다.
- `EncyclopediaCatalog`는 판매 카탈로그·장착 프로필, 스킬 정의 폴더, PartyDefinition의 4직업, 몬스터 CSV, 상태이상 8종으로 구성한다. 스킬은 현재 수치 CSV를 적용해 표시한다.
- AssetRegistry 검색 완료 후 비동기 로드하고 화면 종료 시 취소한다. Run 생성·저장·추첨은 하지 않는다.

## 9 DemonicUI 공통 테마

- 구현: [DemonicUITheme.h](../Source/ProjectA/UI/Theme/DemonicUITheme.h)·[DemonicUITheme.cpp](../Source/ProjectA/UI/Theme/DemonicUITheme.cpp). 원본 `/Game/DemonicUI`는 참조만 한다.
- 공통 스크롤바: 두께 10, 부드러운 휠 이동, 포커스 이동 시 자동 스크롤.

### 9-1 적용 구조

- `UDemonicUITheme`의 클래스 기본 객체가 `FObjectFinder`로 텍스처를 읽고 `UPROPERTY` 참조를 유지한다.
- `ApplyControls`가 WidgetTree의 버튼·콤보박스·체크박스·입력창·스크롤바·글자에 공통 외형을 적용한다. 콜백·바인딩은 각 화면이 관리한다.

| 요소 | 텍스처 |
|---|---|
| 긴 버튼 | `Button_A_Long_Ready/aimed/active` |
| 짧은 기호 버튼 | `Button_A_Lil_Ready/aimed/active` |
| 패널 | `GUI_Elements/pop_up_window_B` |
| 배경 | `Backgrounds/Background` |
| 구분 장식 | `GUI_Elements/Top_frame_m` |
| VSync 체크 | `Red_buttons/Button_tiny_ready`·`Button_tiny_ok` |
| 장비창 | `Widgets/WB_Equipment` 구성, `GUI_Elements/Icon_frame`·`bar_button_mid`·`equipment_male` |

- native 콤보박스는 `UDemonicComboBoxString`으로 만든다.
- 경고·직업 색상은 보존한다. 프리뷰 투명 영역과 전투 중앙에는 불투명 배경을 넣지 않는다.
- 상점·인벤토리·시작 메뉴·모드 선택·항복 확인·설정 패널은 공간이 부족할 때만 `ScaleBox`의 `DownOnly`로 축소한다.
- 기존 `WBP_RunMapWidget`·`WBP_EncounterResultWidget`은 알려진 `ContentBox`·`Overlay` 구조에서만 배경·프레임을 적용한다.

### 9-2 화면별 범위

| 화면 | 적용 범위 |
|---|---|
| MainMenu | 성 배경·제목·게임 시작/이어하기·관리 이어가기 패널. 이어하기 옆 104×40 항복 버튼 |
| 모드 선택·항복 확인 | 모드 선택은 MenuStack, 항복 확인은 ModalStack(돌아가기 기본 포커스) |
| Options | 설정 패널·목록·VSync·적용/닫기·15초 확인 패널 |
| 캐릭터 생성 | 슬롯 카드·이름 입력·직업 선택·상세·저장/취소/시작 |
| 개발용 협동 | 방 생성·주소 입력·참가·대기실·준비/시작/나가기 |
| RunMap | 경로 패널·노드 버튼·안내 |
| 상점 선택·상점 | 하단 3열 카드, NPC와 오른쪽 상품 패널 |
| 인벤토리 | 장비 슬롯·분류 탭·사본 목록·상세·스킬 탭 |
| 전투 결과 | 배경·승패 결과·Continue·메뉴 복귀 |
| 라운드 계획 | 상단 요약·우측 대상·좌하단 파티 카드·하단 스킬·우하단 이동/준비 |
| GameplayRoot 안내 | 저장 복구·저장 다시 시도, 협동 상태·나가기 |

### 9-3 반영과 검증

- C++ 파일 추가 후 프로젝트 파일 재생성 → Development Editor / Win64 빌드 → UE 재시작.
- 에셋 조사 산출물은 `Saved/Automation/DemonicUI`에 있다.

## 10 공통 DPI와 전투 화면 배치

### 10-1 공통 배율

- [DefaultEngine.ini](../Config/DefaultEngine.ini)의 `/Script/Engine.UserInterfaceSettings`: `UIScaleRule=ScaleToFit`, `DesignScreenSize=(X=1920,Y=1080)`, `ApplicationScale=1`.
- 배율 = `min(W / 1920, H / 1080)` (뷰포트 픽셀 기준). 화면별 추가 배율은 두지 않는다.

| 16:9 뷰포트 | 공통 배율 |
|---|---|
| 1280×720 | 2/3 |
| 1920×1080 | 1 |
| 2560×1440 | 4/3 |
| 3840×2160 | 2 |

### 10-2 전투 배치와 카메라

전장 중앙을 비우고 정보를 가장자리에 둔다(공통 DPI 적용 전 UI 단위).

| 영역 | 배치와 크기 |
|---|---|
| 라운드 요약 | 상단 중앙, 너비 620·최대 높이 104·상단 여백 16. 입력 통과 |
| 대상·적 현황 | 우측 상단, 너비 340·최대 높이 280·상단 여백 152 |
| 파티 카드 | 좌하단, 너비 620·최대 높이 280 |
| 선택 캐릭터·스킬 | 하단 중앙의 남은 너비·최대 높이 300 |
| 이동·준비 | 우하단, 너비 340·최대 높이 340 |

- 가장자리 여백 24, 패널 간격 16, 안쪽 여백 14×10. 각 패널은 독립 스크롤한다.
- 저장 재시도 안내는 상단 128 아래에 둔다.
- `CombatArena`의 `CameraComponent`: `bConstrainAspectRatio=false`, `AspectRatio_MaintainYFOV`. `ConfigureGameplayAssets.py`의 기본값도 같다.
- 숄더뷰: 본인 캐릭터의 접근·시전 시작 시 0.3초로 전환, 근접은 복귀 시작·원거리와 마법은 최초 피격(또는 불발 종료)에 0.45초 Cubic 블렌드로 복귀한다. 준비 직후·시작 지연 중에는 전술뷰를 유지한다. 구현은 [PROJECT_PLAN 4-11](PROJECT_PLAN.md#4-11-개발용-협동-진입).

### 10-3 반영과 검증

C++ 빌드와 Config 변경 후 UE를 재시작한다. 기존 WBP·맵은 그대로 쓴다.

## 11 네 직업 선택과 기본 능력치

- `FCharacterPartyDraft`가 4슬롯의 생성·삭제·직접 조작 선택을 관리한다.
- 라운드 UI는 `FCombatPlanningRefreshState`로 변화를 관찰하고 같은 상태에서는 갱신을 생략한다.
- 직업 순서: `Warrior` → `Mage` → `Archer` → `Rogue`. ClassInfo는 직업 정의의 시작 수치([GAME_STATUS 5절](GAME_STATUS.md#5-직업과-능력치))와 AP/SubAP·시작 스킬을 표시한다.
- 지원하지 않는 이전 직업의 Continue는 ID와 오류를 표시하고 자동 변환하지 않는다.

## 12 시작 모드 선택과 항복 확인

- CharacterCreation: 생성 카드의 `SlotActionBox` 행에 native `직접 조작` 버튼이 있다. 선택하면 `선택됨`, 다른 카드는 `AI`로 표시한다. 선택이 없으면 시작 버튼을 비활성화한다.
- `UGameModeSelectionWidget`: 첫 포커스는 싱글플레이. 멀티는 지원 조건에 맞을 때만 활성화하고 Steam 미지원 안내를 표시한다.
- 항복: 이어하기 옆 104×40 버튼 → `URunSurrenderWidget`. 돌아가기에 기본 포커스, 배경 클릭 차단, 중복 확인 차단. 저장이 바뀌면 창을 닫고 다시 확인해야 한다. [삭제 조건](PROJECT_PLAN.md#5-2-저장-버전과-호환)

## 13 전투 승리 보상

- `UEncounterResultWidget`: 버전 1 Target PvE는 아이템 카드 3개·등급·저장된 스킬·공통 골드·개인 잔액을 표시한다. 하나를 고르면 아이템 1개와 골드를 함께 받는다.
- 결과 뷰의 `VictoryRestHP`로 휴식 회복을 별도 표시한다.
- 제출 중에는 카드를 비활성화하고 반영 후 `획득 완료`를 표시한다. 본인 미수령 캐릭터가 여러 명이면 다음 캐릭터로 전환한다.
- 현재 Human이 모두 수령해야 Host의 Continue가 활성화된다. Snapshot·패배에는 보상 카드가 없다.
- 보상 스키마 1과 버전 0·prototype·개발 협동은 골드 카드 3개를 표시한다.
- 기존 `Text_Result`·`Button_Continue` 바인딩을 유지한다. 규칙은 [PROJECT_PLAN 3-2](PROJECT_PLAN.md#3-2-상점-인카운터).

## 14 로딩 화면

- `Async Loading Screen`의 Classic 레이아웃. DemonicUI 성 배경, 하단 밴드에 회전 표시·안내·팁.
- 문구: 시작 `PROJECT A · 시작 준비 중`, 맵 전환 `불러오는 중`.
- 원형 표시 반지름 22·8조각·주기 1.2초, 본문/팁 글자 26/20. 팁 6개 중 하나를 로딩마다 표시한다. 진행률·로딩바는 없다.
- 적용 대상: 최초 실행, Gameplay 이동, 메뉴 복귀, 협동의 ClientTravel/OpenLevel. 같은 Gameplay 내부의 전환과 PIE·Editor·commandlet·전용 서버에는 표시되지 않는다.
- 설정 [DefaultGame.ini](../Config/DefaultGame.ini), 설치·라이선스 [PROJECT_PLAN 1-1](PROJECT_PLAN.md#1-1-로딩-화면-플러그인).

## 15 언어와 번역 리소스

- 번역 갱신은 보류 중이다([현지화 보류](../AGENTS.md#현지화-보류)).
- 설정의 언어는 **한국어 / English**, 기본 한국어. **적용 및 저장**으로 즉시 반영하고 화면 변경과 함께면 **유지 및 저장** 시점에 적용한다.
- `Config/Localization`의 7개 JSON이 namespace·key·한국어 원문·영어 번역을 관리한다. 정적 문자열은 `NSLOCTEXT`, 데이터 문자열은 `ProjectALocalization`의 명시적 키를 쓴다.
- 번역이 없거나 원문 해시가 다르면 원문을 표시한다. 저장된 이름·수치·태그·ID와 사용자 캐릭터명은 번역하지 않는다.
- `python Source/ProjectAEditor/Scripts/BuildLocalization.py`로 ko/en 리소스를 만들고 `--check`로 누락·키 충돌·생성물 일치를 검사한다.

## 16 NPC 월드 표지판

- `/Game/User_JeHoon/UI/Gameplay/WBP_EncounterSign`의 TextBlock으로 표시한다. 기존 `TextRender`는 경로 호환용으로 숨긴 채 둔다.
- `UEncounterSignWidget`은 `Text_Title`에 인카운터의 `FText` 제목만 전달한다. 크기·색·정렬은 WBP에서 고친다.
- 초기 명세 [EncounterSignWidget.json](../Source/ProjectAEditor/UiScaffoldSpecs/EncounterSignWidget.json), `GenerateUiScaffold -Spec=Source/ProjectAEditor/UiScaffoldSpecs/EncounterSignWidget.json`. 명세의 `fontSize`는 생성 시에만 적용한다.
- 무대 템플릿의 `SignWidgetClass` 재정의는 통로의 NPC에도 전달한다. 에셋이 없으면 경고하고 TextRender로 대체하지 않는다.
