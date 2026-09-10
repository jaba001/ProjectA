# T14 순차 작업 대기열

사용자 요청: 1~8번을 한 번에 한 개씩 진행한다. 번호별 구현·검증·커밋·push를 분리하며, 앞 번호를 완료하면 다음 번호로 자동 진행한다. 앞 번호의 필수 검증이 끝나기 전에 다음 번호의 구현을 시작하지 않는다. 미결정 기획이나 서비스 선택이 필요한 시점에는 선택지와 장단점을 설명하고 해당 결정에 의존하는 작업을 대기한다.

[TODO](TODO.md) · [Co-op 확정 기획](T14_COOP_DESIGN.md) · [README](../README.md)

| 번호 | 상태 | 작업 | 완료 기준 / 선행 조건 |
|---|---|---|---|
| 1 | 완료 | Run·원래 참가자·캐릭터 소유권 데이터 | Actor 없는 식별·동의·Host 데이터, 신규 Run 생성과 저장 왕복, 기존 v1 호환, 소유권 조회 검증 |
| 2 | 대기 | 서버 Action Request와 소유권 검증 | 1 완료 후 연결의 참가자 식별, 본인 캐릭터·턴·자원·대상 검증, 중복 요청 거절 |
| 3 | 대기 | 2인 Listen Server 전투 동기화 | 2 완료 후 2개 PIE에서 본인 Unit 조작, Turn·Grid·HP/AP·사망·결과·HUD 일치 |
| 4 | 대기 | 확정 턴 체크포인트와 기존 Host 복구 | 3 완료 후 마지막 확정 턴의 상태를 저장·복원, 처리 중 행동·손상·저장 실패 검증 |
| 5 | 대기 | 아군 AI 판단과 행동 | 4 완료 후 소유권·아군 진영을 유지하는 서버 AI, 인간 입력과 동시 실행 차단 |
| 6 | 대기 | 명시적 Host 승계와 AI 이어하기 | 5 완료 후 원래 참가자·사전 동의 검증, 승계/AI 버튼, 로컬 권위 저장소 대역을 통한 중복 재개 거절 |
| 7 | 대기 | 3·4인 검증 | 6 완료 후 최대 인원 조작권·동기화·끊김·복구·승계/AI 통합 검증 |
| 8 | 대기 | 실제 인증·중앙 저장·MMR 연동 | 7 완료 및 공급자·Backend·MMR 참여 판정/이탈/결과 검증 정책 확정 후 실제 서비스 검증 |

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
