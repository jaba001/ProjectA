# T14 · Listen Server 전투 동기화

[작업 대기열](T14_QUEUE.md) · [Co-op 기획](T14_COOP_DESIGN.md) · [README](../README.md)

## 서버와 클라이언트의 책임

- 서버의 `UTurnManager`만 턴을 시작·종료한다. `ACombatManager`는 전투 ID·Run/Host 세대·현재 턴·승패·유닛 식별/원래 소유권의 `FCombatViewState`를 RepNotify로 배포한다. 클라이언트는 TurnManager를 생성하지 않는다.
- `AUnitBase`는 AP/SubAP·팀·현재 타일·행동/턴/사망 상태·실제 장착 데이터를 복제한다. HP/MaxHP는 GAS Attribute RepNotify, 캐릭터 이동은 Unreal Character Movement의 복제 경로를 사용한다.
- 유닛은 서버 AIController로 이동하므로 ASC의 PlayerController 소유권에 의존하지 않는다. GAS는 Minimal 복제와 서버 전용 능력 실행·종료 정책을 사용한다. 클라이언트 HUD는 복제된 장착 정의를 표시하며 서버만 실제 AbilitySpec을 검사·실행한다.
- Grid 타일은 서버가 생성한다. 클라이언트의 TileMap은 복제된 타일의 Grid/좌표로 구성하고, 점유·영역·전열 보호·표시 상태를 수신한다. 타일 클릭과 하이라이트는 각 로컬 화면의 입력이다.
- 클라이언트는 `PartyPlayerController`의 소유 연결로 값 Command를 보낸다. Run/전투/바인딩/턴/순번·소유권·서버 자원·대상 검증은 2번의 `UCombatActionAuthority`를 재사용한다. 복제값을 로컬에서 바꿔도 서버 행동을 확정할 수 없다.
- `AGameplayGameState`는 Phase·Result·파티·노드 등 읽기 전용 표시 값을 전달한다. 클라이언트 GameInstance의 RunState를 서버 Run으로 복원하지 않는다. 영속 데이터와 Command에는 Actor를 저장하지 않고, 실행 중 복제 뷰의 유닛 조회에만 Actor 참조를 사용한다.

## 연결의 원래 참가자 배정

`AGameplayGameModeBase::AssignRunParticipant`는 신뢰된 서버 C++ 연동 지점이다. 원래 참가자 계정만 연결에 배정하며 같은 연결의 계정 변경과 같은 계정의 중복 연결을 거절한다. Encounter는 Run 설정 뒤 `ApplyCombatParticipantBindings`로 전투에 연결한다. 네트워크 전투는 모든 원래 참가자의 배정이 필요하며 LegacyOffline 입력 호환은 Standalone에만 적용한다.

현재 자동화 fixture가 알려진 서버 연결에 두 계정을 명시적으로 배정한다. 접속 순서나 표시 이름을 영구 계정으로 저장하지 않으며, 이 배정 자체를 로그인 인증으로 취급하지 않는다. 실제 계정 공급자·Lobby/Invite·Steam/EOS·중앙 저장은 이후 범위다.

## 화면과 전투 종료

Host와 Client의 HUD는 같은 서버 턴 이름·HP/AP·장착 목록을 읽고, 자신의 캐릭터 차례에만 행동 버튼을 활성화한다. 늦은 응답은 새 선택을 지우지 않으며 이미 승인된 명령의 재전송이 응답 표시를 Pending에 남기지 않는다. 클라이언트 BeginPlay는 먼저 도착한 초기 복제 입력 문맥을 보존한다.

5번의 `PartyControlMode`는 유닛과 전투 뷰에 복제한다. 아군 AI 차례에는 양쪽 HUD에 `(AI)`를 표시하고 원래 소유자와 Host 모두 직접 조작을 막는다. 인간 RPC와 서버 내부 AI 명령은 별도 실행 문맥·순번을 사용하며 같은 턴·자원·장착·Grid 검증을 거친다. 클라이언트는 AI를 실행하거나 모드를 변경하지 않는다. 조작 모드와 캐릭터의 원래 소유권은 별도이며 [아군 AI 안내](T14_PARTY_AI.md)를 따른다.

네트워크 전투의 최종 유닛은 결과 화면에서 유지해 HP·사망·점유 해제를 관찰할 수 있다. 명시적인 Continue 또는 월드 종료에서 정리한다. 기존 Standalone은 즉시 정리하는 흐름을 유지한다.

전투 밖 노드 선택·Continue의 협동 결정권은 사용자에게 선택지를 제시한 상태다. 결정 전 네트워크 화면은 이 버튼을 읽기 전용으로 표시하며 자동화는 신뢰된 서버 진입점으로 전투 시작과 Continue를 실행한다. 캐릭터 조작권과 진행 선택권을 동일하게 취급하지 않는다.

## 2인 PIE 검증

자동화 이름은 `ProjectA.Coop.ListenServerClientCombat`이다. Gameplay 맵을 두 PIE 월드로 열고 Listen Server와 Client의 서로 다른 NetDriver, 열린 ClientConnections/ServerConnection을 확인한다. 사용자의 에디터 Play 설정은 transient 복제본으로 사용하며 저장하지 않는다.

```powershell
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject' -unattended -nop4 -RenderOffscreen -nosound '-ExecCmds=Automation RunTests ProjectA.Coop.ListenServerClientCombat' '-TestExit=Automation Test Queue Empty'
```

실제 Client RPC로 타인 캐릭터 명령 거절·회복약·이동·스킬·중복 거절·턴 종료를 검증한다. 이후 서버의 치명적 GAS 효과로 사망·승리 상태를 만들고 두 인스턴스의 유닛 값·Grid·승패·HUD와 Continue 후 정리를 비교한다. PIE 요청 전 편집기 NavMesh의 자동 생성 완료와 실제 이동 경로를 기다리고, 이동 요청 전 서버 NavMesh와 캐릭터의 바닥 안착·유효 경로를 확인한다.

맵 로드 직후 PIE를 요청하면 편집기 NavMesh 재생성이 완료되기 전에 PIE 복제가 시작될 수 있다. 엔진은 PIE 시작 때 편집기 생성을 일시정지하므로 테스트가 준비 여부를 먼저 확인해야 한다. 강제 재빌드나 고정 지연 없이 엔진의 정상 완료를 기다리며, 요청 목적지와 서버의 이동 성공 조건은 유지한다.

2026-09-10 순차 3번에서 Development Editor / Win64 빌드, 실제 2인 네트워크 PIE를 포함한 전체 자동화 42건(성공 26·경고 동반 성공 16·실패 0), 별도 Snapshot 상대 PIE 1건을 통과했다. 초기 NavMesh 준비 순서 오류와 실행 파일은 [대기열](T14_QUEUE.md)에 기록했다.

4번은 전투 뷰에 저장 대기·복구 중단 상태를, Run 표시 뷰에 확정 revision과 오류를 전달한다. 원래 참가자 연결이 끊기면 전투를 멈추고, 기존 Host와 원래 참가자를 배정한 새 세션에서 마지막 확정 턴을 복구한다. 전체 52건과 별도 프로세스·고정 상대 복구를 통과했으며 경계와 실행 명령은 [확정 턴 저장·복구](T14_CHECKPOINT.md)를 따른다. 5번 AI 검증의 참가자 전원 연결 조건은 유지한다. 3·4인, 손실/지연 조건, 기존 세션 재접속 UI, 명시적 Host 승계·불참자 AI 이어하기, 실제 인증/MMR 및 발사체별 시각 효과 동기화 검증은 후속 범위다.
