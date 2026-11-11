// SunsetCoveNPCAnimInstance.h
// SunsetCove - Character area. The parent class of every NPC Animation Blueprint (ABP_Visitor, later ABP_Police...).
//
// WHAT THIS CLASS IS
//   The animation contract between NPC gameplay and the NPC models that do not exist yet. It adds one thing to the
//   framework's UAnimationInstance: the NPC activity (ASunsetCoveNPCCharacter::ActivityTag, NPC.Activity.*), copied
//   every frame in a thread-safe way. Everything else comes from the framework, exactly as for the player:
//     Locomotion  Domain / GroundState / WaterState, GroundSpeed, bShouldMove... from the StateMachine tags.
//     Activity    ActivityTag here: resting sitting, resting lying, later swimming at the shore, fishing, chatting.
//
// HOW AN NPC ANIMATION BLUEPRINT USES IT
//   Create the Animation Blueprint for the NPC skeleton with this class as parent. In the AnimGraph: a top state
//   machine with "Locomotion" (the framework variables, like ABP_SunsetCoveCharacter) and "Activity" states entered
//   with HasActivity(NPC.Activity.Rest.Lay)... in the transition rules. The game
//   code never changes when the models arrive: set Mesh and Anim Class in BP_Visitor.
//
// HIERARCHY
//   UAnimInstance -> UAnimationInstance (CoreFramework) -> USunsetCoveNPCAnimInstance (this class) -> ABP_<Npc>
//
// THREADING (same contract as the framework)
//   NativeUpdateAnimation [game thread] copies ActivityTag from the pawn. The AnimGraph and the thread-safe update read
//   only the members of this object.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationInstance.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveNPCAnimInstance.generated.h"

class ASunsetCoveNPCCharacter;

UCLASS()
class SUNSETCOVE_API USunsetCoveNPCAnimInstance : public UAnimationInstance
{
	GENERATED_BODY()

public:
	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	//~ End UAnimInstance Interface

	/** The NPC activity (NPC.Activity.*), or an empty tag while it only walks or stands. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|NPC")
	FGameplayTag ActivityTag;

	/** True while the NPC has any activity (it is resting on a spot...). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|NPC")
	bool bHasActivity = false;

	/** Thread-safe test for transition rules. Hierarchical: NPC.Activity.Rest matches Sit and Lay. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|NPC", meta = (BlueprintThreadSafe))
	bool HasActivity(FGameplayTag Activity) const { return ActivityTag.IsValid() && ActivityTag.MatchesTag(Activity); }

private:
	TWeakObjectPtr<ASunsetCoveNPCCharacter> CachedNPC;
};
