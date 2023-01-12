// Copyright Narrative Tools 2022. 

#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeComponent.h"
#include "NarrativeCondition.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeDialogueSettings.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"

#define LOCTEXT_NAMESPACE "DialogueSM"

UDialogueNode::UDialogueNode()
{

}	

FDialogueLine UDialogueNode::GetRandomLine() const
{
	//Construct the line instead of adding it as a member as to not break dialogues made pre 2.2
	FDialogueLine DialogueLine;
	DialogueLine.DialogueMontage = DialogueMontage;
	DialogueLine.DialogueSound = DialogueSound;
	DialogueLine.Text = Text;
	DialogueLine.Shot = Shot;
	DialogueLine.ShotSettings = ShotSettings;

	if (!AlternativeLines.Num())
	{
		return DialogueLine;
	}
	else
	{
		TArray<FDialogueLine> AllLines = AlternativeLines;
		AllLines.Add(DialogueLine);
		return AllLines[FMath::RandRange(0, AllLines.Num() - 1)];
	}
}

TArray<class UDialogueNode_NPC*> UDialogueNode::GetNPCReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<class UDialogueNode_NPC*> ValidReplies;

	for (auto& NPCReply : NPCReplies)
	{
		if (NPCReply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
		{
			ValidReplies.Add(NPCReply);
		}
	}

	return ValidReplies;
}

TArray<class UDialogueNode_Player*> UDialogueNode::GetPlayerReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<class UDialogueNode_Player*> ValidReplies;

	for (auto& PlayerReply : PlayerReplies)
	{
		if(PlayerReply && PlayerReply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
		{
			ValidReplies.Add(PlayerReply);
		}
	}

	//Sort the replies by their Y position in the graph
	ValidReplies.Sort([](const UDialogueNode_Player& NodeA, const UDialogueNode_Player& NodeB) {
		return  NodeA.NodePos.Y < NodeB.NodePos.Y;
		});

	return ValidReplies;
}

UWorld* UDialogueNode::GetWorld() const
{
	return OwningComponent ? OwningComponent->GetWorld() : nullptr;
}

const bool UDialogueNode::IsMissingCues() const
{
	if (!Text.IsEmpty() && !DialogueSound)
	{
		return true;
	}

	if (!AlternativeLines.Num())
	{
		return false;
	}

	for (auto& Line : AlternativeLines)
	{
		if (Line.Text.IsEmptyOrWhitespace() && !Line.DialogueSound)
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR

void UDialogueNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		//If we changed the ID, make sure it doesn't conflict with any other IDs in the quest
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDialogueNode, ID))
		{
			EnsureUniqueID();
		}
	}
}

void UDialogueNode::EnsureUniqueID()
{
	if (OwningDialogue)
	{
		for (auto& NPCReply : OwningDialogue->NPCReplies)
		{
			//If the ID already exists, just append a 1 to the end 
			if (NPCReply && NPCReply != this && NPCReply->ID == ID)
			{
				ID = FName(ID.ToString() + '1');
				EnsureUniqueID(); // Need to call this again in case new ID is also a duplicate
				return;
			}
		}

		for (auto& PlayerReply : OwningDialogue->PlayerReplies)
		{
			//If the ID already exists, just append a 1 to the end 
			if (PlayerReply && PlayerReply != this && PlayerReply->ID == ID)
			{
				ID = FName(ID.ToString() + '1');
				EnsureUniqueID(); // Need to call this again in case new ID is also a duplicate
				return;
			}
		}
	}
}

#endif


TArray<class UDialogueNode_NPC*> UDialogueNode_NPC::GetReplyChain(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent)
{
	TArray<UDialogueNode_NPC*> NPCFollowUpReplies;
	UDialogueNode_NPC* CurrentNode = this;

	NPCFollowUpReplies.Add(CurrentNode);

	while (CurrentNode)
	{
		if (CurrentNode != this)
		{
			NPCFollowUpReplies.Add(CurrentNode);
		}

		TArray<UDialogueNode_NPC*> NPCRepliesToRet = CurrentNode->NPCReplies;

		//Need to process the conditions using higher nodes first 
		NPCRepliesToRet.Sort([](const UDialogueNode_NPC& NodeA, const UDialogueNode_NPC& NodeB) {
			return  NodeA.NodePos.Y < NodeB.NodePos.Y;
			});

		//If we don't find another node after this the loop will exit
		CurrentNode = nullptr;

		//Find the next valid reply. We'll then repeat this cycle until we run out
		for (auto& Reply : NPCRepliesToRet)
		{
			if (Reply != this && Reply->AreConditionsMet(OwningPawn, OwningController, NarrativeComponent))
			{
				CurrentNode = Reply;
				break; // just use the first reply with valid conditions
			}
		}
	}

	return NPCFollowUpReplies;
}

FText UDialogueNode_Player::GetOptionText(class UDialogue* InDialogue) const
{
	FText TextToUse = OptionText.IsEmptyOrWhitespace() ? Text : OptionText;

	if (InDialogue)
	{
		InDialogue->ReplaceStringVariables(TextToUse);
	}

	return TextToUse;
}

#undef LOCTEXT_NAMESPACE