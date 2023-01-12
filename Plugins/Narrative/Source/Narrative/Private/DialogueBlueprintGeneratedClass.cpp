// Copyright Narrative Tools 2022. 

#include "DialogueBlueprintGeneratedClass.h"
#include "Dialogue.h"
#include <DialogueSM.h>

void UDialogueBlueprintGeneratedClass::InitializeDialogue(class UDialogue* Dialogue)
{
	if (Dialogue)
	{
		//Do what UUserWidget uses to initialize Dialogue from bgclass 
		Dialogue->DuplicateAndInitializeFromDialogue(DialogueTemplate);
	}
}

void UDialogueBlueprintGeneratedClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);
}

void UDialogueBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	if (DialogueTemplate)
	{
		// We don't want any of these flags to carry over from the WidgetBlueprint
		DialogueTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}

void UDialogueBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);
}

void UDialogueBlueprintGeneratedClass::SetDialogueTemplate(UDialogue* InDialogueTemplate)
{
	DialogueTemplate = InDialogueTemplate;

	//These flags will be on the blueprints Dialogue template, need to clear them 
	if (DialogueTemplate)
	{
		DialogueTemplate->ClearFlags(RF_Public | RF_ArchetypeObject | RF_DefaultSubObject);
	}
}
