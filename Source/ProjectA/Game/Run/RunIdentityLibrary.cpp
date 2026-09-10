#include "Game/Run/RunIdentityLibrary.h"

namespace
{
    bool IsIdentifiedOrigin(ERunIdentityOrigin Origin)
    {
        return Origin == ERunIdentityOrigin::LocalDevelopment || Origin == ERunIdentityOrigin::AccountProvider;
    }

    bool IsValidAccountId(const FRunAccountId& AccountId, ERunIdentityOrigin Origin)
    {
        if (!IsIdentifiedOrigin(Origin) || AccountId.Provider.IsNone())
        {
            return false;
        }
        const FString Provider = AccountId.Provider.ToString();
        if (Provider.Len() < 1 || Provider.Len() > 32 || AccountId.Subject.Len() < 1 || AccountId.Subject.Len() > 256)
        {
            return false;
        }
        for (const TCHAR Character : Provider)
        {
            const bool bLetterOrDigit = (Character >= TEXT('A') && Character <= TEXT('Z')) || (Character >= TEXT('a') && Character <= TEXT('z')) || (Character >= TEXT('0') && Character <= TEXT('9'));
            if (!bLetterOrDigit && Character != TEXT('_'))
            {
                return false;
            }
        }
        for (const TCHAR Character : AccountId.Subject)
        {
            if (Character < 33 || Character > 126)
            {
                return false;
            }
        }
        const bool bDevelopmentProvider = AccountId.Provider == FName(TEXT("Development"));
        return Origin == ERunIdentityOrigin::LocalDevelopment ? bDevelopmentProvider : !bDevelopmentProvider;
    }

    bool IsValidConsent(const FRunParticipantData& Participant)
    {
        if (Participant.AIConsent == ERunAIConsent::Unknown)
        {
            return Participant.ConsentPolicyVersion == 0;
        }
        return (Participant.AIConsent == ERunAIConsent::Granted || Participant.AIConsent == ERunAIConsent::Declined) && Participant.ConsentPolicyVersion == 1;
    }

    bool ValidateIdentifiedRoster(const FRunIdentityData& Identity, TArray<FRunAccountId>& OutAccounts, FText& OutError)
    {
        if (Identity.SchemaVersion != 1 || !IsIdentifiedOrigin(Identity.Origin))
        {
            OutError = NSLOCTEXT("RunIdentity", "SchemaOrigin", "지원하지 않는 Run 식별 정보 버전 또는 출처입니다.");
            return false;
        }
        if (!Identity.RunId.IsValid() || Identity.OriginalParticipants.Num() < 1 || Identity.OriginalParticipants.Num() > 4 || Identity.HostEpoch < 1)
        {
            OutError = NSLOCTEXT("RunIdentity", "Header", "Run 식별자, 원래 참가자 수 또는 Host 세대가 올바르지 않습니다.");
            return false;
        }
        TArray<FRunAccountId> Accounts;
        for (const FRunParticipantData& Participant : Identity.OriginalParticipants)
        {
            if (!IsValidAccountId(Participant.AccountId, Identity.Origin) || Accounts.Contains(Participant.AccountId) || !IsValidConsent(Participant))
            {
                OutError = NSLOCTEXT("RunIdentity", "Participant", "원래 참가자 식별자 또는 AI 동의 기록이 올바르지 않거나 참가자가 중복됩니다.");
                return false;
            }
            Accounts.Add(Participant.AccountId);
        }
        if (!IsValidAccountId(Identity.HostAccountId, Identity.Origin) || !Accounts.Contains(Identity.HostAccountId))
        {
            OutError = NSLOCTEXT("RunIdentity", "Host", "Host는 원래 참가자 중 한 명이어야 합니다.");
            return false;
        }
        OutAccounts = MoveTemp(Accounts);
        OutError = FText::GetEmpty();
        return true;
    }
}

bool URunIdentityLibrary::ValidateIdentity(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (Identity.SchemaVersion != 1 || (Identity.Origin != ERunIdentityOrigin::LegacyOffline && !IsIdentifiedOrigin(Identity.Origin)))
    {
        OutError = NSLOCTEXT("RunIdentity", "SchemaOrigin", "지원하지 않는 Run 식별 정보 버전 또는 출처입니다.");
        return false;
    }
    TSet<int32> Slots;
    for (const FRunPartyMember& Member : Members)
    {
        if (Member.SlotIndex < 0 || Member.SlotIndex > 3 || Slots.Contains(Member.SlotIndex))
        {
            OutError = NSLOCTEXT("RunIdentity", "Slots", "파티 슬롯은 중복 없이 0~3번을 사용해야 합니다.");
            return false;
        }
        Slots.Add(Member.SlotIndex);
    }
    if (Identity.Origin == ERunIdentityOrigin::LegacyOffline)
    {
        if (Identity.RunId.IsValid() || !Identity.OriginalParticipants.IsEmpty() || !Identity.HostAccountId.IsEmpty() || Identity.HostEpoch != 0)
        {
            OutError = NSLOCTEXT("RunIdentity", "LegacyIdentity", "이전 오프라인 진행에는 추정한 Run 소유권이나 Host 정보를 추가할 수 없습니다.");
            return false;
        }
        for (const FRunPartyMember& Member : Members)
        {
            if (Member.CharacterId.IsValid() || !Member.OwnerAccountId.IsEmpty())
            {
                OutError = NSLOCTEXT("RunIdentity", "LegacyOwner", "이전 오프라인 캐릭터에는 추정한 식별자나 소유자를 추가할 수 없습니다.");
                return false;
            }
        }
        return true;
    }
    TArray<FRunAccountId> Accounts;
    if (!ValidateIdentifiedRoster(Identity, Accounts, OutError))
    {
        return false;
    }
    TSet<FGuid> CharacterIds;
    TArray<FRunAccountId> CharacterOwners;
    for (const FRunPartyMember& Member : Members)
    {
        if (!Member.bCreated)
        {
            if (Member.CharacterId.IsValid() || !Member.OwnerAccountId.IsEmpty())
            {
                OutError = NSLOCTEXT("RunIdentity", "EmptyMember", "빈 파티 슬롯에는 캐릭터 식별자나 소유자가 없어야 합니다.");
                return false;
            }
            continue;
        }
        if (!Member.CharacterId.IsValid() || CharacterIds.Contains(Member.CharacterId) || !IsValidAccountId(Member.OwnerAccountId, Identity.Origin) || !Accounts.Contains(Member.OwnerAccountId))
        {
            OutError = NSLOCTEXT("RunIdentity", "CharacterOwner", "생성된 캐릭터에는 고유 식별자와 원래 참가자인 소유자가 필요합니다.");
            return false;
        }
        CharacterIds.Add(Member.CharacterId);
        CharacterOwners.AddUnique(Member.OwnerAccountId);
    }
    if (CharacterIds.Num() < 1 || CharacterIds.Num() > 4 || CharacterOwners.Num() != Accounts.Num())
    {
        OutError = NSLOCTEXT("RunIdentity", "OwnedCharacters", "생성된 캐릭터는 1~4명이며 각 원래 참가자는 최소 한 명을 소유해야 합니다.");
        return false;
    }
    return true;
}

bool URunIdentityLibrary::IsOriginalParticipant(const FRunIdentityData& Identity, const FRunAccountId& AccountId)
{
    TArray<FRunAccountId> Accounts;
    FText Error;
    return IsValidAccountId(AccountId, Identity.Origin) && ValidateIdentifiedRoster(Identity, Accounts, Error) && Accounts.Contains(AccountId);
}

bool URunIdentityLibrary::IsCharacterOwner(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FGuid CharacterId, const FRunAccountId& AccountId)
{
    FText Error;
    if (!CharacterId.IsValid() || !ValidateIdentity(Identity, Members, Error) || !IsOriginalParticipant(Identity, AccountId))
    {
        return false;
    }
    for (const FRunPartyMember& Member : Members)
    {
        if (Member.bCreated && Member.CharacterId == CharacterId && Member.OwnerAccountId == AccountId)
        {
            return true;
        }
    }
    return false;
}
