#include "Game/ProjectALanguageSubsystem.h"

#include "Internationalization/TextLocalizationManager.h"
#include "Misc/ConfigCacheIni.h"

void UProjectALanguageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if WITH_EDITOR
    // Preview only game text in PIE; standalone uses the engine's early language initialization.
    // PIE에서는 게임 문구만 미리보기하며 독립 실행은 엔진의 초기 언어 초기화를 사용합니다.
    if (GIsEditor && !IsRunningDedicatedServer())
    {
        FString Language;
        GConfig->GetString(TEXT("Internationalization"), TEXT("Language"), Language, GGameUserSettingsIni);
        FTextLocalizationManager::Get().EnableGameLocalizationPreview(Language == TEXT("en") ? TEXT("en") : TEXT("ko"));
    }
#endif
}
