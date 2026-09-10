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

bool URunParticipationLibrary::ValidateTransition(const FRunParticipationData& Previous, const FRunIdentityData& PreviousIdentity, const FRunParticipationData& Next, const FRunIdentityData& NextIdentity, const TArray<FRunPartyMember>& Members, FText& OutError)
{
    if (!Validate(Previous, PreviousIdentity, Members, OutError) || !Validate(Next, NextIdentity, Members, OutError)) return false;
    OutError = NSLOCTEXT("RunParticipation", "Transition", "재개는 원래 소유권과 번호를 유지하고 기존 인간 참가자만 남길 수 있으며 다음 Host 세대를 사용해야 합니다.");
    if (PreviousIdentity.SchemaVersion != URunIdentityLibrary::CurrentSchemaVersion || PreviousIdentity.HostEpoch == MAX_int32 || NextIdentity.HostEpoch != PreviousIdentity.HostEpoch + 1) return false;
    FRunIdentityData Expected = PreviousIdentity;
    Expected.HostAccountId = NextIdentity.HostAccountId;
    Expected.HostEpoch = NextIdentity.HostEpoch;
    if (!FRunIdentityData::StaticStruct()->CompareScriptStruct(&Expected, &NextIdentity, 0)) return false;
    for (const FRunAccountId& Human : Next.HumanParticipants)
    {
        if (!Previous.HumanParticipants.Contains(Human))
        {
            OutError = NSLOCTEXT("RunParticipation", "PermanentAI", "이 Run에서 이미 AI로 전환된 참가자는 인간 조작으로 복귀할 수 없습니다.");
            return false;
        }
    }
    FRunAccountId Candidate;
    if (!URunIdentityLibrary::TrySelectHostCandidate(PreviousIdentity, Members, Next.HumanParticipants, Candidate, OutError)) return false;
    if (Candidate != NextIdentity.HostAccountId)
    {
        OutError = NSLOCTEXT("RunParticipation", "HostOrder", "재개할 인간 참가자 중 최초 합류 번호가 가장 낮은 참가자가 Host여야 합니다.");
        return false;
    }
    OutError = FText::GetEmpty();
    return true;
}
