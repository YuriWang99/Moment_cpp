// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeNodeBase.h"
#include "QuestSM.generated.h"

class UQuest;
class UQuestState;
class UQuestBranch;
class UNarrativeTask;
class UQuestBlueprint;

/**
* Used for checking the result of our state machine.
*/
UENUM(BlueprintType)
enum class EStateQuestResult : uint8
{
	// Implicit fail - The state is not marked as accept
	NotAccepted,
	// Success - state is an accept state
	Accepted,
	// Explicit fail - state is marked as failure
	Rejected
};

//A quest is a series of state machines, branches are taken by completing all the FNarrativeTasks in that branch.
USTRUCT(BlueprintType)
struct NARRATIVE_API FQuestTask
{
GENERATED_BODY()

public:
	FQuestTask(UNarrativeTask* InEvent, const FString& InArgument, const int32 InQuantity, const bool bInHidden, const bool bInOptional) : Task(InEvent), Argument(InArgument), Quantity(InQuantity), bHidden(bInHidden), bOptional(bInOptional)
	{
		CurrentProgress = 0;
	};

	FQuestTask()
	{
		Task = nullptr;
		Argument = "";
		Quantity = 1;
		CurrentProgress = 0;
		bHidden = false;
		bOptional = false;
		bRetroactive = false;
	};

	virtual ~FQuestTask(){};

	/**The event the player needs to do to complete this task*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	UNarrativeTask* Task;

	/**The reference to be passed into the action*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FString Argument;

	/**The amount of times we need to complete this action to move on to the next part of the quest*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task", meta = (ClampMin = 1))
	int32 Quantity;

	/**Should this task be hidden from the player (Great for quests with hidden options)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	bool bHidden;

	/** SINGLE PLAYER ONLY: Should this task be optional?*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	bool bOptional;

	/** SINGLE PLAYER ONLY: Should it count if the player has already done this task in the past?*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	bool bRetroactive;

	/**Description for this task. For example "Kill 10 Goblins", "Obtain an Iron Sword", "Find the briefcase", etc... */
	UPROPERTY(EditAnywhere, Category = "Task", BlueprintReadOnly, meta = (MultiLine = true))
	FText TaskDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Quests")
	int32 CurrentProgress;

	//Called when we get to the point in the quest that we encounter this task
	virtual void ReachedTask(const class UQuestBranch* OwningBranch, class UNarrativeComponent* NarrativeComp);

	//See if the completed event updated this task
	virtual bool UpdateTask(const FString& RawEvent, const class UQuestBranch* OwningBranch, class UNarrativeComponent* NarrativeComp);

	//By default task is complete if we've done the task the required quantity of times or if task if optional
	virtual bool IsComplete() const;

	//Reset progress on this task
	virtual void ResetTask() {CurrentProgress = 0;}

};

/**
* Represents the result of running a state machine. Contains useful info for checking state machine result.
*/
USTRUCT(BlueprintType)
struct NARRATIVE_API FStateMachineResult
{
	GENERATED_USTRUCT_BODY()

		FStateMachineResult() {};

	FStateMachineResult(EStateQuestResult NewCompletionType, class UQuestBranch* NewBranch, class UQuestState* NewFinalState)
	{
		CompletionType = NewCompletionType;
		FinalState = NewFinalState;
		Branch = NewBranch;
	}

	/**Allows us to check how the machine ended*/
	UPROPERTY()
	EStateQuestResult CompletionType = EStateQuestResult::NotAccepted;

	/**The branch we took to get to finalstate*/
	UPROPERTY()
	class UQuestBranch* Branch = nullptr;

	/**The state that the machine ended on*/
	UPROPERTY()
	class UQuestState* FinalState = nullptr;

};

/**Base class for states and branches in the quests state machine*/
 UCLASS()
 class NARRATIVE_API UQuestNode : public UNarrativeNodeBase
 {

	 GENERATED_BODY()

 public:

#if WITH_EDITOR

	 virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	 void EnsureUniqueID();

#endif WITH_EDITOR

	/**Description for this quest node. For example "Kill 10 Goblins", "Find the Gemstone", or "I've found the Gemstone, I need to return to King Edward" */
	UPROPERTY(EditAnywhere, Category = "Details", BlueprintReadOnly, meta = (MultiLine = true, DisplayAfter="ID"))
	FText Description;

	//Get the quest that this quest node belongs to
	UQuest* GetOwningQuest() const;
	class UNarrativeComponent* GetOwningNarrativeComp() const;

	//The title the node should have on the quest node title
	virtual FText GetNodeTitle() const { return FText::FromString("Node"); };

	//TODO can probably remove this. Last time we went through this branch or state. Used for PIE debugging
	UPROPERTY()
	FDateTime LastExecTime;

	//The quest object that owns this node. 
	UPROPERTY()
	class UQuest* OwningQuest;
 };

 DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStateReachedEvent);

UCLASS(BlueprintType)
class NARRATIVE_API UQuestState : public UQuestNode
{
	GENERATED_BODY()

public:

	UQuestState();

	/**Attempt to run the state*/
	virtual FStateMachineResult RunState(bool& bOutMadeProgress, UNarrativeComponent* NarrativeComp, const FString& InputString = "");

	virtual FText GetNodeTitle() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Quest State")
	TArray<UQuestBranch*> Branches;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnStateReachedEvent OnStateReached;

	/**If input runs out on this state, this is how the result will be interpreted.*/
	UPROPERTY(BlueprintReadOnly, Category = "Quest State")
	EStateQuestResult CompletionType;

};

UCLASS(BlueprintType)
class NARRATIVE_API UQuestBranch : public UQuestNode
{

	GENERATED_BODY()

public:

	UQuestBranch();

	/**Called when this branches state is entered*/
	void OnReached();

	/**The task the player needs to complete */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	FQuestTask Task;

	/**Should we have more than one task? */
	UPROPERTY()
	bool bAddMultipleTasks;

	/**Tasks to complete for this branch to be taken*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Task")
	TArray<FQuestTask> Tasks;

	/**Should this branch be hidden from the player on the narrative demo UI (Great for quests with hidden options that we want to be part
	of the quest logic, but we don't want the UI to show)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Details")
	bool bHidden;

	/**State where we will go if this branch is taken. Branch will be ignored if this is null*/
	UPROPERTY(BlueprintReadOnly, Category = "Details")
	UQuestState* DestinationState;

	/**Try this branch. Return the state we led to if 
	@param RefObject the caller of TryBranch
	@param Input the input to try the branch with
	@param OutMadeProgress Whether the branch made progress. Will be true if input was correct but haven't made enough progress
	*/
	virtual UQuestState* TryBranch(const FString& Input, UNarrativeComponent* NarrativeComp, bool& bOutMadeProgress);

	virtual FText GetNodeTitle() const override;

protected:

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

#endif

	virtual bool AreTasksComplete(const UNarrativeComponent* NarrativeComp) const;

};

