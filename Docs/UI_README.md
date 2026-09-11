# UI 구조·생성 도구

기준일: 2026-09-11. 화면 구성과 JSON 기반 Widget Blueprint 생성 규칙을 정의한다. 게임 흐름·에셋 연결은 [PROJECT_PLAN](PROJECT_PLAN.md), 실행 결과는 [TEST_REPORT](TEST_REPORT.md)를 따른다.

## 구성과 편집 원칙

| 구성 | 위치·역할 |
|---|---|
| 생성 도구 | [GenerateUiScaffoldCommandlet.cpp](../Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.cpp), Editor 전용 |
| 명세 | [UiScaffoldSpecs](../Source/ProjectAEditor/UiScaffoldSpecs): 메뉴 3종·Gameplay 4종 |
| Native UI | [Source/ProjectA/UI](../Source/ProjectA/UI), 화면 동작·선택적 fallback |
| WBP | `/Game/User_JeHoon/UI/MainMenu`, `/Game/User_JeHoon/UI/Gameplay` |
| 검증 사본 | `/Game/User_JeHoon/Validation/T12` |

Designer WBP를 화면 구조의 기준으로 유지한다. JSON은 초기 생성·바인딩 검증·누락 보완에 사용하며 결과 WidgetTree는 WBP에 저장된다. 이미지·브러시·폰트·세부 스타일은 Designer에서 편집한다. Editor API·의존성은 ProjectAEditor에 한정한다.

## 실행과 옵션

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
| CharacterCreation | 슬롯 생성 시 편집 패널·프리뷰 표시. 슬롯 X는 해당 캐릭터 제거. 화면 Back/X는 초안·프리뷰 정리 |
| PreviewStage | MainMenu의 월드 Actor·카메라 사용. SceneCapture2D·RenderTarget 미사용 |
| GameplayRoot | CommonUserWidget. RunLayer·CombatLayer·ModalLayer는 CommonActivatableWidgetStack |
| Gameplay 화면 | RunMap·CombatHUD·Result는 CommonActivatableWidget. 스폰은 EncounterManager가 담당 |
| 입력 | CombatHUD: All/CaptureDuringMouseDown, RunMap·Result: Menu/NoCapture. 커서 표시 유지, CommonUI가 입력 모드 관리 |

프리뷰 설정은 MainMenu에 PreviewStage 1개 배치 → PreviewActorClasses의 StableHand/Scholar/Herbalist/Hunter 연결 → PreviewCamera·Slot0~3Anchor 조정 순서다. 메뉴 전용 Actor를 사용하며 전투 입력·AI·충돌 로직은 제외한다. Stage·클래스 누락 시 경고를 기록하고 카드 UI는 유지한다.

GameplayPlayerController는 화면별 SetInputMode를 추가하지 않으며 메뉴 travel의 잔여 IgnoreInput·초기 포커스만 복구한다. CharacterCreation의 Text_StartGameStatus는 선택 바인딩이며 누락 시 Native 표시 영역을 추가한다. 상세 배치·바인딩은 [PROJECT_PLAN](PROJECT_PLAN.md#gameplay-에셋과-배치)을 따른다.

## 검증·문제 해결

- 생성 결과: `Blueprint compile result: true`, `Asset save result: true`와 오류 여부 확인. DryRun 또는 종료 코드 0만으로 생성 완료를 판단하지 않는다.
- Designer 확인: 부모 클래스·위젯 계층·변수·텍스트·색상·패딩·정렬 확인. Content Browser 미표시 시 경로·필터·에디터 재로드 확인.
- 바인딩/갱신 실패: Native 빌드·이름/타입·부모 클래스·AddMissing/Overwrite 옵션 확인.
- 작동 확인: 버튼 동작·CommonUI 화면 전환·프리뷰 교체. 실행은 [작업 규칙](../AGENTS.md#작동-테스트와-보고서), 결과는 [TEST_REPORT](TEST_REPORT.md)에 기록.
- T12 검증: 메뉴 명세 3종을 Validation/T12로 복사 생성한다. MainMenuScreenWidget의 Text_Title 변경과 T12_MissingLabel(TextBlock, parent=RootOverlay) 추가 후 AddMissing을 2회 실행하고 기존 제목 보존·신규 위젯·바인딩을 확인한다. 테스트 필터는 `ProjectA.Menu.AssetContracts`, 생성본 옵션은 `-T12GeneratedAssets`다.
