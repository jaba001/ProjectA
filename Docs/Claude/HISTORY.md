<!-- author: Claude · owner: Claude · Claude가 직접 구현·커밋한 작업의 완료 이력 -->
# Claude 완료 이력

Claude가 직접 컴파일하고 커밋한 작업을 기록합니다. 2026-10-11 작업(HISTORY 분할, 패키지 빌드, 상태이상 8종)은 [Codex HISTORY 9-56~9-61](../Codex/HISTORY.md)에 있습니다. 실제 게임 동작은 사용자 확인 전이며, 컴파일 성공을 작동 검증으로 보지 않습니다.

## 1 2026-10-12

### 1-1 2026-10-12 단어 단위 줄바꿈과 크래시 수정

- `5aadb7fb`: UE 5.8은 한국어를 줄바꿈할 때 한글끼리만 붙여 둡니다. 그래서 "1명", "AP·SAP"처럼 숫자·기호가 섞인 단어가 중간에서 끊겼습니다. `FProjectALineBreakIterator`(엔진 ICU 후보 중 공백 경계만 허용, 한자·가나는 글자 단위 유지)와 `UProjectATextBlock`을 추가하고, C++에서 생성하는 텍스트 90곳을 교체했습니다.
- `b9fb17a9`: 위 교체에서 `ConstructWidget<UProjectATextBlock>(UTextBlock::StaticClass())`처럼 클래스 인자가 맞지 않는 곳이 41곳 있었습니다. 위젯이 null이 되어 메인 메뉴에서 크래시가 났고, UI 캡처 PIE를 실행하다가 발견해 수정했습니다. **5aadb7fb~7d3e6e7a 구간의 main은 메인 메뉴에서 크래시가 납니다.**
- 회귀 테스트: `ProjectA.UI.LineBreak`(미실행)

### 1-2 2026-10-12 Claude 제안 처리

사용자가 "Claude 제안도 전부 해줘"라고 위임해서 권장안을 기준으로 처리했습니다. 세부 상태는 [PROPOSALS](PROPOSALS.md)에 있습니다.

| 제안 | 커밋 | 내용 |
| --- | --- | --- |
| 1 저장 빈도 | `5549cff7` | 계획 편집은 메모리에만 반영하고, 저장은 라운드 시작과 LockPlans에서만 합니다. 복구된 저장 준비를 철회할 때만 즉시 저장합니다. |
| 2 Revision | `5549cff7` | 슬롯별 "자기 마지막 변경" 기준으로 판정합니다. RPC는 그대로이고, 같은 슬롯의 이전 요청은 계속 거절합니다. |
| 3 복제량 | `d6548142` | 유닛 Status 문구를 NotReplicated로 지정하고, 보이는 상태가 바뀔 때만 ForceNetUpdate를 호출합니다. |
| 4 CSV | `7d3e6e7a` | 선택 1(행 수 계산)만 적용했습니다. DataTable 전환은 보류했습니다. |
| 5 Steam | 없음 | 현재 단계를 유지합니다. |
| 6 저장소 | 문서 | [외부 에셋 복원 목록](EXTERNAL_ASSETS.md)을 작성했습니다. |
| 9 위생 | `2a9d7224` | UPROPERTY raw 포인터 14개를 TObjectPtr로 바꿨습니다. AGENTS.md 규칙 문구는 동시 수정이 끝난 뒤 반영합니다. |

검증: 각 커밋 후 Development Editor / Win64 컴파일 성공. 자동화 테스트·2인 PIE·복제량 측정은 실행하지 않았습니다.

### 1-3 2026-10-12 스킬 상태이상 잠정 데이터

`7d3e6e7a`: `DataCatalogs/SKILL_STATUS_EFFECTS.csv`에 공격 스킬 43개의 상태이상을 속성별로 넣었습니다. 불→화상, 냉기→냉각(보라 이상은 빙결 추가), 번개→감전, 카오스→중독(혜성탄은 혼란), 물리 참격·엄니→출혈, 바위 계열→스턴입니다. 수치는 등급별 잠정값이며, 사용자가 직접 플레이하며 확인하도록 바로 적용해 달라고 요청했습니다. 새 Run은 이 값을 `FRunSkillBalance`에 고정하고, 이전 Run은 상태이상 없이 유지되며, DebugCombat은 현재 표를 바로 읽습니다. 회귀 테스트: `ProjectA.Run.SkillStatusEffects`(미실행)

### 1-4 2026-10-12 영어·한국어 UI 레이아웃

`a02472fd`: 사용자 허락을 받아 왼쪽 모니터 PIE로 ko/en 화면을 각각 10장 캡처해 확인하고 고쳤습니다.

- 장비 슬롯 이름·상태: 한 줄 + 말줄임표로 바꿨습니다. 이전에는 68px 고정 폭에서 글자 단위로 3~4줄씩 쪼개졌습니다.
- 난이도 카드: 고정 줄바꿈 폭 276을 없애고 카드 폭에 맞춰 줄바꿈합니다.
- 백과사전 탭: 띄어쓰기에서 두 줄로 나누고, 한 단어 라벨은 축소만 합니다(`ProjectATextFit::ShrinkToFit`).
- 전투 대상·행동 패널: 폭을 340에서 400으로 넓혔습니다.
- 캐릭터 생성: Designer 라벨이 번역되지 않던 문제와, 아이콘이 없을 때 흰 칸이 보이던 문제를 고쳤습니다.
- 검수 도구: `ProjectA.TodoReview.UiLayoutScreens`(ko/en). 실행 명령은 아래와 같고, 결과는 `Saved/Automation/TodoReview/UiLayout/`에 저장됩니다.

```
UnrealEditor.exe ProjectA.uproject -unattended -nop4 -nosound -ProjectAUiLayoutReview -ProjectAReviewLeftMonitor -ProjectASaveSlot=ProjectA_Automation_UiLayout_<32자리 GUID> -nowrite -ExecCmds="Automation RunTests ProjectA.TodoReview.UiLayoutScreens" -TestExit="Automation Test Queue Empty"
```

제한: 1280×720(16:9)만 확인했습니다. 4:3·21:9와 상점 툴팁, 보상 화면은 캡처하지 않았습니다. 기존 WBP 에셋의 정적 라벨은 바꾸지 않았습니다.

### 1-5 2026-10-12 미로 석재 질감

사용자가 미로 벽·바닥·천장이 너무 매끈하다고 해서 `ConfigureEncounterSurfaces.py`의 `-EncounterStoneDetailOnly` 모드로 프로젝트 재질 3개를 다시 만들었습니다. 원본 텍스처·재질은 SHA256으로 보존을 확인했습니다.

- 노말 강도를 0.25에서 1.1로 올렸습니다. 거칠기 하한은 0.8에서 0.6, 반사는 0.15에서 0.25로 바꿨습니다.
- 벽도 바닥·천장처럼 평면 투영 + 접선 공간 노말을 씁니다. 이전 `WorldAlignedNormal` 벽 그래프는 노말을 강화하자 일부 면이 빛 반대쪽으로 뒤집혀 검게 보였습니다.
- 블록 모서리 어둡게 하기(큐브 면 UV 기준 폭 0.07, 50%)와, `T_Stone_Wall_02` 9m 샘플로 큰 단위 명암 변화(0.85~1.2)를 추가했습니다. 모든 값은 재질 파라미터라 에디터에서 조정할 수 있습니다.
- 바닥·천장 텍스처는 Codex가 맞춘 블록·줄눈 통일을 유지하기 위해 벽 텍스처 그대로 두었습니다.
- 검증: 재질 컴파일 오류 0. Codex의 실내 캡처 도구로 수정 전후 4장을 비교했습니다(`Saved/Screenshots/Encounter/2026-10-12/StoneDetail{Before,After}`). 실제 PIE 이동·성능은 확인하지 않았습니다.
### 1-6 2026-10-13 미로 이음새 틈 막기

사용자 보고: 벽과 천장, 벽과 바닥 사이 틈으로 바깥 하늘색이 보였습니다. 원인은 바닥·천장 아래 줄눈 판, 벽 뒤 줄눈 판, 벽 블록이 같은 선에서 맞닿기만 하고 겹치지 않아 렌더링 틈이 생긴 것이었습니다. 두 벽이 만나는 바깥 모서리도 비어 있었습니다. `EncounterDungeonRoute::PrepareGeometry`를 다음과 같이 고쳤습니다.

- 바닥·천장 줄눈 판을 각 방향으로 32cm(벽 두께) 넓혀 벽 아래·위와 겹치게 했습니다.
- 벽 뒤 줄눈 판을 바닥 판 아래와 천장 판 위로 22cm씩 늘렸습니다.
- 노출된 두 벽의 끝점이 실제로 만나는 모서리에만 32×32cm 기둥을 추가했습니다. 통로·문 개구부는 막지 않습니다.
- 모두 벽 바깥이나 표면 뒤에 있어서 실내 표면의 모습은 바뀌지 않습니다.
- 검증: Development Editor / Win64 컴파일 성공. 실내 캡처 4장(`Saved/Screenshots/Encounter/2026-10-12/StoneDetailSealed`)에서 개구부가 유지되고 틈이 보이지 않는 것을 확인했습니다. 이동하면서 모든 각도를 확인한 것은 아닙니다.