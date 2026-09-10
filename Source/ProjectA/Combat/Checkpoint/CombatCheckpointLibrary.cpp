#include "Combat/Checkpoint/CombatCheckpointLibrary.h"
#include "Combat/Library/CombatTargetingLibrary.h"
#include "DataAsset/OpponentSnapshotCatalogDataAsset.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Game/Run/RunIdentityLibrary.h"
#include "Unit/EnemyUnit.h"
#include "Unit/PlayerUnit.h"
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

bool UCombatCheckpointLibrary::Validate(const FCombatCheckpointData& Checkpoint, const TArray<FRunPartyMember>& Party, FText& OutError)
{
    OutError = NSLOCTEXT("CombatCheckpoint", "Invalid", "전투 체크포인트가 손상되었거나 현재 콘텐츠와 호환되지 않습니다.");
    if ((Checkpoint.SchemaVersion != 1 && Checkpoint.SchemaVersion != CurrentSchemaVersion) || Checkpoint.ContentVersion != CurrentContentVersion || !Checkpoint.AttemptId.IsValid() || Checkpoint.Revision < 1 || Checkpoint.Revision == MAX_int64 || Checkpoint.Identity.Origin == ERunIdentityOrigin::LegacyOffline || Checkpoint.NodeId.IsNone() || Checkpoint.EncounterId.IsNone() || Checkpoint.CompletedTurnSerial < 0 || Checkpoint.CompletedTurnSerial == MAX_int32 || Checkpoint.Units.Num() < 2 || Checkpoint.Units.Num() > 8 || !Checkpoint.Units.IsValidIndex(Checkpoint.NextTurnIndex))
    {
        return false;
    }
    if (!URunIdentityLibrary::ValidateIdentity(Checkpoint.Identity, Party, OutError))
    {
        return false;
    }
    OutError = NSLOCTEXT("CombatCheckpoint", "Units", "전투 체크포인트의 유닛·소유권·스탯·장착·점유 정보가 올바르지 않습니다.");
    TSet<FGuid> UnitIds;
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
        UClass* UnitClass = Cast<UClass>(Unit.UnitClass.TryLoad());
        UClass* ExpectedBase = Unit.Team == ETeam::Player ? APlayerUnit::StaticClass() : AEnemyUnit::StaticClass();
        if (!UnitClass || !UnitClass->IsChildOf(ExpectedBase) || UnitClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            return false;
        }
        if (!FMath::IsFinite(Unit.HP) || !FMath::IsFinite(Unit.MaxHP) || Unit.MaxHP <= 0.0f || Unit.MaxHP > 1000000.0f || Unit.HP < 0.0f || Unit.HP > Unit.MaxHP || Unit.bDead != (Unit.HP == 0.0f))
        {
            return false;
        }
        if (Unit.MaxAP < 1 || Unit.MaxAP > 100 || Unit.AP < 0 || Unit.AP > Unit.MaxAP || Unit.MaxSubAP < 0 || Unit.MaxSubAP > 100 || Unit.SubAP < 0 || Unit.SubAP > Unit.MaxSubAP || Unit.MoveRange < 0 || Unit.MoveRange > 32 || Unit.HealingItemCount < 0 || Unit.HealingItemCount > 1000 || !FMath::IsFinite(Unit.HealingItemAmount) || Unit.HealingItemAmount < 0.0f || Unit.HealingItemAmount > 1000000.0f)
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
            if (!Member || PartySlots.Contains(Unit.PartySlot) || Member->CharacterId != Unit.CharacterId || Member->OwnerAccountId != Unit.OwnerAccountId || !Unit.CharacterId.IsValid())
            {
                return false;
            }
            PartySlots.Add(Unit.PartySlot);
            if (Unit.PartyControlMode == EPartyControlMode::ServerAI)
            {
                const FRunParticipantData* Participant = Checkpoint.Identity.OriginalParticipants.FindByPredicate([&Unit](const FRunParticipantData& Candidate) { return Candidate.AccountId == Unit.OwnerAccountId; });
                if (!Participant || Participant->AIConsent != ERunAIConsent::Granted || Participant->ConsentPolicyVersion != 1)
                {
                    OutError = NSLOCTEXT("CombatCheckpoint", "AIConsent", "서버 AI 조작을 복원하려면 원래 소유자의 정책 버전 1 사전 동의가 필요합니다.");
                    return false;
                }
            }
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
        if (Unit.Skills.IsEmpty() || Unit.Skills.Num() > 5 || !IsAssetPath(Unit.DefaultAttackAbility))
        {
            return false;
        }
        UClass* DefaultAbility = Cast<UClass>(Unit.DefaultAttackAbility.TryLoad());
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
        if (!DefaultAbility || !DefaultAbility->IsChildOf(UGameplayAbility::StaticClass()) || !AbilityClasses.Contains(DefaultAbility))
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
    if (LivingPlayers == 0 || LivingEnemies == 0 || Checkpoint.Units[Checkpoint.NextTurnIndex].bDead)
    {
        return false;
    }
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
            if (Unit.UnitClass != FSoftObjectPath(Catalog->EnemyClasses.FindRef(Member.ClassId).Get()) || Unit.MaxHP != Member.Stats.MaxHP || Unit.MaxAP != Member.Stats.MaxActionPoints || Unit.MaxSubAP != Member.Stats.MaxSubActionPoints || Unit.MoveRange != Member.Stats.MoveRange || Unit.Skills.Num() != Member.SkillIds.Num())
            {
                return false;
            }
            for (int32 SkillIndex = 0; SkillIndex < Member.SkillIds.Num(); ++SkillIndex)
            {
                USkillDefinitionDataAsset* Skill = Catalog->Skills.FindRef(Member.SkillIds[SkillIndex]);
                if (Unit.Skills[SkillIndex] != FSoftObjectPath(Skill) || (SkillIndex == 0 && Unit.DefaultAttackAbility != FSoftObjectPath(Skill->AbilityClass.Get())))
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
