// ArmsAnimInstance.cpp
// SunsetCove - Animation System / Arms Animation. See Public/AnimationSystem/ArmsAnimation/ArmsAnimInstance.h

#include "AnimationSystem/ArmsAnimation/ArmsAnimInstance.h"

#include "Animation/AnimSequence.h"

EArmHeldPose UArmsAnimInstance::SetArm(EArmSide Side, EArmHeldPose Pose, UAnimSequence* Hold, UAnimSequence* PlaceAim, UAnimSequence* ThrowCharge)
{
	// The graph never gets a pose whose sequence is missing (a Sequence Player without a sequence shows the reference pose).
	if (Pose == EArmHeldPose::ThrowCharge && !ThrowCharge)
	{
		Pose = EArmHeldPose::Hold;
	}
	if (Pose == EArmHeldPose::PlaceAim && !PlaceAim)
	{
		Pose = EArmHeldPose::Hold;
	}
	if (Pose == EArmHeldPose::Hold && !Hold)
	{
		Pose = EArmHeldPose::None;
	}

	// Poses that are not selected still get a valid sequence (the Hold one), so a blend into them never starts from nothing.
	UAnimSequence* SafePlaceAim = PlaceAim ? PlaceAim : Hold;
	UAnimSequence* SafeThrowCharge = ThrowCharge ? ThrowCharge : Hold;

	if (Side == EArmSide::Right)
	{
		RightArmPose = Pose;
		RightHoldSequence = Hold;
		RightPlaceAimSequence = SafePlaceAim;
		RightThrowChargeSequence = SafeThrowCharge;
	}
	else
	{
		LeftArmPose = Pose;
		LeftHoldSequence = Hold;
		LeftPlaceAimSequence = SafePlaceAim;
		LeftThrowChargeSequence = SafeThrowCharge;
	}

	return Pose;
}
