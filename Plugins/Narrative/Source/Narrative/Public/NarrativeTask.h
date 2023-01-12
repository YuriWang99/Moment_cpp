// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NarrativeTask.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class NARRATIVE_API UNarrativeTask : public UDataAsset
{
	GENERATED_BODY()

public:

	UNarrativeTask(const FObjectInitializer& ObjectInitializer);

	/**A short name describing what this Task is*/
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FString TaskName;

	/**A description of this task. Will get used as a tooltip in the quest editor so write something useful!  */
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FText TaskDescription;

	/**The name of the argument this tasks takes (For example if your Task is called "Talk To Character", the argument name might be "Character Name")*/
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FString ArgumentName;

	/**The category of this Task, used for organization in the quest tool*/
	UPROPERTY(EditAnywhere, Category = "Task Details")
	FString TaskCategory;

	/**Default argument to autofill */
	UPROPERTY(EditAnywhere, Category = "Autofill")
	FString DefaultArgument;

	/**Default description to autofill */
	UPROPERTY(EditAnywhere, Category = "Autofill")
	FString AutofillDescription;

	/**Destination state willhave its ID filled out with this */
	UPROPERTY(EditAnywhere, Category = "Autofill")
	FName DefaultNextStateID;

	/**Destination state willhave its description filled out with this */
	UPROPERTY(EditAnywhere, Category = "Autofill")
	FText DefaultNextStateDescription;

	/**Convert the task to a raw string that the quest state machines can use. This will just take the task name (i.e "TalkToCharacter"), and append the Argument with an underscore. (i.e "TalkToCharacter_Bob")*/
	FString MakeTaskString(const FString& Argument) const;

	FText GetReferenceDisplayText();

};
