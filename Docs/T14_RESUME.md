# T14 · 명시적 Host 승계와 AI 이어하기 구현 메모

[작업 대기열](T14_QUEUE.md) · [확정 기획](T14_COOP_DESIGN.md) · [아군 AI](T14_PARTY_AI.md) · [턴 저장·복구](T14_CHECKPOINT.md)

## 현재 단계

6번 로컬 개발 범위의 구현·검증을 완료했다. 최초 합류 번호순 승계, 사전 동의 없는 Host 단독 AI 전환, 해당 Run 종료까지 AI 유지(3C)를 적용한다. 2·3·4번이 혼자 재개할 때는 본인이 Host가 되고 나머지를 AI로 전환한다. 현재 Host만 노드 선택과 승리 Continue를 실행한다(5A).

관리 Run v4 저장, 명시적 획득·재개, 메뉴의 싱글 전환, Gameplay 복구와 전투 권위 검증을 코드에 연결했다. 실제 관리 PIE 2건과 전체 회귀 75건, 별도 기존 AI 복구 3건·Snapshot 전투 1건을 통과했다. 이 결과는 로컬 개발 범위의 6번 완료이며 실제 서비스 검증과 구분한다. 8번의 실제 Steam 인증·PlayFab 공동 저장·서비스 연동은 미구현이다.

## 확정한 번호와 호환 계약

최초 Host는 `JoinOrdinal=1`, 이후 최초 합류 순서대로 2·3·4를 저장한다. Identity schema 2는 1부터 참가자 수까지 중복·빈 번호 없이 유지하며 최초 HostEpoch 1의 Host는 1번이어야 한다. 승계 이후 Host는 다른 번호일 수 있다. 번호는 파티 캐릭터 슬롯이나 실행 중 접속 순서가 아니다.

새 일반 싱글 Run은 Identity schema 2·번호 1로 생성한다. 기존 Identity schema 1은 번호 0으로 읽어 기존 복구를 유지하되 배열 인덱스로 최초 순서를 추측하지 않는다. 번호가 없는 기존 저장은 번호 기반 승계 후보 조회를 거절한다.

Run SaveGame 외부 v1/v2/v3와 Opponent Snapshot v1은 유지한다. 관리 Run만 외부 v4를 사용하며 Identity schema 2, 원래 참가자 2~4명, `LocalDevelopment` 출처와 영속 Human 목록을 요구한다. 전투 중 v4는 확정 전투 본문을 포함하고 지도·결과·종료 상태는 참여 목록을 남긴 채 전투 본문을 비운다. 일반 이어하기와 `LoadCheckpoint`는 v4를 거절하며, 기존 협동 저장을 관리 v4로 자동 이관하거나 전투별 AI 플래그에서 Human 이력을 추정하지 않는다.

`TryGetJoinOrdinal`과 `TrySelectHostCandidate`는 유효한 Identity/Party 및 명시한 인간 원래 참가자 집합의 구조를 검사하는 순수 조회다. 현재 접속·온라인 인증·실행 권한은 증명하지 않는다. 관리 재개의 `ValidateTransition`은 이전 확정 Human 목록과 비교하여 이미 AI로 전환된 계정의 재추가를 거절하고, 남길 인간 중 가장 작은 최초 번호가 새 Host인지 확인한다.

## 정책과 독립적인 기반

`FRunParticipationData`는 Human으로 참여하는 원래 계정 목록을 Actor 없이 표현한다. 구조 검증은 원래 참가자·중복·Host의 Human 참여를 확인하며 개인 사전 동의는 요구하지 않는다. 캐릭터 모드는 원래 소유자로부터 조회하며 사망만으로 참가 상태가 바뀌지 않는다. 재개 전후 목록 비교는 별도 전이 검증이 담당한다. 이 DTO와 검증은 접속 인증이나 실제 MMR 참여 판정을 대신하지 않는다.

로컬 권위 저장소 대역은 Run별 전체 저장 본문과 저장소 revision·HostEpoch·실행 세션을 하나의 `USaveGame` 기록으로 확정한다. 저장소 revision은 전투 턴 revision과 별도로 해당 Run의 모든 저장 순서를 나타낸다. 같은 개발 저장소를 보는 실행들이 정확한 이전 stamp로만 갱신·재개하도록 하고, 실행 중에는 배타적인 OS 파일 핸들을 유지한다. 별도의 짧은 파일 잠금으로 읽기·비교·교체를 직렬화한다. 기존 `FRunCheckpointStorage`의 임시 파일·flush·재검증·교체를 재사용한다.

실행 lease 해제는 진행 권한을 놓는 동작이다. 그 자체로 Host를 바꾸거나 AI를 선택하지 않는다. 프로세스 종료로 파일 핸들이 해제된 후에도 새 세대는 명시적인 승인·재개 동작에서만 만든다. 이 개발 대역은 동일 PC의 공유 저장 경로를 대상으로 하며 실제 원격 중앙 저장·온라인 신원·네트워크 장애 판정·위변조 방지를 구현한 것이 아니다.

## 관리 Run 생성·재개·종료 계약

[RunStateSubsystem](../Source/ProjectA/Game/Run/RunStateSubsystem.h)이 일반 저장과 관리 저장을 분기한다. 관리 저장은 현재 Host의 lease로 기준 저장소에만 기록하며 실패하면 일반 SaveSlot으로 우회하지 않는다.

| API | 실행 조건과 확정 시점 |
| --- | --- |
| `ConfigureLocalDevelopmentCaller` | 신뢰하는 C++ 개발 경로가 `Development` 계정과 저장소 namespace를 주입한다. 같은 GameInstance에서 다른 계정이나 namespace로 바꿀 수 없다. |
| `CreateManagedRun` | 번호를 가진 원래 참가자 2~4명, 최초 Host 1번·epoch 1, 호출자=Host로 새 v4 기록과 lease를 만든다. 처음에는 원래 참가자 모두 Human이며 저장 성공 뒤에만 현재 Run을 교체한다. |
| `ReadManagedRun` | 원래 참가자인 개발 호출자가 기준 기록과 stamp를 조회한다. 조회만으로 현재 Run 변경이나 실행 권한 획득이 발생하지 않는다. |
| `ResumeManagedRun` | 기존 실행이 닫혀 있어야 한다. 최신 stamp, 이전 Human 목록의 부분집합, 번호순 Host를 검증한 뒤 새 HostEpoch·Human 목록·전투 Identity/모드를 한 본문으로 원자적으로 획득한다. 실패하면 파일·현재 Run·lease를 바꾸지 않는다. |
| `BeginManagedMenuTravel` | 재개 획득 후 GameInstance 수명으로 엔진 TravelFailure를 감시한다. 같은 실행 SessionId·현재 WorldContext의 실패만 lease를 해제하고 대상·호출자·확정 저장을 보존한다. 성공한 복구·종료·Deinitialize에서는 구독을 해제한다. |
| `ConfirmManagedResumeStarted` | 유효 lease를 가진 Gameplay 복구가 성공했을 때만 재개 대기를 해제한다. 전투 복구 전까지 Human 명령·노드·Continue를 차단하며, 복원 준비와 서버 내부 확정 턴 저장은 허용한다. |
| `CloseManagedRun` | 먼저 Encounter와 콜백을 중단한 뒤 lease와 활성 Run 상태를 비운다. 개발 호출자 문맥과 재개 대상은 GameInstance에 남긴다. 종료 자체가 새 Host를 정하거나 추가 AI를 선택하지 않는다. |

재개 획득은 Run과 CombatCheckpoint의 Identity를 함께 갱신한다. 원래 참가자·최초 번호·캐릭터 ID·소유자·AttemptId·마지막 확정 턴은 유지하며, 같은 턴의 승계를 다음 턴 저장으로 처리하지 않는다. 저장소 revision은 획득과 이후 저장마다 증가하고 새 실행 SessionId를 발급한다. Runtime 전투 식별자·참가자 바인딩·AI 조작 세션도 새로 만들어 이전 요청을 재사용할 수 없게 한다.

Gameplay의 `ResumeManagedGameplay`는 지도·승리 결과를 복원하거나 `RestoreSavedCombat`을 실행한 뒤 대기를 확정한다. 복원 실패는 lease와 대기 상태를 유지하고 원인을 표시한다. 유효 lease를 가진 로컬 Host만 복원 재시도를 할 수 있다. 저장 실패로 중단된 전투는 기존 확정 턴 저장 재시도 경로를 사용한다. Gameplay 월드 종료는 `ShutdownGameplay` 후 `CloseManagedRun` 순서를 지킨다.

## Human 연결·영구 AI와 전투 권위

GameMode는 현재 영속 Human 목록에 있는 원래 참가자만 신뢰 배정하고, 그 인간 모두의 서버 연결을 요구한다. 원래 인원이 2~4명이었어도 Human이 본인 Host 한 명이면 Standalone 복구를 허용한다. Listen Server에서는 현재 Human의 연결이 부족하면 멈추며 자동으로 AI 전환하지 않는다. AI로 남긴 원래 소유자의 연결을 Human으로 배정하지 않는다.

Encounter는 새 전투와 복구 모두 원래 소유자와 영속 참여 목록에서 Human/ServerAI를 결정한다. 저장된 전투 모드가 이 목록과 다르면 스폰 전에 거절한다. 전투 결과가 체크포인트 본문을 비운 뒤에도 v4의 참여 목록이 남으므로 Continue와 다음 전투에서 AI 상태를 유지한다. 새 Host는 다른 캐릭터의 소유권을 얻지 않는다.

[CombatActionAuthority](../Source/ProjectA/Combat/Commands/CombatActionAuthority.h)는 관리 실행 여부를 설정 때 기록하고 `Reset`으로 해제하지 않는다. 관리 Run을 일반 설정으로 낮추는 호출을 거절하며, 현재 lease·로컬 호출자 Host·전체 Identity·실행 SessionId를 명령마다 확인한다. Human 명령과 소유권 조회는 재개 대기 중 거절한다. 연결 배정·AI 모드 설정·서버 AI는 유효 lease를 가진 복원 준비 중 허용하지만 영속 Human 목록과 일치해야 한다.

직접 `CloseManagedRun`을 잘못 호출해 옛 전투 Actor가 남아 있어도 옛 권위 객체의 Human/AI 명령은 거절한다. 이 방어가 Actor 정리를 대신하지는 않는다. 정상 종료는 먼저 전투·Enemy/Party AI·능력 콜백을 중단하고 lease를 반환한다. 로컬 nonce 검증만으로 원격 중앙 권위를 구현했다고 취급하지 않는다.

## 메뉴와 개발 진입 범위

기존 일반 `이어하기`와 별도 관리 재개 패널을 사용한다. 패널은 신뢰하는 개발 경로가 호출자와 `SetManagedResumeTarget`을 설정한 경우에만 표시한다. 계정·Run ID를 사용자가 입력하여 본인을 주장하는 UI나 자동 계정 생성·저장 파일 검색 기능은 없다. 일반 MainMenu 실행에 개발 문맥을 임의로 부여하지 않는다.

- **싱글로 전환하기**: 현재 Human이 여러 명이고 본인이 그 목록에 있는 경우 표시한다. 본인이 Host, 나머지는 해당 Run 종료까지 AI이며 인간 조작으로 돌아올 수 없음을 안내한다. 클릭 시 최신 기록을 다시 조회하고 `{본인}` 목록으로 획득한 뒤 Gameplay로 이동한다.
- **싱글 진행 이어하기**: 이미 본인만 Human인 관리 Run에 표시한다. 같은 영구 AI 목록으로 새 실행을 획득한다. 기존 일반 싱글 저장 버튼을 재사용하지 않는다.
- 메뉴에서 Gameplay로 이동하다 실패하면 확정된 Host·AI를 되돌리지 않고 실행 lease를 해제한다. 기존 메뉴 Controller가 파괴되어도 같은 GameInstance의 새 메뉴에서 최신 기록으로 다시 이어갈 수 있다. Gameplay에 도착한 뒤 전투 복원 실패는 별도로 lease·pending을 유지하는 복원 재시도 경로를 따른다.
- 이미 AI인 본인, 종료된 Run, 없는·손상된 기록, 진행 중 lease, 오래된 stamp는 재개를 허용하지 않는다. 노드 선택·승리 Continue는 현재 Run Host로 신뢰 배정된 로컬 서버만 실행한다.

현재 연결 배정은 [관리 PIE fixture](../Source/ProjectAEditor/Tests/ManagedRunPIETests.cpp)의 명시적인 C++ 계정 매핑이다. 접속 순서를 영속 계정으로 간주하지 않으며 `AccountProvider` 저장을 개발 관리 Run으로 승격하지 않는다. 8번의 실제 Steam 로그인, PlayFab 공유 기록/인증, 로비·초대, 다른 PC 사이의 승계, MMR은 이 경로에 포함하지 않는다. 신뢰할 수 있는 공급자 연동 전에는 개발 fixture 바깥에서 이 기능을 온라인 재개 서비스처럼 노출하지 않는다.

## 기반 검증 — 2026-09-10

아래는 관리 Run 통합 이전 기반 단계의 당시 검증 기록이다. 이후 정책에서 개인 사전 동의 요구를 제거했으며, 이 기록을 새 v4 통합의 성공 증거로 사용하지 않는다.

- Development Editor / Win64 빌드와 전체 회귀 60건 통과: 성공 40건·경고 동반 성공 20건·실패 0건. 신규 참여 데이터 1건과 저장소 3건을 포함하며 기존 2인 전투·체크포인트와 Gameplay 저장 맵 PIE도 통과했다.
- 참여 구조·원래 소유자별 모드·사전 동의·사망한 인간 참가자 보존·네이티브 직렬화, 같은 프로세스의 중복 실행·오래된 revision·손상 기록·쓰기 실패 시 파일/출력/lease 보존을 확인했다.
- 별도 프로세스 검증을 추가한 뒤 다시 빌드했다. Holder가 실행 권한을 보유하는 동안 다른 BusyReader 프로세스의 읽기는 성공하고 획득은 거절됐다. 해당 테스트 Holder만 강제 종료한 뒤 ResumeReader가 epoch 2/revision 2를 명시적으로 획득하고 revision 3 저장을 확정했다. 기존 stamp 재사용은 거절됐다. 두 Reader 테스트는 각각 경고 없이 1건 통과했다.
- 마지막 저장소 집중 테스트 4건도 경고 없이 통과했다. 전용 인자가 없는 `ProcessProbe`가 대기하지 않고 종료하는 경로를 포함한다. 문서 9개의 로컬 링크 124개와 diff 검사를 통과했다.
- 위 프로세스 검증은 공유 로컬 저장소의 실행 권한만 검증한다. 실제 Host 계정 변경·참여 승인·AI 전환·새 Listen Server의 Run 복구는 아래 통합 범위로 남는다. 전체 회귀의 경고는 기존 격리 월드·미설정 Spawn/GAS Cue·의도한 이동 실패·PIE 종료 경로 등이다.

검증 파일: `Saved/Automation/T14ResumeFoundationBuild2.log`, `T14ResumeFoundationFull1/index.json`, `T14ResumeAuthorityBusyReader/index.json`, `T14ResumeAuthorityResumeReader/index.json`, `T14ResumeAuthorityFocused/index.json`. Holder는 종료 후 핸들 해제를 검증하기 위해 테스트 프로세스를 강제 종료했으므로 완료 보고서 대신 `Saved/Logs/T14ResumeAuthorityHolder.log`를 남긴다.

프로세스 테스트는 `ProjectA.RunAuthority.ProcessProbe`에 `-T14AuthorityProbe=Holder|BusyReader|ResumeReader`, 공통 `-T14AuthorityNamespace=<안전한 이름>`, `-T14AuthorityRunId=<GUID>`, `-T14AuthorityProbeId=<GUID>`를 지정한다. Holder의 `Saved/Automation/T14Authority_<ProbeId>.ready`를 확인한 뒤 BusyReader → 해당 Holder 종료 → ResumeReader 순서로 각각 별도 Editor-Cmd 프로세스에서 실행한다. 전용 인자가 없는 일반 실행에서는 안내만 출력하고 대기하지 않는다. Holder는 외부 종료가 없으면 180초 뒤 실패하며 잠금을 해제한다.

## 관리 통합 검증

신규 검증의 성공 수와 보고서 경로는 최종 실행 후 별도로 기록한다. 아래는 구현한 테스트의 의도와 실행 범위이며 통과 결과를 뜻하지 않는다.

- [네이티브 관리 Run 테스트](../Source/ProjectA/Tests/ManagedRunTests.cpp) `ProjectA.Run.Managed`는 생성·lease 중복·최신 stamp·획득/저장 실패 보존·2/3/4번 단독 전환·번호순 재개·영구 AI 복귀 거절·결과/Continue 저장·v4 일반 로드 우회 거절과 기존 형식 호환을 검사한다.
- `ProjectA.ManagedRunPIE.HostSuccession`: 실제 최초 3인 세션에서 전투를 저장하고 종료한 뒤, 원래 2번 Host와 3번 Client만 있는 2인 Listen Server로 재개한다. 1번 원래 Host는 AI로 남으며 두 화면의 턴·HP/AP·Grid·소유권, 실제 AI 행동, 새 저장, 결과 Continue와 다음 전투의 영속 모드를 검사한다.
- `ProjectA.ManagedRunPIE.SoloMenuConversion`: 최초 4인 세션을 종료한 뒤 원래 4번의 실제 MainMenu 전환 버튼을 통해 Standalone Gameplay 복구를 실행한다. 나머지 세 캐릭터의 AI 유지와 새 확정 저장·결과/다음 전투를 검사한다.
- 두 PIE는 마지막 새 전투에서 직접 lease 종료 후 옛 Human/AI 요청·바인딩·모드 변경 거절과 전투 상태 불변을 검사한다. 일반 종료 검증은 `ShutdownGameplay` 후 lease 해제 순서를 사용한다.

관리 PIE의 실제 실행에는 `-T14ManagedRunPIE`와 해당 테스트 선택이 필요하다. 전용 인자가 없는 일반 자동화에서는 안내 후 종료하므로 그 성공 집계만으로 실제 승계 세션 검증을 주장하지 않는다. fixture는 고유 로컬 저장소 namespace와 Run ID를 생성하고 에디터 PIE 설정을 복원한다. 기반의 별도 프로세스 잠금 검증, 7번의 3·4인 동시 전투/복구 검증, 이번 관리 승계 PIE는 각각 검증 대상이 다르다.

2026-09-10 최종 Editor Build3 성공, 전체 `T14ManagedRunFull1` 75건(성공 51·경고 동반 성공 24·실패 0), 실제 관리 `T14ManagedRunPIE2` 2건·기존 AI `T14ManagedRunAI1` 3건·Snapshot `T14ManagedRunSnapshot1` 1건 모두 경고 동반 성공했다. 실제 메뉴의 이동 실패·새 메뉴 재시도와 화면 배치까지 확인했다. 최초 fixture 이름 길이 실패와 상세 항목은 [대기열](T14_QUEUE.md)에 기록했다. 이 로컬 6번 결과를 다른 PC·실제 계정·중앙 MMR 검증으로 취급하지 않는다.
