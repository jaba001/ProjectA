// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateUiScaffoldCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
struct FUiScaffoldWidgetSpec
{
	FString Name;
	FString Type;
	FString Parent;
	FString Text;
	bool bBind = false;
};

struct FUiScaffoldSpec
{
	FString NativeClass;
	FString NativeHeaderPath;
	FString NativeSourcePath;
	FString BlueprintName;
	FString AssetPath;
	FString ParentClass;
	TArray<FUiScaffoldWidgetSpec> Widgets;
};

const FString DefaultSpecPath = TEXT("Source/ProjectAEditor/UiScaffoldSpecs/MainMenuScaffoldTest.json");

const TCHAR* BoolText(bool bValue)
{
	if (bValue)
	{
		return TEXT("true");
	}

	return TEXT("false");
}

FString MakeProjectFullPath(const FString& Path)
{
	if (FPaths::IsRelative(Path))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path);
	}

	return FPaths::ConvertRelativePathToFull(Path);
}

FString GetReflectedClassName(const FString& NativeClassName)
{
	if (NativeClassName.StartsWith(TEXT("U")) && NativeClassName.Len() > 1)
	{
		return NativeClassName.RightChop(1);
	}

	return NativeClassName;
}

bool TryReadRequiredString(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, FString& OutValue)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] JSON object is invalid while reading field '%s'."), *FieldName);
		return false;
	}

	if (!JsonObject->TryGetStringField(FieldName, OutValue) || OutValue.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Missing or empty required JSON field: %s"), *FieldName);
		return false;
	}

	return true;
}

bool IsSupportedWidgetType(const FString& Type)
{
	static const TSet<FString> SupportedTypes = {
		TEXT("CanvasPanel"),
		TEXT("Overlay"),
		TEXT("Button"),
		TEXT("TextBlock"),
		TEXT("VerticalBox"),
		TEXT("Border"),
		TEXT("EditableTextBox")
	};

	return SupportedTypes.Contains(Type);
}

FString GetWidgetCppType(const FString& Type)
{
	if (Type == TEXT("CanvasPanel"))
	{
		return TEXT("UCanvasPanel");
	}

	if (Type == TEXT("Overlay"))
	{
		return TEXT("UOverlay");
	}

	if (Type == TEXT("Button"))
	{
		return TEXT("UButton");
	}

	if (Type == TEXT("TextBlock"))
	{
		return TEXT("UTextBlock");
	}

	if (Type == TEXT("VerticalBox"))
	{
		return TEXT("UVerticalBox");
	}

	if (Type == TEXT("Border"))
	{
		return TEXT("UBorder");
	}

	if (Type == TEXT("EditableTextBox"))
	{
		return TEXT("UEditableTextBox");
	}

	return FString();
}

FString GetWidgetIncludePath(const FString& Type)
{
	if (Type == TEXT("CanvasPanel"))
	{
		return TEXT("Components/CanvasPanel.h");
	}

	if (Type == TEXT("Overlay"))
	{
		return TEXT("Components/Overlay.h");
	}

	if (Type == TEXT("Button"))
	{
		return TEXT("Components/Button.h");
	}

	if (Type == TEXT("TextBlock"))
	{
		return TEXT("Components/TextBlock.h");
	}

	if (Type == TEXT("VerticalBox"))
	{
		return TEXT("Components/VerticalBox.h");
	}

	if (Type == TEXT("Border"))
	{
		return TEXT("Components/Border.h");
	}

	if (Type == TEXT("EditableTextBox"))
	{
		return TEXT("Components/EditableTextBox.h");
	}

	return FString();
}

UClass* GetWidgetClass(const FString& Type)
{
	if (Type == TEXT("CanvasPanel"))
	{
		return UCanvasPanel::StaticClass();
	}

	if (Type == TEXT("Overlay"))
	{
		return UOverlay::StaticClass();
	}

	if (Type == TEXT("Button"))
	{
		return UButton::StaticClass();
	}

	if (Type == TEXT("TextBlock"))
	{
		return UTextBlock::StaticClass();
	}

	if (Type == TEXT("VerticalBox"))
	{
		return UVerticalBox::StaticClass();
	}

	if (Type == TEXT("Border"))
	{
		return UBorder::StaticClass();
	}

	if (Type == TEXT("EditableTextBox"))
	{
		return UEditableTextBox::StaticClass();
	}

	return nullptr;
}

bool CanParentHaveChildren(const FString& Type)
{
	return Type == TEXT("CanvasPanel") || Type == TEXT("Overlay") || Type == TEXT("VerticalBox") || Type == TEXT("Button") || Type == TEXT("Border");
}

bool IsSingleContentParent(const FString& Type)
{
	return Type == TEXT("Button") || Type == TEXT("Border");
}

bool ParseSpecFile(const FString& SpecFullPath, FUiScaffoldSpec& OutSpec)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *SpecFullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to read JSON spec: %s"), *SpecFullPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to parse JSON spec: %s"), *SpecFullPath);
		return false;
	}

	bool bValid = true;
	bValid &= TryReadRequiredString(RootObject, TEXT("nativeClass"), OutSpec.NativeClass);
	bValid &= TryReadRequiredString(RootObject, TEXT("nativeHeaderPath"), OutSpec.NativeHeaderPath);
	bValid &= TryReadRequiredString(RootObject, TEXT("nativeSourcePath"), OutSpec.NativeSourcePath);
	bValid &= TryReadRequiredString(RootObject, TEXT("blueprintName"), OutSpec.BlueprintName);
	bValid &= TryReadRequiredString(RootObject, TEXT("assetPath"), OutSpec.AssetPath);
	bValid &= TryReadRequiredString(RootObject, TEXT("parentClass"), OutSpec.ParentClass);

	const TArray<TSharedPtr<FJsonValue>>* WidgetsArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("widgets"), WidgetsArray) || !WidgetsArray || WidgetsArray->Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Missing or empty required JSON array: widgets"));
		bValid = false;
	}

	if (!bValid)
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& WidgetValue : *WidgetsArray)
	{
		TSharedPtr<FJsonObject> WidgetObject = WidgetValue->AsObject();
		if (!WidgetObject.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Every widgets entry must be a JSON object."));
			return false;
		}

		FUiScaffoldWidgetSpec WidgetSpec;
		bool bWidgetValid = true;
		bWidgetValid &= TryReadRequiredString(WidgetObject, TEXT("name"), WidgetSpec.Name);
		bWidgetValid &= TryReadRequiredString(WidgetObject, TEXT("type"), WidgetSpec.Type);
		WidgetObject->TryGetStringField(TEXT("parent"), WidgetSpec.Parent);
		WidgetObject->TryGetBoolField(TEXT("bind"), WidgetSpec.bBind);
		WidgetObject->TryGetStringField(TEXT("text"), WidgetSpec.Text);

		if (!bWidgetValid)
		{
			return false;
		}

		OutSpec.Widgets.Add(WidgetSpec);
	}

	return true;
}

bool ValidateSpec(const FUiScaffoldSpec& Spec)
{
	bool bValid = true;

	if (!Spec.NativeClass.StartsWith(TEXT("U")) || Spec.NativeClass.Len() <= 1)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] nativeClass must begin with U and contain a reflected class name: %s"), *Spec.NativeClass);
		bValid = false;
	}

	if (!Spec.AssetPath.StartsWith(TEXT("/Game/")))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] assetPath must start with /Game/: %s"), *Spec.AssetPath);
		bValid = false;
	}

	if (Spec.ParentClass != TEXT("UUserWidget"))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Unsupported parentClass for this step: %s"), *Spec.ParentClass);
		bValid = false;
	}

	TSet<FString> Names;
	TMap<FString, int32> IndexByName;
	int32 RootCount = 0;

	for (int32 Index = 0; Index < Spec.Widgets.Num(); ++Index)
	{
		const FUiScaffoldWidgetSpec& Widget = Spec.Widgets[Index];

		if (Names.Contains(Widget.Name))
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Duplicate widget name: %s"), *Widget.Name);
			bValid = false;
		}
		else
		{
			Names.Add(Widget.Name);
			IndexByName.Add(Widget.Name, Index);
		}

		if (!IsSupportedWidgetType(Widget.Type))
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Unsupported widget type for %s: %s"), *Widget.Name, *Widget.Type);
			bValid = false;
		}

		if (Widget.Parent.IsEmpty())
		{
			++RootCount;
		}
	}

	if (RootCount != 1)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Spec must contain exactly one root widget. RootCount=%d"), RootCount);
		bValid = false;
	}

	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		if (Widget.Parent.IsEmpty())
		{
			continue;
		}

		if (!IndexByName.Contains(Widget.Parent))
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Missing parent '%s' for widget '%s'."), *Widget.Parent, *Widget.Name);
			bValid = false;
		}
	}

	TMap<FString, int32> ChildCountByParent;
	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		if (!Widget.Parent.IsEmpty())
		{
			int32& ChildCount = ChildCountByParent.FindOrAdd(Widget.Parent);
			++ChildCount;
		}
	}

	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		const int32* ChildCount = ChildCountByParent.Find(Widget.Name);
		if (!ChildCount)
		{
			continue;
		}

		if (!CanParentHaveChildren(Widget.Type))
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Widget '%s' of type '%s' cannot have children."), *Widget.Name, *Widget.Type);
			bValid = false;
		}

		if (IsSingleContentParent(Widget.Type) && *ChildCount > 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Widget '%s' of type '%s' supports only one child but has %d."), *Widget.Name, *Widget.Type, *ChildCount);
			bValid = false;
		}
	}

	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		TSet<FString> VisitedNames;
		FString CurrentParent = Widget.Parent;

		while (!CurrentParent.IsEmpty())
		{
			if (CurrentParent == Widget.Name || VisitedNames.Contains(CurrentParent))
			{
				UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Circular parent reference detected at widget '%s'."), *Widget.Name);
				bValid = false;
				break;
			}

			VisitedNames.Add(CurrentParent);
			const int32* ParentIndex = IndexByName.Find(CurrentParent);
			if (!ParentIndex)
			{
				break;
			}

			CurrentParent = Spec.Widgets[*ParentIndex].Parent;
		}
	}

	return bValid;
}

FString ConvertHeaderPathToIncludePath(const FString& NativeHeaderPath)
{
	FString IncludePath = NativeHeaderPath;
	IncludePath.ReplaceInline(TEXT("\\"), TEXT("/"));

	const FString RuntimePrefix = TEXT("Source/ProjectA/");
	if (IncludePath.StartsWith(RuntimePrefix))
	{
		IncludePath.RightChopInline(RuntimePrefix.Len());
	}

	return IncludePath;
}

void AppendUniqueBoundTypes(const FUiScaffoldSpec& Spec, TArray<FString>& OutTypes)
{
	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		if (!Widget.bBind)
		{
			continue;
		}

		const FString CppType = GetWidgetCppType(Widget.Type);
		if (!CppType.IsEmpty())
		{
			OutTypes.AddUnique(CppType);
		}
	}
}

FString BuildNativeHeaderText(const FUiScaffoldSpec& Spec)
{
	TArray<FString> BoundTypes;
	AppendUniqueBoundTypes(Spec, BoundTypes);

	const FString GeneratedHeader = FPaths::GetBaseFilename(Spec.NativeHeaderPath) + TEXT(".generated.h");
	FString Text;
	Text += TEXT("// Copyright Epic Games, Inc. All Rights Reserved.\n\n");
	Text += TEXT("#pragma once\n\n");
	Text += TEXT("#include \"CoreMinimal.h\"\n");
	Text += TEXT("#include \"Blueprint/UserWidget.h\"\n");
	Text += FString::Printf(TEXT("#include \"%s\"\n\n"), *GeneratedHeader);

	for (const FString& BoundType : BoundTypes)
	{
		Text += FString::Printf(TEXT("class %s;\n"), *BoundType);
	}

	if (BoundTypes.Num() > 0)
	{
		Text += TEXT("\n");
	}

	Text += TEXT("UCLASS()\n");
	Text += FString::Printf(TEXT("class PROJECTA_API %s : public UUserWidget\n"), *Spec.NativeClass);
	Text += TEXT("{\n");
	Text += TEXT("\tGENERATED_BODY()\n\n");
	Text += TEXT("protected:\n");
	Text += TEXT("\tvirtual void NativeConstruct() override;\n");

	for (const FUiScaffoldWidgetSpec& Widget : Spec.Widgets)
	{
		if (!Widget.bBind)
		{
			continue;
		}

		Text += TEXT("\n");
		Text += TEXT("\tUPROPERTY(meta = (BindWidget))\n");
		Text += FString::Printf(TEXT("\tTObjectPtr<%s> %s;\n"), *GetWidgetCppType(Widget.Type), *Widget.Name);
	}

	Text += TEXT("};\n");
	return Text;
}

FString BuildNativeSourceText(const FUiScaffoldSpec& Spec)
{
	const FString IncludePath = ConvertHeaderPathToIncludePath(Spec.NativeHeaderPath);
	FString Text;
	Text += TEXT("// Copyright Epic Games, Inc. All Rights Reserved.\n\n");
	Text += FString::Printf(TEXT("#include \"%s\"\n\n"), *IncludePath);
	Text += FString::Printf(TEXT("void %s::NativeConstruct()\n"), *Spec.NativeClass);
	Text += TEXT("{\n");
	Text += TEXT("\tSuper::NativeConstruct();\n\n");
	Text += FString::Printf(TEXT("\tUE_LOG(LogTemp, Log, TEXT(\"[%s] Generated scaffold widget constructed.\"));\n"), *Spec.NativeClass);
	Text += TEXT("}\n");
	return Text;
}

bool WriteTextFileIfNeeded(const FString& RelativeOrFullPath, const FString& DesiredText, bool bOverwrite, bool bDryRun)
{
	const FString FullPath = MakeProjectFullPath(RelativeOrFullPath);
	FString ExistingText;
	const bool bExists = FPaths::FileExists(FullPath);

	if (bExists && FFileHelper::LoadFileToString(ExistingText, *FullPath) && ExistingText == DesiredText)
	{
		UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Source file is already up to date: %s"), *FullPath);
		return true;
	}

	if (bExists && !bOverwrite)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Source file exists with different content and -Overwrite was not provided: %s"), *FullPath);
		return false;
	}

	if (bDryRun)
	{
		UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun would write source file: %s Exists=%s"), *FullPath, BoolText(bExists));
		return true;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
	if (!FFileHelper::SaveStringToFile(DesiredText, *FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to write source file: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Source file written: %s"), *FullPath);
	return true;
}

bool GenerateNativeSourceFiles(const FUiScaffoldSpec& Spec, bool bOverwrite, bool bDryRun)
{
	const FString HeaderText = BuildNativeHeaderText(Spec);
	const FString SourceText = BuildNativeSourceText(Spec);

	if (!WriteTextFileIfNeeded(Spec.NativeHeaderPath, HeaderText, bOverwrite, bDryRun))
	{
		return false;
	}

	return WriteTextFileIfNeeded(Spec.NativeSourcePath, SourceText, bOverwrite, bDryRun);
}

UClass* FindGeneratedNativeClass(const FUiScaffoldSpec& Spec)
{
	const FString ReflectedClassName = GetReflectedClassName(Spec.NativeClass);
	UClass* FoundClass = FindFirstObject<UClass>(*ReflectedClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("GenerateUiScaffoldCommandlet native class lookup"));

	if (!FoundClass)
	{
		FoundClass = FindFirstObject<UClass>(*Spec.NativeClass, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("GenerateUiScaffoldCommandlet native class lookup"));
	}

	if (!FoundClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Native class is not loaded yet: %s. Regenerate project files and build the editor target, then run again."), *Spec.NativeClass);
		return nullptr;
	}

	if (!FoundClass->IsChildOf(UUserWidget::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Native class is not a UUserWidget subclass: %s"), *FoundClass->GetName());
		return nullptr;
	}

	return FoundClass;
}

FString GetObjectPath(const FUiScaffoldSpec& Spec)
{
	const FString PackageName = Spec.AssetPath / Spec.BlueprintName;
	return PackageName + TEXT(".") + Spec.BlueprintName;
}

bool DoesAssetExist(const FUiScaffoldSpec& Spec)
{
	const FString PackageName = Spec.AssetPath / Spec.BlueprintName;
	const FString ObjectPath = GetObjectPath(Spec);

	if (FindObject<UObject>(nullptr, *ObjectPath))
	{
		return true;
	}

	return FPackageName::DoesPackageExist(PackageName);
}

UWidgetBlueprint* LoadExistingWidgetBlueprint(const FUiScaffoldSpec& Spec)
{
	UObject* ExistingAsset = LoadObject<UObject>(nullptr, *GetObjectPath(Spec));
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(ExistingAsset);

	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Existing asset is not a Widget Blueprint: %s"), *GetObjectPath(Spec));
		return nullptr;
	}

	return WidgetBlueprint;
}

UWidgetBlueprint* CreateWidgetBlueprintAsset(const FUiScaffoldSpec& Spec, UClass* NativeClass)
{
	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	if (!Factory)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to allocate UWidgetBlueprintFactory."));
		return nullptr;
	}

	Factory->BlueprintType = BPTYPE_Normal;
	Factory->ParentClass = NativeClass;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* CreatedAsset = AssetToolsModule.Get().CreateAsset(Spec.BlueprintName, Spec.AssetPath, UWidgetBlueprint::StaticClass(), Factory, FName(TEXT("GenerateUiScaffoldCommandlet")));
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(CreatedAsset);

	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to create Widget Blueprint asset at %s."), *GetObjectPath(Spec));
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(WidgetBlueprint);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Widget Blueprint asset created: %s."), *GetObjectPath(Spec));
	return WidgetBlueprint;
}

UWidgetBlueprint* CreateOrLoadWidgetBlueprint(const FUiScaffoldSpec& Spec, UClass* NativeClass, bool bOverwrite)
{
	const bool bAssetExists = DoesAssetExist(Spec);
	if (bAssetExists && !bOverwrite)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GenerateUiScaffoldCommandlet] Target asset already exists and -Overwrite was not provided. Asset will not be modified: %s"), *GetObjectPath(Spec));
		return nullptr;
	}

	if (bAssetExists)
	{
		UWidgetBlueprint* ExistingBlueprint = LoadExistingWidgetBlueprint(Spec);
		if (ExistingBlueprint && ExistingBlueprint->ParentClass != NativeClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Existing Widget Blueprint parent differs from requested native class. Overwrite aborted: %s"), *GetObjectPath(Spec));
			return nullptr;
		}

		UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Existing Widget Blueprint loaded for overwrite: %s"), *GetObjectPath(Spec));
		return ExistingBlueprint;
	}

	return CreateWidgetBlueprintAsset(Spec, NativeClass);
}

void RegisterDesignerWidget(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, bool bBind)
{
	if (!WidgetBlueprint || !Widget)
	{
		return;
	}

	Widget->bIsVariable = bBind;
	WidgetBlueprint->OnVariableAdded(Widget->GetFName());
}

bool AddChildWidget(UWidget* Parent, UWidget* Child)
{
	if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(Parent))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(Child);
		if (CanvasSlot)
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}

		return true;
	}

	if (UOverlay* Overlay = Cast<UOverlay>(Parent))
	{
		UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Child);
		if (OverlaySlot)
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Center);
			OverlaySlot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	if (UVerticalBox* VerticalBox = Cast<UVerticalBox>(Parent))
	{
		UVerticalBoxSlot* VerticalBoxSlot = VerticalBox->AddChildToVerticalBox(Child);
		if (VerticalBoxSlot)
		{
			VerticalBoxSlot->SetHorizontalAlignment(HAlign_Center);
			VerticalBoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	if (UButton* Button = Cast<UButton>(Parent))
	{
		UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->SetContent(Child));
		if (ButtonSlot)
		{
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
			ButtonSlot->SetPadding(FMargin(24.0f, 12.0f));
		}

		return true;
	}

	if (UBorder* Border = Cast<UBorder>(Parent))
	{
		Border->SetContent(Child);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Unsupported parent widget instance while attaching '%s' under '%s'."), *Child->GetName(), *Parent->GetName());
	return false;
}

bool ApplyWidgetProperties(const FUiScaffoldWidgetSpec& WidgetSpec, UWidget* Widget)
{
	if (WidgetSpec.Type == TEXT("TextBlock"))
	{
		UTextBlock* TextBlock = Cast<UTextBlock>(Widget);
		if (!TextBlock)
		{
			return false;
		}

		if (!WidgetSpec.Text.IsEmpty())
		{
			TextBlock->SetText(FText::FromString(WidgetSpec.Text));
		}
	}

	return true;
}

bool PopulateDesignerTree(const FUiScaffoldSpec& Spec, UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Widget Blueprint or WidgetTree is missing."));
		return false;
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	WidgetBlueprint->Modify();
	WidgetTree->SetFlags(RF_Transactional);
	WidgetTree->Modify();
	WidgetBlueprint->WidgetVariableNameToGuidMap.Reset();

	if (WidgetTree->RootWidget)
	{
		WidgetTree->RemoveWidget(WidgetTree->RootWidget);
		WidgetTree->RootWidget = nullptr;
	}

	TMap<FString, UWidget*> CreatedWidgets;
	FString RootWidgetName;

	for (const FUiScaffoldWidgetSpec& WidgetSpec : Spec.Widgets)
	{
		UClass* WidgetClass = GetWidgetClass(WidgetSpec.Type);
		if (!WidgetClass)
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] No UClass mapping for widget type: %s"), *WidgetSpec.Type);
			return false;
		}

		UWidget* Widget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*WidgetSpec.Name));
		if (!Widget)
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to construct widget '%s' of type '%s'."), *WidgetSpec.Name, *WidgetSpec.Type);
			return false;
		}

		RegisterDesignerWidget(WidgetBlueprint, Widget, WidgetSpec.bBind);
		if (!ApplyWidgetProperties(WidgetSpec, Widget))
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to apply properties for widget '%s'."), *WidgetSpec.Name);
			return false;
		}

		CreatedWidgets.Add(WidgetSpec.Name, Widget);

		if (WidgetSpec.Parent.IsEmpty())
		{
			RootWidgetName = WidgetSpec.Name;
		}
	}

	UWidget** RootWidgetPtr = CreatedWidgets.Find(RootWidgetName);
	if (!RootWidgetPtr || !*RootWidgetPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Root widget was not created: %s"), *RootWidgetName);
		return false;
	}

	WidgetTree->RootWidget = *RootWidgetPtr;

	for (const FUiScaffoldWidgetSpec& WidgetSpec : Spec.Widgets)
	{
		if (WidgetSpec.Parent.IsEmpty())
		{
			continue;
		}

		UWidget** ParentPtr = CreatedWidgets.Find(WidgetSpec.Parent);
		UWidget** ChildPtr = CreatedWidgets.Find(WidgetSpec.Name);

		if (!ParentPtr || !ChildPtr || !*ParentPtr || !*ChildPtr)
		{
			UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Failed to resolve parent-child pair '%s' -> '%s'."), *WidgetSpec.Parent, *WidgetSpec.Name);
			return false;
		}

		if (!AddChildWidget(*ParentPtr, *ChildPtr))
		{
			return false;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Designer tree populated from JSON spec."));
	return true;
}

bool CompileGeneratedWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Cannot compile a null Widget Blueprint."));
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	const bool bCompiled = WidgetBlueprint->Status != BS_Error;
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Blueprint compile result: %s"), BoolText(bCompiled));
	return bCompiled;
}

bool SaveGeneratedWidgetBlueprint(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Cannot save a null Widget Blueprint."));
		return false;
	}

	UPackage* Package = WidgetBlueprint->GetOutermost();
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] Generated Widget Blueprint has no outer package."));
		return false;
	}

	Package->MarkPackageDirty();

	TArray<UPackage*> PackagesToSave;
	PackagesToSave.Add(Package);

	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Asset save result: %s"), BoolText(bSaved));
	return bSaved;
}

void LogDryRunSummary(const FUiScaffoldSpec& Spec, bool bOverwrite)
{
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun validated spec."));
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun nativeClass=%s parentClass=%s"), *Spec.NativeClass, *Spec.ParentClass);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun header=%s source=%s"), *Spec.NativeHeaderPath, *Spec.NativeSourcePath);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun blueprint=%s assetPath=%s overwrite=%s"), *Spec.BlueprintName, *Spec.AssetPath, BoolText(bOverwrite));
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] DryRun widgetCount=%d"), Spec.Widgets.Num());
}
}

int32 UGenerateUiScaffoldCommandlet::Main(const FString& Params)
{
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Started."));
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Params: %s"), *Params);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Project file path: %s"), *FPaths::GetProjectFilePath());

	FString SpecPath = DefaultSpecPath;
	FParse::Value(*Params, TEXT("Spec="), SpecPath);

	const bool bOverwrite = FParse::Param(*Params, TEXT("Overwrite"));
	const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));
	const FString SpecFullPath = MakeProjectFullPath(SpecPath);

	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Spec path: %s"), *SpecFullPath);
	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] Overwrite=%s DryRun=%s"), BoolText(bOverwrite), BoolText(bDryRun));

	FUiScaffoldSpec Spec;
	if (!ParseSpecFile(SpecFullPath, Spec))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] JSON spec parsing failed."));
		return 1;
	}

	if (!ValidateSpec(Spec))
	{
		UE_LOG(LogTemp, Error, TEXT("[GenerateUiScaffoldCommandlet] JSON spec validation failed."));
		return 1;
	}

	if (bDryRun)
	{
		LogDryRunSummary(Spec, bOverwrite);
		return 0;
	}

	if (!GenerateNativeSourceFiles(Spec, bOverwrite, bDryRun))
	{
		return 1;
	}

	UClass* NativeClass = FindGeneratedNativeClass(Spec);
	if (!NativeClass)
	{
		return 1;
	}

	const bool bAssetExistsBeforeRun = DoesAssetExist(Spec);
	UWidgetBlueprint* WidgetBlueprint = CreateOrLoadWidgetBlueprint(Spec, NativeClass, bOverwrite);
	if (!WidgetBlueprint)
	{
		if (bAssetExistsBeforeRun && !bOverwrite)
		{
			return 0;
		}

		return 1;
	}

	if (!PopulateDesignerTree(Spec, WidgetBlueprint))
	{
		return 1;
	}

	if (!CompileGeneratedWidgetBlueprint(WidgetBlueprint))
	{
		return 1;
	}

	if (!SaveGeneratedWidgetBlueprint(WidgetBlueprint))
	{
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[GenerateUiScaffoldCommandlet] UI scaffold generation completed."));
	return 0;
}
