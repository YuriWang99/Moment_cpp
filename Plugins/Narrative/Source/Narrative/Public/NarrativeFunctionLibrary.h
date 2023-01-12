// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NarrativeFunctionLibrary.generated.h"

/**
 * General functions used by narrative 
 */
UCLASS()
class NARRATIVE_API UNarrativeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/**
	* Grab the narrative component from the local pawn or player controller, whichever it exists on. 
	* 
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeComponent* GetNarrativeComponent(const UObject* WorldContextObject);

	/**
	* Find the narrative component from the supplied target object. 
	*
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (DefaultToSelf = "Target"))
	static class UNarrativeComponent* GetNarrativeComponentFromTarget(AActor* Target);

	/**
	* Find the narrative component from the supplied target object.
	*
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (DefaultToSelf = "Target"))
		static class UNarrativeComponent* GetSharedNarrativeComponentFromTarget(AActor* Target);
	/**
	* Calls CompleteNarrativeTask on the narrative component
	*
	* @return Whether the task updated a quest 
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative", meta = (DisplayName = "Complete Narrative Task", BlueprintInternalUseOnly = "true"))
	static bool CompleteNarrativeTask(class UNarrativeComponent* Target, const class UNarrativeTask* Task, const FString& Argument);

	//Grab a narrative task by its name. Try use asset references instead of this if possible, since an task being renamed will break your code
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeTask* GetTaskByName(const UObject* WorldContextObject, const FString& EventName);

};
