// Copyright Epic Games, Inc. All Rights Reserved.

#include "Moment_cppGameMode.h"
#include "Moment_cppCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMoment_cppGameMode::AMoment_cppGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
