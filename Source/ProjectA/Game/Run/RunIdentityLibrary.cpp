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
        if ((Identity.SchemaVersion != 1 && Identity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion) || !IsIdentifiedOrigin(Identity.Origin))
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
        TSet<int32> JoinOrdinals;
        int32 HostOrdinal = 0;
        for (const FRunParticipantData& Participant : Identity.OriginalParticipants)
        {
            if (!IsValidAccountId(Participant.AccountId, Identity.Origin) || Accounts.Contains(Participant.AccountId) || !IsValidConsent(Participant))
            {
                OutError = NSLOCTEXT("RunIdentity", "Participant", "원래 참가자 식별자 또는 AI 동의 기록이 올바르지 않거나 참가자가 중복됩니다.");
                return false;
            }
            if (Identity.SchemaVersion == 1 ? Participant.JoinOrdinal != 0 : Participant.JoinOrdinal < 1 || Participant.JoinOrdinal > Identity.OriginalParticipants.Num() || JoinOrdinals.Contains(Participant.JoinOrdinal))
            {
                OutError = NSLOCTEXT("RunIdentity", "JoinOrdinal", "구버전은 참가 번호를 추정할 수 없으며 새 번호는 1부터 참가자 수까지 중복 없이 연속되어야 합니다.");
                return false;
            }
            JoinOrdinals.Add(Participant.JoinOrdinal);
            if (Participant.AccountId == Identity.HostAccountId)
            {
                HostOrdinal = Participant.JoinOrdinal;
            }
            Accounts.Add(Participant.AccountId);
        }
        if (!IsValidAccountId(Identity.HostAccountId, Identity.Origin) || !Accounts.Contains(Identity.HostAccountId))
        {
            OutError = NSLOCTEXT("RunIdentity", "Host", "Host는 원래 참가자 중 한 명이어야 합니다.");
            return false;
        }
        if (Identity.SchemaVersion == URunIdentityLibrary::CurrentSchemaVersion && Identity.HostEpoch == 1 && HostOrdinal != 1)
        {
            OutError = NSLOCTEXT("RunIdentity", "InitialHostOrdinal", "최초 Host는 원래 합류 번호 1번이어야 합니다.");
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
    if ((Identity.SchemaVersion != 1 && Identity.SchemaVersion != CurrentSchemaVersion) || (Identity.Origin != ERunIdentityOrigin::LegacyOffline && !IsIdentifiedOrigin(Identity.Origin)))
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
        if (Identity.SchemaVersion != 1 || Identity.RunId.IsValid() || !Identity.OriginalParticipants.IsEmpty() || !Identity.HostAccountId.IsEmpty() || Identity.HostEpoch != 0)
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

bool URunIdentityLibrary::TryGetJoinOrdinal(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const FRunAccountId& AccountId, int32& OutOrdinal, FText& OutError)
{
    if (!ValidateIdentity(Identity, Members, OutError)) return false;
    if (Identity.SchemaVersion != CurrentSchemaVersion)
    {
        OutError = NSLOCTEXT("RunIdentity", "UnknownJoinOrder", "이 저장에는 명시적인 최초 합류 번호가 없습니다.");
        return false;
    }
    const FRunParticipantData* Participant = Identity.OriginalParticipants.FindByPredicate([&AccountId](const FRunParticipantData& Entry) { return Entry.AccountId == AccountId; });
    if (!Participant)
    {
        OutError = NSLOCTEXT("RunIdentity", "JoinAccount", "참가 번호를 조회할 계정은 원래 참가자여야 합니다.");
        return false;
    }
    OutOrdinal = Participant->JoinOrdinal;
    OutError = FText::GetEmpty();
    return true;
}

bool URunIdentityLibrary::TrySelectHostCandidate(const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, const TArray<FRunAccountId>& HumanParticipants, FRunAccountId& OutAccountId, FText& OutError)
{
    if (!ValidateIdentity(Identity, Members, OutError)) return false;
    if (Identity.SchemaVersion != CurrentSchemaVersion)
    {
        OutError = NSLOCTEXT("RunIdentity", "UnknownJoinOrder", "이 저장에는 명시적인 최초 합류 번호가 없습니다.");
        return false;
    }
    if (HumanParticipants.IsEmpty() || HumanParticipants.Num() > Identity.OriginalParticipants.Num())
    {
        OutError = NSLOCTEXT("RunIdentity", "CandidateCount", "Host 후보 조회에는 원래 참가자 중 한 명 이상의 명시적인 인간 참가자가 필요합니다.");
        return false;
    }
    TArray<FRunAccountId> Seen;
    const FRunParticipantData* Candidate = nullptr;
    for (const FRunAccountId& AccountId : HumanParticipants)
    {
        const FRunParticipantData* Participant = Identity.OriginalParticipants.FindByPredicate([&AccountId](const FRunParticipantData& Entry) { return Entry.AccountId == AccountId; });
        if (!Participant || Seen.Contains(AccountId))
        {
            OutError = NSLOCTEXT("RunIdentity", "CandidateAccounts", "명시적인 인간 참가자는 중복되지 않는 원래 참가자여야 합니다.");
            return false;
        }
        Seen.Add(AccountId);
        if (!Candidate || Participant->JoinOrdinal < Candidate->JoinOrdinal)
        {
            Candidate = Participant;
        }
    }
    OutAccountId = Candidate->AccountId;
    OutError = FText::GetEmpty();
    return true;
}
