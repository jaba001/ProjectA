#include "Game/Run/RunParticipationLibrary.h"
#include "Game/Run/RunIdentityLibrary.h"

bool URunParticipationLibrary::Validate(const FRunParticipationData& Participation, const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FText& OutError)
{
    if (!URunIdentityLibrary::ValidateIdentity(Identity, Members, OutError))
    {
        return false;
    }
    OutError = NSLOCTEXT("RunParticipation", "Invalid", "Run 참여 상태가 원래 참가자·Host와 일치하지 않습니다.");
    if (Identity.Origin == ERunIdentityOrigin::LegacyOffline || Participation.SchemaVersion != 1 || Participation.HumanParticipants.IsEmpty() || Participation.HumanParticipants.Num() > Identity.OriginalParticipants.Num() || !Participation.HumanParticipants.Contains(Identity.HostAccountId))
    {
        return false;
    }
    TArray<FRunAccountId> Seen;
    for (const FRunAccountId& Human : Participation.HumanParticipants)
    {
        if (Seen.Contains(Human) || !URunIdentityLibrary::IsOriginalParticipant(Identity, Human))
        {
            return false;
        }
        Seen.Add(Human);
    }
    OutError = FText::GetEmpty();
    return true;
}

bool URunParticipationLibrary::ResolveControlMode(const FRunParticipationData& Participation, const FRunIdentityData& Identity, const TArray<FRunPartyMember>& Members, FGuid CharacterId, EPartyControlMode& OutMode, FText& OutError)
{
    if (!Validate(Participation, Identity, Members, OutError))
    {
        return false;
    }
    const FRunPartyMember* Member = CharacterId.IsValid() ? Members.FindByPredicate([CharacterId](const FRunPartyMember& Candidate) { return Candidate.bCreated && Candidate.CharacterId == CharacterId; }) : nullptr;
    if (!Member)
    {
        OutError = NSLOCTEXT("RunParticipation", "Character", "원래 파티에 없는 캐릭터의 조작 상태를 조회할 수 없습니다.");
        return false;
    }
    OutMode = Participation.HumanParticipants.Contains(Member->OwnerAccountId) ? EPartyControlMode::Human : EPartyControlMode::ServerAI;
    return true;
}
