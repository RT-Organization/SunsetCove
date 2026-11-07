// InteractionComponent.cpp
// SunsetCove - Interaction System. See Public/InteractionSystem/InteractionComponent.h for the role of this class.

#include "InteractionSystem/InteractionComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveViewUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "Interfaces/InteractableInterface.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	TraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Handlers.Reset();
	FocusedActor.Reset();

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Only the machine that owns the view aims. Everything else has no focus.
	if (!IsOwnerLocallyControlled())
	{
		SetFocusedActor(nullptr);
		return;
	}

	TimeSinceFocusUpdate += DeltaTime;
	if (FocusUpdateInterval > 0.0f && TimeSinceFocusUpdate < FocusUpdateInterval)
	{
		return;
	}
	TimeSinceFocusUpdate = 0.0f;

	SetFocusedActor(FindInteractionTarget());
}

// ----------------------------------------------------------------------
// Wiring
// ----------------------------------------------------------------------

void UInteractionComponent::SetupInput(UEnhancedInputComponent* EnhancedInput)
{
	if (!EnhancedInput)
	{
		return;
	}

	if (!InteractAction)
	{
		if (!bWarnedMissingAction)
		{
			bWarnedMissingAction = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("Interaction: '%s' has no Interact Action. Set it on the Interaction component of the character Blueprint (Interaction|Input > Interact Action). The player cannot interact until it is set."),
				*GetNameSafe(GetOwner()));
		}
		return;
	}

	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &UInteractionComponent::HandleInteractInput);
}

void UInteractionComponent::RegisterInteractionHandler(FName HandlerId, const FInteractionCanHandleDelegate& CanHandle, const FInteractionHandleDelegate& Handle)
{
	if (HandlerId.IsNone())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Interaction: a handler needs a name. Registration ignored."));
		return;
	}

	FInteractionHandler NewHandler;
	NewHandler.CanHandle = CanHandle;
	NewHandler.Handle = Handle;

	// Same id again replaces the old handler and keeps its place in the order.
	for (TPair<FName, FInteractionHandler>& Entry : Handlers)
	{
		if (Entry.Key == HandlerId)
		{
			Entry.Value = NewHandler;
			return;
		}
	}

	Handlers.Emplace(HandlerId, NewHandler);
}

void UInteractionComponent::UnregisterInteractionHandler(FName HandlerId)
{
	Handlers.RemoveAll([HandlerId](const TPair<FName, FInteractionHandler>& Entry)
	{
		return Entry.Key == HandlerId;
	});
}

// ----------------------------------------------------------------------
// Commands and queries
// ----------------------------------------------------------------------

void UInteractionComponent::HandleInteractInput()
{
	Interact();
}

bool UInteractionComponent::Interact()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// A fresh trace, not the cached focus: the press must act on what the player aims at right now.
	AActor* Target = FindInteractionTarget();
	SetFocusedActor(Target);
	if (!Target)
	{
		return false;
	}

	bool bHandled = false;

	// 1. The target brings its own behavior.
	if (Target->Implements<UInteractableInterface>() && !IInteractableInterface::Execute_IsInteractionBlocked(Target, Owner))
	{
		IInteractableInterface::Execute_Interact(Target, Owner);
		bHandled = true;
	}

	// 2. A registered system deals with it. Iterate a copy: a handler may change the registry.
	if (!bHandled)
	{
		const TArray<TPair<FName, FInteractionHandler>> HandlersCopy = Handlers;
		for (const TPair<FName, FInteractionHandler>& Entry : HandlersCopy)
		{
			const FInteractionHandler& Handler = Entry.Value;
			if (Handler.CanHandle.IsBound() && Handler.Handle.IsBound() && Handler.CanHandle.Execute(Target))
			{
				if (Handler.Handle.Execute(Target))
				{
					bHandled = true;
					break;
				}
			}
		}
	}

	OnInteracted.Broadcast(Target, bHandled);

	// The interaction may have changed what is interactable (a grabbed prop is gone from the world).
	SetFocusedActor(FindInteractionTarget());

	return bHandled;
}

FText UInteractionComponent::GetFocusedPrompt() const
{
	AActor* Focus = FocusedActor.Get();
	if (Focus && Focus->Implements<UInteractableInterface>())
	{
		return IInteractableInterface::Execute_GetInteractionPrompt(Focus);
	}
	return FText::GetEmpty();
}

// ----------------------------------------------------------------------
// Internals
// ----------------------------------------------------------------------

AActor* UInteractionComponent::FindInteractionTarget() const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}

	FVector Start;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, Start, ViewRotation);
	const FVector End = Start + ViewRotation.Vector() * InteractionRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionTrace), /*bTraceComplex=*/false, Owner);
	const ECollisionChannel Channel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FHitResult Hit;
	const bool bHit = (TraceRadius > KINDA_SMALL_NUMBER)
		? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(TraceRadius), Params)
		: World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params);

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	AActor* Result = IsTargetInteractable(HitActor) ? HitActor : nullptr;

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FColor Color = Result ? FColor::Green : (bHit ? FColor::Orange : FColor::Red);
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, Color, false, FMath::Max(FocusUpdateInterval, 0.02f), 0, 0.5f);
		if (bHit)
		{
			DrawDebugSphere(World, Hit.ImpactPoint, FMath::Max(TraceRadius, 2.0f), 8, Color, false, FMath::Max(FocusUpdateInterval, 0.02f));
		}
	}
#endif

	return Result;
}

bool UInteractionComponent::IsTargetInteractable(AActor* Target) const
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (Target->Implements<UInteractableInterface>() && !IInteractableInterface::Execute_IsInteractionBlocked(Target, GetOwner()))
	{
		return true;
	}

	for (const TPair<FName, FInteractionHandler>& Entry : Handlers)
	{
		if (Entry.Value.CanHandle.IsBound() && Entry.Value.CanHandle.Execute(Target))
		{
			return true;
		}
	}

	return false;
}

void UInteractionComponent::SetFocusedActor(AActor* NewFocus)
{
	AActor* OldFocus = FocusedActor.Get();
	if (OldFocus == NewFocus)
	{
		return;
	}

	FocusedActor = NewFocus;
	OnFocusChanged.Broadcast(NewFocus, OldFocus);
}

bool UInteractionComponent::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}
