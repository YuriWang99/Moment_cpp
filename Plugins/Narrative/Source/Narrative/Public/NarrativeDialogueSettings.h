// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeDialogueSettings.generated.h"

/**
 * Runtime dialogue settings for narrative 
 */
UCLASS(config = Engine, defaultconfig)
class NARRATIVE_API UNarrativeDialogueSettings : public UObject
{
	GENERATED_BODY()
	
public:

	UNarrativeDialogueSettings();

	//How long should the text be displayed for at a minimum? Since default letters per minute is 25 this prevents a reply like "no" from being played too quickly
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin=0.01))
	float MinDialogueTextDisplayTime;

	//If a dialogue doesn't have audio supplied, how long should the text be displayed on the screen for? Lower letters per minute means player gets more time 
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings", meta = (ClampMin = 1))
	float LettersPerMinute;
	
	//When enabled if narrative tries playing a dialogue shot but that shot is already playing, it will restart the shot. Otherwise, it will just let the already started shot continue. 
	UPROPERTY(EditAnywhere, config, Category = "Dialogue Settings")
	bool bRestartDialogueSequenceIfAlreadyPlaying;

};
