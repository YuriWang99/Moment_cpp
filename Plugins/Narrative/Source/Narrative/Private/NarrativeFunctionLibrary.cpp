// Copyright Narrative Tools 2022. 


#include "NarrativeFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeComponent.h"
#include "NarrativeTaskManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"

class UNarrativeComponent* UNarrativeFunctionLibrary::GetNarrativeComponent(const UObject* WorldContextObject)
{
	return GetNarrativeComponentFromTarget(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
}

class UNarrativeComponent* UNarrativeFunctionLibrary::GetNarrativeComponentFromTarget(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	if (UNarrativeComponent* NarrativeComp = Target->FindComponentByClass<UNarrativeComponent>())
	{
		return NarrativeComp;
	}

	//Narrative comp may be on the controllers pawn or pawns controller
	if (APlayerController* OwningController = Cast<APlayerController>(Target))
	{
		if (OwningController->GetPawn())
		{
			return OwningController->GetPawn()->FindComponentByClass<UNarrativeComponent>();
		}
	}

	if (APawn* OwningPawn = Cast<APawn>(Target))
	{
		if (OwningPawn->GetController())
		{
			return OwningPawn->GetController()->FindComponentByClass<UNarrativeComponent>();
		}
	}

	return nullptr;
}


class UNarrativeComponent* UNarrativeFunctionLibrary::GetSharedNarrativeComponentFromTarget(AActor* Target)
{

	if (UNarrativeComponent* NarrativeComp = GetNarrativeComponentFromTarget(Target))
	{
		return NarrativeComp->SharedNarrativeComp;
	}

	return nullptr;
}

bool UNarrativeFunctionLibrary::CompleteNarrativeTask(class UNarrativeComponent* Target, const UNarrativeTask* Task, const FString& Argument)
{
	if (Target)
	{
		return Target->CompleteNarrativeTask(Task, Argument);
	}
	return false;
}

class UNarrativeTask* UNarrativeFunctionLibrary::GetTaskByName(const UObject* WorldContextObject, const FString& EventName)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		if (UNarrativeTaskManager* EventManager = GI->GetSubsystem<UNarrativeTaskManager>())
		{
			return EventManager->GetTask(EventName);
		}
	}

	return nullptr;
}
