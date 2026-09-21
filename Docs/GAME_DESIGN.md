# 게임 기획

기준일: 2026-09-21. 목표 기획·확정 방향과 구분된 콘텐츠 제안을 정의한다. 구현 상태는 [PROJECT_PLAN](PROJECT_PLAN.md), 미결정 사항은 [TODO](TODO.md), 세부 네트워크 정책은 [MULTIPLAYER](MULTIPLAYER.md)를 따른다. 아이템 클론 기획은 [7절](#7-아이템-클론-기획), 라운드 계획·시간차 자동 전투의 확정 방향은 [8절](#8-라운드-계획과-시간차-자동-전투)에 둔다.

## 1 게임의 정체성

**파티 빌드를 성장시키고 상대 Party Snapshot과 직접 전술 전투를 수행하는 협동 로그라이크**를 목표로 한다.

| 핵심 요소 | 설계 방향 |
|---|---|
| 파티 성장 | 직업·장비·스킬·역할의 조합 |
| 로그라이크 Run | 선택·보상의 누적과 빌드 변화 |
| Async PvP | 상대의 동시 접속 없이 저장된 빌드와 전투 |
| Grid 전투 | 라운드마다 이동·대상·스킬을 계획하고 시간차 자동 전투로 해결 |
| Co-op | 원래 소유자가 자신의 캐릭터를 조작하는 공동 Run |

For The King의 파티 운영과 The Bazaar·Backpack Battles의 비동기 상대 빌드를 참고한다. 설계 기준은 단위 시간당 의미 있는 선택의 양이다. 반복 이동·긴 카메라 전환·불필요한 잔여 턴·메뉴 대기는 최소화한다.

## 2 목표 Run과 현재 구현

```text
파티 생성 → Run 시작 → 상점/이벤트/훈련/강화/회복
→ 상대 확인·전투 준비 → Async PvP → 결과·보상 → 다음 Round
→ 최종 Encounter → Run 결과
```

현재 구현은 동일 Gameplay의 두 전투 노드를 시간차 라운드 전투로 진행하며 패배 시 Run을 종료한다. 새 Run의 첫 승리 뒤에는 상점1·상점2·상점3을 고정 제시하고 하나를 선택한다. 상점은 이름 표시·퇴장만 제공하며 두 번째 전투로 연결한다. 마지막 승리 뒤에는 기존 Run 종료를 유지한다.

| 영역 | 현재 구현 | 확장 목표 |
|---|---|---|
| 성장 | 이름·직업·HP·노드 진행 보존 | Run 단위 장비·스킬·보상 누적 |
| 스킬·아이템 | 실제 장착 스킬의 라운드 변환·SAP 위치 이동 예약. 공통 시험 행동과 자동 추가 스킬 부여 제거 | 획득·장착·소모·강화 규칙 |
| 상대 | PvE·로컬 Snapshot·적 AI | 온라인 Build·Formation·Tactics |
| 협동 | Listen Server 계획 요청·원래 소유권·전투 외 관리 재개. 최신 작동 검증 대기 | 계정·초대·P2P·공유 저장 |
| 경쟁 | 인간 참가자 대상 MMR 방향 | 결과 검증·정산·매칭 |

## 3 UI 중심 진행과 전투

물리적 WorldMap 탐험 대신 Node Map 또는 진행 UI를 사용한다. 지역 배경·음악·Arena·상대 Pool·상점·이벤트로 진행을 표현한다. 플레이어별 월드 이동 턴·광범위한 절차 생성·파티 분산은 현재 범위에서 제외한다.

사용자 지시에 따라 기존 개별 유닛 순차 전투를 시간차 라운드 전투로 교체한다. 별도 순차 모드나 기존 AI 실행 경로는 유지하지 않는다. 기존 4×4 Grid와 Run·직업·소유권을 연결하며 Blueprint 참조용 외형만 보존한다. Act·전투 라운드 수·목표 시간은 미정이다. 목표 Run의 다음 Round와 전투 내부의 행동 라운드를 구분한다.

## 4 상대 Snapshot과 전술

파티 구성·Class·Stats·Skills·Equipment·Formation·버전을 값 데이터로 저장하고 기존 Unit/Combat 흐름으로 상대 AI를 생성한다. 초기 구현은 로컬 USaveGame을 사용한다.

장비·전술 ID는 저장 구조만 지원하며 현재 전투에서는 빈 값만 허용한다. HP·전열·후열 우선순위, 공격·방어·지원 성향, 스킬 우선순위는 후속 기획 후보이다. 편집 UI와 평가 규칙은 미구현이다.

온라인 Snapshot·결과 검증은 별도 설계 대상이다. 공격 결과가 상대 소유자의 성적에 영향을 주는지는 미정이다. 저장 형식·지원 한계는 [MULTIPLAYER](MULTIPLAYER.md)에 정의한다.

## 5 협동 확정 정책

- 최대 4인. 원래 소유자만 자신의 캐릭터를 인간 조작하며 대체 참가자를 허용하지 않는다.
- 최초 Host는 1번, 이후 참가자는 최초 합류 순서대로 2·3·4번을 유지한다. 명시적 재개 시 현재 인간 참가자 중 가장 작은 번호가 Host를 승계한다.
- 정상 종료·돌발 끊김만으로 Host·AI 모드를 변경하지 않는다.
- AI 전환은 사전 동의 없이 Host가 결정한다. 단독 전환 시 본인이 Host가 되고 나머지는 Run 종료까지 AI를 유지한다. 소유자의 복귀로 인간 조작을 복원하지 않는다.
- Host 승계는 타인 캐릭터의 조작권을 이전하지 않는다. 현재 Host만 노드·Continue를 결정한다.
- 준비 완료는 계획과 전투 복구 상태의 저장 성공 후 확정한다. 강제 종료 후 마지막으로 저장한 준비 완료 상태를 복구하며, 이전 순차 Combat 저장은 파일을 보존하고 거절한다. 진행 중 시전·투사체의 임의 시점 복원과 경쟁 재실행 방지 정책은 별도다.
- AI 전환 후 현재 인간 참가자에게만 MMR을 반영한다. 정산·계산·이탈 악용 대응은 미정이다.
- Unreal Listen Server·Steam P2P와 Steam+PlayFab을 우선한다. 무료 개발 범위와 운영비를 구분하며 유료 리소스는 별도 승인 없이 활성화하지 않는다.

장비·스킬의 동시 관리와 Ready 기반 진행을 지향한다. 공동 재화·배분·대기/이탈 처리·1인당 캐릭터 수·부족 슬롯 배정·AI 소유자 관전은 추가 결정 대상이다.

## 6 구현 원칙과 다음 콘텐츠

기본 Run은 싱글플레이를 유지한다. 동일 Gameplay 레벨에서 Encounter를 교체하고 Streaming·Level Instance는 콘텐츠 요구에 따라 검토한다. 영속 데이터는 Runtime Data·Command를 사용하며 Actor·Controller·Widget·Ability는 실행 중 표현을 담당한다.

개발 순서는 Vertical Slice 유지 → Run 선택 확장 → 공통화·네트워크 확장이다. 전체 Replication 전환·자동 Host Migration·Dedicated Server·복잡한 월드 생성은 일괄 추진하지 않는다.

확정 방향은 **전투 → 인카운터 3개 제시 → 하나 선택 → 인카운터 진행 → 다음 전투**다. 초기 범위는 동일 기능의 빈 상점 3개로 제한한다. 상품·재화·보상·회복은 제공하지 않는다. 이후 후보 풀·출현 가중치·조건을 확장하며 실제 확률·중복 제시·재출현 규칙은 별도 결정한다.

평가 항목은 빌드 선택의 전술적 영향, 승패 원인의 설명 가능성, AI 허점 의존도, 협동 의사결정 대비 대기 시간이다. 착수 순서는 [TODO](TODO.md)에서 관리한다.

### 6-1 직업 역할 보충 제안

원작 역할을 검토하기 위한 참조 초안이며 현재 ProjectA의 선택 가능한 직업 목록이 아니다. 현재 네 직업과 초기 능력치는 [6-2절](#6-2-네-직업과-공통-시작-능력치)을 따른다. 원작 값은 `Characters.json`의 `Stats`, `Things`, `Passives`와 `SkillConfigs.json`·`Langs/en.json`의 `UI_ENCYCLOPEDIA_SKILL_*` 설명을 대조했다. 아래 HP·SPD·능력치를 ProjectA 초기 수치로 채택하지 않는다. 직업의 `Things`는 해당 정의에 직접 연결된 지급품이며 난이도·Loadout까지 반영한 최종 시작 인벤토리가 아니다.

| 직업 · 원작 ID | 설치본 정적 근거 | ProjectA 역할 제안 · 미확정 |
|---|---|---|
| Hunter · `HUNTER` | HP 30·SPD 74·AWR 78. `BOW_HUNTER_BASIC_00`와 `HERB_HERMIT_01` 각 1개. `SKILL_ELITESNEAK`는 탐험 잠입 관련 | 정밀 원거리 공격수. 기본 사격·관통·제어를 선택하고 대상 이동과 투사체 비행을 고려. 탐험 능력의 Node 변환은 별도 결정 |
| Scholar · `SCHOLAR` | HP 30·SPD 68·INT 78. `BOOK_SCHOLAR_BASIC_00`와 `SCROLL_TELEPORT_01` 각 1개. `SKILL_FINDSCROLL`은 탐험/던전의 스크롤 획득 기회 | 준비형 마법 공격·지원. 시전 시간과 보호 배치를 교환. 주문 효과·시전 시간·Node 정보 제공은 별도 결정 |
| Herbalist · `HERBALIST` | HP 32·SPD 62·INT 76·AWR 76. `STAFF_HERBALIST_BASIC_00`와 `HERB_GODSBEARD_01` 각 1개. `SKILL_FINDHERB`, `SKILL_PARTYHEAL` 연결 | 배치형 회복 지원. 단일 회복과 주변 회복을 선택하며 집결의 이익과 적 범위 공격 위험을 교환. 범위·시점·소모 비용 별도 결정 |
| StableHand · `STABLEBOY` | HP 30·SPD 78·STR 72. `BLADE_STABLEBOY_BASIC_00`와 `HERB_NETTLE_01` 각 1개 | 기동 근접 공격수. 접근·타격·복귀를 활용하며 현행 한 주요 행동 예약을 유지. 고유 효과는 이동·배치 역할을 기준으로 별도 설계 |

네 원작 직업 정의는 모두 `PA=1`, `SA=1`이며 위 네 직업 중 SPD 최댓값은 Stablehand 78이다. 원작 SPD를 ProjectA `CombatSpeed`·이동 속도·투사체 속도 전체에 복사하지 않는다. 패시브의 `PROC_*` 숫자는 실행 수식 확인 없이 최종 발동률로 쓰지 않는다. 이 초안은 직업별 최종 스탯·성장률·추가 행동을 확정하지 않으며 [8절](#8-라운드-계획과-시간차-자동-전투)의 전투 계약을 따른다.

### 6-2 네 직업과 공통 시작 능력치

2026-09-16 사용자 확정: 선택 가능한 직업은 아래 네 개이며 이전 테스트 직업은 선택 화면에서 제거한다. 공통 직업 베이스와 직업별 C++ 자식 클래스로 정의하고 표시 순서를 유지한다.

| 순서 | 직업 | ClassId | 직업 클래스 | HP | 힘 STR | 민첩 DEX | 지능 INT |
|---|---|---|---|---|---|---|---|
| 1 | 전사 | `Warrior` | `UWarriorProfession` | 100 | 10 | 10 | 10 |
| 2 | 마법사 | `Mage` | `UMageProfession` | 100 | 10 | 10 | 10 |
| 3 | 궁수 | `Archer` | `UArcherProfession` | 100 | 10 | 10 | 10 |
| 4 | 도적 | `Rogue` | `URogueProfession` | 100 | 10 | 10 | 10 |

현재 시작값이며 직업별 최종 밸런스·성장률·무기 보정을 확정한 것은 아니다. 2026-09-21 전사에 GKnight 외형과 검 공격을 추가했고 다른 직업의 외형·기존 AP·장착 스킬은 유지한다. 힘·민첩·지능의 피해·명중 보정은 별도다. [현재 스킬 구성](#8-7-기본-전투-전환과-스킬-데이터)

2026-09-16 사용자 요청으로 **민첩 1당 전투 속도 1**을 적용한다. 네 직업의 시작 속도는 10이며 일반 테스트 적의 힘·민첩·지능은 각각 5, 속도는 5다. 일반 적의 기존 HP 150·AP 2는 유지한다. Snapshot 적은 저장된 능력치를 사용하며 일반 적의 5로 덮어쓰지 않는다. 전투 속도는 [8-3절](#8-3-행동-시작-시간)의 시작 지연과 [8-4절](#8-4-공격-접근과-복귀)의 근접 접근·복귀에 적용한다.

이전 테스트 직업의 저장 데이터를 새 직업으로 추정 변환하지 않는다. 지원하지 않는 직업은 원본 파일을 보존하고 명시적으로 거절한다. 패키지 폴더 이동의 경로 리디렉션과 직업 콘텐츠 호환은 구분한다. 구현과 실행 확인은 [PROJECT_PLAN](PROJECT_PLAN.md#파티), [남은 확인](TODO.md#3-2-아이템과-직업)을 따른다.

### 6-3 싱글플레이 직접 조작 캐릭터

2026-09-16 사용자 확정: 일반 싱글 새 게임의 캐릭터 생성 화면에서 플레이어가 직접 조작할 한 명을 선택한다. 나머지 생성 동료는 서버 AI가 계획·실행한다. 선택과 원래 소유권을 분리하며 AI 동료를 선택 캐릭터의 사망 후 인간 조작으로 승계하지 않는다.

선택은 저장·이어하기·다음 전투에서 유지한다. 직접 조작 캐릭터가 사망해도 생존 동료가 있으면 AI 전투를 계속하고 전체 아군의 생존 상태로 승패를 판정한다. 관리 Run·협동·관리 싱글 전환의 기존 인간 참가자/소유권 정책은 유지한다. 기존 저장의 구체적 호환은 [PROJECT_PLAN](PROJECT_PLAN.md#저장과-멀티플레이-연결-경계), 확인 절차는 [남은 확인](TODO.md#2-9-싱글플레이-직접-조작-캐릭터-선택)을 따른다.

## 7 아이템 클론 기획

### 7-1 목적과 적용 상태

2026-09-15 사용자 확정: **집중(Focus) 자원과 회복·소모·판정 보정 효과를 기획에서 제외한다.** 집중초 C03·C04는 삭제하고 해당 ID를 재사용하지 않는다. 무기·직업·파이프·재질·야영에도 관련 효과를 추가하지 않는다. AP 회복이나 추가 행동으로 자동 치환하지 않는다. 2026-09-16 사용자 결정에 따라 공격 적중은 [8-5절의 서버 충돌 판정](#8-5-발동과-피격)을 사용한다. 원작 성공 슬롯은 참조값으로만 보존하며 기본 시간차 전투를 유지한다.

**시간차 전투를 유지하고, 원작 아이템 중 필요한 효과를 ProjectA에 맞게 변환한다.** 참고 작품은 For The King(이하 FTK1)과 For The King II(이하 FTK2)다. 무기 12종, 방어구 8종, 방패 2종, 장신구 4종, 약초 10종, 탐험 도구 4종, 성장 모델 2종의 총 42개 기획 항목을 정의한다. 전체 원작 아이템 도감이나 최신 패치 데이터의 완전 복제는 범위에 포함하지 않는다.

| 표시 | 의미 |
|---|---|
| 원작 확인 | 공개 자료에서 확인한 작품별 구조·효과·수치. 출처의 설명 범위만 사용 |
| 설치 파일 정적 확인 | FTK2 설치본 JSON의 정의·참조·설명문을 확인. 해당 설정의 실제 로드·최종 계산·작동은 미검증 |
| 클론 설계 | 확인한 기능을 재현하기 위한 기획 해석. 항목의 역할·상호작용·검증 목표 포함 |
| 변환 후보 | ProjectA에 도입할 때 변경 가능한 부분. 채택·구현 미확정 |
| 확인 필요 | 원작 자료가 비어 있거나 버전·세부 동작을 확인하지 못한 값. 0이나 없음으로 간주하지 않음 |

공개 자료 참조일은 2026-09-13이며 개발사 가이드와 커뮤니티 Wiki를 사용했다. 2026-09-15에는 사용자 제공 FTK2 Steam 설치본 Build `24247341`의 평문 JSON을 대조했다. 아래 설치본 표가 기존 공개 자료와 다르면 해당 Build의 정적 참조값으로 구분한다. FTK1 항목은 기존 작품의 근거를 유지한다. 원작 영문명은 대조용이고 한글 기능명은 기획용 가칭이다. 개별 행과 보충 표에 없는 가격·판정 횟수·지속시간·입수 확률은 **확인 필요**다. ProjectA의 장비·상점 거래·약초 구현 완료를 의미하지 않는다. 파일 경로·해시·해석 한계는 [7-11절](#7-11-ftk2-설치본-근거와-해석-범위)에 둔다.

읽는 순서는 작품별 구조 → 아이템 카탈로그 → 상세 카드 → 성장·상점 → 데이터 변환 순서다. 원작 수치는 `SourceValue`, 향후 조정 수치는 `DesignValue`로 분리한다. 자료가 없는 필드는 비워 두되 누락 사유를 기록한다.

8절의 전투 방향 확정 후에도 본 절의 원작 참조는 보존한다. 턴당 효과·Speed·AP/SubAP·면역·상태 지속은 새 전투의 행동 라운드·초 단위 시간·실시간 피격에 맞게 별도 변환한다. 원작 Speed를 이동속도·투사체 속도에 자동 연결하거나 기존 AP를 삭제하지 않는다.

### 7-2 두 작품의 구조와 클론 기준

| 구분 | FTK1 원작 확인 | FTK2 원작 확인 | 클론 기준과 변환 표시 |
|---|---|---|---|
| 무기 역할 | 무기와 사용하는 능력치의 적합성이 중요. 양손·파괴 가능 속성 존재 | 무기 카드가 피해·스킬을 제공하며 판정 성공 수가 피해에 관여 | 공격력·원작 판정 능력치 참조·스킬 목록을 별도 기록. 직업 전용 제한은 자동 추가하지 않음 |
| 장비 역할 | 몸·머리·발, 방패, 목걸이·장신구로 방어·능력치·면역·스킬 보완 | 손 장비 및 재질 보정 등 장비 조합 확대 | FTK1 슬롯과 FTK2 손 슬롯을 모델별로 구분. 최종 슬롯 수는 후속 결정 |
| 배치 | FTK2의 이동 가능한 전투 Grid와 구분 | 각 진영 2×4 배치, 행·열·주변 공격과 보호 관계 | ProjectA 4×4와 칸 번호를 직접 대응하지 않고 범위 패턴을 재정의 |
| 행동 자원 | 약초는 전투 중 턴당 사용 제한 | 주요 행동과 보조 행동 분리. 아이템과 이동이 보조 행동을 경쟁 | ProjectA AP·SubAP에 비용을 명시. FTK2 주요 행동 후 자동 턴 종료도 별도 이식 여부 결정 |
| 회복약 | Godsbeard의 고정 회복량이 파이프 단계에 따라 증가 | Godsbeard가 장비 등 추가 HP를 제외한 사용자 HP 기준으로 회복 | 고정값·기준 HP 비율을 별도 모델로 보존. 두 공식을 합산하지 않음 |
| 재질 | 본 기획에서 FTK2 재질표를 FTK1에 적용하지 않음 | 천·가죽·금속의 재질별 추가 보정 | 기본 아이템·등급·재질 분리. 제작·강화 기능의 존재까지 추정하지 않음 |

무기·장비 근거: [FTK1 무기](https://fortheking.wiki.gg/wiki/Bludgeon), [FTK1 방어구](https://fortheking.wiki.gg/wiki/Magic_Shield), [FTK2 무기](https://fortheking.wiki.gg/wiki/List_of_Weapons_FTK2), [FTK2 장비·재질](https://fortheking.wiki.gg/wiki/Attire_%28FTK2%29). Grid·행동·방어 근거: [IronOak Games 전투 가이드](https://steamcommunity.com/sharedfiles/filedetails/?id=2974574027). 약초·회복 근거: [FTK1 약초](https://fortheking.wiki.gg/wiki/Herb), [FTK1 Godsbeard](https://fortheking.wiki.gg/wiki/Godsbeard), [FTK2 Godsbeard](https://fortheking.wiki.gg/wiki/Godsbeard_%28FTK2%29).

### 7-3 공통 전투 규칙

**원작 판정 참조와 ProjectA 효과 변환을 분리한다.** 아래 능력치 판정·성공 개수·완전 성공·확률 예시는 원작을 대조하기 위한 자료다. ProjectA 공격 적중은 [8-5절의 실제 충돌](#8-5-발동과-피격)로 결정하며 명중 확률·성공 슬롯을 사용하지 않는다. 원작의 완전 성공은 모든 판정 슬롯 성공을 뜻하며 치명타나 적중 보장과 구분한다. 원작 스킬의 `일부 성공에도 피해`, `완전 성공 시 추가 효과`, `완전 성공해야 발동`은 참조 조건으로 기록하고, ProjectA의 부가 효과 발동 조건은 스킬별로 별도 결정한다.

원작 판정 참조용 예시: 보정 없는 독립 판정의 슬롯당 성공률을 80%로 가정하면 2회 완전 성공 확률은 64%, 4회는 40.96%다. 이는 판정 횟수의 의미를 설명하는 계산이며 특정 원작 무기의 실제 확률이나 ProjectA 명중률이 아니다. 회피·치명타·상태 저항을 포함한 실제 기대값으로 사용하지 않는다.

FTK2의 물리 방어와 마법 저항은 해당 피해를 감산하며 관통 공격은 방어를 우회한다. 클론에서는 `물리/마법`과 `방어 무시 여부`를 분리하고, 피해 하한·부분 성공 시 관통 여부는 스킬별로 확인한다. 예를 들어 물리 피해 12와 방어 4라면 일반 피해는 8이다. [개발사 방어 설명](https://steamcommunity.com/sharedfiles/filedetails/?id=2974574027)

| 기능 | 클론에 필요한 명세 | ProjectA 변환 시 주의 |
|---|---|---|
| 피해·회복 | 기준값, 산정 대상, 반올림, 적용 순서 | HP 스케일만 바꾸고 원작 계수를 그대로 사용하지 않음 |
| 완전 성공 효과 | 원작 발동 조건, 대상, 기본 피해와의 순서 | ProjectA 충돌 적중과 별개로 변환·제외 여부 결정. 충돌 적중을 완전 성공으로 자동 치환하지 않음 |
| 상태 이상 | 부여 조건, 지속 단위, 중첩, 해제·면역 | Stun·Dazed·Interrupt를 모두 기절 하나로 합치지 않음 |
| 이동·범위 | 선택 대상, 실제 피격 칸, 주변 감쇠, 보호 우회 | 방어 관통과 후열 보호 무시는 서로 다른 기능 |
| 장비 부여 스킬 | 장착 시 부여, 해제 시 회수, 중복 처리 | 직업 스킬·보상 스킬과 같은 ID일 때 출처별로 관리 필요 |
| 소모품 | 사용 가능 상태, 소비 수량, 행동 비용, 실패 시 처리 | 효과가 거절된 경우 수량·자원을 소모할지 명시 |
| 파괴 | 발생 조건, 예외, 파괴 후 행동 | 일반 내구도 소모와 구분. 재고·스킬·Snapshot 갱신 필요 |

#### 7-3-1 원작 상태와 ProjectA 상태의 차이

원작의 명칭을 ProjectA 효과 ID로 바로 변환하지 않는다. 다음은 `StatusEffects.json` 및 `Langs/en.json`의 `UI_ENCYCLOPEDIA_STATUS_*`를 확인한 결과다. `Duration`·`TickFrequency`의 정확한 감소 시점은 미확인이며 ProjectA의 초·라운드로 환산하지 않는다. ProjectA 규칙의 기준은 [STATUS_EFFECTS](STATUS_EFFECTS.md)다.

| ProjectA 상태 | 설치본 정적 확인 | 변환 시 구분 |
|---|---|---|
| 화상 | `STATUS_FIRE_00`: Duration 3, TickFrequency 1, TickCombat true. 도감은 주기 피해로 설명 | ProjectA의 피해 후 수치 감소와 별도. 최종 감소 공식은 미정 |
| 중독 | `STATUS_POISON_00`: Duration 3, TickOverworld true, TickCombat false, STR/VIT/INT/SPD/TAL/AWR 각 -10 | ProjectA의 피해 후 수치 1 감소와 별도. 피해 주기는 미정 |
| 출혈 | `STATUS_BLEED_00`: Duration 4, TickFrequency 2, TickCombat true. 도감은 주기 피해로 설명 | ProjectA의 실제 칸 이동마다 발동과 별도 |
| 냉각 | 이 파일의 최상위 ID·Type에서 CHILL/FREEZE/COLD에 대응하는 별도 상태를 확인하지 못함 | ProjectA 냉각 수치·시각표 정책의 근거로 사용하지 않음 |
| 빙결 | `STATUS_ICE_00`: Duration 3. Frozen/빙결의 영문 도감 설명은 공격으로 받는 피해 25% 증가 | ProjectA의 현재 또는 다음 라운드 행동 차단과 별도 |
| 스턴 | `STATUS_STUN_00`: Duration 1, 도감은 행동 불가. `STATUS_DAZE_00`: Duration 1, 도감은 보조 행동 불가 | ProjectA 스턴의 현재 잔여 행동 차단과 이월 없음은 자체 정책 |
| 혼란 | `STATUS_CONFUSE_00`: Duration 2, TickCombat true. 도감은 무작위 결정 | ProjectA 다음 라운드의 합법 행동 대체·소모 시점은 자체 정책 |
| 감전 | `STATUS_SHOCK_00`: Duration 2. 영문 도감은 첫 판정 슬롯의 성공을 방해하는 효과로 설명 | ProjectA의 중심·인접 번개 피해와 별도. 한글 설명에는 실패 의미가 누락되어 영문과 불일치 |

원작 `UI_ENCYCLOPEDIA_STATUS_IMMUNITY_STUN`은 Stun·Daze를 함께 설명하고 `...IMMUNITY_DAZE`는 Daze만 설명한다. 면역 연결 ID·설명문·실제 차단 집합은 서로 구분해 검증한다. 상태 정의만으로 중첩·재부여·정화 집합·동시 적용 순서를 확정하지 않는다. 기존 ProjectA 8종 정책은 이번 대조로 변경하지 않는다.

### 7-4 무기 카탈로그

표의 기능명은 클론 역할을 표현한다. 스킬 영문명은 원작 대조용이다. 수치 미기재 무기는 원작 수치 확인 후 완성하는 설계 카드이며, 임의의 피해·판정 수를 채우지 않는다.

| ID · 기능명 | 원작 기준 | 재현할 효과 | 클론 설계 목적 | 변환할 부분 |
|---|---|---|---|---|
| W01 수익형 류트 | FTK1 Simple Lute | 양손, 마법 피해 6, 골드 배율 +12%, Support Range. Alto 완전 성공 시 저항 무시, Dazzle 완전 성공 시 Stun | 공격과 제어·경제 기여를 한 무기에 결합 | Talent·수익 산정 주체·탐험 지원 범위 |
| W02 관통 입문 활 | FTK2 Simple Bow | 물리 활. Shot과 Pierce, Pierce 정확도 -5%, 완전 성공 시 관통 | 낮은 방어 상대의 안정 공격과 높은 방어 상대의 관통 선택 | Awareness·판정·방어 수치, 후열 보호와 관통의 분리 |
| W03 다기능 직검 | FTK2 Broadsword 계열 | Slash·Cutting Slice·Heavy Strike | 한 무기 안에서 일반·변형·강공격 선택 | 스킬별 AP 비용·정확한 효과 확인 |
| W04 범위 대검 | FTK2 Greatsword 계열 | Slash·Heavy Strike·Cleave | 단일 집중과 다수 처리 사이 선택 | 양손 슬롯·Cleave의 실제 칸 패턴·감쇠 |
| W05 독 단검 | FTK2 Assassin Blade | Stab·Poison Strike | 직접 피해와 지속 상태의 효용 비교 | 독 발동 조건·턴 수·중첩·면역 |
| W06 제어 망치 | FTK2 Hammer | 설치본 기준 Bash·Stun. 해당 고유 무기에 Knock Left 없음 | 피해와 행동 제어 선택 | VIT 판정·보스 제어 규칙. 밀기는 별도 무기 후보 |
| W07 방어 파쇄 곡괭이 | FTK2 Pick Axe | Piercing Blow·Armor Break | 즉시 관통과 후속 공격을 위한 방어 약화 비교 | 방어 감소 중첩·지속시간·관통 적용 조건 |
| W08 화염 지팡이 | FTK2 Fire Staff | 설치본 기준 Fire Bolt·Heavy Fire Bolt·Fire Blast와 FIRE 상태 연결. 집단 Interrupt 연결 없음 | 단일·강공격·범위 화상 선택 | 범위·화상 적용 조건·지속·AP |
| W09 폭발 화포 | FTK2 Heavy Cannon 계열 | Explosive Shot·Concussive Shot | 집중 배치에 대한 압박 | 재장전 유무·비용 확인, 폭발 범위·아군 피해 |
| W10 제어 구슬 | FTK2 Crystal Ball의 공격·제어 부분 | Bolt·Mezmerize를 채택 후보로 발췌 | 직접 피해와 행동 제어의 선택 | 나머지 원작 행동은 채택 대상에서 제외, 새 라운드 제어 규칙 적용 |
| W11 파괴 위험 무기 모델 | FTK1 Breakable 속성 무기 | 한 공격의 모든 판정 실패 시 파괴, 파괴 후 비무장 | 강한 무기를 잃을 위험과 예비 무기 운용 | 개별 무기의 실제 피해·파괴 정책 |
| W12 냉기 활 | FTK2 Bone Bow 계열 | 설치본 Tier 0은 Ice Arrow·Freeze, Tier 1은 Ice Pierce 추가. Freeze는 DAZE 상태 연결 | 냉기 공격·관통·보조 행동 제어 비교 | 스킬명 Freeze와 ICE 상태·ProjectA 빙결의 구분 |

W01: [Simple Lute](https://fortheking.wiki.gg/wiki/Simple_Lute). W02: [Simple Bow](https://fortheking.wiki.gg/wiki/Simple_Bow_%28FTK2%29). W03~W10은 목록에서 확인한 스킬 구성을 압축한 것이며 이름만으로 세부 효과를 확정하지 않는다. [FTK2 무기 목록](https://fortheking.wiki.gg/wiki/List_of_Weapons_FTK2). W11: [FTK1 무기 속성](https://fortheking.wiki.gg/wiki/Bludgeon). W12: [FTK2 활](https://fortheking.wiki.gg/wiki/Bow_%28FTK2%29).

#### 7-4-1 설치본 무기 수치와 스킬 연결

근거는 `Things/Weapons.json`의 고유 ID, `Equippable.Stats.ATK`, `Value`, `MinTier/MaxTier`, `Interactable.Abilities`다. 아래 수치는 재질·난이도·장착자 보정을 적용하지 않은 정의값이다. `Value`는 최종 판매가가 아니며 `ATK`는 모든 판정 결과의 확정 피해가 아니다. 스킬 뒤 괄호는 **판정 수 Rolls / 정확도 ACC 보정**이다. Tier가 다른 동명 무기는 별도 항목으로 취급한다.

| 카드 · 설치본 ID | ATK / Value / Tier | 판정 능력치 | 연결 스킬과 판정 |
|---|---|---|---|
| W02 · `BOW_HUNTER_BASIC_00` | 6 / 3 / -1 | AWR | Shot `BOW_BASIC_ATTACK` (2 / 0), Pierce `BOW_PIERCE_ATTACK` (2 / -5) |
| W03 · `BLADE_MILITIA_MEDIUM_03` | 26 / 227 / 3 | STR | Slash `BLADE_BASIC_ATTACK` (3 / 0), Cutting Slice `BLADE_BLEED_ATTACK` (3 / -5), Heavy Strike `BLADE_HEAVY_ATTACK` (4 / -20) |
| W04 · `BLADE_MILITIA_HEAVY_03` | 35 / 273 / 3 | STR | Slash `BLADE_BASIC_ATTACK` (4 / 0), Heavy Strike `BLADE_HEAVY_ATTACK` (5 / -20), Cleave `BLADE_SPIN_ATTACK` (4 / 0) |
| W05 · `BLADE_GOBLIN_ASSASSIN_00` | 8 / 28 / 0 | SPD | Stab `GOBLIN_PIERCE_ATTACK` (1 / 0), Poison Strike `MELEE_POISON_ATTACK` (1 / 0) |
| W06 · `BLUNT_MILITIA_MEDIUM_01` | 16 / 45 / 1 | VIT | Bash `BLUNT_BASIC_ATTACK` (4 / 0), Stun `BLUNT_STUN_ATTACK` (4 / 0) |
| W07 · `BLUNT_PICKAXE_MEDIUM_01` | 15 / 45 / 1 | STR | Piercing Blow `AXE_PIERCE_ATTACK` (3 / 0), Armor Break `AXE_ARMORDOWN_ATTACK` (3 / -5) |
| W08 · `STAFF_MILITIA_FIRE_01` | 20 / 50 / 1 | INT | Fire Bolt `MAGIC_FIRE_ATTACK` (4 / 0), Heavy Fire Bolt `MAGIC_FIRE_HEAVY_ATTACK` (6 / -20), Fire Blast `MAGIC_FIRE_SPLASH_ATTACK` (4 / -5) |
| W09 · `GUN_MILITIA_HEAVY_03` | 40 / 637 / 3 | STR | Explosive Shot `GUN_EXPLOSIVE_ATTACK` (3 / 0), Concussive Shot `GUN_CONCUSSIVE_ATTACK` (3 / -5) |
| W10 · `ORB_FORTUNETELLER_BASIC_00` | 6 / 5 / -1 | AWR | 채택 검토 대상인 Bolt `MAGIC_BASIC_ATTACK` (3 / 0), Mezmerize `MAGIC_STUN_ONLY_ATTACK` (2 / -10)만 발췌 |
| W12 · `BOW_BONE_LIGHT_00` | 8 / 15 / 0 | AWR | Ice Arrow `BOW_ICE_ATTACK` (2 / 0), Freeze `BOW_ICE_DAZE_ATTACK` (2 / -7) |
| W12 · `BOW_BONE_LIGHT_01` | 12 / 34 / 1 | AWR | Ice Arrow (2 / 0), Ice Pierce `BOW_ICE_PIERCE_ATTACK` (2 / -7), Freeze (2 / -7) |

W07에는 동명 `AXE_MINES_MINION_00`도 있으며 ATK 29·Value 227·Tier 3·Class AXE다. 위 표의 Class BLUNT인 Tier 1 곡괭이와 합치지 않는다. W05는 기능명상 독 단검이지만 설치 분류는 `BLADE`이며 SPD +8·CRT +8·SA +1·`SKILL_ELITESNEAK`도 연결된다. 원작 SPD 판정을 ProjectA의 실행 시각과 자동 결합하지 않는다.

`Abilities.json`과 연결해 확인한 추가 규칙은 다음과 같다.

| 대상 | 정적 확인 | 남은 확인 |
|---|---|---|
| W04 Cleave | `BLADE_SPIN_ATTACK.TargetArea=SWIPE` | ProjectA 4×4의 실제 피격 칸·감쇠. ROW와 동일하다고 가정하지 않음 |
| W07 Armor Break | `ADD_STATUS=STATUS_ARMORDOWN_00` | 상태 발동 성공 조건·방어 감소 계산·재부여 |
| W08 세 공격 | `ADD_STATUS=STATUS_FIRE_00`, Fire Blast의 `TargetArea=SPLASH` | 완전 성공 gate·SPLASH 대상과 화상 틱 경계 |
| W09 두 공격 | `TargetArea=SPLASH`, 무기와 스킬의 `Ammo=1` | 재장전 자원·행동 비용·최종 탄약 처리 |
| W12 Freeze | `BOW_ICE_DAZE_ATTACK`의 `ADD_STATUS=STATUS_DAZE_00` | DAZE 면역·보조 행동 차단의 정확한 만료 경계 |

공통 상속·최종 명중·완전 성공 조건은 수치 필드와 별도로 검증한다. `MinValue/MaxValue`만으로 성공 슬롯별 피해표를 생성하지 않는다. 이번 정정은 설치본 참조 보완이며 각 스킬의 ProjectA 비용·타이밍·상태 정책 채택을 의미하지 않는다.

### 7-5 방어구·방패·장신구 카탈로그

A01~A04와 R03~R04는 설치본 `Things/Attires.json`의 수치로 보충했다. 이전 [FTK2 Loadout 공개 자료](https://fortheking.wiki.gg/wiki/Lore_Store_%28FTK2%29/Loadout)와 다른 A04의 CRT 및 누락 보정은 아래에 구분한다. Lore 해금 비용·정의의 `Value`·최종 Run 상점 가격은 서로 다른 값이다.

| ID · 기능명 | 원작 기준 | 재현할 효과 | 클론 설계 목적 | 변환할 부분 |
|---|---|---|---|---|
| A01 균형 상의 | FTK2 Padded Jacket | 방어 +1·저항 +1 | 양 피해 유형의 입문 방어 | 피해 스케일·몸 슬롯 |
| A02 기절 방지 투구 | FTK2 Dented Helmet | 방어 +1·저항 +2·Stun 면역 연결 | 방어 수치와 행동 유지의 조합 | Stun·Dazed 면역 구분 |
| A03 회피 신발 | FTK2 Light Boots | 회피 +5·Entangle 면역 | 회피·이동 제약 대응 | 회피 단위·속박 태그 |
| A04 치명 장갑 | FTK2 Light Gloves | 설치 정의 CRT +10·저항 +1 | 손 슬롯을 통한 공격 보완 | CRT 적용 단위·손 슬롯 채택·확률 상한 |
| A05 물리 중갑 모델 | FTK1 Plate Armor 계열 | 물리 방어·Strength 중심, 낮은 저항과 능력치 불이익 가능 | 상대 피해 유형에 따른 선택 | 실제 아이템 지정·감산식·능력치 패널티 |
| A06 마법 로브 모델 | FTK1 Silk Armor 계열 | 저항·Intelligence 등 중심, 낮은 물리 방어 | 마법 대응과 주문 적합성 | 실제 아이템 지정·물리/마법 구분 |
| A07 역할 보완 머리 장비 모델 | FTK1 Headgear 계열 | 방어·능력치·면역·스킬 조합 | 부족한 방어 또는 특수 대응 보충 | 개별 아이템 지정·몸 장비와 예산 배분 |
| A08 재질 변형 방어구 모델 | FTK2 Attire | 기본 장비에 재질별 보정 | 같은 계열 안에서 다른 보조 능력 선택 | 재질 적용 가능 조합·추첨·고정 변형 중 선택 |
| S01 회피 도발 방패 | FTK2 Buckler 계열 | Taunt·Evade Up | 공격을 유도하고 회피로 대응 | 도발 대상 강제 범위·지속·방패 장착 조건 |
| S02 인접 보호 방패 | FTK2 Tower Shield 계열 | Taunt·Armor Up Neighbors | 인접 아군 방어 지원과 범위 공격 노출의 상충 | Neighbors의 실제 칸·효과량·보호 대상 |
| R01 마법 증폭 장신구 | FTK1 Amplifying Crystal | 마법 피해 +1·치명타 확률 +4% | 주문 공격의 직접 보완 | 고정 피해와 치명 배율의 계산 순서 |
| R02 탐험 나침반 | FTK1 Brass Compass | Find Distance +1·Movement +1 | 탐험 편의와 전투 장신구의 교환 | 월드 탐험 대신 Node 정보 제공으로 변환 여부 |
| R03 물리 방어 반지 | FTK2 Armored Ring | HP +3·방어 +2 | 특정 피해 유형 대응 | TRINKET 슬롯·중복 장착 규칙 |
| R04 마법 저항 반지 | FTK2 Magic Ring | HP +3·저항 +2 | R03과 상대 구성별 선택 | TRINKET 슬롯·마법 피해 채택 |

A05~A07은 특정 실물 아이템의 완성 수치 카드가 아닌 계열 클론이다. [FTK1 방어구](https://fortheking.wiki.gg/wiki/Magic_Shield). A08: [FTK2 재질](https://fortheking.wiki.gg/wiki/Attire_%28FTK2%29). S01~S02: [FTK2 방패 목록](https://fortheking.wiki.gg/wiki/List_of_Weapons_FTK2). R01~R02: [FTK1 장신구](https://fortheking.wiki.gg/wiki/Trinket).

재질의 확인 예시는 Silk의 Speed +2, Steel의 방어 +1이다. **천·가죽·금속별 허용 조합을 유지**하며, 모든 방어구에 모든 재질을 붙일 수 있다고 해석하지 않는다. 재질은 위력 단계나 희귀도와 별도 축이다. [재질표](https://fortheking.wiki.gg/wiki/Attire_%28FTK2%29)

설치본 `ItemMaterials.json`에서도 `ATT_CLOTH.Materials.ATT_SILK`의 SPD +2, `ATT_METAL.Materials.ATT_STEEL`의 DEF +1을 확인했다. 각각 `Attributes.Explicit.Stats`의 보정이다. 동일 Steel 명칭의 `WEP_METAL.Materials.WEP_STEEL`은 ATK +1이므로 무기·방어구 재질 ID를 합치지 않는다. 재질 후보 연결·추첨·최종 합산은 별도 확인한다.

#### 7-5-1 설치본 장비 식별과 방패 효과

다음 `Value`는 파일의 정의값이다. A04의 기존 +5% 표기를 `CRT=10`으로 정정하고, A02의 RES 및 R03/R04의 HP 누락을 보충했다. CRT/EVD의 최종 확률 단위·상한은 실행 계산 확인 대상이다.

| 카드 | 원본 · 고유 ID | 슬롯 · Value | 추가 정적 근거 |
|---|---|---|---|
| A01 | `Things/Attires.json` · `CHESTARMOR_LOADOUT_CLOTH_00` | ARMOR · 3 | DEF 1·RES 1 |
| A02 | `Things/Attires.json` · `HELMET_LOADOUT_HEAVY_00` | HELMET · 3 | DEF 1·RES 2, Passives의 `STATUS_IMMUNITY_STUN` |
| A03 | `Things/Attires.json` · `BOOTS_LOADOUT_LIGHT_00` | BOOTS · 3 | EVD 5, Passives의 `STATUS_IMMUNITY_ENTANGLE` |
| A04 | `Things/Attires.json` · `GLOVES_LOADOUT_CRIT_00` | GLOVES · 3 | RES 1·CRT 10 |
| S01 | `Things/Weapons.json` · `SHIELD_MILITIA_TINY_00` (Wooden Buckler) | OFF_HAND · 10 | EVD 7, `SKILL_GUARD`. Taunt는 SPD 2회, Evade Up은 SPD 1회 |
| S02 | `Things/Weapons.json` · `SHIELD_MILITIA_HEAVY_00` (Wooden Tower Shield) | OFF_HAND · 22 | SPD -5·DEF 1·RES 1·PDEF 1, `STATUS_IMMUNITY_MOVE`·`SKILL_GUARD`. Taunt는 VIT 2회, Armor Up Neighbors는 VIT 1회 |
| R03 | `Things/Attires.json` · `TRINKET_BUFF_ARMOR` | TRINKET · 25 | HP 3·DEF 2 |
| R04 | `Things/Attires.json` · `TRINKET_BUFF_RESIST` | TRINKET · 25 | HP 3·RES 2 |

`Abilities.json.SHIELD_TAUNT_ATTACK`은 `IsMajorAction=false`, Target SELF, `STATUS_TAUNT_00` 연결이다. `SHIELD_EVADE_ATTACK`은 주요 행동이며 `STATUS_EVADEUP_02`에 EVD +35·Duration 2가 있다. `SHIELD_ARMOR_SPLASH_ATTACK`은 주요 행동, Target SELF·TargetArea SPLASH, `STATUS_ARMORUP_GROUP_00`에 DEF +4·Duration 2가 있다. 두 버프의 수명 감소·재부여와 SPLASH의 정확한 이웃 칸, Guard·PDEF 계산은 미확인이다.

### 7-6 약초·탐험 도구·성장 카탈로그

같은 이름이라도 작품별 효과가 다르면 별도 ID를 사용한다. 모든 소비형 항목은 수량이 있는 자원으로 설계한다. 사용 상황과 소비 실패 규칙은 상세 카드에서 확정할 대상이다.

| ID · 기능명 | 원작 기준 | 재현할 효과 | 클론 설계 목적 | 변환할 부분 |
|---|---|---|---|---|
| C01 단계형 회복초 | FTK1 Godsbeard | 파이프 0~3에 따라 HP 15/30/45/60 | 회복초 비축과 효율 성장 연결 | 현재 회복약 40과 교체 여부·파이프 소유자 |
| C02 비율형 회복초 | FTK2 Godsbeard | 장비 등 추가분을 제외한 사용자 HP 기준 50% 회복 | 체력 성장에 대응하는 회복 | 기준 HP의 정의·반올림·아군 사용 |
| C05 단계형 정화초 | FTK1 Panax | 독·출혈 등 해제, 독 제거량은 파이프 단계에 영향. 저주·Death Mark 제외 | 범용 회복과 상태 대응의 분리 | 해제 대상·스택 처리·동시 해제 순서 |
| C06 전투 정화초 | FTK2 Panax | 설치 정의의 자기 대상 CURE 상태 해제. 정확한 제거 집합은 미확인 | 공격 약화 등 전투 상태 대응 | ConsumableType ANY의 실제 허용 상황·버프 보존·지원 대상 |
| C07 해주초 | FTK2 Hag's Bane | 사용자 저주 제거 | 일반 정화와 장기 불이익 대응 분리 | 저주를 Run에 유지할지 결정 |
| C08 이동·속도초 | FTK1 Dancing Nettle | 탐험 이동 증가, 전투에서 Speed 증가 | 상황별 사용 가치 변경 | 월드 이동과 Grid 이동을 구분해 대체 |
| C09 은신·회피초 | FTK1 Hermit Grass | 탐험 은신·전투 회피 보완 | 탐험 위험과 전투 생존 대응 | 회피 지속·Node 위험 회피 대체 |
| C10 대가형 완전 회복초 | FTK1 Purple Herb | HP 완전 회복과 최대 HP 감소 | 현재 생존과 장기 체력의 교환 | 감소량·적용 순서·영속 범위 |
| C11 죽음 유예초 | FTK1 Dead Lotus | 짧은 죽음 방지와 저주 대가 | 위기 대응과 후속 정화의 연계 | 유지 시간·최소 HP·보스/경쟁 제한 |
| C12 완전 회복·정화초 | FTK1 Firesilk | 최대 HP까지 회복하고 질환·저주 해제 | 복합 위기에서 소모할 고가치 자원 | 회복·정화 적용 순서·희소 보상 여부 |
| U01 야영 도구 | FTK2 Tinder Pouch | 원작 캠프의 파티 HP 회복 부분만 채택 후보 | 전투 전 자원 정비 | 회복 Encounter나 준비 화면 서비스로 변환 |
| U02 순간이동 주문서 | FTK2 Teleport Scroll | Intelligence에 따른 거리 안의 빈 육지 Hex로 이동 | 탐험 경로 단축 | Node 건너뛰기·전투 이동 중 채택 여부 |
| U03 정찰 주문서 | FTK2 Vision Scroll | 월드의 구름 제거·숨은 위치 공개 | 사전 정보 확보 | 다음 후보 노드·상점·상대 정보 공개 |
| U04 잠금 해제 도구 | FTK2 Lockpicks | 잠긴 대상 개방에 사용하는 도구 | 도구 소비와 잠금 장애의 연결 | 잠금 Encounter·실패 대체 규칙 |
| P01 약초 효율 성장 | FTK1 Old/Elm/Bone/Dragon Pipe | 4단계 파이프를 통한 약초 효율 상승 | 즉시 소비보다 장기 효율에 투자 | 소유 단위·업그레이드 비용·유지 범위 |
| P02 영구 공격 성장 사탕 | FTK2 Black Candy | 소비자 물리 피해 영구 +1 | 장착 슬롯 밖의 성장 | Run 내 지속과 계정 영구 성장 구분·누적 한도 |

C01: [FTK1 Godsbeard](https://fortheking.wiki.gg/wiki/Godsbeard). C02: [FTK2 Godsbeard](https://fortheking.wiki.gg/wiki/Godsbeard_%28FTK2%29). P01: [파이프](https://fortheking.wiki.gg/wiki/Pipes). C05: [FTK1 Panax](https://fortheking.wiki.gg/wiki/Panax). C06: [FTK2 Panax](https://fortheking.wiki.gg/wiki/Panax_%28FTK2%29). C07: [Hag's Bane](https://fortheking.wiki.gg/wiki/Hag%27s_Bane_%28FTK2%29). C08~C12: [FTK1 약초](https://fortheking.wiki.gg/wiki/Herb). U01~U04: [FTK2 Loadout](https://fortheking.wiki.gg/wiki/Lore_Store_%28FTK2%29/Loadout). P02: [Black Candy](https://fortheking.wiki.gg/wiki/Black_Candy_%28FTK2%29).

FTK2 파이프는 아래 설치본의 약초 사용 트리거 장비로 보충한다. P01은 FTK1 단계형 모델로 유지하며 두 작품의 구조를 합치지 않는다. 파이프 예시는 42개 카탈로그에 새 기획 ID를 추가한 것이 아니다. [파이프 문서의 작품별 구분](https://fortheking.wiki.gg/wiki/Pipes)

#### 7-6-1 설치본 소비 효과와 사용 구분

아이템은 `Things/Items.json`, 연결 효과는 `Abilities.json`을 기준으로 한다. C02·C06·C07·P02의 연결 행동은 SELF, `IsMajorAction=false`, `RequiresSkillRoll=false`다. HP 효과의 음수 부호는 원본 그대로 보존한다. 분모·반올림·과잉 회복·만피 사용과 실패 소비는 실행 검증 대상이다.

| 카드 · 아이템 ID | Value · ConsumableType | 연결 Ability · 정적 효과 |
|---|---|---|
| C02 · `HERB_GODSBEARD_01` | 12 · ANY | `BASIC_HEAL_01`: CHANGE_STAT의 HP, `FlatPercent=-50` |
| C06 · `HERB_PANAX_01` | 10 · ANY | `BASIC_CURE_01`: REMOVE_STATUS의 `CURE` selector |
| C07 · `HERB_HAG_BANE_01` | 16 · ANY | `BASIC_CURSE_REMOVE_01`: REMOVE_STATUS의 `CURSE` selector |
| U01 · `TOOL_TINDER_01` | 35 · OVERWORLD | `CREATE_SAFE_CAMP` 연결 |
| U02 · `SCROLL_TELEPORT_01` | 12 · OVERWORLD | `PICK_HEX_TELEPORT` 연결 |
| U03 · `SCROLL_VISION_01` | 16 · OVERWORLD | `PICK_HEX_VISION` 연결 |
| U04 · `TOOL_LOCKPICKS_01` | 10 · OVERWORLD | 명시 Ability 연결 미기재. 잠금 상호작용의 실제 소비 경로는 미확인 |
| P02 · `CANDY_BLACK_01` | 90 · ANY | `BASIC_PHY_ADD_01`: CHANGE_STAT의 PHY, `FlatValue=1`. 저장·영속 범위는 별도 검증 |

`ANY`를 모든 UI·상태에서 무조건 사용 가능하다는 뜻으로 확대하지 않는다. `CURE`와 `CURSE`를 구분하지만 실제 해제 태그 집합과 순서는 아직 확인하지 않았다. 탐험용 도구를 전투 내 이동·정찰 행동으로 변환하면 ProjectA 변경 사항으로 기록한다.

#### 7-6-2 설치본 회복·방어 파이프 4종

근거는 `Things/Attires.json`의 `Equippable.Slots=[PIPE]`, `AbilityTriggers`와 연결 `Abilities.json`·`StatusEffects.json`이다. 다음 네 정의의 `ProcChance=100`은 트리거 설정값이며 모든 상황의 실행 성공을 보장했다는 검증 결과가 아니다.

| 명칭 · 아이템 ID | Tier 범위 · Value | TriggerOn → TriggerAbility | 연결 효과 |
|---|---|---|---|
| Healing Pipe · `PIPE_ELM_HEALING_00` | 0~1 · 25 | ANY_HERB → `ABILITY_TRIGGER_HEAL_01` | HP `FlatValue=-5`, 설명상 추가 회복 |
| Fine Healing Pipe · `PIPE_ELM_HEALING_02` | 2~3 · 126 | ANY_HERB → `ABILITY_TRIGGER_HEAL_02` | HP `FlatValue=-10`, 설명상 추가 회복 |
| Plated Pipe · `PIPE_BONE_ARMOR_00` | 1~2 · 78 | COMBAT_HERB → `ABILITY_TRIGGER_ARMOR_UP_01` | `STATUS_ARMORUP_00`: DEF +5, Duration 2, TickCombat true |
| Fine Plated Pipe · `PIPE_BONE_ARMOR_02` | 3 · 398 | COMBAT_HERB → `ABILITY_TRIGGER_ARMOR_UP_02` | `STATUS_ARMORUP_01`: DEF +10, Duration 2, TickCombat true |

FTK2형의 설계상 선택은 **약초 자체 효과 + 장착 파이프의 회복·방어 부가 효과**다. FTK1형의 단계별 기본 회복량 증가와 별도 모델로 비교한다. 일반/Fine은 별도 ID이므로 자동 업그레이드·제작 관계를 가정하지 않는다. Party Heal 시 소비자·수혜자의 어느 파이프가 발동하는지, 연속 사용 시 방어 중첩·갱신, 트리거의 재발동 방지는 미확인이다.

### 7-7 상세 카드와 판정 예시

#### 7-7-1 W02 관통 입문 활

| 항목 | 기획 명세 |
|---|---|
| 기준 | FTK2 Simple Bow, Common, Hunter 시작 무기 |
| 장착 | 활·양손. 방패와 동시 장착 불가 모델 |
| 능력치·피해 | 설치 정의 AWR·ATK 6·CRT 5·DAM_FLYING 15. 기존 공개 카드의 표시 피해 2~6과 원본 ATK를 구분하며 성공 슬롯별 피해표는 확인 필요 |
| 기본 행동 | Shot: 적 단일 대상 물리 공격 |
| 선택 행동 | Pierce: 적 단일 대상, 정확도 -5%, 완전 성공 시 방어 관통 |
| 비용 | 설치본 Shot·Pierce 모두 주요 행동, AWR 판정 2회. ProjectA AP 비용 미확정 |
| 부가 효과 | Support Range. 월드 전투 합류 지원과 Grid 사거리를 같은 수치로 취급하지 않음 |
| 획득 | `Characters.json.HUNTER.Things`에 `BOW_HUNTER_BASIC_00=1`. 무기 정의 Value 3·Tier -1은 판매 가능성의 근거가 아니며 일반 상점 편입은 제안 |
| 비교 의도 | 저방어 상대에게 안정적인 Shot, 고방어 상대에게 완전 성공 위험을 감수한 Pierce |
| ProjectA 변경란 | 충돌 판정을 유지하며 AP 비용, 관통 발동 조건, 물리 방어, 후열 보호, 공격 범위, 원작 시작 전용 유지 여부 결정 |

원작 대조용 수용 조건: 동일 적과 피해 기준에서 Shot은 방어를 적용한다. Pierce의 완전 성공에는 관통이 적용되지만, 부분 성공에는 완전 성공 보너스를 적용하지 않는다. 관통만으로 후열 타겟 검사를 생략하지 않는다. ProjectA는 슬롯 판정을 사용하지 않으므로 이 조건을 그대로 구현하지 않으며 관통의 새 발동 조건은 미정이다. 이는 원작 대조 계획이며 실행 결과가 아니다. [개별 카드](https://fortheking.wiki.gg/wiki/Simple_Bow_%28FTK2%29), [활 분류](https://fortheking.wiki.gg/wiki/Bow_%28FTK2%29)

설치본 Shot의 `MinValue/MaxValue=0.5/1`, Pierce는 `0/1`이다. `Abilities.json`의 HP 액션에서 Shot은 `IsBlockable=true`, Pierce는 false다. 이 플래그만으로 부분 성공 시 관통 처리까지 확정하지 않으며 위 완전 성공 설명의 런타임 gate와 최종 피해표는 직접 대조 대상으로 유지한다.

#### 7-7-2 C01·P01 회복초와 파이프

아래는 FTK1 원작 확인값이다. 가격은 파이프 기본 업그레이드 가격이며 모든 상황의 최종 판매가를 뜻하지 않는다. [파이프 단계표](https://fortheking.wiki.gg/wiki/Pipes)

| 파이프 | 단계 | 회복초 HP | 기본 업그레이드 가격 |
|---|---|---|---|
| Old Pipe | 0 | 15 | 시작 단계 |
| Elm Pipe | 1 | 30 | 20 Gold |
| Bone Pipe | 2 | 45 | 80 Gold |
| Dragon Pipe | 3 | 60 | 300 Gold |

클론 설계: 회복초 개수와 약초 효율 단계를 별도 관리한다. 파이프 업그레이드는 회복초 재고를 늘리는 기능으로 취급하지 않는다. 카드에는 현재 단계 회복량과 다음 단계 증가량을 함께 표시한다. 회복초를 많이 보유할수록 효율 투자의 가치가 높아지므로 구매 순간의 단순 HP 회복과 비교할 수 있어야 한다.

변환 예시 가정: 향후 ProjectA에서 초과 회복을 버리는 규칙을 채택하면, 최대 HP 100·현재 HP 70에서 45 회복초를 사용한 실효 회복량은 30이다. 이는 ProjectA 제안 계산이며 원작의 모든 회복 예외를 검증한 결과가 아니다. Party Heal의 소모 주체·효율 기준은 별도 확인 후 지정한다.

#### 7-7-3 C02 비율형 회복초

FTK2 문서는 장비·성소 등 추가 HP 이전 기준의 50% 회복으로 설명한다. Party Heal은 Splash 형태이며 주변 대상의 효율이 낮다. 이 설명을 ProjectA의 현재 `MaxHP` 50%와 바로 동일시하지 않는다. [FTK2 회복초](https://fortheking.wiki.gg/wiki/Godsbeard_%28FTK2%29)

| 항목 | 기획 명세 |
|---|---|
| 소비 자원 | 회복초 1개. 아군 지원 시 소비자와 수혜자 구분 필요 |
| 계산 요소 | 기준 HP, 추가 HP, 파이프 보정, 대상별 효율, 반올림 규칙 |
| 사용 상황 | 자기 턴의 월드·전투 및 던전 방/층 사이. Party Heal·Field Medic는 별도 능력에 의존 |
| 설치본 보충 | `BASIC_HEAL_01`의 HP `FlatPercent=-50`; 회복·방어 파이프 4종은 7-6-2절. `UI_ENCYCLOPEDIA_SKILL_PARTYHEAL` 계열 설명은 전투 대상 효율 100%·주변 50% |
| 확인 필요 | 정확한 기준 HP 산식, 경계 HP 반올림, 과잉 회복·만피 사용, Party Heal 파이프 적용 주체와 Field Medic 처리 |
| 클론 표시 | 예상 실효 회복·대상·소모 수량·보조 행동 비용 |
| ProjectA 변경란 | 기준 HP 별도 보관 여부, 현재 회복약 40과 병존/대체, 지원 스킬의 범위 |

비교 예시: 기준 HP 80에 장비 HP 40이 추가된 캐릭터라면 부가 보정 전 계산은 40 회복이다. 현재 최대 HP 120의 절반인 60과 차이가 발생한다. 기준 HP와 장비 보너스를 따로 기록해야 하는 이유를 보여 주는 계산 예시다.

Party Heal의 100%/50%는 **회복 효과의 효율**이며 대상 최대 HP의 100%/50%를 회복한다는 뜻이 아니다. 같은 기본 회복량 40을 가정한 설명 예시에서는 대상 40·주변 20이지만, 실제로 누구의 기준 HP를 사용하는지와 파이프·반올림의 순서는 미확인이다.

#### 7-7-4 W11 파괴 위험과 예비 무기

FTK1의 `Breakable`은 한 공격에서 모든 판정을 실패하면 파괴되는 속성이다. 파괴 후 비무장으로 전환된다는 원작 구조를 보존한다. [무기 속성](https://fortheking.wiki.gg/wiki/Bludgeon)

클론 설계의 상태 흐름은 `장착 → 행동 판정 → 파괴 조건 검사 → 장착 해제·부여 스킬 회수 → 비무장 행동 제공`이다. 파괴 예외가 있는 작품·직업은 별도 규칙으로 추가한다. 기존 행동의 피해 처리와 파괴의 선후, 장비 교체 비용, 저장 복구 시 재파괴 방지는 구현 전 명세 대상이다. 위험 보상으로 높은 피해를 배정하는 것은 클론 밸런스 제안이며 모든 원작 파괴 무기가 강하다는 뜻은 아니다.

### 7-8 성장·상점·보상 설계

**아래 상품 구성·등장 단계·배분은 클론 제안이며 원작 상점 확률이나 ProjectA 확정 정책이 아니다.** 현재 고정 상점 3개의 거래 기능을 변경하지 않는다.

| 성장 구간 가칭 | 제공할 선택 | 예시 카드 | 검토할 영향 |
|---|---|---|---|
| 입문 | 일반 공격·관통·기본 방어·회복 중 결핍 보완 | W02·A01·C01 또는 C02 | 처음부터 모든 대응을 지급하면 구매 판단이 줄어듦 |
| 역할 형성 | 방어 파쇄·제어·면역·정화 | W06·W07·A02·C06 | 강한 제어의 반복 사용과 해제 수단의 균형 필요 |
| 조합 완성 | 범위 공격·인접 보호·재질·효율 투자 | W04·S02·A08·P01 | 배치·회복초 재고·장비 상호작용 평가 |
| 희소 보상 | 완전 회복·위험 무기·영구 수치 성장 | C12·W11·P02 | 특정 보상이 승패를 독점하는지 평가 |

상품 풀 후보는 무기·방어구·보급품으로 나누되, 실제 상점1·2·3에 역할을 부여할지는 후속 결정한다. 보급품의 최소 재고 보장, 직업 능력치에 맞는 무기 보장, 동일 상품 중복, 재방문 갱신, 새로고침 비용, 매입가는 아직 미정이다. W02처럼 원작 입수 경로가 제한된 항목을 일반 상품으로 넣으면 원작 대비 변경 내역을 남긴다.

가격은 원작의 숫자만 복사하지 않고 해당 Run의 수입·전투 수·구매 기회와 함께 조정한다. FTK1 파이프처럼 확인한 기본 가격은 참조 열에 유지한다. 원작 희귀도, 위력 단계, 재질, 상점 가격, Lore 해금을 하나의 등급 필드로 합치지 않는다.

협동에서는 재화 소유자·아이템 보관자·장착 캐릭터·요청자를 구분한다. Host의 노드 선택 권한을 타인 아이템 임의 장착·판매 권한으로 확대하지 않는다. 공동 재화·배분·거래 권한은 [5절](#5-협동-확정-정책)과 [미결정 사항](TODO.md#3-2-아이템과-직업)에 따라 결정한다.

#### 7-8-1 설치본 상점·보상 참조

다음은 [설치본](#7-11-ftk2-설치본-근거와-해석-범위)의 설정값이다. 수량 범위 문자열은 그대로 기록하며 균등 추첨·최종 재고·출현 확률을 뜻한다고 가정하지 않는다. `Inherit`가 있는 변형은 상위 정의와 결합 방법을 확인해야 한다.

| 근거 파일 · 내부 ID | 정적 확인값 | ProjectA 보충 제안 |
|---|---|---|
| `Markets.json` · `TOWN_TYPICAL` | `RequiredItems`: Godsbeard 3개. `RequiredCategories`: HERB `2-3`, PIPE `1`, ARMOR `4-7`, WEAPON `3-6`, SHIELD `0-2` 등 | 필수 보급과 무작위 장비 슬롯을 분리한 재고 구조. 필수품과 범주 추첨의 중복 허용은 별도 결정 |
| `Markets.json` · `MERCHANT_PIPESMITH` | PIPE `4-6`, HERB `5-7` | 회복·정화·약초 보정 장비를 함께 비교하는 보급 전문 상점 후보 |
| `Markets.json` · `MERCHANT_MAGIC` | `Rarity=UNCOMMON`, HAND_EQUIP+MAGICAL `2-4`, ARMOR+MAGICAL `4-6`, SCROLL `3-5` 등 | 공격 유형·역할 태그로 상품 풀을 구분. UNCOMMON의 정확한 필터 동작은 미확인 |
| `Markets.json` · `MERCHANT_ARMOR` | `Rarity=UNCOMMON`, ARMOR `9-14`, SHIELD `1-2` | 방어·면역·보호 역할을 보완하는 전문 상점 후보 |
| `SmartLoots.json` · `HUNTER`, `SCHOLAR` | MAIN_HAND Query는 각각 `[BOW]`, `[BOOK, STAFF]`; 별도 `FavoredItemClasses` 존재 | 파티의 사용 가능 무기 후보와 일반 보상 풀을 별도 관리. 실제 보장 횟수·확률은 미확인 |
| `GameDifficulties.json` · `APPRENTICE_STANDARD`, `JOURNEYMAN_STANDARD`, `MASTER_STANDARD` | `MARKET_INFLATION=80/90/100`, `SMART_LOOT_SHUFFLE=2/4/10` | 난이도별 경제·보상 설정을 분리. 필드값을 할인율이나 재추첨 횟수로 직접 해석하지 않음 |

첫 거래 구현의 제안은 **필수 보급품 슬롯 + 파티 역할 보완 장비 슬롯 + 자유 추첨 슬롯**이다. 상품 후보가 부족할 때는 다른 합법 후보로 대체하고, 그래도 없으면 빈 슬롯을 표시하는 규칙을 후보로 둔다. 상점 진입 시 확정한 상품 ID·수량·가격·변형을 저장하여 로드로 재추첨하지 않는 계약이 필요하다. 현 상점1·2·3의 전문화, 슬롯 개수, 가격, 새로고침, 구매 권한은 [후속 결정](TODO.md#3-2-아이템과-직업) 대상이며 이번에 확정하거나 구현하지 않는다.

### 7-9 데이터 변환을 위한 공통 명세

다음은 엔진과 무관한 기획 필드 제안이다. 현재 C++ 구조체나 DataAsset 필드명이 아니다. 원작 확인 값과 ProjectA 채택 값을 모두 보존한 뒤 구현 단계에서 실제 형식을 결정한다.

| 묶음 | 기록할 필드 | 의미 |
|---|---|---|
| 식별·근거 | DesignId, SourceGame, SourceName, SourceUrl, SourceBuildId, SourceFile, SourceEntryId, SourceFieldPath, SourceFileHash, CheckedDate, VerificationState | W02 등 기획 식별자와 웹/설치본 출처·확인 수준 |
| 표시 | WorkingName, DisplayDescription, IconReference | 가칭·효과 설명·향후 제작 이미지 참조 |
| 분류·장착 | Category, Slot, OccupiedSlots, EquipConditions | 양손 점유·방패 충돌·개별 장착 조건 |
| 성장·변형 | FamilyId, Tier, Rarity, MaterialId, VariantId | 계열·위력 단계·희귀도·재질의 분리 |
| 스탯 | Modifiers의 Stat/Operation/Value/Condition | 고정 증가·비율 증가·조건부 보정 |
| 스킬 | GrantedSkillIds, PassiveEffectIds | 장비 부여 행동과 패시브를 구분 |
| 판정 | CheckStat, RollCount, AccuracyModifier, SuccessRule | 원작 능력치·판정 수·완전 성공의 참조값. ProjectA 명중 확률·성공 슬롯 실행 필드로 사용하지 않음. 자원 소비형 판정 보정 제외 |
| 대상·효과 | TargetRule, AreaPattern, Falloff, EffectEntries | 선택 규칙·영향 칸·감쇠·순서 있는 효과 |
| 사용 비용 | AllowedContexts, ActionCost, ConsumeCount, FailurePolicy | 사용 상황·행동 자원·수량·실패 소비 |
| 상태 | DurationValue, DurationUnit, StackRule, DispelTags, ImmunityTags | 턴/라운드 구분·중첩·해제·면역 |
| 거래·입수 | AcquisitionRule, ShopPoolId, BaseBuyPrice, SellRule, StockRule | 원작 입수와 제안 상품 풀을 구분 |
| 조정 기록 | SourceValue, DesignValue, ChangeReason, DecisionState | 원작 수치·조정 수치·이유·채택 상태 |

`null`은 확인 필요, `0`은 효과량/비용 0 확인, 빈 목록은 효과 없음 확인으로 구분한다. 판정값·희귀도·가격이 없는 카드를 임포터가 자동으로 0값 데이터로 만들지 않도록 한다. 버전별로 같은 이름을 가진 C01/C02 등의 카드는 ID를 합치지 않는다.

위 `null` 규칙은 ProjectA 기획 필드의 계약이다. 원본 JSON의 `Interactable.Abilities`에 있는 `스킬ID: null`을 스킬 없음으로 변환하지 않는다. 스킬 ID 참조와 개별 오버라이드 부재를 구분하고 `Abilities.json`의 정의·상속 관계를 함께 기록한다. 실제 로더의 병합·기본값 처리를 검증하기 전에는 완성된 런타임 데이터로 자동 변환하지 않는다.

| 데이터 층 | 저장할 내용 | 분리 이유 |
|---|---|---|
| 정적 아이템 정의 | 이름·분류·스탯·스킬·효과·입수 규칙·정의 버전 | 모든 인스턴스가 참조하는 기준 |
| Run 아이템 인스턴스 | InstanceId·DefinitionId·소유/보관 식별자·장착 캐릭터·수량·확정 변형·파괴 상태 | 같은 종류 여러 개와 서로 다른 재질·소비 상태 구분 |
| 캐릭터 성장 상태 | 파이프 단계·사탕 누적 보정·적용 이력 | 장착 해제 시 사라질 효과와 지속 성장 분리 |
| 명령 | 요청 ID·행위자·대상·인스턴스·수량·상태 버전 | 구매·장착·판매·사용의 권한/재고/중복 검증 |
| Snapshot | 정의 버전·슬롯별 확정 장비·확정 보정·채택한 소모품/성장 범위 | 상대 재생성 시 재추첨과 소비 중복 방지 |

직렬화 가능한 값 데이터와 안정 ID를 우선하고, 실행 중 Actor·Widget 참조는 영속 데이터의 기준으로 사용하지 않는다. 난수로 생성한 재질·등급은 결과를 저장하며 장착·로드 때 다시 뽑지 않는다. 경쟁 Snapshot에 소모품을 포함할지, AI의 사용 수량과 소모가 어디에 반영되는지는 미정이다. [멀티플레이 계약](MULTIPLAYER.md)

다음 표는 아이템 클론 작성 당시의 연결점이다. 최신 기본 전투·스킬 이행은 8절을 따르며 기존 회복약/순차 GAS 실행 지원으로 해석하지 않는다.

| 현재 연결점 | 확인한 상태 | 후속 변환 작업 |
|---|---|---|
| [SkillDefinitionDataAsset](../Source/ProjectA/DataAsset/SkillDefinitionDataAsset.h) | SkillId·AP 비용·대상·범위·Ability 참조 | 부여 스킬은 기존 ID 체계 사용 검토. 판정·부가 효과 계약은 추가 설계 |
| [SkillTypes](../Source/ProjectA/Types/SkillTypes.h) | Single·Row·Column 등 범위 분류 | FTK2 패턴의 실제 피격 칸·감쇠를 대조. 이름만 같은 패턴으로 간주하지 않음 |
| [AS_Unit](../Source/ProjectA/GAS/Attribute/AS_Unit.h) | GAS Attribute는 HP·MaxHP | 능력치·방어·저항·치명·회피를 별도 설계 |
| [UnitBase](../Source/ProjectA/Unit/UnitBase.h) | HealingItemAmount 40·HealingItemCount 1 | 일반 아이템 재고·정의 기반 소비로 확장할지 결정 |
| [PartySnapshotTypes](../Source/ProjectA/Game/Snapshot/PartySnapshotTypes.h) | EquipmentIds 저장 필드 존재 | 현재 전투의 장비 지원과 구분. 슬롯·변형·장비 스킬 검증 확장 필요 |

DataAsset·USTRUCT·USaveGame·GAS의 기존 확장 지점을 먼저 검토한다. 기획 필드 전체를 한 구조체에 그대로 넣는 것을 전제로 하지 않는다. 생성·복제할 엔진 에셋은 `/Game/User_JeHoon/` 아래에 둔다.

### 7-10 조합 예시와 도입 순서

아래 조합은 클론 설계 예시다. 실제 원작에서 모든 장착·효과 중첩을 검증한 최적 빌드가 아니며, 캐릭터 직업을 고정하지 않는다.

| 조합 | 구성 | 확인하려는 선택 |
|---|---|---|
| 단일 관통 | W02·A04 | 일반 공격과 관통 공격의 대상별 효율 차이 |
| 파쇄 지원 | W07·A01·C06 | 자신이 즉시 피해를 줄지 후속 아군의 공격을 돕는지 |
| 인접 수호 | S02·A02·R03 | 아군을 가까이 보호할지 범위 공격을 피하도록 분산할지 |
| 약초 투자 | C01·P01 | 소모품 즉시 구매와 회복 효율 장기 투자 중 선택 |

추천 검토 순서는 **무기와 부여 스킬 → 기본 방어구·방패 → 회복·정화 → 장착·재고·거래 → 파이프·재질·희소 효과**다. 첫 후보 묶음은 W02·W03·W07·A01·A02·S01·R03·R04와 회복초 C01/C02 중 하나다. 이는 제작 우선순위 제안이며 현재 Run 구조·사용자 작동 테스트 우선순위를 변경하지 않는다.

구현 전에 선택할 정책과 영향은 [TODO의 아이템 후속 결정](TODO.md#3-2-아이템과-직업)에 모은다. 기획 검토·추후 작동 확인 절차는 [남은 확인](TODO.md#3-2-아이템과-직업)을 따른다.

### 7-11 FTK2 설치본 근거와 해석 범위

#### 7-11-1 조사 기준

- 확인일: 2026-09-15. Steam App ID `1676840`, 설치 manifest의 `buildid=24247341`. 최신 공개 패치와 동일하다는 확인은 아니다.
- 설치 경로: `C:\Program Files (x86)\Steam\steamapps\common\For The King II`.
- 아래 원본 경로 기준: 설치 경로의 `For The King II_Data/StreamingAssets/Assets/Configs/JSON~`.
- 방법: 평문 JSON 파싱, 아이템 → Ability → Status/설명문 참조 대조, SHA-256 기록. 게임 실행·실행 코드 분석·런타임 설정 로드 검증은 미실행.
- 범위: 무기·장비·소비 효과·파이프·네 직업 및 상점·재질·상태 참조를 조사했다. 채택 후보 카탈로그는 42개이며 남은 ID는 재번호화하지 않는다. 전체 아이템 도감·전체 밸런스 분석은 아니다.

| 원본 파일 | SHA-256 |
|---|---|
| `Things/Weapons.json` | `55c2ac3d5d36071b6607c55373ddefa2cc38ed1cb47014973533eef4c328aa4d` |
| `Things/Attires.json` | `eb28242ba7e76956e1fff9d93c519c0370255ead0608a94e4fc7800326f3fd67` |
| `Things/Items.json` | `dd4e9436fee20d99e242ca9c83fb5267054c8eeec2620d2e2f8866c1942884a9` |
| `Abilities.json` | `582e2aa75042191b3c160028d2449815a98430754623c8be862fb96a1ba2e21b` |
| `StatusEffects.json` | `d64e3fcca919d9f87c00c8373764a7329b64628587a67205099cd72f5c2bfea6` |
| `ItemMaterials.json` | `b21c4702dd5425fefaa668e9e336569723124ea4a4094f577cff4467dafd4527` |
| `Markets.json` | `45c373e143beb154ce4099399abf697f4d294086981a672773a67098d10a8952` |
| `SmartLoots.json` | `5eb0585790a75ea7a4d2a6b22579dc38ca474bf0c7405f1e481ec6d28e708aaf` |
| `GameDifficulties.json` | `63f009b6dde1eed95bdf034da0862b621ffec2199190a29000b9179e06fd382b` |
| `Characters.json` | `0d111f67b0dcb47cb2e7f3c4e45e17f5b13c21b5fe4ad4e017ec74f51c44faf3` |
| `SkillConfigs.json` | `be91d2cbf2039b5b70fe125c2eb9aaa97c90b64e8e151aeff7ba0a831bb390eb` |
| `Langs/en.json` | `447c8616a85a9c71cace9f842b7cb5007d732cbb6d13ba2ebca0b5ef74779474` |
| `Langs/ko.json` | `a8134afbcaef739463c5ef634260bf55be9ce6644e65b909d8ca7af054a20bcd` |

문서의 고유 ID와 필드 경로가 재확인 기준이다. 로컬 세부 추출은 `Saved/ReferenceAudit/FTK2-20260915/`의 `source-manifest.json`, `catalog-evidence.json`, `status-evidence.json`, `classes-evidence.json`, `economy-evidence.json`에 있다. Saved는 Git 제외 경로이며 다른 PC에서는 위 원본 해시·ID로 다시 확인한다.

재확인 예시:

```powershell
$ftkSourceRoot = 'C:\Program Files (x86)\Steam\steamapps\common\For The King II\For The King II_Data\StreamingAssets\Assets\Configs\JSON~'
$ftkWeaponsPath = Join-Path $ftkSourceRoot 'Things/Weapons.json'
$ftkWeapons = Get-Content -Raw -LiteralPath $ftkWeaponsPath | ConvertFrom-Json -AsHashtable
$ftkWeapons['BOW_HUNTER_BASIC_00'].Interactable.Abilities | ConvertTo-Json -Depth 10
Get-FileHash -Algorithm SHA256 -LiteralPath $ftkWeaponsPath
```

#### 7-11-2 해석과 후속 결정

설치본 정의·현지화 설명·실제 게임 동작·ProjectA 제안을 구분한다. 평문 파일의 존재는 런타임에서 그 값이 실제 선택되었음을 증명하지 않는다. 동명 아이템의 Tier·Class·고유 ID, `Inherits`/`Inherit`, 난이도·재질·상위 정의의 적용 순서를 함께 확인해야 한다.

이번에 보완한 것은 판정 수·명시 스킬 연결·장비 보정·소비 효과 selector·파이프 트리거·상점 템플릿·직업 역할 근거다. 최종 판매가·피해 산식·중첩·지속 경계·재장전·실제 정화 집합은 직접 카드/도감 대조 또는 추가 실행 코드 분석이 필요하다. FTK1 수치의 공백은 FTK2 값으로 채우지 않는다.

ProjectA에는 직업 역할·필수 보급과 장비 슬롯을 분리한 상점·약초 부가 효과 모델을 **채택 미확정 제안**으로 보충했다. 공격 적중은 [8-5절의 충돌 판정](#8-5-발동과-피격)으로 확정했다. 기존 시간차 전투와 상태이상 규칙을 유지하며 원작 완전 성공 부가 효과의 변환·회복 모델·최종 장비 슬롯·개인/공동 재화는 [TODO 3-2절](TODO.md#3-2-아이템과-직업), 경쟁 Snapshot 범위는 [TODO 4-2절](TODO.md#4-2-서비스-구현과-검증)의 사용자 결정 대상으로 둔다. 확인 절차와 미실행 항목은 [남은 확인](TODO.md#3-2-아이템과-직업)에 기록한다.

## 8 라운드 계획과 시간차 자동 전투

이 전투 구조를 유지하는 해로운 상태이상 8종의 규칙·미정 사항·구현 연결 검토는 [STATUS_EFFECTS](STATUS_EFFECTS.md)에 둔다. 상태이상 실행은 아직 기본 라운드 경로에 구현하지 않았다.

### 8-1 목적과 적용 상태

2026-09-14 사용자 결정에 따른 프로토타입의 확정 기획 방향이다. 플레이어가 라운드마다 주요 행동 하나를 예약하고, 속도 차에 따른 시작 지연과 실제 이동·실시간 피격으로 해결한다. 예약 행동을 무한 반복하는 자동 전투가 아니다. 입력 대기 감소, 위치·스킬 차이, 팀원의 지원이 자신의 선택에 미치는 영향을 검증한다.

현재 기본 Combat를 본 절의 시간차 전투로 교체했다. 기존 순차 턴·연속 AI 행동·End Turn 입력 실행은 제거했으며 기존 Blueprint의 클래스·프로퍼티 참조용 외형만 유지한다. 2026-09-18 확정한 이동·피해·준비·전멸·자원·복구 규칙은 본 절과 [TODO 3-1절](TODO.md#3-1-전투-규칙)을 기준으로 한다. 구현·컴파일과 사용자 작동 확인은 구분한다. 엄호는 기획과 관련 실행에서 제거하고 상태이상은 후순위로 유지한다.

### 8-2 행동 라운드

```text
다음 라운드 시작 상태 준비
→ 인간 계획 입력 전에 적 계획 결정·고정
→ 플레이어별 주요 행동 선택·복구 상태 저장 성공 후 준비 완료
→ 전체 준비·계획 검증 및 복구 상태 저장 성공
→ Ready Phase 종료·전체 명령 잠금·AP/SAP 즉시 차감
→ 예약된 SAP 이동 전체 완료
→ AP 전투 시간 0초부터 속도별 행동 시작
→ 이동·지원·시전·근접 타격·투사체·지연 피해 진행
→ 복귀를 포함한 모든 예약 행동의 완료·취소 및 잔여 공격 정리
→ 전투 결과 확인 또는 다음 라운드 계획
```

플레이어는 계획 단계에서만 행동을 선택한다. 해결 도중 재선택·단계 사이 재확정을 요구하지 않는다. 초안과 최종 명령을 구분하며, 인간은 원래 자기 캐릭터의 행동만 편집한다. 준비 완료 방식에는 강제 선택 제한시간이 없다. 잠금 전 준비 취소를 허용하고 계획·이동 예약을 수정한 소유자의 준비만 해제하며 다른 팀원의 준비는 유지한다.

2026-09-16 사용자 요청으로 예약된 SAP 이동 전체를 AP 행동보다 먼저 실행한다. AP 단계 안의 지원·공격은 기존 속도차에 따라 발동한다. 선행 피해로 이후 행동이 취소될 수 있으므로 공격 단계 시작 시 생존자 전원의 공격을 보장하던 동시 판정 가정도 적용하지 않는다.

2026-09-16 사용자 요청에 따라 전장의 적을 클릭한 뒤 하단의 실제 장착 스킬 버튼으로 행동을 적용한다. 전원 준비와 저장 성공 후 라운드를 잠그며 해결 중 재선택은 막는다. 적용 전 자원·타일·대상을 검사하고 원래 자신이 조작하는 모든 생존 유닛의 유효한 계획을 요구한다. 선택 대상 변경만으로 기존 명령을 바꾸지 않으며, 하단에 적용된 행동과 대상을 따로 표시한다. 서버가 소유권·수정 번호·최종 예약 충돌을 재검사한다.

준비 완료의 응답·복제 전에 해당 계획과 양 팀 유닛·라운드·자원·배치·고정된 적 계획 등 복구에 필요한 값을 저장한다. 준비 수정·취소도 저장된 상태와 일치해야 하며 저장 실패 시 기존 확정 상태를 보존한다. 저장이 실패하면 준비 완료와 Ready Phase 종료를 확정하지 않고 재시도할 수 있어야 한다. 강제 종료 뒤에는 마지막으로 저장한 준비 완료 상태를 복구한다. 진행 중 시전·투사체 위치를 임의 시점 그대로 복원하는 기능과는 구분한다.

SAP 이동은 계획 중 캐릭터별 목적지 하나를 예약·변경·취소한다. 선택 시 이동하거나 자원을 소모하지 않는다. 기존 8방향 빈칸 BFS·MoveRange·아군 진영 조건을 사용하고 이동 비용은 SAP 1이다. 공격의 추가 SAP 비용과 합산해 준비를 검증하고 전체 잠금 시 AP와 함께 차감한다. 모든 예약 SAP 이동을 마친 뒤 AP 시계를 0초부터 시작하여 기존 민첩 기반 시작 지연을 적용한다. 이동 중 공격 발동·피해·투사체 생성은 시작하지 않는다. 도착 위치를 새 HomeCoord와 AP 복귀점으로 사용하며 이동 실패·중단은 생존자를 출발점으로 복원하고 이미 차감한 자원을 환불하지 않는다. SAP 이동은 선택 사항이며 AP 행동만 예약하는 방식도 유지한다.

2026-09-17 사용자 요청에 따라 SAP 일반 이동은 고정 속도를 사용한다. 초기 조정값은 기존 기본값 700의 절반인 **350cm/s**이며 민첩·라운드 Speed·CharacterMovement의 `MaxWalkSpeed`에 연동하지 않는다.

### 8-3 행동 시작 시간

전투 속도는 현재 GAS 민첩과 1:1이며 소수 민첩도 유지한다. 속도 1 차이당 0.1초를 적용한다.

`행동 시작 지연(초) = (기준 최고 속도 − 해당 유닛 속도) × 0.1`

| 기준 최고 속도 | 유닛 속도 | 시작 지연 |
|---|---|---|
| 20 | 20 | 0초 |
| 20 | 19 | 0.1초 |
| 20 | 15 | 0.5초 |
| 20 | 10 | 1초 |
| 10 | 5 | 0.5초 |
| 21.25 | 20 | 0.125초 |

최고 속도 유닛은 시간 0초에 행동을 시작하며 같은 속도는 같은 시작 시각을 가진다. 행동 시작과 타격·발사는 구분한다. 실제 적중까지 접근·시전·비행 시간이 추가될 수 있다. 최고·최저 속도 비율 정규화와 지연 상한은 이번 프로토타입에 적용하지 않는다. 큰 속도 격차에 따른 긴 대기는 측정 대상으로 남긴다.

양 진영의 라운드 시작 생존 유닛에서 최고 속도를 구하고 시각표를 고정한다. `AUnitBase::GetCombatSpeed()`는 현재 GAS `Dexterity`를 float로 읽으며 독립 편집 값 20을 사용하지 않는다. 서버는 Planning 진입 시 속도와 지연을 복제용 라운드 View에 고정한다. 실행 중 사망·민첩 변경으로 이번 시각표를 재계산하지 않고 다음 라운드 Planning에서 다시 읽는다. Snapshot에 별도 Speed 필드를 추가하지 않으며 저장된 민첩으로 속도를 구한다.

근접 접근·복귀의 속도 보정은 [8-4절](#8-4-공격-접근과-복귀), SAP 고정 이동 속도는 [8-2절](#8-2-행동-라운드)을 따른다. 비근접 스킬의 시전·이동·투사체 속도 관련 추가 구현은 별도 작업으로 보류한다. AP·SAP는 라운드 계획 시작에 초기화하며 Ready Phase 종료 시 이동·공격 비용을 합산하여 즉시 한 번 차감한다. 한 주요 행동만 예약하며 이후 불발·실패·사망에도 환불하지 않는다. 저장 실패로 Ready Phase를 종료하지 못하면 비용도 차감하지 않는다. 직업·아이템의 세부 자원 밸런스는 후속 검토다.

### 8-4 공격 접근과 복귀

| 항목 | 확정 규칙 |
|---|---|
| 실제 접근 | 근접 접근은 실제 위치 이동이며 공격 시 상대 진영 진입 허용 |
| 접근 기준 | 스킬별 유닛/타일 접근 정의. 현재 검·비무장·휩쓸기는 대상 유닛의 현재 월드 위치를 추적하며 타일 좌표는 배치·복귀 기준으로 사용. 유닛 접근 범위에 도달하면 즉시 시전하고 정확한 간격을 맞추기 위해 후퇴하지 않음 |
| 유닛 충돌 | 아군·적군 유닛 간 이동을 막는 충돌 없음. 유닛이 다른 유닛의 길을 막지 않음 |
| 일반 공격 종료 | 공격 후 일반적으로 복귀 |
| 이동 공격 예외 | 자기 진영 안에서 이동과 공격을 함께 수행하고 위치를 변경하는 스킬 허용 |
| 상대 진영 잔류 | 행동 종료 후 상대 진영에 남는 결과는 허용하지 않음 |

2026-09-17 사용자 요청에 따라 근접(`Kind=Melee`) 접근·복귀를 감속하고 민첩에 연동한다. 아래 수식은 최종 밸런스가 아닌 **초기 튜닝**이다.

`근접 이동 속도(cm/s) = min(DA MoveSpeed × (0.25 + 라운드 Speed / 40), 100000)`

기본 `MoveSpeed=700`에서 속도 0·5·10·20의 접근·복귀는 각각 175·262.5·350·525cm/s다. Planning에서 고정한 Speed를 사용하며 실행 중 민첩 변경은 다음 라운드에 반영한다. 음수·비유한 Speed는 0으로 취급한다. SAP 이동은 [8-2절의 고정 속도](#8-2-행동-라운드)를 사용하며 비근접 스킬·시전·투사체 속도는 유지한다.

유닛 간 이동 충돌을 없애는 것과 투사체·근접 공격의 피격 검출은 별도 계약이다. 피격 검출까지 비활성화하는 의미가 아니다. 접근 중 원래 복귀 칸을 예약하되 피격은 실제 월드 위치로 검사한다. 다른 생존 유닛의 복귀 칸·예약 목적지로 이동하거나 자리를 교환할 수 없다. 서버는 SAP 예약과 최종 잠금에서 충돌을 검사하며 복귀형 공격의 임시 타일 접근 목적지도 예약 대상으로 검사한다. 이동 시도 후 실패·중단하면 생존 유닛은 이동 전 자신의 자리로 복원한다. 사망한 유닛의 복귀를 강제하지 않으며 사망 시 행동 취소 규칙을 적용한다.

초기 이동은 서버의 직접 위치 진행이며 유닛끼리 통과한다. NavMesh 경로 탐색과 물리적인 유닛 길막은 전투 실행에 사용하지 않는다. 기존 타일 전열 보호 판정과 보호 색 표시는 해제하며 위치 방어와 상태효과는 후속 설계다.

### 8-5 발동과 피격

2026-09-16 사용자 결정: **공격 적중은 액션게임과 같은 실제 충돌로 판정한다.** 서버가 공격 범위와 현재 전투에 등록된 생존 적의 Capsule 피격체를 검사한다. 대상 선택은 조준·접근 목적이며 적중을 보장하지 않는다. 별도 명중 확률·원작 성공 슬롯을 사용하지 않으며 치명타·방어·관통·상태 부여 조건을 이번 결정으로 확정하지 않는다.

| 사건 | 확정 규칙 |
|---|---|
| 행동 시작 전 사망 | 예약 행동 취소 |
| 공격 발동 전 사망 | 아직 발동하지 않은 공격 취소 |
| 투사체 발사 후 시전자 사망 | 발사된 투사체는 유지하고 비행·적중 처리 계속 |
| 근접 타격 | 발동 순간 전방 sphere sweep에서 가장 먼저 충돌한 유효 적 한 명에게 적용. 선택한 조준 대상과 실제 피격 대상은 다를 수 있음 |
| 지점 공격 | 발동 순간 지정 지점의 3D sphere overlap에 포함되고 벽에 가려지지 않은 유효 적에게 각각 한 번 적용 |
| 투사체 적중 | 이동 구간 sphere sweep과 시작 위치 overlap으로 최초 적 또는 벽 충돌에서 종료. 벽이 먼저 막으면 적 피해 없음 |
| 목표 선행 사망 | 취소·기존 지점 공격·재타기팅 등 처리 방식은 스킬별 정의. 공통 자동 보정은 확정하지 않음 |
| 지원 | 자기 시작 시각과 스킬 발동 조건에 따라 적용. 적 공격보다 늦게 적용될 수 있음 |

근접 공격의 `HitRange`는 전방 공격 범위의 끝이며 `MeleeRadius`는 공격 구체의 가로·세로 반경이다. 기본 반경은 35이고 실제 반경은 `min(MeleeRadius, HitRange / 2)`를 사용하여 전방 범위가 `HitRange`를 넘지 않게 한다. 지점 공격은 `AimLocation` 중심·`HitRange` 반경의 3D 구체를 사용하고, 중심에서 각 Capsule의 가장 가까운 접점까지 ray를 검사하여 벽 차폐를 적용한다. 이 수치는 편집 가능한 초기 프로필이며 최종 직업 밸런스가 아니다.

피격은 실제 Capsule만 검사하며 메시·센서·사망자·아군·현재 전투 밖 유닛은 제외한다. 벽은 `WorldStatic` 또는 `WorldDynamic` 물체 중 `ECC_WorldDynamic`에 `Block`으로 응답하는 충돌체다. Pawn은 벽 검사에서 제외하여 별도로 피격을 판정하며, Visibility 전용 UI 그리드와 Trigger의 Overlap 응답은 공격을 막지 않는다. 근접·지점 공격은 유닛마다 한 번만 피해를 적용한다. 투사체 프로필의 `bTargetOnly`는 지정 대상 한정 옵션이며 false이면 경로상 첫 적과 충돌한다. 시험 투사체를 실전 스킬로 자동 부여하지 않는다.

발사체 발사·근접 타격의 논리 시점과 표현을 구분한다. C++ 서버의 근접·지점 공격 쿼리와 투사체 발사는 기존 `WindupSeconds` 종료 시 수행하며, 투사체 충돌은 이후 서버 비행 진행마다 검사한다. 클라이언트의 프레임률·애니메이션 알림·충돌 결과가 서버 피해를 독립적으로 확정하지 않는다. 무기 소켓 궤적이나 애니메이션 알림으로 여닫는 타격 구간은 이번 범위에 포함하지 않는다. 스킬의 실제 시전·비행 시간을 변경하면 적중 순서가 달라질 수 있으며 이는 전술 규칙의 변화다. 표현만 바꾼 경우의 결과 일치와 혼동하지 않는다.

0.01초 서버 진행 단위에서 유닛·공격 사건을 안정적인 서버 순서로 처리하고 피해·사망을 즉시 반영한다. 같은 예정 시각이어도 먼저 사망한 유닛의 아직 발동하지 않은 공격은 취소한다. 이미 발사된 투사체는 기존 계약대로 유지한다. 아군 피해·투사체 관통·다단 타격·복잡한 상태효과는 초기 범위에서 제외한다. 유도·지정 대상 한정 여부와 발동 전 목표 상실 처리는 스킬 프로필로 설정한다. 최신 변경의 사용자 작동 확인은 [TODO](TODO.md#2-14-확정-전투-규칙과-준비-완료-복구)를 따른다.

### 8-6 라운드 종료와 적 계획

모든 유닛의 예약 행동이 완료 또는 취소되고, 남은 유효 투사체와 지연 피해가 정리된 상태에서 라운드를 종료한다. 일반 공격의 복귀도 행동 완료에 포함한다. 아직 유효한 공격이 남았다면 최초 사망이나 마지막 발사만으로 결과를 확정하지 않는다. 양 팀이 전멸하면 패배로 표시하고 Run을 종료하며 승리 보상을 지급하지 않는다. 결과 저장이 성공한 뒤 한 번만 확정한다.

투사체는 프로필 최대 수명에 도달하면 종료하며 이동에는 별도 완료/시간 초과 정리가 있다. 현재 지원하는 지연 피해는 선딜 후 발동·비행 중 공격이며 범용 지속 피해 예약 시스템은 미구현이다. 이는 플레이어의 선택 제한시간과 별개다.

적은 각 라운드의 시작 상태를 기준으로 인간 계획 입력 전에 행동을 결정·고정한다. 인간의 비공개 초안을 보고 재판단하지 않는다. 고정 명령의 실행에 필요한 대상 추적·목표 사망 대응은 스킬 정책으로 정의하며 새 전략 선택과 구분한다. 초기 UI는 아군 계획과 고정된 적 의도를 공개한다. 공개 수준의 최종 채택은 별도 결정한다. Snapshot AI 방어 팀과 의도를 보는 인간 공격 팀의 조건은 비대칭이므로 경쟁 밸런스·보상을 별도로 검토한다.

초기 AI는 기존 스킬 순서와 가까운 적 기준을 유지한다. 장착된 복귀형 Tile 공격은 적의 라운드 시작 HomeCoord를 목표와 필요한 접근 좌표로 구성한다. 지원·잔류 이동 전술은 [디자인 보류 항목](TODO.md#5-디자인-확정-후-구현할-일)으로 둔다. 실행 가능한 장착 후보가 없는 AI는 서버 내부 대기로 처리하며 플레이어 목록에 대기 스킬을 추가하지 않는다.

### 8-7 기본 전투 전환과 스킬 데이터

기존 Gameplay의 `ACombatManager`가 `ACombatRoundCoordinator`를 시작하고 CommonUI 계획 화면으로 연결한다. 순차 전투 보존용 별도 모드·맵·실행 경로는 제공하지 않는다. 원래 소유권·최대 4인·Human/AI 구분과 비전투 Run·상점 진행을 유지한다. 실제 두 사람 동시 입력과 4인 상태 일치는 사용자 검증 대기다.

`USkillDefinitionDataAsset::bUseRoundDefinition`을 켜면 `RoundDefinition`을 사용한다. 프로필은 공격 종류·유닛/타일 접근·자기 진영 잔류·목표 상실 대응·유도/대상 한정·선딜·위력·근접 반경·이동/투사체 속도·투사체 반경/수명·AP/SubAP 비용을 정의한다. `MoveSpeed`는 스킬의 기준값이며 근접 접근·복귀에만 [8-4절](#8-4-공격-접근과-복귀)의 민첩 보정을 적용한다.

`ResolveRoundSkill`은 명시 프로필 또는 제한된 이전 공격 정의를 검증한다. `bUseRoundDefinition=true`인 스킬은 새 프로필을 기준으로 하며 기존 GAS AbilityClass를 필수 실행 조건으로 요구하지 않는다. 명시 프로필이 없는 스킬은 EnemyUnit·Single/AroundTarget/접근형 TargetAndSides 또는 EnemyTile·AroundTarget과 유효한 `GA_AttackBase` 피해 정의의 지원 조합만 초기 변환한다. 지원하지 않는 대상/범위·커스텀 능력은 명시 프로필이 필요하다는 스킬별 오류로 거절한다.

| 지원되는 이전 정의 | 초기 라운드 변환 |
|---|---|
| EnemyUnit·Single·제자리 공격 | 투사체, 발동 전 목표 상실 시 마지막 위치 유지 |
| EnemyUnit·Single·`bMoveToTarget` | 실제 유닛 접근형 근접, 발동 전 목표 상실 시 취소 |
| EnemyUnit·TargetAndSides·`bMoveToTarget` | 대상과 양옆 한 칸씩을 훑는 근접, 발동 전 목표 상실 시 취소 |
| EnemyUnit/EnemyTile·AroundTarget | 선택 좌표 중심의 실제 위치 반경으로 적을 판정하는 지점 공격. 반경과 접근 여부를 기존 변환값으로 사용 |
| 기타 대상·범위·커스텀 능력 | 자동 의미 변경 없이 거절. 명시 RoundDefinition 필요 |

변환은 PrimaryAssetId·표시명·AP 비용·authored 공격 위력과 기존 AttackMontage의 시전 표현을 연결하며 나머지 시간·속도는 시험 기본값이다. 명시 프로필의 CastMontage가 있으면 우선 사용한다. 몽타주는 시각 표현이며 루트 모션·알림으로 이동이나 피해를 확정하지 않는다. 기존 GAS 효과·상태효과를 그대로 실행하거나 완전 이식하는 방식이 아니다. 회복 스킬을 다른 효과로 자동 치환하지 않는다.

Snapshot 상대도 같은 `ResolveRoundSkill` 검증을 사용한다. 에셋 ID로 중복을 검사하며 이전 GAS 클래스를 공유하는 서로 다른 명시 스킬은 허용한다.

실전 스킬 목록은 실제 장착 DA만 사용한다. 공통 시험 행동 주입·무장착 대체 공격과 Encounter 진입 시 스킬 풀 자동 추첨을 제거했다. `BP_PlayerUnit`은 비무장 공격·원거리 공격·AOE·휩쓸기 4개를 사용하며 2026-09-21 전사 `BP_WarriorUnit`에 검 공격을 추가하여 5개로 구성했다. 기존 기본공격의 `BPDA_DefaulatAttack` 경로·ID·공격값을 유지하고 표시명만 `비무장 공격`으로 변경했다. 직업별 최종 밸런스·스킬 구성은 별도다.

`BPDA_SweepingStrike`는 근접 접근 후 전방의 넓은 물리 충돌 범위로 여러 적을 타격한다. 명시 프로필의 `bUseMeleeAreaCollision=true`·`MeleeAreaHalfExtent=(75,250,100)`을 사용하며, 단위는 cm이고 값은 전방 깊이·좌우 폭·높이의 반크기다. 초기 시험 범위는 시전자 앞쪽으로 깊이 150·전체 폭 500·전체 높이 200이며, 시전자 방향을 따라 회전한다. 발동 순간 생존 적의 실제 Capsule과 겹치고 벽에 가려지지 않은 경우 적마다 한 번 피해를 적용한다. 타일 간격·배치·가장자리 또는 대상과의 같은 줄 여부는 피격 기준으로 사용하지 않는다. `MeleeArea=Single`과 `AreaType=Single`은 타일 범위 분기를 끄고 한 유닛을 조준하는 설정이며, 실제 복수 피격은 명시한 충돌 박스로 결정한다. 피해 10·AP 1·시전 몽타주·민첩 기반 근접 접근·몽타주 종료 후 복귀를 유지한다. 크기는 시험값이며 DA에서 조절한다. 기존 타일 범위 라이브러리와 `TargetAndSides` 실행은 보존하며 두 판정을 한 스킬에 동시에 켜는 정의는 거절한다. 이전 `DA_SweepingStrike` 경로·PrimaryAssetId를 리디렉션하고 표시명 `휩쓸기`와 Snapshot 별칭 `SweepingStrike`는 유지한다. [최신 확인](TODO.md#2-12-휩쓸기-근접-범위-충돌)

검 공격 `BPDA_swoard_attack`은 기존 공격값을 복제한 단일 근접 공격이며 피해 50·AP 1·민첩 기반 접근과 시전 종료 후 복귀를 사용한다. Paragon Kwang의 `PrimaryAttack_A_Slow` 1.2초와 `PrimaryAttack_A_Slow_Recovery` 0.933333초를 GKnight 전사와 기존 적 작업 사본에 IK 리타깃한다. 공격 끝부분과 포즈가 겹치는 복귀 클립의 첫 0.2초를 제외해 총 1.933333초로 연결하며 발동 0.23초·블렌드 인 0.08초/아웃 0.12초를 사용한다. 기존 Boss Swing 사본은 보존한다. 전사는 기존 4스킬을 유지하고 검을 추가하며 기본 적은 검 하나와 검 공격만 사용한다. Snapshot 입력의 저장된 스킬은 임의 교체하지 않는다. `RoundMontageOverrides`로 유닛별 뼈대에 맞는 표현을 선택하며 실제 검 위치·타격 표현은 [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) 대기다.

`BPDA_RangedAttack`과 별도 `BPGA_RangedAttack`은 기존 기본 공격을 복제한 제자리 단일 대상 원거리 공격이다. 피해 50·AP 1과 `MM_Attack_01_Montage`를 재사용하며 `EnemyUnit`·`Single`·`bMoveToTarget=false`·`bUseRoundDefinition=false`로 기존 투사체 변환을 사용한다. 투사체는 700cm/s·반경 12cm·최대 수명 5초이며 유도 없이 경로상 최초 실제 적 한 명에게 적중하거나 벽에서 종료한다. 원본의 빈 태그 설정을 보존하며 이 콘텐츠 추가가 GAS 태그 실행 연동 완료를 뜻하지는 않는다.

이전 순차 Combat 저장의 로드·관리 재개·새 순차 체크포인트 저장은 명시적으로 거절한다. 파일과 원래 소유권을 덮어쓰지 않는다. 새 라운드는 마지막으로 저장한 준비 완료 상태를 복구 기준으로 사용하며 첫 라운드와 다음 라운드의 Planning 상태도 저장한다. 네트워크·저장 계약은 [MULTIPLAYER 12절](MULTIPLAYER.md#12-시간차-자동-전투의-확장-경계), 최신 검증 상태는 [남은 확인](TODO.md#2-14-확정-전투-규칙과-준비-완료-복구)을 따른다.
