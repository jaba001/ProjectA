# ProjectA

Unreal Engine 5.7 기반의 파티·Grid 턴제 전투 프로젝트. UI 중심의 Run 진행, 파티 빌드 성장, 상대 Snapshot을 활용한 Async PvP와 최대 4인 Co-op을 목표로 한다.

## 구현 상태

| 구분 | 상태 |
|---|---|
| 기본 Run | 싱글플레이. 동일 Gameplay 레벨에서 두 전투 진행 |
| 전투 | 이동·스킬·회복약·턴 종료, GAS 피해, 서버 권위·조작권 검증 |
| 저장 | 일반 Run v1~3, 관리 Run v4, 확정 턴 복구, 로컬 Host 승계·AI 전환 |
| 멀티플레이 | 로컬 Snapshot 및 Listen Server 구현. 최신 2·4인 전투 자동화 통과 |
| T14 잔여 | 최신 4→3인 승계 검증, Steam/PlayFab 인증·P2P·공유 저장·MMR |

일반 메뉴의 협동 생성·초대·로그인 UI는 미구현이다. 현재 협동 검증은 개발용 참가자 배정과 PIE 자동화를 사용한다. 로컬 검증과 온라인 서비스 완료를 구분한다.

## 실행

1. UE 5.7에서 `ProjectA.uproject`와 `/Game/User_JeHoon/LEVEL/MainMenu`를 연다.
2. New Game → 캐릭터 1~4명 생성 → Start Game → 첫 Combat 노드를 선택한다.
3. Move·스킬·회복약·End Turn으로 전투를 진행한다.
4. Victory → Continue → 다음 전투로 진행한다. 패배 시 Run을 종료한다.

일반 Continue는 지원되는 오프라인 저장을 복구한다. 협동·관리 저장은 별도 재개 절차를 사용한다. 상세 설정은 [PROJECT_PLAN](Docs/PROJECT_PLAN.md)을 따른다.

## 문서

| 문서 | 내용 |
|---|---|
| [GAME_DESIGN](Docs/GAME_DESIGN.md) | 목표 기획과 확정 방향 |
| [PROJECT_PLAN](Docs/PROJECT_PLAN.md) | 모듈·전투·UI·에셋·설정 |
| [TODO](Docs/TODO.md) | 미완료 작업과 결정 사항 |
| [MULTIPLAYER](Docs/MULTIPLAYER.md) | 식별·소유권·저장·네트워크 계약 |
| [TEST_REPORT](Docs/TEST_REPORT.md) | 실행 절차·검증 결과·경고 대응 |
| [HISTORY](Docs/HISTORY.md) | 완료 이력과 기준 커밋 |

## 개발 환경

- 엔진: Unreal Engine 5.7, C++, GAS, CommonUI, UMG.
- 모듈: `Source/ProjectA`는 런타임, `Source/ProjectAEditor`는 에셋 도구·에디터 테스트. Editor 의존성은 런타임 모듈에 추가하지 않는다.
- 제작 에셋: `Content/User_JeHoon` (`/Game/User_JeHoon`). 외부 리소스·템플릿 원본은 유지하며 편집 사본을 제작 경로에 둔다.
- UI: [에셋 도구](Source/ProjectAEditor/Scripts/README.md), [위젯·JSON 명세](Source/ProjectA/UI/UI_README.txt).

Development Editor / Win64 빌드:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

C++ 파일 추가·삭제·이름 변경 후 프로젝트 파일 재생성:

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine
```

## 검증 현황

2026-09-11 로컬 전투 자동화: **2인 1건 통과(경고 4), 4인 1건 통과(경고 6), 실패 0**. 조작권·행동 RPC·상태 동기화·두 전투 완료를 확인했다. Steam/P2P·수동 조작·승계 검증은 포함하지 않는다. 상세 범위와 경고 대응은 [TEST_REPORT](Docs/TEST_REPORT.md)에 기록한다.

## 작업 규칙

[AGENTS.md](AGENTS.md)를 기준으로 한다. 작동 테스트는 사용자 수행이 원칙이며 명시적 실행 요청에 한해 Codex가 수행한다. Visual Studio는 자동 실행하지 않는다. 변경은 검증 후 `[codex]` 제목과 한글 본문으로 커밋·push한다. 보고서의 결과·문제·권장 대응은 답변에도 요약한다.
