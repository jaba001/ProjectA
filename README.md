# ProjectA

Unreal Engine 5.8 기반의 파티·Grid 라운드 전투 로그라이트다. UI 중심으로 Run을 진행하고 파티 빌드를 성장시키며, 상대 Snapshot을 쓰는 Async PvP와 최대 4인 Co-op을 목표로 한다.

README에는 개요·실행·빌드·문서 안내만 둔다. 기능별 상세 규칙은 [GAME_STATUS](Docs/Codex/GAME_STATUS.md)·[PROJECT_PLAN](Docs/Codex/PROJECT_PLAN.md)·[UI_README](Docs/Codex/UI_README.md)·[MULTIPLAYER](Docs/Codex/MULTIPLAYER.md)에 있다. 완료 이력과 검증 근거는 커밋 본문으로 관리하며, 이전 HISTORY·기획 문서는 커밋 `28adbfd6`까지의 Git 이력에서 볼 수 있다.

## 게임 구성

| 영역 | 현재 구현 | 기준 문서 |
|---|---|---|
| Run | 새 싱글 Run은 인카운터 3회 → PvE → 인카운터 3회 → 로컬 Snapshot 전투를 10회 반복한다(60선택·10PvE·10Snapshot). 하나의 `Gameplay` 레벨에서 지하 미로 갈림길로 후보 3개 중 하나를 고른다. 성장·재화·장비는 다음 Run에 이월하지 않으며 Run 종료 시 MMR만 반영하는 정책이다(온라인 정산은 미구현). | [목표 Run](Docs/Codex/GAME_STATUS.md#2-run-진행) |
| 전투 | 라운드마다 행동을 계획한 뒤 속도 차이만큼 시간차를 두고 자동 실행한다. 이동에는 SAP, 스킬에는 AP를 쓰며 스킬은 GAS로 실행한다. 명중 확률 없이 서버 충돌로 판정한다. 준비가 완료되면 본인 캐릭터의 숄더뷰로 전환한다. | [전투 규칙](Docs/Codex/GAME_STATUS.md#6-라운드-전투) |
| 장비·스킬 | 장비 슬롯은 무기 2·방어구 4·목걸이 1·반지 2칸이다. 무기에는 5단계 고정 등급과, 태그 조건으로 추첨한 스킬 1개가 붙는다. 장착한 무기의 스킬만 사용하며 수호 스킬은 방패 전용이다. | [장비](Docs/Codex/GAME_STATUS.md#3-장비와-무기-스킬) · [등급](Docs/Codex/GAME_STATUS.md#3-장비와-무기-스킬) |
| 상점·인카운터 | 기본·등급별·태그별 상점과 회복의 샘물·소모품상점·부활소를 그룹 가중치로 제시한다. 상점은 구매·판매·리롤을 지원하고 직업별 NPC를 배치한다. | [상점 분류](Docs/Codex/GAME_STATUS.md#4-상점과-인카운터) |
| PvE 난이도 | 하·중·상을 고르며 각각 초원·석조 던전·얼음 성채에서 전투한다. 몬스터 편성·체력 곡선·골드는 CSV로 정하고 Run 생성 시 저장한다. | [난이도 규칙](Docs/Codex/PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터) |
| 저장 | 전투 밖에서는 일반 저장, 전투 중에는 Ready 경계마다 체크포인트를 쓴다. 저장에 실패하면 준비 완료를 막고 원본을 보존한다. 이전 저장의 카탈로그와 확정 결과도 보존한다. | [저장 경계](Docs/Codex/PROJECT_PLAN.md#5-저장과-멀티플레이-연결-경계) |
| 멀티플레이 | 같은 PC에서 Listen Server로 2·4인 개발 협동을 할 수 있다. 명시적인 Steam 480 개발 연결 시제품이 있다. PlayFab 연동·중앙 저장·실제 2PC 확인은 준비 대기다. | [멀티플레이 계약](Docs/Codex/MULTIPLAYER.md) |
| UI·현지화 | DemonicUI 공통 테마, 1920×1080 기준 DPI, 한국어·English 설정, Async Loading Screen을 사용한다. | [UI 구조](Docs/Codex/UI_README.md) |
| 백과사전 | 메인메뉴에서 장비·스킬·직업·상태이상·전투 공식·모험·몬스터를 탭과 검색으로 조회한다. | [화면·데이터 기준](Docs/Codex/UI_README.md#8-4-시작-메뉴-백과사전) |

남은 작업과 사용자 확인 항목은 [TODO](Docs/Codex/TODO.md)에서 관리한다.

## 실행

1. UE 5.8에서 `ProjectA.uproject`를 열고 `/Game/User_JeHoon/LEVEL/Core/MainMenu`를 연다.
2. **게임 시작 → 싱글플레이**에서 캐릭터를 1~4명 만들고, 직접 조작할 1명을 선택한 뒤 Start Game을 누른다.
3. 전투는 **대상 클릭 → 장착 스킬 선택 → 준비 완료** 순서로 진행한다. 이동은 **이동 예약 · SAP 1 → 아군 빈칸 클릭**으로 예약한다. 스킬 없이 준비 완료하면 행동하지 않고 턴을 넘긴다.
4. Gameplay에서 `I`는 장비·가방, `Esc`/`O`는 설정을 연다. 기본 PIE에서는 `Esc`가 에디터 종료에 쓰이므로 단축키는 독립 게임 창에서 확인한다.

| 진입점 | 용도 |
|---|---|
| `/Game/User_JeHoon/LEVEL/Development/DebugCombat` | 전투 디버그. 스킬·장비·캐릭터/체력·스킬 타이밍 탭을 제공한다. [상세](Docs/Codex/PROJECT_PLAN.md#4-2-전투-디버그-레벨) |
| `/Game/User_JeHoon/LEVEL/Environment/` | 환경 12맵과 지하 던전 2맵의 비교 레벨. [맵 목록](Docs/Codex/PROJECT_PLAN.md#4-4-환경-비교-레벨) |
| 독립 게임 창 2개 → 멀티플레이 | 개발 협동. 첫 창이 방을 만들고 두 번째 창이 `127.0.0.1:7777`로 참가한다. [실행 명령](Docs/Codex/MULTIPLAYER.md#11-개발-실행-참조) |
| `UnrealEditor-Cmd -run=BalanceSim -nullrhi` | 밸런스 측정용 자동 Run. [인자·실행 명령](Source/ProjectAEditor/Scripts/README.md#34-밸런스-측정-commandlet). 사용자 실행 확인 대기. |
| `-CustomConfig=SteamDev -ProjectASteamDev` | Win64 Development 게임 전용의 Steam 480 연결 확인 패널. 기본 설정은 Steam 비활성이다. |

외부 에셋 팩은 Git에 포함되지 않으므로 다른 PC에서는 같은 경로에 따로 설치해야 한다. [에셋 구성](Docs/Codex/PROJECT_PLAN.md#6-gameplay-에셋과-배치)

## 개발 환경과 빌드

- 엔진: Unreal Engine 5.8.3, C++, GAS, CommonUI, UMG. 타깃은 `BuildSettingsVersion.V7`·`EngineIncludeOrderVersion.Unreal5_8`을 사용한다.
- 모듈: `Source/ProjectA`는 런타임, `Source/ProjectAEditor`는 에셋 도구와 에디터 테스트다. 런타임 모듈에는 Editor 의존성을 추가하지 않는다.
- 제작 에셋은 `Content/User_JeHoon`(`/Game/User_JeHoon`)에 두고, 외부 리소스는 원본을 직접 참조한다. 에셋 도구는 [Scripts README](Source/ProjectAEditor/Scripts/README.md)를 따른다.

Development Editor / Win64 빌드:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ProjectAEditor Win64 Development -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64
```

C++ 파일을 추가·삭제하거나 이름을 바꾼 뒤에는 프로젝트 파일을 재생성한다:

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -ProjectFiles -Project="C:\Users\jaba0\Desktop\MyProjects\ProjectA\ProjectA.uproject" -Game -Engine
```

`Build.bat`이 UE 5.8에 번들된 .NET을 선택하므로 UnrealBuildTool을 직접 호출하지 않는다. 현지화 정적 검사는 `python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`로 실행한다. C++·리플렉션을 변경한 뒤에는 UE를 재시작한다.

## 문서

Codex 문서는 `Docs/Codex/`, Claude 문서는 `Docs/Claude/`에 보관한다.

| 문서 | 내용 |
|---|---|
| [GAME_STATUS](Docs/Codex/GAME_STATUS.md) | 현재 구현된 게임 규칙과 진행 상황 요약 |
| [PROJECT_PLAN](Docs/Codex/PROJECT_PLAN.md) | 모듈·전투·UI·에셋·설정의 현재 구조 |
| [UI_README](Docs/Codex/UI_README.md) | UI 구조·JSON 명세·생성 옵션 |
| [MULTIPLAYER](Docs/Codex/MULTIPLAYER.md) | 식별·소유권·저장·네트워크 계약 |
| [TODO](Docs/Codex/TODO.md) | 남은 작업·미결정 제안·사용자 확인 |
| [Docs/Claude](Docs/Claude/README.md) | Claude의 제안·인계 문서 |

스크린샷과 갤러리는 `Docs/ScreanShoot/`에 로컬 보관하며 Git에서 제외한다. 새 clone에는 포함되지 않는다.

`DataCatalogs/`에는 CSV 14개가 있다. 새 기본 Run은 아래에서 *표시한 10개를 생성 시 읽어 결과를 저장한다. 그래서 CSV를 수정하면 그 뒤에 만드는 새 Run에만 반영되며 기존 저장은 바뀌지 않는다.

| CSV | 내용 |
|---|---|
| [WEAPON_ASSETS](DataCatalogs/WEAPON_ASSETS.csv)* | 장착 지원 무기·방패 49개의 종류·경로·가격·이름·고정 등급 |
| [ITEM_RARITY_PROBABILITIES](DataCatalogs/ITEM_RARITY_PROBABILITIES.csv)* | 5등급 기본 등장 확률 50·30·15·4·1% |
| [SKILL_BALANCE](DataCatalogs/SKILL_BALANCE.csv)* | 무기 후보 스킬 62종의 등급·가중치·위력·AP/SAP·선딜 |
| [SKILL_RARITY_PROBABILITIES](DataCatalogs/SKILL_RARITY_PROBABILITIES.csv)* | 장비 5등급×스킬 5등급 가중치 |
| [ENCOUNTER_POOL](DataCatalogs/ENCOUNTER_POOL.csv)* | 인카운터 활성 13개·비활성 15개의 그룹/변형 가중치·상품 필터 |
| [MONSTER_STATS](DataCatalogs/MONSTER_STATS.csv)* | 몬스터 13종의 태그·HP/AP/SAP/속도·원본 스킬 |
| [MONSTER_SPAWN_PROBABILITIES](DataCatalogs/MONSTER_SPAWN_PROBABILITIES.csv)* | 활성 12종의 출현 가중치(합 100) |
| [MONSTER_ENCOUNTERS](DataCatalogs/MONSTER_ENCOUNTERS.csv)* | 10묶음의 지역·선봉·적 수·배율·골드·성장·휴식 |
| [PVE_DIFFICULTIES](DataCatalogs/PVE_DIFFICULTIES.csv)* | 하·중·상 속도·골드 배율 |
| [PVE_HEALTH_CURVE](DataCatalogs/PVE_HEALTH_CURVE.csv)* | 10묶음×3난이도의 기준 HP |
| [MONSTER_ASSETS](DataCatalogs/MONSTER_ASSETS.csv) | 몬스터 13종의 원본 팩·Blueprint·메시 경로(참고 자료) |
| [SKILL_EFFECT_ASSETS](DataCatalogs/SKILL_EFFECT_ASSETS.csv) · [SKILL_CREATION_STATUS](DataCatalogs/SKILL_CREATION_STATUS.csv) · [SKILL_SOUND_ASSETS](DataCatalogs/SKILL_SOUND_ASSETS.csv) | VFX·스킬 생성 현황·SFX 원본 목록과 연결 기록 |

무기 이름이 겹치면 행 순서대로 `이름 1`, `이름 2`처럼 번호를 붙인다. 이 번호는 등급이나 강화 단계와 무관하다. [표시명 규칙](Docs/Codex/PROJECT_PLAN.md#4-1-스킬-이펙트-에셋-목록)

## 작업 규칙

[AGENTS.md](AGENTS.md)를 공통 기준으로 한다. Codex와 Claude가 함께 작업하며 커밋 제목은 각각 `[codex] `와 `[claude] `로 시작한다. 작동 테스트는 사용자가 수행하고, 에이전트는 컴파일과 정적 검사까지 한다. 아이템·적·스킬·효과 확장은 GAS·GameplayTag 분류와 데이터 기반 가중치를 원칙으로 한다.
