# T14 · 아군 AI 판단과 행동

[작업 대기열](T14_QUEUE.md) · [Co-op 기획](T14_COOP_DESIGN.md) · [확정 턴 저장·복구](T14_CHECKPOINT.md) · [README](../README.md)

## 실행 구조

`APlayerUnit`은 원래 클래스·팀·캐릭터 ID·소유자를 유지하며 `UPartyAutoCombatComponent`가 서버에서 턴 판단을 실행한다. 기본 모드는 `EPartyControlMode::Human`이다. 컴포넌트는 프레임 Tick이나 별도 PlayerController를 만들지 않고 기존 턴 시작·종료와 행동 완료를 연결한다. 기존 `AEnemyUnit`과 그 Blueprint 점수 설정은 유지한다.

아군 AI는 사용할 수 있는 자기 회복약, 실제 장착·부여된 공격 스킬, 상대에게 가까워지는 이동, 턴 종료를 판단한다. 스킬은 공통 타겟팅 규칙과 AP를 확인하고 유효 적 대상 수와 비용·거리·HP를 평가한다. 효과 대상에 아군이 포함되는 공격은 제외한다. 이동은 현재 CombatManager의 도달 가능 타일만 후보로 사용하며 동점은 장착 순서·타일 좌표로 안정적으로 처리한다.

판단은 Actor를 포함하지 않는 기존 `FCombatActionRequest`로 변환한다. `ExecuteServerAI`는 가짜 인간 연결을 사용하지 않고 AI 전용 실행 세션·순번을 검사한 뒤 인간 요청과 같은 `ExecuteUnitAction`에서 현재 턴·생존·행동 중 상태·Grid·스킬·대상·자원을 검증한다. Client가 AI 요청으로 우회하는 RPC나 플래그는 제공하지 않는다.

비동기 행동을 시작하기 전에 대기 상태를 설정하고 완료 이후 다음 틱에 재판단한다. 실패는 무한 재시도하지 않고 턴을 종료한다. 턴 종료·사망·전투 정지·월드 종료에서는 진행 중 판단을 무효화하며 이전 전투·턴·조작 세션의 타이머가 새 행동을 실행하지 못하게 한다. AI 모드에서는 PlayerUnit의 인간용 자원 소진 자동 종료 타이머와 중복 실행하지 않는다.

## 조작권과 준비 API

`UCombatActionAuthority::SetPartyControlMode`는 신뢰된 서버 C++의 전투 시작 전 준비 진입점이다. 등록된 원래 캐릭터와 소유자를 확인하고, `ServerAI`에는 그 소유자의 `Granted` 및 동의 정책 버전 1이 필요하다. 이미 턴 관리가 시작된 전투에서는 모드를 변경하지 않는다. 모드 변경은 인간 입력·AI 판단이 진행되는 중에 조작권을 바꾸는 기능이 아니다.

AI 모드는 Unit과 전투 표시 뷰에 복제한다. AI 차례에는 HUD 이름에 `(AI)`를 표시하고 원래 소유자와 Host 모두 인간 조작 버튼을 사용할 수 없다. 서버도 인간 요청을 거절하며 그 요청의 순번을 소비해 나중의 재전송을 막는다. 원래 캐릭터 소유권과 Host 권위는 바뀌지 않는다.

이 API는 실제 불참 판정·Host 승계 승인·AI 이어하기 UI를 대신하지 않는다. 기본 Run은 Human을 유지하며 끊김만으로 AI를 켜지 않는다. 현재 협동 복구의 원래 참가자 연결 배정 조건도 유지한다. 불참자 대신 AI로 이어가는 사용 흐름과 연결 배정 예외는 다음 6번에서 명시적인 승인·저장 계약과 함께 연결한다. 인간 복귀 시점·미결정 MMR 정책은 확정하지 않는다.

## 저장 호환

Run SaveGame의 외부 버전은 v3를 유지하고 전투 본문은 schema 2로 기록한다. 각 유닛의 `PartyControlMode`를 저장하며, 이 필드는 파티 유닛의 조작 방식이다. 적 유닛의 기존 AI는 변경하지 않는다.

기존 schema 1은 파티 Human만 허용한다. 이전 v1/v2의 빈 전투 본문 기본값을 유지해 전투 밖 저장 호환을 보존한다. schema 2의 AI는 원래 소유자의 사전 동의를 다시 검증하고 새 Actor에 모드를 적용한 뒤 정상 턴 시작에서 판단을 실행한다. 실행 중 AI 세션 ID·순번·타이머·선택 중인 대상은 영구 저장하지 않는다. 복구 시 새 세션 ID로 이전 실행 요청을 거절한다.

## 검증과 재현

2026-09-10 Development Editor / Win64 최종 빌드와 전체 자동화 56건(성공 36·경고 동반 성공 20·실패 0)을 통과했다. 새 런타임 테스트 3건은 동의·서버 준비 조건·인간 입력 거절·AI 명령 문맥과 순번·실제 GAS/자원·이동 실패·지연 완료 무효화·다음 AI 턴을 확인한다. 저장 테스트 1건은 schema 1 Human 호환, schema 2 AI·동의·유형 검증과 실제 SaveGame 왕복을 확인한다.

`ProjectA.Coop.CheckpointSessionRestart -T14CheckpointAI`는 실제 두 PIE의 원래 소유자 RPC를 거절한 뒤 서버 AI의 자기 회복약 1회·공격·실제 이동·턴 종료와 Host 차례 복귀를 확인한다. 양쪽 AI 모드·원래 팀/소유권·HP/AP·Grid·HUD·새 확정 저장값을 비교한다. 전용 테스트 Writer가 사전 동의한 AI 모드를 디스크에 명시적으로 준비하며 실제 사용자 승인 UI나 부재 참가자 연결 예외를 검증하는 테스트는 아니다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject' -unattended -nop4 -RenderOffscreen -nosound -T14CheckpointAI '-ExecCmds=Automation RunTests ProjectA.Coop.CheckpointSessionRestart' '-TestExit=Automation Test Queue Empty'
```

별도 Editor-Cmd 프로세스에서도 Writer/Reader를 각각 실행해 통과했다. 필터를 `ProjectA.Persistence.CombatProcessRestart`로 바꾸고, Writer에는 `-T14WriteCombatCheckpoint`, Reader에는 `-T14ReadCombatCheckpoint`를 추가한다. 두 실행 모두 `-T14CheckpointAI -T14CheckpointSlot=T14_CombatProcess_<고유값>`의 같은 슬롯을 사용한다. Writer 종료 뒤 Reader를 시작하며 Reader는 파일의 AI 모드를 그대로 복구한다. 해당 전용 인자 없는 전체 자동화 실행에서는 프로세스 재시작 테스트가 안내만 출력한다.

별도 Snapshot 모드의 기존 저장 맵 PIE도 통과해 인간 입력·상대 전투·승리·Continue·다음 전투·패배 흐름을 확인했다. 최초 2인 AI 테스트는 적만 높은 HP로 준비해 Host가 사망하면서 Host 차례 복귀를 기다렸다. 테스트용 Host 생존 조건과 저장 파티 HP를 보정한 뒤 동일한 행동·턴·동기화 조건을 통과했다. 상세 실행 파일과 최종 결과는 [대기열](T14_QUEUE.md)에 기록한다. 실제 AI 이어하기/승계 UI·3~4인·로그인·MMR·패키지 재빌드는 이 번호의 검증에 포함하지 않는다.
