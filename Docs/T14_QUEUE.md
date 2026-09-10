# T14 순차 작업 대기열

사용자 요청: 1~8번을 한 번에 한 개씩 진행한다. 번호별 구현·검증·커밋·push를 분리하며, 앞 번호를 완료하면 다음 번호로 자동 진행한다. 앞 번호의 필수 검증이 끝나기 전에 다음 번호의 구현을 시작하지 않는다. 미결정 기획이나 서비스 선택이 필요한 시점에는 선택지와 장단점을 설명하고 해당 결정에 의존하는 작업을 대기한다.

2026-09-10 추가 승인: `7,8도 진행해줘`에 따라 당시 6번 정책 대기와 독립적인 3·4인 전투/기존 Host 복구를 검증·커밋·push하고 8번 온라인 준비 문서를 정리했다. 이후 사용자 답변으로 번호순 승계·사전 동의 없는 Host 단독 AI 전환·3C·Steam 우선 4B·Host 진행권 5A를 확정했다. 아래 번호별 과거 검증 기록의 사전 동의·정책 답변 대기 표기는 당시 상태이며 최신 정책은 [확정 기획](T14_COOP_DESIGN.md)과 이 문서 마지막 진행 기록을 따른다.

[TODO](TODO.md) · [Co-op 확정 기획](T14_COOP_DESIGN.md) · [README](../README.md)

| 번호 | 상태 | 작업 | 완료 기준 / 선행 조건 |
|---|---|---|---|
| 1 | 완료 | Run·원래 참가자·캐릭터 소유권 데이터 | Actor 없는 식별·동의·Host 데이터, 신규 Run 생성과 저장 왕복, 기존 v1 호환, 소유권 조회 검증 |
| 2 | 완료 | 서버 Action Request와 소유권 검증 | 서버 참가자 바인딩, 본인 캐릭터·턴·자원·대상 검증과 중복 거절, Standalone 실행 회귀 통과 |
| 3 | 완료 | 2인 Listen Server 전투 동기화 | 실제 2개 PIE의 소유 연결 RPC, 본인 Unit 조작, Turn·Grid·HP/AP·사망·결과·HUD 일치 검증 |
| 4 | 완료 | 확정 턴 체크포인트와 기존 Host 복구 | v3 턴 저장·실패 재시도, 실제 2인 새 세션/별도 프로세스 복구, 고정 상대·손상·처리 중 행동 검증 |
| 5 | 완료 | 아군 AI 판단과 행동 | 소유권·아군 진영 유지, 서버 AI·인간 입력 차단, 실제 2인 AI와 별도 프로세스 모드 복구 검증 |
| 6 | 로컬 개발 범위 완료 | 명시적 Host 승계와 AI 이어하기 | 관리 v4·영속 Human·실제 싱글 전환/실패 재시도·3→2인 새 Host 복구와 중복 실행 거절 검증. 실제 서비스는 8번 |
| 7 | 독립 범위 검증 완료 · 승계 통합 대기 | 3·4인 검증 | 실제 3·4인 조작권·동기화·끊김·기존 Host 복구 및 연결된 소유자의 AI 모드 검증. 승계/불참 AI 통합은 6 정책·구현 후 검증 |
| 8 | 준비 문서 완료 · 개발 서비스 준비 필요 | 실제 인증·중앙 저장·MMR 연동 | Steam P2P 우선·4B 선택. 무료 개발 범위/출시 비용·환경과 남은 랭크 정책 확인 후 실제 구현·검증 |

## 1번 구현 범위

- `FRunIdentityData`: 스키마 버전, 식별 출처, Run ID, 원래 참가자와 AI 동의 기록, Host 계정과 Host 세대.
- `FRunPartyMember`: 캐릭터 ID와 원래 소유자 계정. 빈 슬롯은 식별 정보를 갖지 않는다.
- `FRunAccountId`: 공급자와 대소문자를 구분하는 불투명 계정 Subject. 표시 이름·Actor·접속 순서를 영구 ID로 사용하지 않는다.
- 신규 싱글플레이는 Run마다 임시 개발용 참가자 한 명을 생성해 기존 생성 캐릭터들을 소유하게 한다. 실제 계정 인증이나 Co-op의 1인당 캐릭터 수를 확정하는 기능이 아니다.
- 동의 UI가 아직 없으므로 자동 생성 참가자의 동의는 `Unknown`, 정책 버전은 0이다. 명시적 데이터 입력은 `Granted`/`Declined`와 정책 버전 1을 표현할 수 있다.
- 새 Run 저장은 v2다. 실제 식별 정보가 없는 v1 저장은 `LegacyOffline`으로 읽고 계속 v1으로 저장한다. 소유자를 추측해 붙이거나 손상된 v2를 v1으로 우회하지 않는다.
- `IsOriginalParticipant`와 `IsCharacterOwner`는 데이터 조회다. 온라인 계정 인증, RPC 조작권 강제, Host 승계, AI 실행, MMR 반영, 전투 중 턴 복구는 다음 번호들의 범위다.

## 1번 검증 기록 — 2026-09-10

- Development Editor / Win64 빌드 통과. 재시작 테스트 보강 뒤 해당 테스트를 다시 빌드하고 실행했다.
- 전체 자동화 36건 통과: 성공 22건·경고 동반 성공 14건·실패 0건. 신규 Identity 검증 4건은 경고 없이 통과했다.
- 별도 Snapshot 상대 모드의 저장 맵 PIE 1건 통과. MainMenu에서 생성한 식별 정보를 첫 전투·승리 Continue·둘째 전투까지 유지했다.
- 독립 Editor-Cmd 프로세스의 저장 Writer와 복원 Reader 각각 1건 통과. 디스크 값과 복원한 Run·참가자·동의·Host·캐릭터 ID/소유권·HP를 비교했다.
- 식별 프로퍼티가 실제로 빠진 v1 파일을 복원하고 v1으로 재저장했다. 손상된 v2·중복 식별자·외부 소유자·Host 권한 예외를 거절하고 실패한 초기화/복원이 현재 Run과 저장 파일을 변경하지 않는지 확인했다.
- 문서 로컬 링크·diff 검사를 통과했다. 온라인 인증·다중 PIE·턴 경계 복구·AI/승계·MMR·패키지 재빌드는 이번 번호에서 실행하지 않았다.

검증 파일: `Saved/Automation/T14OwnershipBuild.log`, `T14OwnershipRestartBuild.log`, `T14OwnershipFull/index.json`, `T14OwnershipSnapshotPIE/index.json`, `T14OwnershipRestartWrite/index.json`, `T14OwnershipRestartRead/index.json`.

커밋 제목: `[codex] Run 참가자와 캐릭터 소유권 저장 기반 추가`. 다음 번호는 2번 서버 Action Request와 소유권 검증이다. 다음 실행은 Git 로그와 upstream 반영까지 확인한 뒤 착수한다.

## 2번 구현 범위

1번 `66da97c`의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. Actor 없는 `FCombatActionRequest`를 PlayerController의 소유 연결에서 Server Reliable RPC로 보내며, `UCombatActionAuthority`가 서버에서 설정한 참가자 바인딩·Run/Host/전투 실행 ID·연결 바인딩 ID·턴 번호·요청 순번·소유권·현재 자원과 대상을 검증한다.

스킬은 해당 유닛에 실제 장착된 `UPrimaryDataAsset`의 `FPrimaryAssetId`로만 해석한다. Snapshot의 논리 SkillId 계약은 유지한다. 이동 도달 범위는 서버에서 다시 계산하며 회복약도 공통 요청을 거친다. AI와 자원 소진 자동 턴 종료는 서버 내부 완료 경로를 유지한다.

전투마다 새 실행 ID·유닛 ID를 부여하며, 참가자를 새 연결에 바인딩하면 새 바인딩 ID를 사용한다. 요청 순번은 연결 안에서 증가하고 현재 문맥의 거절된 요청도 재실행하지 않는다. 응답은 최신 요청과 선택 세대를 비교해 늦은 응답이 새 선택을 지우지 않게 한다. `Accepted`는 기존 유닛 행동으로 전달했음을 뜻하며 비동기 성공·실패는 행동 완료 이벤트가 결정한다.

실제 계정 인증은 8번, Client 전투/HUD 상태 복제와 2개 PIE 전송 검증은 3번이다. 현재 자동 참가자 연결은 기존 Standalone의 개발용 단일 참가자에만 적용한다. 미식별 TestMap/기존 v1 입력은 Standalone에 한해 지원하며 잘못된 식별 Run을 이 호환 경로로 우회하지 않는다.

## 2번 검증 기록 — 2026-09-10

- Development Editor / Win64 최종 빌드 통과.
- 전체 자동화 41건 통과: 성공 26건·경고 동반 성공 15건·실패 0건. 명령 테스트 5건은 실제 GAS 피해/AP, 회복약 HP/재고/SubAP, 이동 실행과 실패 정리, 턴 종료, 소유권·문맥·바인딩·순번·서버 스킬/대상 검증을 확인했다.
- 기존 격리 테스트의 로컬 Controller 설정 누락으로 발생한 최초 4건 실패를 수정했다. 실제 Gameplay는 최초 실행에서도 통과했으며 최종 전체 실행에서 기존 입력 테스트도 통과했다.
- 입력 잠금·부족한 AP·변경된 타일 점유로 거절된 요청은 조건이 바뀐 뒤에도 재실행되지 않는다. 재접속 이전 명령, 이전 전투 명령, Host의 타인 캐릭터 명령과 늦은 응답의 새 선택 덮어쓰기를 검증했다.
- 실제 Gameplay 저장 맵 PIE와 별도 Snapshot 상대 모드 PIE 1건을 통과했다. 회복약·Slate 이동·스킬·턴 종료가 공통 요청의 승인/순번을 기록하고 기존 적 AI·승리·Continue·다음 전투·패배 흐름을 유지했다.
- 경고는 격리 월드의 미설정 Grid/월드 문맥, 의도한 이동 실패·거절 경로와 기존 PIE 경고 등이다. 문서 로컬 링크와 diff 검사 통과.
- 실제 네트워크 RPC 왕복·Client 상태 복제·2인 PIE·로그인·턴 복구·Host 승계·MMR·패키지 재빌드는 이번 번호에서 실행하지 않았다.

최종 검증 파일: `Saved/Automation/T14CommandsFixBuild.log`, `T14CommandsFull2/index.json`, `T14CommandsSnapshotPIE/index.json`. 최초 실패 결과는 `T14CommandsFull/index.json`에 보존한다.

커밋 제목: `[codex] 전투 명령의 서버 검증과 소유권 확인 추가`. 다음 번호는 3번 2인 Listen Server 전투 동기화다. 다음 실행은 해당 커밋의 upstream 반영까지 확인한 뒤 착수한다.

## 3번 구현 범위

2번 `05843c3`의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. 서버 `CombatManager`의 전투 뷰와 `GameplayGameState`의 Run 표시 값을 RepNotify로 전달한다. 서버만 TurnManager·AI·실제 AbilitySpec을 실행하며 클라이언트 HUD는 복제된 턴·소유권·장착 정의를 사용한다.

유닛 AP/SubAP·행동·현재 타일·사망·회복약·장착 정보를 복제하고 HP/MaxHP는 GAS Attribute RepNotify, 이동은 Character Movement를 사용한다. Grid는 서버만 생성하며 클라이언트는 복제 타일로 좌표 조회와 점유·영역·전열 보호 표시를 구성한다. PlayerController의 소유 연결에 참가자 바인딩과 입력 문맥을 전달하고 2번의 서버 명령 검증을 재사용한다.

네트워크 결과 화면에서는 최종 유닛 상태를 유지하고 명시적인 Continue에서 정리한다. 신뢰된 서버 C++의 연결 배정은 원래 참가자·중복 연결을 검사하지만 실제 계정 인증은 아니다. 기본 Run과 메뉴 진입은 싱글플레이를 유지하며, 네트워크의 노드 선택·Continue 결정권은 사용자 답변 대기 중이라 화면을 읽기 전용으로 두고 테스트가 서버 진입점을 호출한다. 상세 구조와 테스트 방법은 [전투 동기화 안내](T14_NETWORK.md)를 따른다.

## 3번 검증 기록 — 2026-09-10

- Development Editor / Win64 최종 빌드 통과. Visual Studio를 실행하지 않고 명령줄로 빌드했다.
- 전체 자동화 42건 통과: 성공 26건·경고 동반 성공 16건·실패 0건. 신규 `ProjectA.Coop.ListenServerClientCombat`과 기존 Gameplay 저장 맵 PIE를 포함한다.
- 별도 Snapshot 상대 모드의 저장 맵 PIE 1건 통과. 기존 Slate 입력·GAS 피해·적 AI·승리→Continue→다음 노드·패배 흐름을 유지했다.
- 실제 두 PIE 월드의 Listen Server/Client NetDriver와 열린 연결을 확인했다. 클라이언트의 타인 캐릭터 명령을 거절하고, 본인 턴의 회복약·이동·실제 장착 스킬·중복 거절·턴 종료를 RPC로 검증했다. 직접 Client Unit 호출로 서버 자원·피해를 확정할 수 없는지도 확인했다.
- 양쪽의 HP/MaxHP·AP/SubAP·재고·소유권·현재 타일·턴과 HUD 버튼을 비교했다. 서버의 치명적 GAS 피해 뒤 사망·점유 해제·승리·결과 화면·입력 잠금과 Continue 후 정리를 확인했다. 테스트용 서버 배정과 진행 호출은 실제 로그인이나 노드 투표 정책으로 취급하지 않는다.
- 최초 네트워크 PIE 2회는 이동 준비/완료에서 실패했다. 모든 타일의 투영 실패와 편집기 NavMesh 재생성 직후 PIE 시작 로그를 대조해 테스트 초기화 순서 문제를 확인했다. 편집기 자동 생성 완료와 유효 경로를 기다리도록 수정한 뒤 별도 네트워크 PIE와 전체 실행에서 통과했다. Gameplay 에셋·NavMesh 범위·서버 이동 성공 조건은 변경하지 않았다. 세 번째 진단 실행은 원인 확인 후 종료했다.
- 클라이언트 BeginPlay의 초기 복제 문맥 덮어쓰기, 승인된 요청 재전송 시 Pending 표시 고착, 초기 타일 색상 캐시 순서도 검토·보정했다. 마지막 코드 리뷰에서 추가 확정 P1/P2는 없었다.
- 경고는 기존 미설정 Spawn/GAS Cue 경로, 종료 중 Nav 조회와 격리 테스트의 의도한 거절·실패 경로 등이다. 문서 로컬 링크·diff 검사를 통과했다.
- 3·4인, 네트워크 손실/지연, 끊김/턴 복구, Host 승계·AI 전환, 실제 인증·서비스/MMR, 패키지 재빌드 및 발사체별 시각 효과 동기화 검증은 이번 번호에서 실행하지 않았다.

최종 검증 파일: `Saved/Automation/T14NetworkReadyBuild.log`, `T14NetworkPIE4/index.json`, `T14NetworkFull/index.json`, `T14NetworkSnapshotPIE/index.json`. 최초 실패·진단 로그는 `Saved/Logs/T14NetworkPIE1.log`~`T14NetworkPIE3.log`에 보존한다.

커밋 제목: `[codex] 2인 Listen Server 전투와 HUD 동기화 추가`. 다음 번호는 4번 확정 턴 체크포인트와 기존 Host 복구다. 다음 실행은 해당 커밋의 upstream 반영까지 확인한 뒤 착수한다. 노드 선택·Continue 결정권 질문은 유지하며 답변이 오면 그 정책에 의존하는 진행 입력을 연결한다.

## 4번 구현 범위

3번 `da5aaf0`의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. `RunSaveGame` v3에 Actor 없는 전투 시도·revision·턴 순서·다음 유닛·HP/AP/SubAP·재고·사망·점유·실제 장착과 고정 상대 Snapshot을 저장한다. 기존 전투 밖 v1/v2와 LegacyOffline 호환을 유지한다.

저장 경계는 이전 유닛 턴 종료 완료 → 다음 생존 유닛 선택 → 파일 교체 성공 → 다음 턴 시작이다. 저장 실패는 같은 후보와 이전 파일을 유지하며 다음 인간/AI 턴을 막는다. Host의 저장 재시도, 최종 결과 저장 재시도와 Continue 실패 rollback을 연결했다. 확정 v3를 준비 취소로 지우지 못하도록 검사한다.

`USaveGame` 직렬화 후 같은 디렉터리의 임시 파일을 flush·재검증하고 Win64 파일 교체로 확정한다. 새 세션에서는 원래 Host·참가자·소유권을 확인해 새 Actor와 명령 실행 ID를 만들고 마지막 확정 경계를 한 번 활성화한다. 원래 참가자 끊김은 전투 중단으로 처리하며 자동 Host 승계나 AI 전환을 하지 않는다. 지원하지 않는 GAS 상태와 실제 XY/타일 불일치는 명시적으로 거절한다. 상세 계약은 [확정 턴 저장·복구](T14_CHECKPOINT.md)를 따른다.

## 4번 검증 기록 — 2026-09-10

- Development Editor / Win64 최종 빌드 통과. Visual Studio를 실행하지 않았다.
- 전체 자동화 52건 통과: 성공 35건·경고 동반 성공 17건·실패 0건. 새 저장 테스트 5건, 런타임 테스트 3건과 실제 2인 체크포인트 PIE를 포함한다. 별도 프로세스용 테스트는 전용 인자 없는 전체 실행에서 안내만 출력하며 아래 Writer/Reader 실행으로 실제 재시작을 검증했다.
- 실제 두 PIE 세션에서 회복약·이동·GAS 스킬·적 AI 뒤 경계를 저장했다. 파일 쓰기 실패 시 메모리/디스크 revision 보존과 입력 잠금, 같은 후보 재시도와 다음 턴 1회 시작, 처리 중 이동의 저장 거절을 확인했다.
- 기존 세션을 닫은 뒤 새 Listen Server/Client에서 원래 Host·소유권·턴·HP/AP/SubAP·재고·사망·타일·장착을 비교했다. 실행 ID와 연결 바인딩은 새로 만들고 이전 RPC·Host의 타인 조작은 거절하며 새 소유자의 요청은 정상 실행했다.
- 별도 Editor-Cmd 프로세스의 Writer와 Reader 각각 1건 통과. 상대 Snapshot을 다른 유효한 본문으로 교체한 경우와 삭제한 경우 각각 별도 PIE 1건 통과. 저장된 상대 본문과 실제 복구 빌드는 그대로 유지했다.
- 사망 유닛은 HP 0으로 복원하고 사망/행동 콜백을 재실행하지 않았다. 활성 GAS 능력·지속 효과·쿨다운, 중복/손상/버전/Host/소유권/배치 예외와 결과·Continue 저장 실패 rollback, v1/v2 호환을 확인했다. 실제 XY와 점유 타일 불일치는 스폰 전 거절하며 오류를 HUD에 표시했다.
- 별도 Snapshot 모드의 기존 저장 맵 PIE 1건 통과. 최초 전체 실행의 새 테스트 배열 자체 참조 오류를 수정했다. 다음 전체 실행에서 저장 안내 Overlay가 기존 루트의 클릭 통과 설정을 잃는 문제를 찾아 수정했고, 최종 전체 실행과 별도 Snapshot 실행에서 실제 Slate 타일·버튼 입력과 전투→승리→Continue→다음 전투→패배를 통과했다.
- 경고는 기존 Spawn/GAS Cue 설정, 격리 월드·종료 중 Nav 조회, 의도한 거절/이동 취소 경로 등이다. 문서 로컬 링크와 diff 검사를 통과했다.
- 3·4인, 손실/지연 조건, 기존 세션 재접속 UI, 임의 GAS 효과/게임플레이 난수 복원, Host 승계·AI 이어하기, 실제 인증·중앙 저장·MMR과 패키지 재빌드는 이번 번호에서 실행하지 않았다. 로컬 파일 교체를 온라인 위변조·단일 실행 보장으로 취급하지 않는다.

최종 검증 파일: `Saved/Automation/T14CheckpointBuild7.log`, `T14CheckpointFull3/index.json`, `T14CheckpointRestartWrite/index.json`, `T14CheckpointRestartRead/index.json`, `T14CheckpointSnapshotReplace/index.json`, `T14CheckpointSnapshotDelete/index.json`, `T14CheckpointSnapshotPIE/index.json`. 최초 실패 로그는 `Saved/Logs/T14CheckpointFull1.log`, `T14CheckpointFull2.log`에 보존한다.

커밋 제목: `[codex] 확정 턴 체크포인트와 기존 Host 복구 추가`. 다음 번호는 5번 아군 AI 판단과 행동이다. 다음 실행은 해당 커밋의 upstream 반영까지 확인한 뒤 착수한다. 6~8번과 노드 선택·Continue 결정권 질문은 대기 상태를 유지한다.

## 5번 구현 범위

4번 `19908e2`의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. `APlayerUnit`에 서버 전용 `UPartyAutoCombatComponent`를 연결하고 원래 캐릭터·소유자·팀을 유지한다. 인간과 AI는 공통 명령 실행에서 턴·자원·장착·대상·Grid를 검증하며, AI는 인간 연결 대신 별도 실행 세션과 순번을 사용한다. AI 차례에는 양쪽 인간 입력을 잠그고 HUD에 AI를 표시한다.

AI는 자기 회복약·아군 피해 없는 공격·접근 이동·턴 종료를 판단하고 다음 틱 완료 흐름을 사용한다. 전투 시작 전 신뢰된 서버 준비 API만 모드를 설정할 수 있으며 소유자의 사전 동의를 확인한다. 새 전투 본문 schema 2에 파티 조작 모드를 저장하고 이전 schema 1의 Human·기존 v1/v2 저장을 읽는다. 끊김 시 자동 전환·승계·인간 복귀·MMR·참가 배정 예외는 6~8번으로 유지한다. 상세 구조는 [아군 AI 안내](T14_PARTY_AI.md)를 따른다.

## 5번 검증 기록 — 2026-09-10

- Development Editor / Win64 최종 빌드 통과. Visual Studio를 실행하지 않았다.
- 전체 자동화 56건 통과: 성공 36건·경고 동반 성공 20건·실패 0건. 새 런타임 AI 3건과 저장 호환 1건을 포함한다. 전용 프로세스 테스트는 아래 별도 Writer/Reader로 실제 실행했다.
- AI의 미동의·거절·잘못된 정책 버전, Client의 모드 변경, 원래 소유자/Host의 인간 명령, AI의 이전 문맥·중복·잘못된 장착/대상·부족한 자원을 거절했다. 거절한 순번을 재사용하지 않으며 공통 GAS 피해·AP·회복약 HP/재고/SubAP·이동·턴 종료를 확인했다.
- 아군 포함 피해 스킬 제외, 실패한 이동 후 턴 종료 1회, 중단/이전 완료 콜백 무효화, 다음 AI 턴의 실제 공격과 증가하는 순번을 확인했다. 저장 대기·복구 중단 중 Run 재설정이 원래 식별 정보를 바꾸지 못하도록 보정하고 검증했다.
- 실제 두 PIE 세션의 `-T14CheckpointAI` 전투 1건 통과. 저장된 AI 유닛의 원래 소유자 RPC도 거절하고 회복약 1회·공격 4회·이동 1회·턴 종료와 적 턴 이후 Host 차례 복귀를 확인했다. 양쪽의 모드·소유권·팀·턴·HP/AP·Grid·HUD와 새 확정 기록을 비교했다.
- 별도 Editor-Cmd 프로세스 Writer/Reader 각각 1건 통과. Writer가 사전 동의한 AI 모드를 디스크에 준비하고 Reader는 이 본문을 새 Actor로 복구해 동일한 실제 AI 행동을 검증했다. schema 1 Human과 기존 전투 밖 v1/v2 호환, schema 2 모드/동의·유형 검증도 통과했다.
- 최초 AI Co-op 테스트는 높은 HP의 적에게 Host가 사망해 Host 차례 복귀 조건을 기다렸다. 로그에서 AI의 5·7·9턴 정상 종료와 최종 패배를 확인했다. 테스트 Host HP와 저장 Party HP를 보정한 뒤 원래 행동 횟수·턴·동기화 조건을 유지한 재실행에서 통과했다. 런타임 AI의 턴 종료 로직은 이 실패 때문에 변경하지 않았다.
- 별도 Snapshot 모드의 기존 저장 맵 PIE 1건도 통과했다. 인간 입력·상대 Snapshot·전투→승리→Continue→다음 전투→패배 흐름을 유지했다. 경고는 기존 Spawn/GAS Cue 설정, 격리 월드·종료 중 Nav 조회와 의도한 이동 실패 경로 등이다. 문서 로컬 링크와 diff 검사를 통과했다.
- 기본 Run의 Human 조작과 원래 참가자 전원 연결 조건은 유지한다. 실제 불참 승인·연결 예외·명시적 Host 승계/AI 이어하기 UI·인간 복귀·3~4인·실제 인증/중앙 저장/MMR·패키지 재빌드는 이 번호에서 실행하지 않았다.

최종 검증 파일: `Saved/Automation/T14PartyAIBuild5.log`, `T14PartyAIFull2/index.json`, `T14PartyAICoop2/index.json`, `T14PartyAIRestartWrite/index.json`, `T14PartyAIRestartRead/index.json`, `T14PartyAISnapshotPIE/index.json`. 최초 실패는 `T14PartyAICoop1/index.json`과 `Saved/Logs/T14PartyAICoop1.log`에 보존한다.

커밋 제목: `[codex] 소유권을 유지하는 아군 서버 AI 추가`. 다음 번호는 6번 명시적 Host 승계와 AI 이어하기다. 다음 실행은 해당 커밋의 upstream 반영까지 확인한 뒤 착수한다. 7~8번과 노드 선택·Continue 결정권 질문은 대기 상태를 유지한다.

## 6번 진행 기록 — 2026-09-10

5번 `7fa7235`의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. 현재 AI 모드는 전투 체크포인트에만 있으므로 결과·Map·다음 전투까지 보존할 Run 참여 DTO가 필요하다. 승인된 Human 참가자만 연결을 요구하도록 GameMode/Encounter를 연결하되, 원래 소유권과 미승인 기존 Run의 전원 연결 조건은 유지한다.

정책과 독립적인 `FRunParticipationData`의 원래 참가자·Host·사전 동의 검증과 로컬 권위 저장소 대역의 단일 실행/정확한 revision 교체 기반을 구현·검증했다. RunState·GameMode·메뉴와 기존 v1/v2/v3 저장에는 아직 연결하지 않았다. 검증한 기반을 별도 커밋하되 6번 전체 완료로 취급하지 않는다. 연결/저장/UI의 최종 통합은 [재개 구현 메모](T14_RESUME.md)를 따른다.

아래 세 질문을 선택지와 장단점으로 사용자에게 보냈다. 답변 전 해당 정책을 구현하거나 기본 선택으로 간주하지 않는다. 답변이 없고 상태 변화가 없으면 같은 질문이나 진행 알림을 반복하지 않는다.

1. 승계 확정: A 현재 모인 원래 참가자 전원 동의 후 지정 Host / B 원래 참가자 단독 재개 / C 기존 Host의 별도 위임.
2. 미동의 참가자: A Run 시작 허용·해당 소유자 불참 시 AI 이어하기 차단 / B 전원 동의해야 협동 시작.
3. AI 소유자 복귀: A 다음 확정 턴 경계 / B 다음 전투 / C 해당 Run에서는 AI 유지. Host 자격은 자동 반환하지 않는다.

기존 노드 선택·Continue 결정권 질문도 답변 대기 상태를 유지한다. 7~8번은 6번 완료 뒤 순서대로 진행한다.

### 6번 기반 검증 기록

- Development Editor / Win64 빌드 통과. 전체 회귀 60건 통과: 성공 40건·경고 동반 성공 20건·실패 0건. 기존 2인 전투·체크포인트·Gameplay PIE와 신규 참여/저장소 4건을 포함한다.
- 별도 프로세스 테스트를 추가한 뒤 최종 빌드를 통과했다. BusyReader의 읽기 성공/중복 실행 거절, 해당 Holder 강제 종료 후 ResumeReader의 명시적 획득·새 저장·이전 stamp 거절을 각각 1건씩 통과했다. 실행 중단만으로 저장된 Host 세대나 본문은 바뀌지 않는다.
- 마지막 저장소 집중 테스트 4건을 경고 없이 통과했다. 일반 실행의 프로세스 테스트 안내/즉시 종료도 포함한다. 문서 9개의 로컬 링크 124개와 diff 검사를 통과했다.
- 실제 승계 승인·불참 AI 연결 예외·Run 참여 상태의 결과/다음 전투 보존·복귀 정책·UI는 미구현이다. 이 저장소 검증을 네트워크 Host 승계나 중앙 서버 보안 검증으로 취급하지 않는다. Visual Studio와 패키지 재빌드는 실행하지 않았다.

검증 파일과 프로세스 테스트 방법은 [재개 구현 메모](T14_RESUME.md)의 기반 검증을 따른다. 기반 커밋 제목은 `[codex] 협동 재개 참여 데이터와 로컬 권위 저장소 기반 추가`다. 다음 실행은 해당 커밋의 upstream 반영을 확인하고, 정책 답변이 있을 때 6번 통합을 이어간다. 답변이나 다른 상태 변화가 없으면 질문·성공한 검증·커밋·진행 알림을 반복하지 않는다.

## 7번 독립 범위 진행 — 2026-09-10

사용자의 `7,8도 진행해줘` 추가 승인을 받아 6번 `e7b35f6` 기반 커밋의 upstream 반영과 깨끗한 작업 트리를 확인하고 착수했다. 6번의 기존 질문은 유지한다. 3·4인 실제 PIE 전투 명령·전체 Client 상태 일치와 원래 참가자 전원/기존 Host의 새 세션 복구를 먼저 검증한다. 승계·불참 AI·복귀 정책은 결정하거나 완료 처리하지 않는다.

`DefaultGame.ini`에 Unreal 기본 `GameSession.MaxPlayers=4`를 설정한다. Run 데이터의 4인 제한과 서버 일반 플레이어 입장 정원을 맞추며 `GameSession::ApproveLogin`의 기존 검사 경로를 재사용한다. 관전자·화면 분할 정책이나 온라인 Lobby/Invite를 새로 정의하지 않는다.

3·4인 전투/체크포인트 PIE를 기존 2인 테스트와 함께 파라미터화했다. 각 추가 연결의 실제 RPC와 모든 Client의 상태·HUD·전체 Grid 점유를 확인한다. 실제 한 Client의 연결을 끊은 뒤 0.75초 동안 턴·자원·점유·조작 모드·확정 기록이 변하지 않는지 확인하고 원래 참가자 전원의 새 세션 복구를 검증한다. 연결 끊김 테스트가 사용하는 엔진 `NetCore`를 Editor 모듈 의존성에 추가했다.

### 7번 독립 범위 검증 기록

- Development Editor / Win64 최종 `T14ScaleBuild3` 통과. 전체 자동화 65건 통과: 성공 41건·경고 동반 성공 24건·실패 0건. 기존 저장/소유권/Snapshot/Gameplay 입력·전투 루프와 2·3·4인 전투/복구 6건을 포함한다.
- 실제 3·4인 세션의 각 원격 소유자가 회복약·이동·장착 스킬·재전송 거절·턴 종료를 수행했다. 모든 Client의 소유권·CharacterId·HP/AP/SubAP·재고·턴·Grid·사망·승리/정리 화면이 서버와 일치했다. 공식 `GameSession` 입장 검사에서 3인일 때 여유, 4인일 때 정원 초과 거절을 확인했다. 다섯 번째 실제 PIE 접속을 시도한 것은 아니다.
- 3·4인 실제 Client 연결 끊김 시 Host와 남은 Client가 동일하게 멈췄다. 늦은 콜백의 재진행·자동 AI 전환·Host 변경·임의 결과 발생이 없음을 관찰했다. 이후 전원이 새 PIE 세션에 접속해 원래 Host·전체 캐릭터 소유권·턴/HP/AP·점유·사망한 적·새 연결 식별자와 확정 저장을 비교했다.
- 별도 `-T14CheckpointAI` 2·3·4인 복구 3건 통과(모두 경고 동반 성공). 원래 소유자도 연결한 상태로 저장된 아군 AI의 회복약·공격·이동·턴 종료와 인간 입력 거절, 다른 소유자의 실제 턴 종료·전체 Client 일치를 확인했다. 불참자 연결 예외나 Host 승계는 사용하지 않았다.
- 최초 Build1의 `UNetConnection::Close` 링크 실패는 `NetCore` 의존성 추가로 해결했다. 최초 집중 실행은 7건 성공·2건 실패였으며, 두 실패는 의도한 `ConnectionLost` 엔진 Error 로그 각 1건이었다. 끊김 직전에 해당 로그만 정확히 한 번 예상하도록 추가한 뒤 전체/AI 검증을 통과했다. 다른 네트워크 오류와 중복 발생은 계속 실패 처리한다.
- 경고는 기존 격리 월드·Spawn/GAS Cue·PIE 종료/내비게이션과 의도한 거절·이동 실패 경로 등이다. Visual Studio와 패키지 재빌드는 실행하지 않았다. 6번 승계/불참 AI·기존 세션 재접속 UI·지연/패킷 손실 조건·실제 인증/중앙 저장/MMR은 미완료로 유지한다.
- README·구현 현황·기획·TODO를 맞추고 문서 9개의 로컬 링크 126개와 diff 검사를 통과했다. 코드 리뷰에서 발견한 중단 플래그 비교와 늦은 콜백 관측 누락을 보강한 뒤 위 최종 검증을 실행했다.

검증 파일: `Saved/Automation/T14ScaleBuild3.log`, `T14ScaleFull1/index.json`, `T14ScaleAI1/index.json`. 최초 실패 기록은 `T14ScaleBuild1.log`, `T14ScaleCoop1/index.json`과 대응 `Saved/Logs`에 보존한다. 실행 방법은 [전투 동기화 안내](T14_NETWORK.md)를 따른다.

커밋 제목은 `[codex] 3·4인 전투와 기존 Host 복구 검증 확장`이다. 해당 독립 범위의 커밋·push 후 8번으로 이어간다. 플랫폼(EOS/Steam)과 Backend(PlayFab/직접 HTTPS 서버·PostgreSQL)는 선택지와 장단점을 질문한 상태이며 답변이 없으면 임의로 공급자를 선택하지 않는다.

## 8번 준비 기록 — 2026-09-10

7번 독립 범위 `152aed7`의 검증·커밋·upstream push와 깨끗한 작업 트리를 확인하고 진행했다. 현재 프로젝트에는 실제 공급자 로그인·서버 인증·세션 생성·Backend 연동이 없다. 설치된 UE 5.7의 OSS EOS/Steam과 NetDriver, 공식 PlayFab/서버 저장 문서를 대조해 [온라인 연동 준비](T14_ONLINE.md)에 선택지·준비물·책임 경계·순차 구현과 수용 기준을 기록했다.

현재 사용자에게 보낸 두 질문은 플랫폼 A EOS/B Steam, Backend A PlayFab/B 직접 HTTPS 서버·PostgreSQL이다. 답변을 기다리며 공급자·유료 리소스·실제 배포를 임의 선택하지 않는다. MMR 판정 시점·계산/정산·이탈 악용/결과 검증 정책은 여전히 별도 미정이다. 6번 질문에 대한 답변도 추가되지 않았다.

이번 변경은 준비 문서뿐이다. 실제 로그인·P2P·중앙 저장·MMR이나 8번 전체 완료로 기록하지 않는다. 앞선 7번 검증을 온라인 서비스 검증으로 재사용하지 않는다. 문서 10개의 로컬 링크 133개와 `git diff --check`를 통과했으며 문서만 변경하므로 C++ 빌드·PIE를 반복하지 않았다. `[codex] 온라인 서비스 선택과 인증·중앙 저장 연동 준비 정리`로 별도 커밋·push한다. 이후 새 답변이나 개발 서비스 준비가 있을 때 해당 범위를 이어가며 상태 변화가 없으면 같은 질문·알림·조사·커밋을 반복하지 않는다.

## 완료 요청 후 추가 점검 — 2026-09-10

사용자의 `t14작업 완료해줘` 요청으로 코드와 완료 조건을 다시 대조했다. 원래 Snapshot 전투·2인 PIE 조건은 충족했지만 확장한 6~8번의 승계·실제 서비스 조건은 남아 있다. 승계 확정 주체·미동의 시작·AI 소유자 복귀 시점 세 항목과 온라인 서비스 조합·협동 노드/Continue 결정권 두 항목을 선택지와 영향으로 다시 요청했다. 새 요청 자체를 정책 답변으로 처리하지 않는다.

독립적인 P2 버그를 수정했다. 기존 메뉴는 유효한 협동·AccountProvider v2/v3 저장도 Continue 가능으로 표시하고 Standalone으로 이동했지만, 실제 참가자 바인딩·전투 복구는 이 실행 방식을 지원하지 않아 진행할 수 없었다. 메뉴 전용 `CanContinueStandaloneSavedRun`·`LoadStandaloneCheckpoint`를 연결해 기존 v1 오프라인과 LocalDevelopment 참가자 한 명의 저장만 허용한다. 클릭 시 읽은 동일 저장 객체를 재검증하므로 버튼 검사 이후 파일이 바뀌어도 현재 Run·파일을 보존하고 메뉴에서 이유를 안내한다. 일반 협동 `CanContinueSavedRun`·`LoadCheckpoint`는 유지한다.

검증: Development Editor / Win64 빌드 성공, 전체 자동화 67건(성공 43·경고 동반 성공 24·실패 0) 통과. 신규 `ProjectA.Persistence.StandaloneContinueEligibility`·`StandaloneContinueFileReplacement` 두 건은 경고 없이 통과했다. 유효한 v1/v2/v3와 단일 소유자 다중 캐릭터의 허용, 계정·복수 소유자 저장 거절, 파일 교체 후 현재 Run·저장 활성 상태·오류·이벤트·디스크 보존을 검증했다. 기존 실제 2·3·4인 전투/복구와 저장 맵 PIE 흐름도 통과했다. 로그는 `Saved/Automation/T14StandaloneContinueBuild1.log`, `T14StandaloneContinueFull1/index.json`에 있다. 실제 온라인 인증·Host 승계·MMR 검증을 추가 완료한 것은 아니다.

README 정합성·문서 10개의 로컬 링크 133개·diff 검사와 독립 코드 검토를 확인했다. 이 수정은 `[codex] 협동 저장의 싱글플레이 이어하기 진입 차단`으로 별도 커밋·push하며 6~8번의 남은 의존 작업은 답변과 서비스 준비 후 이어간다. 상태 변화 없이 질문·테스트·커밋을 반복하지 않는다.

## 후속 정책 확정과 코드 반영 — 2026-09-10

사용자는 최초 Host 1번·최초 합류 순서대로 2·3·4번, 명시적 재개에 참여하는 인간 중 가장 작은 번호로 Host 승계, 혼자 싱글 전환 시 본인 Host·나머지 AI를 선택했다. 추가 확인에서 개인 사전 동의를 완전히 없애고 Host가 AI 전환을 단독 확정한다고 답했다. 3C는 해당 Run 종료까지 AI 유지, 5A는 현재 Host만 노드·Continue 결정이다. 4B는 Steam+PlayFab이며 서버 운영비를 피하고자 하는 조건을 추가했다. 무료 개발 한도와 P2P 전투/중앙 저장·랭크 비용은 [온라인 안내](T14_ONLINE.md)에 구분했다.

- Identity schema 2에 최초 `JoinOrdinal`을 저장한다. 신규 로컬 Run은 번호 1이며 schema 1의 번호 미상 0과 기존 v1/v2/v3를 유지한다. 번호 조회·명시 Human 집합의 최소 번호 후보 조회는 인증·승계 승인을 대신하지 않는다. 배열 순서 변경·단독 2/3/4 후보·손상 거절·실제 v2/v3 직렬화와 실패 시 데이터 보존을 검증했다.
- AI 준비·명령·전투 저장/복원·참여 DTO에서 사전 동의 gate를 제거했다. 기존 동의 필드의 형식은 호환용으로 유지한다. Unknown/Declined AI를 허용하면서 서버 권위·원래 소유권·인간 입력 차단·자원/장착/대상·AI 세션/중복 요청 검증을 유지했다.
- 현재 Listen Host의 로컬 Controller와 신뢰된 계정 배정을 대조해 노드·Continue UI와 실제 요청을 허용했다. Client·서버의 원격 Controller·미배정·다른 Host 계정은 거절한다. 실제 2·3·4인 테스트는 Host의 버튼으로 두 번째 전투와 최종 Complete·정리까지 진행했다.
- Development Editor / Win64 최종 Build2 통과. 전체 실행의 69건 중 66건 통과 후 새 UI 준비 시점 검사 3건이 실패했다. CommonUI 활성 화면/버튼 생성만 기다리는 테스트 단계를 추가하고 해당 3건을 재실행해 모두 통과했다. 최종 집계는 서로 다른 69건 성공 45·경고 동반 성공 24·미해결 실패 0이며, 수정 뒤 성공한 나머지 66건을 불필요하게 반복하지 않았다.
- 별도 `-T14CheckpointAI`의 실제 2·3·4인 새 세션 복구 3건(모두 경고 동반 성공)을 통과했다. 동의 없는 AI의 행동·원래 소유자 입력 거절·전체 상태 일치를 확인했다. 별도 Snapshot 모드의 저장 맵 PIE 1건도 통과해 실제 Slate 입력·승리·Continue·다음 전투·패배를 유지했다.
- 코드와 문서를 독립 검토하고 README·현재 기획·과거 검증 기록을 구분했다. 문서 13개의 로컬 링크 156개와 `git diff --check`를 통과했다. Visual Studio를 열지 않았다.

검증 파일: `Saved/Automation/T14HostPolicyBuild2.log`, `T14HostPolicyFull1/index.json`, `T14HostPolicyCombat2/index.json`, `T14HostPolicyAI1/index.json`, `T14HostPolicySnapshot1/index.json`. 최초 UI 준비 시점 실패는 Full1에 보존한다. 커밋 제목은 `[codex] 협동 참가 번호와 Host 결정권 정책 반영`이다.

이 변경은 실제 싱글 전환 UI·Run 참여 상태 영속화·새 Host 전투 복구를 아직 연결하지 않았으므로 6번 전체 완료가 아니다. 이 통합을 로컬 개발 저장소로 진행한 뒤 7번 승계 검증으로 이어간다. 실제 Steam 계정 인증·원격 공동 저장·MMR·유료 서비스는 이번 검증에 포함하지 않았다.

## 6번 관리 Run 통합 — 2026-09-10

정책 커밋 `04ed243`의 upstream 반영 후 로컬 개발 저장소에 관리 Run v4를 연결했다. 원래 번호·캐릭터 소유권과 전투 밖 Human 목록을 보존하며, 새 Host/HostEpoch·목록·안쪽 전투 Identity/모드를 최신 stamp와 함께 원자적으로 획득한다. Human 목록은 이후 재개에서 줄어들 수만 있어 이미 AI인 계정의 인간 복귀를 막는다. 모든 관리 저장은 현재 실행 lease를 검사하고 일반 슬롯으로 우회하지 않는다. 일반 v1/v2/v3와 Snapshot v1은 유지한다.

신뢰된 C++ 개발 호출자와 재개 대상이 설정된 메뉴에 **싱글로 전환하기**·**싱글 진행 이어하기**를 추가했다. 실제 Steam 계정 입력이나 온라인 로그인을 제공하는 메뉴는 아니다. 현재 Human만 연결을 배정하고 한 명이면 Standalone, 둘 이상이면 Listen Server에서 마지막 확정 턴을 복원한다. 새 전투에도 영속 AI 상태를 적용한다. 정상 종료는 전투·콜백을 중단한 뒤 lease를 반환하며 옛 Authority의 인간/AI 요청·바인딩·모드 변경도 차단한다.

검토에서 메뉴 이동 실패 후 lease가 남는 문제와 v4 검증이 실패 이유를 비우는 문제를 보정했다. 관리 메뉴 여행의 TravelFailure는 GameInstance 수명으로 감시하며 같은 실행·WorldContext의 실패에서만 lease를 반환한다. 이미 확정한 Host·AI·저장 바이트는 보존하고 새 메뉴에서 다시 이어갈 수 있다. Gameplay 도착 후 복원 실패는 별도 재시도 흐름을 사용한다.

- Development Editor / Win64 최종 `T14ManagedRunBuild3.log` 성공. 신규 native 4건은 2·3·4번 단독 재개·번호순 공동 재개·3C·Busy/stale·실패 원자성·일반 v4 로드 우회 거절을 검사한다.
- 전체 `T14ManagedRunFull1/index.json` 75건 성공 51·경고 동반 성공 24·실패 0. 새 실제 관리 PIE 2건은 전용 인자가 없으면 안내만 출력하므로 다음 별도 실행과 구분한다.
- `T14ManagedRunPIE2/index.json`의 실제 관리 PIE 2건 모두 경고 동반 성공. 3인 저장→2번 Host/3번 Client·1번 AI 복구와 실제 명령·상태/HUD 일치, 4번의 메뉴 싱글 전환·나머지 AI·결과/다음 전투·새 확정 저장을 확인했다. 마지막 전투에서 직접 lease를 닫은 뒤 옛 명령의 거절과 턴·자원 불변을 검사했다.
- 싱글 테스트는 존재하는 비맵 패키지로 실제 TravelFailure를 발생시키고 옛 메뉴 Controller 파괴·같은 GameInstance의 새 메뉴·확정 바이트 보존·다시 이어하기·정상 전투까지 확인했다. `Saved/Automation/T14ManagedResumeMenu.png`의 실제 화면을 열어 문구와 버튼 배치를 확인했다.
- 최초 `T14ManagedHostPIE1`은 테스트 namespace가 32자 제한을 초과해 초기화에서 실패했다. fixture를 31자로 줄인 뒤 최종 실제 PIE에서 통과했으며 production 검증 조건은 완화하지 않았다.
- 기존 v3 AI opt-in `T14ManagedRunAI1/index.json`의 실제 2·3·4인 복구 3건과 `T14ManagedRunSnapshot1/index.json`의 별도 Snapshot 저장 맵 PIE 1건도 모두 경고 동반 성공했다. 경고는 기존 Spawn/NavMesh/GAS·PIE 종료와 의도한 연결/맵 이동 실패 경로이며 미해결 실패는 없다.
- README 정합성·코드 교차 검토·실제 메뉴 화면 확인·문서 링크와 diff 검사 후 `[codex] 관리 Run 승계와 싱글 AI 이어하기 연결`로 커밋·push한다. Visual Studio와 패키지 빌드는 열거나 실행하지 않았다.

이 통합은 동일 PC의 로컬 개발 Run을 대상으로 한다. 원격 중앙 저장·실제 Steam 계정·P2P/초대·MMR 검증은 8번에 남는다. 기존 기반의 별도 프로세스 잠금 검증을 이번 실제 원격 승계 검증으로 재사용하지 않는다. 다음 7번은 원래 4명·현재 인간 3명·불참 AI 1명의 추가 실제 PIE를 확인한다.
