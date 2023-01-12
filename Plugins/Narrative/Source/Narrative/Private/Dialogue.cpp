// Copyright Narrative Tools 2022. 

#include "Dialogue.h"
#include "Net/UnrealNetwork.h"
#include "NarrativeComponent.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "DialogueSM.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeDialogueSettings.h"

UDialogue::UDialogue()
{
	FMovieSceneSequenceLoopCount InfiniteLooping;
	InfiniteLooping.Value = -1;

	PlayerTalkingSettings.bPauseAtEnd = true;
	PlayerTalkingSettings.LoopCount = InfiniteLooping;
	SelectReplySettings.bPauseAtEnd = true;
	SelectReplySettings.LoopCount = InfiniteLooping;

	bCanBeExited = true;
}

UWorld* UDialogue::GetWorld() const
{

	if (OwningComp)
	{
		return OwningComp->GetWorld();
	}

	return nullptr;
}

bool UDialogue::Initialize(class UNarrativeComponent* InitializingComp, class AActor* NPC, FName StartFromID)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		//We need a valid narrative component to make a quest for 
		if (!InitializingComp)
		{
			return false;
		}

		if (UDialogueBlueprintGeneratedClass* BGClass = Cast<UDialogueBlueprintGeneratedClass>(GetClass()))
		{
			BGClass->InitializeDialogue(this);

			//If a dialogue doesn't have any npc replies, or doesn't have a valid root dialogue something has gone wrong 
			if (NPCReplies.Num() == 0 || !RootDialogue)
			{
				UE_LOG(LogNarrative, Warning, TEXT("UDialogue::Initialize was given a dialogue with an invalid RootDialogue, or no NPC replies."));
				return false;
			}

			UDialogueNode_NPC* StartDialogue = StartFromID.IsNone() ? RootDialogue : GetNPCReplyByID(StartFromID);

			if (!StartDialogue && !StartFromID.IsNone())
			{
				UE_LOG(LogNarrative, Warning, TEXT("UDialogue::Initialize could not find Start node with StartFromID: %s. Falling back to root node."), *StartFromID.ToString());
				StartDialogue = RootDialogue;
			}

			//Initialize all the data required to begin the dialogue 
			if (StartDialogue)
			{
				OwningComp = InitializingComp;
				NPCActor = NPC;
				OwningController = OwningComp->GetOwningController();
				OwningPawn = OwningComp->GetOwningPawn();

				if (RootDialogue)
				{
					RootDialogue->OwningDialogue = this;
				}

				for (auto& Reply : NPCReplies)
				{
					if (Reply)
					{
						Reply->OwningDialogue = this;
						Reply->OwningComponent = OwningComp;
					}
				}

				for (auto& Reply : PlayerReplies)
				{
					if (Reply)
					{
						Reply->OwningDialogue = this;
						Reply->OwningComponent = OwningComp;
					}
				}

				//Generate the first chunk of dialogue 
				if (OwningComp->HasAuthority())
				{
					const bool bHasValidDialogue = GenerateDialogueChunk(StartDialogue);

					if (!bHasValidDialogue)
					{
						return false;
					}
				}

				OnBeginDialogue();
				return true;
			}
		}
	}

	return false;
}

void UDialogue::Deinitialize()
{
	OnEndDialogue();

	StopDialogueSequence();

	if (DialogueAudio)
	{
		DialogueAudio->Stop();
		DialogueAudio->DestroyComponent();
	}

	if (DialogueSequencePlayer)
	{
		DialogueSequencePlayer->Destroy();
	}

	NPCActor = nullptr;

	//Make sure the dialogue doesn't reference anything
	OwningComp = nullptr;

	if (RootDialogue)
	{
		RootDialogue->OwningComponent = nullptr;
	}

	for (auto& Reply : NPCReplies)
	{
		if (Reply)
		{
			Reply->OwningComponent = nullptr;
		}
	}

	for (auto& Reply : PlayerReplies)
	{
		if (Reply)
		{
			Reply->OwningComponent = nullptr;
		}
	}
}

void UDialogue::DuplicateAndInitializeFromDialogue(UDialogue* DialogueTemplate)
{
	if (DialogueTemplate)
	{
		//Duplicate the quest template, then steal all its states and branches - TODO this seems unreliable, what if we add new fields to UDialogue? Look into swapping object entirely instead of stealing fields
		UDialogue* NewDialogue = Cast<UDialogue>(StaticDuplicateObject(DialogueTemplate, this, NAME_None, RF_Transactional));
		NewDialogue->SetFlags(RF_Transient | RF_DuplicateTransient);

		RootDialogue = NewDialogue->RootDialogue;
		NPCReplies = NewDialogue->NPCReplies;
		PlayerReplies = NewDialogue->PlayerReplies;
	}
}

#if WITH_EDITOR
void UDialogue::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	//If a designer clears the speakers always ensure at least one is added 
	if (Speakers.Num() == 0)
	{
		FSpeakerInfo DefaultSpeaker;
		DefaultSpeaker.DefaultShot = nullptr;
		DefaultSpeaker.SpeakerID = GetFName();
		Speakers.Add(DefaultSpeaker);
	}

	//If any NPC replies don't have a valid speaker set to the first speaker
	for (auto& Node : NPCReplies)
	{
		if (Node)
		{
			bool bSpeakerNotFound = true;

			for (auto& Speaker : Speakers)
			{
				if (Speaker.SpeakerID == Node->SpeakerID)
				{
					bSpeakerNotFound = false;
				}
			}

			if (bSpeakerNotFound)
			{
				Node->SpeakerID = Speakers[0].SpeakerID;
			}
		}
	}
}

void UDialogue::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);


}

#endif

FSpeakerInfo UDialogue::GetSpeaker(const FName& SpeakerID)
{
	for (auto& Speaker : Speakers)
	{
		if (Speaker.SpeakerID == SpeakerID)
		{
			return Speaker;
		}
	}

	//Nodes created before the release of Speakers won't have their speaker set. Therefore, just return the first speaker.
	if (Speakers.Num() && Speakers.IsValidIndex(0))
	{
		return Speakers[0];
	}

	return FSpeakerInfo();
}

void UDialogue::SkipCurrentLine()
{
	//Only allow skipping lines in standalone 
	if (OwningComp && OwningComp->GetNetMode() == NM_Standalone)
	{
		//Skip whatever dialogue line is currently playing
		if (CurrentNode)
		{
			if (CurrentNode->IsA<UDialogueNode_NPC>())
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
				FinishNPCDialogue();
			}
			else
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_PlayerReplyFinished);
				FinishPlayerDialogue();
			}
		}
	}
}

void UDialogue::SelectDialogueOption(UDialogueNode_Player* Option)
{
	//Validate that the option that was selected is actually one of the available options
	if (AvailableResponses.Contains(Option) && Option->AreConditionsMet(OwningPawn, OwningController, OwningComp))
	{
		PlayPlayerDialogueNode(Option);
	}
}

bool UDialogue::HasValidChunk() const
{
	//Validate that the reply chain isn't just full of empty routing nodes and actually has some content to play
	bool bReplyChainHasContent = false;

	for (auto& Reply : NPCReplyChain)
	{
		if (Reply && Reply->HasText())
		{
			bReplyChainHasContent = true;
			break;
		}
	}

	//Chunk is valid if there is something for the player to say, or something for the NPC to say that isn't empty (empty nodes can be used for routing)
	//At this point we have a valid chunk and Play() can be called on this dialogue to play it
	if (AvailableResponses.Num() || bReplyChainHasContent)
	{
		return true;
	}

	return false;
}

bool UDialogue::GenerateDialogueChunk(UDialogueNode_NPC* NPCNode)
{
	if (NPCNode && OwningComp && OwningComp->HasAuthority())
	{
		//Generate the NPC reply chain
		NPCReplyChain = NPCNode->GetReplyChain(OwningController, OwningPawn, OwningComp);

		//Grab all the players responses to the last thing the NPC had to say
		if (NPCReplyChain.Num() && NPCReplyChain.IsValidIndex(NPCReplyChain.Num() - 1))
		{
			if (UDialogueNode_NPC* LastNPCNode = NPCReplyChain.Last())
			{
				AvailableResponses = LastNPCNode->GetPlayerReplies(OwningController, OwningPawn, OwningComp);
			}
		}

		//Did we generate a valid chunk?
		if (HasValidChunk())
		{
			return true;
		}
	}

	return false;
}

void UDialogue::ClientReceiveDialogueChunk(const TArray<FName>& NPCReplyIDs, const TArray<FName>& PlayerReplyIDs)
{	
	if (OwningComp && !OwningComp->HasAuthority())
	{
		//Resolve the nodes the server sent us 
		NPCReplyChain = GetNPCRepliesByIDs(NPCReplyIDs);
		AvailableResponses = GetPlayerRepliesByIDs(PlayerReplyIDs);

		//Server ensures chunks are valid before sending them to us. If they aren't something has gone very wrong
		check(HasValidChunk());

		Play();
	}
}

void UDialogue::Play()
{
	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("Playing %d nodes on %s"), NPCReplyChain.Num(), *RoleString);
	//Start playing through the NPCs replies until we run out
	if (NPCReplyChain.Num())
	{
		PlayNextNPCReply();
	}
}

void UDialogue::ExitDialogue()
{
	if (OwningComp)
	{
		OwningComp->ExitDialogue();
	}
}

void UDialogue::NPCFinishedTalking()
{
	//Ensure that a narrative event etc hasn't started a new dialogue
	if (AvailableResponses.Num() && OwningComp && OwningComp->CurrentDialogue == this)
	{
		//Play the select responses shot
		if (OwningComp && SelectReplyShot)
		{
			PlayDialogueShot(SelectReplyShot, SelectReplySettings);
		}

		//NPC has finished talking. Let UI know it can show the player replies.
		OwningComp->OnDialogueRepliesAvailable.Broadcast(this, AvailableResponses);

		//Also make sure we stop playing any dialogue audio that was previously playing
		if (DialogueAudio)
		{
			DialogueAudio->Stop();
			DialogueAudio->DestroyComponent();
		}
	}
	else
	{
		//There were no replies for the player, end the dialogue 
		ExitDialogue();
	}
}

void UDialogue::PlayNPCDialogueNode(class UDialogueNode_NPC* NPCReply)
{
	check(OwningComp && NPCReply);

	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("PlayNPCDialogueNode called on %s with node %s"), *RoleString, *GetNameSafe(NPCReply));

	if (NPCReply)
	{
		CurrentNode = NPCReply;

		CurrentLine = NPCReply->GetRandomLine();
		ReplaceStringVariables(CurrentLine.Text);

		CurrentSpeaker = GetSpeaker(NPCReply->SpeakerID);

		//Node has no text, must be used for routing, so go to the next node
		if (NPCReply->Text.IsEmptyOrWhitespace())
		{
			PlayNextNPCReply();
			return;
		}

		//Actual playing of the node is inside a BlueprintNativeEvent so designers can override how NPC dialogues are played 
		PlayNPCDialogue(NPCReply, CurrentLine, CurrentSpeaker);

		//Call delegates and BPNativeEvents
		OwningComp->OnNPCDialogueLineStarted.Broadcast(this, NPCReply, CurrentLine, CurrentSpeaker);
		OnNPCDialogueLineStarted(NPCReply, CurrentLine, CurrentSpeaker);

		const float Duration = GetLineDuration(CurrentNode, CurrentLine);

		if (Duration > 0.01f)
		{
			GetWorld()->GetTimerManager().ClearTimer(TimerHandle_NPCReplyFinished);
			//Give the reply time to play, then play the next one! 
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_NPCReplyFinished, this, &UDialogue::FinishNPCDialogue, Duration, false);
		}
		else
		{
			FinishNPCDialogue();
		}
	}
	else 
	{
		//Somehow we were given a null NPC reply to play, just try play the next one
		PlayNextNPCReply();
	}
}

void UDialogue::PlayPlayerDialogueNode(class UDialogueNode_Player* PlayerReply)
{
	//NPC replies should be fully gone before we play a player response
	check(!NPCReplyChain.Num());
	check(OwningComp && PlayerReply);

	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("PlayPlayerDialogueNode called on %s with node %s"), *RoleString, *GetNameSafe(PlayerReply));

	if (PlayerReply)
	{
		CurrentNode = PlayerReply;
		
		CurrentLine = PlayerReply->GetRandomLine();
		ReplaceStringVariables(CurrentLine.Text);

		//Call delegates and BPNativeEvents
		OwningComp->OnPlayerDialogueLineStarted.Broadcast(this, PlayerReply, CurrentLine);
		OnPlayerDialogueLineStarted(PlayerReply, CurrentLine);

		//Actual playing of the node is inside a BlueprintNativeEvent so designers can override how NPC dialogues are played 
		PlayPlayerDialogue(PlayerReply, CurrentLine);

		const float Duration = GetLineDuration(CurrentNode, CurrentLine);
		if (Duration > 0.01f)
		{
			//Give the reply time to play, then play the next one! 
			GetWorld()->GetTimerManager().SetTimer(TimerHandle_PlayerReplyFinished, this, &UDialogue::FinishPlayerDialogue, Duration, false);
		}
		else
		{
			FinishPlayerDialogue();
		}
	}
}

void UDialogue::ReplaceStringVariables(FText& Line)
{
	//Replace variables in dialogue line
	FString LineString = Line.ToString();

	int32 OpenBraceIdx = -1;
	int32 CloseBraceIdx = -1;
	bool bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
	bool bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);
	uint32 Iters = 0; // More than 50 wildcard replaces and something has probably gone wrong, so safeguard against that

	while (bFoundOpenBrace && bFoundCloseBrace && OpenBraceIdx < CloseBraceIdx && Iters < 50)
	{
		const FString VariableName = LineString.Mid(OpenBraceIdx + 1, CloseBraceIdx - OpenBraceIdx - 1);
		const FString VariableVal = GetStringVariable(CurrentNode, CurrentLine, VariableName);

		if (!VariableVal.IsEmpty())
		{
			LineString.RemoveAt(OpenBraceIdx, CloseBraceIdx - OpenBraceIdx + 1);
			LineString.InsertAt(OpenBraceIdx, VariableVal);
		}

		bFoundOpenBrace = LineString.FindChar('{', OpenBraceIdx);
		bFoundCloseBrace = LineString.FindChar('}', CloseBraceIdx);

		Iters++;
	}

	if (Iters > 0)
	{
		Line = FText::FromString(LineString);
	}
}

void UDialogue::Tick_Implementation(const float DeltaTime)
{
}

void UDialogue::PlayNextNPCReply()
{
	//Keep going through the NPC replies until we run out
	if (NPCReplyChain.IsValidIndex(0))
	{
		UDialogueNode_NPC* NPCNode = NPCReplyChain[0];
		NPCReplyChain.Remove(NPCNode);
		PlayNPCDialogueNode(NPCNode);
	}
	else //NPC has nothing left to say 
	{
		NPCFinishedTalking();
	}
}

void UDialogue::FinishNPCDialogue()
{
	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("FinishNPCDialogue called on %s with node %s"), *RoleString, *GetNameSafe(CurrentNode));

	if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(CurrentNode))
	{
		if (OwningComp)
		{
			if (NPCNode->AreConditionsMet(OwningPawn, OwningController, OwningComp))
			{
				OwningComp->CompleteNarrativeTask("PlayDialogueNode", NPCNode->ID.ToString());

				NPCNode->ProcessEvents(OwningPawn, OwningController, OwningComp);
			}

			//We need to re-check OwningComp validity, as ProcessEvents may have ended this dialogue
			if (OwningComp)
			{
				//Call delegates and BPNativeEvents
				OwningComp->OnNPCDialogueLineFinished.Broadcast(this, NPCNode, CurrentLine, CurrentSpeaker);
				OnNPCDialogueLineFinished(NPCNode, CurrentLine, CurrentSpeaker);

				PlayNextNPCReply();
			}
		}
	}
}

void UDialogue::FinishPlayerDialogue()
{
	//FString RoleString = OwningComp && OwningComp->HasAuthority() ? "Server" : "Client";
	//UE_LOG(LogNarrative, Warning, TEXT("FinishPlayerDialogue called on %s with node %s"), *RoleString, *GetNameSafe(CurrentNode));
	//Players dialogue node has finished, generate the next chunk of dialogue! 
	if (UDialogueNode_Player* PlayerNode = Cast<UDialogueNode_Player>(CurrentNode))
	{
		if (!OwningComp || !PlayerNode)
		{
			UE_LOG(LogNarrative, Warning, TEXT("UDialogue::PlayerDialogueNodeFinished was called but had a null OwningComp or PlayerNode. "));
			return;
		}

		//Call delegates and BPNativeEvents
		OwningComp->OnPlayerDialogueLineFinished.Broadcast(this, PlayerNode, CurrentLine);
		OnPlayerDialogueLineFinished(PlayerNode, CurrentLine);

		if (PlayerNode->AreConditionsMet(OwningPawn, OwningController, OwningComp))
		{
			//Both auth and local need to run the events
			PlayerNode->ProcessEvents(OwningPawn, OwningController, OwningComp);

			if (OwningComp && OwningComp->HasAuthority())
			{
				OwningComp->CompleteNarrativeTask("PlayDialogueNode", PlayerNode->ID.ToString());

				//Player selected a reply with nothing leading off it, dialogue has ended 
				if (PlayerNode->NPCReplies.Num() <= 0)
				{
					ExitDialogue();
					return;
				}

				//Find the first valid NPC reply after the option we selected. TODO: Use NPC replies Y-pos in the graph to prioritize order of check
				UDialogueNode_NPC* NextReply = nullptr;

				for (auto& NextNPCReply : PlayerNode->NPCReplies)
				{
					if (NextNPCReply->AreConditionsMet(OwningPawn, OwningController, OwningComp))
					{
						NextReply = NextNPCReply;
						break;
					}
				}

				//If we can generate more dialogue from the reply that was selected, do so, otherwise exit dialogue 
				if (GenerateDialogueChunk(NextReply))
				{
					//RPC the dialogue chunk to the client so it can play it
					OwningComp->ClientRecieveDialogueChunk(MakeIDsFromNPCNodes(NPCReplyChain), MakeIDsFromPlayerNodes(AvailableResponses));

					Play();
				}
				else
				{
					UE_LOG(LogNarrative, Warning, TEXT("No more chunks generated from response. Ending dialogue! "));
					ExitDialogue();
				}
			}
		}

	}
}

UDialogueNode_NPC* UDialogue::GetNPCReplyByID(const FName& ID) const
{
	for (auto& NPCReply : NPCReplies)
	{
		if (NPCReply->ID == ID)
		{
			return NPCReply;
		}
	}
	return nullptr;
}

UDialogueNode_Player* UDialogue::GetPlayerReplyByID(const FName& ID) const
{
	for (auto& PlayerReply : PlayerReplies)
	{
		if (PlayerReply->ID == ID)
		{
			return PlayerReply;
		}
	}
	return nullptr;
}

TArray<UDialogueNode_NPC*> UDialogue::GetNPCRepliesByIDs(const TArray<FName>& IDs) const
{
	TArray<UDialogueNode_NPC*> Replies;

	for (auto& ID : IDs)
	{
		for (auto& Reply : NPCReplies)
		{
			if (Reply && Reply->ID == ID)
			{
				Replies.Add(Reply);
				break;
			}
		}
	}

	return Replies;
}

TArray <UDialogueNode_Player*> UDialogue::GetPlayerRepliesByIDs(const TArray<FName>& IDs) const
{
	TArray<UDialogueNode_Player*> Replies;

	for (auto& ID : IDs)
	{
		for (auto& Reply : PlayerReplies)
		{
			if (Reply && Reply->ID == ID)
			{
				Replies.Add(Reply);
				break;
			}
		}
	}

	return Replies;
}

TArray<FName> UDialogue::MakeIDsFromNPCNodes(const TArray<UDialogueNode_NPC*> Nodes) const
{
	TArray<FName> IDs;

	for (auto& Node : Nodes)
	{
		IDs.Add(Node->ID);
	}

	return IDs;
}

TArray<FName> UDialogue::MakeIDsFromPlayerNodes(const TArray<UDialogueNode_Player*> Nodes) const
{
	TArray<FName> IDs;

	for (auto& Node : Nodes)
	{
		IDs.Add(Node->ID);
	}

	return IDs;
}

TArray<UDialogueNode*> UDialogue::GetNodes() const
{
	TArray<UDialogueNode*> Ret;

	for (auto& NPCReply : NPCReplies)
	{
		Ret.Add(NPCReply);
	}

	for (auto& PlayerReply : PlayerReplies)
	{
		Ret.Add(PlayerReply);
	}

	return Ret;
}

void UDialogue::PlayDialogueAnimation_Implementation(class UDialogueNode* Node, const FDialogueLine& Line)
{
	//Are we playing an animation on the player, or the NPC? 
	const bool bIsNPCAnim = Node->IsA<UDialogueNode_NPC>();
	AActor* ActorToUse = bIsNPCAnim ? NPCActor : OwningPawn;

	if (Line.DialogueMontage && ActorToUse)
	{
		if (bIsNPCAnim)
		{
			//For NPC replies, we'll play the montage on every mesh with the speakers ID added to it as a tag.
			TArray<UActorComponent*> Meshes = ActorToUse->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), CurrentSpeaker.SpeakerID);

			for (auto& Mesh : Meshes)
			{
				if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Mesh))
				{
					if (SkelMeshComp->GetAnimInstance())
					{
						SkelMeshComp->GetAnimInstance()->Montage_Play(Line.DialogueMontage);
						break;
					}
				}
			}

			//If we found some meshes with the tag then exit out - we don't want to play more anims
			if (Meshes.Num())
			{
				return;
			}
		}



		//Use characters anim montage player otherwise use generic
		if (ACharacter* Char = Cast<ACharacter>(ActorToUse))
		{
			if (Char)
			{
				Char->PlayAnimMontage(Line.DialogueMontage);
			}
		}
		else if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(ActorToUse->GetComponentByClass(USkeletalMeshComponent::StaticClass())))
		{
			if (SkelMeshComp->GetAnimInstance())
			{
				SkelMeshComp->GetAnimInstance()->Montage_Play(Line.DialogueMontage);
			}
		}

	}
}

void UDialogue::PlayDialogueSound_Implementation(const FDialogueLine& Line)
{
	//Stop the existing audio regardless of whether the new line has audio
	if (DialogueAudio)
	{
		DialogueAudio->Stop();
		DialogueAudio->DestroyComponent();
	}

	if (Line.DialogueSound)
	{
		//If this dialogue was supplied an NPC actor we should play the sound at the location of that actor
		if (NPCActor)
		{
			//In order to play spatialized audio, look for a skeletal mesh component on the NPC actor with the speakers tag added 
			TArray<UActorComponent*> Meshes = NPCActor->GetComponentsByTag(USkeletalMeshComponent::StaticClass(), CurrentSpeaker.SpeakerID);

			if (Meshes.Num())
			{
				for (auto& Mesh : Meshes)
				{
					DialogueAudio = UGameplayStatics::SpawnSoundAtLocation(OwningComp, Line.DialogueSound, NPCActor->GetActorLocation(), NPCActor->GetActorForwardVector().Rotation());
					break;
				}
			}
			else
			{
				DialogueAudio = UGameplayStatics::SpawnSoundAtLocation(OwningComp, Line.DialogueSound, NPCActor->GetActorLocation(), NPCActor->GetActorForwardVector().Rotation());
			}
		}
		else //Else just play 2D audio 
		{
			DialogueAudio = UGameplayStatics::SpawnSound2D(OwningComp, Line.DialogueSound);
		}
	}
}

void UDialogue::PlayNPCDialogue_Implementation(class UDialogueNode_NPC* NPCReply, const FDialogueLine& LineToPlay, const FSpeakerInfo& SpeakerInfo)
{
	PlayDialogueSound(LineToPlay);
	PlayDialogueAnimation(NPCReply, LineToPlay);

	//If the dialogue line has a shot defined, play that. Otherwise, just play the speakers default shot
	if (LineToPlay.Shot)
	{
		PlayDialogueShot(LineToPlay.Shot, LineToPlay.ShotSettings);
	}
	else if (SpeakerInfo.DefaultShot)
	{
		PlayDialogueShot(SpeakerInfo.DefaultShot, FMovieSceneSequencePlaybackSettings());
	}
	else
	{
		StopDialogueSequence();
	}
}

void UDialogue::PlayPlayerDialogue_Implementation(class UDialogueNode_Player* PlayerReply, const FDialogueLine& Line)
{
	PlayDialogueSound(Line);
	PlayDialogueAnimation(PlayerReply, Line);

	//If the dialogue line has a shot defined, play that. Otherwise, just play the default player shot
	if (Line.Shot)
	{
		PlayDialogueShot(Line.Shot, Line.ShotSettings);
	}
	else if (PlayerTalkingShot)
	{
		PlayDialogueShot(PlayerTalkingShot, PlayerTalkingSettings);
	}
}

float UDialogue::GetLineDuration_Implementation(class UDialogueNode* Node, const FDialogueLine& Line)
{
	/*
	* By default, we wait until the Dialogue Audio is finished, or if no audio is supplied
	* we wait at a rate of 25 letters per second (configurable in .ini) to give the reader time to finish reading.
	*/
	if (Line.DialogueSound)
	{
		return Line.DialogueSound->GetDuration();
	}
	else
	{
		float LettersPerMinute = 25.f;
		float MinDialogueTextDisplayTime = 2.f;

		if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
		{
			LettersPerMinute = DialogueSettings->LettersPerMinute;
			MinDialogueTextDisplayTime = DialogueSettings->MinDialogueTextDisplayTime;
		}

		return FMath::Max(Line.Text.ToString().Len() / LettersPerMinute, MinDialogueTextDisplayTime);
	}
}

FString UDialogue::GetStringVariable_Implementation(class UDialogueNode* Node, const FDialogueLine& Line, const FString& VariableName)
{
	return VariableName;
}

void UDialogue::OnNPCDialogueLineStarted_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{

}

void UDialogue::OnNPCDialogueLineFinished_Implementation(class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{

}

void UDialogue::OnPlayerDialogueLineStarted_Implementation(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{

}

void UDialogue::OnPlayerDialogueLineFinished_Implementation(class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{

}

void UDialogue::PlayDialogueShot(class ULevelSequence* Shot, const FMovieSceneSequencePlaybackSettings& Settings)
{
	if (Shot && OwningController && OwningController->IsLocalPlayerController())
	{
		//We're trying to play a dialogue shot that is already playing, check if the settings don't allow this
		if (DialogueSequencePlayer && DialogueSequencePlayer->GetSequence() == Shot)
		{
			if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
			{
				if (!DialogueSettings->bRestartDialogueSequenceIfAlreadyPlaying)
				{
					return;
				}
			}
		}

		//Stop any currently running sequence 
		StopDialogueSequence();

		//Narrative needs to initialize its cutscene player 
		if (!DialogueSequencePlayer)
		{
			ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Shot, Settings, DialogueSequencePlayer);
		}
		else if (DialogueSequencePlayer && DialogueSequencePlayer->SequencePlayer)
		{
			FLevelSequenceCameraSettings CamSettings;
			DialogueSequencePlayer->PlaybackSettings = Settings;
			DialogueSequencePlayer->SetSequence(Shot);
		}

		if (DialogueSequencePlayer)
		{
			if (DialogueSequencePlayer && DialogueSequencePlayer->SequencePlayer)
			{
				DialogueSequencePlayer->SequencePlayer->Play();
			}
		}

	}
}
void UDialogue::StopDialogueSequence()
{
	if (OwningController && OwningController->IsLocalPlayerController())
	{
		if (DialogueSequencePlayer && DialogueSequencePlayer->SequencePlayer)
		{
			DialogueSequencePlayer->SequencePlayer->Stop();
		}
	}
}
