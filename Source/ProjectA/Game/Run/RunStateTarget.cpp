#include "Game/Run/RunStateSubsystem.h"
#include "Game/Run/RunProgressRules.h"
#include "Game/Run/RunSaveGame.h"
#include "DataAsset/PartyDefinitionDataAsset.h"
#include "DataAsset/TargetRunDefinitionDataAsset.h"
#include "Unit/UnitDataRules.h"

bool URunStateSubsystem::ConfigureTargetRun(URunSaveGame* Save, FText& OutError) const
{
    const UTargetRunDefinitionDataAsset* Definition = PartyDefinition && PartyDefinition->TargetRunDefinition ? PartyDefinition->TargetRunDefinition.Get() : GetDefault<UTargetRunDefinitionDataAsset>();
    if (!Save || !Definition->BuildState(Save->TargetRun, OutError)) return false;
    Save->Nodes = RunProgressRules::GetTargetRoute().Nodes;
    Save->EncounterProgress = FRunEncounterProgress();
    Save->EncounterProgress.SchemaVersion = 2;
    Save->EncounterProgress.AfterCompletedNodeCount = 0;
    if (!UTargetRunDefinitionDataAsset::BuildOffers(Save->TargetRun, 0, 0, Save->EncounterProgress.Offers)) return false;
    Save->GoldRewardState = FRunGoldRewardState();
    Save->Phase = ERunPhase::EncounterChoice;
    const UPartyDefinitionDataAsset* Catalog = PartyDefinition ? PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
    for (FRunPartyMember& Member : Save->Party)
    {
        if (!Member.bCreated) continue;
        FProfessionDefinition Profession;
        if (!Catalog->ResolveProfession(Member.ClassId, Profession, OutError)) return false;
        Member.CurrentHP = Profession.MaxHP;
    }
    return ConfigureRecoveryRun(Save, OutError);
}

bool URunStateSubsystem::ResolveMemberProfession(const FRunPartyMember& Member, FProfessionDefinition& OutProfession, FText& OutError) const
{
    const UPartyDefinitionDataAsset* Catalog = PartyDefinition ? PartyDefinition.Get() : GetDefault<UPartyDefinitionDataAsset>();
    if (!Catalog->ResolveProfession(Member.ClassId, OutProfession, OutError)) return false;
    if (IsTargetRun()) UTargetRunDefinitionDataAsset::ApplyGrowth(TargetRun, CompletedNodes.Num(), OutProfession);
    if (!UnitDataRules::IsValidMaxHP(OutProfession.MaxHP) || !UnitDataRules::IsValidAttributes(OutProfession.Strength, OutProfession.Dexterity, OutProfession.Intelligence))
    {
        OutError = NSLOCTEXT("TargetRun", "GrowthLimits", "성장 적용 후 캐릭터 능력치가 허용 범위를 벗어났습니다.");
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}
