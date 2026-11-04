// SunsetCoveNPCAnimInstance.cpp
// SunsetCove - Character area. See Public/Character/SunsetCoveNPCAnimInstance.h for the role of this class.

#include "Character/SunsetCoveNPCAnimInstance.h"

#include "Character/SunsetCoveNPCCharacter.h"

void USunsetCoveNPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	CachedNPC = Cast<ASunsetCoveNPCCharacter>(TryGetPawnOwner());
}

void USunsetCoveNPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// The framework copies its own snapshot first (locomotion, state machine tags).
	Super::NativeUpdateAnimation(DeltaSeconds);

	const ASunsetCoveNPCCharacter* NPC = CachedNPC.Get();
	if (!NPC)
	{
		// The pawn can be set after initialization (preview scenes, possession changes).
		CachedNPC = Cast<ASunsetCoveNPCCharacter>(TryGetPawnOwner());
		NPC = CachedNPC.Get();
	}

	ActivityTag = NPC ? NPC->GetActivityTag() : FGameplayTag();
	bHasActivity = ActivityTag.IsValid();
}
