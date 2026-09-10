# T14 · 확정 턴 체크포인트와 기존 Host 복구

[작업 대기열](T14_QUEUE.md) · [Co-op 기획](T14_COOP_DESIGN.md) · [전투 동기화](T14_NETWORK.md) · [README](../README.md)

## 확정 경계

저장 순서는 이전 유닛의 `OnTurnEnd` 완료 → 다음 생존 유닛 선택 → 전투 값 저장 완료 → 다음 `OnTurnStart`다. 첫 전투는 완료 턴 번호 0 경계를 저장한 뒤 첫 턴을 시작한다. 경계의 모든 유닛은 비활성이고 AP/SubAP는 아직 다음 턴의 초기화 전 값이다.

저장된 `CompletedTurnSerial`은 완료된 턴 번호, `NextTurnIndex`는 다음에 시작할 유닛의 배열 인덱스다. 복원은 그 유닛의 `OnTurnStart`를 한 번 실행하므로 활성 유닛의 AP/SubAP는 정상 턴 시작 값으로 초기화되고 다른 유닛의 잔량은 저장값을 유지한다. 기존 `OnTurnChanged`는 적의 판단·행동이 시작된 뒤 발생하므로 저장 훅으로 사용하지 않는다.

`UTurnManager`의 저장 게이트가 실패하면 턴 번호·선택한 다음 유닛을 유지한다. 인간 입력·적 AI·자동 턴 종료를 진행하지 않으며, Host의 **저장 다시 시도** 버튼이 같은 후보 본문을 재시도한다. 클라이언트도 오류와 멈춘 전투 상태를 확인하지만 서버의 로컬 파일 쓰기를 직접 재시도하지 않는다. 재시도 성공 후 다음 턴은 한 번만 시작한다.

최종 승패도 디스크 저장 성공 뒤 Run 결과로 공개한다. 종료 기록의 저장이 실패하면 최종 유닛과 대기 결과를 유지하고 재시도한다. 결과 Continue 저장 실패 시에도 기존 결과 상태와 파일을 보존한다.

## 저장 계약

- 전투 중 저장은 `RunSaveGame` v3다. 전투 밖 v1/v2 형식과 읽기는 유지하며, v1의 `LegacyOffline` 소유권을 추정해 전투 복구 대상으로 이관하지 않는다.
- `FCombatCheckpointData`는 스키마/콘텐츠 버전, 전투 시도 `AttemptId`, 순번 `Revision`, Run/원래 참가자/Host, 노드·인카운터, 완료 턴과 다음 유닛 인덱스를 저장한다.
- `Units` 배열이 턴 순서다. 각 유닛은 체크포인트 ID, 원래 캐릭터/소유자/파티 슬롯, 팀, 클래스 에셋 경로, 이름, HP/MaxHP·AP/SubAP·상한, 이동 범위, 회복약, 사망, 타일 좌표, 변환, 순서 있는 실제 장착과 기본 공격을 저장한다.
- 상대 Snapshot 전투는 읽어 온 원본 본문·카탈로그와 해석한 실제 유닛 빌드를 보존한다. 복원할 때 외부 상대 슬롯을 다시 읽거나 `EncounterSkillPool`을 다시 추첨하지 않는다.
- 영속 데이터에는 Actor·Controller·GAS 실행 핸들이 없다. 복원된 Actor와 명령용 실행 ID·유닛 ID·연결 바인딩 nonce는 새로 생성한다. Run·소유권·Host 계정/세대·AttemptId·체크포인트 ID는 유지한다.

현재 콘텐츠 버전은 `UCombatCheckpointLibrary::CurrentContentVersion` 1이다. 저장된 스킬·클래스 경로의 동작 계약을 바꾸는 콘텐츠 변경은 버전도 갱신해야 한다. 저장 파일은 현재 로컬 콘텐츠를 참조하며 실행 코드 자체를 동결하거나 서버가 인증한 데이터를 제공하지 않는다.

## 파일 쓰기와 손상 검증

`USaveGame`과 `SaveGameToMemory`의 Unreal 직렬화를 사용한다. 임시 파일을 같은 저장 디렉터리에 쓰고 flush·바이트 재검증을 마친 뒤 Win64의 파일 교체 연산으로 확정 파일을 바꾼다. 기존 파일을 먼저 지우지 않는다. 현재 어댑터는 Win64 로컬 파일 저장을 대상으로 하며 다른 플랫폼과 중앙 저장은 별도 구현이 필요하다.

UE 5.7 기본 `SaveGameToSlot`은 직접 덮어쓰고, `IFileManager::Move`의 덮어쓰기 경로는 목적지를 먼저 삭제할 수 있어 마지막 확정 기록 보존에 그대로 사용하지 않았다. 미완료 `.tmp` 파일은 로드 대상이 아니다. 동기 저장을 사용하므로 이전 비동기 쓰기가 나중에 도착해 최신 기록을 덮는 경로는 없다. 전원 상실이나 저장 장치 고장에 대한 절대적인 내구성을 주장하지 않는다.

버전·Run/Host 일치, 원래 소유권, 중복 ID/점유, 숫자 범위, 생존/HP 일치, 다음 턴의 생존 유닛, 해석 가능한 클래스·스킬과 원래 직업/아레나 적합성을 검증한다. 생존 유닛의 실제 XY는 점유 타일 중심에서 5cm 이내여야 하며 상대 클래스·장착·최대 스탯은 고정 Snapshot과 대조한다. 손상된 v3를 v1/v2로 우회하지 않으며 잘못된 로드는 현재 Run을 교체하지 않는다. 준비 실패로 생성된 복구 액터는 정리하고 저장 기록은 유지한다. 이미 확정된 전투는 준비 취소로 지도에 되돌려 지울 수 없다.

## 복구 진입점과 현재 경계

저장된 단일 개발용 Run은 기존 MainMenu Continue → Gameplay에서 자동 복원한다. 협동 복구는 신뢰된 서버 코드가 기존 Host의 로컬 연결과 원래 참가자 전원의 연결을 배정한 뒤 `AEncounterManager::RestoreSavedCombat`을 호출한다. 실제 로그인·Lobby/Invite를 제공하는 API가 아니며 다른 사람에게 캐릭터 소유권을 넘기지 않는다.

원래 참가자의 연결이 끊기면 전투를 멈춘다. 부분 실행 상태를 새 체크포인트로 저장하거나 Host·인간 조작권을 자동 변경하지 않는다. 기존 Host와 원래 참가자는 새 세션에서 마지막 확정 기록을 복원한다. 살아 있는 기존 세션에 즉시 재접속하는 UI와 Host 승계·AI 이어하기는 후속 범위다.

현재 복구는 프로젝트의 유휴 Grid 전투를 대상으로 한다. 활성 Ability, 미완료 이동/스킬 액터, 지속/주기 효과, 쿨다운·상태 태그 등 현재 저장 계약이 표현하지 못하는 상태는 조용히 버리지 않고 거절한다. 장착 추첨 결과는 저장하지만 향후 전투 중 게임플레이 난수를 재현하는 범용 RandomStream 저장이나 임의 GAS 상태 복원까지 구현한 것은 아니다. 전열 보호는 복원한 진영·점유에서 재계산한다.

미확정 턴의 반복 악용, 중앙 권위 revision, 동시 Host 승계·MMR 중복 반영 방지는 6~8번과 미결정 랭크 정책의 범위다. 네트워크 노드 선택·Continue의 결정권은 기존 질문의 답변 대기 상태를 유지한다.

## 검증과 실행

2026-09-10 Development Editor / Win64 빌드와 전체 자동화 52건(성공 35·경고 동반 성공 17·실패 0), 별도 Writer/Reader·상대 교체/삭제·Snapshot 전투 회귀를 통과했다. 상세 결과와 최초 실패 수정 기록은 [대기열](T14_QUEUE.md)에 정리한다.

자동화 필터 `ProjectA.Checkpoint`는 저장·손상·GAS·사망·턴 게이트 테스트 8건을 실행한다. `ProjectA.Coop.CheckpointSessionRestart`는 두 개의 실제 PIE 세션을 차례로 열며, `-T14CheckpointOpponent=Replace` 또는 `Delete`를 추가하면 격리된 원본 상대 슬롯 변경·삭제 후 복구를 검증한다.

독립 프로세스 테스트 `ProjectA.Persistence.CombatProcessRestart`는 첫 프로세스에 `-T14WriteCombatCheckpoint`, 다음 프로세스에 `-T14ReadCombatCheckpoint`를 지정한다. 두 프로세스에 동일한 `-T14CheckpointSlot=T14_CombatProcess_<고유 영문숫자>`를 전달한다. 전용 인자 없이 실행하면 실제 재시작을 수행하지 않는다. PIE 검증은 `-unattended -nop4 -RenderOffscreen -nosound`와 `-ExecCmds="Automation RunTests <필터>"`, `-TestExit="Automation Test Queue Empty"`로 실행하며 테스트별 별도 Run 슬롯을 사용한다.
