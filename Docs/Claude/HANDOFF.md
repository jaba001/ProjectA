<!-- author: Claude · owner: Claude · Codex는 4절 처리 상태 줄만 수정한다 -->
# Claude 변경 인계

Claude가 컴파일·커밋을 직접 할 수 없을 때만 이 문서로 Codex에 인계한다. 2026-10-11부터 Claude는 Visual Studio 내장 Git과 UE 5.8 `Build.bat`으로 직접 처리한다([CLAUDE.md](../../CLAUDE.md)). 1~4절은 첫 인계 기록이다. 처리 절차는 [AGENTS.md의 작성자별 문서 분리](../../AGENTS.md#작성자별-문서-분리)를 따른다.

## 1 2026-10-11 변경 요약

[리뷰](REVIEW_2026-10-11.md) 3-1·3-2·3-8·3-9를 수정했다. 동작 정책은 바꾸지 않았다.

| 검증 | 결과 |
| --- | --- |
| `python Source/ProjectAEditor/Scripts/BuildLocalization.py --check` | 통과. 번역 1095→1318개, ko/en locres 재생성 |
| 변경 문자열을 비교하는 테스트 확인 | 런타임 테스트 0곳, 에디터 테스트 2곳을 같은 키의 `NSLOCTEXT` 비교로 수정 |
| Development Editor / Win64 컴파일 | **미실행** |
| Native·PIE 테스트 | **미실행** |

## 2 변경 내용

### 2-1 현지화되지 않은 문구

- 런타임 모듈(테스트 제외)의 `FText::FromString(TEXT("한글"))`, `RoundText(TEXT("..."))`와 같은 형태의 삼항식 232곳(고유 키 223개)을 `NSLOCTEXT("<파일명>Text", "M_<CRC32>", "원문")`으로 바꿨다. 키는 원문 UTF-8의 CRC32라서 같은 원문은 같은 키가 된다.
- 영어 번역은 새 카탈로그 `Config/Localization/RuntimeMessages.json`에 있다. "English / 한글" 형식의 원문은 영어 부분만 번역으로 사용했다.
- `CombatRoundCoordinator.cpp`에서 쓰지 않게 된 `RoundText` 함수를 삭제했다.
- 남은 항목: `FString::Printf`를 조합하는 디버그·개발 화면 문구 약 15곳(`CombatDebugWidget`, `CombatRoundDebug`, `SteamDevelopmentLobby`, `DevelopmentCoopSubsystem`)과 기존 이중 언어 노드 이름(`RunProgressRules`)은 바꾸지 않았다.
- 테스트: `ExistingSaveContinueReviewTests.cpp`, `SnapshotEncounterFailureTests.cpp`의 기대 문구를 같은 네임스페이스·키의 `NSLOCTEXT`로 바꿔 언어 설정과 무관하게 비교하도록 했다.

### 2-2 32비트 시드

- `Types/ProjectARandom.h`를 추가했다. `ProjectARandom::NewSeed()`는 `FGuid::NewGuid()`의 해시로 32비트 시드를 만든다.
- 바꾼 곳: `RunLevelDesign.cpp`, `RunEncounterPool.cpp`, `RunStateTarget.cpp`, `RunItemShopCatalog.cpp`, `RunStateSubsystem.cpp`, `RunEncounterPoolDataAsset.cpp`.
- 시드 저장 형식(`int32`)과 이미 저장된 시드의 재현은 바뀌지 않는다. 음수 시드가 나올 수 있으며 `FRandomStream`에서 유효하다.

### 2-3 네이티브 태그

- `ProjectACombatTags::Data_Damage`와 `Event_Attack_Release`를 추가했다. `DefaultGameplayTags.ini`의 같은 태그는 유지한다.
- 바꾼 곳: `GE_Damage.cpp`, `CombatEffectLibrary.cpp`, `GA_AttackBase.cpp`, `AN_SkillRelease.cpp`.

### 2-4 로그 카테고리

- 런타임 모듈(테스트 제외)의 `UE_LOG(LogTemp, ...)` 91곳을 기존 `LogProjectA`로 바꾸고, 필요한 파일에 `#include "ProjectA.h"`를 추가했다. 테스트 코드의 `LogTemp`는 그대로 두었다.
- 로그 필터를 `LogTemp`로 걸어 두었다면 `LogProjectA`로 바꿔야 한다.

## 3 Codex 처리 절차

1. Development Editor / Win64로 컴파일한다. 실패하면 원인을 수정하고 4절에 기록한다.
2. `python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`를 실행한다.
3. 위 파일만 스테이징하여 제목 `[claude] 현지화·시드·네이티브 태그·로그 카테고리 정리`로 별도 커밋한다. 본문에는 검증 결과를 적고 Codex 작업과 같은 커밋에 섞지 않는다.
4. 작동 확인은 사용자에게 인계한다. 영어 설정에서 전투 상태·준비 메시지가 영어로 나오는지, 새 Run을 여러 번 시작했을 때 배치가 달라지는지 확인한다.

## 4 처리 상태

- 컴파일: 2026-10-11 Development Editor / Win64 통과(66개 작업, 61.05초, 종료 0). `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 실행. 수정 없이 성공했으며 로그는 `Saved/Automation/ClaudeHandoff/Build.Editor.log`에 보존한다.
- 현지화·정적 검사: `python Source/ProjectAEditor/Scripts/BuildLocalization.py --check` 종료 0, 번역 1,318개·ko/en 리소스·한국어 원문 대체 일치. Python 3.12.14를 현재 명령의 PATH에 지정했으며 시스템 설정은 변경하지 않았다. 전체 staged diff 54개·`git diff --check` 통과. 로그는 `Saved/Automation/ClaudeHandoff/Localization.Check.log`다.
- 커밋: `0ac5e5a3b3b9fca6c91fc3a02cd06fa091a88c5b` `[claude] 현지화·시드·네이티브 태그·로그 카테고리 정리`, `origin/main` push 완료. 현지화·시드·태그·로그 관련 54개 파일만 포함했다. 기존 협업 규칙·Claude 문서와 기능에 불필요한 `ProjectAAssetManager.cpp` 헤더 추가는 제외했다. 이 인계 문서는 기존 미추적 상태를 유지하며 처리 상태만 로컬 갱신한다.
- 사용자 작동 확인: 미확인
