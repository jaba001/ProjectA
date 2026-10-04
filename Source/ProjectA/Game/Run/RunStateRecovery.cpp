#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunSaveGame.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Game/Run/RunParticipationLibrary.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "Engine/World.h"

bool URunStateSubsystem::ConfigureRecoveryRun(URunSaveGame* Save, FText& OutError) const
{
    if (!Save || Save->TargetRun.SchemaVersion != 1) return false;
    FRunRecoveryState& Rules = Save->TargetRun.Recovery;
    Rules = FRunRecoveryState();
    Rules.SchemaVersion = 1;
    Rules.ConsumableTag = RunRecoveryRules::GetHealingItemTag();
    Rules.HealingSkill = RunRecoveryRules::GetHealingSkillPath();
    if (!RunRecoveryRules::ValidateState(Rules, OutError)) return false;
    for (FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated) continue;
        FRunConsumableStack Stack;
        Stack.ItemTag = Rules.ConsumableTag;
        Stack.Skill = Rules.HealingSkill;
        Stack.Quantity = Rules.StartingQuantity;
        Member.Consumables = {Stack};
    }
    return true;
}

bool URunStateSubsystem::ValidateRecoverySave(const URunSaveGame* Save, FText& OutError) const
{
    if (!Save) return false;
    const FRunRecoveryState& Rules = Save->TargetRun.Recovery;
    if (Save->TargetRun.SchemaVersion == 0)
    {
        const FRunRecoveryState Empty;
        return FRunRecoveryState::StaticStruct()->CompareScriptStruct(&Rules, &Empty, 0) && !Save->Party.ContainsByPredicate([](const FRunPartyMember& Member) { return !Member.Consumables.IsEmpty(); });
    }
    if (!RunRecoveryRules::ValidateState(Rules, OutError)) return false;
    for (const FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated)
        {
            if (!Member.Consumables.IsEmpty()) return false;
            continue;
        }
        if (Member.Consumables.Num() != 1 || !RunRecoveryRules::ValidateStacks(Member.Consumables, OutError)) return false;
        const FRunConsumableStack& Stack = Member.Consumables[0];
        if (Stack.ItemTag != Rules.ConsumableTag || Stack.Skill != Rules.HealingSkill) return false;
    }
    return true;
}

bool URunStateSubsystem::PurchaseRecoveryOffer(const FRunAccountId& BuyerAccountId, FGuid CharacterId, FName OfferId, FText& OutError, int32 ExpectedRevision)
{
    OutError = FText::FromString(TEXT("현재 회복 서비스를 구매할 수 없습니다."));
    if ((GetWorld() && GetWorld()->GetNetMode() == NM_Client) || !CanMutateManagedRun() || bManagedResumePending || !IsTargetRun() || Phase != ERunPhase::Shop || EncounterProgress.bCompleted) return false;
    const FRunEncounterOffer* Selected = EncounterProgress.FindSelectedOffer();
    const FRunRecoveryState& Rules = TargetRun.Recovery;
    if (!Selected || !Selected->IsService() || OfferId != Selected->GetResolvedTag().GetTagName() || Rules.Revision <= 0 || Rules.Revision == MAX_int32 || ExpectedRevision != Rules.Revision || !RunRecoveryRules::ValidateState(Rules, OutError)) return false;
    const FRunPartyMember* Member = PartyMembers.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.CharacterId == CharacterId; });
    if (!Member || !URunIdentityLibrary::IsCharacterOwner(RunIdentity, PartyMembers, CharacterId, BuyerAccountId) || !FMath::IsFinite(Member->CurrentHP) || Member->CurrentHP < 0.f) return false;
    if (bManagedRun)
    {
        EPartyControlMode Mode = EPartyControlMode::ServerAI;
        if (!URunParticipationLibrary::ResolveControlMode(Participation, RunIdentity, PartyMembers, CharacterId, Mode, OutError) || Mode != EPartyControlMode::Human) return false;
    }
    else
    {
        int32 PlayerSlot = INDEX_NONE;
        if (!URunParticipationLibrary::ResolveStandalonePlayerSlot(PartyMembers, PlayerSlot, OutError) || Member->SlotIndex != PlayerSlot) return false;
    }
    FProfessionDefinition Profession;
    if (!ResolveMemberProfession(*Member, Profession, OutError)) return false;
    const FGameplayTag Service = Selected->GetResolvedTag();
    const bool bConsumable = Service.MatchesTag(FRunEncounterOffer::GetConsumableShopTag());
    const bool bRevival = Service.MatchesTag(FRunEncounterOffer::GetRevivalTag());
    const bool bRecovery = Service.MatchesTag(FRunEncounterOffer::GetRecoveryTag());
    if (!bConsumable && !bRevival && !bRecovery) return false;
    const int32 Price = bConsumable ? Rules.ConsumablePrice : bRevival ? Rules.RevivalPrice : Rules.RecoveryPrice;
    if (Member->Gold < Price || (bRevival ? Member->CurrentHP != 0.f : Member->CurrentHP <= 0.f) || (bRecovery && Member->CurrentHP >= Profession.MaxHP)) return false;
    const FRunConsumableStack* Owned = Member->Consumables.FindByPredicate([&Rules](const FRunConsumableStack& Stack) { return Stack.ItemTag == Rules.ConsumableTag && Stack.Skill == Rules.HealingSkill; });
    if (bConsumable && (!Owned || Owned->Quantity >= RunRecoveryRules::MaximumQuantity)) return false;
    // Persist the complete candidate first, so rejected writes and repeated revisions change nothing.
    // 완성된 후보를 먼저 저장하여 저장 실패와 중복 revision 요청이 상태를 바꾸지 않도록 합니다.
    TStrongObjectPtr<URunSaveGame> Save(CreateSaveData());
    FRunPartyMember* Purchased = Save->Party.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.CharacterId == CharacterId; });
    if (!Purchased) return false;
    Purchased->Gold -= Price;
    if (bConsumable)
    {
        FRunConsumableStack* Stack = Purchased->Consumables.FindByPredicate([&Rules](const FRunConsumableStack& Candidate) { return Candidate.ItemTag == Rules.ConsumableTag && Candidate.Skill == Rules.HealingSkill; });
        if (!Stack) return false;
        ++Stack->Quantity;
    }
    else Purchased->CurrentHP = bRevival ? Profession.MaxHP * Rules.RevivalFraction : FMath::Min(Profession.MaxHP, Purchased->CurrentHP + Rules.RecoveryHP);
    ++Save->TargetRun.Recovery.Revision;
    if (!CommitSaveCandidate(Save.Get(), OutError)) return false;
    OutError = FText::GetEmpty();
    return true;
}
