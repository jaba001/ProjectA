# T14 로컬 상대 Snapshot

사용자 선택: **1A 로컬 Snapshot 전투 우선 + 2B Unreal SaveGame v1**. 현재 전투 한 사이클을 검증한 뒤 Listen Server Co-op 동기화를 다음 구현 단계로 잡는다. T14 전체 완료는 Co-op 다중 PIE 조건까지 충족해야 한다.

## Unreal 기본 기능과 데이터 경계

`FPartySnapshot`과 하위 `USTRUCT`는 Actor·Controller·Ability 인스턴스를 보유하지 않는다. `UPartySnapshotSaveGame : USaveGame`에 이 데이터를 담고 `UGameplayStatics::CreateSaveGameObject`, `SaveGameToSlot`, `LoadGameFromSlot`으로 저장·불러온다. 작은 파티 데이터를 전투 밖에서 처리하므로 동기 API를 사용한다. 진행 중 자동 저장이나 대용량 데이터로 확장할 때는 비동기 SaveGame API를 적용한다. [Epic 저장·불러오기 안내](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine)

`UPartySnapshotLibrary`는 저장 데이터 검증과 슬롯 I/O를 담당한다. `UOpponentSnapshotCatalogDataAsset`은 고정 ID를 신뢰된 적 클래스·스킬 에셋으로 해석한다. `AEncounterManager`는 해석한 데이터로 `AEnemyUnit`을 생성하고 기존 `ConfigureProfession → RegisterUnits → StartCombat → Result → Cleanup` 경로를 재사용한다. 저장 데이터에서 임의의 Actor 클래스 경로를 로드하지 않는다.

## v1 계약

| 필드 | 의미 |
|---|---|
| SchemaVersion | 저장 구조 버전. 현재 1만 허용 |
| ContentVersion | 카탈로그 콘텐츠 버전. 실행할 카탈로그의 버전과 일치해야 함 |
| SnapshotId / MemberId / ClassId | Snapshot·파티원·직업의 고정 식별자 |
| CharacterName | 표시 이름 |
| Stats | MaxHP·CurrentHP·MaxActionPoints·MaxSubActionPoints·MoveRange |
| SkillIds | 순서 있는 스킬 목록. 첫 스킬이 기본 공격이며 나머지는 추가 슬롯 |
| EquipmentIds / TacticsId | 후속 장비·전술 식별자. 저장·복원은 가능하며 현재 전투 실행은 빈 값만 지원 |
| FormationSlot | 월드 좌표가 아닌 아레나의 상대 배치 슬롯. `Arena.EnemyCoords[FormationSlot]`으로 변환 |

현재 1~4명, 배치 슬롯 0~3, 스킬 1~5개를 지원한다. 파티원 ID·배치·스킬 ID 중복과 동일 AbilityClass 중복 장착을 거절한다. 수치 검증 상한은 HP 1,000,000, AP/SubAP 100, 이동 범위 32이며 밸런스 목표 수치가 아니다. ID 형식·유효 수치·카탈로그·실제 Enemy 영역 배치를 전체 검사한 뒤 유닛을 생성한다.

현재 HP는 저장값 그대로 적용하며 자동 완전 회복하지 않는다. HP 0은 데이터로 저장할 수 있지만 현재 Snapshot 전투에서는 거절한다. 사망한 상대 파티원의 포함·제외 및 경쟁 콘텐츠의 HP 정규화 정책은 별도 결정 사항이다. 적 AI의 아이템 사용은 미구현이므로 Snapshot 적에게 회복약을 자동 지급하지 않는다. 플레이어의 기존 전투 한정 SkillPool 지급은 유지하고 상대의 저장된 스킬 목록은 무작위로 변경하지 않는다.

버전 불일치·알 수 없는 ID·미지원 장비/전술·잘못된 배치는 오류를 표시하고 Run Map으로 복구한다. 실패 시 기존 PvE 적으로 대체하지 않는다. 저장 검증 실패는 기존 슬롯 파일을 덮어쓰지 않으며, 로드 검증 실패는 호출자의 기존 Snapshot 값을 보존한다. 이전 버전의 자동 변환은 아직 구현하지 않는다.

## 실행 방법

1. Development Editor / Win64 빌드를 완료한다.
2. Unreal Editor의 Python 실행 또는 PythonScript commandlet으로 [ConfigureSnapshotContent.py](../Source/ProjectAEditor/Scripts/ConfigureSnapshotContent.py)를 실행한다. 샘플 상대 Blueprint, 카탈로그, GameMode 카탈로그 연결과 `SampleOpponent` 저장 슬롯을 만든다. 기존 샘플 슬롯은 덮어쓰지 않는다.
3. Editor 실행 인자에 `-ProjectAOpponentSnapshot=SampleOpponent`를 전달한다. 명령줄 지정은 에셋을 수정하지 않는다. 메뉴에서 이어하기를 선택하기 전부터 같은 상대 설정을 사용하도록 실행 인자로 통일한다.
4. MainMenu에서 새 파티를 만든 뒤 기존 Run 노드를 선택한다. 저장된 상대와 전투하고 Victory → Result Continue → 다음 노드 흐름을 진행한다. Defeat는 기존처럼 Run 종료다.
5. 실행 인자를 제거하면 기본 PvE Run을 사용한다.

프로젝트 루트 PowerShell에서 최초 샘플 생성:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' "$PWD\ProjectA.uproject" -run=pythonscript "-script=$PWD\Source\ProjectAEditor\Scripts\ConfigureSnapshotContent.py" -unattended -nullrhi -nop4
```

이후 Snapshot 모드로 Editor를 열고 MainMenu에서 PIE 실행:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' "$PWD\ProjectA.uproject" -ProjectAOpponentSnapshot=SampleOpponent
```

샘플은 Hunter 1명, MaxHP 140 / CurrentHP 120, AP 2 / SubAP 1, 이동 범위 1, 배치 0, DefaultAttack + SweepingStrike를 저장한다. 현재 두 Run 노드는 같은 상대 슬롯을 전투 시작마다 다시 불러온다. 네 직업의 상대 외형은 현재 같은 Blueprint를 사용하며 직업별 외형·스킬 콘텐츠 완성을 의미하지 않는다. 카탈로그의 `Skills` 맵 키가 Snapshot의 고정 ID이며 기존 스킬 에셋의 표시명과 독립적이다.

Blueprint에서도 `Make PartySnapshot`으로 데이터를 구성해 `PartySnapshotLibrary.SaveSnapshot/LoadSnapshot`을 호출할 수 있다. `SlotId`는 영문·숫자·밑줄 1~64자이며 상대 데이터는 `ProjectA_Opponent_<SlotId>`에 저장된다. Windows 개발 환경에서는 `Saved/SaveGames/ProjectA_Opponent_SampleOpponent.sav`에서 확인할 수 있다. 샘플 `.sav`는 로컬 생성 데이터이며 Git·패키지에 포함하지 않는다. 패키지에서 샘플을 사용하려면 해당 실행 환경에서 저장 API로 먼저 생성해야 한다.

Snapshot 모드의 Run 체크포인트는 기본 `ProjectA_SnapshotRun_<SlotId>`로 분리하며 PvE의 `ProjectA_Run`을 덮어쓰지 않는다. 명시적인 `-ProjectASaveSlot=...`은 기존처럼 우선한다. 이어하기에는 같은 상대 실행 인자가 필요하다. 상대 파일 자체의 내용은 체크포인트에 복사하지 않으므로, 진행 중인 Run에서 해당 상대 슬롯을 덮어쓰면 다음 전투는 새 저장값을 사용한다. 고정 상대 이력과 온라인 매칭은 후속 범위다.

## Co-op으로 넘어가는 기준

로컬에서 저장·불러오기, 상대 생성, 아군 직접 조작, 상대 AI의 실제 피해, 사망, 결과 1회, 유닛/턴/점유 정리와 다음 노드 진행을 검증한다. 이 사이클이 안정되면 같은 데이터·행동 검증 경계를 유지하며 Listen Server/Client로 확장한다.

다음 단계에서는 원래 소유자만 조작할 수 있는 계정·캐릭터 매핑, 서버 Action Request, 턴·Grid·HP/AP·사망·결과 복제 및 클라이언트 HUD를 구현하고 최소 두 PIE 인스턴스로 확인한다. 최대 4인, 본인 캐릭터 고정, 기존 Host 유지와 명시적 Host 승계·불참자 AI 이어하기, 시작 시 AI 전환 사전 동의, 이후 인간 참가자만 MMR 반영, 마지막 확정 턴 경계 복구로 기획을 확정했다. 상세 합의와 남은 정책은 [Co-op 확정 기획](T14_COOP_DESIGN.md)을 따른다. Steam/EOS, Lobby, Backend, 매칭, 경쟁 결과 검증은 별도 작업이며 이번 로컬 Snapshot 검증에 포함하지 않는다. [T14 기획](TODO.md)

현재 `FPartySnapshot`은 상대 빌드 저장값이며 아군 계정 소유권·Run 참가자·Host 승계·턴 상태 복구를 구현하지 않는다. 기존 `URunSaveGame`도 전투 밖 체크포인트만 저장한다. Co-op 복구 데이터는 별도 후속 계약으로 설계하며 기존 Snapshot v1과 같은 기능으로 취급하지 않는다.

Unreal은 멀티플레이를 염두에 둔 초기 설계를 권장한다. 따라서 Co-op 구현을 다음 단계로 두더라도 새 데이터와 명령의 검증을 로컬 UI에 강하게 결합하지 않는다. [Epic 네트워크 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine?application_version=5.7)

## 검증

2026-09-10 Development Editor / Win64 빌드와 전체 자동화 32건(성공 18·경고 동반 성공 14·실패 0)을 통과했다. 별도 Snapshot 선택 프로세스의 `ProjectA.VerticalSlice.SavedMapsPIELoop` 1건도 통과했다. 저장 왕복·거절, 카탈로그 해석, 실제 Gameplay 준비 실패 복구, Run 체크포인트 분리와 기존 PvE 회귀를 검증했다.

Snapshot PIE는 `.sav`에서 읽은 이름·클래스·HP/AP/SubAP·이동 범위·스킬 순서·배치를 실제 적과 대조하고, Slate Move/Skill/타일 입력, 적 AI의 실제 스킬 피해, 자연 승리, Continue, 다음 노드, 강제 GAS 패배 및 결과 1회·점유/턴/유닛 정리를 확인했다. 전투 화면도 확인했다. 리포트는 `Saved/Automation/T14Full/index.json`, `Saved/Automation/T14SnapshotPIE/index.json`, 빌드는 `Saved/Automation/T14Build.log`다. 경고는 테스트 월드의 NavMesh/액터 정리 및 기존 엔진 설정 관련이며 실패는 없다.

패키지 재빌드와 Listen Server/Client 다중 PIE는 미실행이다. T14 전체는 미완료로 유지한다. 실행 결과는 [작업 보드](TODO.md)에도 기록하며, 로컬 SaveGame은 온라인 상대 데이터·경쟁 결과의 신뢰성을 보장하는 수단으로 취급하지 않는다.
