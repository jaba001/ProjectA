# ProjectA

Unreal Engine 5.7 기반의 파티·Grid 턴제 전투 프로젝트. UI 중심의 Run 진행, 파티 빌드 성장, 상대 Snapshot을 활용한 Async PvP와 최대 4인 Co-op을 목표로 한다.

기본 전투는 [라운드 계획·시간차 자동 전투](Docs/GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투)다. 각 라운드의 행동을 함께 계획하고 속도 1 차이당 0.1초 후 실행한다. 실제 접근·복귀, 유닛 이동 비충돌, 서버 실시간 피격을 사용한다. 기존 순차 턴 실행은 제거했으며 기존 Blueprint 참조용 클래스·프로퍼티만 남긴다. 일부 [임시 구현 정책](Docs/TODO.md#8-시간차-자동-전투-후속-결정)은 사용자 확인 대기다.

## 구현 상태

| 구분 | 상태 |
|---|---|
| 기본 Run | 두 전투 사이 상점1·상점2·상점3 중 하나 선택·진입·퇴장. 동일 Gameplay 유지 |
| 전투 | CommonUI 행동 계획·준비, 시간차 실행·접근/복귀·근접 거리 검사·투사체 충돌·속도순 지원. 기존 장착 스킬의 초기 변환과 공통 시험 행동 제공 |
| 저장 | 비전투 일반 v1/v2·관리 v4 유지. 이전 Combat 저장의 이어하기·재개·새 턴 저장은 거절. 라운드 중간 복구 미지원 |
| 멀티플레이 | 기존 Listen Server·원래 소유권·관리 lease를 라운드 요청에 연결. 최신 2/4인 작동 검증은 미실행 |
| T14 잔여 | Steam/PlayFab 인증·P2P·공유 저장·MMR. Cue 설정 적용 수정과 일반 전투 재검증은 별도 추적 |

메인메뉴의 **개발용 협동**에서 2~4인 방 생성·IPv4 주소 참가·준비·Host 시작을 지원한다. 참가자마다 Hunter 한 명을 배정하며 실제 연결은 Unreal Listen Server를 사용한다. Shipping에서는 비활성이다. Steam 초대·로그인·관리 Run 재개 UI는 별도이며, 새 UI의 작동 검증은 대기다.

## 실행

1. UE 5.7에서 `ProjectA.uproject`와 `/Game/User_JeHoon/LEVEL/MainMenu`를 연다.
2. New Game → 캐릭터 1~4명 생성 → Start Game → 첫 Combat 노드를 선택한다.
3. 전투 계획 화면에서 조작할 아군·스킬·대상·필요한 목적지를 선택하고 **계획 적용**한다. 자신의 모든 생존 아군 계획을 정한 뒤 **준비 완료**한다. 자동 실행·복귀·잔여 투사체 정리 후 다음 라운드를 계획한다.
4. 첫 Victory → Continue → 상점1·상점2·상점3 중 하나 선택 → 나가기 → 두 번째 전투로 진행한다. 마지막 Victory 뒤 Continue 또는 패배 시 Run을 종료한다.

상점은 이름과 나가기 버튼만 제공하는 시험용 화면이다. 상품·재화·회복·확률 추첨은 미구현이다. 새 Run에 적용하며 기존 저장은 원래 두 전투 경로를 유지한다. 선택 목록·선택한 상점·퇴장 여부를 저장하고 협동에서는 Host만 진행을 결정한다. 확인 절차는 [테스트 보고서 10절](Docs/TEST_REPORT.md#10-상점-인카운터)을 따른다.

C++·리플렉션 변경 반영을 위해 빌드 후 UE를 재시작한다. 기존 Gameplay 맵·GameMode·Root UI를 사용하며 이번 전환에 새 에셋 생성이나 Config 변경은 필요하지 않다.

일반 Continue는 지원되는 전투 외 오프라인 저장을 복구한다. 이전 순차 전투의 Combat 저장 파일은 변경하지 않고 로드를 거절한다. 새 전투 종료 전 중단하면 마지막 전투 외 확정 저장부터 다시 시작한다. 협동·관리 저장은 별도 재개 절차를 사용한다. 상세 설정은 [PROJECT_PLAN](Docs/PROJECT_PLAN.md)을 따른다.

개발용 협동: 독립 게임 창 2개에서 MainMenu → 개발용 협동을 연다. 첫 창은 2인 방 생성, 두 번째는 `127.0.0.1:7777`로 참가한다. 각자 준비 완료 후 Host가 시작하고 첫 전투 노드를 선택한다. 창 실행 명령과 4인·이탈 확인 절차는 [테스트 보고서 9절](Docs/TEST_REPORT.md#9-개발용-협동-ui)을 따른다.

## 문서

| 문서 | 내용 |
|---|---|
| [GAME_DESIGN](Docs/GAME_DESIGN.md) | 목표 기획·확정 방향·아이템 클론 제안 |
| [STATUS_EFFECTS](Docs/STATUS_EFFECTS.md) | 해로운 상태이상 8종의 확정·잠정·미정 규칙, 구현 연결 검토와 미실행 검증 계획 |
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

최종 Development Editor / Win64 컴파일 성공(23.23초, 오류·경고 0). 프로젝트 파일 재생성 및 새 C++ 11개·삭제된 테스트 3개의 vcxproj/filters 반영, 문서 링크·앵커와 diff 검사를 통과했다. 선행 C4996 경고는 수정했다. 게임·PIE·자동화 테스트는 미실행이며 [TEST_REPORT 12절](Docs/TEST_REPORT.md#12-시간차-자동-전투-기획-검토)의 직접 확인이 필요하다.

2026-09-11 `ff22940`의 승계 PIE 3건·관리 계약 4건 성공은 이전 순차 전투의 이력이다. 새 라운드 전투·새 UI·피격·복구의 성공 근거로 사용하지 않는다. 기존 Cue fallback 경고와 상점·일반 협동 사용자 확인도 남아 있다.

## 작업 규칙

[AGENTS.md](AGENTS.md)를 기준으로 한다. 작동 테스트는 사용자 수행이 원칙이며 명시적 실행 요청에 한해 Codex가 수행한다. Visual Studio는 자동 실행하지 않는다. 변경은 검증 후 `[codex]` 제목과 한글 본문으로 커밋·push한다. 보고서의 결과·문제·권장 대응은 답변에도 요약한다.
