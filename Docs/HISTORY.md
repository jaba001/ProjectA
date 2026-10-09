# ProjectA 완료 작업과 검증 기록

갱신일: 2026-10-09. 완료 범위·기준 커밋·실행 근거를 기록한다. 현행 구현은 [PROJECT_PLAN](PROJECT_PLAN.md), 확정 기획은 [GAME_DESIGN](GAME_DESIGN.md), 다음 콘텐츠·온라인 서비스·에셋 도입은 [TODO](TODO.md)를 따른다. 삭제 전 상세 보고서는 Git 이력에 보존한다. 과거 검증은 이후 변경의 통과 근거로 사용하지 않는다. 과거 로그·맵 경로는 실행 당시 기준이며 현행 경로는 [레벨 폴더 기준](PROJECT_PLAN.md#4-10-레벨-폴더와-이전-경로-호환)을 따른다.

## 최근 변경

| 기준 | 변경·검증 |
|---|---|
| 2026-10-09 라이브러리 활 공격 애니메이션 | Sparrow 원본 FBX 1개에서 활 당기기·발사 몽타주를 작성해 정밀 화살·화염 화살비에 연결하고 구형 SkeletonGuard Snapshot을 함께 지원했다. 컴파일·에셋 작성은 통과했으며 실제 재생·그립·협동 확인은 남아 있다. [범위·근거](#9-41-2026-10-09-라이브러리-활-공격-애니메이션) |
| 2026-10-09 전투장 바닥과 표시 폴리싱 | 중앙 지면의 흑백 점무늬를 흙색으로 변환하고 표면 9개의 큰 무늬·색조·노멀 강도, 타일 상태별 투명도와 기본 전투 채도를 보완했다. 표면 저장·그래프 검사와 C++ 컴파일은 통과했으며 실제 화면·입력 검수는 남아 있다. [범위·근거](#9-40-2026-10-09-전투장-바닥과-표시-폴리싱) |
| 2026-10-09 라이브러리 NPC와 서비스 무대 | 원본 캐릭터 5종·호환 idle 2종·소품 22종을 직접 참조하여 상점·회복소·소모품점·부활소에 소품 34개를 배치했다. 컴파일·정적 검사는 통과했고 실제 화면·idle·협동 검수는 남아 있다. [범위·근거](#9-39-2026-10-09-라이브러리-npc와-서비스-무대) |
| 2026-10-09 지하 미로 갈림길 연출 | 후보 순서에 맞춘 좌·직진·우 통로와 코너·벽·천장을 로컬 런타임에 구성하고 선택 하단 3열·NPC 도착 후 오른쪽 거래 패널을 연결했다. Development Editor / Win64 컴파일·정적 검사를 통과했으며 실제 이동·화면·협동 확인은 남아 있다. [범위·상태](#9-35-2026-10-09-지하-미로-갈림길-연출) |
| 2026-10-09 장착 가능 아이템만 판매 | 장착 지원 49개만 판매·신규 보상 후보로 유지하고 미지원 240행·빈 전문점 15행을 제거했다. 기존 보유품·원본 에셋·저장된 보상은 보존하며 이전 미지원 진열은 숨기고 구매를 거절한다. [범위·근거](#9-34-2026-10-09-장착-가능-아이템만-판매) |
| 2026-10-08 위임 실행 검수와 협동 보완 | 초기 저장 원자성·새 협동 1인 1캐릭터·Snapshot 소모품 차단·NPC 지붕 가림을 보완했다. 고유 Native 222개, 동일 PC 1/2/4인 30전투·27상점, NPC 18경우, 기존 저장 사본·소모 후 Continue와 새 Win64 패키지를 검수했다. 정상 Run은 9전투·30선택 후 자연 패배이며 80단계 완주·최종 밸런스·Steam/PlayFab은 미완료다. [범위·근거](#9-33-2026-10-08-위임-실행-검수와-협동-보완) |
| 2026-10-07 TODO 권장안 로컬 구현 | T14 제안 9·10의 Steam 친구별 방 검색·요청 정리·재접속 인증 표시와 제안 12의 태그 기반 Snapshot 후보 선택 API를 보완했다. 프로젝트 파일 재생성 9.67초와 Development Editor / Win64 컴파일·링크 45.82초, 독립 코드·문서 정적 검사를 통과했다. 기존 Run·저장·에셋은 유지하며 게임·PIE·자동화·실제 Steam/PlayFab 연결은 미실행이다. [범위·근거](#9-25-2026-10-07-todo-권장안의-로컬-구현) |
| 2026-10-06 TODO 완료 항목 이관 규칙 | AGENTS에 완료 근거를 HISTORY로 통합한 뒤 TODO에서 삭제하는 규칙을 추가했다. 완료·중복 절 14개와 혼합 절의 완료 부분을 정리하고 실행 조건·과거 수치·검증 한계를 보존했다. 제안 선택 9개와 실제 미완료 확인을 유지하며 삭제 절의 참조를 갱신했다. 문서·링크·체크 보존·diff 정적 검사만 수행했으며 새로운 컴파일·작동 테스트는 하지 않았다. [이관 범위](#9-20-2026-10-06-todo-완료-기록-정리) |
| 2026-10-06 무기 랜덤 스킬·등급 기획 | 스킬상점·스킬 소지 상한 폐기, 무기만 태그 조건에 맞는 스킬 무작위 부여, 흰색·초록색·파란색·보라색·주황색 5등급을 목표 기획에 반영했다. 기존 GAS/아이템 태그·공통 추첨·상점/저장 의존을 정적으로 확인하고 기존 스킬상점 CSV를 폐기 대상으로 표시했다. 사용권·등급별 차이·추첨 시점·중복·기존 Run 전환은 당시 미선택 제안으로 남겼으며 이후 채택 결과는 [9-23절](#9-23-2026-10-06-무기-스킬-정책-선택)을 따른다. 문서·CSV·링크·diff 정적 확인만 수행했으며 코드·에셋·저장 변경과 컴파일·게임 실행은 하지 않았다. |
| 2026-10-06 속도 단일 스탯 | 힘·민첩·지능을 제거하고 GAS·직업 상세·전투·Snapshot·체크포인트·성장을 Speed 하나로 연결했다. HP/AP/SAP/보호막, 속도 10/5와 기존 행동·접근 시간은 유지한다. 이전 민첩·공통 성장은 로드 전용 필드로 속도에 이관하고 에셋 속성 Redirect 3개를 추가했다. 관련 회귀와 구태그 이관·재저장 검사를 작성하고, UE 5.8.3 Development Editor / Win64 최종 증분 컴파일·링크 6.36초, 독립 코드·엔진 소스 대조·문서 링크·diff 정적 검사를 통과했다. 프로젝트 에셋 339개의 바이트 조사에서 구 스탯 정확명은 기존 파티 DA만 확인했으며 에셋·저장 파일은 수정하지 않았다. 게임·PIE·자동화 테스트 미실행, [TODO 28절](TODO.md#28-속도-단일-스탯-확인)의 화면·이전 저장 이어하기 확인 대기. 근거: `Saved/Logs/SpeedOnlyStatsFinalBuild.log`. |
| 2026-10-06 저항 시스템 기획 폐기 | 1인 개발 부담을 줄이려는 사용자 결정에 따라 저항 수치·저항에 따른 피해 경감·저항 증감/무시 옵션을 개발 범위에서 제외하고 기획·TODO·README를 동기화했다. 원작 저항 자료는 참조 이력으로 보존하고 저항 중심 장비 후보는 제외했다. 현재 저항 구현이 없어 코드·에셋 변경 없이 속성 태그·공통 피해·보호막과 별도 상태이상 기획을 유지했다. 소스 대조·문서 링크·diff 정적 검사 통과, 문서만 변경하여 컴파일·게임 실행과 추가 작동 확인은 필요하지 않다. |
| 2026-10-06 체인 연결 속도 조정 | 연결 가시성 조정 요청에 따라 체인 5종 DA·설명·명세의 점프 간격을 0.15초에서 0.4초로 늘리고 기존 검수 기대값을 갱신했다. 마지막 연결 제거·대상 수·거리·피해·태그·원본 VFX 참조는 유지했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 16.30초, 5종 저장·독립 재로드·60종 데이터 검사와 원본 1,900파일 보존, Python 구문·CSV 변경 범위·문서 링크·diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행, [TODO 23절](TODO.md#23-다중-체인-공격-확인)의 체감 속도·잔상 확인 대기. 근거: `Saved/Automation/ChainTiming/{Author,Reload}.json`, `Saved/Logs/ChainTimingBuild.log`. |
| 2026-10-06 체인 최종 연결 제거 | 마지막 적 타격·후보 소진·중단 시 마지막 연결 파티클도 즉시 제거하고 복제 완료 상태로 늦은 이력의 재생을 차단했다. 별도 오디오·피격 효과·GAS 판정은 유지했다. 최종 제거·복제 완료 회귀와 기존 PIE 검수 기대를 갱신했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 17.23초, 독립 코드 리뷰·문서 링크·전체 diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행, [TODO 23절](TODO.md#23-다중-체인-공격-확인)의 실제 최종 연결 제거 확인 대기. 근거: `Saved/Logs/ChainFinalCleanupBuild.log`. |
| 2026-10-06 체인 이전 구간 잔상 제거 | 다음 체인 연결 시작 시 이전 Niagara·Cascade 컴포넌트를 즉시 제거하고 누적 복제 이력의 최신 유효 구간만 표시하도록 수정했다. 최초 표시 연결에서 사운드를 한 번 요청하며 별도 오디오·마지막 구간 자연 종료와 GAS·피해·점프 간격을 유지했다. 시각 교체·누적 이력 재생 방지 회귀 2개 및 PIE 관측 기대를 갱신했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 30.94초, 독립 코드 리뷰·문서 링크·전체 diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행이며 이동 대상의 실제 잔상 확인은 [TODO 23절](TODO.md#23-다중-체인-공격-확인)에 남긴다. 근거: `Saved/Logs/ChainVisualReplacementBuild.log`. |
| 2026-10-06 디버그 편성과 체력 | 독립 디버그 전투를 전사 1명·첫 번째 적 1명으로 변경하고 초기·수동 추가·전투 초기화의 양쪽 GAS 현재/최대 HP를 10000으로 설정했다. 공용 정의·일반 Run은 유지하며 기존 검수 소스의 편성·HP 기대값과 다수 표적 fixture를 조정했다. 기존 사용자 WorldMap 삭제와 관련 Redirect·감사 목록 정리를 함께 반영했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 27.50초, 코드 리뷰·Python 구문·문서 링크·diff 정적 검사 통과. 남은 프로젝트 에셋 339개의 바이트 검색에서 WorldMap 참조 0이며 엔진 재로드 검증과는 구분한다. 게임·PIE·자동화 테스트 미실행, [사용자 확인](TODO.md#27-디버그-초기-편성과-체력-확인) 대기. 빌드 근거: `Saved/Logs/DebugCombatDefaultsBuild.log`. |
| 2026-10-04 레벨 폴더 정리 | 프로젝트 341파일·43,833,372바이트와 외부 원본의 같은 크기 후보 29파일에서 SHA-256 동일 사본 0쌍을 확인했다. 고유 World 18개를 Unreal AssetTools로 용도·테마 하위 폴더에 이동하고 상태 스냅샷 동일성을 확인했다. 기존 던전 Redirector 2개를 삭제했으며 이번 이동 18개는 엔진이 옛 경로를 직접 정리했다. 옛 물리 경로/Registry 20개 부재와 디스크 13,963패키지의 옛 경로 전방 참조 0을 확인했다. 메뉴 Blueprint 참조·기본/쿠킹 경로·CoreRedirect를 갱신했다. UE 5.8.3 Editor / Win64 38.89초 컴파일·오류/경고 0, 적용/독립 재로드 종료 0과 옛 SoftObjectPath 20개의 정확한 새 World 해석을 확인했다. 보호 Content 19,786파일·원본 저장 49개·설정 1개·외부 Editor 키 설정의 SHA를 보존했으며 최종 Content 19,805파일에 예상 밖 추가는 없다. 로컬 에디터 카메라 좌표와 맵 경로/컴파일 시각 외 설정도 보존했다. 환경 14맵의 navigation을 공식 재빌드하여 각 64타일·native Recast payload SHA의 원본 일치와 18맵의 export 삭제 0을 확인하고 독립 재로드·전체 보호 SHA 검사를 다시 통과했다. 이번 이동 후 PIE·게임·자동화 테스트는 미실행이다. 근거: `Saved/Automation/LevelFolders/{Audit,Apply,Verify,ProtectedFinal,EditorMapPaths.Final,Payloads.AfterNavigationRepair}.json`, `Build.Editor.log`·`Navigation.Build.log`. [정리 기준](PROJECT_PLAN.md#4-10-레벨-폴더와-이전-경로-호환)·[후속 검수 이력](#9-18-2026-10-04-목표-run과-위임-후속-검수) |
| 2026-10-04 다중 체인 실행 | `Skill.Shape.Chain` 쿼리와 `Chain` 설정으로 서버 전용 연쇄 실행을 연결했다. 최근접 생존 미타격 적을 선택하고 첫 판정·태그·벽 차단·원래 시전자의 GAS·AP 한 번 소모를 유지한다. 원본 연결 VFX 5종은 구간별 직접 참조하며 파일을 복제하지 않는다. ID 기반 진행 상태·복제 표현과 구간 정리를 구현했으며 협동 지원 완료로 처리하지 않는다. UE 5.8.3 Development Editor / Win64 최종 컴파일을 통과했다. 기존 74스킬의 읽기 전용 해석·VFX 참조와 원본 1,900파일 보존 검사를 통과했다. CSV는 현재 체인 5행의 총 10셀만 변경하고 퇴역 577행·비대상 셀·바이트를 보존했다. 초기 수치는 선택 대기이며 DA 작성·독립 재로드 검증은 보류했다. 도입 당시 실행은 미검수였으며 이후 전체 175 native·5종 25사례/75장·실제 카드 구매/Continue 결과는 [최신 실행 이력](#9-17-2026-10-04-ue-58-todo-실행-검수)을 따른다. |
| 2026-10-04 스킬 VFX 방향 보정 | 전체 74스킬·원본 Niagara 79종의 그래프·주효과/피격 83슬롯을 정적으로 검수했다. 체인 5종의 월드 끝점·화염 화살비/우박 폭격 2종의 로컬 시작점과 상대 변환·LWC 타일 변환을 반영했다. 가시엄니는 비공개 모듈 입력 하나만 Local로 바꾼 필수 Niagara 파생본 1개(약 12.1MiB)를 추가했다. UE 5.8.3 Development Editor / Win64 컴파일·기존 DA 8개/파생본 1개 작성·독립 재로드·전체 감사·DataValidation을 오류·경고 없이 통과했다. 파생본의 나머지 좌표 입력·시각 설정과 재로드 해시가 일치했고 나머지 신규 52개·기본/몬스터 14개·원본 1,900파일을 보존했다. 기존 판정·태그·GAS·SFX·시점·풀·파티를 유지했다. 당시 게임·PIE·자동화 테스트는 미실행이며 이후 결과는 [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록했다. 근거: `Saved/Automation/SkillVfxDirection/`의 작성·재로드·전체 감사·원본 그래프·최종 보존 보고서. |
| 2026-10-04 체인 스킬 방식 분류 | `Skill.Shape.Chain`과 GameplayTagQuery 기반 체인 탭을 추가하여 방식 7탭·지원 효과 우선 분류를 유지했다. 링크 5종은 Beam을 보존하여 Chain만 추가하고 대상 하나 연결·판정·VFX/SFX·타이밍을 유지한다. CSV 각 700행에서 대상 5행의 총 25셀만 변경하고 퇴역 577행·비대상 행 바이트를 보존했다. UE 5.8.3 Development Editor / Win64 16.02초 성공·경고 0, 태그 작성 43.07초·독립 재로드 40.89초 종료 0·오류/경고 0. 나머지 신규 스킬 55개·보존 공격 14개·원본 1,900파일 해시와 CSV 재로드·렌더·문서 링크 검사 통과. 게임·PIE·자동화 테스트 미실행, 당시 다중 대상 연쇄 미구현. 근거: `Saved/Automation/ChainSkillFilter/{Author,Reload,CsvValidation}.json`; [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-04 사망 유닛 HP 표시 숨김 | HP 오버레이가 사망 유닛을 그리기 전에 제외하도록 수정하여 HP·보호막·체력바 전체를 숨기고 디버그 부활 후 재표시한다. UE 5.8.3 Development Editor / Win64 컴파일·링크 19.87초 성공, 코드·문서·diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행. [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-03 구입 VFX/SFX와 신규 스킬 도입 | Fab 6팩 원본 1,900파일·1,355,489,933바이트를 설치하고 Niagara 123개 중 스킬 60개·보조 63개를 명세로 구분했다. 신규 DA 60개·상점/Run 풀 2개를 작성하고 기본 파티를 61후보 상점에 연결했다. 기존 공격 14개·원본·퇴역 명세를 보존했으며 CSV 기존 577행 바이트를 유지하여 각각 700행으로 확장하고 SFX 234행을 추가했다. Development Editor / Win64 컴파일·에셋 작성·독립 재로드·DataValidation·원본/보존 공격 해시·CSV·문서·diff 정적 검사 통과. 원본 데모 입력 경고 20개, 작성/재로드 오류·경고 0. 시험 수치·판정 프로필이며 게임·PIE·자동화 테스트·실제 VFX/SFX 재생은 미실행. 근거: `Saved/Automation/DrGameSkills/Author.json`·`Reload.json`·`FinalPreservation.json`·`CsvValidation.json`; [도입 기준](PROJECT_PLAN.md#4-9-구입-vfx와-sfx-도입)·[사용자 확인](TODO.md#19-구입-vfxsfx-스킬-확인) |
| 2026-10-03 VFX 스킬과 기존 원본 팩 정리 | 생성 스킬 176개·원본 VFX 팩·방향 파생·전용 의존·풀·테스트 투사체의 2,813패키지·2,430,314,679바이트(약 2.26GiB)와 관련 17폴더 루트를 삭제했다. 두 기본 공격·몬스터 전용 공격 12개·공격 애니메이션·공유 원본을 보존하고 저장 호환·가용 후보 상점·생성 폐기·CSV 이력을 반영했다. UE 5.8.3 Development Editor / Win64 최종 컴파일·링크 4.74초, 엔진 삭제·독립 읽기 전용 재로드 모두 종료 0. 남은 스킬 14개·생존 Registry 20,963패키지 참조 검사, 추적 Content 293파일 SHA·남은 Content 17,844파일 메타데이터 보존과 CSV·문서·diff 정적 검사 통과. 적용 시 폐기 데모·Transient 경고 11개, 독립 재로드 오류·경고 0. 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/SkillReset/Apply.json`·`Reload.json`; [정리 범위](PROJECT_PLAN.md#4-8-기본-공격-외-스킬-정리)·[후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-03 CSV 보관 폴더 통합 | CSV 5개를 프로젝트 루트 `DataCatalogs/`로 이동하고 런타임 무기 로드·UFS 패키징 의존성·스킬 생성 명세·문서 링크를 갱신했다. CSV 내용·SHA·행 수와 스킬 원본 해시를 보존했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 16.92초 성공, 생성된 타깃 receipt의 무기 CSV UFS 경로·문서 링크·diff 정적 검사 통과. 게임·PIE·패키징 미실행. [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-03 캐릭터 즉시 생성 | 생성 버튼을 슬롯 기본 직업·기본 몸체·자동 이름의 즉시 생성으로 바꾸고 상세 편집은 Edit에서만 열도록 했다. 편집 취소 시 생성된 캐릭터와 편집 전 값을 유지하며 별도 직접 조작 선택은 보존했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 18.76초 성공, 문서 링크·diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행. [UI 기준](UI_README.md#4-화면프리뷰)·[후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-03 Lumen·Nanite 비활성화와 밝기 개선 | 프로젝트 지원·런타임 설정을 끄고 SSR·일반 메시 대체 LOD·일반 그림자로 전환했다. 환경 12맵·던전 2맵의 노출을 +1EV(2배)로 높이고 야외 SkyLight +35%·던전 보조광과 환경광을 적용했다. UE 5.8.3 실제 CVar 7개·던전 이동 Redirector 검사, 14맵 조명·navigation 저장·최종 독립 재로드 모두 종료 0. 오류 0이며 기존 commandlet CrowdManager·navigation 변환 경고가 있다. 작성 시 보호 대상 20,643파일 SHA·비조명 설정, 재로드 전후 Content 전체 20,657파일 SHA를 보존했다. 기존 사용자 이동을 포함하고 원본 메시 설정을 변경하지 않았다. C++ 변경과 렌더링·게임·PIE 실행은 없으며 실제 FPS는 미확인이다. 근거: `Saved/Automation/Lighting/{Inspection,Configuration,Reload}.json`; [구성 기준](PROJECT_PLAN.md#4-7-렌더링-설정과-비교-레벨-밝기)·[사용자 확인](TODO.md#14-렌더링과-밝기-확인) |
| 2026-10-03 미사용 프로젝트 에셋 정리 | Unreal 기능으로 미사용 애니메이션 결과 5,559개·4,592,948,479바이트(약 4.28GiB)를 삭제했다. 원본 Fab 팩·FBX·Rig·현행 콘텐츠·저장 호환·T12를 포함한 남은 Content 20,655파일의 SHA256 동일, 프로젝트 에셋 461개 보존. 삭제 후 빈 Registry 조회 오류를 수정했으며 독립 참조·몬스터·캐릭터 재로드와 기본 Paragon 2개 검사 모두 종료 0. Paragon 기본 임포트를 2개로 제한하고 명시적 전체·개별 선택과 계획 기반 정리 도구를 추가했다. Python 구문·선택 옵션·문서 링크·diff 정적 검사 통과, 게임·PIE·자동화 테스트 미실행. [정리 기준](PROJECT_PLAN.md#4-6-미사용-프로젝트-에셋-정리)·[후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| 2026-10-03 몬스터 콘텐츠 구현 | Fantasy_Pack·StylizedCreaturesBundle 원본 직접 참조로 10종·설원 재질 변형 2개와 native GroundSpeed·DefaultSlot·기존 GAS 기본 공격 연결을 작성했다. 기본 편성 오크·트롤·늑대·골렘 4개, 기존 검 적 포함 소프트 디버그 카탈로그 13개와 캡슐 생성 높이 보정을 적용했다. Development Editor / Win64 컴파일 및 에셋 작성·독립 재로드 종료 0. 신규 62개·1,673,195바이트(약 1.6MiB), 원본 5루트·1,574파일 SHA 보존, 기존 추적 5,973개 중 DA_DefaultEncounter만 변경, 재로드 전후 신규/변경 63개 SHA 동일. Gameplay 혼합 편성·앞열 2/뒷열 2 정적 검사 통과. 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/Monsters/Configuration.json`, `Reload.json`, `FinalFileAudit.json`; [구성 기준](PROJECT_PLAN.md#4-5-몬스터-콘텐츠)·[사용자 확인](TODO.md#12-몬스터-콘텐츠-확인) |
| 2026-10-02 지하 던전 비교 레벨 | 보유 FANTASTIC·Modular Dungeon Collection을 원본 경로에서 직접 참조하여 `DungeonFantasy`·`DungeonStone`을 작성했다. 메시·조명·불꽃은 각각 108·9·6개와 101·8·6개이며 기존 독립 전투 모드만 신규 맵에 지정했다. Gameplay·DebugCombat·기본 Run 연결을 보존하고 UE 5.8의 폐기된 `r.Mobile.VirtualTextures` 설정을 제거했다. 제작·두 맵 navigation 저장·별도 재로드 종료 0, 카메라 48표본·전장 여백·충돌/navigation 제외·원본 참조·저장 설정·보호 파일 7,732개 SHA 검사 통과. 재로드 오류 0·기존 Gameplay nav 변환 및 commandlet CrowdManager 경고 3건. Python 구문·문서 링크·diff 검사 통과, 렌더링·게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/Dungeons/Configuration.json`, `Reload.json`, `NavigationFantasy.log`, `NavigationStone.log`, `Static.json`; [사용자 확인](TODO.md#10-지하-던전-비교-레벨-확인). |
| 2026-10-02 설정 O 단축키 | `O`를 기존 `Esc`와 같은 CommonUI 설정 열기·닫기·확인 복원 경로에 연결하고 인벤토리 안내와 사용 문서를 갱신했다. UE 5.8.3 Development Editor / Win64 컴파일·링크 16.74초, 코드 검토·문서 링크·diff 정적 검사 통과. 당시 게임·PIE·자동화 실행은 미수행이었다. 이후 사용자 입력 확인은 [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록했다. 근거: `Saved/Automation/SettingsOShortcutBuild.log`. |
| 2026-10-02 포더킹 참고 목록형 인벤토리 | 상점과 `I` 창의 가방 격자를 24px 아이콘·이름·사본 수량의 세로 목록으로 교체하고 GameplayTagQuery 분류 탭·개수·선택 강조·클릭 상세와 별도 스킬 탭을 추가했다. 원본 ItemIndex·장비 Revision·권위 스킬 fallback·빈 가방 Drop·서버 장착 조건을 유지하며 원본 에셋·저장 형식은 변경하지 않았다. UE 5.8.3 Development Editor / Win64 최종 컴파일·링크 8.32초, 독립 코드 검토·문서 링크·diff 정적 검사 통과. 당시 에디터·게임·PIE·자동화 실행은 미수행이었다. 이후 화면·입력 확인은 [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록했다. 근거: `Saved/Automation/InventoryListFinalBuild.log`. |
| 2026-10-02 UE 5.8.3 마이그레이션 | 엔진 연결·게임/Editor 타깃을 5.8·V7·Unreal5_8로 전환하고 Notify·Niagara·IKRetargeter·ControlRig API 호환을 수정했다. 번들 .NET 10으로 프로젝트 파일 재생성 12.47초, 최종 Development Editor / Win64 컴파일·링크 12.30초 성공. 독립 코드 리뷰·문서 링크·솔루션·diff 정적 검사 통과. 당시 에셋 변환·로드·게임·PIE·자동화 실행은 미수행이었다. 이후 사용자 확인은 [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수)에 기록했다. 근거: `Saved/Automation/UE58ProjectFiles.log`, `UE58MigrationVerifiedBuild.log`. 로컬 설치 목록에 누락된 엔진은 HKCU `Unreal Engine/Builds`의 `5.8` 경로로 등록했으며 Launcher 설치 목록은 변경하지 않았다. |
| 2026-10-01 TODO 재검증과 완료 구현 이관 | TODO의 완료 구현·컴파일 설명을 남은 확인과 분리하여 [9-15절](#9-15-2026-10-01-todo-재검증과-구현-이관)에 통합했다. 사용자 실행 요청으로 자동화 166개·1/2/4인 진행·메뉴 버튼·실제 Quit를 확인하고 저장 거절 사유·호환 Skeleton 검 추적·게임 문맥의 표시명 비교를 보완했다. 최종 컴파일 13.34초와 대상 저장 검사 에디터 7개·게임 6개·실제 메뉴 Continue가 통과했다. 전체 메뉴의 idle 루프 실패와 제외한 표현 검사는 해당 절을 따른다. |
| 2026-10-01 TODO 공통 기능 보완 | 디버그 에셋 수집에 공통 삭제 경로 필터를 적용하고 전투 최종 HP를 결과·보상·단계와 같은 저장 후보에 반영하여 쓰기 실패 시 Run 파티 선변경을 방지했다. 기존 결과 호출 호환과 태그 분류를 유지하며 실패·재시도·잘못된 HP·중복 결과의 회귀 소스를 보완했다. 작성 도구 안내의 오래된 4스킬 고정 문구를 현재 기본 장착 목록으로 정정했다. UHT 포함 Development Editor / Win64 컴파일·링크 20.77초 성공, 독립 코드 검토·문서 링크·diff 정적 검사 통과. 회귀 소스는 컴파일만 수행하고 게임·PIE·자동화 테스트는 미실행이다. 새 팩·저장 교체 제안 5·실제 사용자 확인은 대기다. 근거: `Saved/Automation/TodoIndependentFixesBuild.log`; [검증 범위](#9-15-2026-10-01-todo-재검증과-구현-이관) · [구현 기준](PROJECT_PLAN.md#9-공통-검증과-실행-책임) |
| 2026-10-01 새 에셋 구입에 따른 TODO 정리 | 기존 팩·모델·애니메이션·장비/FX 수량에 묶인 확인을 채택 에셋 기준으로 수정하고 신규 도입 절차·저장 참조 교체의 미선택 제안 5를 추가했다. 기존 작업 ID·제안 1~4의 선택·저장/소유권/서버 판정 계약과 과거 구현·검증 이력을 보존하고 제목 변경 링크를 갱신했다. 문서 내용·링크·전체 diff 정적 검사 통과. 코드·설정·에셋·CSV 수정 및 구입·임포트·게임 실행 없음. 팩·교체 범위·실제 도입 검증은 대기다. [도입 계획](TODO.md#6-신규-에셋-선정과-도입) |
| 2026-10-01 AOE·테스트 원거리 콘텐츠·기존 저장 제거 | 스킬 DA 2개·미사용 전용 Ability Blueprint 2개를 엔진 기능으로 삭제하고 캐릭터 5개·이전 명령 패널의 참조를 정리했다. 기존 Run·Snapshot·체크포인트의 보유·진열·예약 제거를 확대하고 생성 스킬 176종·공통 범위/투사체 기능을 유지했다. Development Editor / Win64 20.65초·회귀 증분 13.13초·최종 서식 증분 4.76초 성공, 독립 코드 리뷰·Python 25개 구문·문서 링크 474개·diff 검사 통과. 저작 도구의 Python 반환값 처리 수정 후 삭제 성공·잔여 참조 0, 기존 저장 26개 원본과 다른 추적 콘텐츠 유지 확인. 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/RemoveTestAttacksBuild.txt`, `RemoveTestAttacksFinalBuild.txt`, `RemoveTestAttacks.json`, `RemoveTestAttacksStatic.json`; [검증 범위](#9-15-2026-10-01-todo-재검증과-구현-이관) |
| 2026-10-01 휩쓸기 콘텐츠·기존 저장 제거 | 스킬 에셋 1개 삭제·참조 7개 정리, 상점 179종/디버그 180종과 작성 도구의 재생성 방지 반영. 로컬·관리 Run 및 Snapshot 읽기 후보의 보유·진열·예약을 제거하고 인간 준비 해제·계획 Revision 갱신, 저장된 태그/가중치의 결정적 상점 빈칸 보충·다른 상태 보존 구현. Development Editor / Win64 22.62초·최종 회귀 증분 5.10초 성공, 독립 코드 리뷰·Python 25개 구문·문서 링크 473개·diff 검사 통과. 최초 저작은 저장 전 레지스트리 의존성으로 삭제를 보류했으며 패키지 정보 갱신 후 엔진 삭제 성공·잔여 참조 0. 기존 저장 26개 원본 및 다른 추적 콘텐츠 유지 확인. 회귀 소스는 컴파일만 수행하고 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/RemoveSweepingStrikeBuild.txt`, `RemoveSweepingStrikeFinalBuild.txt`, `RemoveSweepingStrike.json`, `RemoveSweepingStrikeStatic.json`; [검증 범위](#9-15-2026-10-01-todo-재검증과-구현-이관) |
| 2026-10-01 기본 Run 10전투·반복 상점 | 새 Run을 기존 DefaultEncounter의 전투 10회로 확장하고 1~9번째 승리 보상 뒤 상점 선택·퇴장, 마지막 보상 뒤 완료에 연결. 방문 회차·선택 초기화를 Continue와 함께 원자 저장하고 기존 두 전투 저장을 보존하며 지도 목록에 높이 300 상한 스크롤 적용. UHT 포함 Development Editor / Win64 23.78초·최종 회귀 증분 7.63초 성공, 독립 코드 리뷰·문서 링크·diff 정적 검사 통과. 전체 진행·중간 저장 실패/재개·반복 상점·레거시 회귀 소스는 컴파일만 수행했으며 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/TenBattleBuild.txt`, `TenBattleFinalBuild.txt`; [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) |
| 2026-10-01 스킬상점 5개 진열·전체 리롤 | 기존 4종과 생성 풀 176종을 직접 참조해 5개를 중복 없이 추첨하며 리롤 비용은 입장 1G·성공마다 +1G로 증가. 카탈로그·태그 조건·가중치·진열·비용 저장, 소유자·생존 Human·Revision 검증과 원자 차감·저장 후 공개, 기존 고정 상품 저장 보존. 화면은 진열만 복제하고 기존 풀의 cook 참조 연결. UHT 포함 Development Editor / Win64 29.44초·회귀 포함 15.23초·최종 UI 증분 5.13초 성공, 후보 180개·직렬화 풀 참조·CSV SHA·원본 콘텐츠 보존·문서 링크 464개·독립 코드 리뷰·diff 정적 검사 통과. 리롤/실패저장/재개/레거시 회귀 소스는 컴파일만 수행했으며 게임·PIE·자동화 테스트 미실행. 근거: `Saved/Automation/SkillShopStatic.py`, `SkillShopBuild.txt`; [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) |
| 2026-09-30 화살·도끼·베기 방향 보정 | Niagara 후보 26개 중 기존 로컬 본체 19개는 원본 유지, 월드 공간 7개만 원본 팩/하위 구조의 TargetDirection 필수 파생과 BPDA VFX 참조·direction_source에 연결. 베기 4개 속도/회전 보존, 투사체 3개 메시 본체 로컬·독립 전진 속도 제거·비메시 잔상 월드 공간 유지. 도끼·불꽃 화살의 활성 위치 이벤트 5개는 공식 GenerateLocationEvent/ReceiveLocationEvent 1.1로 갱신하고 저장본 확인. Cascade 로컬 PSA_Velocity에 0.01cm/s 방향값 전달. 실제 추가 7개·5,750,319바이트(5.48MiB)로 사전 최대 81MiB 안내와 구분. 원본 26개 SHA·577개 명세의 식별/설정/태그 보존, BPDA 7개 직렬화 데이터의 VFX 이름 참조 외 동일 확인. 외부 메시/재질 직접 참조와 베기 23개/이미터 162개·Swipe +X 경계 정적 조사. UHT 포함 Development Editor / Win64 최종 컴파일·링크 18.86초 성공. 독립 검토·저장본/명세 정적 검사와 Python 프로필 176개·정상 입력 2개·잘못된 입력 12개 거절·CSV 577행 식별 보존 검사 통과. 게임·PIE·자동화 테스트 미실행. 근거: Saved/Automation/CombatVfxDirection/Authoring.json; [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) |
| 2026-09-30 디버그 스킬 방식 필터 | 기존 효과·형태 태그의 GameplayTagQuery로 전체·투사체·범위형·근접공격·지원형·미분류 6탭 구현. 속성 8탭·검색과 교차하고 다른 분류의 선택·미보유 조건을 반영한 개수·현재 조건·선택 강조 표시. 지원 효과 우선·Beam 범위형, 생성 176종의 CSV 분류 대조 및 기존 5종 미분류 유지. 외부 스크롤과 목록 높이 확보로 작은 창의 분류 줄바꿈 대응. UHT 포함 Development Editor / Win64 32.74초·최종 12.00초 성공, 독립 코드·분류 검토 및 CSV 원본 5,780셀·577개 경로·176개 프로필·참조 SHA 보존 검사 통과. 게임·PIE·자동화 테스트 미실행, [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) 대기 |
| 2026-09-30 VFX 피격 대기·디버그 타이밍 조절 | 기본 0초 EffectHitDelaySeconds와 실제 경과 시계, 별도 스킬 타이밍 탭·보유 스킬 선택과 5개 타이밍/속도의 공용 임시 적용·복원·복사, 생성 명세의 선택적 timing 연결 완료. 원본 태그·DataAsset·Run 저장 보존, 장착/생성 이후 임시값 유지와 전투 초기화 복원. 기존 CSV 577행·10열/순서·태그·원본 577개·목적지 176개·상태 CSV 대조와 현재 source SHA 일치, 확인 필요 54개 보존을 읽기 전용 검사했다. Saved/ValidateVfxTimingStatic.py의 엔진 모형 기반 Python 검사에서 기존 프로필 176개·사용자 지정 2개 수용·잘못된 입력 12개 거절·CSV 원본 열/SHA 보존 통과. 문서 5개의 상대 링크 339개(앵커 276개)·독립 코드 검토·diff --check 통과. UHT 포함 Development Editor / Win64 최종 증분 빌드·링크 12.00초 성공과 후속 CLI의 최신 상태 확인 0.77초 통과. 지연/실제 시계/태그/차폐/사망/잘못된 입력/느린 프레임/디버그 공용 조정 회귀 6개 소스는 컴파일만 수행했다. 사용자 애니메이션 정상 보고는 당시 표현 범위에 한정하며 최신 게임·PIE·자동화 미실행, [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) 대기 |
| 2026-09-30 스킬 CSV 방식 분류 | 기존 577행·10열을 보존하고 스킬 방식·분류 기준 2열 추가. 투사체 95·범위형 91·근접공격 28·지원형 68·이동형 5·보조 효과 257·보류 33개. 생성 프로필 176개와 미생성 활용안을 구분하고 CSV 참조 해시만 동기화. 전체 원본 셀·행 순서·프로필·원본 경로 보존, CSV 재로드·분류 대조·문서 링크·diff 정적 검사 통과. 에셋·실행 코드 변경 및 Unreal 실행 없음. [구현 기준](PROJECT_PLAN.md#4-1-스킬-이펙트-에셋-목록) |
| 2026-09-30 디버그 스킬 속성 분류 탭 | ResolveRoundSkill의 효과 태그 캐시 기반 전체·5속성·복합·미분류 8탭 구현. 선택 강조·미보유 검색 결과 수·이름/에셋명 검색 조합·좁은 창 줄바꿈·목록 처음 이동과 초기화/전투 미준비의 0개 표시 연결. 복합은 각 속성에도 포함하고 보유 목록·원본 태그·에셋·전투 규칙 보존. UHT 포함 Development Editor / Win64 17.42초·최종 증분 6.25초, 독립 코드 검토·문서 링크 333개·diff 검사 통과. 게임·PIE·자동화 테스트 미실행, [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) 대기 |
| 2026-09-30 TODO 이력 이관·CSV 가격 검증 | 완료 체크 18개·이전 마녀/Assassin 구성을 9절로 이관하고 미확인 절차·기존 작업 번호·정책 선택 4개 보존. CSV 가격의 전체 문자열·양수 int32 범위 검사와 기존 카탈로그 보존 회귀 소스 추가. Development Editor / Win64 최종 빌드·링크 15.50초 성공, 오류/경고 출력 0·독립 코드 검토 통과. 원본 CSV 872개 경로·표시명, 상태 577개·생성 176개 BPDA·스펙/sourceSHA·확인 필요 54개 정적 대조 통과. 문서 링크 408개·전체 스테이징 diff·diff --check 통과. 기존 미확인 62개와 정책 선택 4개 보존을 대조했으며 게임·PIE·자동화 테스트 미실행. [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) |
| 2026-09-30 디버그 도구 확대·체력 설정·스킬 표현 | 확대 패널·3개 탭·보유/전체 독립 스크롤과 양 진영 생성 후 선택, 계획 단계 GAS 최대/현재 HP·전부 회복 구현. 효과 판정 기간을 유지하며 VFX 자연 완료·추가 5초 제한, 투사체 기본 0.5배 감속·사거리 보존·발사 방향/시계와 지원 Cascade 인스턴스 파라미터 연결 보완. 최종 Development Editor / Win64 빌드·링크 11.97초, diff·문서 링크 365개 검사 통과. PIE·게임·자동화 테스트 미실행, [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) 대기 |
| 2026-09-28 디버그 전투 도구·VFX 사전 준비 | CommonUI 생성 순서·null HUD 수정 후 요청 PIE에서 전투 UI·공허의 폭발 피해·종료 확인. 아군 부활, 직업/적 정의 기반 추가, 기존 ID·장착·계획 보존과 종료 후 재개 구현. 네이티브 부활 2종·추가 3종은 축소 월드 정리 경고 동반 성공. 이후 최초 Niagara 컴파일 지연을 확인하고 VFX 사전 준비·유효성 검사·참조 유지와 부활의 기존 SAP 경로 보존 반영. 최신 Development Editor / Win64 빌드·링크 7.43초, diff·링크·저장 26개 보존 검사 통과. 보완 회귀·실제 맵 회귀 소스는 컴파일만 수행했으며 최신 부활 자세·추가 UI·VFX 화면은 사용자 확인 대기. [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) |
| 2026-09-28 전투 디버그 레벨 | 무료 테스트 상점·전용 풀 제거, 일반 3상점 복원·옛 테스트 태그 호환 해석. 독립 DebugCombat 1개·native 모드/컨트롤러/도구 UI로 스킬 181종·장비 49종의 계획 단계 추가·제거·즉시 반영·초기화 지원. Development Editor / Win64 컴파일, 맵·스킬 181패키지·기존 저장 26개 보존 정적 검사, 별도 재로드 1.41초 통과. 맵 전환·종료 중 CrowdManager의 NavData 재탐색 경고 1종은 엔진 정리 경로로 확인했으며 MapCheck 오류/경고 0. 실제 게임·자동화 테스트 미실행. [구현 기준](PROJECT_PLAN.md#4-2-전투-디버그-레벨) |
| 2026-09-27 무료 테스트 스킬상점 | 새 Run의 Shop_03을 전체 BPDA 181종·0G 테스트 상점으로 연결. 전용 태그·직렬화 목록·서버 구매·스크롤 UI를 연결하고 기존 5개 한도·일반 상점·이전 저장 정책 유지. Development Editor / Win64 컴파일 20.75초, 데이터 검증·독립 재로드 1.11초(오류/경고 0), 원본 스킬 181패키지·저장 26개 해시 보존 확인. 무료 구매·원자 저장·호환 회귀 소스 추가·컴파일, 실제 게임·자동화 테스트 미실행. 2026-09-28 독립 디버그 레벨로 대체 |
| 2026-09-27 스킬 경로·파일명 정리 | 176개를 AssetTools로 `Skills/` 직속·해시 없는 파일명으로 이동, 중복 2그룹 4개만 `_1`, `_2` 적용. 내부 SkillId·표시명·프로필·풀 보존, Redirector 176개 정리·Package/Object·PrimaryAssetId 리디렉션 설정. 이동 7.03초·독립 재로드 9.86초, 새 경로/옛 저장 SoftObjectPath 각 176개·611파일 해시·CSV 176경로만 변경 확인. 구경로 최초 탐색 경고 176개 후 CoreRedirect 복구 성공, 새 에셋 오류/경고 없음. PrimaryAssetId는 엔진 CDO 설정 로드 확인이며 네이티브 조회·실제 게임 복구 미검증. [기획 기준](GAME_DESIGN.md#8-7-기본-전투-전환과-스킬-데이터) |
| 2026-09-27 인카운터 풀 CSV | 기존 상점 3개와 5속성별 무기·스킬 상점 10개를 `ENCOUNTER_POOL.csv` 13행으로 정리. 기존 ID·태그 보존, 신규는 기획·미연동 표시, 미정 가중치 공란. CSV 재읽기·ID/이름 중복·5속성 조합·미리보기·문서 링크 정적 검사 통과. 코드·엔진 에셋·게임 실행 변경 없음 |
| 2026-09-27 CSV 기반 스킬 생성 | 원본 VFX 참조 BPDA 176개·별도 풀 생성, 불명확한 401개 보류·행별 현황 CSV 작성. 단일 투사체 충돌 소멸·다중 대상 베기/범위 Query 판정·GAS 즉시 치유/라운드 한정 보호막·5속성 태그 연결. 보호막 재시전 합산·빔의 벽 앞 판정 보완 포함 Development Editor / Win64 최종 증분 컴파일 4.58초, 데이터 검증·읽기 전용 재로드 8.88초(경고/오류 0) 통과. 원본 CSV와 기존 583패키지 해시 보존, 기존 상점·장착 유지. PIE·게임·자동화 테스트·협동 작동 미검증. [기획 기준](GAME_DESIGN.md#8-7-기본-전투-전환과-스킬-데이터) |
| 2026-09-25 에셋 게임 내 이름 | 무기 295개·이펙트 577개 한국어 표시명 추가, 원본 열·경로·가격 보존. 새 Run 시작 장비·상점·인벤토리·장비창 반영, 기존 저장 이름 유지·4열 CSV 호환. Development Editor / Win64 컴파일(최종 증분 4.44초)·CSV 정적 검사 통과. 파싱/저장 호환 테스트 소스 추가, 게임·자동화 테스트 미실행. [구현 기준](PROJECT_PLAN.md#4-콘텐츠ui-설정) 대기 |
| 2026-09-25 스킬 이펙트 목록 | `SKILL_EFFECT_ASSETS.csv` 577행: NiagaraSystem 343·ParticleSystem 146·효과 구성 BP 88. Slash/Trail·마법 투사체/장판/시전/피격/빔/보호막 등을 이름·폴더 기준 분류. uasset 15,007개 AR 및 대상 689개 최상위 Export 정적 파싱·클래스 대조, 시스템 누락/중복 0·경로/CSV 형식/문서 링크 검사 통과. 엔진 실행·원본 변경 없음. 시각 확인 필요 54개는 당시 미확인 범위였다. [현행 목록](PROJECT_PLAN.md#4-1-스킬-이펙트-에셋-목록) |
| 2026-09-25 시작 장비·상점 드래그 장착 | 네 직업 시작 장비·양손 점유·상점 드래그 교체/해제·원자 저장/복구·원본 메시 부착 구현. 기존 스킬·검 판정·이전 저장 보존, 장착 49개 지원·나머지 246개 보관 유지. UHT 포함 Development Editor / Win64 컴파일·최종 증분 빌드 11.22초·원본 경로 295개·문서 링크·diff 정적 검사 통과. 게임·PIE·자동화·cook 미실행. 근거: `Saved/Logs/StartingEquipmentBuildFinal.log`; [구현 기준](PROJECT_PLAN.md#3-1-시작-장비와-장착) 대기 |
| 2026-09-25 상점 인카운터 명명·분류 | 상점1·2를 스킬상점·아이템상점으로 명명하고 인카운터 GameplayTag를 진입·구매·리롤·UI 공통 분류에 연결. 기존 저장 ID·사용자 지정 이름·상점3 보존. UHT 포함 Development Editor / Win64 컴파일 17.20초·정적 검사 통과, 게임·자동화 테스트 미실행. [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) 대기 |
| 2026-09-25 상점2 아이템 시험 | 상점1·3 유지, 무기 CSV 295개 가격 1G·원본명·중복 없는 5개 진열·1G 리롤과 개인 아이템 보관·인벤토리 표시 구현. 구매·리롤은 저장 성공 후 반영하고 이어하기에서 복원. UHT 포함 Development Editor / Win64 컴파일·최종 증분 빌드 5.49초·CSV 원본 3열/행 순서·에셋 경로 295개·정적 검사 통과. 게임·자동화 테스트 미실행, [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) 대기 |
| 2026-09-25 상점 장비·인벤토리 패널, 0ab5792 | DemonicUI WB_Equipment·WB_Bag를 참고한 장비·상품·본인 인벤토리 3열과 I 창 공통 패널 구현. 원본 제목 바·프레임·실루엣·아이콘·이름/수량/스킬 조회·긴 이름 줄바꿈·스크롤 연결. 당시 장비 9칸은 미장착 안내였으며 실제 장착은 이후 2-33으로 대체. UHT 포함 Development Editor / Win64 17.91초·참조/레이아웃/권한/링크·스테이징 diff/공백 검사 통과. 게임·PIE·자동화 미실행. [UI 기준](UI_README.md#2-실행과-옵션) |
| 2026-09-25 생성 화면 정면·거리 통일 | 소스·저장 맵의 카메라 X `-500`, 슬롯 앵커 Yaw `90°`, 네 프리뷰 Blueprint·남녀 프리뷰 변환 회전 `0°` 적용. 기본 방향과 임시 드래그 회전을 분리해 편집 종료·몸체 갱신 시 복원하며 슬롯 위치·상세 거리 `1.15` 유지. Development Editor / Win64 컴파일 17.41초·독립 재로드 오류/경고 0. 메시·애니메이션·재질·전투 몸체 변환·의상 103개와 사용자 마법사 Staff 컴포넌트 삭제 보존 확인. 기존 파라곤 리그 12개 삭제도 보존하며 13,084개 패키지·92,614개 의존 관계에서 참조 0개 확인. 원본 복제·실제 UI/게임 실행 없음. 근거: `Saved/Automation/PreviewFacingBuild.txt`, `PreviewFacingReload.json`, `PreviewFacingPreservation.json`, `DeletedParagonRigsAudit.json`, `Saved/Logs/PreviewFacingReload.log`, `DeletedParagonRigsAudit.log`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-25 캐릭터 프리뷰 거리 조정 | MainMenu 카메라 X -700→-600·슬롯 Y ±675/225→±450/150으로 확대·중앙 정렬. 상세 거리 배율 1.35→1.15를 `Focused Camera Distance Scale`로 노출하고 [Details 조절 방법](UI_README.md#4-화면프리뷰) 기록. Development Editor / Win64 컴파일 13.92초 성공·저장 맵의 카메라/슬롯/직업 연결 독립 재로드 오류/경고 0. 실제 UI·게임 미실행. 근거: `Saved/Automation/PreviewCameraDistanceBuild.txt`, `PreviewCameraDistanceReload.json`, `Saved/Logs/PreviewCameraDistanceReload.log`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-25 기본 스태프 제거와 프리뷰 드래그 회전 | 마법사 메뉴·전투·Snapshot Blueprint 3개의 기본 `Staff` 메시·표시·충돌을 제거하고 원본 에셋 보존. 생성/수정의 회전 버튼을 미리보기 우클릭 좌우 드래그로 교체하고 해제·화면 닫기·캡처 상실 시 종료하며 몸체 선택 화살표 유지. Development Editor / Win64 컴파일 15.18초 성공, 작성·읽기 전용 재로드 오류/경고 0. 실제 UI·게임 미실행. 근거: `Saved/Automation/MageStaffPreviewDragBuild.txt`, `MageDefaultStaffConfigure.json`, `MageDefaultStaffReload.json`, `Saved/Logs/MageDefaultStaffReload.log`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-25 Primitive 남녀 몸체 선택 | 네 직업 생성/수정에서 `BodyVariants` 배열을 화살표로 순환하고 남자 `SKM_Primitive_Charater_01_Body`·여자 `SKM_Primitive_02_Body` 원본 참조. `BodyId`를 기존 appearance 구조의 Run·체크포인트·Snapshot·복제로 전달하며 이전 누락 값은 기본 남자로 해석. ROG 의상 UI·착용 중지, 8부위·103개 항목과 원본은 향후 아이템용 보존. 원본 Skeleton의 호환·리타깃·DefaultSlot 설정만 갱신하고 메시·물리 4개 해시 불변, 모델·텍스처 복제 없음. Development Editor / Win64 컴파일·독립 재로드 통과, 오류/경고 0. 두 몸체 포즈 138개·스태프 부착 6개·물리 연결과 기존 의상 ID·BodyId 값 직렬화 왕복 확인. 실제 UI·게임·PIE 미실행. 근거: `Saved/Automation/PrimitiveBodyBuild.txt`, `PrimitiveAppearanceReload.json`, `PrimitiveSourcePreservation.json`, `Saved/Logs/PrimitiveAppearanceReload.log`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-25 의상 착용 시 Manny 텍스처 보존 | ROG 신체 파츠에 원본 Manny 두 MI·텍스처를 직접 연결하고 Head/Arms/Legs만 원본 경로에서 두 재질 슬롯으로 복원. 정점 위치·UV·노멀·가중치·뼈대·물리와 기존 ROG 기본 재질 보존, 모델·텍스처 복제 없음. `UCharacterAppearanceAssetLibrary`와 `-RogAppearanceMaterialsOnly`로 기존 직업·Blueprint·스태프 설정을 유지하며 미사용 중립 MI 제거. Development Editor / Win64 빌드 4.35초 성공·별도 재로드 오류/경고 0. 6개 파츠의 93,607개 삼각형 재질 영역, 미수정 3개 파츠 해시와 의상 목록 불변 확인. 작성 시 삭제 참조 수집 전환 경고 1개 후 정상 삭제·독립 재로드 통과. 실제 UI·게임 확인 전. 근거: `Saved/Automation/TexturedMannyBodyBuild.txt`, `RogAppearanceReload.json`, `Saved/Logs/TexturedMannyBodyReload.log`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-24 네 직업 의상과 TopDown 기본 외형 | ROG 8부위·103개 의상 UI를 네 직업에 공통 연결하고 무의상·액세서리 상태는 TopDown 원본 Manny 몸체·재질 유지. 신체를 가리는 의상은 원본 ROG 파츠와 부위별 색 구분 없는 프로젝트 전용 중립 MI 한 개 공유. 원본 메시·텍스처 복제 없음. 마법사 스태프의 `hand_l` 부착, 기존 궁수 Unit/프리뷰 경로·이전 Snapshot 호환 보존. Development Editor / Win64 컴파일·별도 재로드의 원본 메시 120개·본 161개·검 표본 328개·스태프 부착 3개·물리 연결 22개·재질 저장 검사 통과. 신규 MI는 4,005바이트. UI·게임·자동화 실행 없음. 근거: `Saved/Automation/AllProfessionsAppearanceBuild.txt`, `RogAppearanceReload.json`, `Saved/Logs/AllProfessionsAppearanceReload.log`; [검증 상태와 사용자 확인](UI_README.md#4-화면프리뷰) |
| 2026-09-24 전사 공통 외형과 ROG 의상 UI | 전사를 Manny 공통 몸체·애니메이션으로 전환하고 원본 ROG 기반 8부위·103개 의상 선택·회전 프리뷰·저장/취소 UI 연결. Run·Snapshot·체크포인트·복제에 외형 ID를 전달하며 이전 빈 선택과 Snapshot 클래스 호환 보존. 원본 복제·추가 리타깃 없음, 기존 래그돌 실행 유지. Development Editor / Win64 빌드·독립 재로드의 메시 120개·본 161개·검 표본 82개·물리 연결 22개 검사 통과. UI·게임·자동화 실행 없음. 근거: `Saved/Automation/RogAppearanceBuild.txt`, `RogAppearanceReload.json`; [UI 기준](UI_README.md#4-화면프리뷰) |
| 2026-09-23 마녀·Assassin 외형과 배율 정리 | 마법사 Stylized Dark Witch·도적 Assassin Skin1의 원본 경로를 메뉴·전투·Snapshot에 연결하고 기존 스태프 한 개를 왼손에 부착. 마녀 루트 배율 100을 제거하고 형상·바인드 자세·변형 본 계층·물리를 같은 임포트 경로에서 정리해 높이 187.12cm·본 배율 1·연결된 물리 바디 48개로 저장. 편의 복제 없이 리타깃만 생성, 전사·궁수·래그돌 실행 유지. Development Editor / Win64 컴파일·별도 재로드·시퀀스 48개 포즈/배율·검 표본 164개·물리 연결 검사 통과. 기존 MainMenu 변경 포함, 게임/PIE/자동화 테스트 미실행. 근거: `Saved/Automation/WitchAssassinBuild.txt`, `WitchAssassinReload.json`; [당시 구성](#9-13-마녀와-assassin-외형) |
| 2026-09-23 선택 화면 전사 외형 보완 | MainMenu의 Warrior만 `BP_WarriorMenuPreview`로 연결하고 GKnight 원본 메시·기존 `MM_Idle_Warrior` 반복 재생 적용. 전투·다른 직업 프리뷰 불변 해시, Blueprint 컴파일·별도 재로드 통과. 작업 중 별도로 삭제된 지팡이 임포트 에셋 5개를 보존하여 함께 반영하며 외부 참조 없음 확인. 에셋 복제·C++ 변경·게임 실행 없음. 근거: `Saved/Automation/WarriorMenuReload.json`, `RemovedStaffReferences.json`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-23 파라곤 외형 롤백·전체 래그돌 복구 | 전사 GKnight·나머지 Manny·적/Snapshot Skeleton_Guard·공통 메뉴 프리뷰 복원. 기존 저장의 직업별 Blueprint 경로는 이전 모델로 연결하고 스킬·후속 기능·원본 참조 통합을 유지했다. 전 진영에 기존 래그돌·복제 충격량 복구, 사망 애니메이션 분기·작성 도구 제거. 에셋 일괄 삭제는 자동 승인 검토에서 범위 불명확으로 거절되어 미사용 생성 결과·팩·사용자 수정 에셋을 보존했다. Development Editor / Win64 빌드 22.48초·11개 전투/4개 프리뷰 별도 재로드·검 궤적/기존 리타깃·저장 파일 26개 불변 검사 통과. 복구 저장본 재로드 오류/경고 0, 별도 구경로 호환 검사는 해석 성공 전의 기존 경고 6개·오류 0. 게임·PIE 미실행. 근거: `Saved/Automation/PreParagonRollbackBuild.txt`, `PreParagonRollbackReload.json`, `WarriorContentReload.json`; [외형 확인](PROJECT_PLAN.md#6-gameplay-에셋과-배치)·[래그돌 확인](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-23 마법사 지팡이 손 소켓 보정 | 메뉴·전투·Snapshot Blueprint 3개의 `Staff`를 원본 `hand_lSocket`에 연결. 손목 쪽 배치와 손가락 길이 방향 축을 손바닥 안쪽의 가로 축으로 보정하고 메시 피벗과 실제 손잡이 중심의 차이를 반영했다. 생성 명세 갱신, 원본 복제·수정 없음. Blueprint 컴파일·별도 재로드 및 원본 메시/뼈대/지팡이 해시 3개 검사 통과, 재로드 오류 0·기존 Rig 경고 1. PIE/게임 미실행. 근거: `Saved/Automation/MageGrip/Reload.json`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-23 메뉴의 불필요한 지팡이 숨김 | MainMenu에 독립 배치된 `SM_Staff_02·03·04`의 게임 내 표시·충돌 비활성화. 배치·원본 에셋과 마법사 손의 `SM_Staff_01` 유지, 복제·삭제 없음. 별도 저장본 재로드에서 대상 3개 숨김·충돌 해제와 마법사 지팡이 표시 확인, PIE/게임 미실행. 근거: `Saved/Automation/MenuStaffVisibilityReload.json`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-23 적 래그돌 복원 | 사용자 요청에 따라 사망 물리 코드의 주석을 해제하고 `ETeam::Enemy`에 기존 래그돌·서버 충격량 생성/복제/적용 복원. 아군의 단발 사망 애니메이션 유지, Snapshot 상대도 적 물리 분기 사용. 에셋 변경 없음. Development Editor / Win64 빌드 17.43초·정적 검사 통과, PIE/게임 미실행. 근거: `Saved/Automation/EnemyRagdollBuild.txt`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-23 사망 애니메이션 전환 | 기존 래그돌·충격량 생성/적용 코드 주석 보존, 단발 사망 시퀀스·종단 자세 유지. 파라곤 원본 4종과 기본 Manny 직접 참조, 해골 적 리타깃 1개(265,642바이트), 전투 Blueprint 11개 연결. Development Editor / Win64 빌드 6.69초·별도 저장본 재로드/포즈 검사 통과, 재로드 오류 0·기존 Rig 경고 4. 작성용 에디터 시작 시 자동 Smoke 검사 조건 오류 4개와 종료 코드 0xC0000005가 발생했으나 저장 완료 후 별도 commandlet은 종료 코드 0으로 통과. PIE/게임 미실행. 근거: `Saved/Automation/DeathAnimationBuild.txt`, `DeathAnimationsReload.json`, `Saved/Logs/DeathAnimationsReload.log`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-22 공통 검증과 실행 책임 분리 | 능력치/장착·계획 검증 공통화, AI·충돌·GAS 효과 실행 분리와 실제 태그 조건 연결, 즉시 효과 적용 거절·미지원 지속 효과 검증 보완. Run 경로/저장 버전·후보 저장 후 반영, 4슬롯 초안·UI 갱신·구형 호환 코드·에디터 리타깃 공통 함수 분리. 테스트 전용 효과를 Editor 모듈로 분리하고 에셋 정리 결과를 보존하여 통합. 원래 프로젝트의 파일 재생성 10.30초·UHT 포함 Development Editor / Win64 빌드 32.64초 성공, 오류/경고 0. 독립 코드 검토·Python 구문 17개·문서 내부 링크 302개·diff 검사 통과. 추가 회귀 코드는 컴파일만 수행하며 게임·PIE·자동화·에셋 재작성 미실행. 근거: `Saved/Automation/RefactorIntegratedBuild.txt`, `RefactorIntegratedStatic.json`; [구현 기준](PROJECT_PLAN.md#9-공통-검증과-실행-책임) |
| 2026-09-22 User_JeHoon 복사본 정리 | 원본 직접 참조·외부 팩 간 비교 금지 규칙 반영. Manny/GKnight/Skeleton_Guard 사본 6개를 Unreal로 통합하고 애니메이션 참조·원본 두 뼈대의 DefaultSlot·이전 경로 호환 보존. 검 소켓 수정본과 필수 파생 결과 유지. Content 순감소 34.01MiB. Development Editor / Win64 최종 빌드 2.94초, 별도 재로드 5,385개·공용 3종/직업 8종의 검 포즈 샘플·원본 참조 확인. 오류 0·옛 이름 조회 후 해석 경고 4·원본 Rig 경고 4. PIE/게임 미실행. 근거: `Saved/Automation/CopiedAssetsBuild.txt`, `CopiedAssetsDiskResult.json`, `CopiedAssetsReload.json`, `Saved/Logs/CopiedAssetsReloadFinal.log`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-22 직업별 파라곤 외형 | 전사 Kwang·궁수 Sparrow·마법사 Gideon·도적 Countess 원본 참조, 메뉴/전투/Snapshot 연결과 `SM_Staff_01` 왼손 부착. 필요한 전투 동작만 리타깃하고 기존 장착·검 판정 유지. Development Editor / Win64 빌드 19.96초 성공, 별도 저장본 재로드에서 직업 매핑·몽타주·플레이어/Snapshot 8종 검 샘플·지팡이/내장 무기 설정 확인. 원본 메시·뼈대 해시 8/8 유지, 재로드 오류 0·원본 구형 Rig 참조 경고 4. 실제 게임 확인 대기. 근거: `Saved/Automation/ProfessionAppearanceBuildFinal.txt`, `Saved/Logs/ProfessionAppearanceReload.log`, `Saved/Automation/ProfessionAppearanceReload.json`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-22 전투 승리 골드 보상 | 승리 시 5~15G 선택지 3개와 개인 1회 수령·현재 Human 전원 수령 후 Host Continue 구현. 골드/수령 원자 저장·재개 보존·저장 재시도 재추첨 방지·기존 저장 호환 및 결과 UI 연결. 프로젝트 파일 재생성 10.03초·UHT 포함 Development Editor / Win64 빌드 23.08초 성공. 회귀 테스트 8개 추가·기존 진행/상점·1인/협동 UI 진행 테스트 보정 후 최종 증분 빌드 9.30초 성공. 독립 코드 검토·문서 링크·diff 검사 통과, 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/GoldRewardBuild.log`, `Saved/Logs/GoldRewardFinalBuild.log`; [UI 기준](UI_README.md#13-전투-승리-보상) |
| 2026-09-22 대상 사망 시 근처 적 재선택 | 근접·투사체 스킬의 발동 전 대상 사망을 현재 위치 기준 유효한 생존 적 재선택으로 통일. 유닛 접근·시전·칼날 추적을 다시 시작하며 비용·원래 복귀점·지점 공격·발사 후 투사체 유지. 기존 프로필과 enum 저장값을 보존하고 휩쓸기 제작 명세 갱신. UHT 포함 Development Editor / Win64 빌드 19.84초 성공, 독립 코드 검토·Python 구문·문서 링크·diff 검사 통과. 재선택·후보 없음·시전 회복·프로필 회귀 코드 추가·갱신, 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/DeadTargetRetargetBuild.log`; [기획 기준](GAME_DESIGN.md#8-5-발동과-피격) |
| 2026-09-22 스킬 미선택 턴 넘기기 | 인간의 빈 스킬 명령을 비용 없는 대기로 허용하고 스킬 선택 취소·SAP 이동 단독 계획·schema 3 준비 완료 저장 복구에 연결. 미장착 스킬·이동 자원·소유권 검증 유지. UHT 포함 Development Editor / Win64 빌드 17.47초 성공, 독립 코드 검토·문서 링크·diff 정적 검사 통과. 턴 넘기기·선택 취소·이동·저장 회귀 코드 추가, 게임·PIE·자동화 미실행. 근거: `Saved/Logs/SkipTurnReadyBuild.log`; [기획 기준](GAME_DESIGN.md#8-2-행동-라운드) |
| 2026-09-22 개인 골드 HUD | Gameplay 좌상단에 인벤토리와 같은 개인 확정 잔액 표시. 구매/회복 저장·복제 갱신과 0G 표시, 캐릭터 미확정/대기실 숨김 및 입력 통과 적용. UHT 포함 Development Editor / Win64 빌드 15.64초 성공·오류/경고 0, 코드 검토·문서 링크·diff 정적 검사 통과. 기존 `BP_EnemyUnit` 사용자 변경 보존, Codex의 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/PersonalGoldHUDBuild.log`; [UI 기준](UI_README.md#8-3-개인-골드-상시-표시) |
| 2026-09-22 상점 HP 전체 회복 | 모든 스킬 상점에 본인 생존 Human 전용 전체 회복 1G 추가. 서버 직업 설정의 최대 HP 사용, 만피·사망·타인·AI 구매 차단 및 HP/골드 동시 저장. 기존 schema 1의 상품·잔액 보존, schema 0 제외. 인벤토리·설정 변경과 통합한 UHT 빌드 후 최종 Development Editor / Win64 증분 빌드 5.80초 성공·오류/경고 0. 회귀 코드·독립 검토·문서 링크·diff 검사 완료, 게임·PIE·자동화 미실행. 근거: `Saved/Logs/ShopRecoveryBuild.log`; [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) |
| 2026-09-22 인벤토리·설정 단축키 | Gameplay에서 CommonUI 지속 액션으로 I 인벤토리 열기/닫기·Esc 설정 열기/닫기 연결. 개인 골드·보유 스킬 조회와 로컬 배경 입력 차단, 기존 화면 변경 확인/복구 유지. 프로젝트 재생성 8.04초·UHT 포함 Development Editor / Win64 통합 빌드 20.32초 성공·오류/경고 0, 코드 검토·문서 링크·diff 검사 통과. PIE 기본 Esc 중지 우선권 유지, 실제 화면·게임·자동화 미실행. 근거: `Saved/Logs/GameplayShortcutsBuild.log`; [UI 기준](UI_README.md#8-2-gameplay-인벤토리와-설정-단축키) |
| 2026-09-22 참고 이미지 기반 전투 UI 배치 | 상단 라운드 요약·우측 대상/적·좌하단 파티 카드·하단 중앙 장착 스킬·우하단 이동/준비로 분리. 기존 권한·대상/타일 선택·공통 DPI 유지, 패널별 스크롤·선택 카드 강조와 저장 안내 간격 적용. 상점 변경을 포함한 최종 Development Editor / Win64 빌드 6.05초 성공·오류/경고 0, 독립 코드 검토·문서 링크·diff 검사 통과. UI 에셋 재생성·PIE·실제 화면 확인 미실행. 근거: `Saved/Logs/SkillShopBuildFinal.log`; [UI 기준](UI_README.md#10-2-전투-배치와-카메라) |
| 2026-09-22 비무장 시작·개인 스킬 상점 | 새 Run 아군 비무장 1개·직접 조작 캐릭터별 10G, 모든 상점 기존 공격 4종 각 1G. 본인 Human 구매·중복/잔액/소유권 검사와 골드·스킬 원자 저장, 일반/관리 재개·다음 전투 장착 연결. 전 직업 검 부착·Manny 몽타주 제작, 기존 적/Snapshot/과거 저장 기본값 보존. 프로젝트 파일 재생성·UHT 포함 Development Editor / Win64 통합 빌드와 상품 참조·상점 퇴장 오류 안내 보완 후 최종 증분 빌드 3.92초 성공·오류/경고 0, 회귀 코드는 컴파일만 수행. 제작 commandlet 종료 0·오류/경고 0, 실제 구매·게임·PIE·자동화 미실행. 근거: `Saved/Logs/SkillShopBuildFinal.log`, `Saved/Logs/SkillShopMessageBuildFinal.log`, `Saved/Logs/ShopSkillPresentationAuthoring.log`; [구현 기준](PROJECT_PLAN.md#3-2-상점-인카운터) |
| 2026-09-21 디버그 UI 폴더 분리 | HP 위젯 헤더·소스를 `Source/ProjectA/UI/Debug/`로 이동하고 include·관리 기준 갱신. UHT 포함 Development Editor / Win64 빌드 8.90초 성공, 동작 변경 없음·실제 화면 확인 대기. 근거: `Saved/Logs/DebugUIFolderBuild.log` |
| 2026-09-21 개발용 유닛 HP 표시 | 전투 아군·적 머리 위에 실제 GAS HP/MaxHP 숫자·체력바 표시, 이동·카메라·DPI 추적과 입력 통과. Debug/Development 기본 활성, `projecta.Debug.UnitHP 0`/`1` 전환·Shipping/Test 제외. UHT 포함 Development Editor / Win64 빌드 15.80초 성공·오류/경고 0. 에셋·Config 변경 없음, 실제 화면·플레이·자동화 미실행. 근거: `Saved/Logs/UnitHealthDebugBuild.log`; [UI 기준](UI_README.md#7-1-개발용-유닛-hp-표시) |
| 2026-09-21 검 방향·그립과 칼날 궤적 판정 | Python Rotator의 위치 인자 해석으로 저장된 Yaw 180도를 명시적 Roll 180도로 수정하고 손잡이 Z를 -10cm로 보정. 검만 서버 몽타주 에셋 포즈·칼날 소켓 궤적을 활성 0.23~0.43초에 검사하여 최초 적에게 피해 50/AP 1 적용. 렌더 재생 종료와 판정 누적을 분리하고 기존 GAS·사망 취소·벽 차폐·복귀 보존. 최종 Development Editor / Win64 빌드 4.99초와 별도 재로드의 칼날 표본 123개·리타깃 48개·이전 참조 73개 통과. 회귀 3개는 컴파일만 수행, 실제 플레이·자동화 미실행. 근거: `Saved/Logs/WeaponBladeTraceBuildFinal.log`, `Saved/Logs/WeaponBladeTraceReload.log`; [구현 기준](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙) |
| 2026-09-21 전투 상단 현황 배경 | 상단 라운드·명단에 어두운 반투명 배경·둥근 테두리·여백 적용. 높이는 내용에 맞추고 마우스 입력 통과 유지. Development Editor / Win64 컴파일·정적 검사 통과, 실제 화면 확인 대기. 근거: `Saved/Logs/CombatRosterPanelBuild.log`; [UI 기준](UI_README.md#10-2-전투-배치와-카메라) |
| 2026-09-21 보행·공격 리타깃의 과도한 골반 이동 수정 | Retargeter 4개의 중복 연산을 각 11→6개로 정리하고 시퀀스 48개를 기존 경로에 재작성, 원본 Root Motion 설정·참조 보존. 전사 전방 보행의 골반 이동 약 454→8cm. Development Editor / Win64 빌드 12.18초 성공, 별도 재로드 48/48 포즈·길이·골반 이동·Kwang 세그먼트/타이밍·이전 참조 73개 통과. 오류 0·기존 경로 조회 경고 6 후 대상 해석 확인. 사용자 보고의 공격 전 순간이동·흔들림은 수정 후 실제 플레이 확인 대기. 근거: `Saved/Logs/RetargetRootMotionBuild.log`, `Saved/Logs/RetargetRootMotionKwangReload.log`; [구현 기준](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙) |
| 2026-09-21 검 접근 후퇴 수정과 Kwang 공격 연결 | 유닛 접근 범위 도달 시 즉시 시전하여 정확한 간격을 맞추는 후퇴 방지. 전사/적의 검 몽타주를 Kwang 공격·복귀 2개 세그먼트로 교체, 중복 복귀 0.2초 제외·총 1.933333초·발동 0.23초·블렌드 0.08/0.12초. 피해 50/AP 1·기존 장착·Boss Swing 사본 보존. 최종 Development Editor / Win64 빌드와 별도 재로드에서 세그먼트·타이밍·뼈대·소켓·5/1 장착·이전 참조 73개 확인, 오류 0·기존 경로 조회 경고 6 후 대상 해석 확인. 회귀 코드 컴파일만 수행, 수정 후 실제 플레이·자동화 미실행. 근거: `Saved/Logs/KwangSwordBuildFinal.log`, `Saved/Logs/KwangSwordReload.log`, `Saved/Automation/KwangSwordReload.json`; [구현 기준](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙) |
| 2026-09-21 Paragon Manny 애니메이션 가져오기 | 원본 32개 캐릭터 폴더 구조대로 AnimSequence 5,385개 작성·저장, Manny 뼈대·프리뷰 메시 작업 사본 연결. 샘플링률 자동 판정·종료 프레임 경계 맞춤, Additive/MSA 자동 설정 없음·기존 전투 스킬 연결 유지. Development Editor / Win64 빌드·별도 재로드 5,385개 통과: 당시 양수 길이·161개 본 트랙·원본 FBX·뼈대·프리뷰 확인, 오류/경고 0. 후속 최신 저장본도 5,385개 재검사 통과·오류/경고 0이며 단일 프레임 3개의 56/57/58개 본 트랙을 보존. Codex의 Animation Editor 실제 재생·PIE 미실행. 근거: `Saved/Logs/ParagonImportBuild.log`, `Saved/Logs/ParagonReload.log`, `Saved/Logs/ParagonReloadFinal.log`, `Saved/Automation/ParagonAnimationsReload.json`; [구현 기준](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-21 작업 사본의 원본 폴더 구조 보존 | 전사·적·검 사본 73개를 원본 팩/하위 폴더 구조로 재배치하고 이전 참조·Redirector 정리 완료. 외부 팩 원본 변경 없음. 최종 Editor / Win64 빌드와 별도 재로드에서 이전 경로 73개·전사/적 Swing 세그먼트·뼈대·소켓·5/1 장착 확인. 재로드 오류 0·구객체 조회 경고 6 후 새 대상 해석 확인, 이전 통합 폴더의 사본/Redirector 잔존 0. 실제 플레이 미실행. 근거: `Saved/Logs/WarriorFolderBuild.log`, `WarriorFolderReload.log`, `WarriorFolderRedirectorCleanup.log`, `Saved/Automation/WarriorContentReload.json`; [구현 기준](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙) |
| 2026-09-21 전사·검 공격과 휩쓸기 정리 | GKnight 전사·Weapon_Pack 검·BossyEnemy 공격의 작업 사본과 IK 리타깃 연결. 전사 기존 4스킬+검 공격, 기본 적 검 1개·검 공격만 장착, Snapshot 저장 스킬 규칙 유지. 기본공격 표시명 `비무장 공격`, 휩쓸기 `BPDA_SweepingStrike`로 변경·이전 참조 리디렉션. 유닛별 몽타주 대체·보행/DefaultSlot 유지, 맞지 않는 Manny Foot IK 제거. 검 발동 2.15초·원본 길이 5.8667초 유지. 최종 Development Editor / Win64 빌드와 별도 에셋 재로드 통과, 재로드 오류 0·구경로 fallback 조회 경고 2 후 새 에셋 확인. 외부 팩 원본 수정·실제 플레이 없음. 근거: `Saved/Logs/WarriorFinalBuild.log`, `Saved/Automation/WarriorContentReload.json`; [구현 기준](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙) |
| 2026-09-21 전투 확정 규칙과 Ready 복구 | 이동 복귀/예약 충돌·자리 교환 금지·실패 원점 복원, 서버 즉시 피해/사망·미발동 공격 취소, 수정 소유자만 Ready 해제, 양 팀 전멸 패배·Run 종료, Ready 종료 AP/SAP 차감·미환불 적용. 엄호 제거·상태이상 후순위·비근접 추가 속도 보류. schema 3에 비용 차감 전 계획·양 팀 상태를 저장한 뒤 준비를 확정하고 저장 실패 시 차단·재시도. 일반 v5·관리 v4·LegacyOffline v6 복구, v1 비전투·기존 소유권 유지. Development Editor / Win64 빌드 통과, 회귀 코드 추가·최신 PIE/게임/자동화 미실행. 근거: `Saved/Logs/ReadyCheckpointBuildFinal.log`, `Saved/Logs/WarriorFinalBuild.log`; [저장 계약](MULTIPLAYER.md#6-준비-완료-경계와-파일-저장) |
| 2026-09-18 테스트 적 4마리 배치 | `DA_DefaultEncounter`의 기존 `BP_EnemyUnit`을 4개로 구성하고 Gameplay Arena를 앞열 `(1,2)`, `(2,2)`·뒷열 `(0,3)`, `(3,3)`으로 저장. 두 Combat 노드에 공통 적용하며 적 능력치·스킬은 유지. 파일 잠금 실패 후 에디터 종료 상태에서 저장 재시도 성공, 별도 프로세스 재로드와 Gameplay 에셋 연결 검사 통과. 생성/검사 스크립트의 수·좌표 및 폐기된 HUD 검사를 현재 native 계획 화면 기준으로 갱신. Python 구문·문서 링크·diff 검사 통과, C++·Config 변경 없음·빌드 불필요·PIE 미실행. 근거: `Saved/Automation/TestEnemiesReload.json`, `GameplayAssetValidation.json`, `TestEnemiesStatic.json`; [작동 확인](PROJECT_PLAN.md#6-gameplay-에셋과-배치) |
| 2026-09-18 휩쓸기 근접 대상·양옆 | `TargetAndSides` 범위와 근접 접근·복귀 연결. 실제 캡슐 횡방향 스윕·벽 차폐·중복 타격 방지, 가장자리 인접 칸 제한. DA 피해 10·AP 1 보존, 기본공격 몽타주 연결. UHT 포함 Development Editor / Win64 빌드 20.61초 성공, 명령줄 DA 저장·별도 프로세스 재로드 및 정적 검사 통과. 범위·접근·복귀·차폐·목표 사망 회귀 추가, 최신 PIE/자동화 실행은 미실행. C++ 파일 생성 없음·Config 변경 없음·VS 미실행·제작 프로세스 종료. 근거: `Saved/Automation/SweepingStrikeBuild.log`, `SweepingStrikeReload.json`; [검증 제한](#9-15-2026-10-01-todo-재검증과-구현-이관) |
| 2026-09-18 TODO 테스트 위임 실행 | 최신 결과 기준 자동화 89개 통과. 싱글 두 전투/상점/Run 완료, 2인 → 4인 Listen Server PIE의 소유자별 요청·HP/계획 복제·원격 AP/몽타주/SAP, 별도 게임 프로세스 메뉴 이어하기·항복·해상도 확인/취소/15초 복원 검증. 네 스킬은 화면 전환 후 Slate 클릭 한 번으로 대상 선택, 버튼 delegate로 실행하여 AP·피해·투사체·제자리 원거리 시전 확인. idle 반복·몽타주 사망/취소/유한 대기·Snapshot 소수 민첩·이전 DA 경로 저장 호환 포함. 테스트 fixture의 HP 기록 시점·LocalPlayer Outer·경고 검사 보완, 파괴 중 유닛의 몽타주 RPC 차단, 역참조 없는 테스트 GameMode Redirector를 엔진 FixupRedirects로 삭제. Editor / Win64 빌드 5.64초·정적 검사 통과, 프로젝트 재생성 및 vcxproj/filters 확인. 기존 저장 44개 해시 보존. Config 변경·VS 실행 없음, 시험 프로세스 종료. 근거: `Saved/Automation/TodoLatestResults.json`, `TodoCombatFinal`, `TodoRangedStationary`, `TodoExtended`, `TodoOptions`, `TodoRestartContinue`, `TodoSurrenderUI`; [재실행](../Source/ProjectAEditor/Scripts/README.md). 창은 데스크톱 크기로 제한되며 전체 해상도/모니터 조합·조작감·Steam/다중 PC/지연·미작성 Cue는 검증 범위 밖. 이전 실패 실행은 수정 전 이력 |
| 2026-09-18 장착 스킬 4개 표시·실행 수정 | `BP_PlayerUnit`에 실제 DA 4개 장착, `EnemyTile·AroundTarget` 제한 변환 허용, CommonUI 기본 입력 데이터 연결. 사용자 기본공격 수정과 네 DA 원본 보존. Development Editor / Win64 최종 빌드 8.02초·정적 검사 통과. 관련 회귀 6개와 `SavedSkillLoadout`의 실제 새 게임 PIE 4회 통과: 네 버튼 표시·선택·서버 계획·AP 1, 적 HP 150→100/100/0/140, 원거리 투사체 관찰. AOE는 치사 결과 검증이며 정확한 초과 피해량 검증은 아님. 최종 PIE 오류 0·종료 시 RecastNavMesh 경고 4. 기존 저장 44개 해시 보존·전용 시험 저장 정리 확인. 버튼/대상 delegate 검사로 실제 마우스 hit-test·애니메이션 육안·협동 확인은 별도. 휩쓸기 몽타주 미지정. 근거: `Saved/Automation/FourSkillRuntime`, `FourSkillFinalRuntime`, `FourSkillStatic.json`; [실행 명령](../Source/ProjectAEditor/Scripts/README.md). C++ 파일 추가 없음·VS 미실행·시험 에디터 종료 |
| 2026-09-18 단일 대상 원거리 공격 제작 | 기본 공격 DA·GA를 별도 원거리 에셋으로 복제하여 기존 Single·EnemyUnit·제자리 투사체 경로 연결. 피해 50·AP 1·몽타주·태그 설정과 원본 보존. Blueprint 컴파일·저장 및 독립 재로드 확인 통과. C++ 변경·PIE·게임·자동화 실행 없음. 장착 대상 선택과 사용자 작동 확인 대기 |
| 2026-09-17 일반 SAP 이동 감속 | 사용자 확인에서 근접 접근·복귀 속도는 적절. 일반 SAP 이동만 민첩·MaxWalkSpeed와 무관한 350cm/s 고정값으로 조정하고 기존 회귀 3개 보완. Development Editor / Win64 빌드 6.41초 성공. 에디터 정상 종료 후 재실행·자동화 없음, 변경한 SAP 이동은 사용자 확인 대기 |
| 2026-09-17 근접 이동 감속·민첩 연동 | 접근·복귀에 라운드 민첩 스냅샷을 적용해 기본 아군 350·적 262.5cm/s로 초기 조정. SAP·비근접·시전·투사체 속도 유지. 회귀 1개 추가·기존 1개 보완, UHT 포함 Development Editor / Win64 빌드 24.61초 성공·정적 검사 통과. 에디터·자동화 미실행, 사용자 작동 확인 대기 |
| 2026-09-16 사용자 플레이 확인·문서 간소화 | 사용자가 지금까지 플레이한 범위에서 이상이 없다고 보고. 사용자 삭제한 테스트 보고서를 반영하고 추가 확인은 TODO에 짧게 기록하도록 변경. 세부 협동·예외·자동화 통과로 확대하지 않음. 게임 코드 변경 없음 |
| 2026-09-16 이동 보행 입력 복구 | 기존 AnimBP의 속도+가속도 조건에 누락된 라운드 이동 가속도를 연결. CharacterMovement 자식으로 SAP 이동·AP 접근/복귀 및 정지/시전/중단/사망 입력 동기화, 클라이언트 MOVE_None 신호 복원·기존 보간 유지. 회귀 1건 추가, 프로젝트 재생성 23.21초·UHT 포함 Editor 빌드 16.55초 성공·컴파일 오류/경고 0. 실제 재생·자동화는 미실행, 에셋/Config 변경·에디터/VS 실행 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#27-4-이동-애니메이션-입력-복구) |
| 2026-09-16 단일 클릭·SAP 이동 예약 | 계획 화면의 임시 마우스 캡처로 최초 눌림 전달. 이동 예약/변경/취소와 잠금 시 합산 AP/SAP 1회 차감, 전체 SAP 이동 후 AP 시계 시작·새 자리 복귀 연결. 이동 실패 원점 복원·안내 보존과 사망자 AP 취소. 회귀 2건 갱신·3건 추가, UHT 포함 Editor 빌드 18.90초 성공·컴파일 오류/경고 0. 코드·문서 정적 검사 완료, 실제 플레이·자동화 미실행. Config/에셋 변경·에디터/VS 실행 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#27-단일-클릭과-sap-이동-예약) |
| 2026-09-16 전장 클릭과 SUP 이동 복구 | 실제 장착 DA만 표시하도록 공통 시험 6행동·무장착 대체 공격·Encounter 자동 추가 스킬 부여 제거. 적 클릭 → 하단 스킬 → 준비 완료, SUP 1 독립 이동·아군 빈칸 BFS·이동 후 새 복귀점 연결. 회귀 3건 추가·fixture 이관. UHT 통과 후 생존 함수명 오류 수정, 최종 Editor 빌드 5.13초 성공·오류/경고 0. 실제 플레이·자동화 미실행, 에디터/VS 실행 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#26-전장-대상-선택과-sup-이동-복구) |
| 2026-09-16 싱글 직접 조작 선택 | 사용자 확정에 따라 캐릭터 생성에서 직접 조작할 1명 선택·나머지 동료 ServerAI, 선택 저장/복원·다음 전투 유지·인간 사망 후 AI 진행 연결. 원래 소유권·협동/관리 정책 유지, 일반 이전 저장과 LegacyOffline의 호환 구분. UHT 포함 Editor 빌드 18.32초 성공·컴파일 오류/경고 0. 독립 코드 검토 통과, 실제 플레이·자동화 미실행. 기존 에디터 정상 종료 후 재실행 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#25-싱글플레이-직접-조작-캐릭터-선택) |
| 2026-09-16 민첩 기반 전투 속도 반영 | 독립 속도 20을 제거하고 현재 GAS 민첩을 float 전투 속도로 사용. 기본 아군 10·일반 적 세 능력치 5/속도 5, 기존 0.1초 차 공식과 Planning 시각표 고정 유지. Snapshot 민첩·일반 적 HP 150/AP 2 보존, 이동/시전/투사체 보정 없음. 회귀 추가·보강과 UHT 포함 Editor 컴파일 21.62초 성공·오류/경고 0. 클래스 기본값 5개·직업 4개의 읽기 전용 검사 통과. 실제 플레이·자동화 미실행, 사용자 요청 없는 에디터 재실행 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#24-민첩-기반-전투-속도와-일반-적-능력치) |
| 2026-09-16 시작 모드 선택·싱글 여정 항복 구현 | 게임 시작을 싱글 캐릭터 생성/멀티 LAN 개발방으로 분기하고 첫 화면의 별도 협동 버튼 제거. 이어하기 옆 104×40 항복과 취소 기본 포커스 확인창, 현재 싱글 저장만 독점 접근으로 확인 후 삭제·변경/실패 보존 구현. 프로젝트 재생성 22.94초·UHT 통과, 최종 Editor 빌드 4.90초 성공. WBP 2개 컴파일·저장·독립 재로드 오류/경고 0. 실제 플레이·자동화 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#23-시작-모드-선택과-싱글-여정-항복) |
| 2026-09-16 캐릭터 생성 프리뷰 Idle 연결 | 프리뷰 메시의 AnimClass/AnimToPlay 누락 수정. 동일 Skeleton의 기존 MM_Idle을 SingleNode 자동·반복 재생으로 BP 기본값과 생성 스크립트에 연결. BP 컴파일·저장·독립 재로드 성공, 두 검사 로그 오류/경고 0. 기존 메시·배치·NoCollision·직업 맵 보존. 에디터 시작 11.36초·응답 확인 후 열어 둠, 기존 시작 오류/경고 잔존. C++·Config·맵·전투 코드 무변경, 실제 화면·PIE·자동화 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#22-4-캐릭터-생성-프리뷰-idle) |
| 2026-09-16 네 직업과 공통 초기 능력치 | 전사·마법사·궁수·도적의 C++ 베이스/자식 정의와 HP 100·힘/민첩/지능 각 10 연결. Party/Snapshot/MainMenu 맵 4개·WBP 라벨 16개 반영 및 독립 재로드 확인. 구직업 저장은 명시 거절·원본 보존, 기존 저장 26개 해시 유지. 프로젝트 재생성 23.44초·UHT 포함 일반 Editor DLL 빌드 25.10초·컴파일 오류/경고 0. 에디터 시작 11.54초·응답 확인 후 열어 둠, 기존 시작 오류/경고 잔존. 실제 플레이·자동화·Continue·협동 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#22-네-직업과-기본-능력치) |
| 2026-09-16 전체 시전 대기와 DA 폴더 정리 | 발동 1회·Windup·충돌 판정을 유지하고 서버 몽타주 인스턴스의 블렌드 아웃 종료 후 복귀. 실제 프레임 시간 기반 대기·최대 60초 반복/종료 누락 정리와 복귀 방향 복원 연결. DA 7개를 유형별 5개 폴더로 이동하며 이름·값·ID와 미작성 공격 보존, 이후 직업 반영은 위 항목으로 구분. 새 참조·구경로 7개 해석·루트 잔존 DA/Redirector 0개 확인. 최종 Editor 빌드 25.10초 성공·컴파일 오류/경고 0. 실제 전투·자동화·저장 Continue 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#21-5-전체-시전-대기와-da-폴더-정리) |
| 2026-09-16 공격 후 복귀 방향 복원 | 복귀 이동 뒤 방향이 남던 문제 수정. 행동 전 회전을 계획 잠금 시 저장하고 정상 복귀·시간 초과·제자리 완료에서 회전·정지 속도 복원, 기존 이동 복제로 전달. 사망·잔류 성공 방향 보존. 기존 이동 회귀 확장·경계 회귀 1건 추가, UHT 포함 Editor 컴파일 20.33초·일반 DLL 최종 빌드 2.20초 성공·오류/경고 0. 독립 검토·문서/프로젝트 항목 정적 검사 통과. 실제 플레이·자동화 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#21-4-공격-후-복귀-방향-복원) |
| 2026-09-16 DA 시전 몽타주 연결 | 사용자 전투 검수의 애니메이션 미재생 원인인 라운드 몽타주 연결 누락 수정. 명시 CastMontage 우선·공격 Ability fallback, Casting 1회 재생·표현 multicast·취소 정리. 서버 피해/이동 시점 유지, root motion·알림 중복 타격 차단. UHT 포함 Editor 빌드 25.42초·컴파일 오류/경고 0, 독립 코드 검토·문서 정적 검사·VS 프로젝트 항목 확인 통과. 기존 회귀 2건 확장·자동화 미실행. 에디터 재시작 13.10초 확인, 수정 후 실제 재생은 사용자 검증 대기. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#21-da-시전-몽타주-연결) |
| 2026-09-16 그래픽 드라이버 복구 | 명시적 사용자 요청으로 NVIDIA 616.92에서 보관된 596.21 INF를 Windows API로 복구. 최초 직후 WMI 검사 실패와 후속 PnP·레지스트리·활성 드라이버 확인을 구분. 기본 D3D12 SM6 에디터 시작 30.00초 성공·사용자 전투 진입, 이후 몽타주 빌드 후 재시작 13.10초 성공. 시작 오류·경고는 잔존하며 전체 전투 검증과 구분. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#20-그래픽-드라이버-복구와-에디터-시작) |
| 2026-09-16 싱글 Run 통합 확인 준비 | TODO 1·2절만 점검. 결과 Continue 저장 재시도 성공 후 오류 잔류와 스폰 불가능한 직업 클래스의 사전 검사 누락 수정. 기존 준비 취소·입력/재진입 차단·fallback·Cue 구성 유지. 회귀 1건 추가·직업 회귀 확장, UHT 포함 Development Editor / Win64 빌드 24.43초 성공·컴파일 오류/경고 0. 독립 코드 검토·정적 검사 통과, Config·에셋 변경 없음. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#1-직접-플레이-확인)·[당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#18-3-개발-확인과-제한). 작동·자동화·에디터 데이터 검사는 미실행 |
| 2026-09-16 기존 미커밋 변경 통합 규칙 | 커밋 시 작업 관련 여부와 작성자에 관계없이 저장소의 기존 추가·수정·삭제를 함께 포함하도록 AGENTS·README 갱신. 이미 작업 폴더에서 삭제된 BP_PartyPlayerController·TestGameModebase·TestMap의 Git 삭제 이력도 반영. 검증 범위와 완료 후 미커밋 상태 확인 규칙 추가. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#5-엔진-콘텐츠-경로-통일) |
| 2026-09-16 TODO 체크형 제안과 작업 요청 규칙 | 전투 규칙 4개에 번호·체크형 선택지 8개·권장안·영향·수용 조건 추가. 선택과 완료 상태를 분리하고 TODO 단일 작업 요청 기준을 AGENTS·README에 반영. 게임 규칙 선택·코드 변경 없음. `.md`의 Notion 연결은 확인했으며 원본 저장·체크 반영은 미확인. 검증 기록은 [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#문서-정적-검증) |
| 2026-09-16 서버 공격 충돌 판정 | 근접 전방 스윕의 첫 적 캡슐, 지점 3D 겹침·벽 차폐, 투사체 벽 충돌·전투 참가자 제한으로 통합. MeleeRadius 프로필과 회귀 5개 추가, 명중 확률·성공 슬롯 미사용을 기획에 반영. 최초 UBA 임시 폴더 오류는 `-NoUBA`로 우회했으며 최종 Development Editor / Win64 빌드 14.14초 성공·컴파일 오류/경고 0. 코드·문서·diff 정적 검사 통과, 작동·자동화 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#19-서버-공격-충돌-판정) |
| 2026-09-16 TODO 2절 후속 오류 수정 | 준비 취소 저장 실패 뒤 화면 갱신의 입력 재활성화 차단, 로컬·복제 표시의 준비/저장 오류 동시 유지, 동기 Map 통지 중 새 노드·재시도 재진입 방지. Controller 경로 회귀와 Cue 설정 3단계 진단 보강. Editor 빌드 13.57초·테스트 fixture 보완 후 최종 4.46초 성공, 컴파일 오류·경고 0. 독립 리뷰 발견 1건 수정, 작동·자동화 미실행. Cue 최초 원인은 미확정이며 이전 NavMesh/Editor 로그는 엔진 정리·시작 테스트·설치 리소스로 분류. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#8-hudgameplaycue-경고-수정)·[당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#18-준비-취소-복구와-파티-데이터-사전-검사) |
| 2026-09-16 준비 취소 복구·데이터 사전 검사 | 일반·관리 Run의 준비 취소 저장 실패 복구와 기존 버튼 재시도, 원래 오류 보존·중복 시작 차단. 파티 에셋 Data Validation을 런타임 직업 검사와 공유. Cue 검색은 UE 5.7 Globals 호환 설정으로 보완하며 최초 빈값 원인은 미확정. 새 회귀 3건·관리 회귀 1건 확장, 프로젝트 재생성 23.17초·Editor 컴파일 14.72초 성공·컴파일 오류/경고 0. 코드 리뷰·문서/프로젝트 파일/diff 정적 검사 통과, 작동·에셋 검증 미실행. 자율 진행 순서와 통합 사용자 확인을 TODO에 기록. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#18-준비-취소-복구와-파티-데이터-사전-검사) |
| 2026-09-16 TODO 가독성 정리 | 미완료 작업을 사용자 확인·오류 수정·게임 규칙 결정·온라인 연동·디자인 후 구현의 5개 절로 정리. 각 항목 앞에 할 일을 명시하고 완료표·완료 체크·과거 검증 설명 제거. 기존 이력 문서의 기록과 미완료 조건 보존, 관련 링크·절 번호 갱신. 문서 링크·앵커·번호·diff 정적 검사 통과. 결과는 TEST_REPORT 17-4절에 기록하며 코드·작동 상태는 변경하지 않음 |
| 2026-09-16 계획 입력 검사·장착 Tile 공격 AI | UI와 서버의 공통 명령 검사, 합법 대상 표시·대상/다른 아군 사망 후 스킬 초안 유지·자원/타일 적용 검사·본인 생존 유닛 계획의 준비 조건 보완. 기존 AI 선택 순서·인간 초안 전 고정을 유지하며 장착된 복귀형 Tile 공격 연결. 디자인 의존 항목은 당시 TODO 5절에 보류·재개 조건을 기록했다. [현행 기획 기준](GAME_DESIGN.md#6-구현-원칙과-다음-콘텐츠) Editor 빌드 17.52초·최종 초안 보존 보강 후 증분 빌드 4.61초 성공, 두 빌드 컴파일 오류·경고 0. 최종 코드·문서 정적 검사 통과, 자동화·사용자 작동 검증 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#17-계획-입력-검사와-장착-tile-공격-ai) |
| 2026-09-15 공통 DPI·전투 화면 배치 | 1920×1080 기준 공통 ScaleToFit·ApplicationScale 1 설정, 화면별 추가 축소 제거. 전투 바깥 가로 여백 제거·세로 12 유지, 활성 카메라 고정 비율 해제·세로 시야 유지와 생성 기본값 정합. 기존 에셋·WBP 무변경. Editor 빌드 14.79초 성공·컴파일 오류/경고 0, 8개 화면 배율·카메라/Python·문서 정적 검사 통과. 작동 검증 8항목 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#16-공통-dpi와-전투-화면-배치) |
| 2026-09-15 FTK2 설치본 기획 보충 | Build 24247341의 평문 JSON으로 무기 판정·스킬 연결·장비 수치·소비 효과·회복/방어 파이프 4종·상점·직업 근거 보충. Light Gloves·Hammer·Fire Staff·Bone Bow 등 공개 참조와의 차이 정정. 시간차 전투 유지·집중 자원과 관련 효과 제외 확정, 카탈로그 42개로 정리. 원본 해시 13개·문서 5개 정적 검사 통과. 사용자 대조 절차는 [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#11-아이템-클론-기획-검토). 코드·에셋 구현 및 게임 실행 없음 |
| 2026-09-15 DemonicUI 공통 테마 | native 공통 테마와 밝은 글자 콤보박스 추가. 메뉴·설정/확인·캐릭터 생성/상세·협동·Run·상점·결과·라운드 계획·저장/협동 안내에 적용. 중앙 패널 축소·메뉴/관리 이어가기 동시 배치·기존 Designer 지도/결과 보완. 원본 에셋·WBP·JSON 무변경. 프로젝트 재생성 18.14초·최종 Editor 빌드 9.03초 성공, 컴파일 오류·경고 0. 텍스처 참조 11개·문서 정적 검사 통과. 작동 검증 14항목 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#15-demonicui-공통-테마) |
| 2026-09-15 시작 메뉴 설정 | native Options에 해상도·3개 화면 모드·5단계 품질·VSync와 사용자 품질 보존 추가. 15초 확인·전체 복원·정상 창 종료 전 복구·확인 중 전체 화면 단축키 차단·CommonUI 입력 연결. 초안 취소·사용자 Landscape/렌더 배율 보존·명시 프리셋 선택 회귀 코드 보강. 새 에셋·Config 변경 없음. Editor 기능 빌드 18.45초·최종 회귀 코드 포함 증분 빌드 3.88초, 두 빌드 컴파일 오류·경고 0. 코드·문서 정적 검사 통과, 작동 검증 17항목 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#14-시작-메뉴-화면그래픽-설정) |
| 2026-09-14 기본 시간차 전투 전환 | 기존 순차 턴·연속 AI·End Turn 실행 제거, 기존 Gameplay에 CommonUI 계획·0.1초 속도차 실행·실제 접근/복귀·서버 피격·발사체 존속 연결. 장착 스킬 초기 변환·명시 프로필과 공통 시험 행동 제공. 비전투 Run/상점/소유권/lease 유지, 이전 Combat 저장은 무변경 거절. 최종 Editor 컴파일 23.23초·오류/경고 0, 프로젝트 파일 재생성·링크/앵커·diff 검사 통과. 순차 PIE fixture 3개 파일 삭제, 메뉴 검증 분리 유지. 작동 검증 미실행. 임시 정책 확인 대기. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#12-시간차-자동-전투-기획-검토) |
| 2026-09-14 시간차 전투 기획 | [GAME_DESIGN 8절](GAME_DESIGN.md#8-라운드-계획과-시간차-자동-전투)에 라운드별 계획·속도차 0.1초·실제 접근/복귀·유닛 비충돌·서버 실시간 피격·잔여 공격 정리·적 계획 선고정을 통합. 후속 계약·검증 절차·현재 순차 구현과의 경계 기록. 문서화 완료이며 전투 구현·작동 검증은 미실행 |
| 2026-09-13 아이템 클론 기획 | FTK1·FTK2 참조 항목 44개, 작품별 차이·출처·상세 카드·데이터 변환 필드·후속 결정·사용자 검토 절차 작성. 기존 GAME_DESIGN에 통합. 시스템 구현·원작 직접 실행 검증은 포함하지 않음 |
| e90cd21 | 제작 경로를 Content/User_JeHoon으로 통일. T12 위젯 3종 이동·Git 추적. 패키지 로드 3개, 이전 경로 에셋/Redirector 0개, Editor 빌드 성공. 수동 확인 대기 |
| 640a614 | e90cd21 기준 2인 전투 자동화: 경고 동반 성공 1건, 오류 0·경고 4, 14.998초 |
| 8c29017 | 2인 경고 대응 분석·보고서 직접 요약 규칙. 게임 코드 변경·재실행 없음 |
| 2026-09-11 후속 | 8c29017 기준 4인 전투 자동화: 경고 동반 성공 1건, 오류 0·경고 6, 31.563초. Markdown 문체·중복 정리 |
| UI 문서 이전 | Source의 UI_README.txt를 Docs/UI_README.md로 이전·축약. 생성 옵션·JSON 필드·바인딩·프리뷰 규칙 유지, SizeBox·크기 필드·작업 경로 제한 반영 |
| HUD·Cue 경고 수정 | MainMenu·Gameplay의 빈 HUD 생성 요청 생략, UE DeveloperSettings의 Cue 검색 경로 지정. 에셋 메타데이터 조사·Editor 빌드·정적 검사 완료, 수정 후 작동 검증 대기 |
| T14-7 승계 검증 | ff22940 기준 3→2인·4→3인 승계·단독 메뉴 AI 재개 PIE 3건 경고 동반 성공, 관리 계약 4건 성공. 테스트 오류 0·경고 11. 빈 HUD 경고 미발생, Cue fallback 재발과 빈 실행 설정 배열 확인. 게임 코드·제작 에셋 변경 없음 |
| 개발용 협동 UI | Non-Shipping 메뉴의 2~4인 방 생성·IPv4 참가·준비·Host 시작, 접속 순서 배정·기존 전투 연결·별도 저장·이탈/추가 접속 차단 구현. Editor 컴파일·정적 검사 완료, 작동 검증 대기. 영구 에셋 생성·변경 없음 |
| 상점 인카운터 | 첫 승리 뒤 상점1·상점2·상점3 중 선택·진입·퇴장과 다음 전투 연결. DataAsset 기반 고정 목록·Run 저장·Host 진행/Client 표시·기존 저장 경로 보존. Editor 컴파일 완료, 작동 검증 대기. 상품·재화·추첨·영구 에셋 생성 없음 |

아래 2·4인/승계 성공은 이전 순차 전투 코드의 이력이다. 최신 기본 라운드 전투·저장 계약의 성공 근거로 사용하지 않는다. 당시 2·4인 검증은 기존 사용자 삭제 에셋 3건을 보존한 작업 폴더에서 수행했다. 조작권·행동 RPC·상태·두 전투 완료를 확인했으며 Steam/P2P·수동 조작·승계·복구 검증은 포함하지 않는다. 상세는 [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md)에 기록한다.

## 1. 초기 리뷰와 Vertical Slice

- 2026-09-07: 전투·GAS·AI·메뉴·UI 생성 경로를 정적으로 검토했다. P1 2건과 P2 4건을 기록했으며 당시 빌드·PIE·패키징은 실행하지 않았다.
- 2026-09-08: 공통 행동 완료 규약과 AI 실패 복구, 파티 데이터, Gameplay/Encounter/Result 루프를 구현했다. 초기 묶음은 `1e2f4b6`이다.
- 기본 흐름은 MainMenu → 캐릭터 생성 → Gameplay → Run Map → Combat → Result → Continue → 다음 전투다. Defeat는 진행을 종료한다.
- Gameplay는 TestMap을 바탕으로 생성했으며 Arena·Grid·카메라·파티/적 설정을 분리했다. 반복 전투마다 맵을 다시 여는 구조는 도입하지 않았다.
- 새 Gameplay WBP 4종을 실제 생성·컴파일·저장하고 별도 프로세스에서 에셋 설정 29항목을 확인했다. 원본 TestMap/MainMenu/WorldMap 보존도 비교했다.
- 최초 Gameplay NavMesh가 비어 이동하지 못한 문제는 해당 맵의 내비게이션 데이터를 빌드·저장해 해결했다.
- 사용자 플레이에서 발견한 클릭 불능은 CommonUI 입력 모드와 메뉴 travel의 잔여 입력 잠금 문제였다. HUD 입력 설정·뷰포트 잠금·최초 포커스를 보정했다.
- 직접 행동 호출만 하던 검증을 Slate 합성 마우스의 Move/Skill 버튼 → 타일 클릭으로 보강했다. 실제 이동·점유·GAS 피해와 전투 화면 전환을 확인했다.
- 최종 Editor 빌드와 자동화 8건이 통과했다. 자연 공격 반복으로 Victory, Continue 후 같은 월드의 다음 전투, 치명적 GAS 피해로 유도한 Defeat를 구분해 검증했다.
- 당시 자동으로 Visual Studio를 열었던 절차는 이후 사용자 요청과 `94e0a19`로 중단했다. 완료 안내를 위해 IDE를 열지 않는다.

## 2. T01~T13 완료 순서

T01·T02·T08의 기반은 초기 Vertical Slice에서 함께 구현했다. 표는 작업 번호순이며 별도 커밋이 없는 항목을 새 커밋으로 추정하지 않는다.

| 작업 | 완료 내용 | 핵심 검증·커밋 |
|---|---|---|
| 2026-10-03 근접 공격 표시명 변경 | 기존 검 공격의 이름을 `근접 공격`으로 통일하고 생성 명세·알림·문서·몬스터 목록을 갱신했다. 기존 상점 진열도 에셋의 현재 이름을 우선하며 저장 데이터는 변경하지 않는다. 에셋 경로·`SwordAttack` ID·GAS·공격 모션·칼날 판정과 다른 스킬 속성 12개를 보존했다. | UE 5.8.3 Development Editor / Win64 컴파일·링크 17.20초 성공, 에셋 저장·독립 읽기 전용 재로드 모두 종료 0·오류/경고 0, 대상 외 추적 Content 477파일 SHA 보존. Python·문서 링크·CSV·diff 정적 검사 통과. 게임·PIE·자동화 테스트 미실행. [후속 검수 이력](#9-17-2026-10-04-ue-58-todo-실행-검수) |
| T01 / R01 | 이동 여부와 독립된 행동 상태·GAS 종료 연결·성공/실패/취소 완료 1회. 제자리 스킬의 불필요한 복귀 제거 | 몽타주 유/무·동기 완료·활성화 거절·busy/중복 입력 검사. 초기 `1e2f4b6` |
| T02 / R02 | 적 AI에 실패·취소 결과 전달, 다음 틱 재판단/안전한 종료. 원타일·시작 위치 복구와 오래된 콜백 차단 | 접근/복귀 phase 주입·no-nav 거절·1회 완료. 소비한 AP는 비환불. 초기 `1e2f4b6` |
| T03 / R03 | DataAsset 비용을 HUD·입력·AI·GAS의 단일 AP 기준으로 통합 | 비용 1/2/3/0/-1, 과거 Ability 비용 무시, 실패 시 자원 보존. 전체 11건. `e5dcb76` |
| T04 / R04 | 플레이어·적의 타겟 규칙·전열 보호 공통화, 실행 직전 대상 재검증 | 576개 조건, 점유자/진영/생존 변경·재시도. 전체 14건. `5517601` |
| T05 / R05 | 플레이어 입력과 AI 내부 턴 종료 분리, 타일 명령의 Controller 집중 | 적 턴 입력 차단·AI 독립 종료·busy/사망/종료 문맥 거절. 전체 16건. `db987e2` |
| T06 / R06 | 직접 효과와 스폰 효과의 범위 계산 통합, 미지원 타입·잘못된 반경 거절 | 72조건의 실제 피해·AP·완료 일치와 중복 impact 차단. 전체 18건. `3414054` |
| T07 | 스폰 공격의 몽타주+impact 완료 책임, 시간 초과/취소/사망 정리, 두 자원 소진 후 턴 종료 | 늦은 피해 차단·완료 1회·AP/SubAP 조합. 전체 20건. `ac20045` |
| T08 | GameInstance 수명의 4슬롯 파티 데이터를 Gameplay 스폰으로 연결 | 빈 슬롯 제외·슬롯/좌표 유지·생성 검증·레벨 이동. 초기 `1e2f4b6` |
| T09 | 공통 직업 정의와 이름/직업 편집·ClassInfo를 실제 스탯·장착에 연결 | 네 직업·저장/취소·빈 이름 거절·travel 후 유지. 전체 21건. `4dba375` |
| T10 | 결과 1회·입력 잠금·HP 기록·정리·Continue·두 번째 전투 루프 재확인 | T09 이후 코드 변경 없이 기존 21건의 성공 근거 재사용. 기록 커밋 `d6605d4` |
| T11 | 단일 슬롯 체크포인트·메뉴 Continue·그래픽/VSync 옵션·Quit 연결 | Editor/게임 빌드·전체 23건·BuildCookRun·독립 패키지 저장/이어하기/종료. `b383e6a` |
| T12 | Designer 구조 유지, JSON 생성/검증·AddMissing·메뉴 프리뷰·카메라/배치 정리 | 전체 24건·실제 WBP compile/save·반복 추가 1개/0개·화면 확인. `78f1f81` |
| T13 | 회복약·가중 추가 스킬 획득/장착·적 이동 판단과 실패 전이 | 실제 효과/재고/SubAP·획득 스킬 피해·이동/막힘·두 전투 재지급. 전체 26건. `a97ed4d` |

각 전체 건수는 해당 변경 시점의 자동화 모음이며 서로 합산하지 않는다. 경고 동반 성공도 포함하고 위 최종 실행의 실패는 0건이었다.
T10은 이미 구현된 루프의 완료 근거 정리이며 새 전투 기능을 다시 구현한 커밋이 아니다.
T11의 당시 전투 중 종료는 전투 전부터 복구했다. T14의 확정 턴 저장은 이후 별도 스키마로 추가했다.
T12의 JSON DryRun은 구조 검사다. 실제 생성본의 클래스·바인딩·Blueprint 컴파일·저장 검증은 별도 실행으로 확인했다.
T13의 초기 회복약/추가 스킬은 전투마다 지급하고 HP만 다음 전투에 이어졌다. 이후 전투 중 복구는 T14 체크포인트가 담당한다.

## 3. T14 로컬 Snapshot과 Co-op 확장

초기 싱글플레이 유지/네트워크 보류 기록 `43e9b9e`는 이후 하이브리드 기획 `6cd7a48`로 갱신했다.
게임 방향 `18d71e3`, Co-op 소유권·복구 기획 `ee61c84`는 문서 결정이며 실제 네트워크 구현과 구분한다.
로컬 Snapshot 우선·Unreal `USaveGame` v1을 선택한 뒤 Co-op 작업을 1~8번으로 분할했다.

| 단계 | 완료된 범위 | 당시 최종 근거·커밋 |
|---|---|---|
| 로컬 Snapshot | 값 데이터 저장/검증, 신뢰된 ClassId/SkillId 카탈로그, 기존 Enemy/Combat 연결, 상대별 Run 저장 분리 | 전체 32건·별도 Snapshot 저장 맵 PIE 1건. `4092e53` |
| 1. 식별·소유권 | Run/계정/캐릭터 ID·원래 소유자·Host 값 데이터, v2 저장과 실제 식별 정보 없는 v1 호환 | 전체 36건·Snapshot PIE·독립 Writer/Reader. `66da97c` |
| 2. 서버 명령 | 값 Command와 소유 연결 RPC, 참가자 바인딩·소유권·턴·자원·대상·중복 검증 | 전체 41건·Snapshot PIE. `05843c3` |
| 3. 2인 동기화 | 서버 TurnManager, Combat/GameState 뷰, Unit/GAS/Grid·결과·HUD 복제 | 전체 42건·실제 Listen/Client RPC·Snapshot PIE. `da5aaf0` |
| 4. 턴 저장·기존 Host 복구 | v3 확정 턴 경계·고정 상대·실패 재시도·새 Actor/명령 문맥 복구 | 전체 52건·2인 새 PIE·별도 프로세스·상대 교체/삭제. `19908e2` |
| 5. 아군 AI | 원래 소유권/팀 유지, 서버 판단·공통 명령·별도 AI 세션, 전투 본문 schema 2 모드 저장 | 전체 56건·실제 AI PIE·독립 Writer/Reader. `7fa7235` |
| 6. 독립 기반 | 참여 DTO와 로컬 저장소의 단일 실행 lease·정확한 stamp 교체 | 전체 60건·프로세스 Busy/강제 종료 후 재획득. 이때는 게임 루프 미연결. `e7b35f6` |
| 7. 독립 3·4인 | 각 원격 소유자 RPC·전체 상태/HUD·끊김 정지·기존 Host 새 세션 복구 | 전체 65건·별도 2/3/4인 AI 복구 3건. 승계 검증과 구분. `152aed7` |
| 8. 온라인 준비 | 공식 UE/공급자 연동 지점·서비스 준비물·책임과 수용 기준 문서화 | 문서 검증만 수행. 실제 서비스 미연동. `f877e4d` |
| 추가 Continue 수정 | 협동/AccountProvider 저장의 잘못된 Standalone 진입 차단, 클릭 시 파일 재검증 | 전체 67건·파일 교체 후 메모리/디스크 불변. `cfbb9b9` |
| 확정 정책 반영 | 최초 참가 번호·사전 동의 gate 제거·Host의 노드/Continue 권한 | 서로 다른 69건 최종 성공·AI 복구 3건·Snapshot PIE. `04ed243` |
| 6. 로컬 통합 완료 | 관리 v4·영속 Human 목록·번호순 Host·싱글 전환 메뉴·현재 Human 복구·lease 검사 | 전체 75건·실제 관리 PIE 2건·AI 복구 3건·Snapshot PIE. `122f756` |

### 검증 범위

- Snapshot은 로컬 카탈로그·지원 수치·빈 장비/전술 ID 범위다. 온라인 상대·결과 검증은 미구현이다.
- 서버 명령은 연결·소유권·Run/Host/전투·순번을 검사한다. Accepted는 실행 승인으로 비동기 행동 성공과 구분한다.
- 확정 턴은 저장 성공 후 다음 턴을 시작한다. 지원하지 않는 GAS 상태·난수 복구는 제외한다.
- 관리 재개는 3→2인 Host 승계와 4번의 단독 전환·travel 실패 재시도를 검증했다. 이후 추가된 4→3인 승계는 당시 검증 대기였으며 후속 결과는 최근 변경과 TEST_REPORT 2절을 따른다.
- AI 사전 동의 정책은 폐기되었으며 필드는 직렬화 호환용이다. 현재 정책과 저장/실패 계약은 [MULTIPLAYER](MULTIPLAYER.md)에 정의한다.

## 4. 실패에서 확인한 원인과 수정

| 시점 | 확인한 문제 | 수정과 최종 확인 |
|---|---|---|
| 초기 Gameplay | 빈 NavMesh와 CommonUI Menu 입력이 이동/타일 클릭을 차단 | Gameplay 내비게이션 저장, HUD 입력 설정·travel 잠금/포커스 수정, 실제 Slate 입력 추가 |
| T04 | 창 배치에 따른 Slate hit-test 실패 | 창 위치·크기를 명시한 동일 빌드의 PIE와 최종 전체 실행 통과 |
| T05 / 명령 경계 | 격리 fixture의 Controller 미등록·로컬 Controller 설정 누락 | 테스트 초기화 수정 후 전체/집중 실행 통과. 실제 Gameplay 경로와 구분 |
| T07 / T09 | 타이머/예약 활성화 누락과 테스트 배열 자기 참조 | 프레임 진행·예약 준비 및 복사 후 배열 추가로 테스트 자체 보정 |
| T14 3번 | PIE 시작 시 재생성 중 NavMesh가 비어 복제되어 이동 실패 | 편집기 내비게이션 완료·유효 경로를 기다림. 맵 범위나 이동 성공 조건은 완화하지 않음 |
| T14 3번 | Client BeginPlay 문맥 덮어쓰기·재전송 Pending 고착·타일 색 캐시 순서 | 복제 초기화와 응답/표시 경로 보정 후 실제 네트워크·전체 회귀 통과 |
| T14 4번 | 저장 안내 Overlay가 클릭 통과 설정을 잃음 | 루트 hit-test 처리 수정 후 기본/상대 Snapshot의 Slate 전투 루프 통과 |
| T14 5번 | fixture Host가 죽어 Host 차례를 영원히 기다림 | 로그로 AI 턴 종료 정상 확인, Host와 저장 Party HP 보정. 원래 행동 횟수·복제 조건 유지 |
| T14 7번 독립 | `UNetConnection::Close` 링크 의존성과 의도한 연결 종료 Error | Editor `NetCore` 추가, 종료 직전 해당 Error 1회만 예상. 다른 오류는 계속 실패 |
| 정책 반영 | Host 진행 UI가 활성화되기 전 버튼을 검사 | CommonUI 활성 화면/버튼 준비 대기 후 실패했던 3건 재실행 성공 |
| 관리 Run | 메뉴 travel 실패 후 lease 잔존·v4 오류 이유 소실 | GameInstance 수명의 동일 실행 travel 실패 감시, 확정 바이트 보존·lease 반환·메뉴 재시도 연결 |
| 관리 PIE | namespace가 저장소 최대 32자를 초과 | fixture를 31자로 수정 후 실제 PIE 2건 성공. production 제한 유지 |

초기 R01~R06 리뷰 결함은 T01~T06에서 모두 후속 처리했다. 오래된 리뷰의 멀티플레이 미검증 문장은 현재 상태가 아니므로 미해결 결함으로 재등록하지 않는다.
기존 경고에는 축소 월드·Spawn/GAS Cue 설정·종료 중 Nav 조회·의도한 거절/취소가 포함됐다. 경고 동반 성공을 경고 없는 실행이나 패키지 검증으로 바꾸어 기록하지 않는다.

## 5. 대표 검증 산출물

아래 경로는 당시 로컬 `Saved` 산출물이다. Git에 포함되지 않아 다른 checkout에는 없을 수 있으며, 전체 로그 복사 대신 대표 결과만 남긴다.

| 범위 | 대표 기록 |
|---|---|
| 초기 Slice·입력 | `Saved/Automation/GameplayInputFix2/index.json`, `GameplayAssetValidation.json` |
| T03~T07 | `T03APCost1`, `T04TargetingFinal`, `T05InputFinal2`, `T06AreaFinal`, `T07CompletionFinal3`의 `Saved/Automation` 결과 |
| T09~T13 | `T09ProfessionFinal4`, `T11Final`, `T12Final`, `T13Final`의 결과와 T11 패키지 `T11PackWrite`/`T11PackContinue` 로그 |
| Snapshot·소유권·명령 | `T14Full`, `T14SnapshotPIE`, `T14OwnershipFull`, `T14OwnershipRestartWrite/Read`, `T14CommandsFull2` |
| 2인·확정 턴 | `T14NetworkFull`, `T14CheckpointFull3`, `T14CheckpointRestartWrite/Read`, `T14CheckpointSnapshotReplace/Delete` |
| 아군 AI | `T14PartyAIFull2`, `T14PartyAICoop2`, `T14PartyAIRestartWrite/Read` |
| 3·4인·추가 정책 | `T14ScaleFull1`, `T14ScaleAI1`, `T14StandaloneContinueFull1`, `T14HostPolicyFull1`, `T14HostPolicyCombat2` |
| 관리 Run 통합 | `T14ManagedRunBuild3.log`, `T14ManagedRunFull1/index.json`, `T14ManagedRunPIE2/index.json`, `T14ManagedRunAI1/index.json`, `T14ManagedRunSnapshot1/index.json` |

T14의 Writer/Reader·AI·관리 PIE는 전용 인자가 없는 전체 실행에서 안내만 남기는 경우가 있다. 위 전체 건수와 별도 실제 실행 기록을 함께 읽는다.
정책 반영 69건은 최초 66건 성공과 UI 준비 보정 후 3건 성공을 합친 서로 다른 테스트 집계다. 수정 후 전체를 한 번에 다시 통과한 기록은 아니다.
관리 통합의 전체 75건은 성공 51·경고 동반 성공 24·실패 0이며, 관리 PIE 2건은 전용 인자로 별도 실행했다.
3·4인 정원 검증은 엔진 입장 검사에서 4명 이후 거절을 확인했다. 실제 다섯 번째 PIE 접속을 실행한 것으로 기록하지 않는다.

## 6. 검증 제한

T14 1~6번은 `122f756`까지 로컬 구현·검증을 완료했다. 당시 대기였던 4→3인 승계의 Client별 바인딩·턴 종료 응답은 후속 ff22940 기준 실행에서 통과했다. 기존 전투·Host 복구 성공과 별도 기록이다.

Steam/PlayFab·중앙 저장·결과 중복 방지·MMR은 미구현이다. 실제 마우스 조작·이동 중 취소·최신 패키지·네트워크 지연/손실 검증은 기존 자동화와 구분한다. 작동 테스트는 명시적 요청 범위에서만 Codex가 실행한다.

## 7. 테스트 인계·문서 통합

- 2026-09-10: 사용자 중심 작동 검증 원칙을 AGENTS·README에 반영했다. 이후 명시적 요청에 한해 Codex가 실행한다.
- Docs 14개를 기획·구조·작업·계약·검증·이력의 6개로 통합했다. 원문은 Git 이력에서 조회한다.
- 직전 지시 이전에 시작된 `Saved/Automation/T14ManagedScaleFull1/index.json`은 76건(성공 49·경고 동반 성공 27·실패 0)이었다. 이 결과에는 전용 인자로 실행한 관리 PIE 3건이 포함되지만, 실행 뒤 추가한 최신 바인딩 복제 대기 조건은 포함하지 않는다. 최신 소스 작동 성공으로 계산하지 않는다.
- Unity 빌드 보조 함수명 충돌·바인딩 대기 변경의 Editor 빌드는 `-DisableAdaptiveUnity`로 성공했다. 로그: `Saved/Automation/T14ManualHandoffBuild1.log`.
- 인계 당시 작동 테스트는 미실행이었다. 이후 7번 승계 실행 결과는 [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md)를 따른다.

## 8. 서비스 준비 점검

- 2026-09-11: 7번 승계 검증 대기, Steam App ID·PlayFab Title 미준비를 확인했다.
- UE 5.7 설치 소스에서 OnlineSubsystemSteam·SteamSockets와 IP 전용 SocketSubsystemSteamIP의 차이를 확인했다. 프로젝트 서비스 설정·PlayFab 플러그인은 없는 상태다.
- 공식 안내를 대조해 공용 Steam ID 480 개발 연결과 자체 앱 등록 수수료, PlayFab Foundation 전환·Xbox 관련 자격 조건, 계정/Title 준비 절차를 MULTIPLAYER와 TEST_REPORT에 추가했다.
- 점검 범위는 문서·정적 조사다. 가입·결제·SDK 설치·실행·설정 변경은 미수행이며 온라인 연동은 미구현이다.

## 9 TODO 완료 항목 이관

2026-09-30에 남은 작업에서 분리했다. 9-1~9-12는 2026-09-18까지의 코드로 수행한 실행 확인이며 이후 변경의 성공 근거가 아니다. 기존 TODO 번호와 당시 제한을 보존한다. 현행 코드의 추가 검증과 제한은 [9-15절](#9-15-2026-10-01-todo-재검증과-구현-이관)에 기록한다.

### 9-1 사용자 작동 확인

2026-09-16 사용자 확인: **지금까지의 일반 플레이에서 이상 없음.** 개별 예외 상황·협동까지 통과한 것으로 확대하지 않는다. 아래 완료 항목은 이후 2026-09-18 위임 실행의 당시 결과다.

- [x] 싱글 PIE의 두 전투·결과·상점 선택/퇴장·Run 완료, 별도 게임 프로세스의 메뉴 이어하기와 다음 노드 개방을 확인했다.
- [x] 서버 월드에서 벽·센서·중복 피격·진영·투사체 수명과 시전자 사망을 검증했다. [판정 기준](GAME_DESIGN.md#8-5-발동과-피격)
- [x] 동일 PC Listen Server PIE를 2인 → 4인 순서로 실행했다. 소유자별 요청·HP/계획 복제·원격 AP/몽타주/보행·Host 전용 결과/상점 진행을 확인했다. Steam·다중 PC·지연/손실 검증은 포함하지 않는다. [협동 기준](MULTIPLAYER.md)
- [x] 크기가 다른 PIE 창 4회에서 대상 클릭·네 스킬 버튼과 화면 캡처를 확인했다. 실제 게임 창의 해상도 변경·확정 저장·명시 취소·15초 복원을 통과했다. 창 크기는 데스크톱 영역에 맞춰 제한되므로 요청한 4:3/울트라와이드 해상도 전체를 검증한 것은 아니다.

2026-09-18 이전 작업에서 사용자가 TODO 테스트 전체를 Codex에 위임했다. 위 완료 표시는 당시 코드의 실행 이력이다. 이후 확정 규칙·Ready 복구의 자동화 범위와 강제 종료 후 Actor 재구성의 미확인은 [9-15절](#9-15-2026-10-01-todo-재검증과-구현-이관)에 기록한다. 당시 결과·제한은 [HISTORY](#최근-변경)를 따른다. 별도 실행 위임이 없는 최신 작업에서는 PIE·게임·자동화 테스트를 시작하지 않는다.

### 9-2 GameplayCue 설정과 경고

원래 항목: TODO 2-1의 설정 검사.

- [x] 실제 Game 설정·설정 객체·Globals 검색 경로 일치와 전체 `/Game` fallback 경고 소멸을 확인했다.

새 Cue 에셋을 채택하면 실제 발동 확인이 필요하다. [신규 에셋 도입](TODO.md#6-신규-에셋-선정과-도입)

### 9-3 전투 준비 취소와 저장 실패

원래 항목: TODO 2-2.

- [x] 저장 실패 주입 후 준비 취소·Continue 재시도·입력 차단·중첩 요청 거절·확정 파일 보존을 통과했다.
- [x] 잘못된 직업 클래스·능력치·시작 스킬 거절과 기존 fallback 수용을 통과했다.

### 9-4 이전 테스트 에셋 참조

원래 항목: TODO 2-3.

- [x] 역참조가 없는 `TestGameModebase` Redirector 패키지를 Unreal `ResavePackages -FixupRedirects`로 정리했다. 현재 Gameplay·MainMenu는 별도 GameMode를 사용한다.

### 9-5 DA 시전 몽타주 연결

원래 항목: TODO 2-4.

- [x] 실제 PIE 몽타주의 반복·취소·잘못된 재생 속도·사망 정리와 서버 회귀의 유한 대기·복귀를 통과했다. 협동 종료 중 파괴 액터의 불필요한 몽타주 RPC도 수정했다.

### 9-6 DA 유형별 폴더와 저장 호환

원래 항목: TODO 2-5.

- [x] 이전 DA 카탈로그 경로를 담은 지원 직업의 비전투 저장을 복원했다. 현재 에셋 해석·직접 조작 선택·원본 저장 바이트 보존을 확인했다. 제거된 테스트 직업의 저장은 계속 거절한다.

### 9-7 캐릭터 생성 프리뷰

원래 항목: TODO 2-6.

- [x] 실제 네 프리뷰의 idle 반복 경계 통과·슬롯 삭제·화면 닫기/재진입과 액터 정리를 확인했다.

### 9-8 시작 모드 선택과 싱글 여정 항복

원래 항목: TODO 2-7.

- [x] 실제 항복 화면의 취소·삭제 실패 재시도·성공 후 이어하기 비활성화를 통과했다. 파일 변경·공유 잠금 실패·저장 바이트 보존은 저장 계층 회귀에서도 확인했다.

### 9-9 민첩 기반 전투 속도

원래 항목: TODO 2-8.

2026-09-17 사용자 확인: 근접 접근·복귀 속도 체감 적정.

- [x] 소수 민첩·라운드 중 변경의 고정 시각표를 실행 검증했다. 실제 Snapshot 스폰에서 민첩 12.5/5.25와 시작 지연 0/0.725초, 아군 10의 0.25초를 확인했다.
- [x] 근접 접근·복귀의 민첩 5/10별 실제 이동 거리·보행 입력·몽타주 대기·원래 방향 복원을 통과했다. [초기 튜닝](GAME_DESIGN.md#8-4-공격-접근과-복귀)은 최종 밸런스 확정과 구분한다.

### 9-10 싱글플레이 직접 조작 캐릭터 선택

원래 항목: TODO 2-9.

- [x] 저장/복원·다음 전투의 직접 조작 선택 보존과 인간 사망 후 AI 동료의 독립 진행을 실행 회귀로 확인했다.

### 9-11 전장 대상 선택과 SAP 이동 예약

원래 항목: TODO 2-10.

- [x] 실제 싱글·2인·4인 PIE에서 SAP 예약 → 350cm/s 이동 → 새 복귀점 확정 → AP 실행을 확인했다. 원격 메시의 보행 속도·애니메이션 인스턴스도 관찰했다. 속도 체감의 최종 선호는 별도다.
- [x] 서버 회귀에서 SAP 실패·중단·사망 시 비용·복귀·정지와 생존자 진행을 통과했다.

### 9-12 기본 원거리 공격

원래 항목: TODO 2-11.

상태: 기본공격·원거리 공격·AOE·휩쓸기를 공통 플레이어 시험 장착에 연결했다. 2026-09-18 실제 PIE 4회에서 목록·선택·AP 소모·피해 적용과 원거리 투사체를 확인했다. [검증 범위](#최근-변경)

- [x] 새 게임 → 첫 전투에서 화면 전환 완료 후 Slate 마우스 누름/해제 한 번을 실제 뷰포트 hit-test와 컨트롤러 입력으로 전달해 네 스킬을 표시·실행했다. 스킬 버튼은 delegate로 눌렀으며 AP·피해·투사체와 원거리 시전자의 제자리 유지를 확인했다. 당시 휩쓸기는 지점 공격·몽타주 미지정 상태였으며 이후 삭제·저장 변환 범위는 [9-15절](#9-15-2026-10-01-todo-재검증과-구현-이관)을 따른다. [공격 정의](GAME_DESIGN.md#8-7-기본-전투-전환과-스킬-데이터)

### 9-13 마녀와 Assassin 외형

원래 항목: TODO 2-29.

상태: 이전 구성 이력이다. 2026-09-23 마법사 Stylized Dark Witch·도적 Assassin Skin1을 연결하고 마녀 원본 임포트의 본 배율 1·높이 187.12cm·물리 바디 48개와 왼손 스태프를 정리했다. 당시 Development Editor / Win64 컴파일·별도 재로드·48개 시퀀스 포즈/배율·검 표본 164개·물리 연결 검사를 통과했다. 실제 게임 실행은 하지 않았다. 원본·리타깃 자료는 보존하며 현재 네 직업 외형과 확인 절차는 [2-30절](UI_README.md#4-화면프리뷰)을 따른다.

### 9-14 CSV 분류와 표시명 정적 검증

원래 항목: TODO 2-34·2-35의 완료 근거. 실제 재생·한국어 표시·기존 저장 호환은 새 에셋 채택 후 확인할 범위이며, 현재 목록과 표시명 기준은 [스킬 이펙트 목록](PROJECT_PLAN.md#4-1-스킬-이펙트-에셋-목록)·[콘텐츠 설정](PROJECT_PLAN.md#4-콘텐츠ui-설정)을 따른다.

| 기준 | 완료 범위·검증 |
|---|---|
| 2026-09-25, 76c32ec | 이펙트 577행 작성. uasset 15,007개 정적 조사, 효과 시스템 489개 누락 없음·구성 Blueprint 88개·원본 경로/클래스 확인. 원본·C++ 변경과 엔진 실행 없음. |
| 2026-09-25, 43dfdaa | 무기 295개·이펙트 577개 한국어 이름 연결. 당시 Development Editor / Win64 최종 증분 4.44초·CSV 원본 보존/이름/왕복 검사 통과. 파싱·이전 저장 호환 회귀 소스는 컴파일만 수행. 게임·PIE·자동화 미실행. |
| 2026-09-27, 3f214bd | 속성·테마 528행 갱신. 단일 502·복합 31·공용 표현/기반 BP 공란 44. 행 순서·다른 9열·허용 5속성/복합 표기·UTF-8 BOM CSV 재읽기·문서 링크·전체 스테이징 diff·diff --check 통과. 엔진·게임 미실행. |
| 2026-09-27, 236b1fd | 스킬 중복명 127그룹 272행에 기존 순서의 번호 부여. 무기 295개·단독 이름 유지, 전체 표시명 872개 중복 없음·그룹 순번·다른 열/행 순서·CSV 재읽기·미리보기·링크·전체 스테이징 diff·diff --check 통과. C++·원본 에셋 변경과 엔진·게임 실행 없음. |

### 9-15 2026-10-01 TODO 재검증과 구현 이관

기준: `65c64a1` 이후 2026-10-01 당시 수정. TODO의 완료 구현 설명을 이관하고 작업 ID·제안 1~4의 선택 `1·1·2·1`·제안 5 미선택·수용 조건을 당시 문서에 유지했다. 이후 사용자가 TODO에서 해당 확인 목록과 제안 5를 삭제했다. 아래 완료 범위와 새 에셋·실제 화면·서비스 확인을 구분하며 이전 검증 기록은 당시 코드의 이력으로 보존한다.

| 원래 항목 | 이관한 완료 구현 |
|---|---|
| 2-1 | Game 설정·DeveloperSettings CDO·AbilitySystemGlobals의 Cue 검색 경로 일치와 프로젝트 경로 적용·전체 `/Game` fallback 제외를 실행 검증했다. 실제 Cue 표현 에셋은 미작성이다. |
| 2-12 | 삭제 3종의 Run·Snapshot·Ready·상점 후보 변환과 디버그 수집의 공통 제외 판정. 다른 스킬·보유품·골드·진행·소유권 보존. |
| 2-13·2-19·2-24·2-37 | 기존 적 4마리 편성, 새 10전투·중간 상점 9회와 기존 두 전투 경로 유지. 스킬 5칸·전체 리롤 1G 시작/+1G·회복 1G·개인 보유, 승리 5~15G 3택1·중복 수령 방지·Host Continue. 일반 3상점·1G 상품과 이전 무료 상점의 일반 전환에서 보유 스킬·골드를 보존했다. 가격은 시험값. |
| 2-14·2-22·2-23·제안 1~4 | 복귀/예약 칸 중복·자리 교환 거절, 서버 순서 피해·선행 사망 취소, 해당 소유자만 준비 해제, 양 팀 전멸 시 패배. Ready 경계 일반 v5·관리 v4·LegacyOffline v6 저장·복구 데이터, 저장 실패 차단·재시도, 무스킬 Ready·SAP 단독 이동·근접 재선택. |
| 2-15·2-16·2-25·2-26·2-28 | 현행 검의 서버 칼날 접촉·접근/시전/복귀, 기존 애니메이션 임포트·클래스 경로 호환·원본 참조 통합, 공통 래그돌·사망·충격량. 새 팩 선정·리타깃·물리/외형 검증은 별도. |
| 2-17·2-18·2-20·2-21·2-30·2-32 | 전투 패널·공통 DPI, 개발 HP 표시, I/Esc·개인 잔액·공통 장비/인벤토리, BodyId·남녀 프리뷰·회전·정면 카메라 구현. 화면 조작·해상도별 확인은 당시 미완료 범위. |
| 2-27 | 공통 능력치·계획 검증, AI·스킬·GAS·충돌 책임 분리와 저장 후보 반영. 최종 HP·전투 결과·보상·단계의 원자 저장과 실패 시 전체 상태 보존·재시도 1회 공개. |
| 2-31·2-33·2-35·3-2 | 아이템상점 5칸·구매/리롤 1G·CSV 정수 가격 검증·표시명 보존. 시작 장비·9슬롯·양손 점유·개별 사본·장착 명령/권한/Revision·원자 저장. 네 직업·공통 시작 능력치. 추가 장비 능력치·부여 스킬·Snapshot EquipmentIds는 미구현. |
| 2-34·2-36·2-37 | 기존 FX/스킬 CSV·한국어 이름·태그·생성 명세와 경로/PrimaryAssetId 연결, 즉시 치유·라운드 보호막·실행 프로필. 디버그 분류/검색·타이밍·부활·유닛/HP 편집, VFX 종료 제한·투사체 0.5배 및 사거리 보정. 새 표현 선정·실제 시각 품질은 별도. |

이번 보완: 저장 검증 helper의 성공이 공통 거절 사유를 지우지 않도록 지역 오류를 사용했다. 칼날 포즈 추출은 Unreal의 메시 계층 호환 검사와 압축 SkeletonRemapping을 사용하고 압축 누락·구버전 본 정보·에디터 raw 강제 상태를 안전하게 거절한다. 원본 에셋은 변경하지 않았다. 충돌 테스트의 GAS/사망 초기화, 과거 저장의 보상 스키마와 삭제 후 후보 177개 기대값을 바로잡았다. 원자 저장 회귀는 에디터 FText의 임시 GUID와 게임 상태를 구분하면서 전체 런타임 속성·원본 파일 바이트·발행 횟수·저장 후 공개 순서 검증을 유지한다.

메뉴 Continue 보완: 보유품과 고정 카탈로그의 표시명·Asset·Tags·Price가 같아도 에디터에서 저장된 서로 다른 `FText` key를 게임 문맥에서 동일성 비교하여 정상 저장을 거절했다. 공통 값 비교로 오거절을 해소하고 네 필드의 무결성 검사를 유지했다. 기존 writer 저장을 직접 읽은 최종 `MenuContinue`의 Info는 `GIsEditor=0`·`strictEqual=0`·`valueEqual=1`과 두 key를 기록했으며 원본을 새 fixture로 교체하지 않았다.

| 이번 실행 검증 | 결과·범위 |
|---|---|
| Development Editor / Win64 컴파일 | 메뉴 fixture의 실제 Save·`-ProjectAFlowOnly`를 포함한 `BuildFlowAndContinueFinal.log` 5.82초 성공 후, 공통 저장 값 비교·회귀·메뉴 진단을 포함한 최종 `BuildSaveValidationFinal.log` 컴파일·링크 13.34초 성공. |
| 앞선 엔진 자동화 | `HeadlessFinal/index.json` 166개 통과(일반 성공 102·경고 동반 64), 실패·미실행 0. 삭제 저장/후보 변환·남은 스킬/골드/진행 보존, 일반·관리·LegacyOffline Ready 데이터, 예약 충돌·무스킬 준비/SAP·대상 사망·잔여 투사체·양 팀 전멸, 소유권·보상·상점·장착·CSV 가격 경계·원자 저장을 확인했다. 최종 저장 값 비교 보완 전 결과이며 이후 전체 166개는 다시 실행하지 않았다. |
| 최종 대상 저장 검사 | `SaveValidationEditor/index.json` 7개·`SaveValidationGame/index.json` 6개 통과, 각 실패·미실행 0. 양 문맥에서 Checkpoint·Standalone Continue 적격성/파일 교체·항복 확정/적격성/파일 교체를 확인하고 에디터에서는 전체 후보 공개 보존도 확인했다. 저장 문맥 검사이며 Ready Actor 재구성·표현 검증은 포함하지 않는다. |
| 2/4인 Listen Server PIE | `RenderPIESecond.json`의 `RunRoundPIE.2Players`·`4Players` 통과. 각 10전투·9회 아이템상점 선택/퇴장, 매 전투 적 4명, 개인 골드 카드 delegate·중복 차단·Host Continue, HP/골드 저장 재로드, 원격 AP/몽타주·SAP 350cm/s 관찰. 구매·장비 조작은 포함하지 않는다. |
| 1인 진행 PIE | `RuntimeFlowFirst.json`의 `RunRoundPIE.1Players` 통과. 같은 10전투·9상점·보상/HP 저장 재로드를 확인했다. 메뉴와 다른 fixture 초기화 순서의 로컬 계정 문맥을 갱신했으며 정상 난이도 검증은 아니다. |
| 메뉴 버튼 PIE | `MenuFlow/index.json`의 `SavedMenuLifecycle -ProjectAFlowOnly` 통과(경고 3건). 4슬롯 생성·실제 Save·직업/이름 수정·취소·직접 조작 선택/교체·삭제, ClassInfo 배치와 닫기/재진입의 프리뷰 정리를 확인했다. 프리뷰 애니메이션 재생·루프 경계와 두 에셋 스크린샷은 명시적으로 제외했다. |
| 독립 게임 창 Quit | `MenuQuit/index.json`의 `PackagedQuit` 통과. `MenuQuit.log`의 실제 Quit 버튼 요청·`UGameEngine::HandleExitCommand`·자연 프로세스 종료를 확인했다. uncooked `-game` 실행이며 패키징 검증은 아니다. |
| 독립 게임 창 Continue | 최종 `MenuContinue/index.json` 1개 통과. 실제 이어하기 버튼 → Gameplay의 저장된 Result에서 이름·HP 61·진행과 기존 골드 3택1 복원 → 원래 소유자의 수령 → Continue → `Shop_02` 퇴장 → `Combat_02` 개방을 확인하고 고유 시험 저장을 정리했다. 초기 timeout은 `MenuContinueFirst.*`, 적격성 실패는 `MenuContinueEligibilityFailure.json`·`.log`에 보존했다. uncooked `-game` 실행이다. |
| 남은 프리뷰 검증 | `RuntimeFlowFirst.json`의 전체 `SavedMenuLifecycle`에서 idle 루프 경계 assertion 4건 실패. 재생/반복 플래그는 통과했지만 캡처·삭제가 같은 프레임에 진행되어 실시간 대기가 애니메이션 Tick을 보장하지 못했을 가능성이 있으며 에셋 불량으로 확정하지 않는다. 새 에셋 연결 후 실제 Tick·재생 시각·루프를 확인한다. |
| 별도 프로세스 저장 복원 | `RestartWrite/index.json`·`RestartRead/index.json` 각 1개 통과. Run/캐릭터/원래 소유자 식별·HP·스킬·진행과 수령 후 Continue를 복원했다. Ready 전투 Actor 재구성 검사가 아니다. |
| 독립 게임 창 설정·항복 | `GameWindow/index.json` 2개 통과. 화면 설정 미리보기·취소·15초 만료·확인 후 디스크 재로드와 항복 취소·삭제 실패 재시도·이어하기 비활성화를 확인했다. uncooked `-game` 실행이며 패키징 검증은 아니다. |
| 프로세스 잠금·명시적 재개 | `AuthorityBusyReader/index.json`·`AuthorityResumeReader/index.json` 각 1개 통과. 고유 저장 공간에서 보유 프로세스의 PID를 대조해 강제 종료한 뒤 OS 잠금 획득·Epoch/Revision/Session 갱신·쓰기·이전 Stamp 거절을 확인했다. 자동 Host 승계·Steam/중앙 저장 검증은 아니다. |
| 중단 전 VFX 표본 | `RenderPIEInterruptedForScope.log`의 `AuthoredToolsAndVfx` 통과와 실제 PIE 캡처 8장 저장을 확인했다. 다중 viewport의 전역 요청 오캡처를 실제 PIE 렌더 이벤트 캡처로 수정했다. 둘째 표본은 첫 입자 관측 이후 최소 0.1초이며 수명 중간 프레임이 아니다. 첫 표본 일부에서 표현이 희미해 최초 가시성·방향·최종 품질 완료로 처리하지 않았다. |

실행 근거는 `Saved/Automation/TodoArchive/`에 보존한다. 최초 엔진 자동화 8개 실패는 저장 오류 사유·메시 호환의 실제 결함과 fixture 초기화/기대값/텍스트 비교 문제를 수정한 뒤 모두 통과했다. 초기 PIE의 1인 패배·종료 후 포인터 접근과 옛 메뉴의 편집 미저장 흐름은 생존 HP fixture·다중 라운드·월드 수명 검사·실제 편집 저장 버튼으로 보완했다. HP 10,000·적 1회 타격 조건은 진행 검증용이며 정상 난이도 검증은 아니다. 최종 컴파일 13.34초·대상 저장 재검증 후 `Verification.json`에서 기존 저장 관련 파일 44개의 SHA와 `GameUserSettings` 원본 복원 해시가 동일하며 작업 ID·정책 선택 보존을 확인했다. 검증에 사용한 UE 프로세스는 모두 종료했다.

후속 사용자 지시에 따라 스킬별 실행·VFX와 아이템 구매·장비·드래그·메시의 추가 실행은 제외했다. 이미 완료한 실행은 위 범위로만 보존하며 `SavedSkillLoadout`의 최신 fixture는 컴파일 후 재실행하지 않았다. 전체 메뉴의 idle 루프는 새 에셋 연결 후 실제 Tick·재생 시각과 함께 다시 확인한다. 새 에셋 도입은 [TODO 6절](TODO.md#6-신규-에셋-선정과-도입), 서비스 연동은 [TODO 4절](TODO.md#4-온라인-협동과-경쟁)을 따른다. 실제 마우스 입력 범위·해상도별 가독성·Ready 강제 종료 후 Actor/준비 상태 재구성은 이 기록의 미확인 범위다.

### 9-16 2026-10-03 환경 비교 레벨

Fab 웹 보유 라이브러리에서 Orasot 환경 번들·Infinity Blade Ice Lands·Stylized Dynamic Nature를 확인하고 원본 팩 경로에 도입했다. UE 5.8.3에서 초원·숲·대나무·사막·습지·빙하·설원 요새·여름 해안의 별도 비교 맵 12개를 작성했다. 맵 목록과 원본 참조는 [PROJECT_PLAN 4-4](PROJECT_PLAN.md#4-4-환경-비교-레벨)를 따른다. 기존 Gameplay·DebugCombat·던전과 기본 Run 진입은 유지한다.

총 3,240개 장식을 맵별 ISM 9~17개 그룹으로 묶고 거리 제한·그림자 제한·장식의 충돌 및 navigation 비활성화를 적용했다. 원본 메시·텍스처를 직접 참조하며 지면 Material 9개와 RVT 해제·얼음 ISM usage override·물 표현용 자식 MI 44개만 새로 작성했다. 새 맵은 합계 2,143,260바이트, 새 재질은 267,354바이트이며 원본 팩은 Git에 포함하지 않는다.

에셋 작성·12개 navigation 저장·최종 독립 재로드 명령은 모두 ExitCode 0이다. 재질 그래프·부모/텍스처/파라미터·ISM usage, 저장된 배치·재질·충돌·카메라 48표본·화면 비율별 지면 커버·물리 노출을 검사했다. 작성부터 navigation 저장·재로드까지 보호 대상 8,176개 파일의 SHA가 유지됐고, 재로드 전후 맵 12개의 SHA도 동일하다. Python AST·문서 링크·diff 정적 검사를 통과했다. NullRHI 재질 재컴파일 요청의 API 오류 목록은 비어 있었으나 실제 셰이더 완료·화면·게임 조작·FPS는 검증하지 않았다. 추가 확인은 [후속 검수 이력](#9-19-2026-10-04-todo-재개-검수와-저장제목-보완)을 따른다.

등록되지 않은 GUID를 가리키던 작업 사본의 엔진 연결을 설치된 UE 5.8.3의 연결값 `5.8`로 복구했다. 최종 값은 기존 HEAD와 동일하다. 기존 `Automation_ProjectA.sln`의 자동 생성 프로젝트 순서 변경도 함께 보존했으며 프로젝트 구성 목록은 동일하다. C++ 변경이 없어 추가 컴파일은 수행하지 않았다.

### 9-17 2026-10-04 UE 5.8 TODO 실행 검수

기준: 사용자가 TODO 전체 작업과 가능한 실행 검수를 위임한 2026-10-04 작업 사본·UE 5.8.3. 아래는 실제 실행한 빌드별 범위이며 9-15·9-16의 당시 성공·실패를 최신 코드의 근거로 대체하지 않는다. HP Paint 공간 보완 뒤 환경 15개·전체 native 175개·기존 사용자 저장 UUID 사본의 실제 Continue와 SkillShop 구매/별도 Continue/다음 전투 4종을 통과했다. 프로젝트 VFX DA 세 개의 표현/신호·Poison 자연 음향 종료와 최신 cooked 콘텐츠/같은 바이너리의 저장 작성/Continue/Quit도 통과했다. 제안 6~14는 모두 미선택이며 체인 초기 수치 질문도 미응답이다. Steam/PlayFab ID·유료 리소스·온라인 정책을 임의로 채택하지 않았다.

| 검수 | 실제 결과·제한 |
|---|---|
| 컴파일·전체 native | HP Paint 공간 보완 당시 25.19초·SkillShop fixture 당시 11.22초 빌드 뒤, 인벤토리 종료 가드 보완을 포함한 최종 Development Editor / Win64 컴파일·링크 8.90초를 통과했다. `Native.Final/index.json` 06:45:12의 **전체 175/175**은 일반 성공 109·경고 동반 성공 66·실패/미실행/진행 중 0·12.445초·프로세스 ExitCode 0이다. 새 CSV 289개·모든 패키지 경로, `PackagedSkillProfiles`의 61후보/세 보완 DA 실제 로드와 체인 연출·발동 사운드 자연 완료 보존을 포함한 전체 재실행이며 부분 실행 합산은 아니다. native 결과와 최신 패키지 결과는 인벤토리 가드만 보완한 후속 Editor 빌드의 재실행으로 확대하지 않는다. uncooked Editor의 두 콘텐츠 로드 검사와 실제 패키지 UFS 결과는 별도다. |
| 체인 Runtime | native 자동화 6개에서 최근접 생존 미타격·중복 제외·태그·원래 시전자·첫 물리 볼륨/벽/타이밍·이후 거리/LOS/사망·GAS HP/AP 1회·라운드 잠금·중단 정리를 확인했다. 별도 발동 사운드를 판정 종료 때 자르던 경계를 자연 완료/기존 최대 수명으로 보완하고 3/10/60초 actor 정리 시한·hit/완료 1회를 확인했다. 이 회귀는 오디오 비활성이므로 청취 검증이 아니다. 렌더 전수의 초기 25회에서 5종의 네 방향/LWC·세 최근접 대상·움직인 캡슐 추종·원래 Niagara instance/age와 시전자 GAS·AP 1회·첫 연결 SFX 설정을 확인했다. 시험 설정은 transient `3/400cm/0.4s/1`이며 실제 DA는 `MaxTargets=1`을 유지한다. 초기 수치 선택·DA 활성화/재로드·전체 렌더/오디오 품질·협동 동기화는 대기다. |
| 에셋 보존·삭제 | Gameplay 29개·환경 맵 14개·Material 9개/MI 44개·몬스터 정의 12개와 Skeleton·Blueprint 64개의 로드/참조를 확인했다. 삭제 8,372패키지 미존재와 의존성 23,411개 안전성을 점검했다. 모든 검수 프로세스 종료 후 최종 Content 19,807파일 중 19,804개의 크기/SHA256 동일과 승인된 Line_Lava·PoisonCarousel·Ninja DA 세 개의 정확한 전후 변경을 통과했다. 원본 저장 49파일·GameUserSettings 1파일과 외부 Editor 키 설정 SHA도 보존했다. baseline/Content 불일치·예상 밖 Content/추가 Save는 0이며 외부 원본 팩·제작/리타깃 자료·사용 중 콘텐츠를 유지했다. |
| 환경·사망 표시 | HP Paint 공간을 보완한 25.19초 빌드의 `Environment/index.json` 05:41:54는 **15/15**을 통과했다(경고 동반 성공 15·실패/미실행/진행 중 0·186.766초·프로세스 ExitCode 0). 실제 Windows 창에서 환경 14맵의 4:3·16:9·21:9 렌더 42장과 사망/부활 3장을 직접 보아 몸체/UI/타일과 머리 위 HP·숨김/재표시를 확인했다. 5유닛×3비율 앵커 오차는 최대 0.585px, 사망 라벨 5→4→5/box 15→12→15·보호막 소모와 native 데스크톱 위치 440,352→477,381에서도 Paint 원점 3,35를 유지했다. 실제 Slate 타일 클릭·delegate/예약·공개 SAP/점유와 14맵의 동일 1280×720/품질 연속 180프레임도 통과했다. 플랫폼 평균 8.374~11.688ms·p95 10.153~18.312ms는 Editor/native Windows 간격이며 CPU/GPU·표시 완료·최적화 전후 비교는 아니다. UI 갱신 대기/GT viewport 보완 후 앞선 teardown assertion은 재발하지 않았고 실패 로그도 보존했다. |
| 저장 스킬·실제 대상 클릭 | 실제 CursorPointerIndex를 반영한 최신 `SkillLoadout/index.json`의 `SavedSkillLoadout` 1개가 일반 성공·실패/미실행 0·프로세스 ExitCode 0을 통과했다. 16:9 비무장·4:3 근접의 두 실제 세션에서 저장된 장착 목록·Slate LMB 대상 클릭·GAS HP 50 감소/AP 1·근접 공격 라벨을 확인했다. 기존 사용자 저장과 해당 스킬상점 상품 표시의 전수 검수는 아니다. |
| 원본 저장 사본 읽기 | Python의 사본은 `/Script/ProjectA.RunSaveGame` 역직렬화 후 비노출 UPROPERTY의 protected 접근에서 실패했으며 실패 보고서와 원본 보존 근거를 유지했다. 별도 Editor C++의 명시 UUID/크기/SHA256 검수는 raw 필드와 현재 공식 `ResolveRoundSkill`의 `근접 공격` 라벨을 통과했다(각 1개·프로세스 ExitCode 0). 원본은 Version 5/Combat/Combat_01·파티 4명 HP 100·골드 0/0/10/0·Archer 직접 조작·확정 아이템 295개/진열 0·스킬 177개/진열 5였으며 네 명 모두 근접 DA를 보유하지 않았다. raw RoundDefinition.Name은 비어 있고 공식 해석 결과는 현재 표시명을 사용한다. 원본 355,741바이트·기준 SHA256과 사본 바이트 보존/소유 사본 정리를 확인했다. 원본 슬롯을 Unreal로 Load/Save하지 않았으며 실제 사용자 Continue·이행·cooked 호환은 이 읽기 결과에 포함하지 않는다. |
| 기존 사용자 사본 Continue | `ExistingSaveContinue.8b5bd7bf1d914b069dcfb4fea2b41908/index.json` 06:17:30의 1개·프로세스 ExitCode 0을 통과했다. 원본과 같은 바이트의 UUID 사본에 실제 메뉴 적격성/Continue delegate·공식 메모리 이행·공개 전투 복원을 적용하여 파티 4명/전투 8유닛·라운드 1/revision 1·HP/AP/SAP·원래 소유권·명령·확정 카탈로그를 비교했다. 퇴역 스킬 후보는 메모리에서 177→1로 정리되고 확정 아이템 295개는 유지됐다. 새 882×462 PNG에서 8개 머리 위 HP·몸체/전장/UI를 직접 확인했다. 원본 49파일/설정 1파일과 사본 바이트가 전후 동일하며 소유 사본/임시 파일을 정리했다. 최초 드라이버는 설정 SHA 불일치로 엔진 실행/사본 생성 전에 차단했고 알려진 원본 설정 백업 복원 후 재실행했다. 이 보호 실패 보고서도 보존한다. 원본 슬롯의 Unreal Load/Save·실제 원본 덮어쓰기·cooked 호환은 수행하지 않았다. |
| 별도 저장·게임 Continue | `Restart.Write`·`Restart.Read` 각 1개, `Continue.Write`·`Continue.Game` 각 1개 통과. 실제 uncooked 게임의 Continue → 보상 수령 → `Shop_02` 퇴장 → `Combat_02`를 확인했다. 패키징 및 Ready 전투 Actor의 임의 시점 복원 검증은 아니다. |
| 게임 창 Quit | `Game.Quit/index.json` 1개 통과·프로세스 ExitCode 0. 실제 Quit 버튼 → Unreal 자연 종료를 확인했으며 TestExit 강제 종료 플래그를 사용하지 않았다. 기존 `PackagedQuit` 이름과 별개로 uncooked `-game` 실행이다. |
| 게임 창 설정·항복 | `Game.Options/index.json` 2/2 통과. 실제 창 크기 변경의 Apply/Cancel·15초 Revert·확인 후 설정 재로드와 메뉴 항복을 확인했다. Gameplay 인벤토리의 O/Esc 후속 검수와 구분한다. |
| OS lease·명시적 재개 | `Authority.Current`의 BusyReader·ResumeReader 각 1개 통과. 시작한 Holder의 PID·시작 시각·실행 경로·고유 인자를 대조하여 해당 프로세스만 강제 종료하고 epoch 2/revision 3·Session 갱신·stale Stamp 거절을 확인했다. 당시 프로브 baseline의 `UserSavesPreserved=50`은 최종 원래 저장 계수와 구분한다. 같은 PC 파일/OS lease 검수이며 자동 Host 승계·Steam·중앙 정본 저장 성공을 뜻하지 않는다. |
| DebugCombat 표본·필터 | `PIE.Current.log`의 `AuthoredToolsAndVfx`가 03:30:22 UTC에 통과했다. 방식 7/속성 8탭·Chain 5종·보유 1종 제외·교차 필터/복원과 아군 래그돌/부활·장착 보존을 확인했다. BlazeBlast·Arrow·Slash_Katana·Link_Electric의 실제 Niagara 활성/준비·입자/시각 진행과 프레임 캡처 8장을 확인했다. 두 번째 캡처는 최초 관측 후 최소 0.1초이며 수명 중간 프레임이 아니다. 전체 60종 방향·SFX 완료를 뜻하지 않는다. |
| 1/2/4인 진행 PIE | `PIE.Current/index.json` 5/5 통과(경고 동반 성공 5·실패 0·ExitCode 0). 위 DebugCombat과 RunRound 1/2/4인·SnapshotPreparationFailure를 포함한다. 각 Run은 10전투·9상점·개인 보상·원래 소유권·Host Continue·HP/AP·저장 재로드를 확인했다. 원본 근접·비무장 DA를 저장/장착하고 작은 캡슐에는 비무장을 사용한다. 아군 HP 10,000·적 1회 타격 HP는 진행 fixture이며 제작 콘텐츠/정상 난이도는 변경하지 않았다. 이 5개를 고유 native 175개에 합산하지 않는다. |
| 몬스터 전수 | 최신 `MonsterSettling`의 `07A14C554242AEE3972CC1958B40850F`가 전체 13종·78장·경고 동반 성공 1·실패/미실행 0·191.777초·프로세스 ExitCode 0을 통과했다. 실제 AI 공격·원래 몽타주·GAS 피해/AP 1회·접근/복귀와 치명 GAS의 HP 0·래그돌·캡슐 비충돌·타일 해제·공개 Restart의 원래 4적/생존 아군/HP/AP/SAP 복원을 확인했다. 78장에서 몸체/재질/크기/방향/복귀와 초기/2.5/8초 자세·뚜렷한 지면 관통 없음까지 직접 확인했다. 2.5~8초의 8,469프레임 수치는 모두 유한하며 8초 awake 4·asleep 9, 최대 선속도 17.757cm/s·각속도 1.297rad/s다. 앞선 `D4EA013E4D2FC8223A793A92CD190A97` 13종/65장·120.765초에서는 2.5초 awake 12·asleep 1이었다. 전원 최종 정지/안착 보장이 아니며 원본 에셋과 물리를 변경하지 않았다. 아군 생존 HP만 transient fixture다. |
| 최신 메뉴 | `Menu.Full`의 최신 전체 1개가 경고 동반 성공·오류 0·프로세스 ExitCode 0을 통과했다. 네 프리뷰의 잘림을 실제 투영 카메라 보정으로 수정하고 3종 비율 8-corner/idle·즉시 생성·Edit 저장/취소·남녀 몸체/여자 원래 방향·Slate RMB 회전/캡처 해제·닫기/카메라/cache 복귀·직접 조작/Start를 확인했다. 앞선 포인터 0 fixture 실패는 실제 CursorPointerIndex로 보완했으며 제품 캡처 해제는 유지했다. 최신 4:3/21:9 네 몸체·FemaleAfterCancel/MaleRestored·ProfessionDetails/PartyAfterDetails 이미지도 실제로 보아 몸체/버튼 가시성·겹침 없음을 확인했다. 긴 직업 설명은 스크롤 영역의 의도된 clipping이다. |
| 최신 인벤토리 | 종료 가드 보완 빌드 8.90초의 최종 `InventoryUI/index.json` 07:09:07은 **4/4**을 통과했다(일반 성공 1·경고 동반 성공 3·실패/미실행/진행 중 0·38.865초·프로세스 ExitCode 0). 실제 Windows의 3종 비율에서 289개 탭/이름/가격·실제 카드/OfferId 구매·Slate LMB 개별 사본/Revision/저장과 O/Esc native 창/설정 정확 복귀·AI/사망/상점 밖 조회/거절을 확인했다. 새 27장의 탭·가격·상품·상세·상태·Options를 직접 보아 가림/겹침 없음을 확인했다. PIE 종료 요청 전에 뷰포트 참조를 유지하고 world 종료 뒤 게임 스레드의 FlushRenderingCommands/참조 해제를 확인했다. fixture는 289개·복제 1개·구매 1개의 291개이며 앞선 295/296/297개와 구분한다. `InventoryViewportGuard.FinalPreservation.json`에서 원본 저장/설정 50파일·외부 키 파일·승인 DA 세 개의 SHA 보존과 추가 Save 0을 재확인했다. PIE StopPlaySession의 plain-Esc chord만 실제 입력 동안 격리하고 원래 두 active chord를 즉시 복원했으며 저장 호출은 없다. 장비 해제/거절은 합성 NativeOnDrop·공개 요청, AI 조회/사망은 상태 fixture로 구분한다. 앞선 05:19:36·43.115초 성공은 종료 가드 보완 전 이력이다. |
| 스킬상점 UI·구매 후 시전 | `SkillShopUI.1519002d471b481992aa76394c21b67e/index.json` 06:53:02의 1개·31.350초·경고 동반 성공·실패/미실행/진행 중 0·프로세스 ExitCode 0을 통과했다(최종 fixture 빌드 11.22초). 원래 61후보/태그/가격·유효 5진열에서 근접/번개 연결/치유/보호막을 실제 카드로 1G씩 구매하여 비무장 포함 5스킬 탭·저장·4개의 독립 메뉴 Continue/Leave/Combat_02 카드의 정확 NodeId 1회와 다음 전투를 확인했다. 각 GAS 1회/AP 1·원래 시전자/피해·치유·보호막, 근접의 실제 몽타주와 DrGame의 실제 Niagara age를 통과했다. 10장의 상품/스킬 탭·검 동작·번개 source→오크·치유/보호막·완료를 직접 확인했다. 원래 HP 100/AP 2·오크 캡슐 42cm를 사용하고 치유의 현재 HP만 transient 50으로 설정했으며 모든 원본 50파일 SHA/소유 슬롯·임시 파일 정리를 통과했다. 첫 결과·고정 5진열은 공개/유효 snapshot fixture이며 물리 마우스 구매·추첨 분포·정상 난이도·청감 검수는 아니다. 근접 접근 때 HP끼리 순간 겹침이 있어 모든 표시의 무겹침을 주장하지 않는다. legacy 태그 기대·Map 카드 누락·old revision Ready의 앞선 세 fixture 실패는 고유 이력과 원본 보존 근거로 남겼다. |
| VFX 전수 실행 | 이전 `NoAudioData`·PIE 참조/CVar 정리·실제 Niagara age 진행 전 표본 문제를 보완한 `DABA01C0409DF05FE98EAC9614D743AB`가 전체 87회·계획/완료 PNG 277장·실패/미실행/진행 중 0·프로세스 ExitCode 0을 통과했다(446.31초·NavMesh maxTiles 경고 동반 성공 1). 60 DrGame·기본 공격 2개를 포함하며 체인 25사례/75장·투사체 18종·대표 참격의 실제 대상/방향을 확인했다. Spawn_Ninja_Root·Line_Lava·FeudFang은 충분한 진행 화면을 추가 확인한다. WAV 85개의 클리핑은 0이며 PoisonCarousel 1개 무음을 조사 중이다. 기본 공격 2개는 녹음하지 않았으며 master mix 신호 분석은 개별 SFX 청감 품질 검증과 구분한다. 이 87회는 18.50초 빌드에서 실행했고 공통 종료 경로의 viewport guard는 후속 최종 빌드·Environment/Monster 실행으로 확인한다. |
| VFX 후속 비교 | 최종 빌드의 `26908734477E173FDD7DBC965AD98031`은 7회·계획 30장 중 실제 29장/예열 전 Ninja 0.08초 표본 이월 1장·경고 동반 성공 1·실패/미실행 0·53.200초·프로세스 ExitCode 0을 통과했다. 실제 29장에서 FeudFang의 0.3/0.6초 source→target 성장을 확인했으나 Ninja는 0.12/0.3/0.6초에도 표현이 없고 Line_Lava는 시전자 기둥만 보여 조사한다. WAV 7개 중 Poison 기본/청취자만 이동은 무음, 카메라만 근거리/카메라와 청취자 근거리 두 조건은 비무음이었다(클리핑 0). 카메라 거리와의 관련을 지지하는 대조이며 제작 SFX 청감 품질과 두 효과의 가시성 완료를 뜻하지 않는다. |
| VFX 프로젝트 DA 보완 | Line_Lava의 StepDistance 250 덮어쓰기만 제거하여 원본 기본 1400·5회/0.3초 방출을 사용하고, PoisonCarousel은 내부 User.AudioOn=false와 동일 원본 Cue의 외부 단일 발동을 연결했다. Ninja는 프로젝트 효과 위치만 30cm 올려 translation -90→-60으로 지면에 가려진 초기 연기를 보완했다. 세 기존 프로젝트 DA의 작성·독립 재로드·전체 생성기 무저장 검수 60스킬/61후보·created/updated 0·오류/경고 0을 통과했다. 원본 Niagara/Cue/Wave는 보존하고 `ApprovedContentChanges.json`에 세 전후 SHA를 기록했다. 최종 Content 19,807개 중 의도된 세 변경과 나머지 19,804개 크기/SHA256 보존을 통과했다. 앞선 87회/277장은 이 보완 전의 근거다. |
| VFX 보완 후 비교 | `D5C237DB4A0CFF52693C01B61A554914`의 `VfxFocused/index.json` 06:27:44는 7회·계획 31장/실제 30장/예열 전 Ninja 0.08초 표본 이월 1장·경고 동반 성공 1·실패/미실행 0·55.940초·프로세스 ExitCode 0을 통과했다. Line_Lava의 다섯 분리 기둥/source→target 진행·FeudFang 성장과 높이 보완한 Ninja의 초기 희미한 연기를 관측했다. WAV 7개 모두 비무음·클리핑 0이며 Poison 네 조건의 세계 사운드는 최대 한 개였다. 원본 Cue 길이 2.14308초/Cue pitch 1.2의 기대 재생 1.78590초를 실제 오디오 스레드의 active→absent 전이/두 부재 표본과 실제 submix 꼬리로 확인했다. 네 조건 모두 자연 종료 true·마지막 0.2초 RMS 0이며 개별 청감 품질 검증은 아니다. 앞선 `32E0F64D`의 7회/30장에서는 녹음 구간이 짧아 Cue 완주 근거로 사용하지 않았다. |
| Ninja 격리 높이 비교 | `7199168B4C4CCE81F0C6F694EF28CFED`의 `VfxNinjaVisibility/index.json` 06:16:27는 원본/일시적 visual Z +30cm/User.HeightOffset=20의 3회·계획 18장/실제 15장/0.08초 표본 이월 3장·경고 동반 성공 1·실패/미실행 0·53.834초·프로세스 ExitCode 0을 통과했다. 전체 입력/해석 프로필의 단일 높이 필드 복원 비교와 원본 보존을 확인했으며 원본 DA를 수정하지 않았다. 첫 fixture의 일시적 PrimaryAssetId 비교 실패는 별도 이력으로 보존했다. 입자/실행 성공을 실제 가시성 완료로 확대하지 않는다. |
| CSV 정리 | 첫 cook는 삭제 VFX 무기 6개 참조 오류를 확인한 뒤 소유 PID·실행 경로·Cook 인자·로그를 대조하여 중단했고 UAT는 ExitCode 25로 종료했다. 삭제 명세와 파일 부재가 확인된 6행만 제외하여 새 카탈로그를 289개·진열 5개로 정리했다. 장착 프로필 49개·보관용 240개와 남은 경로·행 바이트/순서·표시명/가격·BOM/CRLF 정적 검사 및 최신 전체 175회귀를 통과했다. 원본 Content·기존 확정 295개 저장을 다시 작성하지 않으며 이전 295개 실행은 당시 이력이다. |
| Cook·패키지 콘텐츠 | HP와 세 프로젝트 DA를 포함한 최신 BuildCookRun 80.10초·UAT ExitCode 0을 통과했다. `Package.Csv.dbbb6e07` 06:55:00의 2/2·실패 0·0.092초·프로세스 ExitCode 0에서 cooked=1의 289개 CSV/모든 경로와 61스킬/세 보완 DA 실제 로드를 확인했다. 동일 exe SHA256 `BA50B569F39ED90D6B3195C2C8B4458A88B2FD52EFB797CB5F6C916DE7A49E9D`·build 로그·5컨테이너에 후속 검수를 연결했다. 앞선 `a0372e6a`는 HP/DA 보완 전 이력이다. 원본 의존성 Display 중 두 PreviewMesh는 editor-only 메타데이터이며 IceSpike의 기본 M_default 슬롯은 실제 참조지만 사용하는 Hail Niagara가 기존 IceCycle MI로 덮어쓴다. 외부 원본은 변경하지 않았다. |
| 최신 패키지 Continue·Quit | 같은 cooked 바이너리의 `Package.CookedWrite.12598320`이 203,452바이트의 격리 저장을 작성하고 `Package.Continue.e4f658d3`가 동일 SHA256 `00028BE95241B223827D725494F5AA3E03FAF06D5C842A3D0A39EDFCBFAA8154`를 실제 Result → 보상 수령 → `Shop_02` 퇴장 → `Combat_02`로 재개한 뒤 소유 슬롯을 정리했다. `Package.Quit.47ce4288`도 실제 버튼/자동화 Success 완료·RequestExit(0)·자연 Exiting·프로세스 ExitCode 0을 통과했다. 네 실행은 같은 바이너리/컨테이너·cooked=1·수정된 세 프로필의 로드 근거를 공유한다. 앞선 Write의 JSON UTC 소수 끝 0 차이는 엔진 실행 전 차단 이력이며 정확한 UTC ticks 비교 후 재실행했다. 자연 Quit의 종료 전 JSON 미발행은 정확한 닫힌 완료 로그로 판정한다. 수정 전 `09ef7c9a`/`62c4ff2f`/`e9f2c978`과 Editor writer→cooked의 FText 크기 차이 실패는 별도 이력이다. 기존 사용자 저장은 이 패키지 검수에 사용하지 않았다. |

초기 RunRound 진행 fixture는 작은 늑대 캡슐과 원본 검의 물리 비접촉 때문에 네 적 사망 기대를 만족하지 못했다. 전체 Attack/Recovery 388표본의 최소 간격 12.388cm·접촉 0으로 타이밍 변경의 근거가 없음을 확인했다. 원본 충돌 반경·캡슐·피해를 유지하고 진행 fixture만 원본 근접·비무장 두 DA를 실제 저장/장착해 적 기하에 맞는 공격을 사용하도록 보완했다. HP 10,000과 적 1회 타격 조건은 격리된 진행 fixture이며 정상 난이도·생산 밸런스 변경이 아니다.

근거는 `Saved/Automation/TodoReview/`의 각 `index.json`·로그·`Authority.Current/Verification.json`과 실제 캡처에 보존한다. `Preservation.Final.json` 06:58:27·프로세스 ExitCode 0에서 `BeforeUserState.json`의 **원본 저장 49파일·GameUserSettings 1파일**과 외부 Editor 키 설정 SHA256을 보존하고 Content 19,804개의 크기/SHA256 동일 및 승인 DA 세 개의 정확한 변경을 확인했다. baseline/Content 불일치·예상 밖 Content·추가 Save는 0이며 소유 Authority probe 세 개를 정리했다. 모든 UE/게임/Visual Studio 프로세스가 종료된 상태다. 해당 콘텐츠·패키지 완료 기록은 이 절로 통합했으며 남은 확인만 TODO에 유지한다. 전체 장비 UI·최종 가독성/밝기·Ninja 표현 선호·개별 SFX 청감·변경 전후 FPS·Editor 저장의 패키지 호환·다른 PC 및 Steam/PlayFab·경쟁 검증은 완료로 확대하지 않는다.

2026-10-06 TODO 이관 시 보존한 당시 실행 조건이다. 아래 명령은 재실행 승인이 아니며 최신 코드 검증으로 간주하지 않는다.

별도 읽기 검수 조건: [ExistingSaveCopyReviewTests.cpp](../Source/ProjectAEditor/Tests/ExistingSaveCopyReviewTests.cpp)의 `ProjectA.TodoReview.ExistingSaveCopy`는 외부에서 원본 SHA256과 UUID 사본을 준비하고 `-ProjectAExistingSaveCopySlot`·`-ProjectAExistingSaveCopyBytes`·`-ProjectAExistingSaveCopySHA256`을 모두 제공해야 한다. 실제 Continue·이행·cooked 호환과 구분한 결과는 위 원본 저장 사본 읽기 행에 보존한다.

실제 사본 Continue 조건: [ExistingSaveContinueReviewTests.cpp](../Source/ProjectAEditor/Tests/ExistingSaveContinueReviewTests.cpp)의 `ProjectA.TodoReview.ExistingSaveContinue`는 별도 UUID 사본과 `-ProjectASaveSlot`·`-ProjectAExistingSaveContinueSlot`·`-ProjectAExistingSaveContinueBytes`·`-ProjectAExistingSaveContinueSHA256`을 요구한다. 원본·설정 보호 실패와 후속 복원·재실행, 실제 결과·cooked 제외 범위는 위 기존 사용자 사본 Continue 행에 보존한다.

추가 구매 검수 조건: [SkillShopUiPIETests.cpp](../Source/ProjectAEditor/Tests/SkillShopUiPIETests.cpp)의 `ProjectA.TodoReview.SkillShopUI`에는 같은 새 `ProjectA_Automation_SkillShop_<32hex>`를 `-ProjectASaveSlot`과 `-ProjectASkillShopReviewSlot`에 명시한다. 첫 결과·유효 5진열·치유용 현재 HP만 격리한 fixture이며 실제 결과와 제한은 위 스킬상점 UI·구매 후 시전 행에 보존한다.

### 9-18 2026-10-04 목표 Run과 위임 후속 검수

사용자가 제안 6~14의 권장 조합(제안 9는 선택 2, 나머지는 선택 1)과 개발 시험 수치를 채택·위임했다. 기본 새 싱글 Run을 인카운터 60회·PvE 10회·로컬 Snapshot 10회의 80단계로 연결하고 편성·성장·보상을 저장 시 고정했다. 기존 2/10전투 저장과 명시적 `-ProjectAPrototypeRun`, 로컬 Co-op 프로토타입은 유지한다.

회복 소모품은 GAS로 HP 25/AP 1·시작 1개·구매 1G를 적용하며 일반 스킬 5칸과 분리했다. 상점 회복은 HP 25/1G, 부활은 성장 후 최대 HP의 25%/1G이고 소유권·Ready·수량·저장 실패 경계를 검증했다. 정식 회복 DA 작성과 독립 재로드를 통과했다. Steam 480은 기본 비활성인 명시적 개발 옵션으로 세션·초대·공식 SteamAuth 결과를 연결한 접속 시제품이며, 실제 2PC/계정 및 PlayFab 검증은 준비되지 않았다([멀티플레이 계약](MULTIPLAYER.md)).

근거는 `Saved/Automation/TodoCompletion_20261004/`에 보존한다. Development Editor/Win64의 15.76초 빌드 뒤 회복 DA cook 포함만 보완한 최종 `Build.FinalCookDependency.log`가 15.09초에 성공했다. 아래 실행은 cook 보완 직전 런타임의 각 코드 시점·범위로 구분하며 추가 패키지 실행은 보류한다.

| 검수 | 확인 결과와 한계 |
| --- | --- |
| 네이티브 회귀 | `NativeFull.e94784e56b48478380b45922bb0e99dd` **182/182**, 후속 `TargetTransactions.316596dd874f4a84a9423063717ed01a` **3/3**을 통과했다. 후속 고유 추가는 1개로 합계 183고유 회귀이며 단일 전체 183회 실행은 아니다. |
| 전체 진행·영속 트랜잭션 | 후속 합성 승리 회귀에서 실제 RunState 공개 경로의 60선택·20결과·170회 재로드, 마지막 저장 실패/재시도, PvE만 성장·골드 지급, Snapshot 무보상, 최종 완료·재개 거절을 확인했다. 전투 승리를 명시적으로 주입한 저장 회귀이며 정상 전투 완주를 뜻하지 않는다. |
| 정상 목표 Run | `NormalTarget.b7f6dd5d427e4d1fa87546d05cea39ec`는 원래 HP·피해·AP·재화로 직접 조작 1명/AI 3명을 진행해 **5/20승리·18/60선택 후 6번째 전투(Snapshot)에서 자연 전멸**했다. 84저장 경계 일치와 실제 PIE 종료→메뉴 Continue를 통과했고 패배 화면을 확인했다. 정상 완주·밸런스 승인으로 표시하지 않는다. |
| 기존 저장 Continue | 최신 `ExistingContinue.d68163b2c85346509e39af4531ef13da`는 원본 바이트의 UUID 사본에서 실제 Continue·공식 메모리 내 마이그레이션·파티 4명/유닛 8개·라운드 복원을 통과했다. 자연 렌더 준비 후 캡처했으며 원본 슬롯을 Unreal로 다시 저장하지 않았다. |
| 체인 방향·연결·SFX | `AuthoredChains.f5ba6e922e4944b0b4dfa2fc6638ee8e` **25사례·200PNG·25WAV/132.225초**를 통과했다. 5종×(4방향+LWC)의 후반 25장 전부에서 시전자→4대상 연결·방향·가시성을 직접 확인했고 원본 DA·원래 시전자·AP 1회·4피격·각 구간 이동 대상 추종을 대조했다. WAV는 모두 비무음·클리핑 0이며 개별 청감 품질을 뜻하지 않는다. |
| 몬스터 최종 안착 | `RagdollFinal.d4b67380eb78483abdea7f0856794adf` **13종·91PNG/260.151초**를 통과했다. 원래 물리의 모든 몸체가 자연 프레임에서 연속 3초 이상 수면에 도달했고 최종 판정 시점은 최대 28.173초였다. 강제 수면·속도 초기화·물리 변경은 없다. |
| 스킬상점 후속 | `SkillShopUI.f04079bc6bab4e66bd74d45413974864`는 **1개 경고 동반 성공·실패 0/32.441초**다. 61후보·4구매·4개 독립 Continue·다음 전투 GAS/AP와 10PNG를 확인했다. 실제 4대상 체인의 후반 연결도 보이며 일부 캡처의 직전 단계 UI 잔상은 다음 라운드 화면 전환 근거로 사용하지 않는다. |
| 현재 환경 성능·화면 | 최종 `EnvironmentPerformance.dd675c5182c440e79629c8b8c90ef9d8`의 14맵 CSV/JSON은 16,740프레임·140.069초이며 원시값 재계산이 일치했다. 맵별 프레임 간격 평균 8.336~8.546ms·p95 9.642~10.652ms, GPU 평균 2.782~4.814ms, 33.333ms 초과 2표본·최대 39.197ms다. 경로 PNG 56장 직접 검수·별도 비율 42장 파일 검사. 동시 다른 게임과 Editor 부하가 있으며 표시 완료 시간·변경 전후 개선은 미측정이다. |
| 환경 이동 미완료 | 최종 전체는 **7성공·7실패/331.136초**다. DarkMarsh·DungeonStone·FrozenPass·IceCitadel·PineRidge·SavannahGrove·SunsetLagoon은 실제 trace 뒤 Slate LMB의 단일 예약·무이동·SAP 무차감 검증에서 실패했다. 하위 실패 원인은 미확정이며 성공 7맵의 SAP 완료를 전체에 적용하지 않는다. 사용자 보류 요청에 따라 추가 수정·실행 없이 TODO에 남겼다. |

체인 작성기의 중첩 구조체 반영과 후반 캡처를 보완했으며 앞선 100PNG 결과만으로 전체 연결 가시성을 완료 처리하지 않았다. 기존 Continue의 앞선 `82607d50` 실행 성공은 렌더 준비 전 캡처와 구분한다. 성능 검수의 최초 14실패는 fixture `Stage 23` 충돌로 DeathPaint 창 이동 경로에 진입한 원인이며 `Stage 25`로 분리한 최종 결과는 위 표와 같다. 실패·보완 전 근거는 삭제하지 않았다.

실제 온라인 협동·Steam 초대·PlayFab 중앙 저장·경쟁 결과 검증, 정상 난이도 80단계 완주와 개별 SFX 청감은 이 결과에 포함하지 않는다. 9-17절의 이전 코드·콘텐츠·패키지 결과를 이번 변경의 최신 검증으로 대체하지 않는다.

마무리 정적 검토에서 회복 DA의 런타임 문자열 참조가 cook에 누락될 수 있어 `ProjectAAssetManager`에 해당 패키지 한 개의 존재·제외 여부 검사와 포함을 추가했다. 2026-10-04 사용자의 현재 작업 완료·나머지 보류 요청에 따라 재패키징·추가 플레이·서비스 연동은 [TODO](TODO.md)에 보류했다.

최종 `ProtectedAfter.json`·`UserProtection.Final.json`은 Content 19,806개 중 기존 19,800개 크기/SHA256 동일·체인 DA 5개 승인 변경·회복 DA 1개 추가와 원본 저장/설정 52파일 보존을 통과했다. 검수로 변경된 Editor 환경설정을 원래 바이트로 복원하고 소유 Continue 사본 2개만 정리했으며 추가 Save는 0이다. Unreal 프로세스는 모두 종료했고 Editor와 Visual Studio를 다시 열지 않았다.

TODO 12·23·24·25 이관 보충: 당시 체인 승인값은 첫 대상 포함 4명·600cm·0.15초·점프마다 80%이며 피해는 25→20→16→12.8이었다. 2026-10-06의 0.4초·이전/최종 연결 제거 검증으로 확대하지 않는다. native 182/182의 세부는 일반 115·경고 동반 67·실패/미실행 0·15.063초다. 자연 래그돌 표본 16,136개에서 13종 모두 3초 연속 수면에 도달했고 최종 관측은 8.009~28.173초였다. 8초 시점 awake 6종도 이후 수면에 도달했으며 최종 13PNG를 직접 확인했다. 일반 곰 표현 선호는 미완료다.

당시 환경 98PNG의 무결성과 경로 56장의 실제 화면을 확인했으며 별도 비율 42장은 이 실행에서 직접 판정하지 않았다. 50ms 초과 표본은 0이다. Core/MainMenu의 새 게임·기존 저장 사본 Continue에서 Core/Gameplay 진입·파티 4명/유닛 8개·자원/점유·정상 화면과 원본 보존을 확인했다. 맵 후속 성공과 42장 직접 검수는 9-19절의 별도 실행이다.

### 9-19 2026-10-04 TODO 재개 검수와 저장·제목 보완

사용자의 재개 요청에 따라 기존 위임 범위의 왼쪽 모니터 검수를 진행했다. cooked 목표 Run의 저장에서 임시 경로 261자가 직접 Win32 호출에 실패하는 문제를 확인하고 Unreal과 같은 절대화·구분자·긴 경로 처리를 적용했다. 원자 교체·배타 핸들·확인 토큰 계약은 유지한다. 상점 제목의 양옆 패널 침범은 이름·완료 수의 짧은 표시와 가용 폭 줄바꿈으로 수정하고, 비활성 전투 phase의 진행 문구 누적을 막았다.

근거는 `Saved/Automation/TodoResume_20261004/`이며 상세 수용 범위는 이 절에 보존한다. 남은 검수는 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완)을 따른다. 최종 Development Editor 및 Game / Win64 컴파일 13.31초, 기존 cook을 재사용한 패키징 62.54초를 통과했다. 전체 cook은 저장 보완 후 142.97초에 성공했으며 최종 패키지의 컨테이너 5개가 동일함을 SHA로 확인했다.

| 검수 | 확인 결과와 한계 |
|---|---|
| 전체 native | `NativeFull.e06e7e3923b741e6ab78a219a051d3b3` 단일 실행 **183/183**, 일반 116·경고 동반 67·실패/미실행 0·15.391초·종료 0 |
| 패키지 | `PackageFixed`에서 CSV/프로필 2개·새 목표 저장·독립 실제 메뉴 Continue·긴 경로 저장 각 1개와 실제 Quit를 통과했다. 최종 `PackageFinal2`에서 저장/Continue 각 1개와 Quit를 다시 확인했다. 회복 DA·후반 PvE/Snapshot 의존성을 실제 로드하고 저장 바이트·manifest·자연 종료·소유 파일 정리를 대조했다. 최종 312자/임시 349자 경로의 실제 저장·교체·실패 보존·토큰 삭제를 확인했으며 UNC 실행은 별도다 |
| 환경 | `EnvironmentPerformance.97af6eebbfa941019b413244799a290a` **14/14**·328.791초. 자연 PIE 프레임의 Slate down/up·viewport 수신·단일 타일 선택·서버 예약·Ready 후 SAP 1회 차감을 관측했다. 3비율 42장 직접 검수와 16,808프레임/140.061초의 원시 성능 재계산을 통과했다. 이전 7실패의 정확한 원인은 확정하지 않았으며 제품 입력 변경·변경 전후 FPS 비교는 없다 |
| 정상 Run·서비스 | 최종 `NormalTarget.e5c9d0c855134838a68f054448b08009` 75.372초·오류 0, **6승·21선택 후 7번째 전투의 자연 패배**·15라운드·107저장 경계·첫 승리 뒤 메뉴 Continue를 확인했다. 실제 회복약 UI 6회에서 동일 GAS context HP +25/AP 1/재고 -1, 서비스 UI 14회에서 소모품 5·전체 회복 4·25HP 회복 4·자연 사망 후 부활 1회(0→28.75)를 확인했다. 물리적 마우스 입력·소모 후 메뉴 Continue·80단계 완주·최종 밸런스는 별도다 |
| 화면 | 제목 보완 뒤 `ac477950`의 서비스 14장, 최종 `e5c9d0c8`의 대표 서비스·회복약·패배 9장에서 제목 겹침 해소와 상태를 확인했다. 이전 `4357ceb4`의 정상 Run 29장은 해당 시점의 전체 화면 검수이며 최종 29장 전부를 다시 판정한 결과가 아니다 |
| 원본 보호 | 검수 전후 Content **19,806개 전체** 크기/SHA 동일·추가/삭제 0. 검수로 바뀐 Editor 설정을 복원하고 원본 저장·설정 **52파일** 동일·추가 Save 0. Unreal 검수 프로세스 모두 종료 |

초기 패키지 드라이버는 GUI 프로세스 종료 대기가 빠져 로그 판정을 일찍 수행했으므로 실제 프로세스 대기와 정확한 test 경로/개수 대조로 보완했다. 초기 긴 경로 저장 실패와 정상 Run fixture의 서비스 화면 전환·라운드 표시 갱신 대기 실패를 보존했다. 실제 UI가 준비된 뒤 한 번 요청하도록 fixture를 고쳤으며 제품 능력치·AI 권한·승패를 변경하지 않았다. 보고서 HTML 템플릿 오류·TSR 알림·Recast 경고는 성공한 assertion과 구분한다.

Steam 다중 PC·PlayFab·중앙 저장·관전·온라인 Snapshot·결과 검증·MMR은 시험 환경과 미정 정책 준비가 필요하다. 정상 80단계 완주, 변경 전후 성능 기준 비교 및 개별 SFX·Ninja·맵 밝기·일반 곰 사망 표현의 최종 선호는 완료 처리하지 않았다.

TODO 11·26 이관 보충: 맵별 `<Map>_sap_input.json`에 실제 커서·focus·서버 예약 경계를 보존했다. 14맵 성능은 평균 8.330~8.348ms·p95 8.926~9.268ms, GPU 평균 2.234~3.582ms, 최대 34.039ms·33.333ms 초과 1회·50ms 초과 0회다. 변경 전후 향상률이나 표시 완료 FPS는 아니다. 패키지에서 PvE 클래스 10종·Snapshot 클래스 4종을 로드하고 같은 저장 바이트의 3선택→첫 전투 4아군/1늑대·재고·체크포인트를 복원했다. 최종 manifest 7파일·컨테이너 5개 동일과 소유 sav/tmp 잔여 0을 확인했다.

패키지 재검수: `ProjectA.TodoReview.PackagedCsv+ProjectA.TodoReview.PackagedSkillProfiles`, `ProjectA.Package.TargetWrite`→별도 프로세스의 `ProjectA.Package.TargetContinue`, `ProjectA.Package.LongCheckpoint`, `ProjectA.Menu.PackagedQuit`를 사용한다. 새 32자리 UUID의 `-ProjectAPackagedTargetId`와 같은 `-ProjectASaveSlot=ProjectA_Automation_TargetPackage_<UUID>`, 격리 `-UserDir`, `-ProjectAReviewLeftMonitor`가 필요하며 LongCheckpoint는 최종 저장 경로가 260자 이상이어야 한다. 원래 사용자 슬롯은 사용하지 않는다.

### 9-20 2026-10-06 TODO 완료 기록 정리

이관 기준은 `694a3465`의 TODO다. 이번 작업은 문서 정리이며 새로운 컴파일·게임·자동화 검증을 수행하지 않았다. 과거 완료 기록과 이미 존재하는 근거는 9-17~19절에 통합하고 누락된 실행 명령·격리 인자·측정값을 보충했다.

| 기존 TODO | 이관 위치와 남은 범위 |
|---|---|
| 7·8·9·13·15·16·17·18·20·21 | 완료 검수는 9-17절. 7절의 다른 PC 대기는 TODO 4절에 통합 |
| 11 | 완료 환경 검수는 9-19절. 변경 전후 성능·최종 선호는 TODO 14절 |
| 12·24·25 | 완료 몬스터·맵 진입·목표 Run 후속은 9-18절. 일반 곰 표현은 TODO 12절, 외부 준비는 4절 |
| 10·14·19·22·23 | 완료 부분은 9-17~19절. 분위기·밝기·성능·SFX/Ninja·최신 체인은 TODO 10·14·19·23절에 유지. 22절의 중복 미완료는 19절로 통합 |
| 26 | 완료 결과·패키지 명령은 9-19절. 정상 완주·소모 후 메뉴 Continue는 TODO 26절 |

제안 6~14의 선택 9개, 무기 기획 제안 15~19의 미선택 상태, 최신 디버그·속도 확인 27·28절은 보존했다. 저항 폐기 결정은 최근 변경과 GAME_DESIGN의 기존 확정 범위를 유지한다. 완료 절의 참조는 HISTORY로, 미완료 참조는 남은 TODO로 갱신한다.


### 9-21 2026-10-06 스킬 소지 상한 제거

TODO 29절의 확정 정책 중 스킬 소지 상한 제거를 구현했다. `UnitDataRules`·유닛 설정·서버 구매·Run 저장·Snapshot·체크포인트·디버그 편집과 UI에서 5개 제한을 제거했다. 경로별 최소 필요 스킬 수·ID/별칭 중복·태그/유효성·소유권·구매 비용·저장 원자성은 유지한다. 일반 스킬 목록과 분리하여 소모품 종류는 `RunRecoveryRules::MaximumStackTypes=5`, 상점 진열은 최대 5개를 유지한다.

6개 이상 스킬의 구매·유닛/디버그 반영·저장 및 Continue·저장 실패 보존을 확인하는 회귀 소스를 보완했다. UE 5.8.3 `ProjectAEditor Win64 Development -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 테스트 소스를 포함한 Runtime/Editor 모듈의 31개 작업·41.96초로 성공했다. 근거는 `Saved/Logs/UnlimitedSkillsBuild.log`다. 게임·PIE·자동화 테스트는 실행하지 않았다. 실제 스크롤·선택·메뉴 Continue는 [TODO 29절](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)에 남긴다. 이 상한 제거분의 검증 시점에는 스킬상점 폐지·무기 랜덤 스킬·등급·기존 Run 전환을 구현하지 않았다. 이후 같은 날 사용자가 제안 15~19의 선택 1을 채택한 기록은 [9-23절](#9-23-2026-10-06-무기-스킬-정책-선택)에 구분한다.

### 9-22 2026-10-06 제안 6과 8 확정 내용 이관

2026-10-06 이관 기록이다. 아래 확정 선택은 2026-10-04 사용자 채택이며 당시 구현·검수 근거는 [9-18절](#9-18-2026-10-04-목표-run과-위임-후속-검수)·[9-19절](#9-19-2026-10-04-todo-재개-검수와-저장제목-보완)에 통합되어 있다. 이관 자체는 새 작동 검증이 아니다. 정상 80단계 완주·소모 후 실제 메뉴 Continue·최종 밸런스는 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완), 온라인 PvP·서비스 준비는 [TODO 4절](TODO.md#4-온라인-협동과-경쟁)에 남긴다.

#### 9-22-1 제안 6 Run HP·사망·소모품

2026-10-04 구현·당시 검수 상태: 새 목표 싱글 Run에 누적 HP·사망 슬롯, 전투 회복 소모품, 회복·부활 인카운터와 저장을 구현했다. 회복약 HP 25/AP 1/시작 1개/1G, 회복소 HP 25/1G, 부활 최대 HP 25%/1G는 사용자가 설계·적용을 위임한 개발 시험값이다. 네이티브 검사의 실제 GAS 1회·발동 전 사망 취소·권한/태그/만피/재고 거절·저장 실패/중복 revision·재로드를 통과했다. 정상 Run에서 실제 UI를 통한 회복약 6회·회복 서비스·자연 사망 후 부활을 확인했으며 범위는 [9-19절](#9-19-2026-10-04-todo-재개-검수와-저장제목-보완)을 따른다. 최종 밸런스·80단계 완주는 별도다.

- [x] 선택 1: 누적 HP와 사망 슬롯을 Run 동안 유지하고 생존한 본인 캐릭터가 전투에서 회복 소모품을 사용한다. 사망자는 부활 인카운터에서만 복귀한다.
- [ ] 선택 2: 누적 HP·사망 보존은 같고 소모품은 비전투 인카운터에서만 사용한다. 전투 중 회복 수단은 스킬로 제한한다.

권장: 선택 1. 소모품을 전투 선택에 연결하되 서버의 소유권·생존·사용량 검증이 추가된다. 선택 후 사용 비용·회복량·부활 상태를 데이터로 정의하고, 사용과 수량·HP 저장을 함께 확정한다. 수용 조건은 중복 요청의 중복 소모 방지, 저장 실패 시 원상 유지, 재개 후 HP·사망·잔여 수량 일치다.

#### 9-22-2 제안 8 목표 Run의 풀·편성·성장

2026-10-04 구현·당시 검수 상태: 새 싱글 Run의 60선택·10PvE·10로컬 Snapshot 경로와 고정 태그 후보·편성·성장/보상 데이터를 구현했다. 10묶음 시험값은 [현재 구조](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)를 따른다. 순서·횟수·저장 직렬화 회귀를 통과했으며 기존 2/10전투 저장과 개발 협동은 소급 확장하지 않는다. 실제 정상 Run 및 온라인 PvP 검수는 별도다.

- [x] 선택 1: 진행 묶음별 허용 태그 풀·고정 PvE 편성·성장/보상 표를 데이터로 지정한다. 기존 지원 콘텐츠와 로컬 Snapshot으로 순서·저장을 먼저 검증한 뒤 온라인 PvP를 연결한다.
- [ ] 선택 2: 단계와 콘텐츠 태그 조건·가중치로 인카운터·PvE 편성을 추첨하고 단계별 성장 곡선을 적용한다. 추첨·중복·가중치 결합 규칙까지 정한 뒤 목표 Run에 연결한다.

권장: 선택 1. 초기 검증에서 콘텐츠 추첨과 난이도 변화를 분리할 수 있다. 선택 후 풀·편성·성장 표의 실제 값과 PvP 선행 조건을 확정하며 시험 가격·보상을 최종 수치로 사용하지 않는다. 수용 조건은 인카운터부터 시작해 회차마다 후보 3개 중 하나를 선택하고 정상 완주 순서·횟수와 재개 위치·확정 선택을 유지하는 것이다. 온라인 PvP 완료는 제안 9~14의 실제 연동과 검증 후 기록한다.

### 9-23 2026-10-06 무기 스킬 정책 선택

사용자가 제안 15~19의 선택 1을 모두 채택하고 추가 세부 결정도 권장안으로 처리하도록 위임했다. 무기당 스킬 1개·5등급 동일 추첨 가중치·가격 1G·비무장 기본 공격 유지·시작 무기 생성 시 부여를 개발 시험값으로 채택했다. 등급명은 색 이름을 사용하며 최종 밸런스·등급별 위력 상승을 보장하는 수치와 구분한다. 아래는 TODO에서 이관한 선택지·이유·수용 조건이다. 선택 및 소스 구현과 실제 사용자 작동 확인은 구분한다.

#### 9-23-1 제안 15 무기 스킬 사용권

선택 기록: 2026-10-06 선택 1 채택. 장착한 무기 사본의 사용권을 부여·회수하는 정책을 확정했다. 위임된 시험값으로 비무장 공격을 유지하고 시작 무기는 생성 시 스킬을 부여한다.

- [x] 선택 1: 장착한 무기의 스킬만 사용한다. 무기를 해제하면 해당 사본의 사용권을 회수하고 다시 장착하면 복원한다.
- [ ] 선택 2: 인벤토리에 보유한 모든 무기의 스킬을 사용한다. 무기 장착 여부와 사용권을 분리한다.

채택: 선택 1. 무기 교체와 스킬 구성이 직접 연결되고 목록이 불필요하게 커지지 않는다. 소지 개수 상한이나 별도 5칸 장착 제한은 두지 않는다. 위임된 비무장 기본 공격 유지·시작 무기 생성 시 부여 규칙과 장착 사용권을 구현한다. 수용 조건은 장착/해제·저장 실패·이어하기에서 아이템별 권한이 일치하고 다수 스킬 UI가 잘리지 않는 것이다.

#### 9-23-2 제안 16 등급별 차이

선택 기록: 2026-10-06 선택 1 채택. 모든 등급의 부여 개수는 같고 높은 등급은 허용 스킬 후보가 달라진다. 위임된 시험값은 무기당 1개·5등급 동일 가중치·가격 1G이며 등급명은 색 이름을 사용한다. 후보 데이터와 구현 결과는 아래 9-24절을 따른다.

- [x] 선택 1: 모든 등급의 무기에 같은 수의 스킬을 부여하고, 높은 등급은 부여 가능한 스킬 후보가 달라진다.
- [ ] 선택 2: 높은 등급일수록 부여 스킬 수를 늘린다. 단계별 개수 표와 후보를 별도로 정한다.
- [ ] 선택 3: 부여 개수·후보는 같고 높은 등급은 스킬 수치 보정만 달라진다.

채택: 선택 1. 등급별 효과를 후보 데이터로 관리하여 스킬 조합 수와 개별 수치 보정 부담을 줄인다. 위임된 시험값과 등급별 후보 데이터를 반영하며 서로 다른 등급의 콘텐츠가 준비되기 전에는 차이가 구현됐다고 표시하지 않는다. 수용 조건은 색상과 텍스트·실제 후보 규칙이 일치하고 무기 적합성 조건을 모든 등급에서 지키는 것이다.

#### 9-23-3 제안 17 무작위 부여 시점

선택 기록: 2026-10-06 선택 1 채택. 사본 생성 시 등급·스킬을 한 번 확정하고 저장하는 정책을 확정했다.

- [x] 선택 1: 상점 진열·보상 생성 등 사본 생성 시 한 번 추첨하여 결과를 미리 보여준다.
- [ ] 선택 2: 구매·보상 수령 시 한 번 추첨하고 그 전에는 후보 범위만 보여준다.

채택: 선택 1. 구매 전에 결과를 비교할 수 있고 같은 사본의 정체성이 명확하다. 후속으로 사본별 확정 결과와 생성 버전을 저장한다. 수용 조건은 구매·재장착·재접속·이어하기로 결과가 재추첨되지 않고 서버가 동일 결과를 복원하는 것이다. 추가 결정 위임에 따라 아이템 리롤은 기존 1G를 유지하며 보유 사본은 그대로 두고 진열만 새 사본으로 생성한다.

#### 9-23-4 제안 18 중복 스킬

선택 기록: 2026-10-06 선택 1 채택. 한 무기 내부 중복 금지와 다른 무기의 동일 스킬 버튼 통합·출처별 사용권을 확정했다.

- [x] 선택 1: 한 무기 안에서는 같은 스킬을 중복 부여하지 않는다. 서로 다른 무기에 붙은 같은 스킬은 사용 버튼 하나로 합치고 부여 출처를 보존한다.
- [ ] 선택 2: 한 무기 안의 중복은 막고, 다른 무기의 같은 스킬은 무기별 버튼으로 분리한다.

채택: 선택 1. 같은 행동 버튼이 반복되는 것을 줄인다. 후속으로 후보 추첨의 중복 옵션과 출처별 사용권을 구현하고 등급에 따른 동일 스킬 변형을 도입한다면 병합 기준을 별도 정한다. 수용 조건은 양손 무기 이중 부여 방지와 한 출처를 해제해도 남은 출처의 스킬 유지다. 적합한 후보가 부족하면 무관한 스킬이나 중복으로 임의 보충하지 않는다.

#### 9-23-5 제안 19 기존 Run 전환

선택 기록: 2026-10-06 선택 1 채택. 기존 Run의 상점·구매 스킬·선택 이력은 종료까지 보존하고 새 Run부터 무기 스킬 획득 규칙을 적용하도록 구현한다. 기존 저장을 새 후보로 다시 추첨하지 않는다.

- [x] 선택 1: 기존 Run은 종료까지 당시 상점·습득 스킬을 유지하고 새 Run부터 새 획득 규칙을 적용한다. 스킬 소지 상한 제거는 공통 적용한다.
- [ ] 선택 2: 기존 Run도 전환한다. 과거 선택 ID·구매 스킬·아이템·골드는 보존하고 이후 스킬상점 후보와 현재 방문을 명시적으로 이관한다.

채택: 선택 1. 진행 중 저장을 보존하며 이관 범위를 줄인다. 후속 작업은 새 후보 풀과 이전 저장 호환 경로를 분리하는 것이다. 기존 Run의 현재 스킬상점 방문은 유지한다. 수용 조건은 이전 저장 원본 보존, 선택 이력·재화·아이템 손실 없음, 새 Run에서 스킬상점과 구매 요청 차단이다.

### 9-24 2026-10-06 무기 스킬과 새 Run 전환

제안 15~19의 선택 1과 추가 결정 위임에 따라 새 일반 Target Run에 `WeaponSkillAcquisitionVersion=1`을 적용했다. 새 후보에서 스킬상점을 제외하고 해당 구매 요청을 차단하며 아이템·회복/부활·보상·장비 UI의 스킬상점 상태 의존을 분리했다. 기존 저장의 누락 값은 0으로 읽어 당시 상점·진열·습득 스킬·장비·골드·진행을 유지한다. 명시적 prototype·개발 협동 경로도 기존 규칙을 유지한다. 스킬 소지 상한 제거는 공통 적용한다.

`RunWeaponSkillRulesDataAsset`의 62개 후보는 기존 GAS 실행 태그와 별도 선택 태그·무기 Query·등급 Query를 공통 후보 추첨에 전달한다. 근거리 무기·활·석궁·마법 무기를 구분하며 방패·탄약·기타는 등급만 생성하고 스킬은 부여하지 않는다. 색 5단계의 가중치는 각각 1, 무기당 스킬 1개와 상품/리롤 1G는 시험값이다. 후보가 부족한 등급을 제외하고 전체 적합 후보가 없으면 실패하며 무관한 스킬로 보충하지 않는다.

시작 아이템·상점 진열 사본을 생성할 때 GUID·생성 버전·등급·확정 스킬을 기록하고 Run에는 당시 후보·태그 조건·가중치·등급 표시 규칙도 저장한다. 구매는 진열된 사본을 그대로 인계하고 재장착·Continue로 재추첨하지 않는다. 비무장 기본 스킬과 현재 장착 사본의 스킬을 합치며 동일 Skill ID는 한 번만 실행 목록에 넣고 각 사본의 부여 출처는 보존한다. 양손 중복 부여를 막고 한 출처를 해제해도 다른 출처가 있으면 유지한다. 구매·리롤·장착은 저장 성공 후 공개하며 실패 시 이전 메모리·파일을 유지한다.

상점·인벤토리·장비는 저장된 등급 색/텍스트와 확정 스킬을 표시하고 스킬 탭은 현재 장착 사본의 이름·슬롯 출처를 보여준다. 클라이언트 표시에는 등급 메타데이터를 전달한다. 장착 목록은 전투·체크포인트 경로에 연결했다. Snapshot에는 스킬 개수 상한 제거만 별도 적용했으며 새 무기 스킬의 상대 카탈로그 등록·사본 공개/내보내기·온라인 검증은 구현 범위에 포함하지 않는다. 새 Target Run은 기존 고정 Snapshot 상대를 사용한다.

`CreateWeaponSkills.py`로 프로젝트 전용 `DA_MeleeAttack`·`DA_CrossbowAttack`·`DA_WeaponSkillRules` 3개를 작성하고 `DA_VerticalSliceParty`에는 규칙 참조를 추가했다. 새 파일은 각각 3,372B·4,257B·51,609B로 총 59,238B이며 원본 FX·메시를 복제하지 않았다. 근접 공격·화살 원본 2개의 SHA256은 유지됐다. 새 근접 DA는 공통 근접 충돌을 사용하도록 `bUseWeaponTrace=false`, `bUseMeleeAreaCollision=false`와 비어 있는 검 소켓 필드를 저장하고 기존 몽타주/직업 재매핑을 재사용한다.

최종 재작성은 정상 종료 코드 0, `Saved/Automation/WeaponSkills/Author.json`은 후보 62개·등급 5개를 기록했고 Python 구문 검사를 통과했다. 작성 로그는 `Saved/Logs/WeaponSkillsAuthor.log`다. 엔진 초기화의 `LogAutomationTest` 조건 실패 15줄은 남아 있으며 프로젝트 자동화 검사를 요청하거나 실행하지 않았고 무오류 실행으로 기록하지 않는다.

1차 컴파일의 C++ 오류 2개를 수정한 뒤 Development Editor / Win64 컴파일·링크는 6개 작업·8.15초로 성공했으며 후속 회귀 소스를 반영한 최종 Development Editor / Win64 컴파일·링크도 8개 작업·18.74초로 성공했다. 최종 로그는 `Saved/Logs/WeaponSkillsFinalBuild.log`다. 태그 적합성·부족 후보·확정 사본 직렬화·장착 출처 병합·저장 실패·기존/새 Run Continue 회귀 소스를 추가했다. 게임·PIE·자동화 테스트는 실행하지 않았으며 [TODO 29절](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)의 사용자 확인이 남아 있다.

작성 명령의 프로젝트 절대경로를 생략한 표기는 UE 5.8.3 `UnrealEditor-Cmd.exe ProjectA.uproject -ExecutePythonScript=Source/ProjectAEditor/Scripts/CreateWeaponSkills.py -unattended -nop4 -nosplash -NullRHI -NoSound -NoLiveCoding`이며 절대 `-abslog`에 위 작성 로그를 지정했다. 최종 C++ 빌드 대상은 `ProjectAEditor Win64 Development -WaitMutex -FromMsBuild -architecture=x64`다. 기존 UI 회귀 3개는 명시적 legacy prototype 초기화를 사용하고 정상 목표 Run 회귀는 실제 메뉴 생성의 비무장 1+무기 1스킬·스킬상점 비활성·고정 규칙 저장을 검사하도록 보완했다. 해당 검사는 소스 컴파일만 수행했다.

`AuditSkillVfxDirections.py`는 고정 74개 검사를 제거하고 디스크 패키지와 레지스트리 스킬 경로의 완전 일치·누락·중복·타 클래스 거절을 검사하도록 갱신했다. 원본 해시 보존과 소모품을 포함한 전체 VFX 검수 범위는 유지한다. 이 도구는 Python AST·diff 정적 검사만 통과했으며 실행하지 않았다. 최종 문서 10개의 로컬 파일/앵커 링크 637개와 `git diff --check`를 통과했다.

### 9-25 2026-10-07 TODO 권장안의 로컬 구현

2026-10-07 권장안 진행 위임에 따라 기존 제안 7·9~14의 채택 선택을 유지하고 외부 준비와 독립적인 T14 제안 9·10·12를 구현했다. 작업 시작 기준은 `71211b13`이며 기존 미커밋 변경은 없었다. 계정·PlayFab Title·무료 사용 조건·서로 다른 PC 2대의 준비를 가정하지 않았다. 제안 11·13·14의 중앙 정본·결과 검증·MMR과 신규 에셋 선정은 여전히 미완료다.

제안 9·10: Steam friends-only 로비는 일반 공개 검색에서 제외되므로 같은 게임의 친구 목록을 읽고 UE 5.8의 단일 `FindFriendSession`을 순차 호출한다. 동일 세션 중복·비친구 Host·잘못된 표식/빌드/정원을 제외하고 요청 번호·대기 상태로 오래된 콜백과 동기 실패의 이중 처리를 막는다. 다음 검색은 다음 tick에서 진행하며 검색·생성·참가 중 예약한 나가기를 처리한다. 로그아웃한 계정의 SteamAuth 관측을 삭제하고 엔진 기본 실패 추방을 유지한다. 검색 버튼도 친구 검색으로 명시했다. 설치 엔진의 `OnlineSessionInterfaceSteam.cpp`·`OnlineSessionAsyncLobbySteam.cpp`·`OnlineFriendsInterfaceSteam.cpp`와 [공식 로비 공개 범위](https://partner.steamgames.com/doc/api/isteammatchmaking#ELobbyType)를 대조했다. 기존 Run·소유권·관리 저장과 Steam 비활성 기본 설정은 유지했다.

제안 12: `FPartySnapshotCandidate`와 `UPartySnapshotSelectionLibrary::SelectOpponent`에 Snapshot·진행 단계·GameplayTagContainer 값 데이터를 추가했다. 같은 콘텐츠 버전·진행 단계·GameplayTagQuery 조건과 기존 카탈로그의 생존·배치·정식 클래스/스킬·지원 형식을 검증한 뒤 공통 후보 선택기로 가중치 1의 균등 추첨을 수행한다. 적격 후보의 같은 ID는 거절하며 실패 시 이전 출력·난수 상태, 성공 시 원본 후보를 보존한다. 기존 Run의 고정 상대 생성·저장에는 연결하지 않은 로컬 개발 API다. 온라인 게시·승인·매칭·보존·소모품 AI·소유자 자산의 이중 소모 방지는 포함하지 않는다.

`ProjectA.Snapshot.CandidateSelection` 회귀 소스에 잘못된 버전·단계·태그·사망·임의 클래스/스킬·장비·스키마와 중복 ID 거절, 실패 보존·독립 값 사본·시드 재현을 추가했다. 독립 정적 리뷰는 후보 검증·비동기 완료 순서·실패/나가기·재접속 관측을 확인했으며 추가 결함을 찾지 못했다. TODO 23·26·27·28·29의 체인 정리·Debug HP·저장 선확정·이전 속도 이관·새 Run 규칙도 정적으로 대조했으며 실제 플레이 확인으로 확대하지 않는다.

재생성된 `Automation_ProjectA.sln`은 기존 줄의 내용·개수가 같고 순서만 변경됐으며 함께 반영했다. TODO의 기존 채택 체크 7개와 사용자 확인 항목을 보존하고 완료 작업 체크를 추가하지 않았다. 문서 10개의 로컬 파일·앵커 링크 646개와 전체 diff 정적 검사를 통과했다.

UE 5.8.3 `Build.bat -ProjectFiles -Project="C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject" -Game -Engine`은 9.67초, `Build.bat ProjectAEditor Win64 Development -Project="C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64`는 UHT 포함 19개 작업·45.82초로 성공했다. 컴파일 오류·경고는 없으며 프로젝트 생성의 VS2022/.NET 10 안내는 기존 C++/Automation 솔루션 분리 조건이다. 근거는 `Saved/Logs/TodoRecommendedProjectFiles.log`·`Saved/Logs/TodoRecommendedBuild.log`, 문서 파일/앵커 검사는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`에 기록한다. 게임·PIE·Unreal 자동화·에셋 작성/재로드는 실행하지 않았다. [제안 10의 실제 2PC 검색·초대·재접속 확인](TODO.md#4-1-2-제안-10-초대재접속관전)과 [제안 12의 로컬 회귀 실행·온라인 구현](TODO.md#4-2-2-제안-12-온라인-snapshot과-상대-선정)은 남아 있다.

### 9-26 2026-10-08 에셋별 아이템 등급

사용자의 에셋 외형별 등급 분류 요청에 따라 제안 16·17의 5색·무기당 1스킬·사본 생성 시 확정 정책을 유지하고 새 Run의 등급 선정만 에셋별 고정값으로 변경했다. [무기 CSV](../DataCatalogs/WEAPON_ASSETS.csv) 289행에 `등급`·`분류 근거`를 추가했다. 흰색 62·초록색 110·파란색 95·보라색 20·주황색 2개이며 기준은 [GAME_DESIGN 2-4-4](GAME_DESIGN.md#2-4-4-에셋별-등급-분류)를 따른다. 기존 행 순서·종류·경로·이름·가격 5열은 유지했다. 가격 1G·상품 후보 가중치·전투 수치·장착 지원 범위는 변경하지 않았다.

엔진을 실행하지 않고 uasset에 저장된 PNG/JPEG 미리보기 288개를 추출하여 직접 비교했다. 자체 미리보기가 없는 기존 `User_JeHoon/Weapon_Pack/Mesh/Weapons/Weapons_Kit/SM_Sword`는 원본과 FBX `FileMD5=faa9e75e1e63051605a506c9eff7e332` 및 원본 재질 참조가 같음을 확인하고 원본의 초록색을 배정했다. 이는 현재 형상 동일성을 입증한 결과가 아니며 사본 외형 확인을 TODO에 남겼다. 원본 에셋 복제·수정·이동·삭제는 없고 289개 SHA256이 작업 전과 같다. 근거는 `Saved/Automation/ItemRarity/previews.json`·`classification.json`·`StaticVerification.json`, CSV 재읽기와 원래 열 보존 결과는 `catalog_verification.json`이다. `catalog_before.png`·`catalog_after.png`·`catalog_after_rare.png`로 카탈로그 표시도 확인했다.

`FRunItemDefinition.CatalogRarityTag`를 카탈로그·사본에 저장하고 6/7열 CSV의 색 이름을 기존 `Item.Rarity.*`로 엄격 해석한다. 새 사본은 기준 등급의 무기 태그·등급 Query 후보에서 스킬만 추첨하며 후보가 부족해도 다른 등급이나 무관한 스킬로 대체하지 않는다. 4/5열 CSV와 기준 태그가 없는 기존 저장은 당시 무작위 등급 정책을 유지한다. 기존 저장 카탈로그·보유품·진열 사본을 현재 CSV로 이관하지 않으며 기준 등급 변조와 실패 시 카탈로그·출력 사본·난수 상태 변경을 막는다. 독립 정적 검토에서 시작 장비·상점·저장 경로의 추가 회귀를 찾지 못했다.

`ProjectA.Run.Shop.CatalogAuthoredRarity`, `ProjectA.Run.WeaponSkills.AuthoredRarityAndFrozenSave` 회귀 소스를 추가하고 `DevelopmentRuleData`에 실제 카탈로그 등급·무기 후보 호환 검사를 보완했다. UE 5.8.3 `Build.bat ProjectAEditor Win64 Development -Project="C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64`는 UHT 포함 19개 작업·46.50초로 성공했다. 로그는 `Saved/Logs/ItemRarityBuild.log`다. 추가 회귀는 컴파일만 수행했으며 게임·PIE·Unreal 자동화 테스트는 실행하지 않았다. [TODO 29절](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)의 새 Run 표시·구매/장착·Continue·기존 저장 보존과 사본 외형 확인은 미완료로 유지한다.

문서 10개의 로컬 파일·앵커 링크 654개와 `git diff --check`를 통과했다. `verify_catalog.py`로 기존 5열·289개 원본 해시·등급과 근거의 전 행 일치·경로 중복 없음·UTF-8 BOM/CRLF를 확인했다. TODO의 채택 체크 7개와 미완료 항목을 유지하고 완료 작업 체크나 빈 절을 추가하지 않았다. 문서 검사 결과는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`이다.

### 9-27 2026-10-08 아이템 등급별 등장 확률

사용자의 등급별 확률·CSV 기록 요청과 권장안 위임에 따라 [ITEM_RARITY_PROBABILITIES.csv](../DataCatalogs/ITEM_RARITY_PROBABILITIES.csv)에 흰색 50%·초록색 30%·파란색 15%·보라색 4%·주황색 1%의 개발 기본값을 추가했다. 기본·실전형 장비의 공급과 상위 외형의 희소성을 구분한 초기값이며 최종 밸런스 검증과 구분한다. 5열은 등급·기존 GAS 등급 태그·확률·아이템상점 범위·비고다. 5색의 중복·누락·이름/태그 불일치, 범위·비고 오류와 잘못된 수치를 거절한다. 확률은 소수 둘째 자리까지 0~100을 허용하고 0.01% 단위 정수의 합계 10000을 검사한다. 에셋별 등급·이름·가격을 담은 기존 무기 CSV는 변경하지 않았다.

새 일반 Run 생성의 `ConfigureTargetRun`에서만 확률 목록·버전 1을 `ItemShopState.RarityProbabilities`에 저장하고 최초 진열·다음 방문·리롤은 저장된 정책을 사용한다. `RunItemRarityProbabilities::Select`는 아이템 태그와 기준 등급 태그·스킬 적합성으로 후보를 선별하고 공통 `GameplayTagCandidateSelection`으로 등급을 먼저, 해당 등급 안에서 에셋을 균등 추첨한다. 같은 진열의 에셋은 중복하지 않으며 비어 있거나 소진된 등급은 제외하고 재정규화한다. 0%는 대체 후보로 사용하지 않는다. 적격 에셋 부족 등 실패 시 상품·Revision과 선택 API의 출력·주입 난수 상태를 보존한다. 기존 `Roll`의 전역 난수 시드 생성 방식은 유지한다.

확률 필드가 없는 기존 Run은 에셋별 고정 등급을 사용하는 저장도 버전 0의 기존 균등 아이템 추첨을 유지한다. 현재 CSV로 저장된 카탈로그·상품·보유 사본·스킬을 이관하거나 재추첨하지 않는다. 직업별 지정 시작 장비·무기 스킬 후보·가격 1G·전투 수치·에셋과 장착 지원 범위는 변경하지 않았다. 상점 테마·전리품·인카운터 확률로 범위를 확대하지 않았다. 패키징의 UFS 목록에 확률 CSV를 추가했다. [기획 기준](GAME_DESIGN.md#2-4-5-아이템-등급별-등장-확률)

`ProjectA.Run.Shop.RarityProbabilities` 아래 `CsvValidation`·`GradeFirstSelection`·`EligibilityAndAtomicity`·`FrozenSaveAndLegacyPolicy` 회귀 소스를 추가했다. 등급별 에셋 개수가 다른 카탈로그의 동일 시드 등급 선택, 첫 슬롯 분포와 등급 내 균등 추첨, Query·스킬 부적합·0%·소진·중복 제한·실패 보존·저장 왕복·기존 균등 결과를 검사한다. 독립 정적 리뷰에서 추가 결함을 찾지 못했다. 게임·PIE·자동화 테스트는 실행하지 않았으며 [TODO 29절](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)의 실제 상점·CSV 변경 이후 저장 복원·회귀 실행 확인은 미완료다.

UE 5.8.3 `Build.bat -ProjectFiles -Project="C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject" -Game -Engine`은 9.60초, `Build.bat ProjectAEditor Win64 Development -Project="C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject" -WaitMutex -FromMsBuild -architecture=x64`는 18개 작업·36.51초로 성공했다. 근거는 `Saved/Logs/ItemRarityProbabilitiesProjectFiles.log`·`Saved/Logs/ItemRarityProbabilitiesBuild.log`다. 프로젝트 생성의 VS2022/.NET 10 안내는 기존 C++/Automation 솔루션 분리 조건이며, 재생성된 `Automation_ProjectA.sln`의 580줄은 내용·개수가 같고 순서만 바뀌어 함께 반영했다.

확률 CSV 재읽기·합계 100%·5색 고유 태그·UTF-8 BOM/CRLF와 기존 무기 CSV 보존을 확인하고 `probabilities_preview.png`를 검토했다. 근거는 `Saved/Automation/ItemRarityProbabilities/CsvValidation.json`·`StaticVerification.json`이다. 문서 10개의 로컬 파일·앵커 링크 670개와 전체 diff 정적 검사를 통과했다. TODO의 기존 채택 체크 7개와 미완료 항목을 보존하고 새 확률 회귀·작동 확인만 추가했다. 문서 검사 결과는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`이다.

### 9-28 2026-10-08 상점 분류와 인카운터 가중치

사용자의 기본상점·등급별상점·태그별상점 구성과 인카운터 풀 가중치·CSV 정리 요청을 반영했다. [ENCOUNTER_POOL.csv](../DataCatalogs/ENCOUNTER_POOL.csv)는 기존 18개 ID를 보존하고 신규 25개를 추가한 15열·43행이다. `활성 여부` 1인 28개는 기본 1·등급별 5·기존 `Item.Weapon` 종류 태그별 19·회복/소모품/부활 서비스 3개이며 나머지 15개는 기존 저장·과거 기획 기록이다. 신규 기본상점은 `Shop_Item_Basic`, 전문 상점은 `Shop_Item_Rarity_*`·`Shop_Item_Tag_*` ID를 사용하며 기존 `TargetOffer_03/04/05`의 서비스 ID를 유지한다. 이전 스킬상점과 속성별 무기상점 ID를 새 상점으로 재사용하지 않고 속성 태그를 추정하지 않는다.

그룹 가중치는 기본 40·등급 전체 20·태그 전체 20·회복 10·소모품 7·부활 3이다. 그룹을 먼저 추첨하고 해당 그룹의 변형을 추첨하여 같은 ID 없이 3개를 제시한다. 선택한 ID·고갈 그룹·0 가중치를 제외해 재정규화하며 변형 수가 그룹 총비중을 늘리지 않는다. 활성 행의 0 가중치 후보는 저장된 풀에 보존하되 추첨에서 제외한다. 등급 상점 내부 가중치는 흰색 50·초록색 30·파란색 15·보라색 4·주황색 1, 태그 상점은 각각 1이다. 상점 변형 가중치는 기존 `ITEM_RARITY_PROBABILITIES.csv`의 상품 등급 확률과 별도 데이터다. [기획 기준](GAME_DESIGN.md#2-3-상점-분류와-인카운터-가중치)

새 일반 Run만 CSV의 활성 풀·그룹/변형 가중치·상품 Query·진열 정책을 읽어 고정하고 `FRunTargetState.EncounterSelectionVersion=1`과 `EncounterSeed`를 저장한다. 동일 전투·방문 회차의 제시를 재현하여 Continue·저장 검증에서 현재 CSV로 재추첨하지 않는다. 기존 정책 버전 0의 목표 Run은 당시 고정 순환과 선택 이력을 유지하고 prototype·개발 협동 경로도 보존한다. 새 아이템상점은 `SelectionVersion=1`에 `ActiveEncounterId/ActiveItemQuery/ActiveStockPolicyVersion`을 저장하여 입장·리롤·재개에서 같은 상점 조건을 사용한다.

기본상점은 중복 없는 상품 5개, 전문 상점은 적격 상품 수와 5 중 작은 수를 진열한다. 주황색 2개·마법서 1개 등 소수 후보를 지원하고 양수 그룹/변형 가중치와 인카운터 Query를 만족하는 선택 가능 상점에 적격 상품이 0개이면 새 Run 설정을 거절한다. 기존 상품 등급 확률·에셋별 고정 등급·무기 스킬 Query·가격/리롤 1G·시작 장비·전투 수치는 유지한다. 새 확률을 기존 저장에 소급 적용하지 않으며 원본 에셋·온라인 서비스·신규 보상 구현은 포함하지 않는다. 인카운터 CSV를 UFS 패키징 목록에 추가한다.

`ProjectA.Run.Shop.Profiles`의 `FixedAndSpecializedStock`·`EligibilityAndAtomicity`·`FrozenSaveAndLegacyPolicy` 회귀 소스를 추가했다. 기본 5개·등급 2개·태그 1개 진열, 필터 전환/리롤·0 확률·스킬 적합성·실패 시 전체 상태 보존·저장된 필터와 기존 균등/가중 5칸 상점 보존을 검사하며 실행 결과와 구분한다.

`ProjectA.Run.EncounterPool`에 CSV 오류·초기화 원자성, 그룹/변형 추첨·0 가중치·고갈·기존 60회차 순환, 저장 시드·Query·CSV 표시 문구 직렬화 회귀 소스 3개를 추가했다. 기존 거래 검증은 기본상점이 제시되는 시험용 저장 시드를 사용하고 이전 Run fixture는 새 정책 메타데이터를 초기화한다. 저장된 인카운터 표시 문구는 문자열을 비교하며 나머지 필드를 그대로 검사한다.

CSV 재읽기에서 15열·43행·활성 28개·보존 15개·그룹 가중치 합 100을 확인했다. 289개 아이템과 대조하여 25개 상품 조건의 등록 태그·양수 확률 후보 수를 정적으로 확인했다. 근거는 `Saved/Automation/EncounterShops/CsvValidation.json`·`CatalogValidation.json`이며 실제 스킬 생성 실행과 구분한다. 문서 10개의 로컬 파일·앵커 링크 685개와 diff 검사를 통과했고 링크 근거는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`이다.

`Build.bat -ProjectFiles -Project=ProjectA.uproject -Game -Engine`(6.12초)와 `Build.bat ProjectAEditor Win64 Development -Project=ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`(26 actions, 41.88초)가 성공했다. 로그는 `Saved/Logs/EncounterShopsProjectFiles.log`·`Saved/Logs/EncounterShopsBuild.log`다. 새 CSV 파서·가중 추첨·상품 필터·저장 검증·회귀 소스를 포함한 최신 컴파일과 코드 정적 검토 결과이며 게임·PIE·자동화 테스트는 실행하지 않았다. 실제 상점 필터·소수 후보 진열·구매/리롤·CSV 변경 후 새/기존 Run·Continue 확인은 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남겼다.

### 9-29 2026-10-08 전투 아이템 선택과 공통 골드 보상

사용자의 전투 후 아이템 보상·무작위 골드 동시 지급 요청과 권장안 위임에 따라 아이템 3택1을 적용했다. `WeaponSkillAcquisitionVersion=1`의 일반 Target Run은 다음 PvE 승리부터 보상 스키마 2를 사용한다. 해당 Run에 저장된 카탈로그·등급 확률·무기 스킬 규칙의 공통 추첨·사본 생성으로 중복 없는 에셋 3개를 제시하며 직전 전문상점의 Query를 적용하지 않는다. 골드는 저장된 해당 묶음 `GoldChoices`의 최솟값~최댓값 사이 정수를 양 끝값 포함으로 한 번 추첨하고 어느 아이템을 선택해도 같은 금액을 지급한다. 기본 묶음 i(0~9)의 범위는 `5+i~10+iG`다. [기획 기준](GAME_DESIGN.md#2-1-성장경제보상과-run-종료)

`FRunGoldRewardState` 스키마 2에 아이템 후보·공통 골드·수령 기록을 저장한다. 결과 저장 실패의 재시도는 같은 추첨을 유지하고 선택 시 아이템 1개·골드·수령 기록을 원자 저장한 뒤 공개한다. 아이템은 가방에 추가하며 자동 장착하지 않는다. 이미 저장된 스키마 0/1의 결과·골드 선택지·수령 기록은 보존하고 과거 결과에 소급 지급하지 않는다. 무기 스킬 획득 버전 0의 Target·prototype·개발 협동은 기존 골드 정책을 유지하며 Snapshot·패배에는 새 보상을 지급하지 않는다.

[ITEM_RARITY_PROBABILITIES.csv](../DataCatalogs/ITEM_RARITY_PROBABILITIES.csv)의 적용 범위를 `아이템상점·전투보상`으로 명시하고 파서는 이전 `아이템상점` 표기도 허용한다. 확률값 50/30/15/4/1과 새 Run 생성 시 저장하는 규칙은 유지하며 기존 카탈로그·확률을 다시 작성하지 않는다. 확률 상태가 없는 이전 저장은 당시 균등 추첨을 유지한다. 결과 UI는 아이템의 등급·스킬과 공통 골드·개인 잔액·수령 상태를 표시하며 이전 골드 카드는 기존 저장에 계속 사용한다. [UI 기준](UI_README.md#13-전투-승리-보상)

`ProjectA.Run.Reward.Items` 아래 `CommonFrozenPolicy`·`InvalidInputsAndAtomicity`·`FrozenSerialization` 회귀 소스를 추가했다. 저장된 균등/등급별 선택과 난수 상태, 방패·탄약 포함·지원 무기 스킬, 중복 없는 3개·골드 범위, 실패 시 출력/난수 보존·잘못된 저장 거절·직렬화 보존을 검사하며 실행 결과와 구분한다.

`ProjectA.Run.Target.ItemRewards`의 `AtomicResultAndClaimRetry`·`LegacyGoldAndMalformedCopies` 통합 회귀 소스 2개를 추가하고 기존 80단계 합성 진행의 보상 검증을 갱신했다. 결과 저장 실패의 동일 추첨 재시도, 수령 실패의 인벤토리·골드·수령 기록 불변, 한 번 지급·재개·중복 거절, 이전 골드 보상 수령 후 다음 PvE 전환과 잘못된 사본/골드/수령 기록 로드 거절을 검사한다. 테스트 소스 컴파일과 실제 실행은 구분한다.

`Build.bat -ProjectFiles -Project=ProjectA.uproject -Game -Engine`(4.50초)와 `Build.bat ProjectAEditor Win64 Development -Project=ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`(23 actions, 39.43초)가 성공했다. 로그는 `Saved/Logs/CombatRewardsProjectFiles.log`·`Saved/Logs/CombatRewardsBuild.log`다. CSV는 `D2:D6` 범위 표기만 변경하고 확률 합계 100과 나머지 셀 보존·재읽기를 확인했다(`Saved/Automation/CombatRewards/CsvValidation.json`). 문서 10개의 로컬 파일·앵커 링크 700개와 diff·최신 코드 정적 검토를 통과했다. 링크 근거는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`이다. 게임·PIE·자동화 테스트는 실행하지 않았으며 실제 카드·수령·Continue·저장 실패 재시도·기존 보상 호환 확인은 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남겼다.

### 9-30 2026-10-08 몬스터 스탯과 출현 자료 정리

사용자의 몬스터 스탯·확률 CSV 정리 요청에 따라 `MONSTER_STATS.csv` 13종, `MONSTER_SPAWN_PROBABILITIES.csv` 13종, `MONSTER_ENCOUNTERS.csv` 10묶음을 추가했다. 스탯 자료는 기본 HP 150·AP 2·SAP 1·속도 5·보호막 0·이동거리 1과 공격 수치·선딜·스킬 경로를 기본값·작성 명세·과거 재로드 기록에 대조한다. 현재 에셋을 엔진으로 재조회한 결과와 구분하며 기존 `MONSTER_ASSETS.csv`의 ID·원본 경로는 유지한다. [자료 기준](PROJECT_PLAN.md#4-5-몬스터-콘텐츠)

출현 자료는 현재 고정 편성 집계와 권장 가중치·정규화 확률을 별도로 기록한다. 권장 가중치의 합은 100이며 공통 후보 전체가 적격인 첫 1슬롯 기준이다. 향후 태그 필터에는 남은 가중치의 재정규화가 필요하고 최종 밸런스나 런타임 적용 완료로 해석하지 않는다. 편성 자료는 기본 목표 Run의 10 PvE 묶음·25개체·4슬롯 순서와 진행 단계 `4+8i`·전투 순번 `2i+1`을 정리한다. 등장 100%는 해당 묶음 도달 조건이며 PvE 적의 스탯 성장은 없다. 기존 최대 HP +5·속도 +1 성장은 아군 규칙이다. [목표 Run 기준](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)

공격 피해 50·AP 1은 `ConfigureMonsterContent.py`의 해석된 기본 공격 복사와 2026-10-04 `Saved/Automation/TodoCompletion_20261004/RagdollFinal.d4b67380eb78483abdea7f0856794adf/Console.log`의 13종 실행 이력을 대조했다. 12종 선딜은 `MonsterContentSpecs.json`, 기존 검병의 0.23초 선딜과 무기 궤적은 `Saved/Automation/MeleeSkillName/Baseline.json`을 기준으로 한다. 기본 HP/AP는 `Saved/Automation/Monsters/Reload.json`과 대조하며 SAP·속도·보호막·이동거리는 현행 C++ 기본값이다.

CSV 3개의 재읽기·13/13/10행·25개체 집계·권장 가중치와 확률 합계 100·13종 스킬 파일 존재를 확인하고 표시 미리보기 5개를 검토했다. 작성·재읽기 근거는 `Saved/Automation/MonsterBalance_20261008/CsvValidation.json`이다. 문서 10개의 로컬 링크 712개·diff 검사를 통과했으며 링크 근거는 `Saved/Automation/TodoRecommended/DocumentationValidation.json`이다. C++·설정·원본 에셋·기존 CSV·TODO·현재 런타임·기존 저장은 변경하지 않았다. 컴파일이 필요한 코드 변경은 없으며 게임·PIE·자동화 테스트와 에셋 재로드는 실행하지 않았다. 이전 몬스터 실행 이력과 TODO의 미완료 확인 항목은 보존한다.

### 9-31 2026-10-08 CSV 기반 레벨 난이도

사용자의 레벨 디자인 난이도 적용 위임에 따라 앞선 자료 CSV 3개를 새 기본 Run 입력으로 연결했다. `MONSTER_STATS.csv`는 기존 13종 ID와 원본 경로를 유지한 13열의 종별 스탯·지역/역할 태그·스킬 목록, `MONSTER_SPAWN_PROBABILITIES.csv`는 활성 12종 합계 100과 개발용 검병 0, `MONSTER_ENCOUNTERS.csv`는 17열·10묶음의 숲·늪·동굴·설원·최종 구간 규칙이다. 5묶음 Brute·10묶음 Boss 선봉을 먼저 고르고 남은 적을 태그 Query·공통 가중 추첨으로 동종 중복 없이 선정한다. 원본 Blueprint·스킬·공격 피해 50·공격 AP 1·선딜은 변경하지 않았다. [현재 수치·구조](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)

기본 `ConfigureTargetRun`만 LevelDesign 버전 1의 최초 시드·시작 파티 인원·카탈로그·규칙·최종 몬스터 편성을 저장한다. 시작 1~4명에 따라 적 수와 PvE HP를 조정하고 7묶음은 기준 적 수를 줄인다. 기존 `EnemyClasses`를 보존하면서 `EnemyRoster`의 클래스·HP/AP/SAP/속도/이동거리·원본 스킬을 스폰에 적용한다. 저장된 입력으로 편성을 검증하고 체크포인트의 적 수·순서·불변 스탯·스킬을 대조한다. 이전 저장과 명시적 맞춤 정의는 버전 0의 당시 편성·클래스 기본값·보상·회복 정책을 유지하며 CSV를 소급 적용하지 않는다.

60선택·20전투와 아이템 3택1을 유지하고 새 기본 난이도의 골드를 첫 3~5G에서 마지막 8~15G로 조정했다. 10회 수령 시 기대 골드 73.5G·시작 10G·현재 아이템 가격 1G를 개발 초기값으로 둔다. PvE 승리마다 생존 파티에 휴식 HP 20~40을 성장 후 최대 HP까지 회복하며 사망자·Snapshot·패배에는 적용하지 않는다. 기존/최종 HP를 회복 전에 검증하고 결과·보상과 같은 후보로 저장하여 실패 재시도에서 중복 회복하지 않는다. 중앙 결과 뷰의 `VictoryRestHP`로 휴식 적용을 표시하고 지도 이름에 구간 주제를 반영했다. 런타임 CSV 6개를 UFS 패키징 대상으로 지정했다.

`ProjectA.Run.LevelDesign`에 CSV 합류·오류 거절·실패 시 원본 보존, 파티 인원 배율·태그·선봉·시드, 직렬화·기존 정책·변조 거절의 회귀 소스를 추가했다. `ProjectA.Run.Target.LevelDesign.SurvivorRestAndAtomicRetry`는 생존 동료 회복·최대 HP·사망 보존·결과 저장 실패/재시도·Snapshot/패배/이전 정책의 회복 제외를 다루며 기존 합성 진행 검증도 새 휴식을 반영했다. 회귀 소스와 실제 실행 결과는 구분한다.

2026-10-08 `Build.bat -ProjectFiles -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -Game -Engine`의 프로젝트 생성 6.43초와 `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`의 최종 컴파일·링크 10.38초가 성공했다. 최초 빌드의 Unity 재분할로 드러난 기존 상점 회귀의 변수명 가림 C4459는 로컬 이름만 구분해 해소했다. 근거는 `Saved/Logs/LevelDesignProjectFiles.log`·`LevelDesignBuild.Initial.log`·`LevelDesignBuild.log`이며 최종 빌드 오류·경고는 0이다. 생성된 `Automation_ProjectA.sln`은 행 순서만 변경되고 전체 행 집합은 동일하다.

CSV 재계산·수식 오류 0·렌더 확인·내보내기/재입력 일치와 실제 13/13/10행, 클래스·스킬 경로 각 13개, 양수 후보 12개·합계 100, 1~4인×10구간의 40조합 후보 수·배율·선봉을 정적으로 확인했다. 근거는 `Saved/Automation/LevelDesign_20261008/CsvValidation.json`·`independent_validation.json`·`Preservation.json`이다. 문서 10개 로컬 링크·앵커 724개와 `git diff --check`를 통과했고 TODO의 기존 미완료 33개·선택 7개 및 HISTORY 이전 본문을 보존했다. Content와 원본 몬스터 목록은 변경하지 않았다. 게임·PIE·Unreal 자동화 테스트는 실행하지 않았다. 새 난이도의 강제 승리 없는 80단계 완주·실제 Continue·CSV 변경 전후 저장 호환·휴식 표시와 중복 방지는 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 미완료로 유지한다. 9-30의 자료 정리 및 과거 실행 근거를 최신 난이도 성공 근거로 대체하지 않는다.

### 9-32 2026-10-08 통합 Gameplay와 NPC 상점 프로토타입

사용자가 여러 레벨을 하나의 큰 레벨로 구성하고 외부 에셋 없는 NPC 상점 연출을 요청했다. 기존 `Core/Gameplay`에 환경 12종·던전 2종의 장식 명세를 12,000cm 간격으로 합성한다. 중앙 전투 Arena·Grid·카메라·물리 바닥·navigation·GameMode·조명과 체크포인트의 절대 좌표를 유지한다. 기존 비교 맵 14개는 제작 자료·참조 경로 호환을 위해 보존하며 원본 메시·기존 재질을 직접 참조한다. 생성 입력과 보존·재작성 명령은 [에셋 도구 31번](../Source/ProjectAEditor/Scripts/README.md)에 기록했다.

`AEncounterPrototypeStage` 5개는 기본 도형으로 상인·대장간·회복소·소모품점·제단을 표현한다. 신규 색상 재질 1개와 절차적 인사·호흡을 사용하며 모델·텍스처·애니메이션을 추가하지 않는다. 필수·제외 GameplayTagContainer와 우선순위로 기존 인카운터를 해석하고 기본·등급별 상점은 상인, 태그별 상점은 대장간을 공유한다. 저장·복제 후 확정된 방문에 맞춰 로컬 카메라를 0.65초 이동한 뒤 인사와 오른쪽 패널을 표시한다. 방문 키·세대 번호·타이머 취소로 구매·리롤 갱신의 재연출과 종료 후 콜백을 방지한다. 인벤토리 버튼과 I 입력은 기존 창을 사용하며 거래·진행·저장·권위 경로는 유지한다. 태그 분류 회귀 소스를 추가했으며 실제 자동화 실행 결과와 구분한다.

2026-10-08 프로젝트 생성 4.44초와 Development Editor / Win64 컴파일·링크 12.60초가 성공했다. 명령은 `Build.bat -ProjectFiles -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -Game -Engine` 및 `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`다. 최초 Unity 재분할에서 기존 CSV 로더 둘의 `MaximumCatalogSize` 이름 충돌이 드러나 난이도 로더 내부 이름만 구분했고 값과 동작은 유지했다. 근거는 `Saved/Logs/UnifiedGameplayProjectFiles.log`·`UnifiedGameplayBuild.Initial.log`·`UnifiedGameplayBuild.log`다.

에셋 작성 커맨들릿 52.41초와 별도 읽기 전용 재로드 50.88초가 성공했다. `UnifiedGameplaySpecs.json`의 14구역·장식 3,449개를 ISM Actor 174개·개별 메시 Actor 117개로 저장하고 NPC 5개·전경 카메라 1개를 추가했다. 기존 Actor 13개의 배치·컴포넌트·전투 참조·조명 및 원본 비교 맵을 보존했다. 결과는 `Saved/Automation/UnifiedGameplay_20261008/{Author,Reload,PreservedLayout,ProtectedAuthor,ProtectedReload}.json`, 로그는 `Saved/Logs/UnifiedGameplayAuthor.log`·`UnifiedGameplayReload.log`다. 최초 작성은 던전 평면 벽 20개의 0 두께를 잘못 거절해 저장 전에 중단됐으며 실제 원본 축의 평면 여부를 검증하도록 수정했다. 초기 로그는 `UnifiedGameplayAuthor.Initial.log`에 보존했다. 최종 작성·재로드의 스크립트 오류는 0이며 종료 월드 정리 시 CrowdManager의 RecastNavMesh 경고 1건은 별도로 기록한다.

작성·재로드 스크립트 전후의 보호 대상 19,873파일 해시가 일치했다. 커맨들릿 시작·종료에서는 `Saved/Config`의 에디터 로컬 설정 1개 갱신과 CrashReportClient 설정의 자동 생성·정리가 발생했으며, 원본 Content·프로젝트 Config·게임 저장 보존과 구분한다. 재로드와 문서의 재실행 명령에는 엔진 INI 저장 방지 옵션 `-nowrite`를 사용한다. 전체 프로세스 이후 보호 대상의 추가 변화는 없으며 근거는 `Saved/Automation/UnifiedGameplay_20261008/AfterProcessPreservation.json`이다.

Python·JSON·문서 링크·diff 정적 검사와 카메라·UI 전환 수명 읽기 검토를 수행했다. 솔루션 행 집합, 런타임 CSV, TODO의 기존 미완료 36개·선택 7개와 HISTORY 이전 본문을 보존하고 사용자 확인 2개를 추가했다. 근거는 `Saved/Automation/UnifiedGameplay_20261008/StaticChecks.json`이다. 게임·PIE·자동화 테스트·패키지 실행과 실제 화면/FPS 검수는 수행하지 않았다. NPC 인사·카메라·패널·인벤토리, 이전 Run Continue와 화면비·성능의 수용 조건은 [TODO 26절](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 미완료로 유지한다.

### 9-33 2026-10-08 위임 실행 검수와 협동 보완

2026-10-08 사용자가 권장안 구현·실행 검수·멀티플레이 작업을 추가 질문 없이 진행하도록 위임했다. 판단 근거와 외부 준비의 한계는 사용자 요청으로 작성한 [위임 판단](EXECUTION_DECISIONS.md)에 기록한다. 아래 결과는 이번 코드의 로컬 Native 회귀·동일 PC PIE·새 Win64 패키지 실행이며 과거 검증을 재사용한 것이 아니다.

식별 Run의 prototype·개발 협동 초기화를 기존 `CommitSaveCandidate`에 연결하여 체크포인트 저장 성공 뒤에만 새 상태를 적용·통지한다. 실패 시 기존 Run·파일을 보존하며 메모리 전용 동작은 유지한다. 제안 7의 `ValidateNewRunRoster`를 일반/관리 새 협동 생성에 적용하여 원래 참가자마다 한 캐릭터만 허용한다. 기존 저장의 로드·재개에 새 편성 제한을 소급하지 않고 저장 schema·원래 소유권·참가 번호·관리 lease 계약을 유지한다. 저장 거절 과정에서 하위 검증 성공이 오류 문구를 비운 뒤 후속 검사가 실패하던 경로도 보완하여 누락된 이유를 제공한다.

제안 12의 Snapshot 카탈로그는 해석한 스킬의 `Item.Consumable` 및 자식 태그를 공통 규칙으로 거절한다. 별칭 ID로 수량 없는 소모품을 일반 스킬처럼 실행할 수 없다. 개발용 `LoadAndSelectOpponent`는 슬롯 이름·중복 검사 뒤 진행 단계·GameplayTagQuery가 일치하는 파일을 읽어 기존 균등 선별 API에 연결한다. 일치 슬롯의 누락·손상을 제외 재추첨으로 숨기지 않으며 실패 시 출력·난수, 성공/실패 모두 원본 파일을 보존한다. Snapshot schema 1과 기본 Run의 고정 상대는 유지하며 이 API는 기본 Run·온라인 공급자에 연결하지 않은 준비 기능이다. [현재 계약](MULTIPLAYER.md#5-async-pvp-상대-snapshot)

Development Editor / Win64 명령 `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`가 회귀 수정 후 12.22초, 후속 표시 코드 포함 6.85초에 성공했다. 근거는 `Saved/Automation/Autonomous_20261008/EditorBuild.RegressionFix.log`·`EditorBuild.Presentation.log`다. 컴파일 성공을 화면 검수로 확대하지 않는다.

실행 명령은 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/RunEditor.ps1 -Group NativeFull`이다. UUID 시험 슬롯·`-nowrite`·`-NullRHI`를 사용했다. NativeFull은 `Source/ProjectA/Tests`의 EditorContext 회귀와 메뉴 에셋·체크포인트 효과 계약을 선택하며 별도 프로세스 probe 2개와 ClientContext 전용 패키지 검사는 제외한다. 실행별 `RequestedTests.json`·`Invocation.json`에 정확한 필터·인수를, `Engine.log`·`Report/index.json`에 결과를 보존한다.

최초 `Saved/Automation/Autonomous_20261008/NativeFull.56e41d02035c4695a26f5f120eacce98`는 213개 성공(142성공·71경고 포함 성공), 8실패였다. 저장 거절 이유 누락, 초기화 전 격리 월드의 네이티브 복제 콜백 실행 조건, 가중치 도입 뒤에도 세 방문 안에 서비스가 나온다고 가정한 회복 fixture를 수정했다. 체인 fixture만 `FEditorScriptExecutionGuard`를 사용하며 실제 네트워크 전송을 모사하지 않는다. 회복 fixture는 실제 저장 풀과 `BuildOffers`로 서비스가 제시되는 결정적 시드를 고정하고 공개 구매·저장 경로를 검증한다.

수정 후 `Saved/Automation/Autonomous_20261008/NativeFull.3f9528c5049e44ec80e34de68ef2b83e`의 `Report/index.json`은 148성공·73경고 포함 성공, 실행된 221개 통과·실패 0·보고서상 미실행 0·진행 중 0, 72.408초를 기록했다. 경고를 0으로 처리하지 않으며 상세 내용은 해당 보고서·로그에 유지한다.

요청 선언 222개와 실제 결과 221개의 차집합에서 `ProjectA.Snapshot.CandidateSelection` 1개의 실행 누락을 확인했다. 같은 이름 아래 새 `LocalSlots` 테스트가 생기면서 부모/자식 경로가 충돌한 경우이며 보고서의 미실행 0만으로 요청 전부 완료로 판정하지 않는다. 기존 테스트의 조건은 유지하고 이름을 `ProjectA.Snapshot.CandidateSelection.InMemory`로 변경했다.

후속 명령 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/RunEditor.ps1 -Group Snapshot`은 `ProjectA.Snapshot.CandidateSelection` 필터를 실행했다. `Saved/Automation/Autonomous_20261008/Snapshot.e684c464b8774a5481e03b4777d341d6/Invocation.json`·`Report/index.json`을 대조하여 `InMemory`·`LocalSlots` 두 실제 테스트명이 모두 성공 결과에 포함됨을 확인했다. 2성공·경고/실패/미실행 0, 0.021초·프로세스 종료 코드 0이다. 전체 221개와 새로 실행된 `InMemory` 1개를 합쳐 고유 Native 회귀 222개가 통과했으며 `LocalSlots` 재검증은 중복 가산하지 않는다. 다음 표는 실제 실행 결과로 이관한 범위다.

| 필터 | 이번 Native 실행으로 확인한 범위 |
|---|---|
| `ProjectA.Persistence.InitialRunAtomicRetry`, `ProjectA.Recovery.NewCoopRoster` | 초기 저장 실패 시 상태·파일·통지 보존과 재시도, 새 협동 한 참가자당 한 캐릭터 검사와 기존 저장 호환 |
| `ProjectA.Snapshot` 9개 | 값 데이터·카탈로그·로컬 슬롯의 태그 조건/중복 ID 거절, 실패 시 출력/난수와 원본 파일 보존, 소모품 차단 및 순서 있는 6개 일반 스킬. 후속 `CandidateSelection.InMemory`에서 전체 부적합 후보·선택 결과의 원본 독립성·동일 시드 재현까지 확인 |
| `ProjectA.Run.EncounterPool`, `ProjectA.Run.Shop.Profiles`, `ProjectA.Run.Shop.RarityProbabilities` | CSV 거절·그룹/등급 우선 추첨·시드 재현·태그 조건·부족 상품·빈 등급/0% 처리·중복 금지·실패 원자성·저장된 정책과 기존 균등/순환 보존 |
| `ProjectA.Run.Target.ItemRewards`, `ProjectA.Run.Target.LevelDesign.SurvivorRestAndAtomicRetry`, `ProjectA.Run.Recovery` | 아이템/골드 결과·수령 저장 실패 재시도와 중복 방지·기존 보상 보존, 생존자 휴식·최대 HP·사망 보존·Snapshot/패배 제외, 서비스 권한·원자 저장·재로드 |

위 Native 결과는 실제 상점 화면·소모 후 메뉴 Continue·강제 승리 없는 80단계 완주·사용자 선호·성능·다중 PC·Steam/PlayFab·패키지 실행의 통과 근거가 아니다. 해당 수용 조건과 제안 7·9~14의 선택은 [TODO](TODO.md)에 보존한다. 서비스 자격·유료 리소스를 활성화하거나 미정 MMR 정책을 확정하지 않았다.

동일 PC 협동 검수는 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/RunEditor.ps1 -Group Coop`로 실행했다. `Saved/Automation/Autonomous_20261008/Coop.e90d4eff1cf44c659cd1f1097ca0bc5a/Invocation.json`·`Engine.log`·`Report/index.json`이 정확한 실행 인수와 결과를 보존한다. `ProjectA.RunRoundPIE.1Players`·`2Players`·`4Players`는 각각 284.666초·147.329초·93.051초에 성공했고 합계 525.047초, 실패 0·미실행 0·진행 중 0·프로세스 종료 코드 0이다. 세 시험 모두 경고 포함 성공이며 각 2건, 합계 6건의 경고는 보고서에 보존한다. CrowdManager/RecastNavMesh 경고와 1인 종료 검사에서 의도적으로 잘못된 몽타주 속도를 주입한 경고를 무경고 성공으로 기록하지 않는다.

각 실행은 prototype의 실제 10전투·중간 9상점을 완료하여 합계 30전투·27상점을 진행했다. 1인은 Standalone, 2/4인은 같은 에디터 프로세스 안의 별도 PIE 월드와 실제 Listen Server·Client NetDriver를 사용했다. 수동 시험 계정·원래 소유자 바인딩, 시험 저장의 검 스킬 추가·1G 차감, 매 전투 아군 최대/현재 HP 10000과 적 HP `min(기존 HP, 검·비무장 공격의 최소 Power)` 보정이 있는 개발 fixture다. 원본 공격·충돌 프로필은 유지하며 모든 전투에서 검·비무장 공격을 실제 시전한다. 일반 메뉴의 새 Run 생성이나 CSV 난이도의 무보정 완주 시험이 아니다.

서버/원격의 CombatId·계획 revision·단계·유닛 소유 슬롯·HP·스킬/대상, 원격 AP 차감·몽타주·SAP 보행, 개인 골드 카드의 한 번 지급·복제와 수령 전 Continue 잠금, Host 전용 결과/상점 진행을 검사했다. 매 결과를 새 Run subsystem으로 읽어 Run ID·인원·HP·개인 골드·수령 기록을 대조했다. 이는 같은 프로세스의 결과 저장 재로드이며 독립 프로세스 재개·관리 Host 승계 시험과 구분한다.

중간 상점은 기존 `Shop_02`를 반복 선택했다. 공통 `RunEncounterPIEHelpers`가 각 Host/Client의 카메라 blend 종료·자기 월드의 선택 태그에 맞는 NPC ViewTarget·저장된 상점 제목, Client 선택/퇴장 권한 거절, 퇴장 후 `GameplayEncounterOverview` 복귀를 확인했다. NPC 전체 5무대·화면비·리롤/인벤토리 중 카메라·성능·수동 조작 품질은 이 결과에 포함하지 않는다. 실제 메뉴 lobby 생성/접속·Steam 두 PC 인증·온라인 저장/재개·영구 AI 전환·관전은 미완료이며 제안 7의 선택 체크와 남은 수용 조건을 유지한다.

정상 메뉴 관측은 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/RunEditor.ps1 -Group NormalTarget`로 실행했다. 근거 루트는 `Saved/Automation/Autonomous_20261008/NormalTarget.96eda816f0514d2580d5b9eba22705f8`이며 `Invocation.json`·`Engine.log`·`Report/index.json`과 `NormalRun/ProjectA_Automation_NormalRun_96eda816f0514d2580d5b9eba22705f8/NormalTargetRun.json`을 대조했다. `ProjectA.TodoReview.NormalTargetRun`은 126.984초, 오류 0·경고 3·프로세스 종료 코드 0으로 관측 계약을 통과했다. 경고는 CrowdManager/RecastNavMesh 2건과 자연 패배를 완주 성공으로 해석하지 말라는 안내 1건이다.

정상 새 게임의 기본 캐릭터 4명·직접 조작 1명·동료 AI 3명을 사용하고 HP·피해·AP·상점 재고·골드·적 편성·결과를 보정하지 않았다. 공개 계획/Ready 요청과 실제 UI delegate로 16라운드를 제출했으며 9전투·30선택 완료 후 `TargetCombat_10`에서 자연 패배했다. 결과는 `ObservedNaturalDefeat`, `passed_observation_contract=true`, `completed_all_eighty_stages=false`다. 2026-10-04의 6승 후 패배와 별개의 최신 관측이며 60선택·20전투 완주나 최종 밸런스 승인이 아니다.

첫 승리 `TargetCombat_01`의 보상 선택 전 PIE 종료·재시작 후 실제 메뉴 Continue가 저장된 파티·진행·보상 후보를 복원했고 `actual_menu_continue_verified=true`다. 전체 진행에서 안정 경계 저장을 126회 역직렬화하여 공개 상태와 대조했다. 소모품은 이후 `TargetCombat_05`부터 실제 사용되었으므로 이 메뉴 Continue를 소모 후 메뉴 재개 검증으로 계산하지 않는다.

회복약 UI 계획/Ready 요청 7회가 모두 실제 GAS 적용으로 이어졌다. 각 관측은 동일 효과 문맥, GAS 적용 1회·AP 1 차감·관련 HP 변화와 저장 경계의 재고 1 감소를 확인했다. 실제 서비스 UI 구매는 회복 2회·소모품점 7회다. 아이템 구매 요청·장비 변경 요청은 각각 0회, 부활 구매도 0회여서 해당 동선은 검증 완료로 처리하지 않는다. 이 첫 관측은 실제 delegate와 공개 요청의 실행이며 물리 마우스 조작·체감 품질·소모 후 메뉴 Continue·협동의 다른 무대/화면비·80단계 완주를 완료 처리하지 않았다. 소모 후 Continue·부활의 후속 결과는 아래 최종 c8ac 관측과 구분한다.

NPC 화면은 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/RunEditor.ps1 -Group Presentation`으로 확인했다. 최초 `Presentation.6e88b136bc014127bf4fc1f4ec5511fa`는 요청한 창 크기와 실제 viewport 크기의 차이를 검사하여 실패했다. 저장된 사용자 설정은 변경하지 않고 시험 창의 실제 viewport를 확인하며 제한된 횟수로 크기를 맞추도록 수정했다. 후속 `Presentation.d2feb3b5e6014a5db695bf14b46e51ae`는 자동 검사를 통과했으나 실제 PNG 검토에서 회복소 지붕의 얼굴 가림을 발견했다. 투영 좌표만으로 실제 가림까지 판정할 수 없음을 보고서에 명시했다.

`EncounterPrototypeStage`의 회복소 지붕 중심을 223→310cm로 높이고 기둥 높이를 맞췄다. 기존 재구성 경로를 사용하므로 Content 재작성이나 사본 에셋 추가는 없다. `EditorBuild.Shelter.log`의 Development Editor / Win64 컴파일 15.75초가 성공했다. 최종 `Presentation.5171c74f09ad41e9aca5758847622f64/Report/index.json`은 114.702초·실패 0·경고 포함 성공 1을 기록했다. 기본·등급별·태그별·회복·소모품·부활 6종을 960×720·1280×720·1260×540에서 확인한 18경우와 Shop/Inventory PNG 36개를 해당 UUID 하위 폴더에 보존했다.

각 경우에 실제 메뉴의 4캐릭터 생성, 저장된 선택의 태그에 맞는 NPC·카메라 blend 완료·활성 패널, 실제 인벤토리 열기/닫기 delegate, 진열/revision/카메라 보존, 퇴장 후 전경 복귀와 NPC tick 정지를 검사했다. 지정 종류가 제시되도록 시험 저장의 초기 시드와 그 시드로 계산한 후보만 고정한 fixture이며 자연 출현 빈도·구매·리롤·장착·스크롤 입력을 검증한 것은 아니다. `NPCVisualReview.Root.json`의 회복소 3개와 `NPCVisualReview.Agent.json`의 나머지 15개 Shop PNG를 직접 검토하여 얼굴·눈·오른쪽 패널의 가림 해소를 확인했다. 상점 18화면의 정적 시각 검토와 Inventory 18화면의 자동 상태/캡처를 구분하며 실제 입력 품질·협동의 다른 무대·성능은 TODO에 유지한다.

새 Development / Win64 패키지는 `powershell -ExecutionPolicy Bypass -File Saved/Automation/Autonomous_20261008/Package.ps1`의 `BuildCookRun -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -unattended -utf8output`으로 생성했다. `Package.Arguments.json`·`Package.Build.log`에 전체 명령을 보존하며 코드 빌드 35.86초·쿠킹 130.24초·전체 186.36초, `BUILD SUCCESSFUL`·종료 코드 0이다. `Package.RuntimeCsvManifest.json`에서 런타임 CSV 6개를 확인하고 실행 파일과 컨테이너 등의 SHA256을 `Package.Manifest.json`에 고정했다.

같은 패키지를 `RunPackage.ps1 -Mode Csv`, `TargetWrite`, `TargetContinue`, `LongCheckpoint`, `Quit` 순서로 별도 프로세스에서 실행했다. 각 실행은 왼쪽 모니터·격리 UserDir·UUID 시험 슬롯과 `-nowrite`를 사용하며 전후 패키지 해시를 대조했다. `Package.Csv.f7270fd69d484f4facdf456db35d8af6`는 289개 아이템 CSV/원본 패키지와 61개 스킬·수정 프로필 2검사, `Package.TargetWrite.678bc9db43bc4adca2bb11e5794d2da0`는 공개 기본 Target 초기화·체크포인트 작성, `Package.TargetContinue.1851f39f0f1846229dc13b8ebeb2000a`는 같은 파일의 실제 메뉴 Continue·3선택·첫 Planning 저장, `Package.LongCheckpoint.128bcd2433114260a1648d39b1428738`는 긴 경로의 원자 저장 검사를 통과했다. 각각 0.086초·0.735초·3.139초·0.110초, 합계 5검사 모두 오류·경고 0이며 실제 전투/80단계 완주와 구분한다.

`Package.Quit.f04aeeb3404a48eaadf8766e4c3271eb`는 `-TestExit` 없이 실제 MainMenu Quit delegate로 정상 종료했다. 로그의 성공 결과·`UGameEngine::HandleExitCommand`·`Exiting`과 종료 코드 0을 확인했다. 모든 `Invocation.json`·`Result.json`·`Engine.log`를 위 경로에 보존하며 패키지 검수도 테스트용 저장만 정리한다. 이전 패키지의 실행 이력을 이번 결과로 재사용하지 않았다.

기존 저장 사본 검수는 `RunEditor.ps1 -Group ExistingContinue`로 실행했다. 최초 `ExistingContinue.874326592e9b47668def497e7fee179a`는 원본이 반드시 Combat 단계라고 가정한 기존 fixture에서 실패했다. 실제 원본은 `EncounterChoice`였으므로 검사 대상을 저장된 단계별로 분기하고 공통 파티·진행·편성·카탈로그·거래·규칙 비교를 유지했다. Combat은 기존 실제 유닛/체크포인트 검사를 유지하고 Shop·선택·Map·Result는 해당 화면·카메라 준비를 검사한다. 종료/관리/세션 저장의 올바른 거부는 복원 성공과 분리하고 손상·이관 실패는 통과로 처리하지 않는다. 원본 저장이나 eligibility를 변경하지 않았다.

`EditorBuild.Continue.log`의 Development Editor / Win64 컴파일 17.25초 성공 뒤 실행한 `ExistingContinue.8d9272bf7c6d42f79da9f373e689e573`는 3.789초·실패 0·경고 포함 성공 1·종료 코드 0이다. 동일 이름의 UUID JSON에서 실제 메뉴 delegate·일반 저장 버전 2·4캐릭터·기존 스킬 61개/아이템 289개 카탈로그와 선택 단계 복원을 확인했다. 실제 PNG는 전경과 5/80 선택 화면·이전 스킬상점 후보를 유지했고 `original_bytes_unchanged=true`·`owned_clone_cleaned=true`다. 원본 슬롯을 Unreal에서 직접 로드/저장하지 않았으며 이 사본의 선택 단계 성공을 기존 상점/전투/다른 스키마 전체 호환으로 확대하지 않는다.

소모 후 메뉴 Continue 검수를 추가한 첫 `NormalTarget.086dfa711d4c459a8b23a39894ffb206`는 해당 복원 비교는 통과했지만 전체 관측은 실패했다. 직접 캐릭터의 자연 사망 이후 참가 슬롯 0을 AI 동료의 소유 슬롯 0과 같다고 판단한 fixture가 동료 명령을 요청했고 서버가 정상 거절했다. 검수에서 양수 참가 슬롯·원래 캐릭터 ID·생존 여부를 함께 검사하고 사망 후에는 AI 진행만 관측하도록 수정했다. 실제 권한·AI·HP·결과는 변경하지 않았으며 기존 무진행 180초·전투 100라운드 제한을 유지했다. 수정 후 `EditorBuild.Spectator.log`의 Development Editor / Win64 컴파일 8.26초가 성공했다.

최종 `RunEditor.ps1 -Group NormalTarget`의 `NormalTarget.c8acb8712df742dab5a47c264cefb7b1`는 133.499초·실패 0·경고 포함 성공 1·종료 코드 0이다. 해당 `NormalRun/ProjectA_Automation_NormalRun_c8acb8712df742dab5a47c264cefb7b1/NormalTargetRun.json`은 첫 승리 결과 화면 Continue와 회복약 사용 후 `TargetCombat_05`의 2라운드 Planning에서 실제 PIE 재시작·메뉴 Continue를 각각 확인했다. 복원 전후 전체 파티·진행·체크포인트와 실제 유닛의 HP/최대 HP·AP/SAP·속도·소모품·사망·소유권·타일 점유·계획/Ready가 일치했고 새 CombatId를 사용했다. 전후 PNG와 실제 복원 화면도 확인했으며 임의 시전 중 강제 종료나 온라인 재접속 검증으로 확대하지 않는다.

최종 일반 관측은 실제 17라운드 요청·저장 경계 대조 135회, 소모품 GAS 사용 7회·회복 서비스 6회·소모품점 구매 6회·자연 사망 후 부활 구매 1회를 확인했다. 직접 캐릭터 사망 후 두 전투에서 동료 AI만 관측했으며 동료에게 명령·Ready를 보내지 않았다. 9전투·30선택 뒤 자연 패배했고 80단계는 완주하지 못했다. 아이템 구매/장비 변경 요청은 각각 0회다. 앞선 96eda 관측과 별도 실행으로 보존하며 일반 관측 통과를 완주·최종 밸런스 승인으로 해석하지 않는다. 소모 후 실제 메뉴 Continue의 해당 수용 조건만 TODO에서 이관했다.

모든 실행 종료 후 `Protect.py verify`의 `ProtectedAfter.json`에서 Content 19,809개와 보호한 사용자 저장·설정 52개가 시작 해시와 일치하며 추가 저장 사본 0임을 확인했다. 전체 쿠킹이 변경한 에디터 설정은 ProjectA/ProjectAEditor 모듈 컴파일 시간 두 필드뿐임을 대조한 뒤 백업 바이트로 복원했다. 원본 게임 저장이나 사용자 설정의 다른 변경을 덮어쓰지 않았으며 `CookMetadataRestoration.json`에 근거를 보존했다. 검수 프로세스는 모두 종료하고 에디터·IDE를 열린 상태로 남기지 않았다.

`NativeCoverage.json`은 현재 요청 선언과 성공 결과의 고유 222개 이름이 정확히 일치함을 확인했다. 최종 문서 11개·로컬 링크/앵커 753개·`git diff --check`가 통과했고 TODO의 기존 선택 7개를 보존했다. 새 판단 문서 외에 별도 테스트 문서를 만들지 않았으며 `Docs/TEST_REPORT.md`도 복원하지 않았다. 생성된 솔루션의 변경은 580개 전체 행 집합을 유지한 순서 변경이다. 빌드·실행·시각 검토·보존 검사의 집계와 한계는 `Saved/Automation/Autonomous_20261008/FinalSummary.json`에 보존한다. 작업 시작 시 기존 미커밋 변경은 없었다.

### 9-34 2026-10-09 장착 가능 아이템만 판매

2026-10-09 사용자 요청에 따라 판매 카탈로그의 미지원 항목을 제거했다. 기준 커밋은 `86cbbaf1`이며 작업 시작에 기존 미커밋 변경은 없었다. `WEAPON_ASSETS.csv`는 289→49행(검 2·단검 20·방패 15·활 11·스태프 1), `ENCOUNTER_POOL.csv`는 43→28행·활성 13개다. 후보가 없는 태그 전문점 14개와 주황 등급 전문점 1개를 제거하고 과거 18개 ID·남은 행의 원본 경로·이름·등급·가격·순서를 보존했다. 현재 등급은 흰색 9·초록색 17·파란색 19·보라색 4·주황색 0이며 기존 확률 CSV를 수정하지 않고 유효 후보에서 50:30:15:4를 재정규화한다.

현재 CSV 로더는 모든 상품의 태그 기반 장착 프로필을 요구한다. 신규 진열·리롤·전투 보상은 공통 후보 로직의 장착 필터를 사용한다. `EquipmentSelectionVersion=0`의 이전 진열은 저장 검증을 유지하고 새 진열 생성 시 버전 1로 고정한다. 이전 검·스태프 전문점도 지원 후보 2/1개로 리롤할 수 있으며 후보가 없는 이전 전문점은 빈 진열·퇴장을 허용하고 유료 리롤은 차단한다. UI는 미지원 진열만 숨겨 상품 ID·리비전 연결을 유지하며 서버도 구매를 거절한다. 리롤 가능 여부는 서버가 계산한 값만 표시 뷰로 전달한다. 보유품·장착 인덱스·저장 카탈로그·이미 확정된 전투 보상과 원본 Content는 삭제하지 않는다.

TODO 29의 `SM_Sword` 외형·등급 추가 확인은 해당 미지원 행이 판매 목록에서 제거되어 폐기했다. 외형 확인을 완료한 것으로 처리하지 않으며 원본·사용자 사본도 삭제하지 않는다. 신규 상품과 이전 저장의 화면·입력 확인은 [TODO 29](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)에 유지한다.

| 검증 | 실제 결과·근거 |
|---|---|
| CSV | artifact-tool 원본 import/반환 대조·셀 삭제·재import·preview와 독립 CSV 검사 통과. 49개 uasset 존재, 남은 필드·과거 18행·BOM/CRLF 보존. `Saved/Automation/EquipableCatalog_20261009/Validation.json` |
| 컴파일 | `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 최초 43.10초·호환 수정 후 최종 5.59초 성공. `Saved/Automation/EquipableCatalog_20261009/EditorBuild.log`·`EditorBuild.Compatibility.log` |
| Native 회귀 | 최초 `RunEditor.ps1 -Group NativeFull`은 226개 중 225개 통과·1개 실패(25.619초). 기존 정책의 초기 `Reroll` 호출을 새 UI 제한이 막아 `SelectionVersion=0`의 기존 초기화 분기를 복원했다. 최종 `-Group CatalogRegression` 관련 49개 통과(48 성공·1 경고 성공, 15.631초). 두 보고서의 최신 결과로 고유 226개·누락/미해결 실패 0개를 확인했다. 근거: `Saved/Automation/EquipableCatalog_20261009/NativeCoverage.json`과 `NativeFull.3b7bb48a262347e5961fa78e08187072/Report/`, `CatalogRegression.41e1972e55914e9cb02966d1c5c02f1b/Report/` |
| 정적·보존 | 독립 코드·문서 검토, 문서 11개·로컬 링크 757개·diff 검사 통과. 원본 Content 19,809개·사용자 저장/설정 52개의 해시 동일, 추가 시험 저장 0개. TODO 선택 7개 보존. `Saved/Automation/EquipableCatalog_20261009/FinalSummary.json`·`ProtectedAfter.json`·`DocumentationValidation.json` |

현재 변경의 실제 상점 화면·물리 입력·다중 PC·패키지 실행은 검수하지 않았다. 이전 실행 이력은 최신 변경의 작동 확인 근거로 대체하지 않는다.

### 9-35 2026-10-09 지하 미로 갈림길 연출

2026-10-09 사용자 요청에 따라 인카운터의 공중 전경·외부 NPC 무대 이동을 지하 미로 갈림길 연출로 변경했다. 후보 순서 0=좌회전·1=직진·2=우회전을 공통 `EncounterDungeonLayout`에서 정의하고 기존 가중치·태그·EncounterId와 Host 선택 권한을 유지한다. `AEncounterDungeonRoute`가 로컬 런타임 native 도형으로 벽·바닥·천장·코너를 구성하며 기존 NPC 무대의 설정·재질을 참조한 표시 Actor를 생성한다. 원본 Content·Gameplay 맵·전투 Arena·Grid·체크포인트 좌표·저장 스키마를 변경하지 않는다.

실제 선택이 저장·복제된 경우에만 2.8초 통로 이동·코너 회전을 표시한다. 상점 Continue와 늦게 접속한 클라이언트는 도착점에 바로 배치하고 같은 방문의 구매·리롤 갱신은 연출을 재시작하지 않는다. 퇴장 뒤 다음 갈림길 전환에는 짧은 페이드를 사용한다. 선택 UI는 전체 배경 없이 하단 3열 카드를 표시하며 원래 후보 ID로 요청한다. 이동 중에는 기존 전환 잠금으로 UI를 숨기고 도착 후 오른쪽 구매 패널을 연다. 연출이 없는 맵의 세로 선택·상점 표시와 전투 카메라 복귀는 유지한다.

기준 커밋은 `8f9802be`이며 작업 시작에 기존 미커밋 변경은 없었다. `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`의 최초 빌드는 새 소스에 따른 Unity 묶음 변경으로 기존 `RunEncounterPool` 내부 이름이 다른 CSV 파일과 충돌했다. 후보 개수·태그 파서의 내부 이름만 구분한 뒤 최종 컴파일·링크가 12.47초에 성공했다. 로그는 `Saved/Automation/DungeonEncounter_20261009/EditorBuild.log`·`EditorBuild.Final.log`다.

경로 축 정렬·통과 셀 바닥·도착점과 NPC 카메라 위치·방향 순서·잘못된 방향을 검사하는 Native 테스트 2개(`ProjectA.Run.EncounterPresentation.DungeonLayoutGeometry`, `DungeonDirectionContract`)를 작성하고 컴파일했다. 기존 PIE·패키지 검수는 표시된 방향 버튼·이동 완료·원래 선택 ID·퇴장 정리를 확인하도록 수정했다. 독립 코드 검토와 문서 11개·로컬 링크 764개·diff 정적 검사를 통과했으며 TODO의 기존 선택 7개와 미완료 항목을 보존했다. 근거는 `Saved/Automation/DungeonEncounter_20261009/DocumentationValidation.json`·`StaticChecks.json`이다.

현재 규칙에 따라 게임·PIE·자동화 테스트·패키지를 실행하지 않았으며 에디터·IDE도 열지 않았다. 과거 9-32·9-33의 공중 전경/NPC 화면 성공은 당시 코드의 이력으로 보존하며 새 미로의 가림·이동·Continue·협동 검증으로 대체하지 않는다. 남은 확인은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완), 현행 구성은 [PROJECT_PLAN 4-13](PROJECT_PLAN.md#4-13-통합-gameplay와-npc-상점)과 [UI_README](UI_README.md)에 기록한다.

### 9-36 2026-10-09 시드별 미로 계획과 구간 재사용

2026-10-09 사용자 요청에 따라 새 Run의 전체 논리 미로를 생성 시 고정하도록 확장했다. `DungeonState` 버전 1은 RunId에서 만든 별도 지형 시드, 일반 Target 60방문·명시적 프로토타입/개발 협동 9방문의 통로 변형과 순서 있는 후보 ID 3개를 저장한다. 8개 통로 변형은 기존 좌회전·직진·우회전 방향을 유지하며 변형 0은 이전 고정 좌표와 같다. 기존 태그·가중치 추첨으로 후보를 고정하고 이후 실제 후보는 저장된 ID를 당시 Run의 풀에서 해석한다. 상품·전투·보상 난수는 소비하지 않는다.

새 필드는 기존 SaveGame 안에 추가하고 바깥 저장 파일 버전은 유지한다. 누락된 계획은 버전 0·빈 값으로 읽어 이전 저장의 고정 배치와 선택 흐름을 소급 변경하지 않는다. 서버가 확정한 계획을 읽기 전용 GameState 뷰로 복제하고 Continue에서도 복원한다. 진행·구매·소유권·Host 선택 권한과 원본 Gameplay·Engine 기본 도형·기존 NPC 설정을 유지하며 신규 에셋은 추가하지 않는다.

로컬 컨트롤러는 서로 떨어진 현재·다음 구간 Actor를 최대 2개 보관하고 방문 이동 때 교환·재사용한다. 같은 변형은 기존 지형을 유지하고 다른 변형만 소유 NPC·ISM 인스턴스·조명을 정리해 재구성한다. 준비 구간은 메시·조명·NPC까지 숨기며 새로 생성한 구성요소에도 표시 상태를 상속한다. 전투 등 비인카운터 단계에는 두 구간 모두 숨긴다. 실제 선택 저장 후 2.8초 이동, 상점 재개의 도착점 복원, 다음 갈림길의 짧은 페이드와 하단 3열/오른쪽 거래 UI는 유지한다.

기준 커밋은 `a05c527f`이며 작업 시작에 기존 미커밋 변경은 없었다. `Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 최초 45.17초, 검수 소스 보완 후 최종 40.41초에 성공했다. 로그는 `Saved/Automation/SeededDungeon_20261009/EditorBuild.log`·`EditorBuild.Final.log`다.

Native 회귀 3개(`ProjectA.Run.Dungeon.FrozenBoundariesAndRandomIsolation`, `LegacyCorruptionAndAtomicFailures`, `FrozenPlanSerialization`)를 작성하고 기존 배치 회귀 2개를 8개 변형으로 확장해 컴파일했다. 저장 손상·시드 분리·60/9방문·방향별 후보 순서·SaveGame 왕복과 실제 Continue/표시 뷰의 계획 보존을 검사하도록 구성했다. 기존 PIE 검수에는 저장 변형과 이동 경로 일치·구간 캐시 2개 상한을 추가했다. 독립 코드 검토와 문서 11개·로컬 링크 767개·diff 정적 검사를 통과했으며 TODO의 선택 7개와 미완료 항목을 보존했다. 근거는 `Saved/Automation/SeededDungeon_20261009/DocumentationValidation.json`·`StaticChecks.json`이다.

게임·PIE·자동화 테스트·패키지는 실행하지 않았으며 에디터·IDE도 열지 않았다. 이전 9-35의 컴파일과 과거 화면 성공을 이번 변경의 실행 근거로 사용하지 않는다. 동일 시드/재개·구간 수·숨김·2인 경로 일치의 실제 확인은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 유지한다.

### 9-37 2026-10-09 스킬 수치·등급과 장비별 추첨

2026-10-09 사용자 수치 설계 위임에 따라 기존 무기 후보 62종의 고유 스킬 등급·기본 가중치·위력·AP/SAP·선딜을 `SKILL_BALANCE.csv` 11열에 정리하고 장비 5등급×스킬 5등급의 가중치를 `SKILL_RARITY_PROBABILITIES.csv` 25행·6열로 분리했다. 기준 커밋은 `00773132`이며 작업 시작의 Git 상태는 깨끗했다. 등급 분포는 흰색 7·초록색 15·파란색 17·보라색 15·주황색 8이다. 기본 가중치 1·AP 1·SAP 0과 기존 선딜을 유지하고 범위/연쇄의 대상 수를 고려해 위력을 배정했다. 별도 기본 공격 2·몬스터 12·회복 소모품 1종은 유지한다. 수치와 확률표는 [GAME_DESIGN 2-4-6](GAME_DESIGN.md#2-4-6-스킬-등급과-장비별-추첨-확률)을 따른다.

추첨은 기존 무기 GameplayTagQuery를 만족하는 양수 후보의 스킬 등급만 재정규화하고 등급 안에서 기본 가중치로 중복 없이 선택한다. 후보 개수로 등급 확률이 달라지지 않으며 사본당 1스킬을 유지한다. 활의 흰색/파란색 두 후보와 석궁의 흰색 한 후보도 같은 규칙을 사용한다. 장비 등급별 최종 가중치는 흰색 `80/18/2/0/0`, 초록색 `45/40/13/2/0`, 파란색 `18/32/38/11/1`, 보라색 `5/15/35/38/7`, 주황색 `1/4/20/45/30`이다.

새 Run은 규칙 `SchemaVersion=1` 안에 `BalanceVersion=1`·후보 수치·가중치를 저장하고 사본의 `SkillBalanceVersion/GrantedSkillBalances`에 표시·검증용 결과를 보존한다. 이전 버전 0의 풀·원본 수치·사본은 유지하고 CSV를 재개 때 소급 적용하지 않는다. 파서는 누락·중복·잘못된 ID/경로/태그·수치·가중치 합계를 거절하고 실패 시 기존 상태를 보존한다. 두 CSV를 UFS RuntimeDependency에 포함한다. UI는 서버가 확정한 스킬 등급·위력·비용을 사본에서 표시한다.

전투와 체크포인트 검증·복구는 현재 Run의 고정 수치를 양 팀의 같은 SkillId에 동일 적용한다. Snapshot 자체의 스킬 목록·캐릭터 수치는 변경하지 않으며 상대 Run의 과거 수치별 경쟁 재현이나 온라인 검증을 추가하지 않는다. GAS 태그·효과·충돌·범위·체인·몽타주 시점과 원본 에셋은 유지한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 최초 44.51초, 독립 검토 보완 후 최종 17.71초에 성공했다. 로그는 `Saved/Automation/SkillBalance_20261009/EditorBuild.log`·`EditorBuild.Final.log`다. 기본 시험 Query만 전환해 별도 GAS 조건을 보존하고, 후보의 다중 등급 태그를 거절하며 실제 선택 등급과 후보 등급도 대조한다.

Native 회귀 5개(`ProjectA.Run.SkillBalance.CsvAtomicityAndFrozenSave`, `EquipmentWeightsAndFrozenCopies`, `ProjectA.Combat.SkillBalance.FrozenProfileContract`, `ProjectA.Checkpoint.SkillBalance.SharedCostsAndLegacy`, `SerializedFrozenSnapshot`)와 기존 무기 규칙 검수 보완을 작성·컴파일했다. CSV 실패 원자성·등급별 추첨·사본 수치 변조·기존 버전·SaveGame 왕복·양 팀 AP/SAP·Snapshot 계획을 다룬다. 정상 Run PIE 검수의 장비·보상 평가도 저장된 수치를 사용하도록 갱신했지만 실행하지 않았다.

CSV 작성 도구의 숫자형·재읽기·미리보기와 별도 CSV 정적 검증에서 62개 ID/원본 경로·등급 분포·25개 가중치·행 합계 100을 확인했다. 문서 11개·로컬 링크 784개·전체 diff 정적 검사를 통과했으며 TODO의 선택 7개와 미완료 37개를 보존했다. 근거는 `Saved/Automation/SkillBalance_20261009/Validation.json`·`IndependentValidation.json`·`DocumentationValidation.json`·`StaticChecks.json`이다. 원본 에셋 변경·기존 미커밋 변경은 없다.

게임·PIE·자동화 테스트·패키지는 실행하지 않았으며 에디터·IDE도 열지 않았다. 이전 이력의 성공을 이번 변경의 동작 근거로 사용하지 않는다. 실제 화면·등급 추첨·CSV 변경 후 재개·전투 수치와 체크포인트 복원은 [TODO 29](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)에 유지한다.

### 9-38 2026-10-09 PvE 하·중·상 선택

2026-10-09 요청에 따라 새 기본 싱글 Run의 각 PvE 직전에 좌회전·하/직진·중/우회전·상 3후보를 제공한다. 시작 기준은 `a2728329`이며 기존 미커밋 변경은 없었다. 기존 Map 진입에 선택을 통합하여 서비스 60방문·전투 20회·80단계를 유지한다. Snapshot·이전 저장·맞춤 정의·개발 협동은 원래 진입 정책을 유지한다. 실제 구간 이동을 추가하지 않고 갈림길의 선택 확정 후 기존 전투 카메라로 전환한다.

[PVE_DIFFICULTIES.csv](../DataCatalogs/PVE_DIFFICULTIES.csv) 6열·3행에 HP/속도/골드 배율을 하 `0.8/0.9/0.75`, 중 `1/1/1`, 상 `1.3/1.1/1.5`로 기록하고 패키지 UFS 의존성에 포함했다. 항상 세 후보를 제공하므로 등장 확률은 두지 않는다. `RunPveDifficulty` 공통 규칙은 GameplayTag로 선택을 해석하고 미리보기·스폰·체크포인트 불변 스탯·보상 생성/검증에 같은 값을 적용한다. 기준 몬스터 편성·태그 조건·피해·AP/SAP·아이템 등급 확률·성장·휴식은 유지한다. [수치·저장 기준](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)

Run의 독립 정책 버전 1에 CSV 규칙과 PvE별 선택 태그를 저장하고 이전 버전 0은 원래 값을 사용한다. 요청의 현재 노드·난이도·선택 수를 검사한 뒤 준비와 함께 메모리에 반영하며 준비 실패 시 선택을 제거한다. 기존 Ready 경계부터 전투·선택을 함께 영속 저장하므로 클릭 즉시 저장을 추가하지 않는다. CSV 변경은 이후 새 Run에만 적용한다. 표시 View는 값만 복제하고 명령의 기존 권위·Host 조건을 유지한다. 싱글 Target을 협동 지원 완료로 확대하지 않는다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크를 최종 6.88초에 통과했다. 첫 빌드에서 발견한 신규 회귀의 int32/int64 비교 오버로드 모호성을 명시적인 예상값 자료형으로 수정했다. 로그는 `Saved/Automation/PveDifficulty_20261009/EditorBuild.log`·`EditorBuild.Final.log`다.

Native 회귀 4개(`ProjectA.Run.PveDifficulty.StrictCsvAndAtomicity`, `PreviewScalingAndFailurePreservation`, `ProgressAndFrozenSave`, `ProjectA.Run.Target.PveDifficulty.ChoiceAbortAndFrozenSave`)를 작성·컴파일했다. CSV 실패 원자성·모든 구간 배율 상한·미리보기/해석 일치·20개 진행 경계·SaveGame 왕복·하/상 선택·Abort 저장 실패와 재시도·위조 선택 거절을 검사하도록 구성했다. 기존 Target·정상 PIE·패키지 검수 코드는 PvE만 명시적으로 중 난이도를 선택하도록 보완했다. 회귀 코드는 실행하지 않았다.

CSV 숫자형 작성·export/reimport·미리보기와 독립 정적 검사, 문서 11개·로컬 링크 792개·전체 diff 검사를 통과했다. TODO의 기존 선택 7개·미완료 37개를 보존하고 사용자 확인 1개를 추가했다. 근거는 `Saved/Automation/PveDifficulty_20261009/DifficultyCsvValidation.json`·`DifficultyCsvPreview.png`·`DocumentationValidation.json`·`StaticChecks.json`이다. 독립 코드 검토에서 선택·스폰·보상·체크포인트 복원의 배율 일치와 기존 저장 경로를 확인했다. Content·원본 에셋 변경은 없다.

게임·PIE·자동화 테스트·패키지는 실행하지 않았으며 에디터·IDE도 열지 않았다. 실제 카드 입력·난이도 체감·Continue·준비 실패 복구 확인은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다. 이전 실행 성공을 최신 동작 확인으로 대체하지 않는다.

### 9-39 2026-10-09 라이브러리 NPC와 서비스 무대

2026-10-09 요청에 따라 기존 기본 도형 NPC의 표시를 설치된 라이브러리 원본으로 교체했다. 시작 기준은 `36eb8879`이며 기존 미커밋 변경은 없었다. Primitive 캐릭터 4종과 Fantasy Dwarf, 각 원본 스켈레톤의 idle 2종, Fantastic Dungeon·Dungeon Modular·PurePoly 소품 22종을 직접 참조한다. 네이티브 `UEncounterStageVisualCatalog`의 GameplayTagQuery·우선순위로 6개 인카운터 그룹을 5개 외형에 연결하고 총 34개 소품을 배치한다. [역할별 구성](PROJECT_PLAN.md#4-13-통합-gameplay와-npc-상점)

회복소는 치유사·벤치·모닥불·회복 물품, 소모품점은 약초사·약병·책장·가마솥, 부활소는 뿔 장식 사제·제단·의식서·촛대를 사용한다. 상인은 거래 테이블·장부, 대장장이는 작업대·화덕·검 진열로 구분한다. 원본 재질을 유지하고 bounds를 기준으로 균일 배율·바닥 위치를 계산한다. 소품 받침 높이와 공통 바닥 크기도 배치에 맞췄다. 원본 에셋 복제·수정·리타깃·맵 재작성은 없다.

무대 재구성은 직접 생성한 소품·부품만 정리하며 태그 설정과 카탈로그를 미로 표시 Actor에 복사한다. 숨김·퇴장 시 실제 skeletal component의 idle 재생·tick을 멈추고 숨긴 무대의 조명도 끈다. 장식의 충돌·navigation·복제는 비활성화한다. 로컬 카메라 경로·거래·저장·Host 권위는 유지한다. `UProjectAAssetManager::ModifyCook`은 선택된 원본 패키지 29개만 열거하며 원본 팩 전체를 추가하지 않는다. 외형 참조 실패 또는 명시적 비활성에서는 기존 도형 표현을 사용한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 최초 39.94초, 소품 높이·재구성 보완 후 12.62초, 바닥 크기 최종 변경 후 4.88초에 성공했다. 로그는 `Saved/Automation/NpcLibrary_20261009/EditorBuild.log`·`EditorBuild.Final.log`·`EditorBuild.FinalPlatform.log`다. Native 회귀 `ProjectA.Run.EncounterPresentation.LibraryProfilesAndCookReferences`와 기존 PIE 검수 보완을 작성·컴파일했다. 실제 원본 NPC·idle·소품·비충돌·퇴장 정지와 실제 얼굴 뼈의 투영을 확인하도록 구성했으며 실행하지 않았다.

원본 내장 썸네일을 확인하고 현재 파일의 스켈레톤 참조·SHA256과 기존 Asset Registry 정보를 대조했다. 선택 캐릭터 5개와 idle의 공통 스켈레톤 참조를 확인했으며 Primitive 정면은 저장된 ref pose·기존 프리뷰 설정으로, Dwarf 방향은 같은 팩의 기존 사용 코드로 판단했다. Dwarf 정면은 직접 렌더 확인이 아닌 추론이다. 원본 경로 29개·소품 34개·받침 높이·바닥 포함 범위와 대략적인 카메라 시선/소품 AABB를 정적으로 검사했다. 저장된 bounds를 사용하며 검 1개는 요청 크기를 보수적 상자로 대입했다. 이 계산은 실제 idle·옷·UI 가림 검수를 대신하지 않는다. 근거는 `Saved/Automation/NpcLibrary_20261009/NpcRecommendations.json`·`ThumbnailSources.json`·`StaticChecks.json`이다.

문서 링크·전체 diff 검사를 통과하고 TODO의 기존 선택 7개·미완료 38개를 보존했으며 사용자 확인 1개를 추가했다. 문서 검사 근거는 `Saved/Automation/NpcLibrary_20261009/DocumentationValidation.json`이다. 게임·PIE·자동화 테스트·cook·패키지는 실행하지 않았으며 에디터·IDE도 열지 않았다. 실제 방향·가림·idle·반복 방문·Continue·2인 외형·로딩과 성능은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 미완료로 유지한다. 이전 도형 NPC 실행 성공을 이번 외형의 검증으로 대체하지 않는다.

### 9-40 2026-10-09 전투장 바닥과 표시 폴리싱

2026-10-09 전투맵 품질과 바닥 개선 요청을 반영했다. 시작 기준은 `06f5c435`이며 기존 미커밋 변경은 없었다. 현재 Run은 전투마다 맵을 전환하지 않고 통합 Gameplay 중앙 PineRidge의 배치된 아레나를 재사용한다. 기존 `NormalTarget` 전투 캡처와 원본 메타데이터를 대조하여 `T_Dirt_basecolor`의 G16·비감마 흑백 점무늬가 별도 색상 보정 없이 RGB로 표시되는 것을 확인했다. 현재 자료로 바닥의 높이 충돌이나 메시 구멍을 확정하지는 않았다.

프로젝트 전용 표면 Material 9개를 같은 경로에 저장했다. PineRidge는 낮은 대비의 흙색 범위·세부 비중 0.18·600cm 반복으로 변경했다. 나머지 8개는 원본 RGB를 유지하며 색조·거칠기·밝기 변화를 조절했다. 공통 큰 무늬는 기존 원본 `T_TilingNoise03_M` 512×512를 약 2,600cm마다 샘플링하고 기존 노멀 4개는 0.35~0.40 강도로 혼합·정규화한다. 환경 비교 12맵과 통합 배치가 공유하는 재질에 적용되며 던전 전용 원본 바닥 메시·재질은 교체하지 않았다. [수치·구성](PROJECT_PLAN.md#4-4-환경-비교-레벨)

`CombatGridTile`은 빈칸/점유/이동/스킬의 알파를 0.24/0.42/0.65/0.78로 구분하고 hover 증가·상한과 전열 보호의 옅은 착색을 같은 표시 갱신 경로로 처리한다. 원본 스프라이트와 상태 우선순위는 보존한다. `CombatArena`는 명시적인 채도 재정의가 없는 카메라에만 0.88을 적용하며 재입장 때 누적하지 않는다. 원본 텍스처·맵 배치·재질 인스턴스 44개·광원·노출·Grid 좌표·클릭 박스·충돌·navigation·저장·Replication 계약은 유지했다.

`ConfigureEnvironmentSurfaces.py`에 색상 범위·원본 RGB의 큰 무늬·노멀 강도와 해당 그래프 검사를 추가했다. `-EnvironmentSurfacesOnly`는 mutable 출력 범위를 9개 표면으로 제한하고 나머지 MI를 보호 해시에 포함한다. `-EnvironmentSurfacesReportDir`는 작업공간 내부 상대 경로만 허용한다. 기존 옵션을 생략한 그래프와 생성 경로를 보존하며 표면 이외의 에셋을 다시 생성하지 않는다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 36.26초에 성공했다. 로그는 `Saved/Automation/BattleFloor_20261009/EditorBuild.log`다. 이후 `UnrealEditor-Cmd.exe ProjectA.uproject -run=pythonscript -script=Source/ProjectAEditor/Scripts/ConfigureEnvironmentSurfaces.py -EnvironmentSurfacesRebuild -EnvironmentSurfacesOnly -EnvironmentSurfacesReportDir=Saved/Automation/BattleFloor_20261009 -EnablePlugins=PythonScriptPlugin -unattended -nop4 -NullRHI -NoTraceServer -AssetGatherAll=false`로 에셋 작성만 수행했다. 실제 절대 경로를 쓰는 재현 명령은 [에셋 도구 20번](../Source/ProjectAEditor/Scripts/README.md)에 기록한다.

에셋 작성 커맨들릿은 12.33초·종료 코드 0·오류 0·경고 0으로 표면 9개를 저장하고 노드·상수·입력 연결·원본 샘플러·출력을 확인했다. 보호 파일 2,564개의 SHA는 동일하며 변경 Content는 지정된 기존 Material 9개뿐이다. 결과는 `Saved/Automation/BattleFloor_20261009/SurfacesConfiguration.json`, 로그는 `SurfacesAuthor.log`·`SurfacesAuthor.Console.log`다. NullRHI 작성 중 재질 컴파일 API 오류는 없었지만 GPU shader 실행·화면 평가·별도 프로세스 재로드는 수행하지 않았다. 원본 조사 근거는 `SurfaceTextureMetadata.json`·`MacroMaskCandidates.json`이며 작성 전 9개 재질의 해시는 `BeforeMaterials.json`에 기록했다.

독립 읽기 검토와 Python 구문·명세·변경 경로·문서 링크·diff 정적 검사를 수행했다. TODO의 기존 선택 7개·미완료 39개를 보존하고 사용자 확인 1개를 추가했다. 근거는 `Saved/Automation/BattleFloor_20261009/StaticChecks.json`·`DocumentationValidation.json`이다. 게임·PIE·Unreal 자동화 테스트·cook·패키지·신규 화면 캡처는 실행하지 않았고 Editor 창·IDE도 열지 않았다. 실제 바닥의 반복·이음새·접지, 타일 상태의 구분·입력·Continue·2인 표시와 성능은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다. 과거 캡처는 문제 진단에만 사용하며 최신 결과의 검수 근거로 대체하지 않는다.

### 9-41 2026-10-09 라이브러리 활 공격 애니메이션

활 공격 애니메이션 적용 요청을 반영했다. 시작 기준은 `f6ab4335`이며 기존 미커밋 변경은 없었다. 원본 `ParagonAnimationsRetargetedToManny/SparrowManny/Attack/Primary_Fire_Slow.FBX` 1,608,928바이트를 선택했다. 161개 본 이름과 Manny 구조를 대조하고 FBX 곡선을 정적으로 분석했다. 원본 0.50~1.00초의 재당김을 0.30초에 재생하고 0.00~0.40초의 발사·회복을 이어 총 0.70초로 구성했다. 원본 1.0→0.0초 접합에서 분석한 머리·손 위치 차이는 최대 약 0.12cm이며 실제 재생 평가와 구분한다. 조사 근거는 `Saved/Automation/BowAnimation_20261009/Recommendations.json`·`FbxStaticCurves.json`·`HandPoseSamples.json`이다.

공통 Manny 시퀀스·몽타주와 구형 SkeletonGuard 시퀀스·몽타주 4개(546,176바이트)를 원본 팩 하위 구조의 프로젝트 경로에 작성했다. 현재 남녀 몸체는 기존 Manny 호환을 사용하며 구형 Snapshot만 기존 `RTG_SwordEnemy`로 시퀀스 1개를 리타깃한다. 새 Rig·몸체·스켈레톤 복제는 없다. 기존 `BP_SnapshotOpponent`에는 몽타주 override 한 쌍만 추가하고 기존 외형·스킬·override를 보존했다. 현재 네 직업과 해당 Snapshot은 같은 스킬 참조를 사용한다. 이전 클래스·Rig의 정적 참조 근거는 `LegacySkeletonGuardStatic.json`이다.

정밀 화살·화염 화살비 DA는 `CastMontage`만 변경했다. 태그·위력·AP/SAP·0.3초 선딜·화염 화살비의 발동 후 0.4초 판정 지연·GAS·서버 발사·저장 형식은 유지한다. `DrGameSkillSpecs.json`과 생성 도구는 몽타주 참조를 재현하며 석궁 작성 도구는 활 당기기 참조를 제거한다. runtime/RPC 변경은 없다. 선딜 변경 시 몽타주 명세의 준비 시간도 검토하며 자동 속도 조절은 구현하지 않았다. [현행 구조](PROJECT_PLAN.md#4-12-타겟행동-세부-규칙)

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 21.04초에 컴파일·링크 성공했다. 이후 [에셋 도구 32번](../Source/ProjectAEditor/Scripts/README.md)의 `UnrealEditor-Cmd -run=pythonscript -script=ConfigureBowAnimation.py -NullRHI` 작성은 11.07초·종료 0·오류 0으로 완료했다. 리타깃 중 import-data 의존 로드와 별도 애니메이션 커브 없음 경고 2개가 있었으며 후속 압축 데이터 저장은 완료됐다. 본 트랙은 161개다. 로그는 `Saved/Automation/BowAnimation_20261009/EditorBuild.log`·`Author.log`·`Author.Console.log`다.

작성 과정에서 두 몽타주의 0.70초 구간·비반복·DefaultSlot·블렌드, 공통 AnimBP 슬롯, 남녀 몸체 8포즈 표본·7본의 유효 좌표/배율, 스킬의 나머지 필드와 기존 Snapshot 구성을 검사했다. 원본·비대상 프로젝트 에셋·CSV 보호 파일 1,213개의 SHA가 동일하고 출력은 지정된 기존 3개·신규 4개뿐이다. 근거는 `Author.json`이며 기존 출력 사본은 `BeforeAssets`에 보존했다. 별도 프로세스 재로드·GPU 렌더·실제 게임·PIE·자동화 테스트는 실행하지 않았고 Editor 창·IDE도 열지 않았다.

독립 코드 검토·Python 구문·JSON·참조·diff·문서 링크 정적 검사를 수행했다. TODO의 기존 선택 7개·미완료 40개를 보존하고 사용자 확인 1개를 추가했으며 근거는 같은 폴더의 `StaticChecks.json`·`DocumentationValidation.json`이다. 실제 손잡이·시위·발사 방향·블렌딩·중단/사망·Continue·2인 원격과 구형 Snapshot 재생은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다. 정적 포즈 검사와 과거 전투 성공을 최신 동작 검수로 대체하지 않는다.

### 9-42 2026-10-09 검과 마법 시전 애니메이션

검·마법 시전 적용 및 화면 검수 요청을 반영했다. 시작 기준은 `c9acf62f`이며 기존 미커밋 변경은 없었다. 일반 근접 1개·VFX 참격 4개·전방/빔/체인 마법 27개·광역/회복/보호막 27개의 DA와 생성 명세에 `CastMontage`만 연결했다. 일반 근접 게임 발동 0.23초, 구형 `BPDA_swoard_attack`의 Kwang 몽타주·서버 칼날 추적, 활·석궁·피해·AP/SAP·태그·CSV·GAS·저장·판정 코드는 보존했다. runtime 변경은 StartingStaff 프로필의 표시용 부착 변환 1개다. 내부 `Kind=MELEE`인 빔/체인도 기존 콘텐츠 분류에 따라 마법으로 연결한다.

Greystone `Attack_A_Med`의 준비 0~8/30초와 나머지 베기, `Attack_A_Slow_Recovery`를 연결한다. 일반 근접은 준비 7/30초·총 1.6초로 게임 발동과의 차이는 3.33ms, 참격은 준비 0.3초·총 1.6667초다. Gideon `Primary_Attack_A_Medium`은 준비 0~7/30초를 0.3초로 조정해 총 1.3초, Muriel `ConsecratedGround_Cast`는 준비 0~8/30초를 0.3초에 재생하고 원본 1.6초까지 복귀해 총 1.6333초다. 마법 시퀀스의 오른손 손가락 19개 회전만 선택 임포트한 Greystone 첫 자세로 고정하고 StartingStaff 부착 위치·회전을 닫힌 손바닥 축에 맞췄다. 엄지/검지 간격은 초기 Idle 6.646cm에서 1.378cm로 줄었고, 남녀 동일 부착 좌표의 근거는 `ClosedGripGeometry.json`이다. 손목·팔·기타 회전·위치·배율·시점은 보존하고 구형 SkeletonGuard 필수 결과를 다시 작성했다. 신규는 검 8개·마법 8개, 총 2,590,103바이트이며 원본 FBX·메시·Skeleton·AnimBP는 직접 참조한다. [작성 명세](../Source/ProjectAEditor/Scripts/CombatAnimationSpecs.json)와 `Saved/Automation/CombatAnimations_20261009/GreystoneRecommendations.json`·`GreystoneCandidatePoses.json`에 선택 근거를 보존한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 최종 컴파일·링크는 `EditorBuild.Staff.log`의 12.02초 성공이다. [도구 33번](../Source/ProjectAEditor/Scripts/README.md)의 최종 작성 `Author.Staff.log`는 11.48초·오류 0·경고 5개다. 리타깃 import-data/커브 없음 4개와 엔진 참조 수집 fallback 1개이며 임시 이름 에셋은 남지 않았다. 독립 읽기 전용 재로드 `Reload.Staff.log`는 8.39초·오류/경고 0이다. 작성 보호 1,614개·재로드 전체 1,690개 파일 SHA 불변과 기존 60개·신규 16개 출력 해시 일치, 스킬의 나머지 필드·BP 구성·DefaultSlot·구간·블렌드·남녀 포즈·손가락 38/69프레임을 검사했다. `Author.json`·`Reload.json`·`StaticChecks.json`에 근거를 보존한다. 22개 정적 검사는 Python/JSON 구문·매핑·보존 범위·실제 결과·허용 경고 문구/횟수를 확인했다.

초기 `Visual` 검사는 소품 초기화 순서가 잘못되어 장비 화면의 성공 근거로 채택하지 않았다. fixture를 공식 `GrantEquipment`로 수정한 `VisualEquipment` 화면에서 Kwang 양손 베기가 방패를 머리 위로 올리는 문제를 확인해 Greystone으로 교체하고, 미채택 프로젝트 소유 몽타주 2개는 엔진 참조 확인 후 삭제했다. `VisualFinal`의 첫 손 보정은 확대 화면에서 손목 쪽 부착과 열린 손이 남아 최종 그립 성공 근거로 채택하지 않았다. `Author.IdleGrip.json`·`Reload.IdleGrip.json`에 당시 상태를 보존했다. 초기 Kwang 작성/재로드는 `Author.Kwang.json`·`Reload.Kwang.json`의 당시 결과다. Greystone 최초 재로드의 구간 시작 1 ULP 차이는 `Reload.Diagnostic.log`에서 확인했으며 엔진과 동일하게 앞 구간의 `GetEndPos()`를 사용해 수정했다. 손가락 조회 probe의 `-nowrite` 누락으로 바뀐 에디터 모듈 시각은 시작 SHA와 정확히 일치하는 바이트로 복원했고 생성된 진단 설정은 `ProbeGeneratedConfig`로 이관했다. 최종 사용자 설정·저장 73개 불변 근거는 `UserStateAfter.json`이다.

최종 화면 검수 명령은 `powershell -ExecutionPolicy Bypass -File Saved/Automation/CombatAnimations_20261009/RunReview.ps1 -Label VisualStaff`이다. 새 검수 저장 슬롯·`-nowrite`·`-ProjectAReviewLeftMonitor`로 왼쪽 모니터에 배치한 D3D12/SM6 DebugCombat PIE에서 `ProjectA.TodoReview.CastAnimations`를 실행했다. 남성 검/참격/전방/광역/회복/보호막/활과 여성 검/전방/회복의 10회, 실제 뷰포트 PNG 31장, 자연 재생·발동·AP/GAS·몽타주 종료·원위치 복귀·실제 장비 메시/소켓/렌더 검사가 51.17초에 통과했다. 자동화 결과는 실패 0·RecastNavMesh 없음 경고 1개다. `VisualStaff/Report/index.json`, `VisualStaff/CastAnimations/8212A71944134240A21041B0D4A3DDF6/summary.json`·PNG에 근거를 보존한다. 실행 종료 후 Editor를 닫았으며 IDE는 열지 않았다.

PNG에서 검·방패의 한손 자세, 남녀 몸체의 시전과 복귀, 지팡이 손 모양을 확인했다. 59개 스킬 전체·모든 장비 조합·구형 SkeletonGuard 화면·Continue·사망/중단·2인 원격 검증으로 확대하지 않는다. 근접 검수 카메라의 광역 FX 가림과 기존 골반 높이 투사체 발사 FX는 별도 표현 범위이며, 발사 소켓 개선과 나머지 검수는 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다. 기존 선택 7개·미완료 41개를 보존하고 확인/후속 2개를 추가했으며 문서 링크 근거는 `DocumentationValidation.json`이다.

### 9-43 2026-10-09 상점 아이템 설명 UI

아이템상점의 마우스 설명을 네이티브 `UShopItemTooltipWidget`으로 구현했다. 시작 기준은 `37296652`이며 기존 미커밋 변경은 없었다. 아이콘·등급 이름/색·가격/구매 상태·장착 위치/양손 여부·부여 스킬의 효과와 확정 위력/AP/SAP/선딜·구매 후 장착 안내를 표시한다. 상품 카드에 표준 UMG 툴팁을 연결하고 구매 버튼의 텍스트 툴팁을 비워 비활성 버튼도 같은 설명을 사용한다. 카드별 위젯과 스킬 행을 재사용하며 구매·리롤에는 현재 상품 표시값으로 갱신하고 숨김·서비스 전환에는 연결을 해제한다. GameplayTag와 장착 프로필·실행 정의를 사용하며 저장·구매·복제·추첨 경로와 콘텐츠 에셋은 변경하지 않았다.

새 수치는 상품의 `GrantedSkillBalances`, 아이템 등급은 복제된 `ItemRarities`를 사용한다. 기존 원본 설명의 기본 위력은 표시하지 않으며 저장값 누락을 현재 CSV·에셋 값으로 대체하지 않는다. 구버전만 기존 실행 정의의 수치를 사용한다. 패널 너비는 440 논리 단위이고 긴 내용은 최대 600 높이 안에서 전체 비율을 축소한다. 높이는 구성 시점 포인터 모니터의 작업 영역과 데스크톱 DPI를 반영하며 화면 이동 후 다음 갱신 전의 재계산은 구현하지 않았다. 기본 스킬 1개 외 사용자 지정 다수 스킬도 생략하지 않지만 긴 목록은 글자가 작아질 수 있다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 최종 5.58초·오류/경고 0으로 성공했다. 최초 컴파일에서 발견한 `UWidget::Cursor` 이름 가림은 지역변수 이름 변경으로 수정했다. 근거는 `Saved/Automation/ShopItemTooltip_20261009/EditorBuild.log`·`EditorBuild.Final.log`다. 독립 소스 검토·기존 표시 함수/TODO 보존·문서 링크·diff 정적 검사를 수행했으며 같은 폴더의 `StaticReview.json`·`DocumentationValidation.json`에 기록한다. 기존 미완료 43개·선택 7개를 보존하고 사용자 확인 1개를 추가했다. 게임·PIE·자동화 테스트·화면 캡처는 실행하지 않았고 Editor·IDE도 열지 않았다. 실제 hover·가독성·리롤/퇴장·2인 클라이언트 확인은 [TODO 29](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)에 남긴다.

### 9-44 2026-10-09 전체 UI 가독성과 조작 피드백 폴리싱

전체 게임 폴리싱 요청에 따라 메뉴·설정·전투 계획·지도·상점·인벤토리·보상 화면을 보완했다. 시작 기준은 `5a81b773`이며 기존 미커밋 변경은 없었다. 메뉴와 확인 패널은 공간이 부족할 때만 비율을 축소하고 한국어 문구·이어하기 불가 사유·Gameplay 단축키를 표시한다. 공통 목록은 스크롤바 두께·휠 이동·포커스 스크롤을 통일하며 기존 즉시 포커스 이동과 휠 입력 소비를 유지한다. 구성은 네이티브 UI이며 에셋·맵·CSV·저장 형식·GAS·서버 권위는 변경하지 않았다.

공통 아이템 툴팁을 장비·가방·보상에도 연결하고 화면별 상태와 장착 안내를 구분했다. 상점·보상 카드에는 스킬 이름/등급을 간결하게 표시한다. 인벤토리 스킬 탭·선택 상세는 장착 사본의 확정 수치와 효과를 조회하며 버전 1 저장값 누락을 원본 기본값으로 대체하지 않는다. 선택 상태·장착 미지원·빈 슬롯·사망 안내와 상세 스크롤을 보완했다. 전투 UI는 현재 지정 대상과 예약 명령, AP/SAP·이동 합계·무행동/이동만 준비·서버 대기를 구분하며 준비 버튼을 스크롤 밖 하단에 고정했다. 보상 UI는 지급 처리/수령 완료와 참가자 수령 진행을 표시하고 난이도 배율의 기준을 명시한다. 상세는 [UI 구조](UI_README.md#7-기본-라운드-전투-ui)·[인벤토리](UI_README.md#8-2-gameplay-인벤토리와-설정-단축키)에 기록한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 컴파일·링크는 37.24초, 고정 준비 버튼·툴팁 재사용·기존 포커스 정책 보완 후 7.48초에 성공했다. 최종 오류·경고는 0이며 근거는 `Saved/Automation/GamePolish_20261009/EditorBuild.log`·`EditorBuild.Final.log`다. 독립 소스 검토와 처리 함수·문서 링크·TODO 보존·diff 정적 검사를 수행했다. 같은 폴더의 `CommandPreservationReview.json`·`DocumentationValidation.json`·`StaticReview.json`에 근거를 기록한다. 기존 미완료 44개·선택 7개를 보존하고 사용자 확인 2개를 추가했다.

게임·PIE·Unreal 자동화 테스트·화면 캡처는 실행하지 않았으며 Editor·IDE도 열지 않았다. 실제 화면비별 가림·입력·보상 재시도·2인 표시·Continue는 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)·[29](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)에 남긴다. 기존 투사체 발사 FX는 원본 emitter의 공간 설정과 실제 장비 소켓 화면 확인이 필요하므로 이번 변경에 포함하지 않고 기존 TODO를 유지한다. 이전 실행 성공을 최신 UI의 작동 검증으로 대체하지 않는다.

### 9-45 2026-10-09 라이브러리 로딩 화면 플러그인

라이브러리 플러그인 기반 로딩 화면 요청을 반영했다. 시작 기준은 `3f3dfee3`이며 기존 미커밋 변경은 없었다. 로컬 Epic Launcher `WindowsEditor/GameUserSettings.ini`의 AsyncLoadingScreen 5.3 기록을 확인했고 현재 UE 5.8 설치본이 없어 제작자의 MIT 공개 소스 1.7.0·UE 5.8용 `77f3cdcfc32afa2422d1208e04b61df3bd9a52dc`를 프로젝트 플러그인으로 추가했다. 소스 줄 끝 공백·말미 빈 줄 정리 외 기능 변경은 없으며 데모 Content·영상은 제외했다. 출처·설치 범위·라이선스 배포는 [PROJECT_PLAN 1-1절](PROJECT_PLAN.md#1-1-로딩-화면-플러그인)에 기록한다.

Startup/Default MoviePlayer 화면에 기존 DemonicUI 성 배경·하단 밴드·청동색 원형 표시·한국어 팁 6개를 설정했다. 맵 전체 진행률을 가정하지 않고 자동 종료·최소 대기 없음·수동 종료/추가 엔진 tick/PSO 대기 비활성을 명시했다. 배경은 원본 경로를 참조하고 설정된 배경만 `ModifyCook`에 추가한다. MIT LICENSE는 게임 패키지 NonUFS 의존성으로 등록했다. 개별 travel·서버 권위·저장·GAS 코드는 변경하지 않았다. 플러그인의 표시용 팁 선택은 원본의 전역 RNG를 사용하며 저장된 Run 추첨 데이터의 변경과 구분한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`와 같은 옵션의 `ProjectA Win64 Development` 컴파일·링크가 각각 31.58초·32.36초에 성공했다. 라이선스 스테이징 추가 후 빌드는 5.72초·3.21초, 원본 말미 빈 줄 정리 후 최종 빌드는 4.13초·10.45초이며 오류·경고 0이다. 로그는 `Saved/Automation/LoadingScreen_20261009/EditorBuild.log`·`GameBuild.log`·`EditorBuild.Final.log`·`GameBuild.Final.log`·`EditorBuild.Whitespace.log`·`GameBuild.Whitespace.log`다. 플러그인 출처/소스 대조·INI 구조/필드·배경 경로·빌드 receipt·문서 링크·TODO 보존·전체 diff 정적 검사를 수행하며 근거는 같은 폴더의 `StaticReview.json`·`PluginReview.json`·`DocumentationValidation.json`에 기록한다. 기존 미완료 46개·선택 7개를 보존하고 사용자 확인 1개를 추가했다.

독립 소스 검토에서 일반 PIE 제외와 Gameplay 다음 tick 준비의 수동 종료 교착 가능성을 확인해 엔진의 자동 종료 경로를 유지했다. Editor·게임·PIE·자동화 테스트·cook·패키지·화면 캡처는 실행하지 않았으며 IDE도 열지 않았다. 초기 준비/한글/화면비·재진입·접속 실패·2인 전환·패키지 포함과 원본 배경의 최초 로드/메모리 확인은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다. 같은 월드의 인카운터 이동과 OpenLevel 이전 동기 작업은 [표시 범위](UI_README.md#14-로딩-화면)에서 구분한다.

### 9-46 2026-10-09 한국어영어 설정과 텍스트 현지화

설정의 번역 기능 구현 요청에 따라 한국어 기본·English 선택과 저장을 추가했다. 시작 기준은 `cec00299`이며 기존 미커밋 변경은 없었다. 언어는 비영상 적용 또는 화면 유지 확정 시 저장하며 적용 전 닫기·화면 복구·시간 초과는 기존 언어를 유지한다. 독립 게임은 Unreal 언어 초기화와 사용자 설정을 사용하고 PIE는 게임 리소스 미리보기만 변경한다. 변경 범위와 유지보수 명령은 [UI 15절](UI_README.md#15-언어와-번역-리소스), ICU·UFS 배포는 [PROJECT_PLAN 1-2절](PROJECT_PLAN.md#1-2-언어-리소스와-배포)에 기록한다.

메뉴·캐릭터 생성·전투·인벤토리·상점·보상·난이도의 고정 문자열을 FText로 전환하고 조합 인자를 보존했다. 6개 JSON에 1,046개 번역을 작성했으며 현재 NSLOCTEXT 754개를 모두 포함한다. 아이템 49종·스킬 77종·몬스터 13종·인카운터 28종·난이도 3종·노드 30개 및 직업/몸체를 실제 ID·경로와 원문으로 연결했다. 로딩 제목·팁 6개는 동일 리소스를 사용한다. 사용자 이름·저장·확률·GAS·명령/서버 권위는 유지한다. 미등록 콘텐츠·원문 불일치·일부 기존 개발 진단의 고정 문자열은 원문을 표시한다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64` 및 같은 옵션의 `ProjectA Win64 Development` 컴파일·링크가 각각 45.66초·33.93초에 성공했다. 인벤토리/구형 스킬상점 표시 연결 보완을 포함한 최종 증분 빌드도 11.43초·15.95초에 성공했고 오류·경고는 0이다. 근거는 `Saved/Automation/Localization_20261009/EditorBuild.Final.log`·`GameBuild.Final.log`다. 명령줄 컴파일이며 게임·PIE·Unreal 자동화 테스트·패키지 실행·화면 캡처는 수행하지 않았다.

`python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`는 전체 키·원문·서식 인자·한영 생성물 일치를 확인한다. `ResourceValidation.json`은 독립 바이너리 해석으로 양쪽 리소스 전체와 game receipt의 3개 UFS 의존성을 대조하고, 엔진 기본 영문 locres 66,406개 중 원문과 동일한 66,393개에서 source CRC 일치를 확인한다. `ValidateContentTranslations.json`·`CombatStaticReview.json`·`DocumentationValidation.json`은 콘텐츠 원문·명령 보존·문서 링크 검사의 근거다. 기존 TODO 미완료 47개·제안 선택 7개를 보존하고 언어 실행 확인 1개를 추가했다. 재실행 저장·즉시 갱신·영문 줄바꿈·두 독립 클라이언트의 혼합 언어 동작은 사용자 확인 전이며 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다.

### 9-47 2026-10-10 상점 동시 배치와 아이템 판매

상점에서 인벤토리·상품·NPC 동시 표시와 보유품 판매 요청을 반영했다. 시작 기준은 `422d22ad`이며 기존 `ProjectA.uproject`의 AsyncLoadingScreen MarketplaceURL 추가·Steam 배열 서식을 보존해 함께 포함했다. 새 에셋·맵 수정·원본 복제는 없다. 왼쪽 장비 300·가방 430, 중앙 NPC 공간 480, 오른쪽 상품 620 UI 단위와 간격 16으로 구성하고 상점의 I는 가방으로 포커스를 이동한다. 카메라는 기존 도착 위치를 유지하며 NPC 방향과 FOV를 조정한다. 구매·판매·리롤로 입장 연출을 반복하지 않는다. [화면 구조](UI_README.md#8-2-gameplay-인벤토리와-설정-단축키)

가방의 미장착 사본을 선택하여 판매가 확인 후 확정·취소한다. 기본·등급·태그 아이템 상점은 저장 가격의 절반 내림·최소 1G를 지급하며 현재 1G 품목은 1G다. 전문 상점의 매입은 진열 필터와 독립적이며 유효한 구형 장착 미지원 사본도 받는다. 장착품은 해제 후 판매하고 되사기는 제공하지 않는다. 서버는 신뢰 소유자·생존 Human·방문·사본·장비/상점 Revision을 검증하고 삭제·골드·후속 장비 인덱스·직전 보상 판매 표식을 원자 저장한다. 클라이언트는 요청 GUID와 두 Revision 도착을 확인하며 요청 전에 선택을 비워 다른 사본의 연속 판매를 막는다. [권한·가격·저장 계약](PROJECT_PLAN.md#3-2-상점-인카운터)

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 49.95초에 성공했고 구형 무장착 Revision 경계 보완 후 최종 증분 빌드도 6.98초에 성공했다. 같은 옵션의 `ProjectA Win64 Development` 컴파일·링크는 36.44초에 성공했다. 로그는 `Saved/Automation/ShopSales_20261010/EditorBuild.log`·`EditorBuild.Final.log`·`GameBuild.log`이며 오류·경고 0이다. `ProjectA.Run.ItemSale` 회귀 4개에 가격·태그/소유권·사본/Revision·인덱스 이동·저장 실패/재시도·Continue·직전 보상·구형 호환 검사를 작성하고 컴파일했으며 실행하지 않았다.

`python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`와 `ValidateResources.py`는 23개 추가 문구를 포함한 기존 6개 JSON의 1,069개 한영 번역·서식 인자·생성물·바이너리 재해석·UFS 의존성을 대조했다. `LayoutStaticReview.json`은 5개 viewport의 배치와 NPC 계획 경계에 대한 정적 계산 45개, `BackendStaticReview.json`은 권한/저장/직전 보상 경로의 소스 검토 근거다. `DocumentationValidation.json`·`StaticReview.json`에 링크·기존 TODO 미완료 48개/선택 7개 보존과 확인 2개 추가·프로젝트 설정·전체 diff 검사를 기록한다.

게임·PIE·Unreal 자동화 테스트·패키지·화면 캡처와 IDE 실행은 수행하지 않았다. 원근 계산은 실제 의상·idle·소품 가림의 렌더 확인을 대신하지 않는다. 세 화면비·한영 판매 UI, 저장 실패/Continue, 협동 지연·동시 거래는 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)의 사용자 확인으로 남긴다. 이전 실행 이력으로 이번 변경의 작동 성공을 표시하지 않는다.

### 9-48 2026-10-10 오른손 참격 VFX 회전 방향

근접 VFX와 오른손 무기 애니메이션의 시작·진행 방향 비교 요청을 반영했다. 시작 기준은 `48461306`이며 기존 미커밋 변경은 없었다. 현재 참격 4종의 원본 패키지 SHA256은 기존 감사와 일치한다. Niagara 패키지의 컴파일된 HLSL·Curve LUT를 읽어 도끼·곡검·낫의 양수 `User.RotateSpeed`가 +Z 회전에 사용됨을 확인했다. 2026-10-09 Greystone `Attack_A_Med` 포즈 기록과 동일한 원본 FBX 해시를 대조했으며 오른손은 로컬 +Y→+X→-Y로 이동한다. 이 과거 포즈는 애니메이션 궤적의 근거이며 이번 수정 후 화면 검수의 근거로 사용하지 않는다.

원본을 직접 참조하는 [CombatMeleeVfxCatalog](../Source/ProjectA/DataAsset/CombatMeleeVfxCatalog.cpp)의 태그 Query·원본 경로·실제 유닛 몽타주 조건으로 도끼 `-1`·곡검 `-1.5`·낫 `-2`의 회전 속도를 적용했다. 서버는 발동 시 `Visual` 사본만 수정하고 기존 NetSerialize로 전달한다. 기존 사용자 회전·동일 키 재정의·모호한 규칙은 보존한다. 원본 에셋·스킬 정의·저장 형식·피해·충돌·피격 VFX·소리·발동 시점·초기 회전 오프셋은 변경하지 않았다. README의 상점 설명에 남아 있던 이전 인벤토리 열기 문구도 9-47의 동시 배치 구현과 일치시켰다.

발도 참격은 회전형 업데이트 대신 `VectorToRadialValue`와 감소하는 Erode 곡선으로 재질을 펼친다. 원본 재질 그래프·엔진 함수·스프라이트 축을 대조한 펼침 경계의 -Z 진행은 오른손 궤적과 같은 회전 부호이므로 보정에서 제외했다. 4종의 기존 초기·중간 이미지에는 점·완성된 링·겹친 피격 효과가 있어 실제 첫 발광 날의 시작각을 확정할 수 없다. 초기 UV 위상·텍스처 마스크·반투명 가림까지 맞는다는 결론은 내리지 않았다. 재현 명령은 `python Saved/Automation/MeleeVfx_20261010/ReadDirectionAudit.py`, 근거는 같은 폴더의 `DirectionAudit.json`이다. 파일·패키지 읽기만 수행하며 Unreal을 실행하지 않는다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`와 같은 옵션의 `ProjectA Win64 Development` 컴파일·링크가 각각 27.25초·26.86초에 성공했다. 로그는 `Saved/Automation/MeleeVfx_20261010/EditorBuild.log`·`GameBuild.log`다. `ProjectA.Combat.MeleeVfx`의 원본 적용 범위·유닛 몽타주/사본 격리·사용자 설정 보존·기존 시각 데이터 직렬화 회귀 4개를 작성하고 컴파일했으며 실행하지 않았다. 독립 소스 검토에서 서버 권위·태그 조건·판정/저장 경로 보존과 추가 동기 로드가 없음을 확인했다.

문서 링크·TODO 보존·전체 diff 정적 근거는 `Saved/Automation/MeleeVfx_20261010/DocumentationValidation.json`·`StaticReview.json`에 기록한다. 기존 미완료 50개·제안 선택 7개를 보존하고 참격 화면 확인 1개를 추가했다. 게임·PIE·Unreal 자동화 테스트·패키지·화면 캡처와 IDE 실행은 수행하지 않았다. 남녀·반대 방향의 적·Continue·2인 원격에서 최초 발광과 베기 궤적이 일치하는지는 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)의 사용자 확인 전이다.

### 9-49 2026-10-10 PvE 난이도별 전투장과 몬스터 안내

PvE 하·중·상에 서로 다른 맵과 등장 몬스터 설명을 제공하도록 변경했다. 시작 기준은 `16f5b13f`이며 기존 미커밋 변경은 없었다. 새 기본 Run의 표시 버전 1에 하 `MeadowBloom`·중 `DungeonStone`·상 `IceCitadel`을 태그 Query로 연결하여 고정한다. 선택 카드와 전투 시작·체크포인트 복구가 동일한 저장 ID를 해석하며 잘못된 ID·중복·미지원 버전을 거절한다. 기존 표시 버전 0은 원래 배경을 유지하고 추가 추첨·시드 소비·CSV 수치 변경은 없다. [저장·편성 계약](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)

통합 Gameplay에 이미 배치된 세 지역의 메시·재질을 직접 참조해 비충돌 임시 장식을 중앙 전투 위치에 표시한다. 일반 메시·ISM 종류와 주요 렌더 설정을 유지하며 전투 구역/카메라 시선을 가리는 장식을 제외한다. 모든 후보 컴포넌트 작성 후에만 배경을 교체하고 실패·퇴장·종료에는 원래 표시 상태를 복원한다. 서버가 확정한 ID의 RepNotify와 BeginPlay가 원격 표시를 연결한다. 실제 그리드·물리 바닥·Nav·카메라·유닛/체크포인트 좌표와 맵/원본 에셋은 변경하지 않았다. 기존 지면 기록과 명세 대조는 `Saved/Automation/PveArenaChoices_20261010/EnvironmentSourceReview.json`이며 과거 기록을 이번 렌더 검수로 취급하지 않는다.

카드는 난이도·전투장 이름/소개·몬스터별 역할/HP/속도/이동거리·합계/보상을 분리한다. 실제 ScaleGroup 결과를 View의 EnemyRoster로 전달하고 UI에서 에셋을 로드하거나 적을 다시 선정하지 않는다. 몬스터 이름은 기존 번역을, 역할은 기존 GameplayTag를 사용한다. 카드 전체 클릭·Host 권한·좌/직/우 순서를 유지하고 패널 높이 상한 520과 양축 스크롤을 적용했다. 한영 리소스에 22키를 추가하여 전체 1,091개의 원문·인자·생성물을 대조했다. [UI 구조](UI_README.md#2-실행과-옵션)

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 43.20초에 성공했다. 지면 허용 오차·렌더 설정 보존·환경 회귀 추가를 포함한 최종 증분 빌드도 6.37초에 성공했다. 같은 옵션의 `ProjectA Win64 Development` 컴파일·링크는 31.26초에 성공했으며 오류·경고는 0이다. 로그는 `Saved/Automation/PveArenaChoices_20261010/EditorBuild.log`·`EditorBuild.Final.log`·`GameBuild.log`다. 새 회귀 3개와 기존 선택 취소/저장 실패/재선택/Continue 회귀 보완은 작성·컴파일만 수행했다. `ProjectA.Run.PveDifficulty` 및 `ProjectA.Run.Target.PveDifficulty.ChoiceAbortAndFrozenSave`의 실제 실행은 별도다.

`python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`와 같은 검수 폴더의 `ValidateResources.py`로 한영 바이너리 재해석·원문 CRC·UFS 스테이징 참조를 확인했다. `DocumentationValidation.json`·`StaticReview.json`에 문서 링크·기존 미완료 51개/선택 7개 보존과 확인 1개 추가·전체 diff 정적 근거를 기록한다. 게임·PIE·Unreal 자동화 테스트·패키지·화면 캡처 및 IDE는 실행하지 않았다. 세 배경의 실제 가림/접지·한영 카드 가독성·Continue·늦은 원격 접속·기존 저장 호환은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)의 사용자 확인 전이다. Target의 기존 싱글 경로에 복제 표현을 연결한 것이며 일반 협동 지원 완료를 뜻하지 않는다.

### 9-50 2026-10-10 준비 완료와 행동 종료의 숄더 카메라

전투의 준비 완료 후 직접 조작하는 한 캐릭터를 가까이 따라가도록 구현했다. 시작 기준은 `82c82d92`이며 기존 미커밋 변경은 없었다. 로컬 컨트롤러가 서버 View의 준비·소유자·행동 단계를 확인하여 선택 캐릭터의 오른쪽 숄더뷰로 0.3초에 전환한다. SAP 이동·접근·시전·시전 후 동작·복귀까지 유지하고 해당 행동 종료·취소·사망·중단에는 전술 시점으로 복귀한다. 준비 취소 후 재선택과 전투/라운드/소유 액터 변경을 구분하며 다른 인간·AI 행동으로 초점을 자동 이전하지 않는다. 이미 끝난 무행동을 촬영하기 위해 실행을 지연하지 않는다. [UI 계약](UI_README.md#10-2-전투-배치와-카메라)

로컬 Transient 카메라는 기본 거리 250cm·오른쪽 65cm·FOV 65와 캡슐 기반 높이를 사용한다. 회전·위치 보간과 복귀 중 시선 방향 유지로 급회전을 줄이고, 매 화면 갱신의 Camera 채널 구체 검사와 시작 시 수집한 장식별 경계로 가림 거리를 제한한다. 비충돌 장식의 경계는 보수적인 상자이며 복잡한 나뭇가지·아치의 정확한 삼각형 가림을 보장하지 않는다. 기존 채도·PostProcess를 유지하고 상점 등 다른 연출이 넘겨받은 시점을 복귀로 덮어쓰지 않는다. 클라이언트의 반복 Gameplay View 갱신도 활성 숄더뷰를 초기화하지 않는다. 원본 에셋·맵·저장·RPC·소유권·서버 전투 판정은 변경하지 않았다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 41.09초, 기하 회귀·GC 참조·시점 식별자 보완을 포함한 최종 빌드는 40.17초에 성공했다. 같은 옵션의 `ProjectA Win64 Development`는 32.73초에 성공했으며 컴파일 오류·경고는 0이다. `ProjectA.Combat.ShoulderCamera`의 소유권/준비/행동/경계 6개와 회전/장식 가림 2개 회귀를 작성·컴파일했으며 실행하지 않았다. 로그는 `Saved/Automation/ShoulderCamera_20261010/{EditorBuild,EditorBuild.Final,GameBuild}.log`다.

독립 코드 검토와 `python Saved/Automation/ShoulderCamera_20261010/ValidateDocumentation.py`·`ValidateStatic.py`로 문서 링크·기존 미완료 52개/제안 선택 7개 보존·확인 1개 추가·전체 diff를 점검했다. 근거는 같은 폴더의 `DocumentationValidation.json`·`StaticReview.json`이다. 게임·PIE·Unreal 자동화 테스트·패키지·화면 캡처·IDE를 실행하지 않았다. 화면비별 근접/활/마법 구도·가림·준비 취소/복귀·Continue·2인 각자 시점의 수용 확인은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 남긴다.

### 9-51 2026-10-10 맵별 스크린샷 수집

사용자의 명시적인 촬영 요청에 따라 현재 프로젝트 맵 17개의 화면 25장을 [스크린샷 갤러리](ScreanShoot/index.html)에 보존했다. 기준 커밋은 `242a12cb`이며 시작 시 미커밋 변경은 없었다. 환경 비교 14맵·DebugCombat 각 1장, MainMenu 1장, 통합 Gameplay의 실제 PvE 하·중·상 각각 전술뷰·준비 후 숄더뷰·행동 후 화면 3장이다. 외부 팩의 데모맵은 대상이 아니다. 모두 1280×720이며 [manifest.json](ScreanShoot/2026-10-10/manifest.json)에 맵 경로·촬영 단계·실제 카메라·원본 위치·SHA-256을 기록했다. PNG를 자르거나 보정하지 않았다.

환경 촬영은 기존 아군/적 각 1명·HP 10000의 비교용 구성을 사용한다. 실제 PvE는 정상 새 게임에서 기본 캐릭터 4명과 직접 조작 1명을 생성하고 공개 인카운터·난이도·스킬·준비 요청으로 진행한다. 메뉴 버튼 delegate 자동화이며 물리 마우스 입력 검수가 아니다. 맵·게임 수치·카메라·행동 시간·시뮬레이션을 촬영용으로 변경하지 않았다. `TodoRenderReviewTests`에 16:9 촬영 후 종료하는 선택 옵션을 추가하고 `PveMapScreenshotTests`에 실제 진행의 촬영과 상태 기록을 구현했다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`의 최초·PvE 추가·최종 빌드는 각각 20.40초·8.57초·7.49초에 성공했다. 로그는 `Saved/Automation/MapScreenshots_20261010/{EditorBuild,EditorBuild.Pve,EditorBuild.Final}.log`다. 실행은 같은 폴더의 `RunCapture.ps1 -Group Environment`와 `-Group Pve`이며, `UnrealEditor-Cmd`의 `-ProjectAMapScreenshotReview -ProjectAReviewLeftMonitor -UserDir=<고유 경로> -ProjectASaveSlot=<고유 슬롯> -ProjectAReviewOutputRoot=<분리 Saved/Automation 경로> -nowrite -ExecCmds="Automation RunTests <필터>" -TestExit="Automation Test Queue Empty"`를 사용한다. 필터는 `ProjectA.TodoReview.Environment`와 `ProjectA.TodoReview.PveMapScreenshots`다.

최종 촬영 보고서는 `Saved/Automation/MapScreenshots_20261010/Environment.e3f8c8a8a245462ca6bff4513b250cf4/Report/index.json`의 15건, `Pve.d1ac6d5450c84a5b9244bd6467d14509/Report/index.json`의 3건 모두 경고 포함 통과이며 실패/미실행은 0이다. 최초 `Pve.51fe8bfd252340ebbfb9cb92a203c090`는 승리 후 Round 정리를 관찰하지 못해 3건 시간 초과했다. 촬영 관찰기를 보완해 결과 상태도 기록하도록 수정한 뒤 재촬영했으며 최초 실패를 게임의 복귀 실패로 해석하지 않는다. 최종 세 사례의 행동 후 실제 카메라 POV는 원래 전술 카메라와 일치했고 이미지는 승리·보상 UI다. 일반적인 전투 중 행동 종료의 체감 확인을 대신하지 않는다.

화면에서 실제 중 난이도의 회색 체크무늬 바닥과 세 숄더뷰의 캐릭터 미노출·원점 POV를 관찰했다. 정적 대조에서 UE 5.8 `PlayerCameraManager.cpp`의 `ACameraActor` 전용 경로가 `GetCameraComponent()->GetCameraView()`를 사용하고 프로젝트의 `CalcCamera()` 추적 계산을 우회함을 확인했다. 호출 횟수를 실행 계측하지는 않았다. 이번 작업은 촬영본을 보존하며 해당 문제의 수정·재검증과 최종 시각 판단을 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)에 유지한다.

실제 PIE 창은 왼쪽 모니터의 `(-1904, 8)`에 배치했고 촬영용 Editor는 종료했다. `ProtectUserState.py --verify`로 기존 저장·사용자/프로젝트 설정·17개 맵 총 69개 파일의 해시와 파일 목록이 그대로임을 확인했다. `ValidateArchive.py`는 25개 PNG 디코딩·해상도·원본 해시·갤러리 링크·17맵 수록과 기존 TODO 미완료 53개/제안 선택 7개 보존을 확인한다. `ValidateDocumentation.py`의 문서 링크 및 전체 diff 정적 결과는 같은 폴더의 `ArchiveValidation.json`·`DocumentationValidation.json`에 기록한다. 카메라/맵 품질 승인, 화면비별 입력, 밸런스, Continue, 멀티플레이 검수 완료로 확대하지 않는다.

### 9-52 2026-10-10 PvE 시작·최종 체력과 성장 곡선

사용자 지정 첫 몬스터 HP 하·중·상 50·100·150과 최종 골렘 1000·1500·3000을 새 기본 Run에 적용했다. 시작 기준은 `cfab4555`이며 기존 미커밋 변경은 없었다. [PVE_HEALTH_CURVE.csv](../DataCatalogs/PVE_HEALTH_CURVE.csv)의 10묶음×3난이도에 초반 완만·후반 증가폭 확장 곡선을 작성하고 기존 7묶음의 적 수 감소를 유지했다. 전체 수치는 [PROJECT_PLAN 5-1](PROJECT_PLAN.md#5-1-목표-run과-회복-시험-데이터)에 통합한다. 기준 HP는 총합이 아닌 편성 중 최고 체력 몬스터이며 다른 적은 저장된 기초 HP 비율을 유지해 올림한다. 첫 적 1명과 최종 골렘의 기준은 파티 1~4인에서 동일하며 인원 보정은 적 수에 유지한다. 태그·가중치 편성·시드·Snapshot·공격 피해·AP/SAP·성장·골드·아이템 보상은 변경하지 않았다.

`PveDifficulty.HealthCurveVersion=1`과 난이도별 10개 `ReferenceHPByGroup`을 기존 SaveGame에 고정한다. 선택 카드·실제 스폰·체크포인트 검증은 공통 해석을 사용하며 카드 HP 배율도 실제 비율로 전달한다. 필드가 없는 기존 저장은 버전 0의 이전 곱셈·올림과 편성을 유지한다. 두 CSV의 행·태그·정수 범위·묶음별 증가·난이도 순서를 원자적으로 검사하고 실패 시 기존 상태를 보존한다. `ProjectA.Build.cs`에 새 CSV를 UFS로 등록했으며 신규 오류 문구 3개의 한영 번역·locres를 갱신했다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 41.98초, 같은 옵션의 `ProjectA Win64 Development`는 31.02초에 성공했다. 로그는 `Saved/Automation/PveHealthCurve_20261010/{EditorBuild,GameBuild}.log`다. `ProjectA.Run.PveDifficulty.HealthCurve`의 CSV 오류·원자성, 16시드×4인원×10묶음×3난이도, 저장 복원·버전 0 호환·편성 순서/동률 회귀 3개를 추가하고 기존 Target 거래 회귀를 갱신하여 컴파일했다. Unreal 자동화 테스트는 실행하지 않았다.

`python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`는 번역 1,094개와 한영 리소스 일치를 확인했다. 같은 검수 폴더의 `ValidateData.py`는 가능 편성 3,368개×3난이도 10,104건의 체력 산술과 CSV·UFS 참조를 검사했다. `ValidateDocumentation.py`·`ValidateStatic.py`는 문서 링크 911개·기존 TODO 미완료 53개/제안 선택 7개 보존과 전체 diff를 확인했다. 결과는 `StaticDataValidation.json`·`DocumentationValidation.json`·`StaticReview.json`에 기록한다. 이 검사는 게임 실행 결과가 아니다. 게임·PIE·패키지·화면 캡처·IDE는 실행하지 않았으며 첫/최종 HP의 실제 표시·Continue·후반 상 난이도의 처치 시간과 생존률은 [TODO 26](TODO.md#26-재개-후-로컬-검수와-저장-보완)의 사용자 확인 전이다.

### 9-53 2026-10-10 수호 스킬의 방패 전용 부여

사용자 요청에 따라 새 일반 Run의 수호자의 결계·낙하/대지/마력/연막/차원/상승 수호 7종을 방패 전용으로 변경했다. 시작 기준은 `535ccc1d`이며 기존 미커밋 변경은 없었다. `RunWeaponSkillRulesDataAsset.ItemQueryOverrides`가 실제 GAS `Skill.Effect.Shield`에 일치하는 후보의 허용 Query를 `Item.Weapon.Shield`로 고정한다. 첫 일치 규칙을 적용하고 실제 적용한 허용 Query를 기존 무기 Query와 OR 결합하여 과거 제작 DataAsset의 방패 제외 설정에도 새 Run에서 적용한다. 이름·클래스 분기를 사용하지 않으며 제작 에셋·GAS 효과·시각 에셋은 수정하지 않았다. 시간의 회복진을 포함한 치유 6종은 마법 무기에 유지한다. [태그 조건](GAME_DESIGN.md#2-4-1-태그-조건과-후보-선별)

시작 방패·상점·리롤·아이템 보상은 기존 공통 생성·등급 추첨을 사용한다. 후보 62종은 근접 5·활 2·석궁 1·마법 47·방패 7이며 수호는 흰색 1·초록색 3·파란색 2·보라색 1종이다. 주황 수호 후보 부재는 기존 가중치 재정규화로 처리한다. `SKILL_BALANCE.csv`의 수호 7행 비고에 방패 전용 조건을 기록하고 수치·등급·확률은 유지했다. Run의 기존 저장 Query와 확정 사본을 그대로 사용하므로 이전 지팡이 수호·무스킬 방패·장착 스킬을 소급 교체하거나 Continue에서 재추첨하지 않는다.

`Build.bat ProjectAEditor Win64 Development -Project=C:/Users/jaba0/Desktop/MyProjects/ProjectA/ProjectA.uproject -WaitMutex -FromMsBuild -architecture=x64`는 19.15초, 같은 옵션의 `ProjectA Win64 Development`는 23.20초에 성공했다. 로그는 `Saved/Automation/GuardianShield_20261010/{EditorBuild,GameBuild}.log`다. 기존 `ProjectA.Run.WeaponSkills.DevelopmentRuleData`를 갱신하고 `GuardianShieldEligibilityAndFrozenQueries`를 추가하여 컴파일했다. 19종 아이템 조건·5등급×3시드 생성·태그 보존·직렬화된 제작 Query·실제 연결된 DataAsset·신구 SaveGame Query·오류 시 출력 보존을 검사하는 코드이며 Unreal 자동화 테스트는 실행하지 않았다.

같은 검수 폴더의 `ValidateData.py`가 명세·CSV·생성 소스의 331개 정적 검사를 통과했다. 후보 62×태그 19의 1,178조합과 태그 19×등급 5의 95조합, 판매 49개/방패 15개, 수치·원본 명세 보존을 확인하고 `DataValidation.json`에 기록했다. `python Source/ProjectAEditor/Scripts/BuildLocalization.py --check`의 번역 1,094개 검사와 `ValidateDocumentation.py`·`ValidateStatic.py`의 링크·기존 TODO 미완료 53개/선택 7개 보존·확인 1개 추가·전체 diff 검사를 수행했다. 결과는 `DocumentationValidation.json`·`StaticReview.json`에 보존한다. 게임·PIE·패키지·IDE를 실행하지 않았으며 실제 장착/해제·보호막 발동·상점/보상·Continue는 [TODO 29](TODO.md#29-무기-랜덤-스킬과-아이템-등급-기획)의 사용자 확인 전이다.
