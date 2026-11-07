// SunsetCoveViewUtils.h
// SunsetCove - Core area. Small helpers shared by systems that aim from the player's view (interaction, placement, throw).
//
// Header only. Nothing here depends on a gameplay system.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace SunsetCoveView
{
	/**
	 * Camera location and rotation of the player that drives Owner. For a pawn with a controller this asks the controller,
	 * which returns the camera that is actually rendered (the managed camera of CoreFramework included). Anything else
	 * falls back to the eye point of the actor.
	 */
	inline void GetOwnerViewPoint(const AActor* Owner, FVector& OutLocation, FRotator& OutRotation)
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;

		if (!Owner)
		{
			return;
		}

		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (const AController* Controller = Pawn->GetController())
			{
				Controller->GetPlayerViewPoint(OutLocation, OutRotation);
				return;
			}
		}

		Owner->GetActorEyesViewPoint(OutLocation, OutRotation);
	}
}
