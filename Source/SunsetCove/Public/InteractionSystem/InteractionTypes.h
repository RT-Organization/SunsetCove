// InteractionTypes.h
// SunsetCove - Interaction System. The delegate types a system uses to plug into the interaction component.
//
// Plain C++ delegates (not Blueprint delegates): handlers are registered by C++ systems, for example the Prop Inventory.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

class AActor;

/** Can this handler interact with Target? Called while the player aims, so keep it cheap and free of side effects. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractionCanHandleDelegate, AActor* /*Target*/);

/** Interact with Target. Return true if the interaction was done (false lets the next handler try). */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractionHandleDelegate, AActor* /*Target*/);

/** The two delegates of one registered handler. */
struct FInteractionHandler
{
	FInteractionCanHandleDelegate CanHandle;
	FInteractionHandleDelegate Handle;
};
