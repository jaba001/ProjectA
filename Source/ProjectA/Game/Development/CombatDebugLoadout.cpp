#include "Game/Development/CombatDebugLoadout.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Combat/CombatManager.h"
#include "Combat/Round/CombatRoundCoordinator.h"
#include "DataAsset/SkillDefinitionDataAsset.h"
#include "Engine/World.h"
#include "Game/GameModes/CombatDebugGameMode.h"
#include "Game/Run/RunEquipmentCatalog.h"
#include "Game/Run/RunEquipmentRules.h"
#include "Game/Run/RunItemShopCatalog.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "Unit/CharacterEquipmentComponent.h"
#include "Unit/UnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatDebugLoadout, Log, All);

UWorld* UCombatDebugLoadout::GetWorld() const
{
    return Manager.IsValid() ? Manager->GetWorld() : nullptr;
}

void UCombatDebugLoadout::Initialize(ACombatManager* InManager)
{
    if (!IsValid(InManager) || !InManager->HasAuthority() || !ACombatDebugGameMode::IsDebugWorld(InManager->GetWorld()))
    {
        Manager.Reset();
        InitializedCombatId.Invalidate();
        SkillAssets.Reset();
        SkillLabels.Reset();
        SkillTags.Reset();
        EquipmentItems.Reset();
        EquipmentMembers.Reset();
        return;
    }
    ACombatRoundCoordinator* Coordinator = InManager->GetRoundCoordinator();
    const FGuid CombatId = IsValid(Coordinator) ? Coordinator->GetView().CombatId : FGuid();
    const bool bNewCombat = Manager.Get() != InManager || InitializedCombatId != CombatId;
    if (bNewCombat)
    {
        Manager = InManager;
        InitializedCombatId = CombatId;
        SkillAssets.Reset();
        SkillLabels.Reset();
        SkillTags.Reset();
        EquipmentItems.Reset();
        EquipmentMembers.Reset();

        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        const FString SkillDirectory(TEXT("/Game/User_JeHoon/Blueprint/DataAsset/Skills"));
        Registry.ScanPathsSynchronous({SkillDirectory});
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(*SkillDirectory));
        Filter.ClassPaths.Add(USkillDefinitionDataAsset::StaticClass()->GetClassPathName());
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
        TArray<FAssetData> Assets;
        Registry.GetAssets(Filter, Assets);
        for (const FAssetData& Asset : Assets) SkillAssets.AddUnique(Asset.GetSoftObjectPath());
        SkillAssets.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right) { return Left.ToString() < Right.ToString(); });
        // Match the runtime SkillName first and cache labels once instead of loading assets during UI searches.
        // 실행 시 사용하는 SkillName을 우선하고 UI 검색 중 에셋을 로드하지 않도록 표시명을 한 번 캐시합니다.
        for (const FSoftObjectPath& Asset : SkillAssets)
        {
            const USkillDefinitionDataAsset* Skill = Cast<USkillDefinitionDataAsset>(Asset.TryLoad());
            FText Label;
            if (Skill) Label = Skill->SkillName.IsEmpty() ? Skill->RoundDefinition.Name : Skill->SkillName;
            if (Label.IsEmpty()) Label = FText::FromString(Asset.GetAssetName());
            SkillLabels.Add(Asset, MoveTemp(Label));
            // Cache the resolved runtime tags without changing authored content or inferring elements from names.
            // 제작된 콘텐츠를 변경하거나 이름으로 속성을 추론하지 않고 해석된 실행 태그를 캐시합니다.
            FCombatRoundSkill Definition;
            FText Error;
            SkillTags.Add(Asset, Skill && Skill->ResolveRoundSkill(Definition, Error) ? MoveTemp(Definition.EffectTags) : FGameplayTagContainer());
        }

        TArray<FRunItemDefinition> Items;
        FText Error;
        if (!RunItemShopCatalog::Load(Items, Error))
        {
            UE_LOG(LogCombatDebugLoadout, Warning, TEXT("Debug equipment catalog could not be loaded. / 디버그 장비 목록을 불러오지 못했습니다: %s"), *Error.ToString());
        }
        else
        {
            const URunEquipmentCatalog& Catalog = URunEquipmentCatalog::Get();
            for (const FRunItemDefinition& Item : Items)
            {
                if (Catalog.ResolveProfile(Item)) EquipmentItems.Add(Item);
            }
            EquipmentItems.Sort([](const FRunItemDefinition& Left, const FRunItemDefinition& Right)
            {
                const int32 NameOrder = Left.DisplayName.ToString().Compare(Right.DisplayName.ToString());
                return NameOrder == 0 ? Left.Asset.ToString() < Right.Asset.ToString() : NameOrder < 0;
            });
        }
    }
    if (!IsValid(Coordinator) || !CombatId.IsValid()) return;
    APlayerController* Controller = GetWorld()->GetFirstPlayerController();
    for (const FCombatRoundUnitView& Entry : Coordinator->GetView().Units)
    {
        if (EquipmentMembers.Contains(Entry.UnitId)) continue;
        FText Error;
        if (!Coordinator->CanEditDebugUnit(Controller, Entry.UnitId, Error) || !IsValid(Entry.Unit) || !Entry.Unit->CharacterEquipment) continue;
        FRunPartyMember Member;
        Member.SlotIndex = Entry.UnitId;
        Member.CharacterId = InManager->GetCharacterId(Entry.Unit);
        if (!Member.CharacterId.IsValid()) Member.CharacterId = FGuid::NewGuid();
        Member.CharacterName = Entry.Unit->RuntimeCharacterName;
        Member.bCreated = true;
        Member.bPlayerControlled = true;
        Member.bHasSkillLoadout = true;
        Member.Equipment.bHasLoadout = true;
        // An explicit empty loadout hides legacy skill meshes while retaining their combat trace components.
        // 명시적인 빈 장비 상태는 전투 판정 컴포넌트를 유지하면서 기존 스킬 메시를 숨깁니다.
        if (Entry.Unit->CharacterEquipment->SetEquipment(true, {})) EquipmentMembers.Add(Entry.UnitId, MoveTemp(Member));
    }
}

FText UCombatDebugLoadout::GetSkillLabel(const FSoftObjectPath& Asset) const
{
    const FText* Label = SkillLabels.Find(Asset);
    return Label ? *Label : FText::FromString(Asset.GetAssetName());
}

const FGameplayTagContainer& UCombatDebugLoadout::GetSkillTags(const FSoftObjectPath& Asset) const
{
    const FGameplayTagContainer* Tags = SkillTags.Find(Asset);
    static const FGameplayTagContainer EmptyTags;
    return Tags ? *Tags : EmptyTags;
}

const FRunPartyMember* UCombatDebugLoadout::GetEquipmentMember(int32 UnitId) const
{
    const ACombatRoundCoordinator* Coordinator = Manager.IsValid() ? Manager->GetRoundCoordinator() : nullptr;
    return ACombatDebugGameMode::IsDebugWorld(GetWorld()) && IsValid(Coordinator) && Coordinator->GetView().CombatId == InitializedCombatId ? EquipmentMembers.Find(UnitId) : nullptr;
}

bool UCombatDebugLoadout::ResolveEditableUnit(APlayerController* Controller, int32 UnitId, ACombatRoundCoordinator*& OutCoordinator, AUnitBase*& OutUnit, FText& OutError) const
{
    OutCoordinator = nullptr;
    OutUnit = nullptr;
    OutError = NSLOCTEXT("CombatDebug", "EquipmentUnavailable", "디버그 전투의 계획 단계에서만 장비를 변경할 수 있습니다.");
    if (!Manager.IsValid() || !ACombatDebugGameMode::IsDebugWorld(GetWorld()) || !Manager->HasAuthority()) return false;
    ACombatRoundCoordinator* Coordinator = Manager->GetRoundCoordinator();
    if (!IsValid(Coordinator) || Coordinator->GetView().CombatId != InitializedCombatId || !Coordinator->CanEditDebugUnit(Controller, UnitId, OutError)) return false;
    const FCombatRoundUnitView* Entry = Coordinator->GetView().Units.FindByPredicate([UnitId](const FCombatRoundUnitView& Candidate) { return Candidate.UnitId == UnitId; });
    if (!Entry || !IsValid(Entry->Unit) || !Entry->Unit->CharacterEquipment || !EquipmentMembers.Contains(UnitId))
    {
        OutError = NSLOCTEXT("CombatDebug", "EquipmentUnitUnavailable", "현재 유닛의 디버그 장비 상태가 준비되지 않았습니다.");
        return false;
    }
    OutCoordinator = Coordinator;
    OutUnit = Entry->Unit;
    return true;
}

bool UCombatDebugLoadout::ApplyEquipment(APlayerController* Controller, int32 UnitId, FRunPartyMember Candidate, FText& OutError)
{
    ACombatRoundCoordinator* Coordinator = nullptr;
    AUnitBase* Unit = nullptr;
    if (!ResolveEditableUnit(Controller, UnitId, Coordinator, Unit, OutError)) return false;
    TArray<FRunEquipmentVisual> Visuals;
    if (!RunEquipmentRules::BuildVisuals(Candidate, Visuals, OutError)) return false;
    if (!Unit->CharacterEquipment->SetEquipment(true, Visuals))
    {
        OutError = NSLOCTEXT("CombatDebug", "EquipmentAttachmentFailed", "장비 메시를 현재 캐릭터에 부착할 수 없습니다. 기존 장비를 유지합니다.");
        return false;
    }
    // Publish the temporary inventory only after the complete visible loadout has been applied successfully.
    // 전체 표시 장비가 적용된 뒤에만 임시 보유 상태와 화면 변경을 공개합니다.
    EquipmentMembers.Add(UnitId, MoveTemp(Candidate));
    Coordinator->NotifyDebugEquipmentChanged(Controller, UnitId);
    OutError = FText::GetEmpty();
    return true;
}

bool UCombatDebugLoadout::GrantEquipment(APlayerController* Controller, int32 UnitId, const FSoftObjectPath& Asset, FGameplayTag Slot, FText& OutError)
{
    ACombatRoundCoordinator* Coordinator = nullptr;
    AUnitBase* Unit = nullptr;
    if (!ResolveEditableUnit(Controller, UnitId, Coordinator, Unit, OutError)) return false;
    const FRunItemDefinition* Item = EquipmentItems.FindByPredicate([&Asset](const FRunItemDefinition& Candidate) { return Candidate.Asset == Asset; });
    OutError = NSLOCTEXT("CombatDebug", "EquipmentNotInCatalog", "장착 가능한 디버그 장비 목록과 유효한 장비 슬롯에서 선택하세요.");
    if (!Item || !Slot.IsValid()) return false;
    FRunPartyMember Candidate = *EquipmentMembers.Find(UnitId);
    FRunEquipmentCommand Command;
    Command.CharacterId = Candidate.CharacterId;
    Command.ItemIndex = Candidate.Items.Add(*Item);
    Command.TargetSlot = Slot;
    Command.ExpectedRevision = Candidate.Equipment.Revision;
    if (!RunEquipmentRules::Apply(Candidate, Command, OutError)) return false;
    return ApplyEquipment(Controller, UnitId, MoveTemp(Candidate), OutError);
}

bool UCombatDebugLoadout::RemoveEquipment(APlayerController* Controller, int32 UnitId, int32 ItemIndex, FText& OutError)
{
    ACombatRoundCoordinator* Coordinator = nullptr;
    AUnitBase* Unit = nullptr;
    if (!ResolveEditableUnit(Controller, UnitId, Coordinator, Unit, OutError)) return false;
    FRunPartyMember Candidate = *EquipmentMembers.Find(UnitId);
    OutError = NSLOCTEXT("CombatDebug", "EquipmentMissingCopy", "현재 보유 목록에 있는 장비만 제거할 수 있습니다.");
    if (!Candidate.Items.IsValidIndex(ItemIndex) || Candidate.Equipment.Revision == MAX_int32) return false;
    Candidate.Equipment.Slots.RemoveAll([ItemIndex](const FRunEquipmentSlot& Slot) { return Slot.ItemIndex == ItemIndex; });
    // Compact only this transient inventory and repair every remaining copy index before validation.
    // 이 임시 보유 목록만 압축하고 검증 전에 남은 모든 사본 인덱스를 보정합니다.
    for (FRunEquipmentSlot& Slot : Candidate.Equipment.Slots)
    {
        if (Slot.ItemIndex > ItemIndex) --Slot.ItemIndex;
    }
    Candidate.Items.RemoveAt(ItemIndex);
    ++Candidate.Equipment.Revision;
    return ApplyEquipment(Controller, UnitId, MoveTemp(Candidate), OutError);
}
