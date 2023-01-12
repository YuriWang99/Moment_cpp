// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "QuestSM.h"
#include "Quest.generated.h"

class UQuestState;
class UQuestBranch;
class UQuestBlueprint;
class UNarrativeTask;


//Represents the state of a particular quest
UENUM(BlueprintType)
enum class EQuestCompletion : uint8
{
	QC_NotStarted,
	QC_Started,
	QC_Succeded,
	QC_Failed
};

//Represents what happened after we completed a narrative task and updated a quest
UENUM()
enum class EQuestProgress : uint8
{
	/**When we complete a task, it either:*/
	QP_NoChange, //Didn't affect the quest at all
	QP_MadeProgress, //Made progress, but didn't update the quest to the next step
	QP_Updated, //Updated the quest to the next step
	QP_Failed, //Failed the quest
	QP_Succeeded // Succeeded the quest
};

UCLASS(Blueprintable, BlueprintType)
class NARRATIVE_API UQuest : public UObject
{
	GENERATED_BODY()
	
protected:

	UQuest();

	//Dialogue assets/nodes etc have the same name on client and server, so can be referenced over the network 
	bool IsNameStableForNetworking() const override { return true; };
	bool IsSupportedForNetworking() const override { return true; };

	//The current state the player is at in this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* CurrentState;

	virtual UWorld* GetWorld() const override;

	UPROPERTY()
	class UNarrativeComponent* OwningComp;

public:

	UFUNCTION(BlueprintPure, Category = "Narrative")
	class UNarrativeComponent* GetOwningNarrativeComponent() const;
		
	UFUNCTION(BlueprintPure, Category = "Narrative")
	class APawn* GetPawnOwner() const;


	//Initialize this quest from its blueprint generated class. Return true if successful. 
	virtual bool Initialize(class UNarrativeComponent* InitializingComp, const FName& QuestStartID = NAME_None);

	virtual void DuplicateAndInitializeFromQuest(UQuest* QuestTemplate);

	EQuestProgress UpdateQuest(UNarrativeComponent* NarrativeComp, const FString& EventString = "");

	//Checks if we can skip a call to UpdateQuest()
	bool CanSkipUpdate(const FString& EventString) const;

	virtual void EnterState(UQuestState* NewState, UNarrativeComponent* RunningComp, UQuestBranch* BranchUsed = nullptr);
	FORCEINLINE UQuestState* GetCurrentState() const { return CurrentState; }

	class UQuestState* GetState(const FName& ID) const;

	UPROPERTY(EditAnywhere, Category = "Quest Details")
	FText QuestName;

	UPROPERTY(EditAnywhere, Category = "Quest Details", meta = (MultiLine = true))
	FText QuestDescription;

	UFUNCTION(BlueprintCallable, Category = "Quests")
    FText GetQuestName() const;

	UFUNCTION(BlueprintCallable, Category = "Quests")
	FText GetQuestDescription() const;

	//The beginning state of this quest
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestState* QuestStartState;

	//Holds all of the states in the quest
	UPROPERTY()
	TArray<UQuestState*> States;

	//Holds all of the branches in the quest
	UPROPERTY()
	TArray<UQuestBranch*> Branches;

	//The branch that was taken to get to the current state
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	UQuestBranch* PreviousBranch;

	/**Current quest progress*/
	UPROPERTY()
	EQuestCompletion QuestCompletion;

	/**All input for this quest*/
	UPROPERTY()
	TArray<FString> QuestActivities;

	/**All the states we've reached so far. Useful for a quest journal, where we need to show the player what they have done so far*/
	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	TArray<UQuestState*> ReachedStates;

protected:


	UFUNCTION()
		void OnQuestStarted(const UQuest* Quest);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName="On Quest Started"))
		void BPOnQuestStarted(const UQuest* Quest);

	UFUNCTION()
		void OnQuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Failed"))
		void BPOnQuestFailed(const UQuest* Quest, const FText& QuestFailedMessage);

	UFUNCTION()
		void OnQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Succeeded"))
		void BPOnQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage);

	UFUNCTION()
		void OnQuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest New State"))
		void BPOnQuestNewState(UQuest* Quest, const UQuestState* NewState);

	UFUNCTION()
		void OnQuestTaskProgressMade(const UQuest* Quest, const FQuestTask& Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Objective Progress Made"))
		void BPOnQuestTaskProgressMade(const UQuest* Quest, const FQuestTask& Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress);

	UFUNCTION()
		void OnQuestStepCompleted(const UQuest* Quest, const class UQuestBranch* Step);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Quest Objective Completed"))
		void BPOnQuestStepCompleted(const UQuest* Quest, const class UQuestBranch* Step);

	UFUNCTION()
		void OnBeginSave(FString SaveName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Begin Save"))
		void BPOnBeginSave(const FString& SaveName);

	UFUNCTION()
		void OnBeginLoad(FString SaveName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Begin Load"))
		void BPOnBeginLoad(const FString& SaveName);

	UFUNCTION()
		void OnSaveComplete(FString SaveName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Save Complete"))
		void BPOnSaveComplete(const FString& SaveName);

	UFUNCTION()
		void OnLoadComplete(FString SaveName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Load Complete"))
		void BPOnLoadComplete(const FString& SaveName);

public:

	UFUNCTION(BlueprintPure, Category = "Quests")
	TArray<UQuestNode*> GetNodes() const;

	//Grab the completion of the quest 
	UFUNCTION(BlueprintPure, Category = "Quests")
	FORCEINLINE EQuestCompletion GetQuestCompletion() const { return QuestCompletion; }

	/* All tasks start at 0 progress, but we might want to override that, for example if the task is find 
	10 sticks, and the player already has 3, we would want to override the initial progress to be 3 instead of the default of 0*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Get Task Initial Progress"))
	int32 GetTaskInitialProgress(const FQuestTask& Task);

};
