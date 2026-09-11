# 사용자 작동 테스트 보고서

갱신일: 2026-09-11 · 대상: 2인 전투 동기화 / 콘텐츠 경로 통일 / T14-7 승계 통합·기본 전투 입력 / T14-8 서비스 준비 · **작동 검증 상태: F의 2인 전투 자동화 통과, 나머지 기존 대기 유지**

2026-09-11 사용자 확인: A/B 작동 테스트는 아직 실행하지 않았고 Steam App ID·PlayFab Title도 미준비다. 아래 A~D의 대기 상태를 유지한다. 서비스 준비 문서 정리 당시에는 문서·설정·엔진 소스만 정적으로 확인했다. 이후 콘텐츠 경로 변경의 검증은 E에 별도로 기록한다.

## 2026-09-10 변경과 확인 결과

| 구분 | 내용 | 상태 |
|---|---|---|
| 문서 정리 | Docs 14개를 기획·구조·작업·멀티플레이·테스트·이력의 6개로 통합 | 내용·로컬 링크 50개 정적 확인 완료 |
| 중단 전 작성한 테스트 코드 | 4명 중 최초 Host 불참 후 3명 재개, 각 Client 바인딩·턴 종료 RPC 승인 확인 추가 | 작성 완료, 사용자 작동 검증 대기 |
| 테스트 안정화 | 참가자 바인딩 복제를 기다린 뒤 비교, Unity 빌드의 중복 보조 함수명 수정 | 컴파일 확인 완료 |
| 컴파일 | Development Editor / Win64, `-DisableAdaptiveUnity` | 성공: `Saved/Automation/T14ManualHandoffBuild1.log` |
| 이번 작동 테스트 | PIE·게임·Unreal 자동화·패키지 실행 | Codex 미실행, 사용자가 아래 절차로 확인 |

앞으로 Codex는 컴파일·정적 검사를 수행하고 이 보고서를 갱신한다. 작동 테스트는 사용자가 실행하며, 결과를 받기 전에는 성공으로 표시하지 않는다. 이전 실행 결과는 [HISTORY](HISTORY.md)의 당시 코드 기준 기록이며 최신 변경의 통과를 대신하지 않는다.

## 준비 조건

- UE 5.7에서 최신 소스와 빌드된 `ProjectAEditor`를 사용한다. Visual Studio 실행은 필요하지 않다.
- 일반 플레이 확인은 `/Game/User_JeHoon/LEVEL/MainMenu`에서 시작한다. 프로젝트 기본 설정과 에셋 경로는 [PROJECT_PLAN](PROJECT_PLAN.md)을 따른다.
- 일반 새 게임은 싱글플레이다. **관리 Run의 협동 생성·참가 번호 선택·실제 Steam 로그인 UI는 아직 없다.** 관리 이어하기 패널은 C++ 개발 fixture가 호출자와 저장 대상을 설정해야 표시된다.
- 아래 관리 시나리오는 같은 PC의 개발 신원·로컬 저장소와 여러 PIE 월드를 사용하는 기존 자동화 fixture다. 실제 친구 초대·다른 PC·Steam 인증 시험과 구분한다.
- 일반 새 게임은 기존 단일 진행 슬롯을 갱신한다. 보존할 세이브가 있으면 실행 전에 `Saved/SaveGames`를 별도 위치에 복사한다. 관리 fixture는 고유 namespace/Run ID를 사용한다.

## A. 직접 플레이 확인

| ID | 사용자가 할 일 | 기대 결과 | 결과 |
|---|---|---|---|
| A1 | MainMenu → 새 게임 → 캐릭터 1~4명 생성 → 시작 → 첫 Combat 노드 선택 | Gameplay에 생성한 파티가 등장하고 현재 턴·HP/AP가 표시됨 | 대기 |
| A2 | 자기 턴에 Move 선택 → 표시된 빈 도달 타일 클릭 | 실제 이동·점유 타일·자원이 갱신되고 완료 후 다음 입력이 가능함 | 대기 |
| A3 | 사용 가능한 스킬 선택 → 유효한 적/타일 클릭 | 타겟 표시와 실제 효과·HP/AP가 일치하고 스킬 완료 후 입력이 풀림 | 대기 |
| A4 | 적 턴과 이동/스킬 처리 중 Move·스킬·타일 클릭을 시도하고 정상 진행을 기다림 | 유효하지 않은 행동은 실행되지 않고 적 행동 완료 후 자기 턴에 입력이 다시 가능함 | 대기 |
| A5 | 전투 승리 → Continue → 두 번째 Combat 노드 선택 | 같은 Gameplay에서 다음 전투가 시작되고 파티·HP·노드 진행이 유지됨 | 대기 |

막힌 항목이 있으면 이후 결과를 억지로 완료하지 않고 최초 실패 단계·화면·로그를 기록한다. 원래 문제인 ‘버튼 또는 타일 무반응’은 A2/A3에서 버튼 선택 여부, 범위 표시 여부, 실제 이동/효과 여부를 나누어 적는다.

## B. 사용자가 실행할 승계 시나리오

다음 세 fixture를 한 번 실행하면 B1~B3을 확인한다. 아래 명령은 **사용자 실행용 안내**이며 Codex는 실행하지 않는다. 기존 에디터의 작업을 저장하고 PIE를 종료한 뒤 PowerShell에 붙여 넣는다. 보고서 경로는 실행 시각마다 새로 만든다.

```powershell
$reportPath = Join-Path (Get-Location) ('Saved/Automation/UserManagedRun_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject' -unattended -nop4 -RenderOffscreen -nosound -T14ManagedRunPIE '-ExecCmds=Automation RunTests ProjectA.ManagedRunPIE.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath"
```

명령의 현재 폴더는 프로젝트 루트여야 한다. 화면을 직접 조작하는 A와 달리 이 명령은 기존 테스트 코드가 세션과 행동을 구동한다. 직접 에디터에서 실행하려면 `-T14ManagedRunPIE` 인자를 주어 에디터를 시작하고 Session Frontend의 Automation에서 `ProjectA.ManagedRunPIE.` 세 항목을 선택한다. 인자가 없으면 안내만 출력하고 종료하므로 그 성공 표시는 실제 검증이 아니다.

| ID / 테스트 | 구성과 확인 절차 | 기대 결과 | 결과 |
|---|---|---|---|
| B1 · `HostSuccession` | 원래 3인의 확정 턴 저장 → 기존 실행 종료 → 2번 Host·3번 Client·1번 AI로 복구 | Run·원래 소유권·확정 본문 유지, 새 실행/바인딩, 3번의 자기 캐릭터 턴 종료 RPC 승인, 타인 조작 거절, AI 실제 공격과 다음 전투 유지 | 대기 |
| B2 · `HostSuccession4Players` | 원래 4인의 확정 턴 저장 → 2번 Host·3/4번 Client·1번 AI로 복구 | 두 Client가 서로 다른 본인 계정 바인딩을 갖고 각자 턴 종료 RPC 승인, 전투 중 Turn·Grid·HP/AP 일치와 결과 화면·Continue 권한 확인 | 대기 |
| B3 · `SoloMenuConversion` | 원래 4번이 메뉴에서 싱글 전환 → 의도한 맵 이동 실패 → 새 메뉴에서 재시도 → 전투/다음 Encounter | 본인이 Host, 나머지 영구 AI, 실패 중 확정 저장 보존·lease 정리, 재시도 후 복구 장벽 해제·AI 행동·다음 전투의 AI 유지 | 대기 |

결과 파일 `$reportPath/index.json`에서 세 테스트 각각의 상태와 오류/경고 메시지를 확인한다. `Saved/Logs/ProjectA.log`와 보고서 경로를 함께 남긴다. 경고가 있으면 ‘경고 동반 성공’으로 기록하고 실패·미실행·실행 중 항목이 없는지 확인한다.

검증 범위: 추가 Client별 승인 검사는 **턴 종료 RPC**이며 이동·회복약·스킬 전부를 각 Client에서 검증하는 것은 아니다. AI 공격은 실제 실행하지만 결과/다음 전투 검사는 fixture가 치명적 GAS 피해를 주어 승리를 유도한다. 이후 결과 화면과 권한을 확인하며 최종 사망·점유 상태 전체를 다시 비교하지는 않는다. 같은 PC 성공만으로 인터넷 환경·지연/손실·서비스 인증이 확인되지는 않는다.

## C. 필요할 때 추가 확인

B가 실패하거나 관련 코드가 다시 바뀌면 필요한 항목만 선택한다. 매 문서 수정마다 전체 테스트를 반복하지 않는다.

| ID | 확인 내용 | 방법과 기대 결과 | 상태 |
|---|---|---|---|
| C1 | 중복 실행·오래된 stamp·영구 AI의 인간 복귀·v4 일반 로드 우회 | 기존 `ProjectA.Run.Managed` fixture를 사용자가 선택 실행. 거절 뒤 저장 본문·진행·실행 권한 보존 | 필요 시 |
| C2 | 닫힌 관리 lease의 옛 명령 | 기존 관리 fixture의 종료·권한 검사를 확인. 직접 Close 직후 인간/AI 명령·바인딩·모드 변경 거절과 턴·HP/AP·모드 불변 확인. 늦은 콜백·저장 시도 검증은 별도 | 필요 시 |
| C3 | 2·3·4인 일반 Co-op 입력·턴 복구 | `ProjectA.Coop.` 기존 시나리오. 각자 조작·동일 상태·기존 Host 복구 확인. 필요한 전용 인자는 [MULTIPLAYER](MULTIPLAYER.md) 참조 | 2인 전투만 F에서 통과, 나머지 필요 시 |
| C4 | 로컬 상대 Snapshot | 개발용 Snapshot 설정 후 전투 진입. 저장한 파티 빌드·배치로 적 생성, 지원하지 않는 데이터는 오류 표시 | 필요 시 |
| C5 | 일반 Continue·패키지 | 별도로 보존한 일반 세이브로 Continue 확인. 패키지 확인은 최신 패키지를 만든 경우에만 그 빌드 기준으로 기록 | 필요 시 |

실패 주입·가짜 연결·관리 lease 조작은 일반 UI에서 재현할 수 있는 버튼으로 제공하지 않는다. 위 테스트 fixture를 쓰거나 필요한 재현 도구를 별도 작업으로 준비해야 한다. Steam/PlayFab과 MMR은 [TODO](TODO.md)의 8번 준비 이후 별도 보고서 대상으로 추가한다.

## D. T14-8 서비스 준비 보고서

목적: 과금·기본 싱글플레이 설정 변경 없이 실제 온라인 연결에 필요한 준비물을 확인한다. 상세 링크와 비용 조건은 [서비스 준비 절차](MULTIPLAYER.md#t14-8-서비스-준비)를 따른다. 현재는 런타임 연동 코드가 없으므로 접속·인증·MMR 작동을 시험하는 단계가 아니다.

| ID | 사용자가 확인할 항목 | 준비 완료 기준 | 상태 |
|---|---|---|---|
| D1 | PlayFab 로그인 → Studio → 새 Title 생성 화면의 모드·요금 확인 | 제공되는 무료 개발 모드/조건 확인. Xbox 자격을 추정하거나 유료 모드를 승인하지 않음 | 미준비 |
| D2 | 무료 조건이 맞으면 개발용 Title 생성 | Title ID와 모드 확인. Secret Key는 공유하지 않음. 무료 조건이 없으면 화면의 모드·비용만 기록 | 미준비 |
| D3 | Steam 자체 ID 등록 시점 또는 480 개발 연결 선택 | 정식 등록 수수료와 공용 480의 제한을 구분하고 사용할 준비 경로 확정 | 미결정 |
| D4 | 추후 연결을 확인할 Steam 계정 2개·PC 2대의 사용 가능 여부 | 같은 빌드를 각 계정에서 실행할 환경. 자체 앱 사용 시 각 테스트 계정에 앱 접근 권한 필요 | 미확인 |

Codex 정적 점검: UE 5.7의 OnlineSubsystemSteam·SteamSockets는 설치되어 있고 프로젝트에서 비활성이다. PlayFab 플러그인·Title 설정은 없다. 계정/Title 생성·결제·SDK 설치·게임 실행은 수행하지 않았다. 준비 결과를 받은 뒤 연결 코드를 구현하고 다음 사용자 작동 보고서를 작성한다. D 완료만으로 T14-7이나 온라인 수용 조건을 통과 처리하지 않는다.

## E. 엔진 콘텐츠 경로 통일

목적: 사용자·Codex 제작 에셋을 `Content/User_JeHoon`에 모으고 이후 생성물도 같은 규칙을 따르는지 확인한다.

대상 변경: 메뉴 검증 위젯 3종을 `T12Validation`에서 `User_JeHoon/Validation/T12`로 이동하고 UI 생성 도구·검증 코드·생성 안내 경로를 통일했다. 일반 메뉴 원본은 `User_JeHoon/UI/MainMenu`에 있다.

준비 조건: UE 5.7과 이번 Development Editor / Win64 빌드를 사용한다. 생성 도구를 직접 확인할 때는 Saved의 T12 명세를 사용하고 일반 메뉴 원본을 덮어쓰지 않는다.

| ID | 사용자 실행 절차 | 기대 결과 | 상태 |
|---|---|---|---|
| E1 | Content Browser에서 `User_JeHoon/Validation/T12`의 메뉴 WBP 3개를 열어 Designer 확인 | 각 위젯의 기존 구성 유지, 누락 참조 오류 없음 | 미실행 |
| E2 | 일반 MainMenu에서 캐릭터 생성 → Gameplay 진입 | 검증 위젯이 일반 메뉴를 대체하지 않고 기존 화면·전투 진입 유지 | 미실행 |
| E3 | UI 생성 도구에 작업 폴더 밖 assetPath를 지정한 명세로 DryRun, 이어서 작업 폴더 안의 명세로 DryRun | 밖 경로는 오류로 거절, 안 경로는 유효성 검사 통과 | 미실행 |

Codex 확인: Unreal AssetTools로 이동·저장 후 별도 에디터 명령줄 프로세스에서 위젯 3종의 패키지 로드와 이전 경로 참조 부재를 확인했다(오류·경고 0). 기존 경로의 에셋·Redirector는 0개다. 추가 Redirector 정리 명령은 이미 빈 폴더여서 대상 없음 경고 1건으로 종료했다. Development Editor / Win64 컴파일은 성공했다(`Saved/Automation/ContentRootBuild.log`). 에셋 편집·패키지 조사만 수행했으며 PIE·게임·Unreal 자동화 작동 테스트는 실행하지 않았다.

## F. 2026-09-11 Codex 직접 실행: 2인 전투 동기화

사용자의 이번 요청인 “2인테스트 직접 해볼래?”에 따라 이 항목만 Codex가 실행했다. 앞으로의 작동 테스트를 자동 실행하도록 일반 규칙을 변경한 것은 아니다.

- 목적·대상: `e90cd21` 코드/에셋과 직전 성공한 Development Editor / Win64 빌드로 `ProjectA.Coop.ListenServerClientCombat` 1건 실행. 기존 사용자 삭제 에셋 3개가 없는 현재 작업 폴더 기준이며 복원하지 않았다.
- 환경: UE 5.7.4, 같은 PC·한 에디터 프로세스의 Listen Server/Client PIE 월드 2개, 각 월드의 별도 NetDriver와 실제 RPC. 화면은 RenderOffscreen으로 렌더링하며 테스트 코드가 계정 배정과 행동을 구동했다. Visual Studio는 실행하지 않았다.
- 실행: 기존 [멀티플레이 실행 안내](MULTIPLAYER.md)의 Co-op 명령에서 필터를 `ProjectA.Coop.ListenServerClientCombat`으로 한정하고 `-ReportExportPath=Saved/Automation/CoopTwoPlayer_20260911_095426`을 지정했다.
- 결과: **1건 경고 동반 성공, 실패 0, 미실행 0, 오류 0, 경고 4**. 테스트 본체 14.998초, 프로세스 종료 코드 0. PIE 세션 종료 확인.
- 원본 기록: `Saved/Automation/CoopTwoPlayer_20260911_095426/index.json`, 같은 폴더의 `Editor.log`와 `index.html`. Saved 보고서는 로컬 보관이며 Git에는 이 요약을 기록한다.

| 확인 항목 | 기대 결과 및 실제 확인 | 결과 |
|---|---|---|
| 조작권·Host 권한 | 자신의 턴·캐릭터에만 행동 허용, 미배정/잘못된 Host와 Client의 노드·Continue 거절 | 통과 |
| Client 행동 | 회복약·실제 이동·장착 스킬의 GAS 피해·턴 종료가 서버에서 실행되고 응답 수신 | 통과 |
| 중복 명령 | 이미 처리한 스킬 RPC 재전송 시 추가 피해·AP 소비 방지 | 통과 |
| 상태 동기화 | Turn·HUD·HP/AP/SubAP·Grid 점유·이동 및 사망 상태 일치 | 통과 |
| 결과와 다음 전투 | Victory 표시·입력 잠금, Host Continue로 정리·다음 전투 진입, 두 노드 완료 후 Complete와 정리 동기화 | 통과 |

경고 4건: 클래스 미지정 SpawnActor 2건은 Host 시작/Client 합류 시 발생했다. GameplayGameMode의 `DefaultPawnClass = nullptr`와 관련 가능성이 있으나 호출 스택으로 원인을 확정하지 않았다. GameplayCue 검색 경로 미지정 1건은 `/Game` 전체 검색 fallback 안내다. RecastNavMesh 탐색 실패 1건은 PIE 종료 과정에서 발생했으며 그 전에 실제 이동 성공을 확인했다. 경고를 숨기거나 테스트 조건을 완화하지 않았다.

범위 제한: 버튼 delegate와 명령 호출을 쓰는 자동화이며 두 사람이 마우스로 조작한 사용성 검증은 아니다. 사망·승리 일부는 테스트용 치명 피해로 유도하므로 자연스러운 전투 밸런스 검증으로 확대하지 않는다. Steam 초대·별도 PC/P2P·저장 복구·Host 승계·3/4인은 이번에 실행하지 않았으며 T14-7/8과 A/B/E의 대기 상태를 완료로 바꾸지 않는다.

## 사용자 결과 기록

아래 내용을 채워 전달하면 실패 항목부터 수정한다. 테스트를 실행하지 않은 항목은 대기로 유지한다.

```text
실행 날짜 / 확인한 커밋:
환경: 직접 PIE / 사용자 실행 fixture / 패키지
항목 ID와 결과: 예) A2 실패, B1 성공, B2 미실행
실패한 최초 단계:
기대 결과 / 실제 결과:
재현 횟수:
보고서·로그 경로 / 오류 문구:
```

이 보고서는 앞으로 같은 파일에서 최신 변경·필수 시나리오·사용자 결과를 갱신한다. 완료된 검증의 핵심 근거는 [HISTORY](HISTORY.md)에 짧게 남긴다.
