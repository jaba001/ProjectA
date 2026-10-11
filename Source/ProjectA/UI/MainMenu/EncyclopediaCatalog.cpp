#include "UI/MainMenu/EncyclopediaCatalog.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Combat/Round/CombatStatusRules.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/RunWeaponSkillRulesDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "Game/Run/RunItemRarityProbabilities.h"
#include "Game/Run/RunPveDifficulty.h"
#include "Game/Run/RunSkillBalance.h"
#include "Game/Run/RunWeaponSkillRules.h"
#include "GAS/CombatGameplayTags.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Csv/CsvParser.h"
#include "UI/Gameplay/RunItemPresentation.h"
#include "UI/ProjectALocalization.h"

namespace
{
    FText Join(const TArray<FText>& Parts)
    {
        return FText::Join(FText::AsCultureInvariant(TEXT("\n\n")), Parts);
    }

    void Add(TArray<FEncyclopediaEntry>& Entries, EEncyclopediaCategory Category, const FText& Title, const FText& Body)
    {
        Entries.Add({Category, Title, Body});
    }

    void AddError(TArray<FEncyclopediaEntry>& Entries, EEncyclopediaCategory Category, const FText& Reason)
    {
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "DataError", "일부 정보를 불러올 수 없습니다"), Reason);
    }

    FText ProfileName(FName Id)
    {
        const TMap<FName, FText> Names = {{TEXT("Dagger"), NSLOCTEXT("Encyclopedia", "Dagger", "단검")}, {TEXT("Shield"), NSLOCTEXT("Encyclopedia", "Shield", "방패")}, {TEXT("Bow"), NSLOCTEXT("Encyclopedia", "Bow", "활")}, {TEXT("OneHandedSword"), NSLOCTEXT("Encyclopedia", "OneHandedSword", "한손검")}, {TEXT("TwoHandedSword"), NSLOCTEXT("Encyclopedia", "TwoHandedSword", "양손검")}, {TEXT("StartingStaff"), NSLOCTEXT("Encyclopedia", "Staff", "지팡이")}};
        const FText* Name = Names.Find(Id);
        return Name ? *Name : FText::FromName(Id);
    }

    FText SkillDetails(const FSoftObjectPath& Path, const FRunWeaponSkillRulesState& Rules, bool bRulesLoaded)
    {
        const USkillDefinitionDataAsset* Definition = Cast<USkillDefinitionDataAsset>(Path.ResolveObject());
        FCombatRoundSkill Skill;
        FText ErrorText;
        if (!Definition || !Definition->ResolveRoundSkill(Skill, ErrorText)) return ErrorText.IsEmpty() ? NSLOCTEXT("RunItem", "MissingSkill", "스킬 정보를 불러올 수 없습니다.") : ErrorText;
        const FRunWeaponSkillCandidate* Candidate = Rules.Candidates.FindByPredicate([&Path](const FRunWeaponSkillCandidate& Entry) { return Entry.Skill == Path; });
        if (Candidate && !bRulesLoaded) return NSLOCTEXT("Encyclopedia", "MissingTuning", "현재 스킬 수치 규칙을 읽지 못했습니다. 이 스킬의 수치는 표시하지 않습니다.");
        FRunItemDefinition Preview;
        Preview.GenerationVersion = 1;
        Preview.GrantedSkills.Add(Path);
        if (Candidate && Rules.BalanceVersion == 1)
        {
            if (!RunSkillBalance::Apply(Candidate->Balance, Skill, ErrorText)) return ErrorText;
            Preview.SkillBalanceVersion = 1;
            Preview.GrantedSkillBalances.Add(Candidate->Balance);
        }
        TArray<FText> Parts;
        const FText Description = RunItemPresentation::SkillDescription(Definition);
        if (!Description.IsEmpty()) Parts.Add(Description);
        const TArray<RunItemPresentation::FItemSkillDetails> Details = RunItemPresentation::SkillDetails(Preview);
        if (!Details.IsEmpty())
        {
            Parts.Add(Details[0].Name);
            Parts.Add(Details[0].Stats);
            Parts.Add(Details[0].Description);
        }
        if (Skill.Kind == ECombatRoundSkillKind::Projectile && !Skill.bUseEffectCollision) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "ProjectileStats", "투사체 속도 {0} cm/s · 판정 반경 {1} cm · 최대 유지 {2}초"), FText::AsNumber(Skill.ProjectileSpeed), FText::AsNumber(Skill.ProjectileRadius), FText::AsNumber(Skill.ProjectileLifetime)));
        if (Skill.Kind == ECombatRoundSkillKind::Melee && !Skill.bUseEffectCollision)
        {
            if (Skill.bUseWeaponTrace) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "WeaponTraceStats", "무기 궤적 판정 · 반경 {0} cm"), FText::AsNumber(Skill.WeaponTraceRadius)));
            else if (Skill.bUseMeleeAreaCollision) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "MeleeAreaStats", "근접 범위 반크기 X {0} / Y {1} / Z {2} cm"), FText::AsNumber(Skill.MeleeAreaHalfExtent.X), FText::AsNumber(Skill.MeleeAreaHalfExtent.Y), FText::AsNumber(Skill.MeleeAreaHalfExtent.Z)));
            else Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "MeleeStats", "근접 유효 거리 {0} cm · 판정 반경 {1} cm"), FText::AsNumber(Skill.HitRange), FText::AsNumber(FMath::Min(Skill.MeleeRadius, Skill.HitRange * 0.5f))));
        }
        if (Skill.Kind == ECombatRoundSkillKind::GroundAttack && !Skill.bUseEffectCollision) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "GroundStats", "지면 판정 반경 {0} cm"), FText::AsNumber(Skill.HitRange)));
        if (Skill.bUseEffectCollision)
        {
            if (Skill.bEffectSphere) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "SphereStats", "효과 판정 대기 {0}초 · 효과 판정 지속 {1}초\n구형 판정 반경 {2} cm"), FText::AsNumber(Skill.EffectHitDelaySeconds), FText::AsNumber(Skill.EffectDuration), FText::AsNumber(Skill.EffectHalfExtent.X)));
            else Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "EffectStats", "효과 판정 대기 {0}초 · 효과 판정 지속 {1}초\n판정 영역 반크기 X {2} / Y {3} / Z {4} cm"), FText::AsNumber(Skill.EffectHitDelaySeconds), FText::AsNumber(Skill.EffectDuration), FText::AsNumber(Skill.EffectHalfExtent.X), FText::AsNumber(Skill.EffectHalfExtent.Y), FText::AsNumber(Skill.EffectHalfExtent.Z)));
        }
        if (CombatRoundRules::UsesChain(Skill)) Parts.Add(FText::Format(NSLOCTEXT("Encyclopedia", "ChainStats", "연쇄 최대 {0}대상 · 도약 거리 {1} cm · 도약 간격 {2}초 · 도약마다 피해 ×{3}"), FText::AsNumber(Skill.Chain.MaxTargets), FText::AsNumber(Skill.Chain.JumpDistance), FText::AsNumber(Skill.Chain.JumpIntervalSeconds), FText::AsNumber(Skill.Chain.DamageMultiplierPerJump)));
        TArray<FCombatStatusEntry> Statuses;
        bool bHasStun = false;
        bool bHasFreeze = false;
        bool bHasConfusion = false;
        for (const FCombatStatusApplication& Application : Skill.StatusApplications)
        {
            bHasStun |= Application.Status == ProjectACombatTags::Status_Stunned;
            bHasFreeze |= Application.Status == ProjectACombatTags::Status_Frozen;
            bHasConfusion |= Application.Status == ProjectACombatTags::Status_Confused;
            if (Application.Status != ProjectACombatTags::Status_Stunned && Application.Status != ProjectACombatTags::Status_Frozen && Application.Status != ProjectACombatTags::Status_Confused) Statuses.Add({Application.Status, Application.Value, 0});
        }
        TArray<FText> StatusNames;
        const FText Persistent = CombatStatusRules::Summary(Statuses);
        if (!Persistent.IsEmpty()) StatusNames.Add(Persistent);
        if (bHasStun) StatusNames.Add(NSLOCTEXT("Encyclopedia", "Stun", "스턴"));
        if (bHasFreeze) StatusNames.Add(NSLOCTEXT("Encyclopedia", "Freeze", "빙결"));
        if (bHasConfusion) StatusNames.Add(NSLOCTEXT("Encyclopedia", "Confusion", "혼란"));
        Parts.Add(StatusNames.IsEmpty() ? NSLOCTEXT("Encyclopedia", "NoStatus", "부여 상태이상: 없음. 스킬의 이름이나 연출만으로 상태이상이 생기지는 않습니다.") : FText::Format(NSLOCTEXT("Encyclopedia", "SkillStatuses", "적중 시 부여 상태이상: {0}\n자세한 처리 방식은 상태이상 탭을 확인하세요."), FText::Join(FText::AsCultureInvariant(TEXT(" · ")), StatusNames)));
        return Join(Parts);
    }

    bool ReadMonsterRows(TArray<TArray<FString>>& OutRows)
    {
        FString Csv;
        if (!FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectDir(), TEXT("DataCatalogs/MONSTER_STATS.csv")))) return false;
        const FCsvParser Parser(MoveTemp(Csv));
        const FCsvParser::FRows& Rows = Parser.GetRows();
        const TCHAR* Headers[] = {TEXT("몬스터 ID"), TEXT("몬스터 이름"), TEXT("Blueprint 클래스 경로"), TEXT("분류 태그"), TEXT("최대 HP"), TEXT("AP"), TEXT("SAP"), TEXT("속도"), TEXT("이동거리(타일)"), TEXT("스킬 경로"), TEXT("공격 피해(참고)"), TEXT("공격 선딜(초·참고)"), TEXT("비고")};
        if (Rows.IsEmpty() || Rows[0].Num() != UE_ARRAY_COUNT(Headers)) return false;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Headers); ++Index) if (FCString::Strcmp(Rows[0][Index], Headers[Index]) != 0) return false;
        for (int32 Index = 1; Index < Rows.Num(); ++Index)
        {
            if (Rows[Index].Num() != UE_ARRAY_COUNT(Headers)) return false;
            TArray<FString>& Copy = OutRows.AddDefaulted_GetRef();
            for (const TCHAR* Cell : Rows[Index]) Copy.Add(Cell);
        }
        return true;
    }

    void AddStatuses(TArray<FEncyclopediaEntry>& Entries)
    {
        const EEncyclopediaCategory Category = EEncyclopediaCategory::Statuses;
        const CombatStatusRules::FTuning Tuning = CombatStatusRules::FTuning::FromSettings();
        const FText BurnFormula = FText::AsCultureInvariant(Tuning.BurnDecay == ECombatBurnDecay::HalfFloor ? TEXT("floor(B / 2)") : TEXT("max(0, floor(B / 2) - 1)"));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "StatusRules", "상태이상의 공통 규칙"), NSLOCTEXT("Encyclopedia", "StatusRulesBody", "수치형 상태는 다시 적중하면 수치를 더합니다. 출혈·냉각·감전은 남은 지속시간도 갱신합니다. 빙결·혼란의 다음 라운드 예약은 중복 부여해도 연장되지 않습니다.\n\n상태 피해도 보호막을 먼저 소모합니다. 사망하거나 전투가 끝나면 상태를 제거합니다. 현재 별도의 정화 행동은 없습니다.\n\n상태이상은 스킬에 실제로 설정된 부여 정보가 있어야 발생합니다. 각 스킬 항목에서 부여 여부와 수치를 확인하세요."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Burn", "화상"), FText::Format(NSLOCTEXT("Encyclopedia", "BurnBody", "라운드의 모든 행동과 투사체가 끝나면 현재 화상 B만큼 피해를 줍니다. 이후 B를 감소시키고, 0이면 제거합니다. 화상을 먼저 처리하고 중독을 처리합니다.\n\n피해 = B\n다음 화상 = {0}\n\n예: 화상 10을 부여한 뒤 첫 피해는 10, 다음 수치는 {1}입니다."), BurnFormula, FText::AsNumber(CombatStatusRules::NextBurn(10, Tuning.BurnDecay))));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Poison", "중독"), NSLOCTEXT("Encyclopedia", "PoisonBody", "라운드 종료 시 화상 다음에 현재 중독 P만큼 피해를 줍니다. 이후 수치를 1 줄이고, 0이면 제거합니다.\n\n피해 = P\n다음 중독 = max(0, P - 1)\n\n예: 중독 3은 추가 부여가 없으면 3 → 2 → 1의 피해를 줍니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Bleed", "출혈"), FText::Format(NSLOCTEXT("Encyclopedia", "BleedBody", "SAP 이동으로 타일 하나에 도착할 때마다 현재 출혈 B만큼 피해를 받습니다. 이동으로 수치가 줄지는 않습니다. 지속시간은 {0}라운드입니다. 접근 공격 후의 복귀는 출혈을 발생시키는 SAP 이동에 포함하지 않습니다. 이동 중 사망하면 남은 경로를 중단합니다.\n\n이동 피해 합계 = B × 완료한 SAP 이동 타일 수\n예: 출혈 4로 2타일 이동하면 피해는 8입니다."), FText::AsNumber(Tuning.BleedingRounds)));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Chill", "냉각"), FText::Format(NSLOCTEXT("Encyclopedia", "ChillBody", "다음 계획 단계부터 {0}라운드 동안 속도를 낮춥니다. 이미 정해진 현재 라운드의 행동 시각은 바꾸지 않습니다.\n\n적용 속도 = min(기본 속도, max({1}, 기본 속도 - 냉각 수치 × {2}))\n기본 속도가 최소값보다 낮아도 속도를 올리지는 않습니다.\n예: 기본 속도 10, 냉각 3이면 속도는 {3}입니다."), FText::AsNumber(Tuning.ChillRounds), FText::AsNumber(Tuning.MinimumChilledSpeed), FText::AsNumber(Tuning.ChillSpeedPerPoint), FText::AsNumber(FMath::Min(10.f, FMath::Max(Tuning.MinimumChilledSpeed, 10.f - 3.f * Tuning.ChillSpeedPerPoint)))));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Freeze", "빙결"), NSLOCTEXT("Encyclopedia", "FreezeBody", "아직 시작하지 않은 대기 행동이 남았다면 현재 행동을 취소합니다. 이미 접근·시전·복귀 중이거나 행동을 마쳤다면 다음 라운드의 행동과 SAP 이동을 막습니다. 다음 라운드가 봉쇄되면 행동 없이 준비 처리합니다. 중복 부여로 봉쇄 라운드를 늘리지는 않습니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Stun", "스턴"), NSLOCTEXT("Encyclopedia", "StunBody", "적중 순간 아직 시작하지 않은 대기 행동이 남은 대상의 현재 행동을 취소합니다. 이미 접근·시전·복귀 중이거나 행동을 마친 대상에게는 효과가 없습니다. 다음 라운드로 이어지지 않습니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Confusion", "혼란"), NSLOCTEXT("Encyclopedia", "ConfusionBody", "다음 계획 단계에서 보유한 유효 스킬과 유효 대상을 무작위로 선택합니다. 아이템은 사용하지 않습니다. 사용할 수 있는 스킬이나 대상이 없으면 대기합니다. 빙결과 겹치면 빙결을 먼저 적용하며 혼란도 소비합니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Shock", "감전"), FText::Format(NSLOCTEXT("Encyclopedia", "ShockBody", "{0}라운드 안에 다음 직접 피해 스킬에 맞으면 발동합니다. 대상 자신과 같은 팀의 상하좌우 인접 유닛에게 현재 감전 S만큼 추가 피해를 줍니다. 대각선은 제외하며 감전은 한 번 발동한 뒤 제거됩니다. 회복·보호막에는 발동하지 않고, 감전 피해로 다른 감전이 연쇄 발동하지 않습니다.\n\n각 대상의 추가 피해 = S\n예: 감전 5가 자신과 인접 동료 2명에게 적용되면 각 5, 합계 15입니다."), FText::AsNumber(Tuning.ShockRounds)));
    }

    void AddFormulas(TArray<FEncyclopediaEntry>& Entries)
    {
        const EEncyclopediaCategory Category = EEncyclopediaCategory::Formulas;
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Stats", "HP · 속도 · AP · SAP"), NSLOCTEXT("Encyclopedia", "StatsBody", "HP가 0이 되면 사망합니다. 최대 HP는 회복의 상한입니다. 속도는 준비가 끝난 라운드에서 행동을 시작하는 순서를 정합니다.\n\nAP는 스킬 행동에, SAP는 전술 이동과 스킬의 보조 비용에 사용합니다. 스킬의 AP·SAP 비용을 합산하여 보유량을 넘는 계획을 할 수 없습니다. 전술 이동은 AP 행동보다 먼저 실행하며 공격의 접근·복귀와 구분합니다.\n\nAP 잔여 = 보유 AP - 예약한 스킬 AP 합계\nSAP 잔여 = 보유 SAP - 예약한 이동·스킬 SAP 합계"));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Damage", "피해와 보호막 공식"), NSLOCTEXT("Encyclopedia", "DamageBody", "기본 피해 D는 해당 스킬의 위력을 사용합니다. 실제 적중과 효과 적용에 성공한 대상에 적용하며, 현재 기본 경로에는 별도의 방어력·치명타 배율·확률 명중식이 없습니다.\n\n보호막 흡수 = min(현재 보호막, D)\n남은 보호막 = max(0, 현재 보호막 - D)\n남은 HP = max(0, 현재 HP - max(0, D - 현재 보호막))\n\n예: HP 100, 보호막 15에서 피해 25를 받으면 보호막 0, HP 90입니다. 보호막은 라운드 종료 시 제거합니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Healing", "회복 공식"), NSLOCTEXT("Encyclopedia", "HealingBody", "회복 스킬은 살아 있는 대상에 위력만큼 HP를 더하고 최대 HP에서 제한합니다. 사망자를 회복 스킬로 살릴 수는 없습니다.\n\n회복 후 HP = min(최대 HP, 현재 HP + 회복 위력)\n실제 회복량 = min(회복 위력, 최대 HP - 현재 HP)\n\n예: 최대 HP 100, 현재 HP 90, 회복 25이면 HP 100이며 실제 회복량은 10입니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Timing", "행동 시각 공식"), NSLOCTEXT("Encyclopedia", "TimingBody", "모든 캐릭터의 준비가 끝난 뒤 확정된 속도와 계획으로 라운드를 해결합니다. 빠른 유닛이 먼저 행동을 시작하고, 접근과 스킬 선딜을 마친 뒤 효과를 발동합니다.\n\n행동 시작 지연(초) = max(라운드 최고 속도 - 유닛 속도, 0) × 0.1\n발동 시각 = 행동 시작 + 접근 시간 + 선딜\n\n예: 최고 속도 12, 내 속도 8이면 시작 지연은 0.4초입니다. 투사체나 효과의 판정 대기가 있으면 실제 적중은 발동 이후입니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "ChainFormula", "연쇄 피해와 투사체"), NSLOCTEXT("Encyclopedia", "ChainFormulaBody", "연쇄 최대 대상 수에는 처음 맞은 대상도 포함합니다. 각 도약의 피해 배율은 누적 적용합니다. 거리와 도약 간격은 스킬별 정보를 확인하세요.\n\n도약 k의 피해 = 기본 위력 × 도약 배율^k (첫 대상 k = 0)\n예: 위력 40, 배율 0.5이면 40 → 20 → 10입니다.\n\n고정 속도의 투사체 예상 이동 시간 = 이동 거리 / 투사체 속도\n이 값은 예상치이며 실제 이동·충돌·대상 위치에 따라 적중 시점이 달라집니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "Selection", "장비 부여 스킬의 선택 확률"), NSLOCTEXT("Encyclopedia", "SelectionBody", "장비 종류의 태그 조건, 장비 등급의 스킬 조건, 양수 가중치를 만족한 후보만 부여할 수 있습니다. 새 Run은 적격 후보가 남은 스킬 등급을 먼저 뽑고, 그 등급 안에서 후보를 뽑습니다. 아래 식은 콘텐츠 선택 확률이며 전투 명중 확률이 아닙니다.\n\n등급 확률 = 해당 등급 가중치 / 적격 후보가 남은 등급 가중치 합\n등급 내 후보 확률 = 후보 기본 가중치 / 같은 등급 적격 후보의 기본 가중치 합\n최종 후보 확률 = 등급 확률 × 등급 내 후보 확률\n\n예: 등급 확률 50%, 같은 등급의 후보 가중치 1과 3이면 최종 확률은 12.5%와 37.5%입니다. 중복 없이 여러 개를 부여할 때는 이미 뽑은 후보를 제외하고 다시 계산합니다. 기존 밸런스 버전 0은 등급 추첨 없이 적격 후보의 기본 가중치를 사용합니다."));
        Add(Entries, Category, NSLOCTEXT("Encyclopedia", "PveFormula", "PvE 난이도와 보상 공식"), NSLOCTEXT("Encyclopedia", "PveFormulaBody", "새 Run은 묶음·난이도별 기준 HP를 사용하며 편성에서 가장 높은 기본 HP를 기준으로 나머지 몬스터의 비율을 유지합니다.\n\n최고 체력 몬스터 HP = 해당 묶음·난이도의 기준 HP\n나머지 HP = ceil(기준 HP × 기본 HP / 편성 내 최고 기본 HP)\n속도 = 기본 속도 × 난이도 속도 배율\n골드 = max(1, round(기본 보상 골드 × 난이도 골드 배율))\n\n예: 기본 HP 200과 100인 편성의 기준 HP가 150이면 각각 150과 75입니다. 이전 저장의 곡선 버전 0은 기존 HP 배율을 유지합니다. 상태 피해·감소 공식은 상태이상 탭에 있습니다."));
    }
}

FSoftObjectPath EncyclopediaCatalog::DefaultPartyPath()
{
    return FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Parties/DA_VerticalSliceParty.DA_VerticalSliceParty"));
}

void EncyclopediaCatalog::GetSkillPaths(TArray<FSoftObjectPath>& OutPaths)
{
    OutPaths.Reset();
    FARFilter Filter;
    Filter.ClassPaths.Add(USkillDefinitionDataAsset::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills"));
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        const FSoftObjectPath Path = Asset.GetSoftObjectPath();
        if (!RunContentMigration::IsRemovedSkill(Path)) OutPaths.AddUnique(Path);
    }
    OutPaths.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right) { return Left.ToString() < Right.ToString(); });
}

FText EncyclopediaCatalog::CategoryName(EEncyclopediaCategory Category)
{
    switch (Category)
    {
    case EEncyclopediaCategory::Equipment: return NSLOCTEXT("Encyclopedia", "EquipmentTab", "장비");
    case EEncyclopediaCategory::Skills: return NSLOCTEXT("Encyclopedia", "SkillsTab", "스킬");
    case EEncyclopediaCategory::Professions: return NSLOCTEXT("Encyclopedia", "ProfessionsTab", "직업");
    case EEncyclopediaCategory::Statuses: return NSLOCTEXT("Encyclopedia", "StatusesTab", "상태이상");
    case EEncyclopediaCategory::Formulas: return NSLOCTEXT("Encyclopedia", "FormulasTab", "전투·공식");
    case EEncyclopediaCategory::Run: return NSLOCTEXT("Encyclopedia", "RunTab", "모험·상점");
    case EEncyclopediaCategory::Monsters: return NSLOCTEXT("Encyclopedia", "MonstersTab", "몬스터");
    default: return FText::GetEmpty();
    }
}

void EncyclopediaCatalog::Build(const UPartyDefinitionDataAsset* Party, const TArray<FSoftObjectPath>& SkillPaths, TArray<FEncyclopediaEntry>& OutEntries)
{
    OutEntries.Reset();
    const URunWeaponSkillRulesDataAsset* RuleAsset = Party && Party->WeaponSkillRules ? Party->WeaponSkillRules.Get() : GetDefault<URunWeaponSkillRulesDataAsset>();
    FRunWeaponSkillRulesState Rules = RuleAsset->Rules;
    FText ErrorText;
    const bool bRulesLoaded = RuleAsset->BuildState(Rules, ErrorText);
    if (!bRulesLoaded)
    {
        AddError(OutEntries, EEncyclopediaCategory::Skills, ErrorText);
        AddError(OutEntries, EEncyclopediaCategory::Equipment, ErrorText);
    }
    Add(OutEntries, EEncyclopediaCategory::Equipment, NSLOCTEXT("Encyclopedia", "EquipmentGuide", "장비 종류와 장착"), NSLOCTEXT("Encyclopedia", "EquipmentGuideBody", "현재 판매 카탈로그의 장비와 장착 위치를 안내합니다. 양손 장비는 주 무기와 보조 무기 슬롯을 함께 사용합니다. 방패는 보조 무기에 장착합니다. 장착으로 부여되는 스킬은 해당 장비를 사용하는 동안 사용할 수 있습니다.\n\n장비 상세에는 현재 가격·등급·장착 위치와 실제 등급·태그 조건을 통과한 부여 가능 스킬을 표시합니다. 실제 생성된 장비의 스킬과 수치는 구매·장착 화면에서 확인하세요.\n\n투구·장갑·신발·갑옷·목걸이·반지 슬롯은 준비되어 있으나 판매·장착 지원은 현재 카탈로그와 프로필에 등록된 콘텐츠에 한합니다."));
    TArray<FRunItemDefinition> Items;
    if (RunItemShopCatalog::Load(Items, ErrorText))
    {
        for (const FRunEquipmentProfile& Profile : URunEquipmentCatalog::Get().Profiles)
        {
            TArray<FText> Members;
            FText Description;
            for (const FRunItemDefinition& Item : Items)
            {
                if (URunEquipmentCatalog::Get().ResolveProfile(Item) != &Profile) continue;
                Members.Add(RunItemPresentation::Name(Item, Rules.Rarities));
                Description = RunItemPresentation::EquipmentDescription(Item);
            }
            if (!Members.IsEmpty()) Add(OutEntries, EEncyclopediaCategory::Equipment, FText::Format(NSLOCTEXT("Encyclopedia", "EquipmentType", "종류: {0}"), ProfileName(Profile.ProfileId)), Join({Description, FText::Join(FText::AsCultureInvariant(TEXT("\n")), Members)}));
        }
        for (const FRunItemDefinition& Item : Items)
        {
            const FRunWeaponRarityRule* Rarity = Rules.Rarities.FindByPredicate([&Item](const FRunWeaponRarityRule& Entry) { return Entry.RarityTag == Item.CatalogRarityTag; });
            const FText Grade = Rarity ? ProjectALocalization::Content(TEXT("ItemRarity.") + Rarity->RarityTag.ToString() + TEXT(".Name"), Rarity->DisplayName) : NSLOCTEXT("Encyclopedia", "RandomRarity", "생성 시 선택");
            TArray<FText> Skills;
            TSet<FSoftObjectPath> EligibleSkills;
            if (bRulesLoaded && RunWeaponSkillRules::CanGenerate(Item, Rules))
            {
                for (const FRunWeaponRarityRule& GradeRule : Rules.Rarities)
                {
                    if (GradeRule.BaseWeight <= 0.0f) continue;
                    if (Item.CatalogRarityTag.IsValid() && GradeRule.RarityTag != Item.CatalogRarityTag) continue;
                    for (const FRunWeaponSkillCandidate& Candidate : Rules.Candidates)
                    {
                        if (!EligibleSkills.Contains(Candidate.Skill) && RunWeaponSkillRules::IsEligibleCandidate(Candidate, Item, GradeRule, Rules))
                        {
                            EligibleSkills.Add(Candidate.Skill);
                            Skills.Add(RunItemPresentation::SkillName(Cast<USkillDefinitionDataAsset>(Candidate.Skill.ResolveObject())));
                        }
                    }
                }
            }
            const FText SkillList = !bRulesLoaded ? NSLOCTEXT("Encyclopedia", "UnavailableCandidates", "수치 규칙을 읽지 못해 후보를 표시할 수 없습니다.") : Skills.IsEmpty() ? NSLOCTEXT("Encyclopedia", "NoCandidates", "부여 가능한 스킬이 없습니다.") : FText::Join(FText::AsCultureInvariant(TEXT("\n")), Skills);
            Add(OutEntries, EEncyclopediaCategory::Equipment, RunItemPresentation::Name(Item, Rules.Rarities), Join({FText::Format(NSLOCTEXT("Encyclopedia", "EquipmentStats", "가격 {0} G · 등급 {1}"), FText::AsNumber(Item.Price), Grade), RunItemPresentation::EquipmentDescription(Item), FText::Format(NSLOCTEXT("Encyclopedia", "Candidates", "부여 가능 스킬\n{0}"), SkillList)}));
        }
    }
    else AddError(OutEntries, EEncyclopediaCategory::Equipment, ErrorText);
    for (const FSoftObjectPath& Path : SkillPaths)
    {
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.ResolveObject());
        if (Skill && !RunContentMigration::IsRemovedSkillId(Skill->SkillId)) Add(OutEntries, EEncyclopediaCategory::Skills, RunItemPresentation::SkillName(Skill), SkillDetails(Path, Rules, bRulesLoaded));
        else AddError(OutEntries, EEncyclopediaCategory::Skills, NSLOCTEXT("Encyclopedia", "SkillLoadFailed", "일부 스킬 정의를 읽지 못했습니다. 콘텐츠 설치 상태를 확인해 주세요."));
    }
    if (SkillPaths.IsEmpty()) AddError(OutEntries, EEncyclopediaCategory::Skills, NSLOCTEXT("Encyclopedia", "EmptySkills", "등록된 스킬이 없습니다. 콘텐츠 설치 상태를 확인해 주세요."));
    if (Party)
    {
        for (const TPair<FName, FProfessionDefinition>& Entry : Party->Professions)
        {
            Add(OutEntries, EEncyclopediaCategory::Professions, ProjectALocalization::Content(TEXT("Profession.") + Entry.Key.ToString() + TEXT(".Name"), Entry.Value.DisplayName), Party->GetProfessionDetails(Entry.Key));
        }
        Add(OutEntries, EEncyclopediaCategory::Professions, NSLOCTEXT("Encyclopedia", "StartingLoadout", "새 캐릭터의 시작 스킬"), NSLOCTEXT("Encyclopedia", "StartingLoadoutBody", "직업 상세는 직업 전투 클래스의 기본 능력과 스킬을 설명합니다. 새 Run에서 만든 캐릭터는 공통 맨손 스킬로 시작하며, 실제 스킬은 습득하거나 장비를 장착하여 늘립니다. 직업의 기본 전투 클래스 장착과 새 Run의 시작 장착은 구분합니다."));
    }
    else AddError(OutEntries, EEncyclopediaCategory::Professions, NSLOCTEXT("Encyclopedia", "PartyMissing", "직업 정의를 읽지 못했습니다. 콘텐츠 설치 상태를 확인해 주세요."));
    AddStatuses(OutEntries);
    AddFormulas(OutEntries);
    Add(OutEntries, EEncyclopediaCategory::Run, NSLOCTEXT("Encyclopedia", "RunFlow", "모험의 진행과 저장"), NSLOCTEXT("Encyclopedia", "RunFlowBody", "기본 Run은 10묶음으로 진행합니다. 각 전투 전에 3번의 인카운터를 선택하며, 묶음마다 PvE와 Snapshot PvP 전투를 진행합니다. 전체 60선택·20전투가 기본 목표입니다.\n\n싱글플레이에서는 선택한 캐릭터 1명을 직접 조작하고 다른 파티원은 AI가 조작합니다. 미로에서 선택한 장소나 전장으로 이동한 뒤 다음 화면을 엽니다.\n\n이어하기는 저장된 진행과 생성 결과를 복원합니다. 전투의 복구 기준은 확정된 준비 경계이며 진행 중 연출의 매 프레임을 저장하지는 않습니다."));
    Add(OutEntries, EEncyclopediaCategory::Run, NSLOCTEXT("Encyclopedia", "Shops", "장비·스킬 상점"), NSLOCTEXT("Encyclopedia", "ShopsBody", "장비 상점에서는 가격과 장착 위치, 부여 스킬을 확인하고 골드로 구매합니다. 기본 상점은 5개 상품을 진열하고 전문 상점은 조건에 맞는 상품을 최대 5개 진열합니다. 재추첨은 화면에 표시된 비용을 지불하며 기존 구매를 되돌리지 않습니다.\n\n새 기본 Run에서는 장비에 부여된 스킬을 사용하며 스킬 상점을 제시하지 않습니다. 이전 Run·명시적인 시험 구성·개발 협동의 스킬 상점은 후보를 선택해 스킬을 습득하는 기존 방식을 유지합니다. 실제 방문의 상품·가격·부여 스킬·비용은 방문 화면에 표시된 저장 결과가 기준입니다."));
    Add(OutEntries, EEncyclopediaCategory::Run, NSLOCTEXT("Encyclopedia", "Recovery", "회복·부활·소모품"), NSLOCTEXT("Encyclopedia", "RecoveryBody", "회복은 살아 있는 파티원의 HP를 보충하고 부활은 사망한 파티원을 되살립니다. 비용과 회복량은 해당 방문의 화면에서 확인합니다.\n\n회복 소모품은 수량을 따로 보관하며, 스킬로 회복할 때 실제 행동 비용과 수량을 함께 확인합니다. 소모품을 무제한 보유 스킬과 같은 것으로 취급하지 않습니다. 회복 공식과 최대 HP 제한은 전투·공식 탭을 확인하세요."));
    FRunItemRarityProbabilityState ItemProbabilities;
    if (RunItemRarityProbabilities::Load(ItemProbabilities, ErrorText))
    {
        TArray<FText> Probabilities;
        for (const FRunItemRarityProbability& Probability : ItemProbabilities.Entries)
        {
            const FRunWeaponRarityRule* Rarity = Rules.Rarities.FindByPredicate([&Probability](const FRunWeaponRarityRule& Entry) { return Entry.RarityTag == Probability.RarityTag; });
            const FText Name = Rarity ? ProjectALocalization::Content(TEXT("ItemRarity.") + Rarity->RarityTag.ToString() + TEXT(".Name"), Rarity->DisplayName) : FText::FromName(Probability.RarityTag.GetTagName());
            Probabilities.Add(FText::Format(NSLOCTEXT("Encyclopedia", "GradeProbability", "{0}: {1}%"), Name, FText::AsNumber(Probability.ProbabilityBasisPoints / 100.f)));
        }
        Add(OutEntries, EEncyclopediaCategory::Run, NSLOCTEXT("Encyclopedia", "ItemProbabilities", "아이템 등급 확률"), Join({FText::Join(FText::AsCultureInvariant(TEXT("\n")), Probabilities), NSLOCTEXT("Encyclopedia", "ItemProbabilitiesBody", "먼저 등급을 뽑고 해당 등급의 적격 아이템을 균등하게 뽑습니다. 해당 방문에서 후보가 없는 등급은 제외하고 남은 등급의 가중치 비율을 사용합니다. 중복을 허용하지 않는 진열에서는 이미 뽑은 아이템을 다음 추첨에서 제외합니다.\n\n실제 등급 확률 = 해당 등급의 기본 확률 / 후보가 남은 등급의 기본 확률 합") }));
    }
    else AddError(OutEntries, EEncyclopediaCategory::Run, ErrorText);
    if (bRulesLoaded && Rules.BalanceVersion == 1)
    {
        for (const FRunWeaponRarityRule& Rarity : Rules.Rarities)
        {
            TArray<FText> Weights;
            for (const FRunSkillRarityWeight& Weight : Rules.SkillRarityWeights)
            {
                if (Weight.EquipmentRarityTag == Rarity.RarityTag) Weights.Add(FText::Format(NSLOCTEXT("Encyclopedia", "GradeWeight", "스킬 {0}: 가중치 {1}"), RunSkillBalance::RarityName(Weight.SkillRarityTag), FText::AsNumber(Weight.Weight)));
            }
            Add(OutEntries, EEncyclopediaCategory::Run, FText::Format(NSLOCTEXT("Encyclopedia", "SkillWeights", "{0} 장비의 스킬 등급 가중치"), ProjectALocalization::Content(TEXT("ItemRarity.") + Rarity.RarityTag.ToString() + TEXT(".Name"), Rarity.DisplayName)), Join({FText::Join(FText::AsCultureInvariant(TEXT("\n")), Weights), FText::Format(NSLOCTEXT("Encyclopedia", "SkillCount", "장비당 부여 스킬 {0}개. 실제 후보 확률은 장비 종류·태그 조건과 후보별 기본 가중치를 함께 적용합니다. 전투·공식 탭의 선택 확률을 확인하세요."), FText::AsNumber(Rules.SkillCount))}));
        }
    }
    FRunPveDifficultyState Difficulty;
    if (RunPveDifficulty::Load(Difficulty, ErrorText))
    {
        for (const FRunPveDifficultyRule& Rule : Difficulty.Rules)
        {
            TArray<FText> Health;
            for (int32 Index = 0; Index < Rule.ReferenceHPByGroup.Num(); ++Index) Health.Add(FText::Format(NSLOCTEXT("Encyclopedia", "GroupHealth", "묶음 {0}: 기준 HP {1}"), FText::AsNumber(Index + 1), FText::AsNumber(Rule.ReferenceHPByGroup[Index])));
            Add(OutEntries, EEncyclopediaCategory::Run, FText::Format(NSLOCTEXT("Encyclopedia", "DifficultyTitle", "PvE 난이도: {0}"), ProjectALocalization::Content(TEXT("Difficulty.") + Rule.DifficultyTag.ToString() + TEXT(".Name"), Rule.DisplayName)), Join({FText::Format(NSLOCTEXT("Encyclopedia", "DifficultyScales", "속도 ×{0} · 골드 ×{1}\n이전 HP 곡선 버전 0의 HP 배율 ×{2}"), FText::AsNumber(Rule.SpeedScale), FText::AsNumber(Rule.GoldScale), FText::AsNumber(Rule.HPScale)), FText::Join(FText::AsCultureInvariant(TEXT("\n")), Health)}));
        }
    }
    else AddError(OutEntries, EEncyclopediaCategory::Run, ErrorText);
    TArray<TArray<FString>> Monsters;
    if (ReadMonsterRows(Monsters))
    {
        for (const TArray<FString>& Row : Monsters)
        {
            const FSoftObjectPath SkillPath(Row[9]);
            Add(OutEntries, EEncyclopediaCategory::Monsters, ProjectALocalization::AssetName(FSoftObjectPath(Row[2]), FText::FromString(Row[1])), Join({FText::Format(NSLOCTEXT("Encyclopedia", "MonsterStats", "기본 HP {0} · 속도 {1}\nAP {2} · SAP {3} · 이동거리 {4}타일\n실제 PvE HP·속도는 묶음과 선택한 난이도에 따라 달라집니다."), FText::AsCultureInvariant(Row[4]), FText::AsCultureInvariant(Row[7]), FText::AsCultureInvariant(Row[5]), FText::AsCultureInvariant(Row[6]), FText::AsCultureInvariant(Row[8])), SkillDetails(SkillPath, Rules, bRulesLoaded)}));
        }
    }
    else AddError(OutEntries, EEncyclopediaCategory::Monsters, NSLOCTEXT("Encyclopedia", "MonsterDataError", "몬스터 카탈로그의 파일 또는 열 구성을 읽지 못했습니다."));
}
