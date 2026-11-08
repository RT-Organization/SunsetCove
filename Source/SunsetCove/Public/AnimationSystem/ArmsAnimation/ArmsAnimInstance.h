// ArmsAnimInstance.h
// SunsetCove - Animation System / Arms Animation. The parent class of the Animation Blueprint that layers items over the arms.
//
// WHAT THIS CLASS IS
//   The graph variables of ABP_FPArms_Items, and nothing else. It has no logic and does no work per frame: UArmsAnimationComponent
//   writes the variables when an item enters or leaves a hand (an event), and the AnimGraph reads them.
//
// WHICH ANIMATION BLUEPRINT USES IT
//   ABP_FPArms_Items, a POST-PROCESS Animation Blueprint of the arms mesh: it runs after the base arms Animation Blueprint
//   (ABP_FPArms_Base, a child of the framework UAnimationInstance, untouched) and receives its pose through the Input Pose node.
//   That is why this class derives from plain UAnimInstance: it only layers poses and needs none of the locomotion variables.
//   The component installs it on the arms mesh by itself (UArmsAnimationComponent::PostProcessAnimClass).
//
// THE GRAPH THAT READS THESE VARIABLES (details in Docs/AnimationSystem/Engine_Setup_Guide.md)
//   Input Pose -> cached pose "Base"
//   Layered Blend per Bone: base "Base"; layer 0 = right arm, layer 1 = left arm (the branch filter bone of each layer is Arm_R / Arm_L).
//   Layer for one arm = Slot (ArmItem_R / ArmItem_L)  <-  Blend Poses by EArmHeldPose (RightArmPose / LeftArmPose):
//       None -> "Base" | Hold -> Sequence Player(HoldSequence, loop) | PlaceAim -> Sequence Player(PlaceAimSequence, loop)
//       ThrowCharge -> Sequence Player(ThrowChargeSequence, NO loop: it stays on its last frame)
//   One-shot animations (grab, throw...) are montages that the component plays in the slot of the arm.
//
// THE RULE THAT KEEPS THE GRAPH SAFE
//   A pose other than None is only ever set together with a non-null sequence for that pose (a Sequence Player without a sequence
//   shows the reference pose). The component guarantees it in one place (SetArm).

#pragma once

#include "CoreMinimal.h"
#include "AnimationSystem/ArmsAnimation/ArmsAnimationTypes.h"
#include "Animation/AnimInstance.h"
#include "ArmsAnimInstance.generated.h"

class UAnimSequence;

UCLASS()
class SUNSETCOVE_API UArmsAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The held pose of the right arm. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Right")
	EArmHeldPose RightArmPose = EArmHeldPose::None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Right")
	TObjectPtr<UAnimSequence> RightHoldSequence;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Right")
	TObjectPtr<UAnimSequence> RightPlaceAimSequence;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Right")
	TObjectPtr<UAnimSequence> RightThrowChargeSequence;

	/** The held pose of the left arm. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Left")
	EArmHeldPose LeftArmPose = EArmHeldPose::None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Left")
	TObjectPtr<UAnimSequence> LeftHoldSequence;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Left")
	TObjectPtr<UAnimSequence> LeftPlaceAimSequence;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "SunsetCove|ArmsItems|Left")
	TObjectPtr<UAnimSequence> LeftThrowChargeSequence;

	/**
	 * Sets the pose and the three sequences of an arm. A pose without its sequence is downgraded (ThrowCharge and PlaceAim fall back to
	 * Hold, Hold falls back to None) and the sequences that are missing are filled with the Hold one, so the graph never plays nothing.
	 * Returns the pose that was really applied.
	 */
	EArmHeldPose SetArm(EArmSide Side, EArmHeldPose Pose, UAnimSequence* Hold, UAnimSequence* PlaceAim, UAnimSequence* ThrowCharge);

	/** The pose an arm shows now. */
	EArmHeldPose GetArmPose(EArmSide Side) const { return Side == EArmSide::Right ? RightArmPose : LeftArmPose; }
};
