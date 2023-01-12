// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LevelSequencePlayer.h"
#include "DialogueSM.h"
#include "MovieSceneSequencePlayer.h"
#include "MovieScene/Public/MovieSceneSequencePlaybackSettings.h"
#include "MovieScene/Public/MovieSceneSequenceTickInterval.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Dialogue.generated.h"

USTRUCT(BlueprintType)
struct FSpeakerInfo
{

	GENERATED_BODY()

	FSpeakerInfo()
	{
		SpeakerID = NAME_None;
		NodeColor = FLinearColor(0.2f, 0.2f, 0.2f);

		DefaultShot = nullptr;

		//DefaultShotSettings = FMovieSceneSequencePlaybackSettings();

		//FMovieSceneSequenceLoopCount InfiniteLooping;
		//InfiniteLooping.Value = -1;

		//DefaultShotSettings.bPauseAtEnd = true;
		//DefaultShotSettings.LoopCount = InfiniteLooping;
	}

	//The name of this speaker. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speaker Details")
	FName SpeakerID;

	//Default shot to play whilst this speaker is talking
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	class ULevelSequence* DefaultShot = nullptr;

	//Default playback settings for when this speakers shot is playing
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	//struct FMovieSceneSequencePlaybackSettings DefaultShotSettings = FMovieSceneSequencePlaybackSettings();

	//Custom node colour for this NPC in the graph
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	FLinearColor NodeColor;

};

//Represents an instance of a running dialogue. Created at runtime, but also used as a template, similar to UWidgetTrees in UWidgetBlueprints. 
UCLASS(Blueprintable, BlueprintType)
class NARRATIVE_API UDialogue : public UObject
{
	GENERATED_BODY()

public:

	UDialogue();

	virtual UWorld* GetWorld() const override;
	virtual bool Initialize(class UNarrativeComponent* InitializingComp, class AActor* NPC, FName StartFromID);
	virtual void Deinitialize();

	virtual void DuplicateAndInitializeFromDialogue(UDialogue* DialogueTemplate);

	//Dialogue assets/nodes etc have the same name on client and server, so can be referenced over the network 
	bool IsNameStableForNetworking() const override { return true; };
	bool IsSupportedForNetworking() const override { return true; };

	#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	#endif	

	FSpeakerInfo GetSpeaker(const FName& SpeakerID);

	//All the speakers in this dialogue 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue Details")
	TArray<FSpeakerInfo> Speakers;

	//Can this dialogue be exited by pressing escape?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue Details")
	bool bCanBeExited;

	//A sequence to play whilst the player is talking
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	class ULevelSequence* PlayerTalkingShot;

	//Playback settings for when the player is talking
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	struct FMovieSceneSequencePlaybackSettings PlayerTalkingSettings;

	//A sequence to play whilst the NPC is waiting for the player to select their reply 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	class ULevelSequence* SelectReplyShot;

	//Playback settings for when the NPC is Waiting for the player to select their reply 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sequences")
	struct FMovieSceneSequencePlaybackSettings SelectReplySettings;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class UNarrativeComponent* OwningComp;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class APawn* OwningPawn;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class APlayerController* OwningController;

	//The NPCActor that we're talking to. This can be null 
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class AActor* NPCActor;

	//The first thing the NPC says in the dialog
	UPROPERTY()
	class UDialogueNode_NPC* RootDialogue;

	//Holds all of the npc replies in the dialogue
	UPROPERTY()
	TArray<class UDialogueNode_NPC*> NPCReplies;

	//Holds all of the player replies in the dialogue
	UPROPERTY()
	TArray<class UDialogueNode_Player*> PlayerReplies;

	//Skips the current dialogue line 
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	virtual void SkipCurrentLine();

public: 

	FORCEINLINE bool IsPlaying() const {return CurrentNode != nullptr; }

	//Server has been given a dialogue option from the player
	void SelectDialogueOption(UDialogueNode_Player* PlayerNode);

	//Used to check at any time on client or server if we have a valid chunk, meaning we can call play() and begin the dialogue
	bool HasValidChunk() const;

	//After we select a dialogue option, this function generates the next chunk of dialogue,
	// a "chunk" being a chain of NPC replies, followed by the players available responses to that chain. 
	bool GenerateDialogueChunk(UDialogueNode_NPC* NPCNode);

	//Called by the client when they have received the next dialogue chunk from the server
	void ClientReceiveDialogueChunk(const TArray<FName>& NPCReplies, const TArray<FName>& PlayerReplies);
	
	//Plays the current chunk of dialogue, then broadcasts the players available reponses. 
	void Play();

	//Called once we've played through all the NPC dialogues and the players reponses have been sent to the UI 
	void NPCFinishedTalking();

	//The NPC replies in the current chunk 
	UPROPERTY()
	TArray<class UDialogueNode_NPC*> NPCReplyChain;

	//The player responses once NPC has finished talking the current chunk 
	UPROPERTY()
	TArray<class UDialogueNode_Player*> AvailableResponses;

	//We need to reference dialogue nodes by their IDs as dialogue objects cant be replicated over the network - helper functions to easily do this: 

	//Get a node via its ID
	UDialogueNode_NPC* GetNPCReplyByID(const FName& ID) const;
	UDialogueNode_Player* GetPlayerReplyByID(const FName& ID) const;

	//Get multiple nodes via their IDs
	TArray<UDialogueNode_NPC*> GetNPCRepliesByIDs(const TArray<FName>& IDs) const;
	TArray <UDialogueNode_Player*> GetPlayerRepliesByIDs(const TArray<FName>& IDs) const;

	//Convert an array of nodes into an array of IDs
	TArray<FName> MakeIDsFromNPCNodes(const TArray<UDialogueNode_NPC*> Nodes) const;
	TArray<FName> MakeIDsFromPlayerNodes(const TArray<UDialogueNode_Player*> Nodes) const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	TArray<UDialogueNode*> GetNodes() const;

	//Replace any {MyVar} style variables in a dialogue line with their value 
	virtual void ReplaceStringVariables(FText& Line);

public:

	/*
	* Tick function for dialogue
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue")
	void Tick(const float DeltaTime);
	virtual void Tick_Implementation(const float DeltaTime);

protected:

	/*
	* Play a dialogue animation. Override this if you want to change how narrative plays animations
	*
	* Default implementation just plays the supplied anim montage on the NPC actor you gave it.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue")
	void PlayDialogueAnimation(class UDialogueNode* Node, const FDialogueLine& Line);
	virtual void PlayDialogueAnimation_Implementation(class UDialogueNode* Node, const FDialogueLine& Line);

	/*
	* Play a dialogue sound. Override this if you want to change how narrative plays sounds.
	*
	* Default implementation just plays the sound at the location of the NPC, or in 2D if no NPC was supplied. 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Dialogue")
	void PlayDialogueSound(const FDialogueLine& Line);
	virtual void PlayDialogueSound_Implementation(const FDialogueLine& Line);

	/*
	* Allows you to override how an NPC dialogue node is played. Narrative plays some audio, a montage, and plays a cinematic shot by default,
	* however if you want to do even more you can override this function
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void PlayNPCDialogue(class UDialogueNode_NPC* NPCReply, const FDialogueLine& Line, const FSpeakerInfo& Speaker);
	virtual void PlayNPCDialogue_Implementation(class UDialogueNode_NPC* NPCReply, const FDialogueLine& Line, const FSpeakerInfo& Speaker);

	/*
	* Allows you to override how an Player dialogue node is played. Narrative plays some audio, a montage, and plays a cinematic shot by default,
	* however if you want modify this behavior or add extra behavior you can override this function
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void PlayPlayerDialogue(class UDialogueNode_Player* PlayerReply, const FDialogueLine& Line);
	virtual void PlayPlayerDialogue_Implementation(class UDialogueNode_Player* PlayerReply, const FDialogueLine& Line);

	/*Returns how long narrative should wait before moving onto the next line.
	*
	* By default, we wait the length of the dialogue audio, or if no audio is supplied
	* we wait at a rate of 25 letters per second to give the reader time to finish reading.
	*
	* However if you want to change how long narrative waits for a dialogue line you can override this function. 
	* 
	* This words per minute can be configured in the Narrative settings.
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	float GetLineDuration(class UDialogueNode* Node, const FDialogueLine& Line);
	virtual float GetLineDuration_Implementation(class UDialogueNode* Node, const FDialogueLine& Line);

	/**
	* Allows you to use variables in your dialogue, ie Hello {PlayerName}.
	*
	* Simply override this function, then check the value of variable name and return whatever value you like!
	*
	* ie if(VariableName == "PlayerName") {return OwningPawn->GetUsername();};
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	FString GetStringVariable(class UDialogueNode* Node, const FDialogueLine& Line, const FString& VariableName);
	virtual FString GetStringVariable_Implementation(class UDialogueNode* Node, const FDialogueLine& Line, const FString& VariableName);

	/**
	* Called when an NPC Dialogue line starts
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void OnNPCDialogueLineStarted(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);
	void OnNPCDialogueLineStarted_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);

	/**
	* Called when an NPC Dialogue line is finished
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void OnNPCDialogueLineFinished(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);
	void OnNPCDialogueLineFinished_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);

	/**
	* Called when a player dialogue line has started
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void OnPlayerDialogueLineStarted(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);
	virtual void OnPlayerDialogueLineStarted_Implementation(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);

	/**
	* Called when a player dialogue line has started
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Dialogue")
	void OnPlayerDialogueLineFinished(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);
	virtual void OnPlayerDialogueLineFinished_Implementation( class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);

	//Tell the dialogue sequence player to start or stop playing a dialogue shot.
	virtual void PlayDialogueShot(class ULevelSequence* Shot, const FMovieSceneSequencePlaybackSettings& Settings);
	virtual void StopDialogueSequence();


	UFUNCTION()
	virtual void PlayNextNPCReply();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	virtual void FinishNPCDialogue();

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	virtual void FinishPlayerDialogue();

	virtual void PlayNPCDialogueNode(class UDialogueNode_NPC* NPCReply);
	virtual void PlayPlayerDialogueNode(class UDialogueNode_Player* PlayerReply);

	//Tells the narrative component this dialogue is finished and clears the dialogue. 
	virtual void ExitDialogue();

	//The current node narrative is playing
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue State")
	class UDialogueNode* CurrentNode;

	//The current speaker that is talking
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue State")
	FSpeakerInfo CurrentSpeaker;

	//The current line that is being played 
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue State")
	FDialogueLine CurrentLine;

	//Sequence actor responsible for playing any cinematic shots during the dialogue
	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class ALevelSequenceActor* DialogueSequencePlayer;

	//Audio component responsible for playing any audio during the dialogue
	UPROPERTY()
	class UAudioComponent* DialogueAudio;

	UPROPERTY()
	FTimerHandle TimerHandle_NPCReplyFinished;

	UPROPERTY()
	FTimerHandle TimerHandle_PlayerReplyFinished;

	UFUNCTION(BlueprintImplementableEvent, Category = Dialogue)
	void OnBeginDialogue();

	UFUNCTION(BlueprintImplementableEvent, Category = Dialogue)
	void OnEndDialogue();

};