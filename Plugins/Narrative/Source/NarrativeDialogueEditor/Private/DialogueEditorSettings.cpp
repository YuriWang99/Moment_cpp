// Copyright Narrative Tools 2022. 


#include "DialogueEditorSettings.h"
#include "Dialogue.h"
#include "DialogueSM.h"

UDialogueEditorSettings::UDialogueEditorSettings()
{
	RootNodeColor = FLinearColor(0.1f, 0.1f, 0.1f);
	PlayerNodeColor = FLinearColor(0.65f, 0.28f, 0.f);
	NPCNodeColor = FLinearColor(0.2f, 0.2f, 0.2f);

	DefaultNPCDialogueClass = UDialogueNode_NPC::StaticClass();
	DefaultPlayerDialogueClass = UDialogueNode_Player::StaticClass();
	DefaultDialogueClass = UDialogue::StaticClass();

	bEnableWarnings = true;
	bWarnMissingSoundCues = true;
}
