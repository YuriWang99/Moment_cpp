// Copyright Narrative Tools 2022. 

#include "NarrativeComponent.h"
#include "NarrativeSaveGame.h"
#include "NarrativeFunctionLibrary.h"
#include "DialogueSM.h"
#include "Quest.h"
#include "NarrativeTask.h"
#include "QuestSM.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/ActorChannel.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "NarrativeDialogueSettings.h"

DEFINE_LOG_CATEGORY(LogNarrative);

static TAutoConsoleVariable<bool> CVarShowQuestUpdates(
	TEXT("narrative.ShowQuestUpdates"),
	false,
	TEXT("Show updates to any of our quests on screen.\n")
);

// Sets default values for this component's properties
UNarrativeComponent::UNarrativeComponent()
{
	SetIsReplicatedByDefault(true);

	OnNarrativeTaskCompleted.AddDynamic(this, &UNarrativeComponent::NarrativeTaskCompleted);
	OnQuestStarted.AddDynamic(this, &UNarrativeComponent::QuestStarted);
	OnQuestFailed.AddDynamic(this, &UNarrativeComponent::QuestFailed);
	OnQuestSucceeded.AddDynamic(this, &UNarrativeComponent::QuestSucceeded);
	OnQuestForgotten.AddDynamic(this, &UNarrativeComponent::QuestForgotten);
	OnQuestStepCompleted.AddDynamic(this, &UNarrativeComponent::QuestStepCompleted);
	OnQuestNewState.AddDynamic(this, &UNarrativeComponent::QuestNewState);
	OnQuestTaskProgressMade.AddDynamic(this, &UNarrativeComponent::QuestTaskProgressMade);
	OnBeginSave.AddDynamic(this, &UNarrativeComponent::BeginSave);
	OnBeginLoad.AddDynamic(this, &UNarrativeComponent::BeginLoad);
	OnSaveComplete.AddDynamic(this, &UNarrativeComponent::SaveComplete);
	OnLoadComplete.AddDynamic(this, &UNarrativeComponent::LoadComplete);

	OnDialogueBegan.AddDynamic(this, &UNarrativeComponent::DialogueBegan);
	OnDialogueFinished.AddDynamic(this, &UNarrativeComponent::DialogueFinished);

	bIsLoading = false;
}


void UNarrativeComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = GetOwningController();
}

void UNarrativeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Tick the current dialogue if we have one 
	if (CurrentDialogue)
	{
		CurrentDialogue->Tick(DeltaTime);
	}
}

void UNarrativeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	Super::EndPlay(EndPlayReason);

	if (CurrentDialogue)
	{
		CurrentDialogue->Deinitialize();
	}

}

void UNarrativeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNarrativeComponent, PendingUpdateList);
	DOREPLIFETIME(UNarrativeComponent, SharedNarrativeComp);

	//Uncomment if you don't other players narrative components to replicate their updates to you 
	//DOREPLIFETIME_CONDITION(UNarrativeComponent, PendingUpdateList, COND_OwnerOnly);
}

bool UNarrativeComponent::HasAuthority() const
{
	return GetOwnerRole() >= ROLE_Authority;
}

bool UNarrativeComponent::IsQuestStartedOrFinished(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion != EQuestCompletion::QC_NotStarted;
		}
	}

	//If quest isnt in the quest list at all we can return false
	return false;
}

bool UNarrativeComponent::IsQuestInProgress(TSubclassOf<class UQuest> QuestClass) const
{  
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Started;
		}
	}

	return false;
}

bool UNarrativeComponent::IsQuestSucceeded(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded;
		}
	}
	return false;
}

bool UNarrativeComponent::IsQuestFailed(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed;
		}
	}
	return false;
}

bool UNarrativeComponent::IsQuestFinished(TSubclassOf<class UQuest> QuestClass) const
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed || QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded;
		}
	}
	return false;
}

class UQuest* UNarrativeComponent::BeginQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID /*= NAME_None*/)
{

	if (QuestClass == UQuest::StaticClass())
	{
		UE_LOG(LogNarrative, Warning, TEXT("BeginQuest was passed UQuest. Supplied quest must be a child of UQuest. "));
		return nullptr;
	}

	if (IsValid(QuestClass))
	{
		if (UQuest* NewQuest = BeginQuest_Internal(QuestClass, StartFromID))
		{
			OnQuestStarted.Broadcast(NewQuest);

			return NewQuest;
		}
	}

	return nullptr;
}

bool UNarrativeComponent::RestartQuest(TSubclassOf<class UQuest> QuestClass, FName StartFromID)
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (int32 i = 0; i < QuestList.Num(); i++)
	{
		if (QuestList.IsValidIndex(i) && QuestList[i]->GetClass()->IsChildOf(QuestClass))
		{
			OnQuestRestarted.Broadcast(QuestList[i]);
			QuestList.RemoveAt(i);
			BeginQuest(QuestClass, StartFromID);

			if (HasAuthority() && GetNetMode() != NM_Standalone)
			{
				SendNarrativeUpdate(FNarrativeUpdate::RestartQuest(QuestClass, StartFromID));
			}

			return true;
		}
	}

	return false;
}

bool UNarrativeComponent::ForgetQuest(TSubclassOf<class UQuest> QuestClass)
{
	if (!IsValid(QuestClass))
	{
		return false;
	}

	for (int32 i = 0; i < QuestList.Num(); i++)
	{
		if (QuestList.IsValidIndex(i) && QuestList[i]->GetClass()->IsChildOf(QuestClass))
		{
			OnQuestForgotten.Broadcast(QuestList[i]);
			QuestList.RemoveAt(i);

			if (HasAuthority() && GetNetMode() != NM_Standalone)
			{
				SendNarrativeUpdate(FNarrativeUpdate::ForgetQuest(QuestClass));
			}
			return true;
		}
	}

	return false;
}

bool UNarrativeComponent::BeginDialogue(TSubclassOf<class UDialogue> DialogueClass, class AActor * NPC, FName StartFromID)
{
	if (HasAuthority())
	{
		//Server constructs the dialogue, then replicates it back to the client so it can begin it
		if (IsValid(DialogueClass))
		{	
			/**If we already have a dialogue running make sure its cleaned up before we begin a new one */
			if (CurrentDialogue)
			{
				OnDialogueFinished.Broadcast(CurrentDialogue);

				CurrentDialogue->Deinitialize();
				CurrentDialogue = nullptr;
			}

			//Attempt to begin the dialogue, and if successful inform client to do the same 
			CurrentDialogue = MakeDialogue(DialogueClass);

			if (CurrentDialogue)
			{
				if (CurrentDialogue->Initialize(this, NPC, StartFromID))
				{
					OnDialogueBegan.Broadcast(CurrentDialogue);

					//This is a networked game, need to inform client to begin this dialogue 
					if (GetNetMode() != NM_Standalone)
					{
						ClientBeginDialogue(DialogueClass, NPC, CurrentDialogue->MakeIDsFromNPCNodes(CurrentDialogue->NPCReplyChain), CurrentDialogue->MakeIDsFromPlayerNodes(CurrentDialogue->AvailableResponses));
					}

					CurrentDialogue->Play();

					return true;
				}
				else
				{	
					//For some reason the dialogue failed to initialize
					CurrentDialogue = nullptr;
					return false;
				}
			}
		}
	}

	return false;
}

void UNarrativeComponent::ClientBeginDialogue_Implementation(TSubclassOf<class UDialogue> DialogueClass, class AActor* NPC, const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	//Server constructs the dialogue, then replicates it back to the client so it can begin it
	if (!HasAuthority() && IsValid(DialogueClass))
	{
		/**If we already have a dialogue running make sure its cleaned up before we begin a new one */
		if (CurrentDialogue)
		{
			OnDialogueFinished.Broadcast(CurrentDialogue);

			CurrentDialogue->Deinitialize();
			CurrentDialogue = nullptr;
		}

		//Attempt to begin the dialogue, and if successful inform client to do the same 
		CurrentDialogue = MakeDialogue(DialogueClass);

		if (CurrentDialogue)
		{
			if (CurrentDialogue->Initialize(SharedNarrativeComp, NPC, NAME_None))
			{
				//Created dialogue won't have a valid chunk yet on the client - use the servers authed chunk it sent
				ClientRecieveDialogueChunk(NPCReplyChainIDs, AvailableResponseIDs);

				OnDialogueBegan.Broadcast(CurrentDialogue);
			}
			else
			{
				//Current dialogue failed to initialize on the client - this should never happen
				CurrentDialogue = nullptr;
			}
		}
	}
}


void UNarrativeComponent::ClientExitDialogue_Implementation()
{
	if (!HasAuthority())
	{
		ExitDialogue();
	}
}

void UNarrativeComponent::ClientRecieveDialogueChunk_Implementation(const TArray<FName>& NPCReplyChainIDs, const TArray<FName>& AvailableResponseIDs)
{
	if (!HasAuthority() && CurrentDialogue)
	{
		CurrentDialogue->ClientReceiveDialogueChunk(NPCReplyChainIDs, AvailableResponseIDs);
	}
}

void UNarrativeComponent::TryExitDialogue()
{
	if (CurrentDialogue)
	{
		if(HasAuthority() && CurrentDialogue->bCanBeExited)
		{
			ExitDialogue();
		}
		else
		{
			ServerExitDialogue();
		}
	}
}

void UNarrativeComponent::ExitDialogue()
{
	if (CurrentDialogue)
	{
		if (HasAuthority())
		{
			ClientExitDialogue();
		}

		OnDialogueFinished.Broadcast(CurrentDialogue);

		CurrentDialogue->Deinitialize();
		CurrentDialogue = nullptr;
	}
}

void UNarrativeComponent::ServerExitDialogue_Implementation()
{
	TryExitDialogue();
}

bool UNarrativeComponent::IsInDialogue()
{
	return CurrentDialogue != nullptr;
}

void UNarrativeComponent::SelectDialogueOption(class UDialogueNode_Player* Option)
{

	if (CurrentDialogue && Option)
	{
		if (!HasAuthority())
		{
			/*If we're not auth we need to ask the server to select the dialogue option. Dialogue pointers can't be passed over the
			network so we just pass the ID, which the server resolves back into the pointer on its end */
			ServerSelectDialogueOption(Option->ID);
		}

		CurrentDialogue->SelectDialogueOption(Option);
	}

}

void UNarrativeComponent::ServerSelectDialogueOption_Implementation(const FName& OptionID)
{
	//Resolve the option ID into the actual option object
	if (CurrentDialogue && !OptionID.IsNone())
	{
		if (UDialogueNode_Player* OptionNode = CurrentDialogue->GetPlayerReplyByID(OptionID))
		{
			SelectDialogueOption(OptionNode);
		}
		else
		{
			UE_LOG(LogNarrative, Warning, TEXT("UNarrativeComponent::ServerSelectDialogueOption_Implementation failed to resolve dialogue option %s."), *OptionID.ToString());
		}
	}
}

bool UNarrativeComponent::CompleteNarrativeTask(const UNarrativeTask* Task, const FString& Argument)
{
	/**
	Behind the scenes, narrative just uses lightweight strings for narrative Tasks. UNarrativeTasks just serve as nice containers for strings ,
	and prevents designers mistyping strings when making quests, since they don't have to type out the string every time, its stored in the asset.

	UNarrativeTasks also do other nice things for us like allowing us to store metadata about the Task, where a string wouldn't let us do that. 
	*/
	if (Task)
	{
		return CompleteNarrativeTask(Task->TaskName, Argument);
	}

	return false;
}

bool UNarrativeComponent::CompleteNarrativeTask(const FString& TaskName, const FString& Argument)
{
	if (HasAuthority())
	{
		if (TaskName.IsEmpty() || Argument.IsEmpty())
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative tried to process an Task that was empty, or argument was empty."));
			return false;
		}

		//We need to lookup the asset to call the delegate
		if (UNarrativeTask* TaskAsset = UNarrativeFunctionLibrary::GetTaskByName(this, TaskName))
		{
			OnNarrativeTaskCompleted.Broadcast(TaskAsset, Argument);
		}
		else
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative tried finding the asset for Task %s, but couldn't find it."), *TaskName);
		}

		FString TaskString = (TaskName + '_' + Argument).ToLower();
		TaskString.RemoveSpacesInline();

		//Convert the Task into an FString and run it through our active quests state machines
		return CompleteNarrativeTask_Internal(TaskString, false);
	}
	else
	{
		//Client cant update quests 
		UE_LOG(LogNarrative, Log, TEXT("Client called UNarrativeComponent::CompleteNarrativeTask. This must be called by the server as quests are server authoritative."));
		return false;
	}

	return false;
}

class UQuest* UNarrativeComponent::BeginQuest_Internal(TSubclassOf<class UQuest> QuestClass, FName StartFromID /*= NAME_None*/)
{
	if (IsValid(QuestClass))
	{
		//If the quest is already in the players quest list issue a warning
		if (IsValid(GetQuest(QuestClass)))
		{
			UE_LOG(LogNarrative, Warning, TEXT("Narrative was asked to begin a quest the player is already doing. Use RestartQuest() to replay a started quest. "));
			return nullptr;
		}

		UQuest* NewQuest = NewObject<UQuest>(GetOwner(), QuestClass);

		const bool bInitializedSuccessfully = NewQuest->Initialize(this, StartFromID);

		if (bInitializedSuccessfully && NewQuest)
		{
			check(NewQuest->GetCurrentState());

			if (NewQuest->GetCurrentState())
			{
				QuestList.Add(NewQuest);

				//If loading from save file don't send update since server will batch all quests and send them to client to begin 
				if (!bIsLoading && HasAuthority() && GetNetMode() != NM_Standalone)
				{
					SendNarrativeUpdate(FNarrativeUpdate::BeginQuest(QuestClass, StartFromID));
				}

				return NewQuest;
			}
		}
	}
	return nullptr;
}

bool UNarrativeComponent::CompleteNarrativeTask_Internal(const FString& RawTaskString, const bool bFromReplication)
{
	if (GetOwnerRole() >= ROLE_Authority || bFromReplication)
	{
		/*We consider an Task relevant if it was completed for the first time ever or if it updated a quest. 
		Relevant Tasks get sent to the client for it to re-run, otherwise the server just saves network by not sending relevant Tasks*/
		bool bTaskWasRelevant = false;

		if (int32* TimesCompleted = MasterTaskList.Find(RawTaskString))
		{
			++(*TimesCompleted);
		}
		else
		{
			MasterTaskList.Add(RawTaskString, 1);
			bTaskWasRelevant = true;
		}

		bool bUpdatedQuest = false;

		//Use index based iteration as QuestList can change during iteration
		for (int32 i = 0; i < QuestList.Num(); ++i)
		{
			if (QuestList.IsValidIndex(i))
			{
				if (UQuest* QuestInProgress = QuestList[i])
				{
					if (UpdateQuest(QuestInProgress, RawTaskString) != EQuestProgress::QP_NoChange)
					{
						bTaskWasRelevant = true;
						bUpdatedQuest = true;
					}
				}
			}
		}

		//To keep things efficient, the server will only tell the client to replay Tasks that updated a quest
		if (GetOwnerRole() >= ROLE_Authority && GetNetMode() != NM_Standalone) // && bTaskWasRelevant) Task relevancy causing some issues with sync for now so disabled
		{
			//For some reason unreal crashes during replication if FNarrativeUpdate doesn't have a quest class supplied, so just send static class along with task string 
			SendNarrativeUpdate(FNarrativeUpdate::CompleteTask(UQuest::StaticClass(), RawTaskString)); 
		}

		return bUpdatedQuest;
	}
	return false;
}

EQuestProgress UNarrativeComponent::UpdateQuest(UQuest* QuestToUpdate, const FString& TaskString)
{
	if (QuestToUpdate)
	{
		const EQuestProgress Progress = QuestToUpdate->UpdateQuest(this, TaskString);

		switch (Progress)
		{
		case EQuestProgress::QP_Succeeded:
		{
			OnQuestSucceeded.Broadcast(QuestToUpdate, QuestToUpdate->GetCurrentState()->Description);
		}
		break;
		case EQuestProgress::QP_Failed:
		{
			OnQuestFailed.Broadcast(QuestToUpdate, QuestToUpdate->GetCurrentState()->Description);
		}
		break;
		}

		return Progress;
	}

	return EQuestProgress::QP_NoChange;
}

class UDialogue* UNarrativeComponent::MakeDialogue(TSubclassOf<class UDialogue> DialogueClass)
{
	if (IsValid(DialogueClass))
	{
		if (DialogueClass == UDialogue::StaticClass())
		{
			UE_LOG(LogNarrative, Warning, TEXT("UNarrativeComponent::MakeDialogue was passed UDialogue. Supplied Dialogue must be a child of UDialogue. "));
			return nullptr;
		}

		if (UDialogue* NewDialogue = NewObject<UDialogue>(GetOwner(), DialogueClass))
		{

			return NewDialogue;
		}
	}
	return nullptr;
}

bool UNarrativeComponent::HasCompletedTask(const UNarrativeTask* Task, const FString& Name, const int32 Quantity /*= 1 */)
{
	if (!Task)
	{
		return false;
	}

	return GetNumberOfTimesTaskWasCompleted(Task, Name) >= Quantity;
}

int32 UNarrativeComponent::GetNumberOfTimesTaskWasCompleted(const UNarrativeTask* Task, const FString& Name)
{
	if (!Task)
	{
		return 0;
	}

	if (int32* TimesCompleted = MasterTaskList.Find(Task->MakeTaskString(Name)))
	{
		return *TimesCompleted;
	}
	return 0;
}

bool UNarrativeComponent::HasCompletedTaskInQuest(TSubclassOf<class UQuest> QuestClass, const UNarrativeTask* Task, const FString& Name) const
{
	if (!Task || !IsValid(QuestClass))
	{
		return false;
	}

	const FString FormattedAction = Task->MakeTaskString(Name);

	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress && QuestInProgress->GetClass()->IsChildOf(QuestClass))
		{
			return QuestInProgress->QuestActivities.Contains(FormattedAction);
		}
	}

	return false;
}


void UNarrativeComponent::SendNarrativeUpdate(const FNarrativeUpdate& Update)
{
	PendingUpdateList.Add(Update);

	ensure(PendingUpdateList.Num());
	PendingUpdateList.Last().CreationTime = GetWorld()->GetTimeSeconds();

	//Remove stale updates from the pendingupdatelist, otherwise it might grow to be huge
	//TODO could keeping the update list history around be used to add more functionality? Its essentially a history of everything the player has done in order, could be valuable
	uint32 LatestStaleUpdateIdx = -1;
	bool bFoundStaleUpdates = false;
	const float UpdateStaleLimit = 30.f;

	if (PendingUpdateList.Num() > 1)
	{
		for (LatestStaleUpdateIdx = PendingUpdateList.Num() - 2; LatestStaleUpdateIdx > 0; --LatestStaleUpdateIdx)
		{
			if (PendingUpdateList.IsValidIndex(LatestStaleUpdateIdx) && GetWorld()->TimeSince(PendingUpdateList[LatestStaleUpdateIdx].CreationTime) > UpdateStaleLimit)
			{
				bFoundStaleUpdates = true;
				break;
			}
		}

		if (bFoundStaleUpdates)
		{
			PendingUpdateList.RemoveAt(0, LatestStaleUpdateIdx + 1);
			UE_LOG(LogNarrative, Verbose, TEXT("Found and removed %d stale updates. PendingUpdateList is now %d in size."), LatestStaleUpdateIdx + 1, PendingUpdateList.Num());
		}
	}
}

void UNarrativeComponent::OnRep_PendingUpdateList()
{
	//Process any updates the server has ran in the same order to ensure sync without having to replace a whole array of uquests 
	if (GetOwnerRole() < ROLE_Authority)
	{
		for (auto& Update : PendingUpdateList)
		{
			if (!Update.bAcked)
			{
				switch (Update.UpdateType)
				{
					case EUpdateType::UT_CompleteTask:
					{
						CompleteNarrativeTask_Internal(Update.Payload, true);
					}
					break;
					case EUpdateType::UT_BeginQuest:
					{
						BeginQuest(Update.QuestClass, FName(Update.Payload));
					}
					break;
					case EUpdateType::UT_RestartQuest:
					{
						RestartQuest(Update.QuestClass, FName(Update.Payload));
					}
					break;
					case EUpdateType::UT_ForgetQuest:
					{
						ForgetQuest(Update.QuestClass);
					}
					break;
				}

				Update.bAcked = true;
			}
		}

		//Clear the inputs out once we've processed them 
		PendingUpdateList.Empty();
	}
}

//void UNarrativeComponent::OnRep_CurrentDialogue()
//{
//	FString RoleString = HasAuthority() ? "Server" : "Client";
//
//	UE_LOG(LogTemp, Warning, TEXT("dialogue %s started on %s"), *GetNameSafe(CurrentDialogue), *RoleString);
//}
//
//void UNarrativeComponent::OnRep_QuestList()
//{
//	FString RoleString = HasAuthority() ? "Server"  : "Client";
//
//	for (auto& Quest : QuestList)
//	{
//		UE_LOG(LogTemp, Warning, TEXT("Quest on %s: %s"), *RoleString, *GetNameSafe(Quest));
//	}
//}

bool UNarrativeComponent::IsQuestValid(const UQuest* Quest, FString& OutError)
{
	
	if (!Quest)
	{
		OutError = "Narrative was given a null quest asset.";
		return false;
	}

	//This has already repped back to client and so if client is replaying narrative will complain about quest 
	//being started twice
	for (auto& QuestInProgress : QuestList)
	{
		if (QuestInProgress == Quest)
		{
			if (QuestInProgress->QuestCompletion == EQuestCompletion::QC_Succeded)
			{
				OutError = "Narrative was given a quest that has already been completed. Use RestartQuest to replay quests";
			}
			else if (QuestInProgress->QuestCompletion == EQuestCompletion::QC_Failed)
			{
				OutError = "Narrative was given a quest that has already been failed. Use RestartQuest to replay quests";
			}
			return false;
		}
	}

	TSet<FName> StateNames;
	for (auto State : Quest->States)
	{
		if (State->ID.IsNone())
		{
			OutError = FString::Printf(TEXT("Narrative was given a quest %s that has a state with no name. Please ensure all states are given names."), *Quest->GetQuestName().ToString());
			return false;
		}

		if (!StateNames.Contains(State->ID))
		{
			StateNames.Add(State->ID);
		}
		else
		{
			OutError = FString::Printf(TEXT("Narrative was given a quest %s that has more then one state with the same name. Please ensure all states have a unique name."), *Quest->GetQuestName().ToString());
			return false;
		}
	}
	return true;
}

void UNarrativeComponent::NarrativeTaskCompleted(const UNarrativeTask* NarrativeTask, const FString& Name)
{
	//If we have a shared narrative component, we should forward any tasks we complete to that component as well as this one
	if (SharedNarrativeComp)
	{
		SharedNarrativeComp->CompleteNarrativeTask(NarrativeTask, Name);
	}
}

void UNarrativeComponent::QuestStarted(const UQuest* Quest)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest started: %s"), *GetNameSafe(Quest));
	}
}

void UNarrativeComponent::QuestForgotten(const UQuest* Quest)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest forgotten: %s"), *GetNameSafe(Quest));
	}
}

void UNarrativeComponent::QuestFailed(const UQuest* Quest, const FText& QuestFailedMessage)
{
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest failed: %s. Failure state: %s"), *GetNameSafe(Quest), *QuestFailedMessage.ToString());
	}
}

void UNarrativeComponent::QuestSucceeded(const UQuest* Quest, const FText& QuestSucceededMessage)
{
	// No need to autosave on quest succeeded because QuestObjectiveCompleted already performs an autosave and is called on quest completion
	if (Quest)
	{
		UE_LOG(LogNarrative, Log, TEXT("Quest succeeded: %s. Succeeded state: %s"), *GetNameSafe(Quest), *QuestSucceededMessage.ToString());
	}
}

void UNarrativeComponent::QuestNewState(UQuest* Quest, const UQuestState* NewState)
{
	// No need to autosave on new objective because QuestObjectiveCompleted already performs an autosave and is called when we get a new objective
	if (Quest)
	{
		if (NewState)
		{
			UE_LOG(LogNarrative, Log, TEXT("Reached new state: %s in quest: %s"), *NewState->Description.ToString(), *GetNameSafe(Quest));

			/**
			* Certain features like retroactive tasks require that we try update quests immediately, but networked games need to wait for input
			* because we need to be certain the client and server components sync properly.
			*/
			if (GetNetMode() == NM_Standalone)
			{
				UpdateQuest(Quest);
			}
		}
	}
}

void UNarrativeComponent::QuestTaskProgressMade(const UQuest* Quest, const FQuestTask& ProgressedObjective, const class UQuestBranch* Step, int32 CurrentProgress, int32 RequiredProgress)
{

}

void UNarrativeComponent::QuestTaskCompleted(const UQuest* Quest, const FQuestTask& Task, const class UQuestBranch* Step)
{

}

void UNarrativeComponent::QuestStepCompleted(const UQuest* Quest, const class UQuestBranch* Step)
{

}

void UNarrativeComponent::BeginSave(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Begun saving using save name: %s"), *SaveName);
}

void UNarrativeComponent::BeginLoad(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Begun loading using save name: %s"), *SaveName);
}

void UNarrativeComponent::SaveComplete(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Save complete for save name: %s"), *SaveName);
}

void UNarrativeComponent::LoadComplete(FString SaveName)
{
	UE_LOG(LogNarrative, Verbose, TEXT("Load complete for save name: %s"), *SaveName);
}

void UNarrativeComponent::DialogueRepliesAvailable(class UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& PlayerReplies)
{

}

void UNarrativeComponent::DialogueLineStarted(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine)
{

}

void UNarrativeComponent::DialogueLineFinished(class UDialogue* Dialogue, UDialogueNode* Node, const FDialogueLine& DialogueLine)
{

}

void UNarrativeComponent::DialogueBegan(UDialogue* Dialogue)
{
	
}

void UNarrativeComponent::DialogueFinished(UDialogue* Dialogue)
{

}

APawn* UNarrativeComponent::GetOwningPawn() const
{

	if (OwnerPC)
	{
		return OwnerPC->GetPawn();
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	APawn* OwningPawn = Cast<APawn>(GetOwner());

	if (OwningPawn)
	{
		return OwningPawn;
	}

	if (!OwningPawn && OwningController)
	{
		return OwningController->GetPawn();
	}

	return nullptr;
}

APlayerController* UNarrativeComponent::GetOwningController() const
{
	//We cache this on beginplay as to not re-find it every time 
	if (OwnerPC)
	{
		return OwnerPC;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	APawn* OwningPawn = Cast<APawn>(GetOwner());

	if (OwningController)
	{
		return OwningController;
	}

	if (!OwningController && OwningPawn)
	{
		return Cast<APlayerController>(OwningPawn->GetController());
	}

	return nullptr;
}

TArray<UQuest*> UNarrativeComponent::GetFailedQuests() const
{
	TArray<UQuest*> FailedQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Failed)
		{
			FailedQuests.Add(QIP);
		}
	}
	return FailedQuests;
}


TArray<UQuest*> UNarrativeComponent::GetSucceededQuests() const
{
	TArray<UQuest*> SucceededQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Succeded)
		{
			SucceededQuests.Add(QIP);
		}
	}
	return SucceededQuests;
}

TArray<UQuest*> UNarrativeComponent::GetInProgressQuests() const
{
	TArray<UQuest*> InProgressQuests;
	for (auto QIP : QuestList)
	{
		if (QIP->QuestCompletion == EQuestCompletion::QC_Started)
		{
			InProgressQuests.Add(QIP);
		}
	}
	return InProgressQuests;
}

TArray<UQuest*> UNarrativeComponent::GetAllQuests() const
{
	return QuestList;
}

class UQuest* UNarrativeComponent::GetQuest(TSubclassOf<class UQuest> QuestClass) const
{
	for (auto& QIP : QuestList)
	{
		if (QIP && QIP->GetClass()->IsChildOf(QuestClass))
		{
			return QIP;
		}
	}
	return nullptr;
}

bool UNarrativeComponent::Save(const FString& SaveName/** = "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	OnBeginSave.Broadcast(SaveName);
	
	if (UNarrativeSaveGame* NarrativeSaveGame = Cast<UNarrativeSaveGame>(UGameplayStatics::CreateSaveGameObject(UNarrativeSaveGame::StaticClass())))
	{
		NarrativeSaveGame->MasterTaskList = MasterTaskList;

		for (auto& Quest : QuestList)
		{
			if (Quest)
			{
				FNarrativeSavedQuest Save;
				Save.QuestClass = Quest->GetClass();
				Save.CurrentStateID = Quest->GetCurrentState()->ID;

				//Store all the branches in the save file - used to be a more efficient TMap but due to replication we cant do this
				for (UQuestBranch* Branch : Quest->Branches)
				{
					Save.CurrentStateBranchIDs.Add(Branch->ID);
					Save.CurrentStateBranchData.Add(FSavedQuestBranch(Branch->Tasks));
				}

				//Store all the reached states in the save file
				for (UQuestState* State : Quest->ReachedStates)
				{
					Save.ReachedStateNames.Add(State->ID);
				}

				NarrativeSaveGame->SavedQuests.Add(Save);
			}
		}

		if (UGameplayStatics::SaveGameToSlot(NarrativeSaveGame, SaveName, Slot))
		{
			OnSaveComplete.Broadcast(SaveName);
			return true;
		}
	}
return false;
}

bool UNarrativeComponent::Load(const FString& SaveName/** = "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveName, 0))
	{
		return false;
	}

	if (UNarrativeSaveGame* NarrativeSaveGame = Cast<UNarrativeSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveName, Slot)))
	{
		OnBeginLoad.Broadcast(SaveName);

		Load_Internal(NarrativeSaveGame->SavedQuests, NarrativeSaveGame->MasterTaskList);

		//We've loaded on the server, we need to send all the load data to the client so it is synced 
		if (HasAuthority() && GetNetMode() != NM_Standalone)
		{
			//Send the unpacked save file to the client - need to split MasterTaskList into 2 arrays since TMaps arent networked
			TArray<FString> Tasks;
			TArray<int32> Quantities;
			MasterTaskList.GenerateKeyArray(Tasks);
			MasterTaskList.GenerateValueArray(Quantities);

			ClientReceiveSave(NarrativeSaveGame->SavedQuests, Tasks, Quantities);
		}

		OnLoadComplete.Broadcast(SaveName);
		return true;
	}
	return false;
}

bool UNarrativeComponent::DeleteSave(const FString& SaveName /*= "NarrativeSaveData"*/, const int32 Slot/** = 0*/)
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveName, 0))
	{
		return false;
	}

	return UGameplayStatics::DeleteGameInSlot(SaveName, Slot);
}

void UNarrativeComponent::ClientReceiveSave_Implementation(const TArray<FNarrativeSavedQuest>& SavedQuests, const TArray<FString>& Tasks, const TArray<int32>& Quantities)
{
	ensure(Tasks.Num() == Quantities.Num());

	//For security reasons its probably best to not pass the save file names to clients
	FString DummyString = "ServerInvokedLoad";
	OnBeginLoad.Broadcast(DummyString);

	//Server sent the TMap as two seperate arrays, reconstruct
	TMap<FString, int32> RemadeTaskList;
	int32 Idx = 0;
	for (auto& Task : Tasks)
	{
		ensure(Quantities.IsValidIndex(Idx));

		RemadeTaskList.Add(Task, Quantities[Idx]);
		++Idx;
	}

	Load_Internal(SavedQuests, RemadeTaskList);

	//For a client save name should be irrelevant so dont bother sending via RPC 
	OnLoadComplete.Broadcast(DummyString);

}

bool UNarrativeComponent::Load_Internal(const TArray<FNarrativeSavedQuest>& SavedQuests, const TMap<FString, int32>& NewMasterList)
{
	bIsLoading = true;

	QuestList.Empty();
	MasterTaskList.Empty();

	MasterTaskList = NewMasterList;

	for (auto& SaveQuest : SavedQuests)
	{
		//Restore the quest from the last state we were in 
		if (UQuest* BegunQuest = BeginQuest_Internal(SaveQuest.QuestClass, SaveQuest.CurrentStateID))
		{
			//Restore all quest branches - this is messy because our TMap design had to be changed into TArrays because TMaps cant replicate 
			if (UQuestState* CurrentState = BegunQuest->GetCurrentState())
			{
				//TODO We are restoring progress on current states branches, but not previous quest branches. If we ever loop back to them their progress will be lost, fix this
				for (auto& Branch : CurrentState->Branches)
				{
					check(Branch);

					int32 BranchIdx = 0;

					if (Branch)
					{
						//Find the saved branch and restore the tasks progress
						for (auto& SavedBranchID : SaveQuest.CurrentStateBranchIDs)
						{
							if (SaveQuest.CurrentStateBranchData.IsValidIndex(BranchIdx))
							{
								//We found the saved branch! Now restore all progress on the tasks.
								if (Branch->ID == SavedBranchID)
								{
									for (auto& Task : Branch->Tasks)
									{
										for (auto& SavedTask : SaveQuest.CurrentStateBranchData[BranchIdx].Tasks)
										{
											//If the saved task has same task asset and argument then assume we found the one since 
											if (SavedTask.Task == Task.Task && SavedTask.Argument == Task.Argument)
											{
												Task.CurrentProgress = SavedTask.CurrentProgress;
												break;
											}
										}
									}
									break;
								}
							}
						}
					}

					++BranchIdx;
				}
			} 

			//Restore all reached states
			TArray<UQuestState*> AllReachedStates;

			//Remove the last state, it will have already been added by BeginQuest_Internal
			BegunQuest->ReachedStates.Empty();

			for (const FName StateName : SaveQuest.ReachedStateNames)
			{
				BegunQuest->ReachedStates.Add(BegunQuest->GetState(StateName));
			}
		}
	}

	//FString RoleString = HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogTemp, Warning, TEXT("LOADINTERNAL ON %s"), *RoleString);

	//for (auto& Quest : QuestList)
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("Quest %s current state %s"), *GetNameSafe(Quest), *GetNameSafe(Quest->GetCurrentState()));

	//	for (auto& Branch : Quest->GetCurrentState()->Branches)
	//	{
	//		UE_LOG(LogTemp, Warning, TEXT("Current state %s branch %s TASKS:"), *GetNameSafe(Quest), *GetNameSafe(Quest->GetCurrentState()), *GetNameSafe(Branch));

	//		for (auto& Task : Branch->Tasks)
	//		{
	//			UE_LOG(LogTemp, Warning, TEXT("Task %s %s progress %d/%d"), *GetNameSafe(Task.Task), *Task.Argument, Task.CurrentProgress, Task.Quantity);
	//		}
	//	}
	//}

	bIsLoading = false;

	return true;
}

void UNarrativeComponent::SetSharedNarrativeComponent(UNarrativeComponent* NewSharedNarrativeComponent)
{
	SharedNarrativeComp = NewSharedNarrativeComponent;
}


