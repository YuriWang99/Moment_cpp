// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeCondition.generated.h"

/**
 * Subclass this in blueprint to conditionally include or exclude dialogue/quest options 
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVE_API UNarrativeCondition : public UObject
{
	GENERATED_BODY()

public:

	virtual UWorld* GetWorld() const override;

	/** Check whether this condition is true or false*/
	UFUNCTION(BlueprintNativeEvent, Category = "Conditions")
	bool CheckCondition(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);
	virtual bool CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, class UNarrativeComponent* NarrativeComponent);

	/**Define the text that will show up on a node if this condition is added to it */
	UFUNCTION(BlueprintNativeEvent, Category = "Conditions")
	FString GetGraphDisplayText();
	virtual FString GetGraphDisplayText_Implementation();

	/**
	If true, narrative will check this condition on the shared component instead of the local one. 
	If this is true but no shared component exists narrative will revert back to the local one, and will warn you in the log
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conditions")
	bool bCheckSharedComponent = false;

	//Set this to true to flip the result of this condition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conditions")
	bool bNot = false;

};
