// AnimNotify_ArmsItemCommit.h
// SunsetCove - Animation System / Arms Animation. The notify that says "NOW the item moves".
//
// HOW TO USE IT
//   Open an arms animation (AS_Arms_Sunbed_Grab_R...), right-click the Notifies track > Add Notify > "Arms Item Commit", and place it on
//   the frame where the item must attach, detach, appear or be launched (the animator writes that frame in the delivery notes).
//   Then set the clip's Commit Mode to "On Notify" in DA_ArmsAnimations. With any other mode the notify is ignored.
//
// WHAT IT DOES
//   It tells UArmsAnimationComponent, which tells the inventory that is waiting (CommitPendingAction). If nobody is waiting it does
//   nothing, so it is harmless on a clip that is played from the console or in the animation editor.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ArmsItemCommit.generated.h"

UCLASS(meta = (DisplayName = "Arms Item Commit"))
class SUNSETCOVE_API UAnimNotify_ArmsItemCommit : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_ArmsItemCommit();

	//~ Begin UAnimNotify Interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify Interface
};
