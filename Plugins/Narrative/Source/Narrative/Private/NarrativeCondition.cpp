// Copyright Narrative Tools 2022. 


#include "NarrativeCondition.h"


UWorld* UNarrativeCondition::GetWorld() const
{
	if (UObject* OuterObj = GetOuter())
	{
		return OuterObj->GetWorld();
	}

	return nullptr;
}

bool UNarrativeCondition::CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{
	return true;
}

FString UNarrativeCondition::GetGraphDisplayText_Implementation()
{
	return FString();
}
