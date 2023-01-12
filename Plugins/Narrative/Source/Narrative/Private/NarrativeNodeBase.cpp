// SurvivalGame Project - The Unreal C++ Survival Game Course - Copyright Reuben Ward 2020


#include "NarrativeNodeBase.h"
#include "NarrativeCondition.h"
#include "NarrativeEvent.h"
#include "NarrativeComponent.h"

UNarrativeNodeBase::UNarrativeNodeBase()
{
	//autofill the ID
	ID = GetFName();
}

void UNarrativeNodeBase::ProcessEvents(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{
	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running events on node %s but Narrative Comp was null."), *GetNameSafe(this));
	}

	for (auto& Event : Events)
	{
		if (Event)
		{
			UNarrativeComponent* CompToUse = Event->bUseSharedComponent && NarrativeComponent->SharedNarrativeComp ? NarrativeComponent->SharedNarrativeComp : NarrativeComponent;

			if (!CompToUse && Event->bUseSharedComponent)
			{
				UE_LOG(LogNarrative, Warning, TEXT("Event %s on node %s uses bUseSharedComponent but SharedNarrativeComp was null. Falling back to local Narrative comp..."), *GetNameSafe(Event), *GetNameSafe(this));
				CompToUse = NarrativeComponent;
			}

			if (CompToUse)
			{
				if (Event)
				{
					Event->ExecuteEvent(Pawn, Controller, CompToUse);
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Tried running event %s on node %s but Narrative Comp was null."), *GetNameSafe(Event), *GetNameSafe(this));
			}
		}
	}
}

bool UNarrativeNodeBase::AreConditionsMet(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent)
{

	if (!NarrativeComponent)
	{
		UE_LOG(LogNarrative, Warning, TEXT("Tried running conditions on node %s but Narrative Comp was null."), *GetNameSafe(this));
	}

	//Ensure all conditions are met
	for (auto& Cond : Conditions)
	{	
		if (Cond)
		{
			
			UNarrativeComponent* CompToCheck = Cond->bCheckSharedComponent && NarrativeComponent->SharedNarrativeComp ? NarrativeComponent->SharedNarrativeComp : NarrativeComponent;

			if (!CompToCheck && Cond->bCheckSharedComponent)
			{
				UE_LOG(LogNarrative, Warning, TEXT("Condition %s on node %s uses bCheckSharedComponent but SharedNarrativeComp was null. Falling back to local Narrative comp..."), *GetNameSafe(Cond), *GetNameSafe(this));
				CompToCheck = NarrativeComponent;
			}

			if (CompToCheck)
			{
				if (Cond && Cond->CheckCondition(Pawn, Controller, CompToCheck) == Cond->bNot)
				{
					return false;
				}
			}
			else
			{
				UE_LOG(LogNarrative, Warning, TEXT("Tried running condition %s on node %s but Narrative Comp was null."), *GetNameSafe(Cond), *GetNameSafe(this));
			}
		}
	}

	return true;
}
