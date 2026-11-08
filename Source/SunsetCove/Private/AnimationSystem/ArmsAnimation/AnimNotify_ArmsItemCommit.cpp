// AnimNotify_ArmsItemCommit.cpp
// SunsetCove - Animation System / Arms Animation. See Public/AnimationSystem/ArmsAnimation/AnimNotify_ArmsItemCommit.h

#include "AnimationSystem/ArmsAnimation/AnimNotify_ArmsItemCommit.h"

#include "AnimationSystem/ArmsAnimation/ArmsAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

UAnimNotify_ArmsItemCommit::UAnimNotify_ArmsItemCommit()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(255, 170, 40, 255);
#endif
}

void UAnimNotify_ArmsItemCommit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// No owner in the animation editor preview: nothing to do.
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UArmsAnimationComponent* ArmsAnimation = Owner ? Owner->FindComponentByClass<UArmsAnimationComponent>() : nullptr)
	{
		ArmsAnimation->HandleCommitNotify(Animation);
	}
}

FString UAnimNotify_ArmsItemCommit::GetNotifyName_Implementation() const
{
	return TEXT("Arms Item Commit");
}
