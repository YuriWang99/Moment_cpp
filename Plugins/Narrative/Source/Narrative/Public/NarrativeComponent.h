// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"

#include "Quest.h"

#include "UObject/TextProperty.h" //Fixes a build error complaining about incomplete type UTextProperty
#include "Components/ActorComponent.h"
#include "QuestSM.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeSaveGame.h"
#include "NarrativeComponent.generated.h"

class UDialogueBlueprint;

class UNarrativeTask;
class UQuestState;
class UQuestGraphNode;

USTRUCT(BlueprintType)
struct FDialogueInfo
{
	GENERATED_BODY()

	FDialogueInfo()
	{
		Dialogue = nullptr;
		NPC = nullptr;
	};

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class UDialogue* Dialogue = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	class AActor* NPC = nullptr;

};

UENUM()
enum class EUpdateType : uint8
{
	UT_CompleteTask,
	UT_BeginQuest,
	UT_ForgetQuest,
	UT_RestartQuest
};

/**
Represents a change in state to the narrative components state machine.

Beginning/Forgetting/Restarting quests and completing quest actions all need to be processed
in the same order on the client that the server processed them in to ensure server/client state machines sync.
So to ensure order we replicate a buffer of these Tasks back to the client which then processes them in order to ensure sync.
*/
USTRUCT()
struct FNarrativeUpdate
{

	GENERATED_BODY()

public:

	FNarrativeUpdate()
	{
		bAcked = false;
		UpdateType = EUpdateType::UT_BeginQuest;
		QuestClass = UQuest::StaticClass();
	}

	//What sort of update this is
	UPROPERTY()
	EUpdateType UpdateType;

	//The quest that was updated 
	UPROPERTY()
	TSubclassOf<class UQuest> QuestClass;

	//Optional payload with some string data about the update
	UPROPERTY()
	FString Payload;

	bool bAcked; //flag client uses to prevent processing update multiple times 
	float CreationTime; // Timestamp server created update at

	//Tell a client to complete an Task 
	static FNarrativeUpdate CompleteTask(const TSubclassOf<class UQuest>& QuestClass, const FString& RawTask)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_CompleteTask;
		Update.QuestClass = QuestClass;
		Update.Payload = RawTask;
		return Update;
	};

	//Tell a client to begin one of its quests
	static FNarrativeUpdate BeginQuest(const TSubclassOf<class UQuest>& QuestClass, const FName& StartFromID = NAME_None)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_BeginQuest;
		Update.QuestClass = QuestClass;
		Update.Payload = StartFromID.ToString();
		return Update;
	};

	//Tell a client to restart one of its quests 
	static FNarrativeUpdate RestartQuest(const TSubclassOf<class UQuest>& QuestClass, const FName& StartFromID = NAME_None)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_RestartQuest;
		Update.QuestClass = QuestClass;
		Update.Payload = StartFromID.ToString();
		return Update;
	};

	//Tell a client to forget one of its quests. 
	static FNarrativeUpdate ForgetQuest(const TSubclassOf<class UQuest>& QuestClass)
	{
		FNarrativeUpdate Update;
		Update.UpdateType = EUpdateType::UT_ForgetQuest;
		Update.QuestClass = QuestClass;
		return Update;
	};

};
DECLARE_LOG_CATEGORY_EXTERN(LogNarrative, Log, All);


//Narrative makes a point to expose everything via delegates so your game can update your UI, or do whatever it needs to do when an update happens. 

//General
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNarrativeTaskCompleted, const UNarrativeTask*, NarrativeTask, const FString&, Name);

//Quests
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestStepCompleted, const UQuest*, Quest, const class UQuestBranch*, Step);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestNewState, UQuest*, Quest, const UQuestState*, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnQuestTaskProgressMade, const UQuest*, Quest, const FQuestTask&, ProgressedTask, const class UQuestBranch*, Step, int32, CurrentProgress, int32, RequiredProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuestTaskCompleted, const UQuest*, Quest, const FQuestTask&, CompletedTask, const class UQuestBranch*, Step);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestSucceeded, const UQuest*, Quest, const FText&, QuestSucceededMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestFailed, const UQuest*, Quest, const FText&, QuestFailedMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestStarted, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestForgotten, const UQuest*, Quest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestRestarted, const UQuest*, Quest);

//Dialogue
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueBegan, class UDialogue*, Dialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueFinished, class UDialogue*, Dialogue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDialogueRepliesAvailable, class UDialogue*, Dialogue, const TArray<UDialogueNode_Player*>&, PlayerReplies);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNPCDialogueLineStarted, class UDialogue*, Dialogue, class UDialogueNode_NPC*, Node, const FDialogueLine&, DialogueLine, const FSpeakerInfo&, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FNPCDialogueLineFinished, class UDialogue*, Dialogue, class UDialogueNode_NPC*, Node, const FDialogueLine&, DialogueLine, const FSpeakerInfo&, Speaker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDialogueLineStarted, class UDialogue*, Dialogue, class UDialogueNode_Player*, Node, const FDialogueLine&, DialogueLine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPlayerDialogueLineFinished, class UDialogue*, Dialogue, class UDialogueNode_Player*, Node, const FDialogueLine&, DialogueLine);

//Save/Load functionality
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginSave, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveComplete, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginLoad, FString, SaveGameName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadComplete, FString, SaveGameName);

/**
Narrative Component acts as the connection to the Narrative system and allows you to start quests, complete Tasks, etc.
In order to use Narrative you must add a Narrative Component to your Pawn/Controller/PlayerState etc. Like you would an AbilitySystemComponent
*/
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Component", meta=(BlueprintSpawnableComponent) )
class NARRATIVE_API UNarrativeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	// Sets default values for this component's properties
	UNarrativeComponent();

	bool HasAuthority() const;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:

	/**Called when a narrative action is completed*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnNarrativeTaskCompleted OnNarrativeTaskCompleted;

	/**Called when a quest objective has been completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestStepCompleted OnQuestStepCompleted;

	/**Called when a quest objective is updated and we've received a new objective*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestNewState OnQuestNewState;

	/**Called when a quest task in a quest step has made progress. ie 6 out of 10 wolves killed*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestTaskProgressMade OnQuestTaskProgressMade;

	/**Called when a quest task in a step is completed*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestTaskCompleted OnQuestTaskCompleted;

	/**Called when a quest is completed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestSucceeded OnQuestSucceeded;

	/**Called when a quest is failed.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestFailed OnQuestFailed;

	/**Called when a quest is started.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestStarted OnQuestStarted;

	/**Called when a quest is forgotten.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestForgotten OnQuestForgotten;

	/**Called when a quest is restarted.*/
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FOnQuestRestarted OnQuestRestarted;

	/**Called when a save has begun*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnBeginSave OnBeginSave;

	/**Called when a save has completed*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnSaveComplete OnSaveComplete;

	/**Called when a load has begun*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnBeginLoad OnBeginLoad;

	/**Called when a load has completed*/
	UPROPERTY(BlueprintAssignable, Category = "Saving/Loading")
	FOnLoadComplete OnLoadComplete;

	/**Called when a dialogue starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FOnDialogueBegan OnDialogueBegan;

	/**Called when a dialogue has been finished for any reason*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FOnDialogueFinished OnDialogueFinished;

	/**Called when the NPC(s) have finished talking and the players replies are available*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FDialogueRepliesAvailable OnDialogueRepliesAvailable;

	/**Called when a dialogue line starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FNPCDialogueLineStarted OnNPCDialogueLineStarted;

	/**Called when a dialogue line finishes*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FNPCDialogueLineFinished OnNPCDialogueLineFinished;

	/**Called when a dialogue line starts*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FPlayerDialogueLineStarted OnPlayerDialogueLineStarted;

	/**Called when a dialogue line finishes*/
	UPROPERTY(BlueprintAssignable, Category = "Dialogues")
	FPlayerDialogueLineFinished OnPlayerDialogueLineFinished;

	//Server replicates these back to client so client can keep its state machine in sync with the servers
	UPROPERTY(ReplicatedUsing = OnRep_PendingUpdateList)
	TArray<FNarrativeUpdate> PendingUpdateList;

	//A list of all the quests the player is involved in
	UPROPERTY(VisibleAnywhere, Category = "Quests")
	TArray<class UQuest*> QuestList;

	//Current dialogue the player is in 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Quests")
	class UDialogue* CurrentDialogue;

	/*A map of every narrative task the player has ever completed, where the key is the amount of times the action has been completed
	a TMap means we can very efficiently track large numbers of actions, such as shooting where the player may shoot a gun thousands of times */
	UPROPERTY(EditAnywhere, Category = "Quests")
	TMap<FString, int32> MasterTaskList;

	/** Pointer to an optional shared narrative component. If set solo completed tasks get forwarded to this shared comp. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Narrative")
	class UNarrativeComponent* SharedNarrativeComp;

	//We set this flag to true during loading so we don't broadcast any quest update delegates as we load quests back in
	bool bIsLoading;

protected:

	//We cache the OwningController, we won't cache pawn as this might change
	UPROPERTY()
	class APlayerController* OwnerPC;

	void SendNarrativeUpdate(const FNarrativeUpdate& Update);

	UFUNCTION()
	void OnRep_PendingUpdateList();

	/**Used internally when a quest is started - Ensures that a quest asset doesn't have any errors,
	for example a quest that has no ending, the start node has no connection, no loose nodes, etc.
	@param OutError passed by ref, if an error is found OutError will contain the error message*/
	bool IsQuestValid(const UQuest* QuestAsset, FString& OutError);

	UFUNCTION()
	virtual void NarrativeTaskCompleted(const UNarrativeTask* NarrativeTask, const FString& Name);

	UFUNCTION()
	virtual void QuestStarted(const UQuest* Quest);

	UFUNCTION()
	virtual void QuestForgotten(const UQuest* Quest);

	UFUNCTION()
	virtual void QuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	UFUNCTION()
	virtual void QuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION()
	virtual void QuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION()
	virtual void QuestTaskProgressMade(const UQuest* Quest, const FQuestTask& ProgressedObjective, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION()
	virtual void QuestTaskCompleted(const UQuest* Quest, const FQuestTask& Task, const class UQuestBranch* Step);

	UFUNCTION()
	virtual void QuestStepCompleted(const UQuest* Quest, const class UQuestBranch* Step);

	UFUNCTION()
	virtual void BeginSave(FString SaveName);

	UFUNCTION()
	virtual void BeginLoad(FString SaveName);

	UFUNCTION()
	virtual void SaveComplete(FString SaveName);

	UFUNCTION()
	virtual void LoadComplete(FString SaveName);

	UFUNCTION()
	virtual void DialogueRepliesAvailable(class UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& PlayerReplies);

	UFUNCTION()
	virtual void DialogueLineStarted(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine);

	UFUNCTION()
	virtual void DialogueLineFinished(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine);

	UFUNCTION()
	virtual void DialogueBegan(class UDialogue* Dialogue);

	UFUNCTION()
	virtual void DialogueFinished(class UDialogue* Dialogue);



public:

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APawn* GetOwningPawn() const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APlayerController* GetOwningController() const;

public:

	//BP exposed functions for scripters/designers 

	/**
	Return true if a given quest is started or finished
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestStartedOrFinished(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest is in progress but false if the quest is finished
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestInProgress(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed and was succeeded
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestSucceeded(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed and was failed
	@param QuestAsset The quest to check
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestFailed(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Return true if a given quest has been completed, regardless of whether we failed or succeeded the quest
	@param QuestAsset The quest to use
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	virtual bool IsQuestFinished(TSubclassOf<class UQuest> QuestClass) const;

	/**
	Begin a given quest. Return quest if success. 
	
	@param QuestAsset The quest to use
	@param StartFromID If this is set to a valid ID in the quest, we'll skip to this quest state instead of playing the quest from the start

	@return The created UQuest instance
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests", meta = (AdvancedDisplay = "1"))
	virtual class UQuest* BeginQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID = NAME_None);


	/**
	Restart a given quest. Will only actually restart the quest if it has been started.
	If the quest hasn't started,  this will do nothing.

	@param QuestAsset The quest to use
	@param StartFromID If this is set to a valid ID in the quest, we'll skip to this quest state instead of playing the quest its default start state

	@return Whether or not the quest was restarted or not
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests", meta = (AdvancedDisplay = "1"))
	virtual bool RestartQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID = NAME_None);

	/**
	Forget a given quest. The quest will be removed from the players quest list, 
	and the quest can be started again later using BeginQuest() if desired.

	@param QuestAsset The quest to use
	@return Whether or not the quest was forgotten or not
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Quests")
	virtual bool ForgetQuest(TSubclassOf<class UQuest> QuestClass);

	/**
	* Only callable on the server. Server grabs root dialogue node, validates its conditions, and sends it to the client via ClientRecieveDialogueOptions
	*
	@param DialogueBlueprint The dialogue to begin 
	@param NPC the actor we're talking to 
	@param NPCName The NPCs name 

	@return Whether the dialogue was successfully started 
	*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues", BlueprintAuthorityOnly, meta=(DefaultToSelf="NPC", AdvancedDisplay = "2"))
	virtual bool BeginDialogue(TSubclassOf<class UDialogue> Dialogue, class AActor* NPC, FName StartFromID = NAME_None);

	/**Used by the server to tell client to start dialogue. Also sends the initial chunk*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientBeginDialogue(TSubclassOf<class UDialogue> Dialogue, class AActor* NPC, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**Used by the server to tell client to end dialogue*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientExitDialogue();

	/**Used by the server to send valid dialogue chunks to the client*/
	UFUNCTION(Client, Reliable, Category = "Dialogues")
	virtual void ClientRecieveDialogueChunk(const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs);

	/**Selects a dialogue option. Will update the dialogue and automatically start playing the next bit of dialogue*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues")
	virtual void SelectDialogueOption(class UDialogueNode_Player* Option);

	/**Tell the server we've selected a dialogue option*/
	UFUNCTION(Server, Reliable, Category = "Dialogues")
	virtual void ServerSelectDialogueOption(const FName& OptionID);

	/**
	* Exit the dialogue, but authoritatively check we're allowed to before doing so. 
	* 
	@return Whether the dialogue was successfully exited
	*/
	UFUNCTION(BlueprintCallable, Category = "Dialogues")
	virtual void TryExitDialogue();

	/**Exit the dialogue, will never fail*/
	virtual void ExitDialogue();

	UFUNCTION(Server, Reliable, Category = "Dialogues")
	virtual void ServerExitDialogue();

	/**Return true if we're in a dialogue 

	@return Whether true if we're in a dialogue, false otherwise 
	*/
	UFUNCTION(BlueprintPure, Category = "Dialogues")
	virtual bool IsInDialogue();

	/**Complete a narrative task.

	If the task updates a quest, the input will be replicated back to the client so it can 
	then update its own state machines and keep them in sync with the server. This means we can use very little network
	bandwidth as we're only sending state machine FName inputs through instead of trying to replicate the entire UQuest object.

	Not marked blueprint callable since CompleteNarrativeTask() is exposed via a custom K2Node.

	@param Task the task that the player has completed
	@param Argument the tasks argument, ie if the task is ObtainItem this would be the name of the item we obtained

	@return Whether completing the task updated any quests */
	virtual bool CompleteNarrativeTask(const UNarrativeTask* Task, const FString& Argument);
	virtual bool CompleteNarrativeTask(const FString& TaskName, const FString& Argument);

protected:
	
	/**
	Internal begin quest that isn't exposed to BP.
	*/
	virtual class UQuest* BeginQuest_Internal(TSubclassOf<class UQuest> QuestClass, FName StartFromID = NAME_None);

	/**Called after CompleteNarrativeTask() has converted the Task into a raw string version of our Task */
	virtual bool CompleteNarrativeTask_Internal(const FString& TaskString, const bool bFromReplication);

	/**Try and update one of our quests using an Task string. Return the progress we made on the quest */
	virtual EQuestProgress UpdateQuest(UQuest* QuestToUpdate, const FString& TaskString = "");

	/**
	Create a dialogue object from the supplied dialogue class and params
	*/
	virtual class UDialogue* MakeDialogue(TSubclassOf<class UDialogue> DialogueClass);


public:

	/**
	Check if the player has ever completed a specific action. For example you could check if the player has ever talked to a given NPC, taken a certain item, etc
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	bool HasCompletedTask(const UNarrativeTask* Task, const FString& Name, const int32 Quantity = 1 );

	/**
	* 
	IN MULTIPLAYER GAMES, THIS FUNCTION WILL ONLY WORK ON THE SERVER. Please see MasterTaskList comment for more info. 

	Check how many times the player has ever completed a narrative Task. Very efficient.
	
	Very powerful for scripting, for example we could check if the player has talked to an NPC at least 10 times and then change the dialogue
	text to a more personalized greeting since the NPC would know the player better. 

	@return The number of times the Task has been completed by this narrative component. 

	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	int32 GetNumberOfTimesTaskWasCompleted(const UNarrativeTask* Task, const FString& Name);

	/**Check if the player has ever completed a specific task that updated a quest. For example if a quest had a task called "Talk to Chef", you could
	use this to check if the player had ever completed that task during the quest.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	bool HasCompletedTaskInQuest(TSubclassOf<class UQuest> QuestClass, const UNarrativeTask* Task, const FString& Name) const;

	/**Returns a list of all failed quests, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetFailedQuests() const;

	/**Returns a list of all succeeded quests, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetSucceededQuests() const;

	/**Returns a list of all quests that are in progress, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetInProgressQuests() const;

	/**Returns a list of all quests that are started, failed, or succeeded, in chronological order.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	TArray<UQuest*> GetAllQuests() const;

	/**Given a Quest class return its active quest object if we've started this quest */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Quests")
	class UQuest* GetQuest(TSubclassOf<class UQuest> QuestClass) const;

	/**Set this players shared component. Any tasks the player completes will be forwarded to the shared component.
	
	This means if you complete a find like "FindItem_Sword", your entire teams shared component will also have that task completed. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Shared Quests")
	void SetSharedNarrativeComponent(UNarrativeComponent* NewSharedNarrativeComponent);

	/**Save every quest we've done, and ever task the player has ever completed
	* 
	* This is very efficient, even if the player has completed millions of tasks save files should be really small, 
	* since narrative just saves the task and the number of times it was done, rather than millions of a task.
	* 
	@param SaveName the name of the save game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Save(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**Load narratives state back in from disk
	@param SaveName the name of the save game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Saving")
	virtual bool Load(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**Deletes a saved game from disk. USE THIS WITH CAUTION. Return true if save file deleted, false if delete failed or file didn't exist.*/
	UFUNCTION(BlueprintCallable, Category = "Saving")
	virtual bool DeleteSave(const FString& SaveName = "NarrativeSaveData", const int32 Slot = 0);

	/**
	Once server loads a save in, it sends all the info to the client via this RPC so it can load it in. We do this so loading save
	games can be done by the server and flow down to the client.
	*/
	UFUNCTION(Client, Reliable, Category = "Saving")
	virtual void ClientReceiveSave(const TArray<FNarrativeSavedQuest>& SavedQuests, const TArray<FString>& Tasks, const TArray<int32>& Quantities);

protected:

	//Internal load function that actually does the work.
	virtual bool Load_Internal(const TArray<FNarrativeSavedQuest>& SavedQuests, const TMap<FString, int32>& NewMasterList);

};
