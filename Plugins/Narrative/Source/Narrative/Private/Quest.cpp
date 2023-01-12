// Copyright Narrative Tools 2022. 

#include "Quest.h"
#include "NarrativeTask.h"
#include "QuestSM.h"
#include "Net/UnrealNetwork.h"
#include "QuestBlueprintGeneratedClass.h"
#include "NarrativeEvent.h"
#include "NarrativeComponent.h"

UQuest::UQuest()
{
	QuestName = FText::FromString("My New Quest");
	QuestDescription = FText::FromString("Enter a description for your quest here.");
}

class UNarrativeComponent* UQuest::GetOwningNarrativeComponent() const
{
	return OwningComp;
}

class APawn* UQuest::GetPawnOwner() const
{
	return OwningComp ? OwningComp->GetOwningPawn() : nullptr;
}

UWorld* UQuest::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	if (GetOwningNarrativeComponent())
	{
		return GetOwningNarrativeComponent()->GetWorld();
	}
	
	return nullptr;
}

void UQuest::DuplicateAndInitializeFromQuest(UQuest* QuestTemplate)
{
	if (QuestTemplate)
	{
		//Duplicate the quest template, then steal all its states and branches
		UQuest* NewQuest = Cast<UQuest>(StaticDuplicateObject(QuestTemplate, this, NAME_None, RF_Transactional));
		NewQuest->SetFlags(RF_Transient | RF_DuplicateTransient);

		if (NewQuest)
		{
			QuestStartState = NewQuest->QuestStartState;
			States = NewQuest->States;
			Branches = NewQuest->Branches;

			//Change the outer of the duplicated states and branches to this quest instead of the template one.
			//TODO look into whether this is safe, should we possibly duplicating each state and branch individually or has StaticDuplicateObject made deep copies? 
			//for (auto& State : States)
			//{
			//	if (State)
			//	{
			//		State->Rename(nullptr, this);
			//	}
			//}

			//for (auto& Branch : Branches)
			//{
			//	if (Branch)
			//	{
			//		Branch->Rename(nullptr, this);

			//		//Cull any extra tasks that were added if this branch only has a single task
			//		if (!Branch->bAddMultipleTasks)
			//		{
			//			Branch->Tasks.SetNum(1);
			//		}
			//	}
			//}
		}
	}
}

bool UQuest::Initialize(class UNarrativeComponent* InitializingComp, const FName& QuestStartID /*= NAME_None*/)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//We need a valid narrative component to make a quest for 
		if (!InitializingComp)
		{
			return false;
		}

		if (UQuestBlueprintGeneratedClass* BGClass = Cast<UQuestBlueprintGeneratedClass>(GetClass()))
		{
			BGClass->InitializeQuest(this);

			//If a quest doesn't have any states or branches, or doesn't have a valid start state something has gone wrong and we should abort
			if (States.Num() == 0 || Branches.Num() == 0 || !QuestStartState)
			{
				return false;
			}

			//At this point, we should have a valid quest assigned to us. Check if we have a valid start state
			if (QuestStartState)
			{
				OwningComp = InitializingComp;

				if (QuestStartState)
				{
					QuestStartState->OwningQuest = this;
				}

				for (auto& Branch : Branches)
				{
					if (Branch)
					{
						Branch->OwningQuest = this;
					}
				}

				for (auto& State : States)
				{
					if (State)
					{
						State->OwningQuest = this;
					}
				}

				//TODO these are going to fire for any quest, do we need this?
				OwningComp->OnQuestStarted.AddUniqueDynamic(this, &UQuest::OnQuestStarted);
				OwningComp->OnQuestFailed.AddUniqueDynamic(this, &UQuest::OnQuestFailed);
				OwningComp->OnQuestSucceeded.AddUniqueDynamic(this, &UQuest::OnQuestSucceeded);
				OwningComp->OnQuestStepCompleted.AddUniqueDynamic(this, &UQuest::OnQuestStepCompleted);
				OwningComp->OnQuestNewState.AddUniqueDynamic(this, &UQuest::OnQuestNewState);
				OwningComp->OnQuestTaskProgressMade.AddUniqueDynamic(this, &UQuest::OnQuestTaskProgressMade);
				OwningComp->OnBeginSave.AddUniqueDynamic(this, &UQuest::OnBeginSave);
				OwningComp->OnBeginLoad.AddUniqueDynamic(this, &UQuest::OnBeginLoad);
				OwningComp->OnSaveComplete.AddUniqueDynamic(this, &UQuest::OnSaveComplete);
				OwningComp->OnLoadComplete.AddUniqueDynamic(this, &UQuest::OnLoadComplete);

				QuestCompletion = EQuestCompletion::QC_Started;

				UQuestState* StartState = QuestStartID.IsNone() ? QuestStartState : GetState(QuestStartID);

				if (!StartState)
				{
					UE_LOG(LogTemp, Warning, TEXT("Tried starting quest %s using start state ID %s but couldn't find a node with that ID. Be careful about renaming node IDs as this can break old save files/code with stale IDs."), *QuestName.ToString(), *QuestStartID.ToString());
					StartState = QuestStartState;
				}

				EnterState(StartState, OwningComp);
				return true;
			}
		}
	}
	return false;
}

EQuestProgress UQuest::UpdateQuest(UNarrativeComponent* NarrativeComp, const FString& EventString /*=""*/)
{

	if (!CurrentState)
	{
		check(false); // shouldnt happen
		return EQuestProgress::QP_NoChange;
	}

	//No need to update this quest 
	if (CanSkipUpdate(EventString))
	{
		return EQuestProgress::QP_NoChange;
	}

	if (QuestCompletion == EQuestCompletion::QC_Started)
	{
		FStateMachineResult QuestResult;
		QuestActivities.Add(EventString);

		bool bMadeProgress = false;
		bool bChangedState = false;

		QuestResult = CurrentState->RunState(bMadeProgress, NarrativeComp, EventString);

		while (QuestResult.FinalState != GetCurrentState())
		{
			PreviousBranch = QuestResult.Branch;

			EnterState(QuestResult.FinalState, NarrativeComp, PreviousBranch);

			bChangedState = true;

			//Run the state again, we may have already completed a branch on it even though we just entered it 
			QuestResult = CurrentState->RunState(bMadeProgress, NarrativeComp, EventString);
		}

		//Check if our event took us to a new state 
		if (bChangedState)
		{
			switch (QuestResult.CompletionType)
			{
				//We reached the end state of a quest and succeeded it
				case EStateQuestResult::Accepted:
				{
					return EQuestProgress::QP_Succeeded;
				}
				//We reached the end state of a quest and failed it
				case EStateQuestResult::Rejected:
				{
					return EQuestProgress::QP_Failed;
				}
				//We reached a new state in the quest
				case EStateQuestResult::NotAccepted:
				{
					return EQuestProgress::QP_Updated;
				}
			}
		}
		else if (bMadeProgress)//We didnt reach a new state on the quest, but we made progress on a step
		{
			return EQuestProgress::QP_MadeProgress;
		}

	}

	//The action didn't update any quests
	return EQuestProgress::QP_NoChange;
}

bool UQuest::CanSkipUpdate(const FString& EventString) const
{
	return false;
}


void UQuest::EnterState(UQuestState* NewState, UNarrativeComponent* SourceComp, UQuestBranch* BranchUsed /*= nullptr*/)
{
	if (NewState)
	{
		CurrentState = NewState;
		ReachedStates.Add(CurrentState);

		//Update the quests completion
		if (NewState->CompletionType == EStateQuestResult::Accepted)
		{
			QuestCompletion = EQuestCompletion::QC_Succeded;
		}
		else if (NewState->CompletionType == EStateQuestResult::Rejected)
		{
			QuestCompletion = EQuestCompletion::QC_Failed;
		}

		//If we're loading quests back in off disk we don't want to broadcast any progress or anything
		if(SourceComp && SourceComp->bIsLoading)
		{
			return;
		}

		if (SourceComp)
		{

			SourceComp->OnQuestNewState.Broadcast(this, CurrentState);


			APawn* OwningPawn = SourceComp->GetOwningPawn();
			APlayerController* OwningPC = SourceComp->GetOwningController();

			if (CurrentState)
			{
				for (auto& Event : CurrentState->Events)
				{
					if (Event)
					{
						Event->ExecuteEvent(OwningPawn, OwningPC, SourceComp);
					}
				}
			}

			if (BranchUsed)
			{
				for (auto& Event : BranchUsed->Events)
				{
					if (Event)
					{
						Event->ExecuteEvent(OwningPawn, OwningPC, SourceComp);
					}
				}
			}
		}

		//We're in a new state. Tell the branches they have been reached, they can then check if we've completed them. 
		for (auto& Branch : CurrentState->Branches)
		{
			if (Branch)
			{
				Branch->OnReached();
			}
		}
	}
}

class UQuestState* UQuest::GetState(const FName& ID) const
{
	for (auto& State : States)
	{
		if (State->ID == ID)
		{
			return State;
		}
	}
	return nullptr;
}

FText UQuest::GetQuestName() const
{
	return QuestName;
}

FText UQuest::GetQuestDescription() const
{
	return QuestDescription;
}

void UQuest::OnQuestStarted(const UQuest* Quest)
{
	BPOnQuestStarted(Quest);
}

void UQuest::OnQuestFailed(const UQuest* Quest, const FText& QuestFailedMessage)
{
	BPOnQuestFailed(Quest, QuestFailedMessage);
}

void UQuest::OnQuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage)
{
	BPOnQuestSucceeded(Quest, QuestSucceededMessage);
}

void UQuest::OnQuestNewState( UQuest* Quest, const UQuestState* NewState)
{
	BPOnQuestNewState(Quest, NewState);
}

void UQuest::OnQuestTaskProgressMade(const UQuest* Quest, const FQuestTask& Task, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress)
{
	BPOnQuestTaskProgressMade(Quest, Task, Step, CurrentProgress, RequiredProgress);
}

void UQuest::OnQuestStepCompleted(const UQuest* Quest, const class UQuestBranch* Step)
{
	BPOnQuestStepCompleted(Quest, Step);
}

void UQuest::OnBeginSave(FString SaveName)
{
	BPOnBeginSave(SaveName);
}

void UQuest::OnBeginLoad(FString SaveName)
{
	BPOnBeginLoad(SaveName);
}

void UQuest::OnSaveComplete(FString SaveName)
{
	BPOnSaveComplete(SaveName);
}

void UQuest::OnLoadComplete(FString SaveName)
{
	BPOnLoadComplete(SaveName);
}

TArray<UQuestNode*> UQuest::GetNodes() const
{
	TArray<UQuestNode*> Ret;

	for (auto& State : States)
	{
		Ret.Add(State);
	}

	for (auto& Branch : Branches)
	{
		Ret.Add(Branch);
	}

	return Ret;
}
