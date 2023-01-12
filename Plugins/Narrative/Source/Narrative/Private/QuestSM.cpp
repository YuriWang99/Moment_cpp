// Copyright Narrative Tools 2022. 

#include "QuestSM.h"
#include "Quest.h"
#include "NarrativeTask.h"
#include "NarrativeComponent.h"
#include "NarrativeQuestSettings.h"

#define LOCTEXT_NAMESPACE "StateMachine"

UQuestState::UQuestState()
{
	Description = LOCTEXT("QuestStateDescription", "Write an update to appear in the players quest journal here.");
}

UQuestBranch::UQuestBranch()
{
	Description = LOCTEXT("QuestBranchDescription", "Describe what the player needs to do to complete this task.");

	Tasks.SetNum(1);
}

void UQuestBranch::OnReached()
{
	for (auto& MyTask : Tasks)
	{
		MyTask.ReachedTask(this, GetOwningNarrativeComp());
	}
}

UQuestState* UQuestBranch::TryBranch(const FString& Input, UNarrativeComponent* NarrativeComp, bool& bOutMadeProgress)
{
	//We shouldnt have a quest state that doesn't have a quest
	check(GetOwningQuest());

	if (NarrativeComp)
	{
		//See if this input updated any of the current tasks in the quest 
		for (auto& MyTask : Tasks)
		{
			//Dont bother updating already completed tasks 
			if (!MyTask.IsComplete())
			{
				bOutMadeProgress = MyTask.UpdateTask(Input, this, NarrativeComp);
			}
		}

		if (AreTasksComplete(NarrativeComp))
		{
		#if WITH_EDITOR
			LastExecTime = DestinationState->LastExecTime = FDateTime::Now();
		#endif
		
			//We've completed all the tasks in this step, this step of the quest is done! 
			NarrativeComp->OnQuestStepCompleted.Broadcast(GetOwningQuest(), this);

			if (const UNarrativeQuestSettings* QuestSettings = GetDefault<UNarrativeQuestSettings>())
			{
				if (QuestSettings->bResetTasksWhenCompleted)
				{
					//Reset the tasks in case we come back to them later on in the quest 
					for (auto& MyTask : Tasks)
					{
						MyTask.ResetTask();
					}
				}
			}

			return DestinationState;
		}

	}
	//haven't completed all the tasks for this branch yet, retrun nullptr
	return nullptr;
}

FText UQuestBranch::GetNodeTitle() const
{
	FString Title = "";
	int32 Idx = 0;


	if (bAddMultipleTasks)
	{
		Title += "Tasks: ";
		Title += LINE_TERMINATOR;

		for (auto& MyTask : Tasks)
		{
			if (!MyTask.Task)
			{
				continue;
			}

			if (Idx > 0)
			{
				Title += LINE_TERMINATOR;
			}

			//For custom tasks, just display "Task"
			const FString TaskName = MyTask.Task->TaskName.IsEmpty() ? FString("Task") : MyTask.Task->TaskName;

			if (MyTask.Quantity <= 1)
			{
				Title += FString::Printf(TEXT("%s: %s"), *TaskName, *MyTask.Argument);
			}
			else
			{
				Title += FString::Printf(TEXT("%s: %s %d times"), *TaskName, *MyTask.Argument, MyTask.Quantity);
			}

			++Idx;
		}
	}
	else
	{
		Title += "Task: ";

		if (!Task.Task)
		{
			Title += " None";
		}
		else
		{	
			//For custom tasks, just display "Task"
			const FString TaskName = Task.Task->TaskName.IsEmpty() ? FString("Task") : Task.Task->TaskName;

			if (Task.Quantity <= 1)
			{
				Title += FString::Printf(TEXT("%s: %s"), *TaskName, *Task.Argument);
			}
			else
			{
				Title += FString::Printf(TEXT("%s: %s %d times"), *TaskName, *Task.Argument, Task.Quantity);
			}
		}
	}

	
	return FText::FromString(Title);
}

#if WITH_EDITOR

void UQuestBranch::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestBranch, Tasks))
	{
		const int32 Idx = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());

		if (Tasks.IsValidIndex(Idx))
		{
			//Keep tasks[0] and task in sync
			if (Tasks.IsValidIndex(0))
			{
				Task = Tasks[0];
			}
			
			FQuestTask& ChangedTask = Tasks[Idx];

			if (ChangedTask.Task && ChangedTask.TaskDescription.IsEmpty())
			{
				const FString NewDescription = ChangedTask.Task->AutofillDescription.Replace(TEXT("%argument%"), *ChangedTask.Argument);
				ChangedTask.TaskDescription = FText::FromString(NewDescription);
			}
		}
	}
	else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestBranch, Task))
	{
		if (Tasks.Num() == 0)
		{
			Tasks.SetNum(1);
		}

		//Keep tasks[0] and task in sync
		if (Tasks.IsValidIndex(0))
		{
			Tasks[0] = Task;
		}

		if (Task.Task && Task.TaskDescription.IsEmpty())
		{
			const FString NewDescription = Task.Task->AutofillDescription.Replace(TEXT("%argument%"), *Task.Argument);
			Task.TaskDescription = FText::FromString(NewDescription);
		}

	} //Autofill task description with ours
	else if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestBranch, Description))
	{
		//Autofill task description if empty 
		if (Task.TaskDescription.IsEmpty())
		{
			Task.TaskDescription = Description;
		}

		if (Tasks.IsValidIndex(0) && Tasks[0].TaskDescription.IsEmpty())
		{
			Tasks[0].TaskDescription = Description;
		}
	}
}

void UQuestBranch::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode() && PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue())
	{
		if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestBranch, Tasks))
		{
			//TODO Idx keeps being -1 so autofill wont work 
			const int32 Idx = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());

			if (Tasks.IsValidIndex(Idx))
			{
				FQuestTask& ChangedTask = Tasks[Idx];
			
				if (ChangedTask.Task && ChangedTask.TaskDescription.IsEmpty())
				{
					//Autofill the description - we have to use this func directly because we dont have a TCHAR literal
					const FString NewDescription = ChangedTask.Task->AutofillDescription.Replace(TEXT("%argument%"), *ChangedTask.Argument);
					ChangedTask.TaskDescription = FText::FromString(NewDescription);
				}
			}

			//Keep tasks[0] and task in sync
			if (Tasks.IsValidIndex(0))
			{
				Task = Tasks[0];
			}
		}
	}
}

#endif

bool UQuestBranch::AreTasksComplete(const UNarrativeComponent* NarrativeComp) const
{
	bool bCompletedAllTasks = true;

	if (!bAddMultipleTasks)
	{
		checkf(Tasks.IsValidIndex(0), TEXT("Using single task but Tasks[0] was not valid and should be."));
	
		return Tasks[0].IsComplete();
	}

	for (auto& MyTask : Tasks)
	{
		if (!MyTask.IsComplete())
		{
			bCompletedAllTasks = false;
			break;
		}
	}

	return bCompletedAllTasks;
}

FStateMachineResult UQuestState::RunState(bool& bOutMadeProgress, UNarrativeComponent* NarrativeComp, const FString& Input)
{
	UQuestState* DestinationState = nullptr;

	if (NarrativeComp)
	{
		for (auto Branch : Branches)
		{
			DestinationState = Branch->TryBranch(Input, NarrativeComp, bOutMadeProgress);

			//One of the branches took us to a new state
			if (DestinationState)
			{
				return FStateMachineResult(DestinationState->CompletionType, Branch, DestinationState);
			}
		}
	}

	//We just stayed on the same state 
	return FStateMachineResult(CompletionType, nullptr, this);
}

FText UQuestState::GetNodeTitle() const
{
	if (ID == NAME_None)
	{
		return FText::GetEmpty();
	}

	FString ReadableString;
	ReadableString = FName::NameToDisplayString(ID.ToString(), false);
	return FText::FromString(ReadableString);
}

#if WITH_EDITOR

void UQuestNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		//If we changed the ID, make sure it doesn't conflict with any other IDs in the quest
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UQuestNode, ID))
		{
			EnsureUniqueID();
		}
	}
}

void UQuestNode::EnsureUniqueID()
{
	if (UQuest* Quest = GetOwningQuest())
	{
		bool bIDAlreadyExists = false;

		for (auto& State : Quest->States)
		{
			//If the ID already exists, just append a 1 to the end 
			if (State && State != this && State->ID == ID)
			{
				ID = FName(ID.ToString() + '1');
				EnsureUniqueID(); // Need to call this again in case new ID is also a duplicate
				return;
			}
		}

		for (auto& Branch : Quest->Branches)
		{
			//If the ID already exists, just append a 1 to the end 
			if (Branch && Branch != this && Branch->ID == ID)
			{
				ID = FName(ID.ToString() + '1');
				EnsureUniqueID(); // Need to call this again in case new ID is also a duplicate
				return;
			}
		}
	}
}

#endif

UQuest* UQuestNode::GetOwningQuest() const
{
	//In the editor, outer will be the quest. At runtime, quest object is manually set
	return OwningQuest ? OwningQuest : CastChecked<UQuest>(GetOuter());
}

class UNarrativeComponent* UQuestNode::GetOwningNarrativeComp() const
{
	if (UQuest* Quest = GetOwningQuest())
	{
		return Quest->GetOwningNarrativeComponent();
	}

	return nullptr;
}

void FQuestTask::ReachedTask(const class UQuestBranch* OwningBranch, class UNarrativeComponent* NarrativeComp)
{

	if (!OwningBranch || !NarrativeComp)
	{
		return;
	}

	if (UQuest* OwningQuest = OwningBranch->GetOwningQuest())
	{
		//Retroactive tasks and initial progress are only available in single player games for now 
		//Set the initial progress 
		const int32 TaskInitialProgress = OwningQuest->GetTaskInitialProgress(*this);

		if (TaskInitialProgress > 0)
		{
			if (NarrativeComp->GetNetMode() == NM_Standalone)
			{
				CurrentProgress = TaskInitialProgress;
			}
			else
			{ 
				UE_LOG(LogNarrative, Warning, TEXT("A quest has overriden GetTaskInitialProgress but this isn't supported in Networked games! Tasks inital progress will be 0."));
			}
		}

		//The task is retroactive, look into the past to see if we've done this task retroactively 
		if (bRetroactive && NarrativeComp)
		{
			if (NarrativeComp->GetNetMode() == NM_Standalone)
			{
				CurrentProgress += NarrativeComp->GetNumberOfTimesTaskWasCompleted(Task, Argument);
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("A task was marked as retroactive but retroactive tasks aren't supported in networked games! Tasks progress won't be effected."));
			}
		}

		//We may have already completed the task via initial progress or retroactive progress
		if (IsComplete())
		{
			NarrativeComp->OnQuestTaskCompleted.Broadcast(OwningBranch->GetOwningQuest(), *this, OwningBranch);
		}
		else
		{
			if (CurrentProgress > 0)
			{
				NarrativeComp->OnQuestTaskProgressMade.Broadcast(OwningBranch->GetOwningQuest(), *this, OwningBranch, CurrentProgress, Quantity);
			}
		}
	}
}

bool FQuestTask::UpdateTask(const FString& RawTask, const class UQuestBranch* OwningBranch, class UNarrativeComponent* NarrativeComp)
{
	if (!OwningBranch)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative found a task in the Quest: %s that had no owning branch. "));
		return false;
	}

	if (!NarrativeComp)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative found a task in the Quest: %s that had an invalid narrative comp passed in. "));
		return false;
	}

	if (!Task)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative found a task in the Quest: %s that had no Task specified. Please check your quest for any missing information."),
			*OwningBranch->GetOwningQuest()->QuestName.ToString());

		return false;
	}

	if (Argument.IsEmpty())
	{
		UE_LOG(LogNarrative, Warning, TEXT("Narrative found a task in the Quest: %s that had no argument specified. Please check your quest for any missing information."), 
		*OwningBranch->GetOwningQuest()->QuestName.ToString());

		return false;
	}

	bool bTaskUpdated = false;

	//TODO maybe just use FString instead of FName since we're converting them to check wildcards anyway
	FString InputToTry = RawTask;
	FString RequiredInput = Task->MakeTaskString(Argument);

	//TODO make wildcards an option that needs to be turned on to improve performance
	bTaskUpdated = RequiredInput.Contains("*") ? InputToTry.MatchesWildcard(RequiredInput) : InputToTry == RequiredInput;

	if (bTaskUpdated)
	{
		++CurrentProgress;

		//If we haven't completed this task but made progress on it we need to broadcast that progress 
		if (IsComplete())
		{
			NarrativeComp->OnQuestTaskCompleted.Broadcast(OwningBranch->GetOwningQuest(), *this, OwningBranch);
		}
		else
		{
			NarrativeComp->OnQuestTaskProgressMade.Broadcast(OwningBranch->GetOwningQuest(), *this, OwningBranch, CurrentProgress, Quantity);
		}
	}

	return bTaskUpdated;
}


bool FQuestTask::IsComplete() const
{
	return CurrentProgress >= Quantity || bOptional; 
}

#undef LOCTEXT_NAMESPACE