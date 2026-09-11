# 테스트 보고서

갱신일: 2026-09-11. 최신 실행 결과와 사용자 검증 절차를 관리한다. 이력은 [HISTORY](HISTORY.md), 설정은 [PROJECT_PLAN](PROJECT_PLAN.md)를 따른다.

| 대상 | 상태 |
|---|---|
| 2인 전투 / F | 경고 동반 성공 1건, 오류 0·경고 4 |
| 4인 전투 / G | 경고 동반 성공 1건, 오류 0·경고 6 |
| 직접 조작·승계·콘텐츠 경로 / A·B·E | 검증 대기 |
| 서비스 준비 / D | Steam App ID·PlayFab Title 미준비 |

작동 테스트는 사용자 수행이 원칙이며 명시적 요청에 한해 Codex가 실행한다. 컴파일·정적 검사와 작동 검증을 구분하며 미실행 항목은 완료로 표시하지 않는다.

## 준비 조건

- UE 5.7에서 최신 소스와 빌드된 `ProjectAEditor`를 사용한다. Visual Studio 실행은 필요하지 않다.
- 일반 플레이 확인은 `/Game/User_JeHoon/LEVEL/MainMenu`에서 시작한다. 프로젝트 기본 설정과 에셋 경로는 [PROJECT_PLAN](PROJECT_PLAN.md)을 따른다.
- 일반 새 게임은 싱글플레이다. **관리 Run의 협동 생성·참가 번호 선택·실제 Steam 로그인 UI는 아직 없다.** 관리 이어하기 패널은 C++ 개발 fixture가 호출자와 저장 대상을 설정해야 표시된다.
- 아래 관리 시나리오는 같은 PC의 개발 신원·로컬 저장소와 여러 PIE 월드를 사용하는 기존 자동화 fixture다. 실제 친구 초대·다른 PC·Steam 인증 시험과 구분한다.
- 일반 새 게임은 기존 단일 진행 슬롯을 갱신한다. 보존할 세이브가 있으면 실행 전에 `Saved/SaveGames`를 별도 위치에 복사한다. 관리 fixture는 고유 namespace/Run ID를 사용한다.

## A. 직접 플레이 확인

| ID | 절차 | 기대 결과 | 상태 |
|---|---|---|---|
| A1 | MainMenu → 새 게임 → 캐릭터 1~4명 생성 → 시작 → 첫 Combat 노드 선택 | Gameplay에 생성한 파티가 등장하고 현재 턴·HP/AP가 표시됨 | 대기 |
| A2 | 자기 턴에 Move 선택 → 표시된 빈 도달 타일 클릭 | 실제 이동·점유 타일·자원이 갱신되고 완료 후 다음 입력이 가능함 | 대기 |
| A3 | 사용 가능한 스킬 선택 → 유효한 적/타일 클릭 | 타겟 표시와 실제 효과·HP/AP가 일치하고 스킬 완료 후 입력이 풀림 | 대기 |
| A4 | 적 턴과 이동/스킬 처리 중 Move·스킬·타일 클릭을 시도하고 정상 진행을 기다림 | 유효하지 않은 행동은 실행되지 않고 적 행동 완료 후 자기 턴에 입력이 다시 가능함 | 대기 |
| A5 | 전투 승리 → Continue → 두 번째 Combat 노드 선택 | 같은 Gameplay에서 다음 전투가 시작되고 파티·HP·노드 진행이 유지됨 | 대기 |

실패 시 최초 단계·화면·로그를 기록한다. A2/A3의 입력 문제는 버튼 선택·범위 표시·이동/효과 적용 단계로 구분한다.

## B. 승계 검증

사용자 실행 대상은 B1~B3이다. 작업 저장·PIE 종료 후 프로젝트 루트 PowerShell에서 실행한다. 보고서는 실행 시각별 경로에 저장한다.

```powershell
$reportPath = Join-Path (Get-Location) ('Saved/Automation/UserManagedRun_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject' -unattended -nop4 -RenderOffscreen -nosound -T14ManagedRunPIE '-ExecCmds=Automation RunTests ProjectA.ManagedRunPIE.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath"
```

테스트 코드가 세션·행동을 구동한다. 에디터에서는 `-T14ManagedRunPIE`로 시작 후 Session Frontend → Automation → `ProjectA.ManagedRunPIE.` 3개를 선택한다. 전용 인자가 없으면 실제 검증 없이 종료된다.

| ID / 테스트 | 구성과 확인 절차 | 기대 결과 | 결과 |
|---|---|---|---|
| B1 · `HostSuccession` | 원래 3인의 확정 턴 저장 → 기존 실행 종료 → 2번 Host·3번 Client·1번 AI로 복구 | Run·원래 소유권·확정 본문 유지, 새 실행/바인딩, 3번의 자기 캐릭터 턴 종료 RPC 승인, 타인 조작 거절, AI 실제 공격과 다음 전투 유지 | 대기 |
| B2 · `HostSuccession4Players` | 원래 4인의 확정 턴 저장 → 2번 Host·3/4번 Client·1번 AI로 복구 | 두 Client가 서로 다른 본인 계정 바인딩을 갖고 각자 턴 종료 RPC 승인, 전투 중 Turn·Grid·HP/AP 일치와 결과 화면·Continue 권한 확인 | 대기 |
| B3 · `SoloMenuConversion` | 원래 4번이 메뉴에서 싱글 전환 → 의도한 맵 이동 실패 → 새 메뉴에서 재시도 → 전투/다음 Encounter | 본인이 Host, 나머지 영구 AI, 실패 중 확정 저장 보존·lease 정리, 재시도 후 복구 장벽 해제·AI 행동·다음 전투의 AI 유지 | 대기 |

결과 파일 `$reportPath/index.json`에서 세 테스트 각각의 상태와 오류/경고 메시지를 확인한다. `Saved/Logs/ProjectA.log`와 보고서 경로를 함께 남긴다. 경고가 있으면 ‘경고 동반 성공’으로 기록하고 실패·미실행·실행 중 항목이 없는지 확인한다.

검증 범위: 추가 Client별 승인 검사는 **턴 종료 RPC**이며 이동·회복약·스킬 전부를 각 Client에서 검증하는 것은 아니다. AI 공격은 실제 실행하지만 결과/다음 전투 검사는 fixture가 치명적 GAS 피해를 주어 승리를 유도한다. 이후 결과 화면과 권한을 확인하며 최종 사망·점유 상태 전체를 다시 비교하지는 않는다. 같은 PC 성공만으로 인터넷 환경·지연/손실·서비스 인증이 확인되지는 않는다.

## C. 추가 회귀 검증

B가 실패하거나 관련 코드가 다시 바뀌면 필요한 항목만 선택한다. 매 문서 수정마다 전체 테스트를 반복하지 않는다.

| ID | 확인 내용 | 방법과 기대 결과 | 상태 |
|---|---|---|---|
| C1 | 중복 실행·오래된 stamp·영구 AI의 인간 복귀·v4 일반 로드 우회 | 기존 `ProjectA.Run.Managed` fixture를 사용자가 선택 실행. 거절 뒤 저장 본문·진행·실행 권한 보존 | 필요 시 |
| C2 | 닫힌 관리 lease의 옛 명령 | 기존 관리 fixture의 종료·권한 검사를 확인. 직접 Close 직후 인간/AI 명령·바인딩·모드 변경 거절과 턴·HP/AP·모드 불변 확인. 늦은 콜백·저장 시도 검증은 별도 | 필요 시 |
| C3 | 2·3·4인 일반 Co-op 입력·턴 복구 | `ProjectA.Coop.` 기존 시나리오. 각자 조작·동일 상태·기존 Host 복구 확인. 필요한 전용 인자는 [MULTIPLAYER](MULTIPLAYER.md) 참조 | 2·4인 전투 F/G 통과, 복구·3인은 별도 |
| C4 | 로컬 상대 Snapshot | 개발용 Snapshot 설정 후 전투 진입. 저장한 파티 빌드·배치로 적 생성, 지원하지 않는 데이터는 오류 표시 | 필요 시 |
| C5 | 일반 Continue·패키지 | 별도로 보존한 일반 세이브로 Continue 확인. 패키지 확인은 최신 패키지를 만든 경우에만 그 빌드 기준으로 기록 | 필요 시 |

실패 주입·가짜 연결·관리 lease 조작은 일반 UI에서 재현할 수 있는 버튼으로 제공하지 않는다. 위 테스트 fixture를 쓰거나 필요한 재현 도구를 별도 작업으로 준비해야 한다. Steam/PlayFab과 MMR은 [TODO](TODO.md)의 8번 준비 이후 별도 보고서 대상으로 추가한다.

## D. T14-8 서비스 준비 보고서

목적: 온라인 연결의 계정·환경·비용 조건 확인. [서비스 준비 절차](MULTIPLAYER.md#t14-8-서비스-준비) 기준이며 접속·인증·MMR 작동 검증은 연동 구현 후 수행한다.

| ID | 사용자가 확인할 항목 | 준비 완료 기준 | 상태 |
|---|---|---|---|
| D1 | PlayFab 로그인 → Studio → 새 Title 생성 화면의 모드·요금 확인 | 제공되는 무료 개발 모드/조건 확인. Xbox 자격을 추정하거나 유료 모드를 승인하지 않음 | 미준비 |
| D2 | 무료 조건이 맞으면 개발용 Title 생성 | Title ID와 모드 확인. Secret Key는 공유하지 않음. 무료 조건이 없으면 화면의 모드·비용만 기록 | 미준비 |
| D3 | Steam 자체 ID 등록 시점 또는 480 개발 연결 선택 | 정식 등록 수수료와 공용 480의 제한을 구분하고 사용할 준비 경로 확정 | 미결정 |
| D4 | 추후 연결을 확인할 Steam 계정 2개·PC 2대의 사용 가능 여부 | 같은 빌드를 각 계정에서 실행할 환경. 자체 앱 사용 시 각 테스트 계정에 앱 접근 권한 필요 | 미확인 |

정적 확인: OnlineSubsystemSteam·SteamSockets 설치/비활성, PlayFab 플러그인·Title 미설정. 계정 생성·결제·SDK 설치·온라인 실행은 미수행이다. 준비 완료 후 연동 구현·작동 검증을 별도 진행한다.

## E. 엔진 콘텐츠 경로 통일

목적: 사용자·Codex 제작 에셋을 `Content/User_JeHoon`에 모으고 이후 생성물도 같은 규칙을 따르는지 확인한다.

대상 변경: 메뉴 검증 위젯 3종을 `T12Validation`에서 `User_JeHoon/Validation/T12`로 이동하고 UI 생성 도구·검증 코드·생성 안내 경로를 통일했다. 일반 메뉴 원본은 `User_JeHoon/UI/MainMenu`에 있다.

준비 조건: UE 5.7과 콘텐츠 경로 변경을 포함한 Development Editor / Win64 빌드를 사용한다. 생성 도구는 Saved의 T12 명세로 확인하며 일반 메뉴 원본은 보존한다.

| ID | 사용자 실행 절차 | 기대 결과 | 상태 |
|---|---|---|---|
| E1 | Content Browser에서 `User_JeHoon/Validation/T12`의 메뉴 WBP 3개를 열어 Designer 확인 | 각 위젯의 기존 구성 유지, 누락 참조 오류 없음 | 미실행 |
| E2 | 일반 MainMenu에서 캐릭터 생성 → Gameplay 진입 | 검증 위젯이 일반 메뉴를 대체하지 않고 기존 화면·전투 진입 유지 | 미실행 |
| E3 | UI 생성 도구에 작업 폴더 밖 assetPath를 지정한 명세로 DryRun, 이어서 작업 폴더 안의 명세로 DryRun | 밖 경로는 오류로 거절, 안 경로는 유효성 검사 통과 | 미실행 |

검증: AssetTools 이동·저장 후 별도 프로세스에서 위젯 3종 로드, 이전 경로 에셋/Redirector 0개 확인(오류·경고 0). 추가 정리 명령은 빈 폴더로 대상 없음 경고 1건. Editor 빌드 성공(`Saved/Automation/ContentRootBuild.log`), 콘텐츠 변경의 작동 검증은 미실행이다.

## F. 2인 전투 동기화

목적: 원래 소유자의 행동 요청과 서버 전투 상태의 Client 반영 확인. 사용자 명시 요청에 따라 Codex가 실행했다.

| 항목 | 기록 |
|---|---|
| 기준 | e90cd21 코드/에셋, 직전 성공한 Development Editor / Win64 빌드 |
| 환경 | UE 5.7.4, 동일 PC·한 프로세스의 Listen Server 1 / Client 1, 별도 NetDriver·실제 RPC, RenderOffscreen |
| 테스트 | ProjectA.Coop.ListenServerClientCombat |
| 결과 | 경고 동반 성공 1건, 실패/오류/미실행 0, 경고 4, 14.998초, 종료 코드 0 |
| 산출물 | Saved/Automation/CoopTwoPlayer_20260911_095426의 index.json·Editor.log·index.html |

### 공통 절차와 수용 결과

| 항목 | 절차·기대 결과 | 2인 / 4인 |
|---|---|---|
| 조작권 | 참가자별 서버 바인딩, 자기 캐릭터·자기 턴만 허용, 미배정/잘못된 Host·Client의 진행 거절 | 통과 / 통과 |
| 행동 | 각 Client의 회복약·이동·장착 스킬·턴 종료를 RPC로 실행하고 서버 결과 수신 | 통과 / 통과 |
| 중복 요청 | 처리된 스킬 재전송 시 추가 피해·AP 차감 거절 | 통과 / 통과 |
| 상태 | Turn·HUD·HP/MaxHP·AP/SubAP·재고·이동·Grid·사망 일치 | 통과 / 통과 |
| 전투 진행 | Victory·입력 잠금·Host Continue·다음 전투·두 노드 Complete·Actor 정리 | 통과 / 통과 |

실행은 [멀티플레이 명령](MULTIPLAYER.md#개발-실행-참조)의 필터를 위 테스트명으로 한정하고 ReportExportPath를 산출물 경로로 지정한다. 기존 사용자 삭제 에셋 3건을 보존했으며 저장된 제작 에셋은 변경하지 않았다. 원본 보고서는 로컬 Saved에 보관한다.

### 경고 대응

| 종류 | 2인 / 4인 | 분석·권장 대응 |
|---|---|---|
| 클래스 미지정 SpawnActor | 2 / 4건 | 참가자 시작·합류 시 발생. 빈 HUDClass가 InitializeHUDForPlayer → ClientSetHUD → SpawnActor로 전달되는 경로가 유력하며 호출 스택은 미확보. 공식 HUD 초기화에서 클래스 없는 요청을 생략하는 수정 검토 |
| GameplayCue 검색 경로 미지정 | 1 / 1건 | /Game 전체 검색 fallback. 실제 Cue 의존성을 조사한 뒤 필요한 검색 경로 명시 |
| 종료 시 RecastNavMesh 없음 | 1 / 1건 | 실제 이동 통과 후 PIE 종료 과정에서 발생. 낮은 우선순위로 추적하며 플레이 중 발생·이동 실패 동반 시 종료/CrowdManager 경로 조사 |

HUD·Cue 항목을 우선 정리하고 수정 후 컴파일·2/4인 재검증을 권장한다. 현재 경고는 미해결이며 로그 필터링·테스트 조건 완화는 적용하지 않았다.

### 검증 제한

테스트는 개발 계정 배정·버튼 delegate·명령 호출을 사용한다. 실제 마우스 조작·사용성·Steam 초대·별도 PC/P2P·지연/손실·저장 복구·Host 승계는 검증 범위에서 제외한다. 사망·승리 일부는 치명 피해로 유도하며 자연 전투 밸런스 검증으로 해석하지 않는다. A/B/E와 T14-7/8의 대기 상태는 유지한다.

## G. 4인 전투 동기화

목적: 최대 정원의 참가자별 조작권·턴 진행·다중 Client 동기화 확인. 사용자 조건부 실행 요청에 따라 Codex가 필요성을 판단하고 실행했다.

| 항목 | 기록 |
|---|---|
| 기준·환경 | 8c29017, 기존 Editor 빌드, UE 5.7.4, 동일 PC·한 프로세스의 Listen Server 1 / Client 3 |
| 테스트 | ProjectA.Coop.ListenServerFourPlayerCombat |
| 결과 | 경고 동반 성공 1건, 실패/오류/미실행 0, 경고 6, 31.563초, 종료 코드 0 |
| 추가 확인 | 원격 소유자 3명 각각의 행동 RPC와 모든 Client 상태 일치. 정원 4명에서 GameSession 추가 입장 거절 검사 |
| 산출물 | Saved/Automation/CoopFourPlayer_20260911_110655의 index.json·Editor.log·index.html |

절차·수용 결과·경고 대응은 F의 공통 표를 따른다. 테스트용 HP·AP 조정은 메모리 사본에 한정한다. 실제 다섯 번째 접속과 4→3인 Host 승계는 실행하지 않았다. PIE와 프로세스 종료를 확인했다.

## 문서 정적 검증

Markdown 9개를 공식 문체로 정리하고 중복 상세를 기준 문서로 통합했다. 실행 명령·정책·수치·검증 근거를 유지하고 로컬 링크 61개·앵커 5개·코드 블록·diff 정적 검사를 통과했다. 문서 편집에 따른 추가 빌드·게임 실행은 수행하지 않았다.

## 사용자 결과 기록

결과 제출 형식은 다음과 같다. 미실행 항목은 대기로 유지한다.

```text
실행 날짜 / 확인한 커밋:
환경: 직접 PIE / 사용자 실행 fixture / 패키지
항목 ID와 결과: 예) A2 실패, B1 성공, B2 미실행
실패한 최초 단계:
기대 결과 / 실제 결과:
재현 횟수:
보고서·로그 경로 / 오류 문구:
```

완료된 검증의 핵심 근거는 [HISTORY](HISTORY.md)에 기록한다.
