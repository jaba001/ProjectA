# ProjectA

Unreal Engine 5.7 기반의 파티·Grid 턴제 전투 프로젝트. UI 중심의 Run 진행, 파티 빌드 성장, 상대 Snapshot을 활용한 Async PvP와 최대 4인 Co-op을 목표로 한다.

기본 전투는 [라운드 계획·시간차 자동 전투](Docs/GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투)다. 각 라운드의 행동을 함께 계획하고 속도 1 차이당 0.1초 후 실행한다. 실제 접근·복귀, 유닛 이동 비충돌, 서버 실시간 피격을 사용한다. 기존 순차 턴 실행은 제거했으며 기존 Blueprint 참조용 클래스·프로퍼티만 남긴다. 일부 [임시 구현 정책](Docs/TODO.md#3-1-전투-규칙)은 사용자 확인 대기다.

## 구현 상태

| 구분 | 상태 |
|---|---|
| 기본 Run | 두 전투 사이 상점1·상점2·상점3 중 하나 선택·진입·퇴장. 동일 Gameplay 유지 |
| 전투 | CommonUI 행동 계획·준비, 합법 대상·자원·타일의 적용 전 검사, 시간차 실행·접근/복귀·서버 피격. AI는 장착된 복귀형 Tile 공격도 선택 가능. 사용자 작동 확인 대기 |
| 저장 | 비전투 일반 v1/v2·관리 v4 유지. 이전 Combat 저장의 이어하기·재개·새 턴 저장은 거절. 라운드 중간 복구 미지원 |
| 시작 메뉴 설정 | 해상도·전체 화면/테두리 없는 전체 화면/창 모드·그래픽 품질·VSync. 화면 변경은 15초 확인 후 저장, 미확인 시 복원 |
| 공통 UI 외형 | 기존 DemonicUI의 청동 장식·붉은 버튼·성 배경을 메뉴·설정·캐릭터 생성·협동·Run·상점·결과·라운드 계획과 저장/협동 안내에 적용. 사용자 시각·입력 확인 대기 |
| 공통 UI 배율 | 1920×1080 기준 공통 DPI로 전체 UI를 한 번 확대·축소. 전투 양옆 패널의 바깥 가로 여백과 카메라의 고정 화면 비율 제한 제거. 사용자 확인 대기 |
| 멀티플레이 | 기존 Listen Server·원래 소유권·관리 lease를 라운드 요청에 연결. 최신 2/4인 작동 검증은 미실행 |
| T14 잔여 | Steam/PlayFab 인증·P2P·공유 저장·MMR. Cue 설정 적용 수정과 일반 전투 재검증은 별도 추적 |

메인메뉴의 **개발용 협동**에서 2~4인 방 생성·IPv4 주소 참가·준비·Host 시작을 지원한다. 참가자마다 Hunter 한 명을 배정하며 실제 연결은 Unreal Listen Server를 사용한다. Shipping에서는 비활성이다. Steam 초대·로그인·관리 Run 재개 UI는 별도이며, 새 UI의 작동 검증은 대기다.

## 실행

1. UE 5.7에서 `ProjectA.uproject`와 `/Game/User_JeHoon/LEVEL/MainMenu`를 연다.
2. New Game → 캐릭터 1~4명 생성 → Start Game → 첫 Combat 노드를 선택한다.
3. 전투 계획 화면에서 조작할 아군·스킬·합법 대상·필요한 목적지를 선택하고 **계획 적용**한다. 자원·타일·대상 조건을 만족하는 계획이 자신의 모든 생존 아군에 적용되면 **준비 완료**할 수 있다. 자동 실행·복귀·잔여 투사체 정리 후 다음 라운드를 계획한다.
4. 첫 Victory → Continue → 상점1·상점2·상점3 중 하나 선택 → 나가기 → 두 번째 전투로 진행한다. 마지막 Victory 뒤 Continue 또는 패배 시 Run을 종료한다.

상점은 이름과 나가기 버튼만 제공하는 시험용 화면이다. 상품·재화·회복·확률 추첨은 미구현이다. 새 Run에 적용하며 기존 저장은 원래 두 전투 경로를 유지한다. 선택 목록·선택한 상점·퇴장 여부를 저장하고 협동에서는 Host만 진행을 결정한다. 확인 절차는 [테스트 보고서 10절](Docs/TEST_REPORT.md#10-상점-인카운터)을 따른다.

C++·리플렉션·프로젝트 설정 변경 반영을 위해 빌드 후 UE를 재시작한다. 기존 Gameplay 맵·GameMode·Root UI를 사용하며 새 에셋 생성은 필요하지 않다.

처음 실행한 MainMenu의 Options에서 화면과 그래픽을 설정한다. 항목 선택 후 적용하며, 해상도·화면 모드가 바뀌면 15초 안에 확인해야 저장된다. 취소·시간 초과·확인 전 화면 닫기는 적용 전 상태로 복원한다. 테두리 없는 전체 화면은 바탕 화면 해상도를 사용한다. 품질·VSync만 바꾸면 적용 시 바로 저장한다. 상세는 [설정 UI](Docs/UI_README.md#8-시작-메뉴-설정), 사용자 확인은 [TEST_REPORT 14절](Docs/TEST_REPORT.md#14-시작-메뉴-화면그래픽-설정)을 따른다.

공통 외형은 native `UDemonicUITheme`이 기존 `/Game/DemonicUI` 텍스처를 참조하여 적용한다. C++ 빌드 후 UE를 재시작하면 기존 Designer 위젯과 native 컨트롤에 반영되며 WBP·JSON 재생성은 필요하지 않다. 원본 에셋은 수정하지 않는다. 적용 범위는 [UI_README 9절](Docs/UI_README.md#9-demonicui-공통-테마), 해상도별 가독성·프리뷰·입력·패키지 리소스 확인은 [TEST_REPORT 15절](Docs/TEST_REPORT.md#15-demonicui-공통-테마)을 따른다.

UI는 1920×1080을 기준으로 뷰포트에 맞춰 같은 비율로 확대·축소한다. 메뉴·설정 등의 개별 축소를 제거하고 프로젝트 공통 DPI를 사용한다. 전투 좌우 패널은 화면 가장자리에 배치하며 전투 카메라는 고정 16:9 검은 띠를 만들지 않는다. 배율 규칙은 [UI_README 10절](Docs/UI_README.md#10-공통-dpi와-전투-화면-배치), 사용자 확인은 [TEST_REPORT 16절](Docs/TEST_REPORT.md#16-공통-dpi와-전투-화면-배치)을 따른다.

일반 Continue는 지원되는 전투 외 오프라인 저장을 복구한다. 이전 순차 전투의 Combat 저장 파일은 변경하지 않고 로드를 거절한다. 새 전투 종료 전 중단하면 마지막 전투 외 확정 저장부터 다시 시작한다. 협동·관리 저장은 별도 재개 절차를 사용한다. 상세 설정은 [PROJECT_PLAN](Docs/PROJECT_PLAN.md)을 따른다.

개발용 협동: 독립 게임 창 2개에서 MainMenu → 개발용 협동을 연다. 첫 창은 2인 방 생성, 두 번째는 `127.0.0.1:7777`로 참가한다. 각자 준비 완료 후 Host가 시작하고 첫 전투 노드를 선택한다. 창 실행 명령과 4인·이탈 확인 절차는 [테스트 보고서 9절](Docs/TEST_REPORT.md#9-개발용-협동-ui)을 따른다.

## 문서

| 문서 | 내용 |
|---|---|
| [GAME_DESIGN](Docs/GAME_DESIGN.md) | 목표 기획·확정 방향·FTK2 설치본 근거·아이템/직업/상점 제안 |
| [STATUS_EFFECTS](Docs/STATUS_EFFECTS.md) | 해로운 상태이상 8종의 확정·잠정·미정 규칙, 구현 연결 검토와 미실행 검증 계획 |
| [PROJECT_PLAN](Docs/PROJECT_PLAN.md) | 모듈·전투·UI·에셋·설정 |
| [UI_README](Docs/UI_README.md) | UI 구조·JSON 명세·생성 옵션 |
| [TODO](Docs/TODO.md) | 미완료 작업과 결정 사항 |
| [MULTIPLAYER](Docs/MULTIPLAYER.md) | 식별·소유권·저장·네트워크 계약 |
| [TEST_REPORT](Docs/TEST_REPORT.md) | 실행 절차·검증 결과·경고 대응 |
| [HISTORY](Docs/HISTORY.md) | 완료 이력과 기준 커밋 |

[아이템 클론 기획](Docs/GAME_DESIGN.md#7-아이템-클론-기획)은 For The King 1·2를 참고한 42개 항목과 작품별 차이·데이터 변환 항목을 제공한다. 시간차 전투를 유지하며 집중(Focus) 자원·집중초·회복/소모형 효과는 기획에서 제외했다. [FTK2 설치본 Build 24247341](Docs/GAME_DESIGN.md#7-11-ftk2-설치본-근거와-해석-범위)의 JSON으로 무기 판정·장비 보정·회복/방어 파이프·상점·직업 역할을 보충했다. 기획 제안이며 장비·거래·약초 시스템 구현과 원작 작동 검증은 포함하지 않는다. 나머지 채택 정책은 [후속 결정](Docs/TODO.md#3-2-아이템과-직업), 직접 대조 절차는 [TEST_REPORT 11절](Docs/TEST_REPORT.md#11-아이템-클론-기획-검토)에서 관리한다.

## 개발 환경

- 엔진: Unreal Engine 5.7, C++, GAS, CommonUI, UMG.
- 모듈: `Source/ProjectA`는 런타임, `Source/ProjectAEditor`는 에셋 도구·에디터 테스트. Editor 의존성은 런타임 모듈에 추가하지 않는다.
- 제작 에셋: `Content/User_JeHoon` (`/Game/User_JeHoon`). 외부 리소스·템플릿 원본은 유지하며 편집 사본을 제작 경로에 둔다.
- UI: [에셋 도구](Source/ProjectAEditor/Scripts/README.md), [위젯·JSON 명세](Docs/UI_README.md).

Development Editor / Win64 빌드:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

C++ 파일 추가·삭제·이름 변경 후 프로젝트 파일 재생성:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine
```

## 검증 현황

2026-09-16 계획 입력 검사와 장착 Tile 공격 AI 보완의 Development Editor / Win64 컴파일은 성공했다(회귀 코드 4건 포함 17.52초, 아군 목록 갱신 시 초안 보존을 보강한 최종 증분 빌드 4.61초, 두 빌드 컴파일 오류·경고 0). 최종 코드·문서 정적 검사를 통과했다. 합법 대상 표시·사망 후 초안 갱신·자원/타일·준비·서버 검증과 AI 회귀는 [TEST_REPORT 17절](Docs/TEST_REPORT.md#17-계획-입력-검사와-장착-tile-공격-ai)에 인계하며 작동 검증 8항목은 미실행이다. 디자인이 필요한 후속 구현은 [TODO 5절](Docs/TODO.md#5-디자인-확정-후-구현할-일)의 결정 후 재개한다.

공통 DPI·전투 화면 배치 변경의 Development Editor / Win64 컴파일이 성공했다(14.79초, 컴파일 오류·경고 0). 8개 뷰포트의 배율 수식·중복 축소 제거·카메라 설정·Python 문법·문서 정적 검사도 통과했다. 해상도별 크기 비례·울트라와이드·4:3·창 크기 변경·전투 클릭·설정 복원은 [TEST_REPORT 16절](Docs/TEST_REPORT.md#16-공통-dpi와-전투-화면-배치)에 인계하며 작동 검증 8항목은 미실행이다.

공통 DemonicUI 테마의 프로젝트 파일 재생성(18.14초)과 모든 소스 보완을 포함한 최종 Development Editor / Win64 빌드(9.03초, 컴파일 오류·경고 0)가 성공했다. 텍스처 참조 11개의 경로·문서 링크·앵커·diff 검사도 통과했다. 시각·입력·패키지 확인은 [TEST_REPORT 15절](Docs/TEST_REPORT.md#15-demonicui-공통-테마)에 인계하며 작동 검증 14항목은 미실행이다.

시작 메뉴 설정 확장의 Development Editor / Win64 컴파일은 성공했다(기능 변경 18.45초, 회귀 코드 포함 최종 증분 빌드 3.88초, 두 빌드 모두 컴파일 오류·경고 0). 코드·문서·링크 정적 검사도 통과했다. 해상도·화면 모드·품질·VSync와 15초 복원·재시작 저장 확인은 [TEST_REPORT 14절](Docs/TEST_REPORT.md#14-시작-메뉴-화면그래픽-설정)에 인계하며 작동 검증 17항목은 미실행이다.

이전 기본 라운드 전투 전환의 Development Editor / Win64 컴파일은 성공했다(23.23초, 오류·경고 0). 당시 프로젝트 파일 재생성·문서 링크·앵커·diff 검사도 통과했다. 해당 결과는 새 설정 UI의 컴파일·작동 성공 근거로 사용하지 않는다. 전투·PIE·자동화 테스트는 미실행이며 [TEST_REPORT 12절](Docs/TEST_REPORT.md#12-시간차-자동-전투-기획-검토)의 직접 확인이 필요하다.

2026-09-11 `ff22940`의 승계 PIE 3건·관리 계약 4건 성공은 이전 순차 전투의 이력이다. 새 라운드 전투·새 UI·피격·복구의 성공 근거로 사용하지 않는다. 기존 Cue fallback 경고와 상점·일반 협동 사용자 확인도 남아 있다.

## 작업 규칙

[AGENTS.md](AGENTS.md)를 기준으로 한다. 작동 테스트는 사용자 수행이 원칙이며 명시적 실행 요청에 한해 Codex가 수행한다. Visual Studio는 자동 실행하지 않는다. 변경은 검증 후 `[codex]` 제목과 한글 본문으로 커밋·push한다. 보고서의 결과·문제·권장 대응은 답변에도 요약한다.
