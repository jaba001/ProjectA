ProjectA UI Scaffold Generator 사용설명서

1. 개요

UI Scaffold Generator는 JSON 명세를 읽어서 Widget Blueprint의 Designer tree를 자동으로 구성하는 에디터 전용 시스템이다.

이 시스템으로 생성된 Widget Blueprint는 Unreal Editor의 Widget Blueprint Designer에서 계층을 확인하고 직접 편집하거나 저장할 수 있다.

런타임에 NativeConstruct 또는 NativeOnInitialized에서 WidgetTree를 생성하는 방식과 다르게, 생성 결과가 WBP asset 내부에 저장된다.

주요 기능:

- JSON 기반 Widget Blueprint Designer tree 생성
- 기존 native C++ widget class를 WBP parent class로 사용
- bind=true widget을 Designer variable로 등록
- TextBlock 텍스트, 색상, 패딩, 정렬 같은 기본 속성 적용
- HorizontalBox child의 Auto/Fill 크기 규칙 적용
- CharacterCreation 슬롯의 빈 상태와 편집 패널을 Designer tree로 구성
- Widget Blueprint compile 및 package 저장
- DryRun 검증
- 명시적인 Overwrite 제어
- 선택적인 native C++ UUserWidget source 생성


2. 주요 구성 파일

Commandlet:

- Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.h
- Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.cpp

JSON spec 폴더:

- Source/ProjectAEditor/UiScaffoldSpecs

현재 ProjectA에서 사용하는 spec:

- MainMenuRootWidget.json
- MainMenuScreenWidget.json
- CharacterCreationWidget.json

UI native class와 WBP:

- Source/ProjectA/UI
- Content/User_JeHoon/UI/MainMenu


3. 기본 사용 흐름

1. 생성하거나 갱신할 WBP의 native parent class를 준비한다.
2. UiScaffoldSpecs 폴더에 JSON spec을 작성한다.
3. -DryRun으로 JSON 구조와 widget 계층을 검증한다.
4. 기존 WBP를 수정할 때는 원본 .uasset을 백업한다.
5. -Overwrite 옵션으로 WBP Designer tree를 생성한다.
6. 로그에서 Blueprint compile과 asset save 성공 여부를 확인한다.
7. Unreal Editor를 재시작하고 WBP Designer tree를 확인한다.


4. Commandlet 실행 방법

기본 실행 파일:

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

프로젝트 파일:

"C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject"

JSON 검증만 실행:

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"

신규 asset 생성:

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"

기존 asset 갱신:

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Overwrite -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScreenWidget.json"


5. Commandlet 옵션

-Spec="경로"

사용할 JSON spec 파일을 지정한다.
경로를 생략하면 MainMenuRootWidget.json을 기본 spec으로 사용한다.

-DryRun

JSON을 읽고 모든 검증을 수행하지만 C++ 파일과 WBP asset을 생성하거나 저장하지 않는다.
새 spec을 작성한 뒤에는 항상 DryRun을 먼저 실행한다.

-Overwrite

기존 source 또는 WBP asset을 다시 생성할 수 있게 한다.
옵션이 없으면 같은 이름의 WBP asset을 수정하지 않는다.

기존 WBP parent class가 JSON의 nativeClass와 다르면 Overwrite를 중단한다.


6. JSON top-level 구조

예시:

{
    "nativeClass": "UMainMenuScreenWidget",
    "nativeHeaderPath": "Source/ProjectA/UI/MainMenu/MainMenuScreenWidget.h",
    "nativeSourcePath": "Source/ProjectA/UI/MainMenu/MainMenuScreenWidget.cpp",
    "blueprintName": "WBP_MainMenuScreenWidget",
    "assetPath": "/Game/User_JeHoon/UI/MainMenu",
    "parentClass": "UCommonActivatableWidget",
    "generateNativeSource": false,
    "widgets": []
}

필드 설명:

nativeClass

- WBP parent로 사용할 native C++ class 이름
- 반드시 U로 시작해야 한다.
- Unreal Reflection에서는 앞의 U를 제외한 이름으로 검색한다.

nativeHeaderPath

- native header 경로
- generateNativeSource=false인 기존 class 재사용 모드에서도 현재 구현상 필수 필드다.

nativeSourcePath

- native cpp 경로
- generateNativeSource=false인 기존 class 재사용 모드에서도 현재 구현상 필수 필드다.

blueprintName

- 생성하거나 갱신할 Widget Blueprint asset 이름
- 비어 있을 수 없다.

assetPath

- WBP를 저장할 Unreal asset 폴더
- 반드시 /Game/으로 시작해야 한다.

parentClass

- nativeClass의 부모 class를 설명한다.
- 지원 값은 UUserWidget, UCommonUserWidget, UCommonActivatableWidget이다.

generateNativeSource

- true: JSON을 기준으로 새로운 native .h/.cpp 파일 생성
- false: 이미 존재하는 native class 재사용
- 생략하면 true가 기본값이다.

widgets

- Designer tree에 생성할 widget 목록
- 최소 한 개 이상의 widget이 필요하다.


7. 기존 native class 재사용

기존 C++ widget class에 Designer tree만 추가할 때는 generateNativeSource=false를 사용한다.

이 모드에서는 다음 조건이 필요하다.

- nativeClass가 현재 Editor binary에 컴파일되어 있어야 한다.
- nativeClass가 UUserWidget 계열이어야 한다.
- 기존 WBP를 갱신한다면 WBP parent class가 nativeClass와 일치해야 한다.
- JSON의 bind=true widget 이름과 C++ BindWidget 또는 BindWidgetOptional property 이름이 정확히 같아야 한다.

ProjectA의 MainMenu WBP들은 이 모드를 사용한다.


8. 새로운 native C++ class 생성

새 class를 생성할 때는 generateNativeSource=true를 사용한다.

현재 source generator의 제한:

- UUserWidget parent만 지원한다.
- PROJECTA_API export macro를 사용한다.
- BindWidget property는 bind=true widget만 생성한다.
- NativeConstruct에는 생성 로그만 추가한다.
- NativeConstruct에서 UI layout을 생성하지 않는다.

새 class를 처음 생성할 때는 한 번의 commandlet 실행만으로 WBP까지 만들 수 없을 수 있다.

권장 순서:

1. commandlet로 .h/.cpp 생성
2. Visual Studio 프로젝트 파일 재생성
3. ProjectAEditor Development Editor 빌드
4. commandlet 재실행
5. 컴파일된 native class를 parent로 사용하는 WBP 생성


9. Widget 항목 구조

예시:

{
    "name": "Button_NewGame",
    "type": "Button",
    "parent": "MenuBox",
    "bind": true,
    "horizontalAlignment": "Fill",
    "verticalAlignment": "Center",
    "padding": [0.0, 4.0, 0.0, 4.0]
}

필드 설명:

name

- Designer tree에서 사용할 widget 이름
- 같은 spec 안에서 중복될 수 없다.
- C++ binding 대상이면 native property 이름과 정확히 일치해야 한다.

type

- 생성할 widget type
- 지원 목록에 있는 값만 사용할 수 있다.

parent

- 부모 widget의 name
- root widget은 빈 문자열을 사용한다.
- spec에는 root widget이 정확히 한 개 있어야 한다.

bind

- true이면 Designer variable로 등록한다.
- native C++ source 생성 모드에서는 BindWidget property도 생성한다.
- layout 전용 widget은 일반적으로 false 또는 생략한다.

text

- TextBlock에 적용할 초기 텍스트
- TextBlock이 아닌 widget에서는 현재 사용하지 않는다.

horizontalAlignment

- Fill, Left, Center, Right 지원
- 생략하면 Center

verticalAlignment

- Fill, Top, Center, Bottom 지원
- 생략하면 Center

padding

- [Left, Top, Right, Bottom] 순서의 숫자 배열
- OverlaySlot, HorizontalBoxSlot, VerticalBoxSlot, ButtonSlot 또는 Border content padding에 적용한다.

sizeRule

- HorizontalBox child의 크기 규칙
- Auto 또는 Fill을 지원한다.
- 생략하면 Auto를 사용한다.

fill

- sizeRule이 Fill인 HorizontalBox child가 차지할 비율
- 0보다 큰 숫자를 사용한다.
- 생략하면 1.0을 사용한다.

color

- [Red, Green, Blue, Alpha] 순서의 숫자 배열
- TextBlock의 ColorAndOpacity, Image의 ColorAndOpacity, Border의 BrushColor에 적용한다.
- 각 값은 일반적으로 0.0에서 1.0 범위를 사용한다.


10. 지원하는 widget type

- CanvasPanel
- Overlay
- Button
- TextBlock
- HorizontalBox
- VerticalBox
- Border
- EditableTextBox
- Image
- CommonActivatableWidgetStack

Widget type mapping:

- CanvasPanel -> UCanvasPanel
- Overlay -> UOverlay
- Button -> UButton
- TextBlock -> UTextBlock
- HorizontalBox -> UHorizontalBox
- VerticalBox -> UVerticalBox
- Border -> UBorder
- EditableTextBox -> UEditableTextBox
- Image -> UImage
- CommonActivatableWidgetStack -> UCommonActivatableWidgetStack


11. Parent-child 규칙

여러 child를 가질 수 있는 parent:

- CanvasPanel
- Overlay
- HorizontalBox
- VerticalBox

한 개의 child만 가질 수 있는 parent:

- Button
- Border

child를 가질 수 없는 widget:

- TextBlock
- EditableTextBox
- Image
- CommonActivatableWidgetStack

Button 또는 Border에 두 개 이상의 child를 지정하면 validation이 실패한다.


12. 자동 검증 항목

Commandlet은 asset을 수정하기 전에 다음 항목을 검증한다.

- 필수 top-level field 존재 여부
- nativeClass U prefix
- 지원하는 parentClass인지 여부
- assetPath가 /Game/으로 시작하는지 여부
- widgets 배열 존재 여부
- widget name 중복
- 지원하지 않는 widget type
- 지원하지 않는 sizeRule
- 0 이하인 fill 값
- root widget 개수
- 누락된 parent 참조
- 순환 parent 참조
- child를 가질 수 없는 parent
- 단일 child parent의 child 개수
- padding과 color 배열의 값 개수 및 숫자 여부

Validation 실패 시 WBP 생성과 저장을 시작하지 않는다.


13. BindWidget 사용 규칙

bind=true는 Designer variable 생성 여부를 결정한다.

기존 class를 재사용할 때:

- C++에 UPROPERTY(meta = (BindWidget)) 또는 BindWidgetOptional property가 있어야 한다.
- JSON name과 C++ property 이름이 정확히 같아야 한다.
- widget type이 C++ property type과 호환되어야 한다.

예시:

JSON:

{
    "name": "Button_NewGame",
    "type": "Button",
    "bind": true
}

C++:

UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UButton> Button_NewGame;


14. Overwrite 안전 규칙

- 기존 WBP를 수정하기 전에 원본 .uasset을 백업한다.
- 처음 작성한 spec은 반드시 DryRun으로 검증한다.
- -Overwrite는 WidgetTree를 제거하고 JSON 기준으로 다시 구성한다.
- Designer에서 수동으로 만든 기존 widget tree는 Overwrite 시 사라질 수 있다.
- 기존 WBP parent class가 다르면 자동 reparent하지 않고 중단한다.
- runtime C++ fallback 코드는 commandlet과 분리해서 유지한다.
- editor-only API와 dependency는 ProjectAEditor 모듈에만 둔다.


15. 빌드와 프로젝트 파일 재생성

C++ 파일을 생성, 삭제 또는 이름 변경한 뒤 프로젝트 파일을 재생성한다.

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine

Development Editor | Win64 빌드:

"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64


16. 결과 확인

Commandlet 로그에서 다음 항목을 확인한다.

- Designer tree populated from JSON spec.
- Blueprint compile result: true
- Asset save result: true
- Success - 0 error(s), 0 warning(s)

그 다음 Unreal Editor에서 대상 WBP를 연다.

확인 항목:

- WBP parent class가 nativeClass와 일치하는지 확인
- Designer hierarchy가 JSON parent 관계와 같은지 확인
- bind=true widget이 Variables로 표시되는지 확인
- TextBlock text, color, padding, alignment가 적용됐는지 확인
- PIE에서 기존 버튼 동작과 CommonUI 화면 전환이 유지되는지 확인


17. MainMenu 화면 전환 규칙

MainMenuRootWidget은 MainStack, MenuStack, ModalStack을 Overlay에 겹쳐서 관리한다.

MainStack은 기본 메인메뉴 화면을 표시하고, MenuStack은 CharacterCreation 같은 메뉴 흐름 화면을 표시한다.

MenuStack에 화면을 Push하면 MainStack은 Hidden 상태가 되어 이전 메뉴 UI가 뒤에 보이지 않고 입력도 받지 않는다.

MenuStack을 Clear하거나 CharacterCreation에서 Back을 누르면 MainStack은 다시 Visible 상태로 복원된다.

CharacterCreation 우측 상단 X 버튼도 Back과 같은 RequestBack 흐름을 사용하므로 MainStack을 다시 Visible 상태로 복원한다.

이 규칙은 stack 구조를 유지하면서 화면 전환 시 이전 UI를 시각적으로 숨기기 위한 것이다.


18. MainMenu 3D PreviewStage 사용법

CharacterCreation의 클래스 카드는 UI만 담당하고, 캐릭터 프리뷰는 L_MainMenu의 실제 월드 액터로 표시한다. SceneCapture2D와 RenderTarget은 사용하지 않는다.

설정 순서:

1. L_MainMenu에 AMainMenuPreviewStage 또는 이를 부모로 만든 Blueprint actor를 한 개 배치한다.
2. 배치한 actor의 PreviewActorClasses 맵에 StableHand, Scholar, Herbalist, Hunter 키와 각 프리뷰 actor class를 연결한다.
3. PreviewCamera가 네 캐릭터와 배경을 함께 담도록 PreviewStage actor와 카메라 transform을 조정한다.
4. Slot0Anchor부터 Slot3Anchor까지 각 캐릭터가 카드 위에 정렬되도록 위치를 조정한다.
5. MainMenuPlayerController를 사용하는 GameMode로 L_MainMenu를 실행한다.
6. CharacterCreation 카드의 Prev 또는 Next 버튼을 눌러 해당 슬롯 actor가 교체되는지 확인한다.

CharacterCreation 슬롯은 처음에는 캐릭터 생성하기 버튼만 표시한다. 버튼을 누르면 해당 슬롯의 SlotEditorBox가 열리고 PreviewStage에 프리뷰 actor가 생성된다. X 버튼을 누르면 SlotEditorBox가 닫히고 PreviewStage의 해당 슬롯 actor가 제거되며, 캐릭터 생성하기 버튼이 다시 표시된다.

MainMenuPlayerController는 레벨에서 PreviewStage를 찾아 해당 actor를 카메라 ViewTarget으로 사용한다. PreviewStage가 없거나 ClassId에 대응하는 actor class가 없으면 경고 로그를 남기며, 카드 UI 자체는 계속 동작한다.

프리뷰 actor class는 메뉴 표시 전용 Blueprint actor를 권장한다. 전투용 입력, AI, 충돌 또는 게임 진행 로직이 자동 실행되지 않도록 구성한다.


19. 문제 해결

WBP가 Content Browser에 보이지 않는 경우:

- JSON assetPath를 확인한다.
- Unreal Editor를 재시작한다.
- Content Browser filter를 해제한다.

Designer tree가 이전 상태로 보이는 경우:

- 외부 commandlet 실행 전에 열어둔 WBP 탭을 저장하지 않는다.
- Unreal Editor를 재시작해서 disk의 최신 asset을 다시 불러온다.

Native class를 찾지 못하는 경우:

- nativeClass 이름을 확인한다.
- ProjectAEditor target을 다시 빌드한다.
- 새 class라면 프로젝트 파일 재생성 후 commandlet을 다시 실행한다.

BindWidget compile warning이 발생하는 경우:

- JSON name과 C++ property 이름을 비교한다.
- widget type과 C++ property type을 비교한다.
- bind=true가 지정됐는지 확인한다.

Overwrite가 중단되는 경우:

- 기존 WBP parent class와 JSON nativeClass가 같은지 확인한다.
- -Overwrite 옵션이 포함됐는지 확인한다.


20. 운영 원칙

UI Scaffold Generator는 Designer 초기 구조 생성과 반복 가능한 구조 갱신을 위한 도구다.

레이아웃 구조와 필수 widget 이름은 JSON으로 관리하고, 이미지 asset, brush, font, 세부 스타일은 생성 후 Widget Blueprint Designer에서 편집할 수 있다.

반복해서 -Overwrite를 실행하면 Designer 수동 수정이 사라질 수 있으므로, spec을 구조의 기준으로 사용할지 초기 생성용으로만 사용할지 작업 전에 결정해야 한다.

21. Persistent Gameplay UI (2026-09-08)

MainMenu → CharacterCreation 4-slot data → Gameplay → Run Map → Encounter → Combat → Result → Run Map 흐름을 추가했다.
GameplayRootWidget은 CommonUserWidget이며 RunLayer/CombatLayer/ModalLayer CommonActivatableWidgetStack을 관리한다.
RunMapWidget, CombatHUDWidget, EncounterResultWidget은 CommonActivatableWidget이다. Run Map은 노드 선택만 요청하며 Unit spawn은 EncounterManager가 담당한다.
GetDesiredInputConfig에서 CombatHUD는 All/CaptureDuringMouseDown, RunMap/Result는 Menu/NoCapture를 지정한다. 커서는 계속 표시한다.
GameplayPlayerController는 화면별 SetInputMode를 호출하지 않는다. BeginPlay에서 MainMenu의 UIOnly travel 잔여 IgnoreInput을 해제하고 로컬 플레이어의 최초 뷰포트 포커스를 복원하며, CommonUI가 실제 입력 전달을 관리한다.
새 Designer WBP 4종은 /Game/User_JeHoon/UI/Gameplay에 저장한다. 기존 /UI/Combat 에셋은 보존한다.
새 JSON spec: GameplayRootWidget.json, RunMapWidget.json, CombatHUDWidget.json, EncounterResultWidget.json.
Generator 기능을 추가하지 않고 실제 Designer tree 생성·Blueprint compile·save 및 별도 프로세스 재로드 검증을 수행한다.
CharacterCreation의 Text_StartGameStatus는 선택 바인딩이며 기존 Designer 구조에 없으면 native가 표시 영역을 추가한다.
정확한 클래스/변수/에디터 연결 순서는 Docs/VERTICAL_SLICE_SETUP.md, 실제 검증 결과는 Docs/VERTICAL_SLICE_REPORT.md를 참고한다.

T12 구조 기준과 누락 보완

- D08: 기존 Designer WBP를 화면 구조의 기준으로 유지한다. JSON은 scaffold와 바인딩 검증에 사용한다.
- -AddMissing: 기존 위젯 속성/계층은 보존하고 누락된 위젯만 추가 후 compile/save. 기존 타입 불일치는 거절한다. -Overwrite와 동시 사용 불가.
- 예: -run=GenerateUiScaffold -Spec=Source/ProjectAEditor/UiScaffoldSpecs/CharacterCreationWidget.json -AddMissing
- DryRun은 JSON 유효성만 확인한다. 실제 생성 검증은 assetPath를 /Game/T12Validation로 바꾼 Saved 아래 복사 명세로 실행한다.
- T12GeneratedAssets 검증 fixture: 메뉴 3종을 위 경로에 생성한다. MainMenuScreenWidget 복사 명세의 Text_Title 내용을 다르게 쓰고 T12_MissingLabel(TextBlock, parent=RootOverlay)을 추가해 AddMissing을 2회 실행한다. 테스트는 기존 제목 보존/신규 위젯 저장/바인딩을 별도 프로세스에서 확인한다.
- 실제 원본에 추가한 것은 CharacterCreation의 Text_StartGameStatus 1개다. 확인된 BindWidget TODO만 검증 주석으로 갱신하며 선택적인 이전 팝업/Warrior 계열 바인딩은 완료로 간주하지 않는다.
