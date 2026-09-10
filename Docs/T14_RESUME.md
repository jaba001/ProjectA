# T14 · 명시적 Host 승계와 AI 이어하기 구현 메모

[작업 대기열](T14_QUEUE.md) · [확정 기획](T14_COOP_DESIGN.md) · [아군 AI](T14_PARTY_AI.md) · [턴 저장·복구](T14_CHECKPOINT.md)

## 현재 단계

6번 진행 중이다. 단일 실행과 참여 데이터 기반을 구현·검증했으며 승계 확정 주체·미동의 참가자의 Run 시작·AI 소유자 복귀 시점은 사용자 답변을 기다린다. 선택지의 추천을 승인으로 취급하지 않는다. 검증한 기반을 별도 커밋하되, 최종 UI·실제 승계 복구·통합 검증과 해당 변경의 커밋·push가 끝나기 전 6번 완료로 표시하지 않는다.

## 정책과 독립적인 기반

`FRunParticipationData`는 Human으로 참여하는 원래 계정 목록을 Actor 없이 표현한다. 구조 검증은 원래 참가자·중복·Host의 Human 참여·목록 밖 원래 소유자의 사전 동의를 확인한다. 캐릭터 모드는 원래 소유자로부터 조회하며 사망만으로 참가 상태가 바뀌지 않는다. 이 DTO와 구조 검증은 접속 인증, 승계 승인, 복귀 정책이나 실제 MMR 참여 판정을 대신하지 않는다.

로컬 권위 저장소 대역은 Run별 전체 저장 본문과 저장소 revision·HostEpoch·실행 세션을 하나의 `USaveGame` 기록으로 확정한다. 저장소 revision은 전투 턴 revision과 별도로 해당 Run의 모든 저장 순서를 나타낸다. 같은 개발 저장소를 보는 실행들이 정확한 이전 stamp로만 갱신·재개하도록 하고, 실행 중에는 배타적인 OS 파일 핸들을 유지한다. 별도의 짧은 파일 잠금으로 읽기·비교·교체를 직렬화한다. 기존 `FRunCheckpointStorage`의 임시 파일·flush·재검증·교체를 재사용한다.

실행 lease 해제는 진행 권한을 놓는 동작이다. 그 자체로 Host를 바꾸거나 AI를 선택하지 않는다. 프로세스 종료로 파일 핸들이 해제된 후에도 새 세대는 명시적인 승인·재개 동작에서만 만든다. 이 개발 대역은 동일 PC의 공유 저장 경로를 대상으로 하며 실제 원격 중앙 저장·온라인 신원·네트워크 장애 판정·위변조 방지를 구현한 것이 아니다.

## 통합 순서와 보존 조건

1. 각 owning PlayerController가 자신의 사전 동의를 제출한다. 요청 본문에서 계정을 받지 않고 서버가 해당 연결의 원래 계정을 해석한다. 기존 싱글 캐릭터 생성 화면에서 Host가 다른 사람의 동의를 대신 체크하지 않는다.
2. 승인된 Human 목록과 조작 상태를 전투 밖에도 보존한다. 현재 `CompleteEncounter`가 전투 본문을 비우므로 이 데이터를 전투 체크포인트에만 넣지 않는다. 기존 v1/v2/v3 호환과 일반 싱글 저장을 보존한다.
3. 새 Host·참여 상태·최신 확정 본문은 승인 결과로 함께 적용한다. 같은 확정 턴의 승계를 일반적인 다음 턴 커밋으로 처리하지 않는다. 원래 참가자·개인 동의·캐릭터 ID·소유자는 바꾸지 않는다.
4. GameMode의 배정·연결 요구와 Encounter의 새 스폰/복구는 승인 Human 목록 및 영속 모드를 사용한다. 승인 기록 없이 단순히 유닛의 AI 플래그만 보고 불참 연결을 허용하지 않는다.
5. 기존 Continue와 분리한 AI 이어하기 화면에서 재개 내용을 확인하고 승인·저장 성공 뒤 새 세션을 연다. 노드 선택·전투 결과 Continue의 미정 협동 결정권을 승계 권한과 묶지 않는다.
6. 이전 Host의 실행·저장·결과는 유효 lease와 세대로 차단한다. 관리 대상 Run의 모든 저장은 해당 lease를 검증하는 경로로 보낸다. 충돌 후 일반 로컬 저장으로 우회하지 않으며 실행을 중단한다. 새 서버의 명령 nonce만으로 이전 프로세스의 진행까지 차단했다고 기록하지 않는다.

현재 추가한 기반은 기존 RunState·GameMode·메뉴에 아직 연결하지 않았다. 기존 Run SaveGame v1/v2/v3 형식도 변경하지 않았다. 승인된 재개 정보 없이 기존 5번의 전투별 AI 플래그를 Human 목록으로 추정 이관하지 않는다.

## 기반 검증 — 2026-09-10

- Development Editor / Win64 빌드와 전체 회귀 60건 통과: 성공 40건·경고 동반 성공 20건·실패 0건. 신규 참여 데이터 1건과 저장소 3건을 포함하며 기존 2인 전투·체크포인트와 Gameplay 저장 맵 PIE도 통과했다.
- 참여 구조·원래 소유자별 모드·사전 동의·사망한 인간 참가자 보존·네이티브 직렬화, 같은 프로세스의 중복 실행·오래된 revision·손상 기록·쓰기 실패 시 파일/출력/lease 보존을 확인했다.
- 별도 프로세스 검증을 추가한 뒤 다시 빌드했다. Holder가 실행 권한을 보유하는 동안 다른 BusyReader 프로세스의 읽기는 성공하고 획득은 거절됐다. 해당 테스트 Holder만 강제 종료한 뒤 ResumeReader가 epoch 2/revision 2를 명시적으로 획득하고 revision 3 저장을 확정했다. 기존 stamp 재사용은 거절됐다. 두 Reader 테스트는 각각 경고 없이 1건 통과했다.
- 마지막 저장소 집중 테스트 4건도 경고 없이 통과했다. 전용 인자가 없는 `ProcessProbe`가 대기하지 않고 종료하는 경로를 포함한다. 문서 9개의 로컬 링크 124개와 diff 검사를 통과했다.
- 위 프로세스 검증은 공유 로컬 저장소의 실행 권한만 검증한다. 실제 Host 계정 변경·참여 승인·AI 전환·새 Listen Server의 Run 복구는 아래 통합 범위로 남는다. 전체 회귀의 경고는 기존 격리 월드·미설정 Spawn/GAS Cue·의도한 이동 실패·PIE 종료 경로 등이다.

검증 파일: `Saved/Automation/T14ResumeFoundationBuild2.log`, `T14ResumeFoundationFull1/index.json`, `T14ResumeAuthorityBusyReader/index.json`, `T14ResumeAuthorityResumeReader/index.json`, `T14ResumeAuthorityFocused/index.json`. Holder는 종료 후 핸들 해제를 검증하기 위해 테스트 프로세스를 강제 종료했으므로 완료 보고서 대신 `Saved/Logs/T14ResumeAuthorityHolder.log`를 남긴다.

프로세스 테스트는 `ProjectA.RunAuthority.ProcessProbe`에 `-T14AuthorityProbe=Holder|BusyReader|ResumeReader`, 공통 `-T14AuthorityNamespace=<안전한 이름>`, `-T14AuthorityRunId=<GUID>`, `-T14AuthorityProbeId=<GUID>`를 지정한다. Holder의 `Saved/Automation/T14Authority_<ProbeId>.ready`를 확인한 뒤 BusyReader → 해당 Holder 종료 → ResumeReader 순서로 각각 별도 Editor-Cmd 프로세스에서 실행한다. 전용 인자가 없는 일반 실행에서는 안내만 출력하고 대기하지 않는다. Holder는 외부 종료가 없으면 180초 뒤 실패하며 잠금을 해제한다.

## 후속 통합 수용 기준

- 원래 A Host/B Client 전투 저장 → 종료 → B Listen Server로 명시적 승계, A 연결 없이 AI 재개. Run·소유권·확정 턴 보존과 세대 변경, 타인 인간 조작 거절, 실제 AI 행동, 새 확정 저장을 확인한다.
- 두 재개 요청의 충돌·오래된 stamp·중복 클릭·쓰기 실패·이전 세대의 갱신을 검증한다. 별도 프로세스의 동시 시도와 종료 후 명시적 획득도 확인한다.
- 선택한 복귀 정책과 개인별 동의를 UI·서버에서 확인한다. 기본 체크나 Host의 일괄 동의로 다른 사람의 동의를 만들지 않는다.
- 결과→Continue→다음 전투에도 승인된 AI 상태를 유지한다. 기본 Human/Snapshot 전투·2인 동기화·v1/v2/v3 저장 회귀를 유지한다.
- 원래 2명 중 1명이 불참하면 새 세션은 인간 1명이다. 승계 후 두 네트워크 화면의 동기화를 함께 확인하려면 원래 3명·현재 인간 2명·불참 AI 1명의 추가 fixture를 사용한다. 이를 7번의 3·4인 동시 접속 검증 완료로 취급하지 않는다.
