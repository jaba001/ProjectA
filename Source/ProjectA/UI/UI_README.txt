ProjectA UI 작업 메모

작성 기준: editor tooling implementation step 4
작성일: 2026-06-17

1. 작업 요약

ProjectA에 에디터 전용 UI Scaffold 생성기를 추가했다.

이 생성기는 JSON 명세 파일을 읽어서 다음 항목을 생성한다.

- Native C++ UUserWidget 클래스
- BindWidget 속성이 포함된 .h / .cpp 파일
- Widget Blueprint asset
- Widget Blueprint Designer에서 실제로 보이는 Designer tree

현재 기본 테스트 명세 파일은 다음 위치에 있다.

Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScaffoldTest.json

현재 기본 생성 대상은 다음과 같다.

- Native class: UMainMenuScaffoldTestWidget
- Header: Source/ProjectA/UI/Generated/MainMenuScaffoldTestWidget.h
- Source: Source/ProjectA/UI/Generated/MainMenuScaffoldTestWidget.cpp
- Widget Blueprint: /Game/User_JeHoon/UI/MainMenu/Generated/WBP_MainMenuScaffoldTest

생성되는 기본 Designer tree는 다음 구조다.

RootCanvas
MainOverlay
Button_Start
Text_Start

Text_Start에는 "Start" 텍스트가 적용된다.


2. 이 작업이 필요한 이유

기존 MainMenu와 CharacterCreation UI에는 C++ native fallback 흐름이 있다.
이 방식은 Blueprint Designer 작업량을 줄이고, WBP가 비어 있어도 UI가 동작하게 만드는 데 유용하다.

하지만 C++ 런타임에서 WidgetTree를 만드는 방식은 Blueprint Designer에서 실제 계층 구조가 보이지 않는다.
그래서 Designer에서 이미지, 버튼 스타일, 배치 등을 직접 편집하고 저장하기 어렵다.

이번 작업의 목적은 런타임 fallback은 그대로 유지하면서, 별도의 에디터 전용 도구로 편집 가능한 Widget Blueprint asset을 생성하는 것이다.

즉, 런타임에서 UI를 만드는 것이 아니라 에디터 명령으로 WBP asset 내부에 실제 Designer tree를 저장한다.


3. 기존 runtime UI와의 관계

이번 생성기는 ProjectAEditor 모듈 안에서만 동작한다.

기존 MainMenu / CharacterCreation runtime fallback 구현은 변경하지 않았다.
기존 WBP_MainMenuScreenWidget, WBP_CharacterCreationWidget도 덮어쓰지 않는다.

생성 대상은 테스트 전용 경로인 /Game/User_JeHoon/UI/MainMenu/Generated 아래로 분리되어 있다.


4. 주요 파일

Editor module:

- Source/ProjectAEditor/ProjectAEditor.Build.cs
- Source/ProjectAEditor/ProjectAEditor.h
- Source/ProjectAEditor/ProjectAEditor.cpp
- Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.h
- Source/ProjectAEditor/Commandlets/GenerateUiScaffoldCommandlet.cpp

JSON spec:

- Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScaffoldTest.json

Generated native sample:

- Source/ProjectA/UI/Generated/MainMenuScaffoldTestWidget.h
- Source/ProjectA/UI/Generated/MainMenuScaffoldTestWidget.cpp

Generated WBP sample:

- Content/User_JeHoon/UI/MainMenu/Generated/WBP_MainMenuScaffoldTest.uasset


5. JSON 명세 구조

기본 JSON은 다음 정보를 가진다.

- nativeClass: 생성할 C++ 클래스 이름
- nativeHeaderPath: 생성할 header 경로
- nativeSourcePath: 생성할 cpp 경로
- blueprintName: 생성할 WBP asset 이름
- assetPath: 생성할 WBP asset 폴더
- parentClass: 현재 단계에서는 UUserWidget 지원
- widgets: Designer tree에 들어갈 widget 목록

widgets 항목의 주요 필드는 다음과 같다.

- name: Designer tree와 BindWidget에 사용할 이름
- type: widget 타입
- parent: 부모 widget 이름
- bind: true이면 C++에 UPROPERTY(meta = (BindWidget)) 생성
- text: TextBlock에 적용할 텍스트


6. 현재 지원하는 widget 타입

- CanvasPanel
- Overlay
- Button
- TextBlock
- VerticalBox
- Border
- EditableTextBox


7. 검증 규칙

commandlet은 생성 전에 JSON 명세를 검증한다.

현재 검증하는 항목은 다음과 같다.

- 필수 top-level field 존재 여부
- nativeClass가 U로 시작하는지 여부
- blueprintName이 비어 있지 않은지 여부
- assetPath가 /Game/으로 시작하는지 여부
- widgets 배열 존재 여부
- widget 이름 중복 여부
- parent 참조 누락 여부
- 지원하지 않는 widget type 여부
- root widget이 정확히 1개인지 여부
- 순환 parent 참조 여부
- 자식을 가질 수 없는 widget에 child가 연결되었는지 여부
- Button / Border처럼 단일 child만 허용하는 widget에 여러 child가 연결되었는지 여부

검증 실패 시 파일과 asset을 생성하지 않고 중단한다.


8. 사용법

기본 spec으로 검증만 실행하려면 다음 명령을 사용한다.

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -DryRun

기본 spec으로 실제 생성을 실행하려면 다음 명령을 사용한다.

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI

이미 같은 WBP asset이 있을 때 다시 생성하려면 -Overwrite를 붙인다.

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Overwrite

다른 spec 파일을 사용하려면 -Spec 옵션을 사용한다.

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -run=GenerateUiScaffold -nop4 -unattended -NullRHI -Spec="Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScaffoldTest.json"


9. DryRun 동작

- JSON 파일을 읽는다.
- 필수 필드와 widget tree 규칙을 검증한다.
- 생성될 class, source path, asset path, widget count를 로그로 출력한다.
- C++ 파일을 쓰지 않는다.
- WBP asset을 만들지 않는다.
- package를 저장하지 않는다.


10. Overwrite 동작

기본 동작은 안전을 위해 overwrite하지 않는 것이다.

같은 WBP asset이 이미 있으면 -Overwrite가 없을 때 asset을 수정하지 않고 skip한다.

-Overwrite가 있으면 기존 WBP를 다시 로드해서 Designer tree를 재구성하고 저장한다.
단, 기존 WBP parent class가 spec의 native class와 다르면 overwrite를 중단한다.


11. 빌드와 프로젝트 파일

C++ header/source 파일을 생성하거나 이름을 바꾼 뒤에는 Visual Studio 프로젝트 파일을 재생성해야 한다.

프로젝트 파일 재생성 명령:

"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine

Development Editor | Win64 빌드 명령:

"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64


12. 수동 확인 방법

Unreal Editor에서 다음 asset을 연다.

/Game/User_JeHoon/UI/MainMenu/Generated/WBP_MainMenuScaffoldTest

Widget Blueprint Designer에서 다음 hierarchy가 보이면 정상이다.

RootCanvas
MainOverlay
Button_Start
Text_Start

Class Settings 또는 Details에서 parent class가 UMainMenuScaffoldTestWidget인지 확인한다.

Button_Start와 Text_Start가 C++ BindWidget으로 연결되는지 확인한다.


13. 주의사항

이 도구는 editor-only 작업용이다.

UnrealEd, UMGEditor, AssetTools, BlueprintGraph, KismetCompiler 같은 editor dependency는 ProjectAEditor 모듈에만 있어야 한다.
runtime ProjectA.Build.cs에는 editor-only dependency를 추가하면 안 된다.

Designer tree는 commandlet이 asset 생성 시점에 저장한다.
NativeConstruct나 NativeOnInitialized에서 UI layout을 생성하는 방식으로 해결하면 안 된다.

새 native class를 처음 생성한 직후에는 그 class가 아직 editor binary에 로드되어 있지 않을 수 있다.
그 경우 프로젝트 파일 재생성 후 ProjectAEditor target을 빌드한 다음 commandlet을 다시 실행해야 한다.

Visual Studio 자동 열기는 작업 후 기본으로 수행하지 않는다.
