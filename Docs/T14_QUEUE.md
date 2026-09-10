# T14 순차 작업 대기열

사용자 요청: 1~8번을 한 번에 한 개씩 진행한다. 번호별 구현·검증·커밋·push를 분리하며, 앞 번호를 완료하면 다음 번호로 자동 진행한다. 앞 번호의 필수 검증이 끝나기 전에 다음 번호의 구현을 시작하지 않는다. 미결정 기획이나 서비스 선택이 필요한 시점에는 선택지와 장단점을 설명하고 해당 결정에 의존하는 작업을 대기한다.

[TODO](TODO.md) · [Co-op 확정 기획](T14_COOP_DESIGN.md) · [README](../README.md)

| 번호 | 상태 | 작업 | 완료 기준 / 선행 조건 |
|---|---|---|---|
| 1 | 완료 | Run·원래 참가자·캐릭터 소유권 데이터 | Actor 없는 식별·동의·Host 데이터, 신규 Run 생성과 저장 왕복, 기존 v1 호환, 소유권 조회 검증 |
| 2 | 완료 | 서버 Action Request와 소유권 검증 | 서버 참가자 바인딩, 본인 캐릭터·턴·자원·대상 검증과 중복 거절, Standalone 실행 회귀 통과 |
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
