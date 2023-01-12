// Copyright Narrative Tools 2022. 


#include "NarrativeEvent.h"


UWorld* UNarrativeEvent::GetWorld() const
{
	if (UObject* OuterObj = GetOuter())
	{
		return OuterObj->GetWorld();
	}

	return nullptr;
}

bool UNarrativeEvent::ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{
	return true;
}

FString UNarrativeEvent::GetGraphDisplayText_Implementation()
{
	return FString();
}
