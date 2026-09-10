#include "Game/Snapshot/PartySnapshotLibrary.h"
#include "Game/Snapshot/PartySnapshotSaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    bool IsAsciiLetterOrDigit(TCHAR Character)
    {
        return (Character >= TEXT('A') && Character <= TEXT('Z')) || (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('0') && Character <= TEXT('9'));
    }

    bool IsStableIdentifier(FName Identifier)
    {
        if (Identifier.IsNone())
        {
            return false;
        }

        const FString Value = Identifier.ToString();
        if (Value.Len() > 128)
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (!IsAsciiLetterOrDigit(Character) && Character != TEXT('_') && Character != TEXT('-') && Character != TEXT('.'))
            {
                return false;
            }
        }
        return true;
    }

    bool IsReadableName(const FString& Name)
    {
        if (Name.Len() > 64 || Name.TrimStartAndEnd().IsEmpty())
        {
            return false;
        }

        for (const TCHAR Character : Name)
        {
            if (Character < 32 || Character == 127)
            {
                return false;
            }
        }
        return true;
    }

    bool HasUniqueIdentifiers(const TArray<FName>& Identifiers)
    {
        TSet<FName> Seen;
        for (const FName Identifier : Identifiers)
        {
            if (!IsStableIdentifier(Identifier) || Seen.Contains(Identifier))
            {
                return false;
            }
            Seen.Add(Identifier);
        }
        return true;
    }
}

bool UPartySnapshotLibrary::ValidateSnapshot(const FPartySnapshot& Snapshot, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (Snapshot.SchemaVersion != 1)
    {
        OutError = NSLOCTEXT("PartySnapshot", "SchemaVersion", "지원하지 않는 Snapshot 저장 버전입니다. v1 데이터가 필요합니다.");
        return false;
    }
    if (Snapshot.ContentVersion < 1 || !IsStableIdentifier(Snapshot.SnapshotId) || Snapshot.Members.Num() < 1 || Snapshot.Members.Num() > 4)
    {
        OutError = NSLOCTEXT("PartySnapshot", "Header", "Snapshot 식별자, 콘텐츠 버전 또는 파티 인원수가 올바르지 않습니다.");
        return false;
    }

    TSet<FName> MemberIds;
    TSet<int32> FormationSlots;
    for (const FPartySnapshotMember& Member : Snapshot.Members)
    {
        if (!IsStableIdentifier(Member.MemberId) || !IsStableIdentifier(Member.ClassId) || !IsReadableName(Member.CharacterName) || MemberIds.Contains(Member.MemberId))
        {
            OutError = NSLOCTEXT("PartySnapshot", "Identity", "파티원의 식별자, 직업 또는 이름이 올바르지 않거나 중복됩니다.");
            return false;
        }
        if (Member.FormationSlot < 0 || Member.FormationSlot > 3 || FormationSlots.Contains(Member.FormationSlot))
        {
            OutError = NSLOCTEXT("PartySnapshot", "Formation", "파티 배치는 중복 없이 0~3번 슬롯을 사용해야 합니다.");
            return false;
        }

        const FPartySnapshotStats& Stats = Member.Stats;
        if (!FMath::IsFinite(Stats.MaxHP) || !FMath::IsFinite(Stats.CurrentHP) || Stats.MaxHP <= 0.0f || Stats.MaxHP > 1000000.0f || Stats.CurrentHP < 0.0f || Stats.CurrentHP > Stats.MaxHP)
        {
            OutError = NSLOCTEXT("PartySnapshot", "Health", "파티원의 최대 HP 또는 현재 HP가 허용 범위를 벗어났습니다.");
            return false;
        }
        if (Stats.MaxActionPoints < 1 || Stats.MaxActionPoints > 100 || Stats.MaxSubActionPoints < 0 || Stats.MaxSubActionPoints > 100 || Stats.MoveRange < 0 || Stats.MoveRange > 32)
        {
            OutError = NSLOCTEXT("PartySnapshot", "ActionStats", "파티원의 AP, SubAP 또는 이동 범위가 허용 범위를 벗어났습니다.");
            return false;
        }
        if (Member.SkillIds.Num() < 1 || Member.SkillIds.Num() > 5 || !HasUniqueIdentifiers(Member.SkillIds))
        {
            OutError = NSLOCTEXT("PartySnapshot", "Skills", "파티원은 중복 없는 스킬 식별자를 1~5개 가져야 합니다.");
            return false;
        }
        if (Member.EquipmentIds.Num() > 16 || !HasUniqueIdentifiers(Member.EquipmentIds) || (!Member.TacticsId.IsNone() && !IsStableIdentifier(Member.TacticsId)))
        {
            OutError = NSLOCTEXT("PartySnapshot", "EquipmentTactics", "파티원의 장비 또는 전술 식별자가 올바르지 않습니다.");
            return false;
        }

        MemberIds.Add(Member.MemberId);
        FormationSlots.Add(Member.FormationSlot);
    }
    return true;
}

FString UPartySnapshotLibrary::GetSaveSlotName(FName SlotId)
{
    if (SlotId.IsNone())
    {
        return FString();
    }

    const FString Value = SlotId.ToString();
    if (Value.Len() < 1 || Value.Len() > 64)
    {
        return FString();
    }
    for (const TCHAR Character : Value)
    {
        if (!IsAsciiLetterOrDigit(Character) && Character != TEXT('_'))
        {
            return FString();
        }
    }
    return TEXT("ProjectA_Opponent_") + Value;
}

bool UPartySnapshotLibrary::SaveSnapshot(FName SlotId, const FPartySnapshot& Snapshot, FText& OutError)
{
    OutError = FText::GetEmpty();
    const FString SlotName = GetSaveSlotName(SlotId);
    if (SlotName.IsEmpty())
    {
        OutError = NSLOCTEXT("PartySnapshot", "Slot", "Snapshot 슬롯은 영문, 숫자, 밑줄로 이루어진 1~64자 식별자가 필요합니다.");
        return false;
    }
    if (!ValidateSnapshot(Snapshot, OutError))
    {
        return false;
    }

    UPartySnapshotSaveGame* Save = Cast<UPartySnapshotSaveGame>(UGameplayStatics::CreateSaveGameObject(UPartySnapshotSaveGame::StaticClass()));
    if (Save)
    {
        Save->Snapshot = Snapshot;
        if (UGameplayStatics::SaveGameToSlot(Save, SlotName, 0))
        {
            return true;
        }
    }
    OutError = NSLOCTEXT("PartySnapshot", "Write", "상대 파티 Snapshot을 저장하지 못했습니다.");
    return false;
}

bool UPartySnapshotLibrary::LoadSnapshot(FName SlotId, FPartySnapshot& OutSnapshot, FText& OutError)
{
    OutError = FText::GetEmpty();
    const FString SlotName = GetSaveSlotName(SlotId);
    if (SlotName.IsEmpty())
    {
        OutError = NSLOCTEXT("PartySnapshot", "Slot", "Snapshot 슬롯은 영문, 숫자, 밑줄로 이루어진 1~64자 식별자가 필요합니다.");
        return false;
    }
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        OutError = NSLOCTEXT("PartySnapshot", "Missing", "저장된 상대 파티 Snapshot이 없습니다.");
        return false;
    }

    const UPartySnapshotSaveGame* Save = Cast<UPartySnapshotSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    if (!Save)
    {
        OutError = NSLOCTEXT("PartySnapshot", "Read", "상대 파티 Snapshot을 불러오지 못했거나 저장 형식이 올바르지 않습니다.");
        return false;
    }
    if (!ValidateSnapshot(Save->Snapshot, OutError))
    {
        return false;
    }

    // Rejected data must not partially replace the currently selected opponent.
    // 거절된 데이터가 현재 선택한 상대 정보를 부분적으로 덮어쓰지 않도록 합니다.
    OutSnapshot = Save->Snapshot;
    return true;
}
