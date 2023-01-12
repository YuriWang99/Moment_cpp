// Copyright Narrative Tools 2022. 

#include "QuestActionFactory.h"
#include "NarrativeTask.h"

#define LOCTEXT_NAMESPACE "QuestActionFactory"

UQuestActionFactory::UQuestActionFactory()
{
	SupportedClass = UNarrativeTask::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UQuestActionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UNarrativeTask>(InParent, Class, Name, Flags);
}

bool UQuestActionFactory::CanCreateNew() const
{
	return true;
}

FString UQuestActionFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewQuestTask"));
}

FText UQuestActionFactory::GetDisplayName() const
{
	return LOCTEXT("QuestText", "Task");
}

#undef LOCTEXT_NAMESPACE



