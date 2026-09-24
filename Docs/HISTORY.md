# ProjectA 완료 작업과 검증 기록

갱신일: 2026-09-25. 완료 범위·기준 커밋·실행 근거를 기록한다. 현재 상태와 간단 확인은 [TODO](TODO.md)를 따른다. 삭제 전 상세 보고서는 Git 이력에 보존한다. 과거 검증은 이후 변경의 통과 근거로 사용하지 않는다.

## 최근 변경

| 기준 | 변경·검증 |
|---|---|
| 2026-09-25 Primitive 남녀 몸체 선택 | 네 직업 생성/수정에서 `BodyVariants` 배열을 화살표로 순환하고 남자 `SKM_Primitive_Charater_01_Body`·여자 `SKM_Primitive_02_Body` 원본 참조. `BodyId`를 기존 appearance 구조의 Run·체크포인트·Snapshot·복제로 전달하며 이전 누락 값은 기본 남자로 해석. ROG 의상 UI·착용 중지, 8부위·103개 항목과 원본은 향후 아이템용 보존. 원본 Skeleton의 호환·리타깃·DefaultSlot 설정만 갱신하고 메시·물리 4개 해시 불변, 모델·텍스처 복제 없음. Development Editor / Win64 컴파일·독립 재로드 통과, 오류/경고 0. 두 몸체 포즈 138개·스태프 부착 6개·물리 연결과 기존 의상 ID·BodyId 값 직렬화 왕복 확인. 실제 UI·게임·PIE 미실행. 근거: `Saved/Automation/PrimitiveBodyBuild.txt`, `PrimitiveAppearanceReload.json`, `PrimitiveSourcePreservation.json`, `Saved/Logs/PrimitiveAppearanceReload.log`; [사용자 확인](TODO.md#2-30-rog-의상-커스터마이징) |
| 2026-09-25 의상 착용 시 Manny 텍스처 보존 | ROG 신체 파츠에 원본 Manny 두 MI·텍스처를 직접 연결하고 Head/Arms/Legs만 원본 경로에서 두 재질 슬롯으로 복원. 정점 위치·UV·노멀·가중치·뼈대·물리와 기존 ROG 기본 재질 보존, 모델·텍스처 복제 없음. `UCharacterAppearanceAssetLibrary`와 `-RogAppearanceMaterialsOnly`로 기존 직업·Blueprint·스태프 설정을 유지하며 미사용 중립 MI 제거. Development Editor / Win64 빌드 4.35초 성공·별도 재로드 오류/경고 0. 6개 파츠의 93,607개 삼각형 재질 영역, 미수정 3개 파츠 해시와 의상 목록 불변 확인. 작성 시 삭제 참조 수집 전환 경고 1개 후 정상 삭제·독립 재로드 통과. 실제 UI·게임 확인 전. 근거: `Saved/Automation/TexturedMannyBodyBuild.txt`, `RogAppearanceReload.json`, `Saved/Logs/TexturedMannyBodyReload.log`; [사용자 확인](TODO.md#2-30-rog-의상-커스터마이징) |
| 2026-09-24 네 직업 의상과 TopDown 기본 외형 | ROG 8부위·103개 의상 UI를 네 직업에 공통 연결하고 무의상·액세서리 상태는 TopDown 원본 Manny 몸체·재질 유지. 신체를 가리는 의상은 원본 ROG 파츠와 부위별 색 구분 없는 프로젝트 전용 중립 MI 한 개 공유. 원본 메시·텍스처 복제 없음. 마법사 스태프의 `hand_l` 부착, 기존 궁수 Unit/프리뷰 경로·이전 Snapshot 호환 보존. Development Editor / Win64 컴파일·별도 재로드의 원본 메시 120개·본 161개·검 표본 328개·스태프 부착 3개·물리 연결 22개·재질 저장 검사 통과. 신규 MI는 4,005바이트. UI·게임·자동화 실행 없음. 근거: `Saved/Automation/AllProfessionsAppearanceBuild.txt`, `RogAppearanceReload.json`, `Saved/Logs/AllProfessionsAppearanceReload.log`; [검증 상태와 사용자 확인](TODO.md#2-30-rog-의상-커스터마이징) |
| 2026-09-24 전사 공통 외형과 ROG 의상 UI | 전사를 Manny 공통 몸체·애니메이션으로 전환하고 원본 ROG 기반 8부위·103개 의상 선택·회전 프리뷰·저장/취소 UI 연결. Run·Snapshot·체크포인트·복제에 외형 ID를 전달하며 이전 빈 선택과 Snapshot 클래스 호환 보존. 원본 복제·추가 리타깃 없음, 기존 래그돌 실행 유지. Development Editor / Win64 빌드·독립 재로드의 메시 120개·본 161개·검 표본 82개·물리 연결 22개 검사 통과. UI·게임·자동화 실행 없음. 근거: `Saved/Automation/RogAppearanceBuild.txt`, `RogAppearanceReload.json`; [사용자 확인](TODO.md#2-30-rog-의상-커스터마이징) |
| 2026-09-23 마녀·Assassin 외형과 배율 정리 | 마법사 Stylized Dark Witch·도적 Assassin Skin1의 원본 경로를 메뉴·전투·Snapshot에 연결하고 기존 스태프 한 개를 왼손에 부착. 마녀 루트 배율 100을 제거하고 형상·바인드 자세·변형 본 계층·물리를 같은 임포트 경로에서 정리해 높이 187.12cm·본 배율 1·연결된 물리 바디 48개로 저장. 편의 복제 없이 리타깃만 생성, 전사·궁수·래그돌 실행 유지. Development Editor / Win64 컴파일·별도 재로드·시퀀스 48개 포즈/배율·검 표본 164개·물리 연결 검사 통과. 기존 MainMenu 변경 포함, 게임/PIE/자동화 테스트 미실행. 근거: `Saved/Automation/WitchAssassinBuild.txt`, `WitchAssassinReload.json`; [사용자 확인](TODO.md#2-29-마녀와-assassin-외형) |
| 2026-09-23 선택 화면 전사 외형 보완 | MainMenu의 Warrior만 `BP_WarriorMenuPreview`로 연결하고 GKnight 원본 메시·기존 `MM_Idle_Warrior` 반복 재생 적용. 전투·다른 직업 프리뷰 불변 해시, Blueprint 컴파일·별도 재로드 통과. 작업 중 별도로 삭제된 지팡이 임포트 에셋 5개를 보존하여 함께 반영하며 외부 참조 없음 확인. 에셋 복제·C++ 변경·게임 실행 없음. 근거: `Saved/Automation/WarriorMenuReload.json`, `RemovedStaffReferences.json`; [화면 확인](TODO.md#2-25-파라곤-적용-롤백) |
| 2026-09-23 파라곤 외형 롤백·전체 래그돌 복구 | 전사 GKnight·나머지 Manny·적/Snapshot Skeleton_Guard·공통 메뉴 프리뷰 복원. 기존 저장의 직업별 Blueprint 경로는 이전 모델로 연결하고 스킬·후속 기능·원본 참조 통합을 유지했다. 전 진영에 기존 래그돌·복제 충격량 복구, 사망 애니메이션 분기·작성 도구 제거. 에셋 일괄 삭제는 자동 승인 검토에서 범위 불명확으로 거절되어 미사용 생성 결과·팩·사용자 수정 에셋을 보존했다. Development Editor / Win64 빌드 22.48초·11개 전투/4개 프리뷰 별도 재로드·검 궤적/기존 리타깃·저장 파일 26개 불변 검사 통과. 복구 저장본 재로드 오류/경고 0, 별도 구경로 호환 검사는 해석 성공 전의 기존 경고 6개·오류 0. 게임·PIE 미실행. 근거: `Saved/Automation/PreParagonRollbackBuild.txt`, `PreParagonRollbackReload.json`, `WarriorContentReload.json`; [외형 확인](TODO.md#2-25-파라곤-적용-롤백)·[래그돌 확인](TODO.md#2-28-전체-래그돌-복구) |
| 2026-09-23 마법사 지팡이 손 소켓 보정 | 메뉴·전투·Snapshot Blueprint 3개의 `Staff`를 원본 `hand_lSocket`에 연결. 손목 쪽 배치와 손가락 길이 방향 축을 손바닥 안쪽의 가로 축으로 보정하고 메시 피벗과 실제 손잡이 중심의 차이를 반영했다. 생성 명세 갱신, 원본 복제·수정 없음. Blueprint 컴파일·별도 재로드 및 원본 메시/뼈대/지팡이 해시 3개 검사 통과, 재로드 오류 0·기존 Rig 경고 1. PIE/게임 미실행. 근거: `Saved/Automation/MageGrip/Reload.json`; [사용자 확인](TODO.md#2-25-파라곤-적용-롤백) |
| 2026-09-23 메뉴의 불필요한 지팡이 숨김 | MainMenu에 독립 배치된 `SM_Staff_02·03·04`의 게임 내 표시·충돌 비활성화. 배치·원본 에셋과 마법사 손의 `SM_Staff_01` 유지, 복제·삭제 없음. 별도 저장본 재로드에서 대상 3개 숨김·충돌 해제와 마법사 지팡이 표시 확인, PIE/게임 미실행. 근거: `Saved/Automation/MenuStaffVisibilityReload.json`; [사용자 확인](TODO.md#2-25-파라곤-적용-롤백) |
| 2026-09-23 적 래그돌 복원 | 사용자 요청에 따라 사망 물리 코드의 주석을 해제하고 `ETeam::Enemy`에 기존 래그돌·서버 충격량 생성/복제/적용 복원. 아군의 단발 사망 애니메이션 유지, Snapshot 상대도 적 물리 분기 사용. 에셋 변경 없음. Development Editor / Win64 빌드 17.43초·정적 검사 통과, PIE/게임 미실행. 근거: `Saved/Automation/EnemyRagdollBuild.txt`; [사용자 확인](TODO.md#2-28-전체-래그돌-복구) |
| 2026-09-23 사망 애니메이션 전환 | 기존 래그돌·충격량 생성/적용 코드 주석 보존, 단발 사망 시퀀스·종단 자세 유지. 파라곤 원본 4종과 기본 Manny 직접 참조, 해골 적 리타깃 1개(265,642바이트), 전투 Blueprint 11개 연결. Development Editor / Win64 빌드 6.69초·별도 저장본 재로드/포즈 검사 통과, 재로드 오류 0·기존 Rig 경고 4. 작성용 에디터 시작 시 자동 Smoke 검사 조건 오류 4개와 종료 코드 0xC0000005가 발생했으나 저장 완료 후 별도 commandlet은 종료 코드 0으로 통과. PIE/게임 미실행. 근거: `Saved/Automation/DeathAnimationBuild.txt`, `DeathAnimationsReload.json`, `Saved/Logs/DeathAnimationsReload.log`; [사용자 확인](TODO.md#2-28-전체-래그돌-복구) |
| 2026-09-22 공통 검증과 실행 책임 분리 | 능력치/장착·계획 검증 공통화, AI·충돌·GAS 효과 실행 분리와 실제 태그 조건 연결, 즉시 효과 적용 거절·미지원 지속 효과 검증 보완. Run 경로/저장 버전·후보 저장 후 반영, 4슬롯 초안·UI 갱신·구형 호환 코드·에디터 리타깃 공통 함수 분리. 테스트 전용 효과를 Editor 모듈로 분리하고 에셋 정리 결과를 보존하여 통합. 원래 프로젝트의 파일 재생성 10.30초·UHT 포함 Development Editor / Win64 빌드 32.64초 성공, 오류/경고 0. 독립 코드 검토·Python 구문 17개·문서 내부 링크 302개·diff 검사 통과. 추가 회귀 코드는 컴파일만 수행하며 게임·PIE·자동화·에셋 재작성 미실행. 근거: `Saved/Automation/RefactorIntegratedBuild.txt`, `RefactorIntegratedStatic.json`; [사용자 확인](TODO.md#2-27-공통-검증과-실행-책임-분리) |
| 2026-09-22 User_JeHoon 복사본 정리 | 원본 직접 참조·외부 팩 간 비교 금지 규칙 반영. Manny/GKnight/Skeleton_Guard 사본 6개를 Unreal로 통합하고 애니메이션 참조·원본 두 뼈대의 DefaultSlot·이전 경로 호환 보존. 검 소켓 수정본과 필수 파생 결과 유지. Content 순감소 34.01MiB. Development Editor / Win64 최종 빌드 2.94초, 별도 재로드 5,385개·공용 3종/직업 8종의 검 포즈 샘플·원본 참조 확인. 오류 0·옛 이름 조회 후 해석 경고 4·원본 Rig 경고 4. PIE/게임 미실행. 근거: `Saved/Automation/CopiedAssetsBuild.txt`, `CopiedAssetsDiskResult.json`, `CopiedAssetsReload.json`, `Saved/Logs/CopiedAssetsReloadFinal.log`; [사용자 확인](TODO.md#2-26-user_jehoon-복사본-정리) |
| 2026-09-22 직업별 파라곤 외형 | 전사 Kwang·궁수 Sparrow·마법사 Gideon·도적 Countess 원본 참조, 메뉴/전투/Snapshot 연결과 `SM_Staff_01` 왼손 부착. 필요한 전투 동작만 리타깃하고 기존 장착·검 판정 유지. Development Editor / Win64 빌드 19.96초 성공, 별도 저장본 재로드에서 직업 매핑·몽타주·플레이어/Snapshot 8종 검 샘플·지팡이/내장 무기 설정 확인. 원본 메시·뼈대 해시 8/8 유지, 재로드 오류 0·원본 구형 Rig 참조 경고 4. 실제 게임 확인 대기. 근거: `Saved/Automation/ProfessionAppearanceBuildFinal.txt`, `Saved/Logs/ProfessionAppearanceReload.log`, `Saved/Automation/ProfessionAppearanceReload.json`; [사용자 확인](TODO.md#2-25-파라곤-적용-롤백) |
| 2026-09-22 전투 승리 골드 보상 | 승리 시 5~15G 선택지 3개와 개인 1회 수령·현재 Human 전원 수령 후 Host Continue 구현. 골드/수령 원자 저장·재개 보존·저장 재시도 재추첨 방지·기존 저장 호환 및 결과 UI 연결. 프로젝트 파일 재생성 10.03초·UHT 포함 Development Editor / Win64 빌드 23.08초 성공. 회귀 테스트 8개 추가·기존 진행/상점·1인/협동 UI 진행 테스트 보정 후 최종 증분 빌드 9.30초 성공. 독립 코드 검토·문서 링크·diff 검사 통과, 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/GoldRewardBuild.log`, `Saved/Logs/GoldRewardFinalBuild.log`; [사용자 확인](TODO.md#2-24-전투-승리-골드-보상) |
| 2026-09-22 대상 사망 시 근처 적 재선택 | 근접·투사체 스킬의 발동 전 대상 사망을 현재 위치 기준 유효한 생존 적 재선택으로 통일. 유닛 접근·시전·칼날 추적을 다시 시작하며 비용·원래 복귀점·지점 공격·발사 후 투사체 유지. 기존 프로필과 enum 저장값을 보존하고 휩쓸기 제작 명세 갱신. UHT 포함 Development Editor / Win64 빌드 19.84초 성공, 독립 코드 검토·Python 구문·문서 링크·diff 검사 통과. 재선택·후보 없음·시전 회복·프로필 회귀 코드 추가·갱신, 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/DeadTargetRetargetBuild.log`; [사용자 확인](TODO.md#2-23-대상-사망-시-가까운-적-재선택) |
| 2026-09-22 스킬 미선택 턴 넘기기 | 인간의 빈 스킬 명령을 비용 없는 대기로 허용하고 스킬 선택 취소·SAP 이동 단독 계획·schema 3 준비 완료 저장 복구에 연결. 미장착 스킬·이동 자원·소유권 검증 유지. UHT 포함 Development Editor / Win64 빌드 17.47초 성공, 독립 코드 검토·문서 링크·diff 정적 검사 통과. 턴 넘기기·선택 취소·이동·저장 회귀 코드 추가, 게임·PIE·자동화 미실행. 근거: `Saved/Logs/SkipTurnReadyBuild.log`; [사용자 확인](TODO.md#2-22-스킬-미선택-준비-완료) |
| 2026-09-22 개인 골드 HUD | Gameplay 좌상단에 인벤토리와 같은 개인 확정 잔액 표시. 구매/회복 저장·복제 갱신과 0G 표시, 캐릭터 미확정/대기실 숨김 및 입력 통과 적용. UHT 포함 Development Editor / Win64 빌드 15.64초 성공·오류/경고 0, 코드 검토·문서 링크·diff 정적 검사 통과. 기존 `BP_EnemyUnit` 사용자 변경 보존, Codex의 실제 게임·PIE·자동화 미실행. 근거: `Saved/Logs/PersonalGoldHUDBuild.log`; [사용자 확인](TODO.md#2-21-개인-골드-상시-표시) |
| 2026-09-22 상점 HP 전체 회복 | 모든 스킬 상점에 본인 생존 Human 전용 전체 회복 1G 추가. 서버 직업 설정의 최대 HP 사용, 만피·사망·타인·AI 구매 차단 및 HP/골드 동시 저장. 기존 schema 1의 상품·잔액 보존, schema 0 제외. 인벤토리·설정 변경과 통합한 UHT 빌드 후 최종 Development Editor / Win64 증분 빌드 5.80초 성공·오류/경고 0. 회귀 코드·독립 검토·문서 링크·diff 검사 완료, 게임·PIE·자동화 미실행. 근거: `Saved/Logs/ShopRecoveryBuild.log`; [사용자 확인](TODO.md#2-19-비무장-시작과-스킬-상점) |
| 2026-09-22 인벤토리·설정 단축키 | Gameplay에서 CommonUI 지속 액션으로 I 인벤토리 열기/닫기·Esc 설정 열기/닫기 연결. 개인 골드·보유 스킬 조회와 로컬 배경 입력 차단, 기존 화면 변경 확인/복구 유지. 프로젝트 재생성 8.04초·UHT 포함 Development Editor / Win64 통합 빌드 20.32초 성공·오류/경고 0, 코드 검토·문서 링크·diff 검사 통과. PIE 기본 Esc 중지 우선권 유지, 실제 화면·게임·자동화 미실행. 근거: `Saved/Logs/GameplayShortcutsBuild.log`; [사용자 확인](TODO.md#2-20-인벤토리와-설정-단축키) |
| 2026-09-22 참고 이미지 기반 전투 UI 배치 | 상단 라운드 요약·우측 대상/적·좌하단 파티 카드·하단 중앙 장착 스킬·우하단 이동/준비로 분리. 기존 권한·대상/타일 선택·공통 DPI 유지, 패널별 스크롤·선택 카드 강조와 저장 안내 간격 적용. 상점 변경을 포함한 최종 Development Editor / Win64 빌드 6.05초 성공·오류/경고 0, 독립 코드 검토·문서 링크·diff 검사 통과. UI 에셋 재생성·PIE·실제 화면 확인 미실행. 근거: `Saved/Logs/SkillShopBuildFinal.log`; [사용자 확인](TODO.md#2-17-전투-상단-현황-가독성) |
| 2026-09-22 비무장 시작·개인 스킬 상점 | 새 Run 아군 비무장 1개·직접 조작 캐릭터별 10G, 모든 상점 기존 공격 4종 각 1G. 본인 Human 구매·중복/잔액/소유권 검사와 골드·스킬 원자 저장, 일반/관리 재개·다음 전투 장착 연결. 전 직업 검 부착·Manny 몽타주 제작, 기존 적/Snapshot/과거 저장 기본값 보존. 프로젝트 파일 재생성·UHT 포함 Development Editor / Win64 통합 빌드와 상품 참조·상점 퇴장 오류 안내 보완 후 최종 증분 빌드 3.92초 성공·오류/경고 0, 회귀 코드는 컴파일만 수행. 제작 commandlet 종료 0·오류/경고 0, 실제 구매·게임·PIE·자동화 미실행. 근거: `Saved/Logs/SkillShopBuildFinal.log`, `Saved/Logs/SkillShopMessageBuildFinal.log`, `Saved/Logs/ShopSkillPresentationAuthoring.log`; [사용자 확인](TODO.md#2-19-비무장-시작과-스킬-상점) |
| 2026-09-21 디버그 UI 폴더 분리 | HP 위젯 헤더·소스를 `Source/ProjectA/UI/Debug/`로 이동하고 include·관리 기준 갱신. UHT 포함 Development Editor / Win64 빌드 8.90초 성공, 동작 변경 없음·실제 화면 확인 대기. 근거: `Saved/Logs/DebugUIFolderBuild.log` |
| 2026-09-21 개발용 유닛 HP 표시 | 전투 아군·적 머리 위에 실제 GAS HP/MaxHP 숫자·체력바 표시, 이동·카메라·DPI 추적과 입력 통과. Debug/Development 기본 활성, `projecta.Debug.UnitHP 0`/`1` 전환·Shipping/Test 제외. UHT 포함 Development Editor / Win64 빌드 15.80초 성공·오류/경고 0. 에셋·Config 변경 없음, 실제 화면·플레이·자동화 미실행. 근거: `Saved/Logs/UnitHealthDebugBuild.log`; [사용자 확인](TODO.md#2-18-개발용-유닛-hp-표시) |
| 2026-09-21 검 방향·그립과 칼날 궤적 판정 | Python Rotator의 위치 인자 해석으로 저장된 Yaw 180도를 명시적 Roll 180도로 수정하고 손잡이 Z를 -10cm로 보정. 검만 서버 몽타주 에셋 포즈·칼날 소켓 궤적을 활성 0.23~0.43초에 검사하여 최초 적에게 피해 50/AP 1 적용. 렌더 재생 종료와 판정 누적을 분리하고 기존 GAS·사망 취소·벽 차폐·복귀 보존. 최종 Development Editor / Win64 빌드 4.99초와 별도 재로드의 칼날 표본 123개·리타깃 48개·이전 참조 73개 통과. 회귀 3개는 컴파일만 수행, 실제 플레이·자동화 미실행. 근거: `Saved/Logs/WeaponBladeTraceBuildFinal.log`, `Saved/Logs/WeaponBladeTraceReload.log`; [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) |
| 2026-09-21 전투 상단 현황 배경 | 상단 라운드·명단에 어두운 반투명 배경·둥근 테두리·여백 적용. 높이는 내용에 맞추고 마우스 입력 통과 유지. Development Editor / Win64 컴파일·정적 검사 통과, 실제 화면 확인 대기. 근거: `Saved/Logs/CombatRosterPanelBuild.log`; [사용자 확인](TODO.md#2-17-전투-상단-현황-가독성) |
| 2026-09-21 보행·공격 리타깃의 과도한 골반 이동 수정 | Retargeter 4개의 중복 연산을 각 11→6개로 정리하고 시퀀스 48개를 기존 경로에 재작성, 원본 Root Motion 설정·참조 보존. 전사 전방 보행의 골반 이동 약 454→8cm. Development Editor / Win64 빌드 12.18초 성공, 별도 재로드 48/48 포즈·길이·골반 이동·Kwang 세그먼트/타이밍·이전 참조 73개 통과. 오류 0·기존 경로 조회 경고 6 후 대상 해석 확인. 사용자 보고의 공격 전 순간이동·흔들림은 수정 후 실제 플레이 확인 대기. 근거: `Saved/Logs/RetargetRootMotionBuild.log`, `Saved/Logs/RetargetRootMotionKwangReload.log`; [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) |
| 2026-09-21 검 접근 후퇴 수정과 Kwang 공격 연결 | 유닛 접근 범위 도달 시 즉시 시전하여 정확한 간격을 맞추는 후퇴 방지. 전사/적의 검 몽타주를 Kwang 공격·복귀 2개 세그먼트로 교체, 중복 복귀 0.2초 제외·총 1.933333초·발동 0.23초·블렌드 0.08/0.12초. 피해 50/AP 1·기존 장착·Boss Swing 사본 보존. 최종 Development Editor / Win64 빌드와 별도 재로드에서 세그먼트·타이밍·뼈대·소켓·5/1 장착·이전 참조 73개 확인, 오류 0·기존 경로 조회 경고 6 후 대상 해석 확인. 회귀 코드 컴파일만 수행, 수정 후 실제 플레이·자동화 미실행. 근거: `Saved/Logs/KwangSwordBuildFinal.log`, `Saved/Logs/KwangSwordReload.log`, `Saved/Automation/KwangSwordReload.json`; [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) |
| 2026-09-21 Paragon Manny 애니메이션 가져오기 | 원본 32개 캐릭터 폴더 구조대로 AnimSequence 5,385개 작성·저장, Manny 뼈대·프리뷰 메시 작업 사본 연결. 샘플링률 자동 판정·종료 프레임 경계 맞춤, Additive/MSA 자동 설정 없음·기존 전투 스킬 연결 유지. Development Editor / Win64 빌드·별도 재로드 5,385개 통과: 당시 양수 길이·161개 본 트랙·원본 FBX·뼈대·프리뷰 확인, 오류/경고 0. 후속 최신 저장본도 5,385개 재검사 통과·오류/경고 0이며 단일 프레임 3개의 56/57/58개 본 트랙을 보존. Codex의 Animation Editor 실제 재생·PIE 미실행. 근거: `Saved/Logs/ParagonImportBuild.log`, `Saved/Logs/ParagonReload.log`, `Saved/Logs/ParagonReloadFinal.log`, `Saved/Automation/ParagonAnimationsReload.json`; [사용자 확인](TODO.md#2-16-paragon-fbx-애니메이션-가져오기) |
| 2026-09-21 작업 사본의 원본 폴더 구조 보존 | 전사·적·검 사본 73개를 원본 팩/하위 폴더 구조로 재배치하고 이전 참조·Redirector 정리 완료. 외부 팩 원본 변경 없음. 최종 Editor / Win64 빌드와 별도 재로드에서 이전 경로 73개·전사/적 Swing 세그먼트·뼈대·소켓·5/1 장착 확인. 재로드 오류 0·구객체 조회 경고 6 후 새 대상 해석 확인, 이전 통합 폴더의 사본/Redirector 잔존 0. 실제 플레이 미실행. 근거: `Saved/Logs/WarriorFolderBuild.log`, `WarriorFolderReload.log`, `WarriorFolderRedirectorCleanup.log`, `Saved/Automation/WarriorContentReload.json`; [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) |
| 2026-09-21 전사·검 공격과 휩쓸기 정리 | GKnight 전사·Weapon_Pack 검·BossyEnemy 공격의 작업 사본과 IK 리타깃 연결. 전사 기존 4스킬+검 공격, 기본 적 검 1개·검 공격만 장착, Snapshot 저장 스킬 규칙 유지. 기본공격 표시명 `비무장 공격`, 휩쓸기 `BPDA_SweepingStrike`로 변경·이전 참조 리디렉션. 유닛별 몽타주 대체·보행/DefaultSlot 유지, 맞지 않는 Manny Foot IK 제거. 검 발동 2.15초·원본 길이 5.8667초 유지. 최종 Development Editor / Win64 빌드와 별도 에셋 재로드 통과, 재로드 오류 0·구경로 fallback 조회 경고 2 후 새 에셋 확인. 외부 팩 원본 수정·실제 플레이 없음. 근거: `Saved/Logs/WarriorFinalBuild.log`, `Saved/Automation/WarriorContentReload.json`; [사용자 확인](TODO.md#2-15-전사와-검-공격-콘텐츠) |
| 2026-09-21 전투 확정 규칙과 Ready 복구 | 이동 복귀/예약 충돌·자리 교환 금지·실패 원점 복원, 서버 즉시 피해/사망·미발동 공격 취소, 수정 소유자만 Ready 해제, 양 팀 전멸 패배·Run 종료, Ready 종료 AP/SAP 차감·미환불 적용. 엄호 제거·상태이상 후순위·비근접 추가 속도 보류. schema 3에 비용 차감 전 계획·양 팀 상태를 저장한 뒤 준비를 확정하고 저장 실패 시 차단·재시도. 일반 v5·관리 v4·LegacyOffline v6 복구, v1 비전투·기존 소유권 유지. Development Editor / Win64 빌드 통과, 회귀 코드 추가·최신 PIE/게임/자동화 미실행. 근거: `Saved/Logs/ReadyCheckpointBuildFinal.log`, `Saved/Logs/WarriorFinalBuild.log`; [사용자 확인](TODO.md#2-14-확정-전투-규칙과-준비-완료-복구) |
| 2026-09-18 테스트 적 4마리 배치 | `DA_DefaultEncounter`의 기존 `BP_EnemyUnit`을 4개로 구성하고 Gameplay Arena를 앞열 `(1,2)`, `(2,2)`·뒷열 `(0,3)`, `(3,3)`으로 저장. 두 Combat 노드에 공통 적용하며 적 능력치·스킬은 유지. 파일 잠금 실패 후 에디터 종료 상태에서 저장 재시도 성공, 별도 프로세스 재로드와 Gameplay 에셋 연결 검사 통과. 생성/검사 스크립트의 수·좌표 및 폐기된 HUD 검사를 현재 native 계획 화면 기준으로 갱신. Python 구문·문서 링크·diff 검사 통과, C++·Config 변경 없음·빌드 불필요·PIE 미실행. 근거: `Saved/Automation/TestEnemiesReload.json`, `GameplayAssetValidation.json`, `TestEnemiesStatic.json`; [작동 확인](TODO.md#2-13-테스트용-적-4마리-배치) |
| 2026-09-18 휩쓸기 근접 대상·양옆 | `TargetAndSides` 범위와 근접 접근·복귀 연결. 실제 캡슐 횡방향 스윕·벽 차폐·중복 타격 방지, 가장자리 인접 칸 제한. DA 피해 10·AP 1 보존, 기본공격 몽타주 연결. UHT 포함 Development Editor / Win64 빌드 20.61초 성공, 명령줄 DA 저장·별도 프로세스 재로드 및 정적 검사 통과. 범위·접근·복귀·차폐·목표 사망 회귀 추가, 최신 PIE/자동화 실행은 미실행. C++ 파일 생성 없음·Config 변경 없음·VS 미실행·제작 프로세스 종료. 근거: `Saved/Automation/SweepingStrikeBuild.log`, `SweepingStrikeReload.json`; [남은 확인](TODO.md#2-12-휩쓸기-근접-범위-충돌) |
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
| 2026-09-16 계획 입력 검사·장착 Tile 공격 AI | UI와 서버의 공통 명령 검사, 합법 대상 표시·대상/다른 아군 사망 후 스킬 초안 유지·자원/타일 적용 검사·본인 생존 유닛 계획의 준비 조건 보완. 기존 AI 선택 순서·인간 초안 전 고정을 유지하며 장착된 복귀형 Tile 공격 연결. 디자인 의존 항목은 [TODO 5절](TODO.md#5-디자인-확정-후-구현할-일)에 보류·재개 조건 기록. Editor 빌드 17.52초·최종 초안 보존 보강 후 증분 빌드 4.61초 성공, 두 빌드 컴파일 오류·경고 0. 최종 코드·문서 정적 검사 통과, 자동화·사용자 작동 검증 미실행. [당시 검증](https://github.com/jaba001/ProjectA/blob/81d1e8256664320cc9fd198d3c5bf309eb5074b9/Docs/TEST_REPORT.md#17-계획-입력-검사와-장착-tile-공격-ai) |
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
