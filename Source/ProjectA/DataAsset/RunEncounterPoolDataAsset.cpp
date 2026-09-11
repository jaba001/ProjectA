#include "DataAsset/RunEncounterPoolDataAsset.h"

URunEncounterPoolDataAsset::URunEncounterPoolDataAsset()
{
    for (int32 Index = 1; Index <= 3; ++Index)
    {
        FRunEncounterOffer& Offer = FixedOffers.AddDefaulted_GetRef();
        Offer.EncounterId = FName(*FString::Printf(TEXT("Shop_%02d"), Index));
        Offer.DisplayName = FText::FromString(FString::Printf(TEXT("상점%d"), Index));
    }
}

bool URunEncounterPoolDataAsset::BuildFixedOffers(TArray<FRunEncounterOffer>& OutOffers, FText& OutError) const
{
    OutError = NSLOCTEXT("RunEncounter", "InvalidPool", "시험용 인카운터는 서로 다른 ID와 이름을 가진 상점 3개가 필요합니다.");
    if (FixedOffers.Num() != 3) return false;
    TSet<FName> Ids;
    for (const FRunEncounterOffer& Offer : FixedOffers)
    {
        if (Offer.EncounterId.IsNone() || Ids.Contains(Offer.EncounterId) || Offer.DisplayName.ToString().TrimStartAndEnd().IsEmpty() || Offer.Type != ERunEncounterType::Shop) return false;
        Ids.Add(Offer.EncounterId);
    }
    OutOffers = FixedOffers;
    OutError = FText::GetEmpty();
    return true;
}
