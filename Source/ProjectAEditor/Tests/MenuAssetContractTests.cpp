#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuAssetContractTest, "ProjectA.Menu.AssetContracts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMenuAssetContractTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Name : { TEXT("MainMenuRootWidget"), TEXT("MainMenuScreenWidget"), TEXT("CharacterCreationWidget") })
    {
        FString Text;
        const FString SpecPath = FPaths::ProjectDir() / TEXT("Source/ProjectAEditor/UiScaffoldSpecs") / (FString(Name) + TEXT(".json"));
        TSharedPtr<FJsonObject> Spec;
        if (!TestTrue(TEXT("Spec can be read"), FFileHelper::LoadFileToString(Text, *SpecPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Spec)))
        {
            return false;
        }
        for (const FString& Directory : { Spec->GetStringField(TEXT("assetPath")), FString(TEXT("/Game/User_JeHoon/Validation/T12")) })
        {
            if (Directory == TEXT("/Game/User_JeHoon/Validation/T12") && !FParse::Param(FCommandLine::Get(), TEXT("T12GeneratedAssets")))
            {
                continue;
            }
            const FString AssetPath = Directory / Spec->GetStringField(TEXT("blueprintName"));
            UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
            if (!TestNotNull(*AssetPath, Blueprint))
            {
                return false;
            }
            TestTrue(TEXT("Saved Blueprint has compiled successfully"), Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr);
            for (const TSharedPtr<FJsonValue>& Value : Spec->GetArrayField(TEXT("widgets")))
            {
                const TSharedPtr<FJsonObject> Widget = Value->AsObject();
                bool bBind = false;
                if (!Widget->TryGetBoolField(TEXT("bind"), bBind) || !bBind)
                {
                    continue;
                }
                const FString WidgetName = Widget->GetStringField(TEXT("name"));
                UWidget* Actual = Blueprint->WidgetTree->FindWidget(FName(*WidgetName));
                if (TestNotNull(*(AssetPath + TEXT(" binding ") + WidgetName), Actual))
                {
                    TestEqual(*(WidgetName + TEXT(" widget type")), Actual->GetClass()->GetName(), Widget->GetStringField(TEXT("type")));
                    if (Directory == TEXT("/Game/User_JeHoon/Validation/T12") && WidgetName == TEXT("Text_Title"))
                    {
                        TestEqual(TEXT("AddMissing preserves existing Designer text."), Cast<UTextBlock>(Actual)->GetText().ToString(), Widget->GetStringField(TEXT("text")));
                    }
                }
            }
            if (Directory == TEXT("/Game/User_JeHoon/Validation/T12") && FString(Name) == TEXT("MainMenuScreenWidget"))
            {
                TestNotNull(TEXT("AddMissing saves the new widget."), Blueprint->WidgetTree->FindWidget(TEXT("T12_MissingLabel")));
            }
        }
    }
    return true;
}

#endif
