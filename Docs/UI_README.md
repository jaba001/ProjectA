# UI 구조·생성 도구

기준일: 2026-09-21. 화면 구성과 JSON 기반 Widget Blueprint 생성 규칙을 정의한다. 게임 흐름·에셋 연결은 [PROJECT_PLAN](PROJECT_PLAN.md), 실행 결과는 [HISTORY](HISTORY.md), 남은 확인은 [TODO](TODO.md)를 따른다.

## 구성과 편집 원칙

| 구성 | 위치·역할 |
|---|---|
| 생성 도구 | [GenerateUiScaffoldCommandlet.cpp](../Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.cpp), Editor 전용 |
| 명세 | [UiScaffoldSpecs](../Source/ProjectAEditor/UiScaffoldSpecs): 메뉴 3종·Gameplay 4종 |
| Native UI | [Source/ProjectA/UI](../Source/ProjectA/UI), 화면 동작·선택적 fallback |
| 공통 테마 | [DemonicUITheme](../Source/ProjectA/UI/Theme/DemonicUITheme.h), 기존 DemonicUI 텍스처와 컨트롤 외형 |
| WBP | `/Game/User_JeHoon/UI/MainMenu`, `/Game/User_JeHoon/UI/Gameplay` |
| 검증 사본 | `/Game/User_JeHoon/Validation/T12` |

기존 Designer WBP를 화면 구조의 기준으로 유지한다. 새 라운드 계획 화면은 7절의 native CommonUI 생성 경로를 사용한다. JSON은 초기 생성·바인딩 검증·누락 보완에 사용하며 결과 WidgetTree는 WBP에 저장된다. 9절의 공통 버튼·배경·글자색은 native 테마가 실행 시 적용하며 나머지 배치·폰트·세부 스타일은 Designer에서 편집한다. Editor API·의존성은 ProjectAEditor에 한정한다.

첫 화면의 게임 시작은 native `UGameModeSelectionWidget`을 연다. 싱글플레이는 기존 CharacterCreation, 멀티플레이는 `UDevelopmentCoopWidget`의 같은 PC·LAN 방 생성·주소 참가·대기실로 연결한다. 첫 화면의 별도 개발용 협동 버튼은 제거한다. Gameplay의 CommonUI 대기실·전투 중 상태/나가기 영역은 유지한다. 추가 확인은 [TODO](TODO.md#1-사용자-작동-확인)에 기록한다.

## 실행과 옵션

`URunEncounterWidget`은 기존 Gameplay RunLayer에서 선택 화면과 빈 상점을 표시한다. `EncounterChoice`에서 상점1·상점2·상점3 버튼을, `Shop`에서 선택한 상점 이름과 나가기 버튼을 사용한다. Host만 버튼을 활성화하고 Client는 같은 상태를 표시한다. CommonUI 메뉴 입력을 사용하며 기존 WBP 변경·새 영구 에셋 생성은 필요 없다. [남은 확인](TODO.md#1-사용자-작동-확인)에서 확인한다.

Development Editor / Win64 빌드 후 프로젝트 루트 PowerShell에서 실행한다. 빌드·프로젝트 파일 재생성 명령은 [README](../README.md#개발-환경)를 따른다.

```powershell
$editor = 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = Join-Path (Get-Location) 'ProjectA.uproject'
$spec = 'Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json'
& $editor $project -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun "-Spec=$spec"
```

| 옵션 | 동작 |
|---|---|
| `-Spec=...` | 명세 경로. 생략 시 MainMenuRootWidget.json |
| `-DryRun` | JSON 구조 검사만 수행. Native 클래스 로드·Blueprint 컴파일·저장 검증은 제외 |
| 변경 옵션 없음 | 신규 WBP 생성. 기존 WBP는 수정하지 않음 |
| `-AddMissing` | 기존 속성·계층 유지, 누락 위젯만 추가 후 컴파일·저장. 기존 위젯 타입 불일치 시 거절 |
| `-Overwrite` | 기존 WidgetTree를 JSON 기준으로 재생성. Native 생성 모드에서는 소스 재작성도 허용 |

실제 생성은 위 명령에서 `-DryRun`을 제거한다. 누락 보완은 `-AddMissing`, 전체 재생성은 `-Overwrite`를 지정한다. 두 옵션은 병용할 수 없다. 기존 WBP의 부모가 nativeClass와 다르면 자동 변경하지 않고 중단한다. Overwrite 전에는 원본을 보존하고 Designer 수정 내용을 확인한다.

## JSON 명세

최소 구조 예시:

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

실제 화면 생성에는 저장소의 전체 명세를 사용한다. 위 예시는 루트만 포함한다.

| 최상위 필드 | 규칙 |
|---|---|
| `nativeClass` | 필수. U 접두사의 Native 클래스명, Reflection 검색에서는 U 제외 |
| `nativeHeaderPath`, `nativeSourcePath` | 필수. 기존 클래스 재사용 모드에서도 지정 |
| `blueprintName` | 필수. 생성·갱신 WBP 이름 |
| `assetPath` | 필수. `/Game/User_JeHoon` 또는 그 하위 경로 |
| `parentClass` | 필수. UUserWidget / UCommonUserWidget / UCommonActivatableWidget |
| `generateNativeSource` | 생략 시 true. 현재 프로젝트 명세는 false로 기존 클래스 재사용 |
| `widgets` | 필수. 1개 이상, 루트는 정확히 1개 |

기존 클래스는 Editor 빌드에 포함된 UUserWidget 계열이어야 한다. Native 생성은 UUserWidget 부모·PROJECTA_API만 지원하며 bind=true 항목의 BindWidget 속성을 생성한다. NativeConstruct는 로그만 추가하고 레이아웃을 생성하지 않는다. 신규 클래스는 소스 생성 → 프로젝트 파일 재생성 → Editor 빌드 → 도구 재실행 순서로 WBP에 연결한다.

### 위젯 필드

| 필드 | 규칙·기본값 |
|---|---|
| `name`, `type` | 필수. 이름 중복 금지, 지원 타입만 허용 |
| `parent` | 부모 name. 루트는 빈 문자열 또는 생략 |
| `bind` | 기본 false. true면 Designer 변수 등록. Native 생성 시 BindWidget 속성 생성 |
| `text` | TextBlock 초기 문자열 |
| `horizontalAlignment` | Fill / Left / Center / Right, 기본 Center |
| `verticalAlignment` | Fill / Top / Center / Bottom, 기본 Center |
| `padding` | 숫자 4개: Left·Top·Right·Bottom. Overlay/HorizontalBox/VerticalBox/Button/SizeBox 슬롯 또는 Border 콘텐츠에 적용 |
| `sizeRule`, `fill` | HorizontalBox 자식: Auto(기본) / Fill. Fill 비율은 양수, 기본 1 |
| `color` | 숫자 4개: RGBA. TextBlock·Image의 ColorAndOpacity, Border의 BrushColor에 적용 |
| `widthOverride`, `heightOverride` | SizeBox 크기. 지정 시 양수 |

### 타입·계층·바인딩

| 자식 허용 | 지원 타입 |
|---|---|
| 여러 개 | CanvasPanel, Overlay, HorizontalBox, VerticalBox |
| 최대 1개 | Button, Border, SizeBox |
| 없음 | TextBlock, EditableTextBox, Image, CommonActivatableWidgetStack |

C++ 타입은 각 이름에 U 접두사를 붙인다. 부모 누락·순환 참조·복수 루트·자식 수 위반·미지원 타입/sizeRule·잘못된 숫자 배열·양수가 아닌 Fill/크기는 생성 전에 거절한다.

바인딩 대상은 JSON name과 C++의 BindWidget/BindWidgetOptional 이름·타입이 일치해야 한다. bind=true는 Designer 변수 등록이며 런타임 버튼 동작을 자동 구현하지 않는다.

## 화면·프리뷰

| 대상 | 동작 |
|---|---|
| MainMenuRoot | MainStack·MenuStack·ModalStack. MenuStack 활성 시 MainStack을 Hidden 처리하고 Back/X 또는 Clear 시 복원 |
| GameModeSelection | 게임 시작에서 MenuStack에 표시. 싱글은 CharacterCreation, 멀티는 기존 개발용 방. 캐릭터 생성·접속 전 멀티 화면 뒤로가기는 모드 선택, 모드 선택 뒤로가기는 첫 화면 복원 |
| CharacterCreation | 슬롯 생성 시 편집 패널·프리뷰 표시. 슬롯 X는 해당 캐릭터 제거. 화면 Back/X는 초안·프리뷰 정리 |
| PreviewStage | MainMenu의 월드 Actor·카메라 사용. SceneCapture2D·RenderTarget 미사용 |
| GameplayRoot | CommonUserWidget. RunLayer·CombatLayer·ModalLayer는 CommonActivatableWidgetStack |
| Gameplay 화면 | RunMap·RoundPlanning·Result는 CommonActivatableWidget. 스폰은 EncounterManager가 담당. 이전 CombatHUD WBP는 참조만 보존 |
| 입력 | RoundPlanning: All/CaptureDuringMouseDown. RunMap·Result: Menu/NoCapture. 커서 표시 유지, CommonUI가 입력 모드 관리 |

프리뷰 설정은 MainMenu에 PreviewStage 1개 배치 → PreviewActorClasses의 Warrior/Mage/Archer/Rogue 연결 → PreviewCamera·Slot0~3Anchor 조정 순서다. 메뉴 전용 Actor를 사용하며 전투 입력·AI·충돌 로직은 제외한다. Stage·클래스 누락 시 경고를 기록하고 카드 UI는 유지한다.

`BP_PartyMenuPreview`의 `SKM_Manny_Simple` 메시에는 같은 Skeleton의 `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`을 SingleNode 모드·속도 1·반복·자동재생으로 연결한다. `ConfigureMenuPreview.py`도 `OverrideAnimationData`로 같은 기본값을 저장한다. BP 컴파일·저장과 독립 재로드에서 재생 설정·기존 메시/배치/NoCollision 보존을 확인했다. 실제 화면의 반복 재생·위치 유지·삭제/재진입 정리는 [남은 확인](TODO.md#2-6-캐릭터-생성-프리뷰)에서 확인한다.

GameplayPlayerController는 화면별 SetInputMode를 추가하지 않으며 메뉴 travel의 잔여 IgnoreInput·초기 포커스만 복구한다. CharacterCreation의 Text_StartGameStatus는 선택 바인딩이며 누락 시 Native 표시 영역을 추가한다. 상세 배치·바인딩은 [PROJECT_PLAN](PROJECT_PLAN.md#gameplay-에셋과-배치)을 따른다.

## 검증·문제 해결

- 생성 결과: `Blueprint compile result: true`, `Asset save result: true`와 오류 여부 확인. DryRun 또는 종료 코드 0만으로 생성 완료를 판단하지 않는다.
- Designer 확인: 부모 클래스·위젯 계층·변수·텍스트·색상·패딩·정렬 확인. Content Browser 미표시 시 경로·필터·에디터 재로드 확인.
- 바인딩/갱신 실패: Native 빌드·이름/타입·부모 클래스·AddMissing/Overwrite 옵션 확인.
- 작동 확인: 버튼 동작·CommonUI 화면 전환·프리뷰 교체. 실행은 [작업 규칙](../AGENTS.md#작동-테스트와-보고서)을 따르며 결과는 [HISTORY](HISTORY.md), 남은 확인은 [TODO](TODO.md#1-사용자-작동-확인)에 기록.
- T12 검증: 메뉴 명세 3종을 Validation/T12로 복사 생성한다. MainMenuScreenWidget의 Text_Title 변경과 T12_MissingLabel(TextBlock, parent=RootOverlay) 추가 후 AddMissing을 2회 실행하고 기존 제목 보존·신규 위젯·바인딩을 확인한다. 테스트 필터는 `ProjectA.Menu.AssetContracts`, 생성본 옵션은 `-T12GeneratedAssets`다.

## 7 기본 라운드 전투 UI

`UGameplayRootWidget`의 CombatLayer는 `UCombatRoundPlanningWidget`을 사용한다. 기존 `CombatHUDWidgetClass`와 `UCombatHUDWidget`은 Blueprint 참조를 위한 외형이며 순차 명령 버튼을 실행하지 않는다. 새 화면은 native 생성으로 동작하므로 WBP/JSON 재생성·기존 맵 재배치·Config 변경이 필요 없다. C++ 빌드 후 UE 재시작으로 리플렉션 변경을 반영한다.

전장에서 **적 또는 스킬이 요구하는 타일 한 번 클릭 → 실제 장착 스킬 버튼 클릭 → 준비 완료** 순서로 행동한다. 스킬 버튼을 누르면 자원·대상·좌표를 검사하고 바로 계획 적용을 요청한다. 하단에는 현재 조작 캐릭터의 HP/AP/SAP·속도, 선택 대상과 적용된 행동을 표시한다. 인간 조작 가능한 캐릭터가 여럿인 기존 파티는 아군 클릭 또는 조작 캐릭터 목록으로 전환한다. 상단에는 아군/적 HP·계획·준비 상태를 표시한다.

스킬 버튼은 선택한 캐릭터에게 실제 장착된 DA만 사용한다. 전사는 비무장 공격·원거리 공격·AOE·휩쓸기·검 공격 5개, 나머지 직업은 기존 4개다. 비무장 공격은 기존 기본공격의 표시명만 변경한 것이다. 무장착 자동 공격과 Encounter 진입 시 추가 스킬 추첨은 사용하지 않는다. `CanPlanCommand`가 거절하는 버튼은 비활성화하고 사유를 제공한다. 인간 조작이 허용된 모든 생존 유닛의 적용된 계획이 유효해야 준비 완료할 수 있다.

**이동 예약 · SAP 1**을 누른 뒤 강조된 아군 빈칸을 한 번 클릭하면 목적지를 예약한다. 계획 중에는 위치·자원 소모가 없으며 예약 좌표를 표시한다. `이동 예약 변경`으로 목적지를 바꾸고 `이동 예약 취소`로 제거한다. `목적지 선택 닫기`는 클릭 선택 모드만 닫고 기존 예약은 유지한다. 전원 준비 후 SAP 이동을 먼저 끝내고 AP 공격을 실행하며 도착 위치를 새 복귀 칸으로 사용한다. 상단에 SAP 이동 실행/AP 행동 실행 단계를 구분한다. 실행·서버 응답 대기 중 입력 잠금, 실패·중단 시 출발점 복원과 잠금 이후 자원 미환불은 유지한다.

입력은 `All / CaptureDuringMouseDown`을 사용하고 화면/빈 루트는 `SelfHitTestInvisible`, 조작 패널은 hit-test를 유지한다. Controller는 실제 뷰포트의 클릭만 전달하므로 패널 뒤 유닛·타일을 함께 선택하지 않는다. 현재 전투에 등록된 유닛과 Grid만 허용하며 이전 순차 타일 행동 API는 재활성화하지 않는다.

`ACombatRoundPlayerController`는 소유 연결의 계획/준비/이동 RPC와 서버 응답을 관리한다. 최신 복제 수정 번호 도착 전 중복 요청을 막으며 이전 라운드 요청은 거절한다. 계획·이동 예약 수정은 해당 소유자의 준비만 해제하고 다른 팀원의 준비를 유지한다. 준비 완료는 복구 상태 저장 성공 뒤 표시하며 실패 시 기존 상태와 오류를 유지하고 전투를 시작하지 않는다. 다른 유닛의 복귀·예약 칸과 자리 교환은 거절한다. 확정 규칙은 [기획 8절](GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투), 최신 사용자 확인은 [TODO](TODO.md#2-14-확정-전투-규칙과-준비-완료-복구), 이전 실행 이력은 [HISTORY](HISTORY.md)를 따른다.

자동 잠금 과정의 저장 실패도 상태 변경을 통지하여 Host의 **저장 다시 시도** 안내를 갱신한다. 재시도 성공 전 전투 시작과 비용 차감을 확정하지 않는다.

### 7-1 개발용 유닛 HP 표시

`UCombatUnitHealthDebugWidget`은 라운드 UI의 배경 레이어에서 전투에 참여한 아군·적의 캡슐 상단 24cm 위에 현재/최대 HP 숫자와 작은 체력바를 표시한다. 그리기 갱신마다 GAS AttributeSet의 실제 HP/MaxHP를 읽어 최대 소수 한 자리로 표시하고 유닛 이동·카메라·공통 DPI를 따라 위치를 갱신한다. 어두운 배경과 아군 청록색·적 주황색을 사용하며 마우스 입력을 통과시킨다.

Debug/Development에서 기본 활성화하며 콘솔 `projecta.Debug.UnitHP 0`으로 숨기고 `projecta.Debug.UnitHP 1`로 표시한다. Shipping/Test에서는 표시 생성과 명령 등록을 제외한다. 카메라 뒤·숨김·파괴 유닛은 제외하고 사망 후 Actor가 남아 있으면 HP 0을 표시한다. 구현·컴파일·실제 화면 확인 상태는 [TODO](TODO.md#2-18-개발용-유닛-hp-표시)를 따른다.

## 8 시작 메뉴 설정

MainMenu의 Options는 native `UOptionsWidget`으로 화면·그래픽 설정을 표시한다. 기존 메뉴의 CommonUI 입력·뒤로 가기·포커스를 사용하며 새 WBP·JSON·맵·Config 변경은 필요 없다. 적용과 저장은 Unreal `UGameUserSettings`를 사용한다.

| 항목 | 표시·동작 |
|---|---|
| 화면 모드 | 전체 화면·테두리 없는 전체 화면·창 모드 |
| 해상도 | 전체 화면은 엔진이 반환한 지원 목록, 창 모드는 엔진의 창 크기 목록을 사용한다. 중복을 제거하고 현재의 유효한 해상도도 보존한다 |
| 테두리 없는 전체 화면 | 게임 창이 위치한 모니터 또는 가장 가까운 모니터의 바탕 화면 해상도로 고정한다. 해상도 선택을 비활성화하고 사유를 표시한다 |
| 그래픽 품질 | 낮음·중간·높음·최고·시네마틱의 5단계(0~4). Landscape·렌더 배율을 포함한 전체 세부 품질을 비교하고 프리셋과 다른 사용자 설정은 프리셋을 고르기 전까지 유지한다 |
| 수직 동기화 | VSync 켜기·끄기 |

### 8-1 적용과 복원

- 화면을 열면 현재 설정을 읽는다. 항목 선택은 초안만 변경하며 적용 전 닫기는 초안을 버린다.
- 품질·VSync만 변경하면 적용 시 바로 반영·저장한다. 기존 사용자 품질은 프리셋을 명시적으로 선택한 경우에만 일괄 변경한다.
- 해상도·화면 모드가 바뀌면 `RequestResolutionChange`로 전체 변경을 저장하지 않고 미리 적용한다. 확인 전에는 `Alt+Enter` 전환용 기본 전체 화면 모드도 변경하지 않는다. 15초 동안 **유지 및 저장**·**이전 설정으로 복구**와 남은 시간을 표시하고 설정 편집을 잠근다.
- 제한 시간 안에 **유지 및 저장**을 선택하면 현재 해상도·화면 모드·품질·VSync를 확정·저장한다.
- 복구·시간 초과·뒤로 가기·닫기·위젯 비활성화/제거는 미리 적용한 화면·세부 품질·VSync를 모두 이전 값으로 복원한다. 정상 게임 창 종료 요청도 종료 전에 복원한다. 복원값을 다시 저장하여 미확인 변경이 다음 실행에 남지 않게 한다.
- 확인 중 뒤로 가기는 복원 후 설정 화면으로 돌아오며 다시 뒤로 가기를 누르면 메뉴로 나간다. 첫 확인 포커스는 **이전 설정으로 복구**에 둔다.
- 확인 중에는 미확인 설정을 저장할 수 있는 엔진의 `Alt+Enter`·`F11` 전체 화면 전환을 잠시 비활성화한다. 확정·복원 후 두 단축키의 이전 활성 상태를 복구한다.
- 확인 시간은 게임 시간 배율과 무관한 실제 경과 시간으로 계산한다. 확인·복원 후 선택값과 포커스를 현재 설정에 맞춰 갱신한다.

해상도와 전체 화면 전환은 사용자가 독립 게임 창에서 확인한다. UI 전환·화면 복원·저장의 추가 확인은 [남은 확인](TODO.md#1-사용자-작동-확인)을 따른다.

## 9 DemonicUI 공통 테마

기존 DemonicUI의 암갈색 패널·청동 장식·붉은 활성 버튼·청회색 성 배경을 현재 사용 중인 화면에 적용한다. 기준 구현은 [DemonicUITheme.h](../Source/ProjectA/UI/Theme/DemonicUITheme.h)·[DemonicUITheme.cpp](../Source/ProjectA/UI/Theme/DemonicUITheme.cpp)다. 원본 `/Game/DemonicUI` 에셋은 참조만 하며 수정·이동·복제하지 않는다. WBP·JSON·맵·Config 재생성은 필요하지 않다.

### 9-1 적용 구조

- `UDemonicUITheme`의 클래스 기본 객체가 `FObjectFinder`로 텍스처를 읽고 `UPROPERTY` 참조를 유지한다. 브러시가 사용하는 텍스처의 가비지 수집 방지와 쿠킹 의존성 연결을 위한 구조이며 패키지에서의 실제 표시 확인은 별도 수행한다.
- `ApplyControls`가 기존 WidgetTree의 버튼·콤보박스·체크박스·입력창·스크롤바·일반 글자에 공통 외형을 적용한다. 화면별 배경·패널·제목·주요 버튼은 명시적으로 지정한다. 콜백·바인딩·활성화 조건은 기존 화면이 관리한다.
- 긴 버튼은 `Button_A_Long_Ready/aimed/active`, 짧은 기호 버튼은 `Button_A_Lil_Ready/aimed/active`를 사용한다. 일반·가리킴·누름·비활성 외형을 구분하고 주요 실행 버튼은 붉은 배경을 사용한다.
- 패널은 `GUI_Elements/pop_up_window_B`, 배경은 `Backgrounds/Background`, 구분 장식은 `GUI_Elements/Top_frame_m`을 사용한다. VSync 체크는 `Red_buttons/Button_tiny_ready`·`Button_tiny_ok`로 표시한다.
- native 콤보박스는 `UDemonicComboBoxString`으로 생성하여 Slate가 선택값과 펼침 목록을 만들기 전에 밝은 글자색을 설정한다. 목록 행·선택·가리킴·스크롤바도 공통 색상으로 표시한다.
- 명시적인 경고·직업 색상은 보존한다. 장식 이미지는 입력을 받지 않으며 프리뷰용 투명 영역과 전투 중앙의 월드 입력 영역에는 불투명 전체 배경을 추가하지 않는다.
- 중앙 패널은 [10절](#10-공통-dpi와-전투-화면-배치)의 프로젝트 공통 DPI를 사용한다. MainMenu의 메뉴와 명시적인 관리 이어가기 패널은 같은 가로 배치를 유지하며 화면별 `ScaleBox` 축소를 중복 적용하지 않는다. 기존 관리 이어가기 표시 조건은 유지한다.
- 기존 `WBP_RunMapWidget`·`WBP_EncounterResultWidget`은 알려진 `ContentBox`·`Overlay` 구조에서 현재 컨트롤을 유지한 채 배경·중앙 프레임을 적용한다. 다른 Designer 계층은 재작성하지 않으며 컨트롤 스타일만 적용한다.

### 9-2 화면별 범위

| 화면 | 적용 범위·보존 계약 |
|---|---|
| MainMenu | 성 배경·제목·게임 시작/이어하기·관리 이어가기 패널. 두 패널 동시 표시 시 나란히 배치하고 공통 DPI 적용. 이어하기 옆 104×40 항복 버튼과 저장 상태별 활성화 |
| 모드 선택·항복 확인 | 공통 테마의 native 화면. 모드 선택은 MenuStack, 항복 확인은 ModalStack. 항복은 돌아가기 기본 포커스와 배경 클릭 차단 적용 |
| Options | 설정 패널·해상도/화면 모드/품질 목록·VSync·적용/닫기·15초 확인 패널. [8절](#8-시작-메뉴-설정)의 저장·복원·포커스 계약 유지 |
| 캐릭터 생성 | 슬롯 카드·이름 입력·직업 선택·직업 상세·저장/취소/시작 버튼. 월드 프리뷰 표시와 투명 차단 영역 유지 |
| 개발용 협동 | 방 생성·주소 입력·참가·대기실·준비/시작/나가기. Host와 원래 소유권의 버튼 활성화 조건 유지 |
| RunMap | Designer/native 경로의 배경·경로 패널·노드 버튼·안내. 동적 `UGameplayActionButton::Configure`에서도 테마 적용 |
| 상점 선택·상점 | 선택 목록·상점 이름·나가기·안내. Host 진행 권한 유지 |
| 전투 결과 | Designer/native 경로의 배경·승리/패배 결과·Continue·메뉴 복귀. 결과별 기존 진행 조건 유지 |
| 라운드 계획 | 상단 아군/적 현황·하단 대상/장착 스킬 버튼·SAP 이동 예약/취소·준비/취소. 전장 타일/유닛 클릭과 해결·이동 중 입력 잠금 |
| GameplayRoot 안내 | 저장 복구 안내·저장 다시 시도, 게임 중 협동 상태·나가기. 기존 표시 조건·저장 재시도·퇴장 동작과 안내 바깥 클릭 통과 유지 |

### 9-3 반영과 검증

C++ 파일 추가에 따른 프로젝트 파일 재생성·Development Editor / Win64 빌드 후 UE를 재시작한다. 명령은 [README](../README.md#개발-환경), 현재 검증 상태와 사용자 절차는 [남은 확인](TODO.md#1-사용자-작동-확인)을 따른다. 이전 설정·전투 실행 결과를 새 테마의 가독성·입력·쿠킹 성공 근거로 사용하지 않는다.

에셋 선정 시 패키지 내부 썸네일과 직렬화된 `SizeX`·`SizeY`를 정적으로 조사했다. Unreal의 PNG 썸네일 저장 방식에 맞춰 조사 이미지의 R/B 채널을 보정했으며 게임 텍스처는 변환하지 않았다. 조사 산출물은 `Saved/Automation/DemonicUI`에 있고 실제 UI 실행 화면은 포함하지 않는다.

## 10 공통 DPI와 전투 화면 배치

### 10-1 공통 배율

프로젝트 전체 UI는 [DefaultEngine.ini](../Config/DefaultEngine.ini)의 `/Script/Engine.UserInterfaceSettings`에서 `UIScaleRule=ScaleToFit`, `DesignScreenSize=(X=1920,Y=1080)`, `ApplicationScale=1`을 사용한다. DPI는 뷰포트 크기에 따라 UI의 글자·버튼·간격을 함께 조절하는 배율이다. 가로·세로를 다른 비율로 늘리지 않으며 기기 종류나 특정 해상도에 따른 별도 배치 규칙은 두지 않는다.

뷰포트 너비를 `W`, 높이를 `H`라고 할 때 공통 배율은 `min(W / 1920, H / 1080)`이다. 뷰포트 픽셀 기준이며 모니터의 물리적 크기와는 구분한다.

| 16:9 뷰포트 | 공통 배율 | 1920×1080 대비 UI 크기 |
|---|---|---|
| 1280×720 | 2/3 | 약 66.7% |
| 1920×1080 | 1 | 100% |
| 2560×1440 | 4/3 | 약 133.3% |
| 3840×2160 | 2 | 200% |

메뉴·설정·개발용 협동·RunMap·상점·결과의 개별 `ScaleBox`·`DownOnly` 축소는 제거한다. 모든 화면에 공통 DPI를 한 번 적용하고 각 화면의 정렬·스크롤·바인딩을 유지한다. 화면 비율이 달라도 동일한 배율 공식을 사용한다.

### 10-2 전투 배치와 카메라

라운드 계획은 상단 중앙 현황과 하단 중앙 조작 패널을 사용한다. 상단은 가로 900·내용에 맞춘 높이이며 어두운 반투명 배경·둥근 모서리·얇은 청동색 테두리와 안쪽 여백 14×8을 적용한다. 상단 패널은 마우스 입력을 통과시킨다. 하단은 가로 900·최대 높이 300 UI 단위이며 두 패널의 바깥 여백은 12다. 하단 내용은 스크롤하고 가운데 월드 영역에서 유닛·타일을 선택한다. 이전 좌우 목록 패널은 사용하지 않는다.

`CombatArena`가 활성화하는 실제 `CameraComponent`는 `bConstrainAspectRatio=false`를 적용하여 고정 16:9 비율의 검은 띠를 제거한다. 세로 시야각을 유지하는 `AspectRatio_MaintainYFOV`를 명시하여 넓은 화면에서는 좌우 시야를 확장한다. `ConfigureGameplayAssets.py`의 생성 기본값도 같은 설정을 사용한다. 기존 제작 카메라 에셋을 다시 생성하거나 이동하지 않아도 런타임에 적용하며 실제 시야·프레이밍은 화면 비율별로 사용자가 확인한다.

### 10-3 반영과 검증

최신 C++ 빌드와 Config 변경을 반영한 뒤 UE를 재시작한다. 기존 WBP·맵을 유지한다. 화면 비율·창 크기·전투 클릭·설정 복원의 추가 확인은 [남은 확인](TODO.md#1-사용자-작동-확인)을 따른다. 9절 테마 변경 당시의 빌드 결과는 이번 배율·카메라 변경의 성공 근거로 사용하지 않는다.

## 11 네 직업 선택과 기본 능력치

캐릭터 생성의 선택 순서는 전사 `Warrior` → 마법사 `Mage` → 궁수 `Archer` → 도적 `Rogue`다. 이전 테스트 직업은 선택 목록에서 제거한다. 이름 편집·취소·1~4명 생성과 원래 소유권은 유지한다. ClassInfo는 직업 정의의 HP 100·힘/민첩/지능 각 10과 기존 전투 클래스에서 해석한 AP/SubAP·시작 스킬을 표시한다. 이 수치는 현재 시작값이며 최종 직업 밸런스가 아니다.

표시 정보의 기준은 `UProfessionBase`와 네 C++ 자식 클래스다. 기본 UI·캐릭터 생성 명세를 갱신하고 실사용 및 `Validation/T12` WBP의 슬롯 제목/직업 이름 16개만 변경한다. 기존 WidgetTree·배치·스타일·바인딩은 유지한다. MainMenu 프리뷰 맵은 새 ID 4개에 기존 공통 `BP_PartyMenuPreview` 외형을 연결하며 직업별 새 외형을 생성하지 않는다. 개발용 협동은 참가자마다 궁수 한 명을 생성한다.

지원하지 않는 이전 직업의 Continue는 해당 ID와 오류를 표시하고 저장 원본·현재 Run을 유지한다. 새 직업으로 자동 변환하지 않는다. Editor 빌드와 저장 후 에셋 재로드에서 새 직업 맵·16개 라벨을 확인했으며 실제 클릭·상세 표시·전투·Continue·협동은 [남은 확인](TODO.md#3-2-아이템과-직업)의 사용자 검증을 따른다.

## 12 시작 모드 선택과 항복 확인

일반 싱글 새 게임의 CharacterCreation은 첫 진입에 직접 조작 선택이 없으며 안내를 표시하고 시작 버튼을 비활성화한다. 생성 카드의 Edit/X 행에 네이티브 `직접 조작` 버튼을 추가한다. 한 명을 선택하면 해당 버튼은 `선택됨`, 직업명 옆은 `직접 조작`으로 표시하며 다른 생성 카드는 `AI`로 표시한다. 선택 카드 삭제 시 선택을 해제하고 시작을 다시 막는다. 직업·이름 편집은 선택을 유지하고 화면을 나갔다가 다시 들어오면 초안·선택을 초기화한다.

Designer와 native 경로 모두 기존 `SlotActionBox` 행을 사용하므로 WBP·생성 명세 재생성은 필요하지 않다. 선택 저장·AI 전투와 기존 저장 호환은 [PROJECT_PLAN](PROJECT_PLAN.md#저장과-멀티플레이-연결-경계), 실제 클릭·프리뷰·계획·사망 후 진행 확인은 [남은 확인](TODO.md#2-9-싱글플레이-직접-조작-캐릭터-선택)을 따른다.

게임 시작은 `UGameModeSelectionWidget`의 싱글플레이/멀티플레이 선택을 거친다. 첫 포커스는 싱글플레이이며 멀티는 기존 지원 조건에 맞을 때만 활성화한다. 같은 PC·LAN의 2~4인 개발용 방과 Steam 미지원 안내를 표시한다. 모드 선택·하위 화면 전환 중 첫 화면이 중복 노출되지 않아야 하며 뒤로가기 후 다시 진입할 수 있어야 한다. 멀티 접속을 시작한 뒤 취소/나가기는 기존 세션 정리·MainMenu 복귀를 따른다.

이어하기 옆 항복 버튼은 104×40 UI 단위이며 공통 DPI를 따른다. `URunSurrenderWidget`은 현재 싱글 여정을 포기하면 이어갈 수 없음을 알리고, 돌아가기에 기본 포커스를 둔다. 취소·CommonUI 뒤로가기는 저장을 변경하지 않는다. 모달 배경은 아래 메뉴의 클릭을 막고 제출 중 중복 확인을 차단한다. 오류는 확인창에 표시하며 저장이 바뀐 경우 창을 닫고 최신 대상으로 다시 확인해야 한다. 삭제 성공 후에는 창을 닫고 이어하기/항복을 비활성화한다. 저장 범위와 기본안의 근거는 [PROJECT_PLAN](PROJECT_PLAN.md#저장과-멀티플레이-연결-경계), 검증은 [남은 확인](TODO.md#2-7-시작-모드-선택과-싱글-여정-항복)을 따른다.
