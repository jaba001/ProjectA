# ProjectA

Unreal Engine 5.7 기반의 파티·Grid 턴제 전투 프로젝트. UI 중심의 Run 진행, 파티 빌드 성장, 상대 Snapshot을 활용한 Async PvP와 최대 4인 Co-op을 목표로 한다.

## 구현 상태

| 구분 | 상태 |
|---|---|
| 기본 Run | 두 전투 사이 상점1·상점2·상점3 중 하나 선택·진입·퇴장. 동일 Gameplay 유지 |
| 전투 | 이동·스킬·회복약·턴 종료, GAS 피해, 서버 권위·조작권 검증 |
| 저장 | 일반 Run v1~3, 관리 Run v4, 확정 턴 복구, 로컬 Host 승계·AI 전환 |
| 멀티플레이 | 로컬 Snapshot 및 Listen Server 구현. 3→2인·4→3인 승계와 단독 AI 재개 검증 통과 |
| T14 잔여 | Steam/PlayFab 인증·P2P·공유 저장·MMR. Cue 설정 적용 수정과 일반 전투 재검증은 별도 추적 |

메인메뉴의 **개발용 협동**에서 2~4인 방 생성·IPv4 주소 참가·준비·Host 시작을 지원한다. 참가자마다 Hunter 한 명을 배정하며 실제 연결은 Unreal Listen Server를 사용한다. Shipping에서는 비활성이다. Steam 초대·로그인·관리 Run 재개 UI는 별도이며, 새 UI의 작동 검증은 대기다.

## 실행

1. UE 5.7에서 `ProjectA.uproject`와 `/Game/User_JeHoon/LEVEL/MainMenu`를 연다.
2. New Game → 캐릭터 1~4명 생성 → Start Game → 첫 Combat 노드를 선택한다.
3. Move·스킬·회복약·End Turn으로 전투를 진행한다.
4. 첫 Victory → Continue → 상점1·상점2·상점3 중 하나 선택 → 나가기 → 두 번째 전투로 진행한다. 마지막 Victory 뒤 Continue 또는 패배 시 Run을 종료한다.

상점은 이름과 나가기 버튼만 제공하는 시험용 화면이다. 상품·재화·회복·확률 추첨은 미구현이다. 새 Run에 적용하며 기존 저장은 원래 두 전투 경로를 유지한다. 선택 목록·선택한 상점·퇴장 여부를 저장하고 협동에서는 Host만 진행을 결정한다. 확인 절차는 [테스트 보고서 10절](Docs/TEST_REPORT.md#10-상점-인카운터)을 따른다.

일반 Continue는 지원되는 오프라인 저장을 복구한다. 협동·관리 저장은 별도 재개 절차를 사용한다. 상세 설정은 [PROJECT_PLAN](Docs/PROJECT_PLAN.md)을 따른다.

개발용 협동: 독립 게임 창 2개에서 MainMenu → 개발용 협동을 연다. 첫 창은 2인 방 생성, 두 번째는 `127.0.0.1:7777`로 참가한다. 각자 준비 완료 후 Host가 시작하고 첫 전투 노드를 선택한다. 창 실행 명령과 4인·이탈 확인 절차는 [테스트 보고서 9절](Docs/TEST_REPORT.md#9-개발용-협동-ui)을 따른다.

## 문서

| 문서 | 내용 |
|---|---|
| [GAME_DESIGN](Docs/GAME_DESIGN.md) | 목표 기획·확정 방향·아이템 클론 제안 |
| [PROJECT_PLAN](Docs/PROJECT_PLAN.md) | 모듈·전투·UI·에셋·설정 |
| [UI_README](Docs/UI_README.md) | UI 구조·JSON 명세·생성 옵션 |
| [TODO](Docs/TODO.md) | 미완료 작업과 결정 사항 |
| [MULTIPLAYER](Docs/MULTIPLAYER.md) | 식별·소유권·저장·네트워크 계약 |
| [TEST_REPORT](Docs/TEST_REPORT.md) | 실행 절차·검증 결과·경고 대응 |
| [HISTORY](Docs/HISTORY.md) | 완료 이력과 기준 커밋 |

[아이템 클론 기획](Docs/GAME_DESIGN.md#7-아이템-클론-기획)은 For The King 1·2를 참고한 44개 항목과 작품별 차이·원작 출처·데이터 변환 항목을 제공한다. 기획 제안이며 장비·거래·약초 시스템 구현은 포함하지 않는다. 채택할 정책은 [후속 결정](Docs/TODO.md#7-아이템-클론-기획-후속-결정)에서 관리한다.

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

2026-09-11 `ff22940` 기준 **승계 PIE 3건·관리 계약 4건 통과, 테스트 오류 0·경고 11**. 원래 소유권·턴 복구·AI 전환·맵 이동 실패 후 재시도를 확인했다. 빈 HUD 경고는 미발생이나 GameplayCue fallback은 재발해 추가 수정이 필요하다. 일반 2·4인 전투는 경고 수정 전 통과 이력이며 수정 후 별도 재검증 대기다. 실행 범위·경고·절차는 [TEST_REPORT](Docs/TEST_REPORT.md)를 따른다.

상점 인카운터 추가 후 Development Editor / Win64 컴파일·정적 검사 완료. PIE·게임·자동화 테스트는 미실행이며 위 과거 결과는 최신 코드의 통과 근거로 사용하지 않는다.

## 작업 규칙

[AGENTS.md](AGENTS.md)를 기준으로 한다. 작동 테스트는 사용자 수행이 원칙이며 명시적 실행 요청에 한해 Codex가 수행한다. Visual Studio는 자동 실행하지 않는다. 변경은 검증 후 `[codex]` 제목과 한글 본문으로 커밋·push한다. 보고서의 결과·문제·권장 대응은 답변에도 요약한다.
