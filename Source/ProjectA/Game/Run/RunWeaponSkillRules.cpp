#include "Game/Run/RunWeaponSkillRules.h"

#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunContentMigration.h"
#include "Game/Run/RunItemShopTypes.h"
#include "Game/Run/RunRecoveryTypes.h"
#include "Game/Run/RunSkillBalance.h"
#include "Misc/PackageName.h"
#include "Types/GameplayTagCandidateSelection.h"

namespace
{
    bool IsValidBaseItem(const FRunItemDefinition& Item)
    {
        return Item.Asset.IsValid() && Item.Asset.GetSubPathUtf8String().IsEmpty() && Item.Asset.ToString().StartsWith(TEXT("/Game/")) && FPackageName::IsValidObjectPath(Item.Asset.ToString()) && !Item.DisplayName.ToString().TrimStartAndEnd().IsEmpty() && !Item.Tags.IsEmpty() && Item.Price > 0;
    }

    bool ResolveCandidate(const FRunWeaponSkillCandidate& Candidate, FCombatRoundSkill& OutSkill, FText& OutError)
    {
        if (!Candidate.Skill.IsValid() || !Candidate.Skill.GetSubPathUtf8String().IsEmpty() || !Candidate.Skill.ToString().StartsWith(TEXT("/Game/")) || !FPackageName::IsValidObjectPath(Candidate.Skill.ToString()) || RunContentMigration::IsRemovedSkill(Candidate.Skill) || Candidate.AllowedItemQuery.IsEmpty() || !FMath::IsFinite(Candidate.BaseWeight) || Candidate.BaseWeight < 0.0f) return false;
        const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Candidate.Skill.TryLoad());
        return Skill && Skill->ResolveRoundSkill(OutSkill, OutError) && !RunRecoveryRules::IsConsumable(OutSkill) && !RunContentMigration::IsRemovedSkillId(OutSkill.SkillId) && Candidate.Tags == OutSkill.EffectTags;
    }

    float GradeWeight(const FRunWeaponSkillCandidate& Candidate, const FRunWeaponSkillRulesState& State, const FRunWeaponRarityRule& Rarity)
    {
        if (State.BalanceVersion == 0) return 1.f;
        const FRunSkillRarityWeight* Entry = State.SkillRarityWeights.FindByPredicate([&Candidate, &Rarity](const FRunSkillRarityWeight& Weight) { return Weight.EquipmentRarityTag == Rarity.RarityTag && Weight.SkillRarityTag == Candidate.Balance.RarityTag; });
        return State.BalanceVersion == 1 && Entry ? Entry->Weight : 0.f;
    }

    bool MatchesCandidate(const FRunWeaponSkillCandidate& Candidate, const FRunItemDefinition& Item, const FRunWeaponRarityRule& Rarity, const FRunWeaponSkillRulesState& State)
    {
        FGameplayTagContainer Tags = Candidate.Tags;
        Tags.AppendTags(Candidate.SelectionTags);
        return Candidate.BaseWeight > 0.0f && GradeWeight(Candidate, State, Rarity) > 0.f && Candidate.AllowedItemQuery.Matches(Item.Tags) && (Rarity.SkillQuery.IsEmpty() || Rarity.SkillQuery.Matches(Tags));
    }

    int32 CountEligibleSkills(const FRunItemDefinition& Item, const FRunWeaponSkillRulesState& State, const FRunWeaponRarityRule& Rarity)
    {
        int32 Count = 0;
        for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
        {
            if (MatchesCandidate(Candidate, Item, Rarity, State)) ++Count;
        }
        return Count;
    }

    bool CanUseRarity(const FRunItemDefinition& Item, const FRunWeaponSkillRulesState& State, const FRunWeaponRarityRule& Rarity, bool bWeapon)
    {
        return Rarity.RarityTag.IsValid() && (!Item.CatalogRarityTag.IsValid() || Item.CatalogRarityTag == Rarity.RarityTag) && FMath::IsFinite(Rarity.BaseWeight) && Rarity.BaseWeight > 0.0f && (!bWeapon || CountEligibleSkills(Item, State, Rarity) >= State.SkillCount);
    }
}

bool RunWeaponSkillRules::ResolveSkill(const FSoftObjectPath& Path, const FRunWeaponSkillRulesState& State, FCombatRoundSkill& OutSkill, FText& OutError)
{
    const USkillDefinitionDataAsset* Definition = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
    if (!Definition)
    {
        OutError = NSLOCTEXT("RunWeaponSkills", "MissingSkill", "스킬 정의를 불러올 수 없습니다.");
        return false;
    }
    FCombatRoundSkill Skill;
    if (!Definition->ResolveRoundSkill(Skill, OutError)) return false;
    if (State.BalanceVersion != 0 && State.BalanceVersion != 1)
    {
        OutError = NSLOCTEXT("RunWeaponSkills", "UnknownBalance", "지원하지 않는 스킬 밸런스 버전입니다.");
        return false;
    }
    if (State.BalanceVersion == 1)
    {
        const FRunWeaponSkillCandidate* Candidate = State.Candidates.FindByPredicate([&Path](const FRunWeaponSkillCandidate& Entry) { return Entry.Skill == Path; });
        if (Candidate && !RunSkillBalance::Apply(Candidate->Balance, Skill, OutError)) return false;
    }
    OutSkill = MoveTemp(Skill);
    OutError = FText::GetEmpty();
    return true;
}

bool RunWeaponSkillRules::Validate(const FRunWeaponSkillRulesState& State, FText& OutError)
{
    if (!RunSkillBalance::Validate(State, OutError)) return false;
    OutError = NSLOCTEXT("RunWeaponSkills", "InvalidRules", "무기 스킬 생성 규칙의 버전·개수·태그·후보·등급이 올바르지 않습니다.");
    if (State.SchemaVersion == 0)
    {
        if (State.SkillCount != 0 || !State.WeaponQuery.IsEmpty() || !State.Candidates.IsEmpty() || !State.Rarities.IsEmpty()) return false;
        OutError = FText::GetEmpty();
        return true;
    }
    if (State.SchemaVersion != 1 || State.SkillCount <= 0 || State.WeaponQuery.IsEmpty() || State.Candidates.IsEmpty() || State.Rarities.IsEmpty()) return false;
    TSet<FSoftObjectPath> Paths;
    TSet<FName> SkillIds;
    bool bHasWeightedSkill = false;
    for (const FRunWeaponSkillCandidate& Candidate : State.Candidates)
    {
        FCombatRoundSkill Skill;
        FText Error;
        if (!ResolveCandidate(Candidate, Skill, Error) || Paths.Contains(Candidate.Skill) || SkillIds.Contains(Skill.SkillId)) return false;
        if (State.BalanceVersion == 1 && !RunSkillBalance::Apply(Candidate.Balance, Skill, Error)) return false;
        Paths.Add(Candidate.Skill);
        SkillIds.Add(Skill.SkillId);
        bHasWeightedSkill |= Candidate.BaseWeight > 0.0f;
    }
    TSet<FGameplayTag> Rarities;
    bool bHasWeightedRarity = false;
    for (const FRunWeaponRarityRule& Rarity : State.Rarities)
    {
        if (!Rarity.RarityTag.IsValid() || Rarity.DisplayName.IsEmpty() || !FMath::IsFinite(Rarity.Color.R) || !FMath::IsFinite(Rarity.Color.G) || !FMath::IsFinite(Rarity.Color.B) || !FMath::IsFinite(Rarity.Color.A) || Rarities.Contains(Rarity.RarityTag) || !FMath::IsFinite(Rarity.BaseWeight) || Rarity.BaseWeight < 0.0f) return false;
        Rarities.Add(Rarity.RarityTag);
        bHasWeightedRarity |= Rarity.BaseWeight > 0.0f;
    }
    if (!bHasWeightedSkill || !bHasWeightedRarity) return false;
    OutError = FText::GetEmpty();
    return true;
}

bool RunWeaponSkillRules::CanGenerate(const FRunItemDefinition& BaseItem, const FRunWeaponSkillRulesState& State)
{
    if (State.SchemaVersion != 1 || State.SkillCount <= 0 || State.WeaponQuery.IsEmpty() || !IsValidBaseItem(BaseItem) || BaseItem.GenerationVersion != 0 || BaseItem.ItemInstanceId.IsValid() || BaseItem.RarityTag.IsValid() || !BaseItem.GrantedSkills.IsEmpty() || BaseItem.SkillBalanceVersion != 0 || !BaseItem.GrantedSkillBalances.IsEmpty()) return false;
    const bool bWeapon = State.WeaponQuery.Matches(BaseItem.Tags);
    for (const FRunWeaponRarityRule& Rarity : State.Rarities)
    {
        if (CanUseRarity(BaseItem, State, Rarity, bWeapon)) return true;
    }
    return false;
}

bool RunWeaponSkillRules::ValidateGeneratedCopy(const FRunItemDefinition& Item, const FRunWeaponSkillRulesState& State, FText& OutError)
{
    OutError = NSLOCTEXT("RunWeaponSkills", "InvalidGeneratedCopy", "아이템 사본의 생성 버전·식별자·등급·부여 스킬이 저장된 규칙과 일치하지 않습니다.");
    if (State.SchemaVersion != 1 || State.SkillCount <= 0 || State.WeaponQuery.IsEmpty() || !IsValidBaseItem(Item) || Item.GenerationVersion != State.SchemaVersion || !Item.ItemInstanceId.IsValid()) return false;
    if (Item.CatalogRarityTag.IsValid() && Item.CatalogRarityTag != Item.RarityTag) return false;
    const FRunWeaponRarityRule* Rarity = State.Rarities.FindByPredicate([&Item](const FRunWeaponRarityRule& Rule) { return Rule.RarityTag == Item.RarityTag; });
    if (!Rarity || !Rarity->RarityTag.IsValid() || !FMath::IsFinite(Rarity->BaseWeight) || Rarity->BaseWeight <= 0.0f) return false;
    const bool bWeapon = State.WeaponQuery.Matches(Item.Tags);
    if (Item.GrantedSkills.Num() != (bWeapon ? State.SkillCount : 0)) return false;
    if (Item.SkillBalanceVersion != State.BalanceVersion || Item.GrantedSkillBalances.Num() != (State.BalanceVersion == 1 ? Item.GrantedSkills.Num() : 0)) return false;
    TSet<FSoftObjectPath> Paths;
    TSet<FName> SkillIds;
    for (const FSoftObjectPath& Path : Item.GrantedSkills)
    {
        const FRunWeaponSkillCandidate* Candidate = State.Candidates.FindByPredicate([&Path](const FRunWeaponSkillCandidate& Entry) { return Entry.Skill == Path; });
        FCombatRoundSkill Skill;
        FText Error;
        if (!Candidate || !MatchesCandidate(*Candidate, Item, *Rarity, State) || !ResolveCandidate(*Candidate, Skill, Error) || Paths.Contains(Path) || SkillIds.Contains(Skill.SkillId)) return false;
        if (State.BalanceVersion == 1 && !FRunSkillBalance::StaticStruct()->CompareScriptStruct(&Item.GrantedSkillBalances[Paths.Num()], &Candidate->Balance, 0)) return false;
        Paths.Add(Path);
        SkillIds.Add(Skill.SkillId);
    }
    OutError = FText::GetEmpty();
    return true;
}

bool RunWeaponSkillRules::Generate(const FRunItemDefinition& BaseItem, const FRunWeaponSkillRulesState& State, FRandomStream& Random, FRunItemDefinition& OutCopy, FText& OutError)
{
    if (!Validate(State, OutError)) return false;
    OutError = NSLOCTEXT("RunWeaponSkills", "InvalidBaseItem", "새 사본 생성에는 기존 생성 결과가 없는 유효한 아이템 정의가 필요합니다.");
    if (State.SchemaVersion != 1 || !IsValidBaseItem(BaseItem) || BaseItem.GenerationVersion != 0 || BaseItem.ItemInstanceId.IsValid() || BaseItem.RarityTag.IsValid() || !BaseItem.GrantedSkills.IsEmpty() || BaseItem.SkillBalanceVersion != 0 || !BaseItem.GrantedSkillBalances.IsEmpty()) return false;
    FRandomStream GeneratedRandom = Random;
    const bool bWeapon = State.WeaponQuery.Matches(BaseItem.Tags);
    const FRunWeaponRarityRule* SelectedRarity = nullptr;
    OutError = NSLOCTEXT("RunWeaponSkills", "NoCompatibleCandidates", "아이템과 등급의 태그 조건을 만족하는 서로 다른 스킬 후보가 부족합니다.");
    if (BaseItem.CatalogRarityTag.IsValid())
    {
        // Authored grades never draw a replacement grade or fall back to an unrelated skill pool.
        // 작성된 등급은 대체 등급을 추첨하거나 무관한 스킬 풀로 대체하지 않습니다.
        SelectedRarity = State.Rarities.FindByPredicate([&BaseItem](const FRunWeaponRarityRule& Rarity) { return Rarity.RarityTag == BaseItem.CatalogRarityTag; });
        if (!SelectedRarity || !CanUseRarity(BaseItem, State, *SelectedRarity, bWeapon)) return false;
    }
    else
    {
        // Preserve the frozen legacy catalog's random grade policy when authored grade metadata is absent.
        // 작성 등급 메타데이터가 없으면 저장된 기존 카탈로그의 무작위 등급 정책을 유지합니다.
        TArray<FGameplayTagWeightedCandidate> RarityCandidates;
        RarityCandidates.Reserve(State.Rarities.Num());
        for (const FRunWeaponRarityRule& Rarity : State.Rarities)
        {
            FGameplayTagWeightedCandidate& Candidate = RarityCandidates.AddDefaulted_GetRef();
            Candidate.Tags.AddTag(Rarity.RarityTag);
            Candidate.BaseWeight = CanUseRarity(BaseItem, State, Rarity, bWeapon) ? Rarity.BaseWeight : 0.0f;
        }
        TArray<int32> RarityIndices;
        if (!GameplayTagCandidateSelection::Select(RarityCandidates, FGameplayTagQuery(), 1, false, GeneratedRandom, RarityIndices)) return false;
        SelectedRarity = &State.Rarities[RarityIndices[0]];
    }
    const FRunWeaponRarityRule& Rarity = *SelectedRarity;
    FRunItemDefinition Copy = BaseItem;
    Copy.GenerationVersion = State.SchemaVersion;
    Copy.ItemInstanceId = FGuid::NewGuid();
    Copy.RarityTag = Rarity.RarityTag;
    Copy.SkillBalanceVersion = State.BalanceVersion;
    if (bWeapon && State.BalanceVersion == 1)
    {
        TSet<int32> Selected;
        for (int32 Slot = 0; Slot < State.SkillCount; ++Slot)
        {
            // Draw a compatible rarity first so a large skill catalog cannot inflate that rarity's chance.
            // 후보 스킬 수가 많은 등급의 확률이 부풀지 않도록 적격 등급을 먼저 추첨합니다.
            TArray<FGameplayTagWeightedCandidate> Grades;
            for (const FRunSkillRarityWeight& Weight : State.SkillRarityWeights)
            {
                FGameplayTagWeightedCandidate Grade;
                Grade.Tags.AddTag(Weight.SkillRarityTag);
                Grade.BaseWeight = 0.f;
                if (Weight.EquipmentRarityTag == Rarity.RarityTag)
                {
                    for (int32 Index = 0; Index < State.Candidates.Num(); ++Index)
                    {
                        const FRunWeaponSkillCandidate& Entry = State.Candidates[Index];
                        if (!Selected.Contains(Index) && Entry.Balance.RarityTag == Weight.SkillRarityTag && MatchesCandidate(Entry, BaseItem, Rarity, State))
                        {
                            Grade.BaseWeight = Weight.Weight;
                            break;
                        }
                    }
                }
                Grades.Add(Grade);
            }
            TArray<int32> GradeIndices;
            if (!GameplayTagCandidateSelection::Select(Grades, FGameplayTagQuery(), 1, false, GeneratedRandom, GradeIndices)) return false;
            const FGameplayTag Grade = State.SkillRarityWeights[GradeIndices[0]].SkillRarityTag;
            TArray<FGameplayTagWeightedCandidate> Candidates;
            for (int32 Index = 0; Index < State.Candidates.Num(); ++Index)
            {
                const FRunWeaponSkillCandidate& Entry = State.Candidates[Index];
                FGameplayTagWeightedCandidate Candidate;
                Candidate.Tags = Entry.Tags;
                Candidate.Tags.AppendTags(Entry.SelectionTags);
                Candidate.BaseWeight = !Selected.Contains(Index) && Entry.Balance.RarityTag == Grade && MatchesCandidate(Entry, BaseItem, Rarity, State) ? Entry.BaseWeight : 0.f;
                Candidates.Add(Candidate);
            }
            TArray<int32> Indices;
            if (!GameplayTagCandidateSelection::Select(Candidates, FGameplayTagQuery::MakeQuery_MatchTag(Grade), 1, false, GeneratedRandom, Indices)) return false;
            Selected.Add(Indices[0]);
            Copy.GrantedSkills.Add(State.Candidates[Indices[0]].Skill);
            Copy.GrantedSkillBalances.Add(State.Candidates[Indices[0]].Balance);
        }
    }
    else if (bWeapon)
    {
        TArray<FGameplayTagWeightedCandidate> Candidates;
        Candidates.Reserve(State.Candidates.Num());
        for (const FRunWeaponSkillCandidate& Entry : State.Candidates)
        {
            FGameplayTagWeightedCandidate& Candidate = Candidates.AddDefaulted_GetRef();
            Candidate.Tags = Entry.Tags;
            Candidate.Tags.AppendTags(Entry.SelectionTags);
            Candidate.BaseWeight = MatchesCandidate(Entry, BaseItem, Rarity, State) ? Entry.BaseWeight : 0.0f;
        }
        TArray<int32> Indices;
        if (!GameplayTagCandidateSelection::Select(Candidates, Rarity.SkillQuery, State.SkillCount, false, GeneratedRandom, Indices)) return false;
        for (const int32 Index : Indices) Copy.GrantedSkills.Add(State.Candidates[Index].Skill);
    }
    if (!ValidateGeneratedCopy(Copy, State, OutError)) return false;
    OutCopy = MoveTemp(Copy);
    Random = GeneratedRandom;
    OutError = FText::GetEmpty();
    return true;
}
