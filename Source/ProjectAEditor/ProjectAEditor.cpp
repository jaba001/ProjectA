// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectAEditor.h"

IMPLEMENT_MODULE(FProjectAEditorModule, ProjectAEditor)

void FProjectAEditorModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("[ProjectAEditor] Editor module started."));
}

void FProjectAEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[ProjectAEditor] Editor module shut down."));
}
