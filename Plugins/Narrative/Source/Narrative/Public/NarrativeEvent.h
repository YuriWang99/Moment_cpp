// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeEvent.generated.h"

/**
 * Subclass this in BP to execute some logic when a particular quest step or dialogue option is reached  */
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVE_API UNarrativeEvent : public UObject
{
	GENERATED_BODY()

public:

	virtual UWorld* GetWorld() const override;

	/**
	If true, run this event on the players shared narrative comp instead of their local one
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	bool bUseSharedComponent = false;

	UFUNCTION(BlueprintNativeEvent, Category = "Event")
	bool ExecuteEvent(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);
	virtual bool ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);

	/**Define the text that will show up on a node if this condition is added to it */
	UFUNCTION(BlueprintNativeEvent, Category = "Event")
	FString GetGraphDisplayText();
	virtual FString GetGraphDisplayText_Implementation();
};
