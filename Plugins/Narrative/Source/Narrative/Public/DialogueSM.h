// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeNodeBase.h"
#include <MovieSceneSequencePlayer.h>
#include "DialogueSM.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueNodeFinishedPlaying);

//TODO add these to dialoguenodes and select one at random
USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()

public:

	FDialogueLine()
	{
		Text = FText::GetEmpty();
		DialogueSound = nullptr;
		DialogueMontage = nullptr;
		Shot = nullptr;
		ShotSettings = FMovieSceneSequencePlaybackSettings();
	}

	/**
	The text for this dialogue node. Narrative will automatically display this on the NarrativeDefaultUI if you're using it, otherwise you can simply grab this 
	yourself if you're using your own dialogue. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details", meta = (MultiLine = true))
	FText Text;

	/**
	If a dialogue sound is selected, narrative will automatically play the sound for you in 3D space, either at the location of the NPC, or the player, whoever is talking. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	class USoundBase* DialogueSound;

	/**
	If a montage is selected, narrative will automatically play this montage on either the NPC or Player, whoever is saying the line.
	
	For multi-NPC dialogues, add a tag to each mesh with the speakers name as the tag. This way narrative knows what skeletal mesh to play the animation on. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	class UAnimMontage* DialogueMontage;

	/**
	If a shot is selected, narrative will automatically play this cinematic shot for you. 

	This will override a speakers selected shot. 
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	class ULevelSequence* Shot;

	//Playback settings for the shot
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	struct FMovieSceneSequencePlaybackSettings ShotSettings;

};

/**Base class for states and branches in the Dialogues state machine*/
 UCLASS(BlueprintType, Blueprintable)
 class NARRATIVE_API UDialogueNode : public UNarrativeNodeBase
 {

	 GENERATED_BODY()

 public:

	UDialogueNode();

	/**
	The text for this dialogue node. Narrative will automatically display this on the NarrativeDefaultUI if you're using it, otherwise you can simply grab this
	yourself if you're using your own dialogue.
	*/
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details", meta = (MultiLine = true))
     FText Text;


	 /**
	 If a dialogue sound is selected, narrative will automatically play the sound for you in 3D space, either at the location of the NPC, or the player, whoever is talking.
	 */
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	 class USoundBase* DialogueSound;

	 /**
	 If a montage is selected, narrative will automatically play this montage on either the NPC or Player, whoever is saying the line.

	 For multi-NPC dialogues, add a tag to each mesh with the speakers name as the tag. This way narrative knows what skeletal mesh to play the animation on.
	 */
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	 class UAnimMontage* DialogueMontage;

	 /**
	 If a shot is selected, narrative will automatically play this cinematic shot for you.

	 This will override a speakers selected shot.
	 */
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematics")
	 class ULevelSequence* Shot;

	 //Playback settings for the shot
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cinematics")
	 struct FMovieSceneSequencePlaybackSettings ShotSettings;

	 /** If alternative lines are added in here, narrative will randomly select either the main line or one of the alternatives.
	 
	 This can make dialogues more random and believable. 
	 */
	 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details", meta=(AdvancedDisplay))
	 TArray<FDialogueLine> AlternativeLines;

	 virtual FDialogueLine GetRandomLine() const;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueNodeFinishedPlaying OnDialogueFinished;

	//The last line the dialogue node played.
	UPROPERTY(BlueprintReadOnly, Category = "Details")
	FDialogueLine PlayedLine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	TArray<class UDialogueNode_NPC*> NPCReplies;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	TArray<class UDialogueNode_Player*> PlayerReplies;
	
	UPROPERTY()
	class UDialogue* OwningDialogue;

	UPROPERTY()
	class UNarrativeComponent* OwningComponent;

	TArray<class UDialogueNode_NPC*> GetNPCReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);
	TArray<class UDialogueNode_Player*> GetPlayerReplies(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);

	virtual UWorld* GetWorld() const;

	//The text this dialogue should display on its Graph Node
	const bool IsMissingCues() const;

private:

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	void EnsureUniqueID();

#endif WITH_EDITOR

 };

UCLASS(BlueprintType)
class NARRATIVE_API UDialogueNode_NPC : public UDialogueNode
{
	GENERATED_BODY()

public:

	/**The ID of the speaker for this node */
	UPROPERTY(BlueprintReadOnly, Category = "Details")
	FName SpeakerID;

	/**Grab this NPC node, appending all follow up responses to that node. Since multiple NPC replies can be linked together, 
	we need to grab the chain of replies the NPC has to say. */
	TArray<class UDialogueNode_NPC*> GetReplyChain(APlayerController* OwningController, APawn* OwningPawn, class UNarrativeComponent* NarrativeComponent);

	//Node is just used for routing and doesn't contain any dialogue 
	FORCEINLINE bool HasText() {return !Text.IsEmptyOrWhitespace(); }

};

UCLASS(BlueprintType)
class NARRATIVE_API UDialogueNode_Player : public UDialogueNode
{
	GENERATED_BODY()

public:

	//Have to pass dialogue in because OwningDialogue is null for some reason - TODO look into why this is
	UFUNCTION(BlueprintPure, Category = "Details")
	virtual FText GetOptionText(class UDialogue* InDialogue) const;

protected:

	/**The shortened text to display for dialogue option when it shows up in the list of available responses. If left empty narrative will just use the main text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details", meta = (DisplayAfter = "Text"))
	FText OptionText;
};