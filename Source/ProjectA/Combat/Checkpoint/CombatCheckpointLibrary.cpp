#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/Round/CombatPlanValidator.h"
#include "Unit/UnitDataRules.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/AssetManager.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
#include "Unit/CharacterAppearanceComponent.h"
#include "DataAsset/CharacterAppearanceCatalog.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"

namespace
{
    bool IsAssetPath(const FSoftObjectPath& Path)
    {
        const FString Value = Path.ToString();
        return Path.IsValid() && Path.GetSubPathUtf8String().IsEmpty() && Value.Len() <= 512 && (Value.StartsWith(TEXT("/Game/")) || Value.StartsWith(TEXT("/Script/ProjectA.")));
    }

    bool IsReadableName(const FText& Text)
    {
        const FString Name = Text.ToString();
        if (Name.Len() > 128 || Name.TrimStartAndEnd().IsEmpty())
        {
            return false;
        }
        for (TCHAR Character : Name)
        {
            if (Character < 32 || Character == 127)
            {
                return false;
            }
        }
        return true;
    }
}

FName UCombatCheckpointLibrary::ResolveSavedSkillId(FName SkillId)
{
    if (SkillId.IsNone() || !UAssetManager::IsInitialized()) return SkillId;
    const FPrimaryAssetId Redirected = UAssetManager::Get().GetRedirectedPrimaryAssetId(FPrimaryAssetId(SkillId.ToString()));
    return Redirected.IsValid() ? FName(*Redirected.ToString()) : SkillId;
}

bool UCombatCheckpointLibrary::Validate(const FCombatCheckpointData& Checkpoint, const TArray<FRunPartyMember>& Party, FText& OutError)
{
    OutError = NSLOCTEXT("CombatCheckpoint", "Invalid", "전투 체크포인트가 손상되었거나 현재 콘텐츠와 호환되지 않습니다.");
    const bool bRound = Checkpoint.SchemaVersion == CurrentSchemaVersion;
    const bool bLegacyOffline = Checkpoint.Identity.Origin == ERunIdentityOrigin::LegacyOffline;
    if (Checkpoint.SchemaVersion < 1 || Checkpoint.SchemaVersion > CurrentSchemaVersion || Checkpoint.ContentVersion != CurrentContentVersion || !Checkpoint.AttemptId.IsValid() || Checkpoint.Revision < 1 || Checkpoint.Revision == MAX_int64 || (bLegacyOffline && !bRound) || Checkpoint.NodeId.IsNone() || Checkpoint.EncounterId.IsNone() || Checkpoint.CompletedTurnSerial < 0 || Checkpoint.CompletedTurnSerial == MAX_int32 || Checkpoint.Units.Num() < 2 || Checkpoint.Units.Num() > 8 || (!bRound && !Checkpoint.Units.IsValidIndex(Checkpoint.NextTurnIndex)))
    {
        return false;
    }
    if (bRound ? (Checkpoint.RoundNumber < 1 || Checkpoint.RoundNumber == MAX_int32 || Checkpoint.PlanRevision < 1 || Checkpoint.PlanRevision == MAX_int32 || Checkpoint.RoundPlans.Num() != Checkpoint.Units.Num()) : (Checkpoint.RoundNumber != 0 || Checkpoint.PlanRevision != 0 || !Checkpoint.RoundPlans.IsEmpty())) return false;
    if (!URunIdentityLibrary::ValidateIdentity(Checkpoint.Identity, Party, OutError))
    {
        return false;
    }
    OutError = NSLOCTEXT("CombatCheckpoint", "Units", "전투 체크포인트의 유닛·소유권·스탯·장착·점유 정보가 올바르지 않습니다.");
    TSet<FGuid> UnitIds;
    TSet<int32> RoundUnitIds;
    TSet<int32> PartySlots;
    TSet<FIntPoint> OccupiedCoords;
    int32 LivingPlayers = 0;
    int32 LivingEnemies = 0;
    int32 EnemyCount = 0;
    for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
    {
        if (!Unit.UnitId.IsValid() || UnitIds.Contains(Unit.UnitId) || (Unit.Team != ETeam::Player && Unit.Team != ETeam::Enemy) || !IsAssetPath(Unit.UnitClass) || !IsReadableName(Unit.CharacterName))
        {
            return false;
        }
        if ((Unit.PartyControlMode != EPartyControlMode::Human && Unit.PartyControlMode != EPartyControlMode::ServerAI) || ((Checkpoint.SchemaVersion == 1 || Unit.Team == ETeam::Enemy) && Unit.PartyControlMode != EPartyControlMode::Human))
        {
            return false;
        }
        UnitIds.Add(Unit.UnitId);
        if (bRound && (Unit.RoundUnitId < 1 || Unit.RoundUnitId > 8 || RoundUnitIds.Contains(Unit.RoundUnitId))) return false;
        RoundUnitIds.Add(Unit.RoundUnitId);
        UClass* UnitClass = Cast<UClass>(Unit.UnitClass.TryLoad());
        UClass* ExpectedBase = Unit.Team == ETeam::Player ? APlayerUnit::StaticClass() : AEnemyUnit::StaticClass();
        if (!UnitClass || !UnitClass->IsChildOf(ExpectedBase) || UnitClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            return false;
        }
        const AUnitBase* Defaults = UnitClass->GetDefaultObject<AUnitBase>();
        const UCharacterAppearanceCatalog* AppearanceCatalog = Defaults->CharacterAppearance ? Defaults->CharacterAppearance->AppearanceCatalog.Get() : nullptr;
        if (AppearanceCatalog)
        {
            FText AppearanceError;
            if (!AppearanceCatalog->ValidateSelection(Unit.Appearance, AppearanceError))
            {
                OutError = AppearanceError;
                return false;
            }
        }
        else if (!Unit.Appearance.IsEmpty()) return false;
        if (!UnitDataRules::IsValidHealth(Unit.MaxHP, Unit.HP) || Unit.bDead != (Unit.HP == 0.0f))
        {
            return false;
        }
        if (!UnitDataRules::IsValidAttributes(Unit.Strength, Unit.Dexterity, Unit.Intelligence))
        {
            return false;
        }
        if (!UnitDataRules::IsValidActionPoints(Unit.MaxAP, Unit.MaxSubAP) || Unit.AP < 0 || Unit.AP > Unit.MaxAP || Unit.SubAP < 0 || Unit.SubAP > Unit.MaxSubAP || !UnitDataRules::IsValidMoveRange(Unit.MoveRange) || Unit.HealingItemCount < 0 || Unit.HealingItemCount > 1000 || !UnitDataRules::IsValidAttribute(Unit.HealingItemAmount))
        {
            return false;
        }
        if (Unit.Transform.ContainsNaN() || !Unit.Transform.GetRotation().IsNormalized() || Unit.Transform.GetLocation().GetAbsMax() > 10000000.0 || Unit.Transform.GetScale3D().GetMin() <= 0.0 || Unit.Transform.GetScale3D().GetAbsMax() > 100.0 || Unit.bHasTile == Unit.bDead)
        {
            return false;
        }
        if (Unit.bHasTile)
        {
            if (Unit.GridCoord.X < 0 || Unit.GridCoord.X > 3 || Unit.GridCoord.Y < 0 || Unit.GridCoord.Y > 3 || OccupiedCoords.Contains(Unit.GridCoord) || (Unit.Team == ETeam::Player ? Unit.GridCoord.Y >= 2 : Unit.GridCoord.Y < 2))
            {
                return false;
            }
            OccupiedCoords.Add(Unit.GridCoord);
        }
        if (Unit.Team == ETeam::Player)
        {
            const FRunPartyMember* Member = Party.FindByPredicate([&Unit](const FRunPartyMember& Candidate) { return Candidate.SlotIndex == Unit.PartySlot && Candidate.bCreated; });
            if (!Member || PartySlots.Contains(Unit.PartySlot) || Member->CharacterId != Unit.CharacterId || Member->OwnerAccountId != Unit.OwnerAccountId || (!bLegacyOffline && !Unit.CharacterId.IsValid()) || (bLegacyOffline && Unit.PartyControlMode != EPartyControlMode::Human))
            {
                return false;
            }
            PartySlots.Add(Unit.PartySlot);
            if (!FCharacterAppearanceSelection::StaticStruct()->CompareScriptStruct(&Member->Appearance, &Unit.Appearance, 0)) return false;
            LivingPlayers += !Unit.bDead ? 1 : 0;
        }
        else
        {
            if (Unit.PartySlot != INDEX_NONE || Unit.CharacterId.IsValid() || !Unit.OwnerAccountId.IsEmpty() || ++EnemyCount > 4)
            {
                return false;
            }
            LivingEnemies += !Unit.bDead ? 1 : 0;
        }
        if (!UnitDataRules::IsValidSkillCount(Unit.Skills.Num(), !bRound) || (!bRound && !IsAssetPath(Unit.DefaultAttackAbility)))
        {
            return false;
        }
        UClass* DefaultAbility = bRound ? nullptr : Cast<UClass>(Unit.DefaultAttackAbility.TryLoad());
        TSet<FSoftObjectPath> SkillPaths;
        TSet<FPrimaryAssetId> SkillIds;
        TSet<UClass*> AbilityClasses;
        for (const FSoftObjectPath& Path : Unit.Skills)
        {
            if (!IsAssetPath(Path) || SkillPaths.Contains(Path))
            {
                return false;
            }
            USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Path.TryLoad());
            if (bRound)
            {
                FCombatRoundSkill RoundSkill;
                FText SkillError;
                if (!Skill || !Skill->ResolveRoundSkill(RoundSkill, SkillError) || SkillIds.Contains(Skill->GetPrimaryAssetId())) return false;
                SkillPaths.Add(Path);
                SkillIds.Add(Skill->GetPrimaryAssetId());
                continue;
            }
            if (!UCombatTargetingLibrary::IsSupportedSkillArea(Skill) || !Skill->GetPrimaryAssetId().IsValid() || !Skill->AbilityClass || Skill->AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || Skill->ActionPointCost <= 0 || SkillIds.Contains(Skill->GetPrimaryAssetId()) || AbilityClasses.Contains(Skill->AbilityClass))
            {
                return false;
            }
            const UGameplayAbility* Ability = Skill->AbilityClass->GetDefaultObject<UGameplayAbility>();
            const UGameplayEffect* Cost = Ability->GetCostGameplayEffect();
            if (Ability->GetCooldownGameplayEffect() || (Cost && Cost->DurationPolicy != EGameplayEffectDurationType::Instant))
            {
                return false;
            }
            SkillPaths.Add(Path);
            SkillIds.Add(Skill->GetPrimaryAssetId());
            AbilityClasses.Add(Skill->AbilityClass);
        }
        if (!bRound && (!DefaultAbility || !DefaultAbility->IsChildOf(UGameplayAbility::StaticClass()) || !AbilityClasses.Contains(DefaultAbility)))
        {
            return false;
        }
    }
    for (const FRunPartyMember& Member : Party)
    {
        if (Member.bCreated && Member.CurrentHP != 0.0f && !PartySlots.Contains(Member.SlotIndex))
        {
            return false;
        }
    }
    if (LivingPlayers == 0 || LivingEnemies == 0 || (!bRound && Checkpoint.Units[Checkpoint.NextTurnIndex].bDead))
    {
        return false;
    }
    if (bRound && !CombatPlanValidation::ValidateCheckpointPlans(Checkpoint, OutError)) return false;
    if (Checkpoint.bHasOpponentSnapshot)
    {
        if (!IsAssetPath(Checkpoint.OpponentCatalog))
        {
            return false;
        }
        const UOpponentSnapshotCatalogDataAsset* Catalog = Cast<UOpponentSnapshotCatalogDataAsset>(Checkpoint.OpponentCatalog.TryLoad());
        if (!Catalog || Checkpoint.OpponentSnapshot.Members.Num() != EnemyCount || !Catalog->ValidateForEncounter(Checkpoint.OpponentSnapshot, 4, OutError))
        {
            return false;
        }
        OutError = NSLOCTEXT("CombatCheckpoint", "OpponentBuild", "저장된 상대 유닛의 빌드가 고정된 Snapshot과 일치하지 않습니다.");
        int32 MemberIndex = 0;
        for (const FCombatCheckpointUnit& Unit : Checkpoint.Units)
        {
            if (Unit.Team != ETeam::Enemy)
            {
                continue;
            }
            // Current HP, AP and formation can change in combat; the original build stays fixed.
            // 현재 HP, AP와 위치는 전투 중 바뀔 수 있지만 원래 빌드는 고정됩니다.
            const FPartySnapshotMember& Member = Checkpoint.OpponentSnapshot.Members[MemberIndex++];
            if (!FCharacterAppearanceSelection::StaticStruct()->CompareScriptStruct(&Member.Appearance, &Unit.Appearance, 0)) return false;
            if (!Catalog->MatchesSavedUnitClass(Member, Unit.UnitClass) || Unit.MaxHP != Member.Stats.MaxHP || Unit.Strength != Member.Stats.Strength || Unit.Dexterity != Member.Stats.Dexterity || Unit.Intelligence != Member.Stats.Intelligence || Unit.MaxAP != Member.Stats.MaxActionPoints || Unit.MaxSubAP != Member.Stats.MaxSubActionPoints || Unit.MoveRange != Member.Stats.MoveRange || Unit.Skills.Num() != Member.SkillIds.Num())
            {
                return false;
            }
            for (int32 SkillIndex = 0; SkillIndex < Member.SkillIds.Num(); ++SkillIndex)
            {
                USkillDefinitionDataAsset* Skill = Catalog->Skills.FindRef(Member.SkillIds[SkillIndex]);
                if (Unit.Skills[SkillIndex].TryLoad() != Skill || (SkillIndex == 0 && Unit.DefaultAttackAbility != FSoftObjectPath(Skill->AbilityClass.Get())))
                {
                    return false;
                }
            }
        }
    }
    else
    {
        const FPartySnapshot EmptySnapshot;
        if (!Checkpoint.OpponentCatalog.IsNull() || !FPartySnapshot::StaticStruct()->CompareScriptStruct(&Checkpoint.OpponentSnapshot, &EmptySnapshot, 0))
        {
            return false;
        }
    }
    OutError = FText::GetEmpty();
    return true;
}
