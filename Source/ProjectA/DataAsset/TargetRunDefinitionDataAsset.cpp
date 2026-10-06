#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "NativeGameplayTags.h"
#include "Unit/EnemyUnit.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TargetRunEncounter, "Encounter");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_TargetRunDevelopment, "Run.Content.Development");

UTargetRunDefinitionDataAsset::UTargetRunDefinitionDataAsset()
{
    OpponentCatalog = FSoftObjectPath(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Snapshots/DA_OpponentSnapshotCatalog.DA_OpponentSnapshotCatalog"));
    EncounterQuery = FGameplayTagQuery::MakeQuery_MatchTag(TAG_TargetRunEncounter);
    const FGameplayTag Tags[] = {FRunEncounterOffer::GetSkillShopTag(), FRunEncounterOffer::GetItemShopTag(), FRunEncounterOffer::GetRecoveryTag(), FRunEncounterOffer::GetConsumableShopTag(), FRunEncounterOffer::GetRevivalTag()};
    const TCHAR* Names[] = {TEXT("스킬상점"), TEXT("아이템상점"), TEXT("회복소"), TEXT("소모품상점"), TEXT("부활소")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tags); ++Index)
    {
        FRunEncounterOffer& Offer = EncounterPool.AddDefaulted_GetRef();
        Offer.EncounterId = FName(*FString::Printf(TEXT("TargetOffer_%02d"), Index + 1));
        Offer.DisplayName = FText::FromString(Names[Index]);
        Offer.EncounterTag = Tags[Index];
    }
    const TCHAR* MonsterPaths[] =
    {
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Wolf/BP_Monster_Wolf.BP_Monster_Wolf_C"),
        TEXT("/Game/User_JeHoon/Fantasy_Pack/Characters/Orc_Hummer/Blueprints/BP_Monster_Orc.BP_Monster_Orc_C"),
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Spider/BP_Monster_Spider.BP_Monster_Spider_C"),
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Boar/BP_Monster_Boar.BP_Monster_Boar_C"),
        TEXT("/Game/User_JeHoon/Fantasy_Pack/Characters/Troll/Blueprints/BP_Monster_Troll.BP_Monster_Troll_C"),
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Bear/BP_Monster_Bear.BP_Monster_Bear_C"),
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Wolf/BP_Monster_SnowWolf.BP_Monster_SnowWolf_C"),
        TEXT("/Game/User_JeHoon/Fantasy_Pack/Characters/Werewolf/Blueprints/BP_Monster_Werewolf.BP_Monster_Werewolf_C"),
        TEXT("/Game/User_JeHoon/StylizedCreaturesBundle/Blueprints/Bear/BP_Monster_SnowBear.BP_Monster_SnowBear_C"),
        TEXT("/Game/User_JeHoon/Fantasy_Pack/Characters/Golem/Blueprints/BP_Monster_Golem.BP_Monster_Golem_C")
    };
    const FName Classes[] = {TEXT("Archer"), TEXT("Warrior"), TEXT("Mage"), TEXT("Rogue")};
    for (int32 GroupIndex = 0; GroupIndex < 10; ++GroupIndex)
    {
        FTargetRunGroup& Group = Groups.AddDefaulted_GetRef();
        Group.Tags.AddTag(TAG_TargetRunDevelopment);
        Group.GoldChoices = {5 + GroupIndex, 7 + GroupIndex, 10 + GroupIndex};
        const int32 Count = FMath::Min(1 + (GroupIndex + 1) / 3, 4);
        for (int32 Index = 0; Index < Count; ++Index) Group.EnemyClasses.Add(FSoftClassPath(MonsterPaths[FMath::Max(0, GroupIndex - Index)]));
        Group.Opponent.SnapshotId = FName(*FString::Printf(TEXT("TargetLocalOpponent_%02d"), GroupIndex + 1));
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FPartySnapshotMember& Member = Group.Opponent.Members.AddDefaulted_GetRef();
            Member.MemberId = FName(*FString::Printf(TEXT("Opponent_%02d"), Index + 1));
            Member.ClassId = Classes[Index];
            Member.CharacterName = FString::Printf(TEXT("Local %s %d"), *Member.ClassId.ToString(), GroupIndex + 1);
            Member.FormationSlot = Index;
            Member.SkillIds.Add(TEXT("DefaultAttack"));
            Member.Stats.MaxHP = Member.Stats.CurrentHP = 100.0f + 5.0f * GroupIndex;
            Member.Stats.Speed = 10.0f + GroupIndex;
        }
    }
}

bool UTargetRunDefinitionDataAsset::BuildOffers(const FRunTargetState& State, int32 CombatIndex, int32 VisitIndex, TArray<FRunEncounterOffer>& OutOffers)
{
    if (CombatIndex < 0 || CombatIndex >= 20 || VisitIndex < 0 || VisitIndex >= 3) return false;
    TArray<FRunEncounterOffer> Eligible;
    for (const FRunEncounterOffer& Offer : State.EncounterPool)
    {
        FGameplayTagContainer Tags;
        Tags.AddTag(Offer.GetResolvedTag());
        if (Offer.IsSupportedEncounter() && (State.EncounterQuery.IsEmpty() || State.EncounterQuery.Matches(Tags))) Eligible.Add(Offer);
    }
    if (Eligible.Num() < 3) return false;
    OutOffers.Reset();
    // Fixed rotation is the trial selection policy; tag queries still determine eligible content.
    // 고정 순환은 시험용 선택 정책이며 적격 콘텐츠는 태그 쿼리로 결정합니다.
    const int32 Offset = (CombatIndex * 3 + VisitIndex) % Eligible.Num();
    for (int32 Index = 0; Index < 3; ++Index) OutOffers.Add(Eligible[(Offset + Index) % Eligible.Num()]);
    return true;
}

bool UTargetRunDefinitionDataAsset::BuildState(FRunTargetState& OutState, FText& OutError) const
{
    FRunTargetState State;
    State.SchemaVersion = 1;
    State.Groups = Groups;
    State.EncounterPool = EncounterPool;
    State.EncounterQuery = EncounterQuery;
    State.OpponentCatalog = OpponentCatalog;
    UOpponentSnapshotCatalogDataAsset* Catalog = Cast<UOpponentSnapshotCatalogDataAsset>(OpponentCatalog.TryLoad());
    OutError = NSLOCTEXT("TargetRun", "Catalog", "목표 Run의 로컬 Snapshot 카탈로그가 유효하지 않습니다.");
    if (!Catalog) return false;
    for (FTargetRunGroup& Group : State.Groups)
    {
        Group.Opponent.ContentVersion = Catalog->ContentVersion;
        for (const FPartySnapshotMember& Member : Group.Opponent.Members)
        {
            State.SnapshotClasses.Add(Member.ClassId, FSoftClassPath(Catalog->EnemyClasses.FindRef(Member.ClassId).Get()));
            for (FName SkillId : Member.SkillIds) State.SnapshotSkills.Add(SkillId, FSoftObjectPath(Catalog->Skills.FindRef(SkillId)));
        }
    }
    OutState = MoveTemp(State);
    OutError = FText::GetEmpty();
    return true;
}

void UTargetRunDefinitionDataAsset::ApplyGrowth(const FRunTargetState& State, int32 CompletedCombats, FProfessionDefinition& Profession)
{
    for (int32 Index = 0; Index < FMath::Min((CompletedCombats + 1) / 2, State.Groups.Num()); ++Index)
    {
        Profession.MaxHP += State.Groups[Index].MaxHPGrowth;
        Profession.Speed += State.Groups[Index].SpeedGrowth;
    }
}

bool UTargetRunDefinitionDataAsset::Validate(const FRunTargetState& State, const FRunProgressView& Progress, const FRunEncounterProgress& Encounter, FText& OutError)
{
    OutError = NSLOCTEXT("TargetRun", "Invalid", "목표 Run의 고정 편성·선택 기록·콘텐츠 버전이 유효하지 않습니다. 저장 원본을 유지합니다.");
    const bool bTargetRoute = Progress.Nodes.Num() == 20;
    if (State.SchemaVersion == 0)
    {
        const FRunTargetState Empty;
        return !bTargetRoute && FRunTargetState::StaticStruct()->CompareScriptStruct(&State, &Empty, 0);
    }
    if (State.SchemaVersion != 1 || !bTargetRoute || State.Groups.Num() != 10 || State.EncounterPool.Num() < 3 || State.EncounterPool.Num() > 32 || Encounter.SchemaVersion != 2 || State.CompletedEncounterChoices.Num() > 60) return false;
    UOpponentSnapshotCatalogDataAsset* Catalog = Cast<UOpponentSnapshotCatalogDataAsset>(State.OpponentCatalog.TryLoad());
    if (!Catalog) return false;
    TSet<FName> OfferIds;
    for (const FRunEncounterOffer& Offer : State.EncounterPool)
    {
        if (Offer.EncounterId.IsNone() || OfferIds.Contains(Offer.EncounterId) || !Offer.EncounterTag.IsValid() || !Offer.IsSupportedEncounter() || Offer.DisplayName.IsEmpty()) return false;
        OfferIds.Add(Offer.EncounterId);
    }
    for (const FTargetRunGroup& Group : State.Groups)
    {
        if (Group.Tags.IsEmpty() || Group.EnemyClasses.IsEmpty() || Group.EnemyClasses.Num() > 4 || !FMath::IsFinite(Group.MaxHPGrowth) || Group.MaxHPGrowth < 0.0f || Group.MaxHPGrowth > 100.0f || !FMath::IsFinite(Group.SpeedGrowth) || Group.SpeedGrowth < 0.0f || Group.SpeedGrowth > 100.0f || Group.GoldChoices.Num() != 3 || Group.GoldChoices.ContainsByPredicate([](int32 Gold) { return Gold <= 0 || Gold > 1000; })) return false;
        for (const FSoftClassPath& Path : Group.EnemyClasses)
        {
            UClass* Class = Path.TryLoadClass<AEnemyUnit>();
            if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return false;
        }
        FText CatalogError;
        if (!Catalog->ValidateForEncounter(Group.Opponent, 4, CatalogError))
        {
            OutError = CatalogError;
            return false;
        }
        for (const FPartySnapshotMember& Member : Group.Opponent.Members)
        {
            if (State.SnapshotClasses.FindRef(Member.ClassId) != FSoftClassPath(Catalog->EnemyClasses.FindRef(Member.ClassId).Get())) return false;
            for (FName SkillId : Member.SkillIds) if (State.SnapshotSkills.FindRef(SkillId) != FSoftObjectPath(Catalog->Skills.FindRef(SkillId))) return false;
        }
    }
    const int32 ExpectedChoices = Encounter.AfterCompletedNodeCount * 3 + Encounter.VisitIndex + (Encounter.bCompleted ? 1 : 0);
    if (State.CompletedEncounterChoices.Num() != ExpectedChoices) return false;
    if (Encounter.bCompleted && (State.CompletedEncounterChoices.IsEmpty() || State.CompletedEncounterChoices.Last() != Encounter.SelectedEncounterId)) return false;
    for (int32 Index = 0; Index < State.CompletedEncounterChoices.Num(); ++Index)
    {
        TArray<FRunEncounterOffer> Offers;
        if (!BuildOffers(State, Index / 3, Index % 3, Offers) || !Offers.ContainsByPredicate([&State, Index](const FRunEncounterOffer& Offer) { return Offer.EncounterId == State.CompletedEncounterChoices[Index]; })) return false;
    }
    TArray<FRunEncounterOffer> ExpectedOffers;
    if (!BuildOffers(State, Encounter.AfterCompletedNodeCount, Encounter.VisitIndex, ExpectedOffers) || ExpectedOffers.Num() != Encounter.Offers.Num()) return false;
    for (int32 Index = 0; Index < ExpectedOffers.Num(); ++Index)
    {
        if (!FRunEncounterOffer::StaticStruct()->CompareScriptStruct(&ExpectedOffers[Index], &Encounter.Offers[Index], 0)) return false;
    }
    OutError = FText::GetEmpty();
    return true;
}
