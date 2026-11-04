// SunsetCoveNPCCharacter.h
// SunsetCove - Character area. The body of every humanoid NPC: visitors today, police, entrepreneur and bodyguards later.
//
// WHAT THIS CLASS IS
//   The body half of an NPC. It walks, turns, sits down and lies down; it never decides anything. The decisions come
//   from behavior components on the same actor (for example USunsetCoveVisitorComponent), and ASunsetCoveAIController
//   turns "go there" into path following on the navmesh. On the Miro NPC board this class is the "Acting" layer.
//
// HIERARCHY
//   ACharacter -> ABaseCharacter (CoreFramework) -> ASunsetCoveNPCCharacter (this class) -> BP_Visitor (Blueprint)
//   It is not a child of ASunsetCoveCharacter: that class is the player (input, first person). NPCs share the
//   framework with the player, not the player's game logic.
//
// WHAT COMES FROM THE FRAMEWORK, AND WHAT IS SWITCHED OFF
//   Kept: the Ground / Fly / Water movement component (visitors will swim later) and the StateMachine tags that
//   UAnimationInstance-based Animation Blueprints read, so NPC and player animation share one pipeline.
//   Switched off in the constructor, because nobody looks through an NPC and nobody steers it with a keyboard:
//     - CameraManager does not build its spring arm and camera.
//     - The perspective is locked to third person, so the body turns toward where it walks.
//     - Flying is off.
//   Path following drives the movement component through acceleration ("Use Acceleration For Paths"). The framework
//   StateMachine and UAnimationInstance read "is moving" from the acceleration, exactly as for player input; with the
//   engine default (velocity requests) a walking NPC would be reported as idle.
//
// NAVIGATION CONTRACT (WHY THE MISSING NPC MODELS DO NOT BLOCK NAVIGATION WORK)
//   Navigation only ever sees the capsule. The default capsule (radius 34 cm, half height 88 cm: a 1.76 m adult, the
//   framework default) matches the "Humanoid" navmesh agent in DefaultEngine.ini. A future adult model keeps this
//   capsule: only Mesh, its Anim Class and the mesh offset change, in the Blueprint, with no code change.
//   A smaller body (a child) gets its own Blueprint with a smaller capsule and still fits the same navmesh.
//
// ACTIVITY (WHAT THE BODY DOES BEYOND LOCOMOTION)
//   ActivityTag (NPC.Activity.*) says "resting, sitting", "resting, lying"... Behaviors set it; the animation layer
//   (USunsetCoveNPCAnimInstance) and the placeholder body read it. Locomotion (idle, walk, swim) stays in the framework
//   StateMachine and keeps being driven by the movement component, as for the player.
//
// RESTING
//   BeginRest takes the body off the navmesh and blends it onto a rest transform (a sunbed, a towel), then sets the
//   activity. EndRest blends it back to a standing point and gives navigation back. While resting the movement
//   component is off (MOVE_None) and the capsule ignores other pawns, so nobody bumps into an invisible standing capsule
//   above a lying NPC. Walking NPCs still steer around it through crowd avoidance.
//
// PLACEHOLDER BODY (UNTIL NPC MODELS EXIST)
//   PlaceholderRoot holds two plain mesh components (a cylinder and a cone in BP_Visitor) that show where the NPC is and
//   which way it faces. Each NPC gets a random colour from PlaceholderColors, and PlaceholderPoses moves the placeholder
//   for each activity (lying, sitting). The placeholder hides itself when Mesh has a skeletal mesh, so a real model
//   needs no code change. Delete the three components once every NPC Blueprint has a model.
//
// SETUP (BP_Visitor, Class Defaults)
//   PlaceholderBody: Static Mesh = Cylinder, PlaceholderNose: Static Mesh = Cone (engine BasicShapes), both with the
//   engine BasicShapeMaterial (its "Color" parameter receives the random colour).
//   SunsetCove|NPC|Placeholder > Placeholder Poses: one entry per NPC.Activity tag the NPC can use.
//   Character Movement > Max Walk Speed: the stroll speed (constructor default 150 cm/s).
//
// NETWORK
//   Single player game, but the body follows the framework: the AI runs where the pawn has authority and ActivityTag
//   replicates, so nothing here assumes one machine.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveNPCCharacter.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** The activity of an NPC changed. NewActivity is empty when the NPC is back to plain locomotion. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSunsetCoveNPCActivityChangedSignature, FGameplayTag, NewActivity, FGameplayTag, OldActivity);

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveNPCCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	/** Passes the initializer to the framework, which replaces the movement component class. Do not drop the parameter. */
	ASunsetCoveNPCCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Activity
	// ------------------------------------------------------------------

	/** Sets what the body is doing (an NPC.Activity tag). An empty tag means plain locomotion. Call it with authority. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|NPC")
	void SetActivityTag(FGameplayTag NewActivity);

	/** The current activity (NPC.Activity.*), or an empty tag. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|NPC")
	FGameplayTag GetActivityTag() const { return ActivityTag; }

	/** Fires on every machine when the activity changes. Bind icons, sounds or effects here. */
	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|NPC")
	FSunsetCoveNPCActivityChangedSignature OnActivityChanged;

	// ------------------------------------------------------------------
	// Resting (called by behavior components, with authority)
	// ------------------------------------------------------------------

	/**
	 * Takes the body off the navmesh and blends it onto a rest transform over RestBlendTime.
	 * FeetTransform: where the feet go (for a sunbed, on the mattress). Its X axis is where the NPC faces.
	 * Activity: the NPC.Activity tag to show (sitting, lying...). Applied at the start of the blend.
	 */
	void BeginRest(const FTransform& FeetTransform, FGameplayTag Activity);

	/**
	 * Clears the activity, blends the body back to StandFeetLocation (a navigable point next to the spot), gives
	 * navigation back and then calls OnStoodUp. Calls OnStoodUp at once when the body is not resting.
	 */
	void EndRest(const FVector& StandFeetLocation, FSimpleDelegate OnStoodUp);

	/** True from BeginRest until the body is back on its feet. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|NPC")
	bool IsResting() const { return bResting; }

	// ------------------------------------------------------------------
	// Components (placeholder body, see the note at the top of this file)
	// ------------------------------------------------------------------

	/** Moved by PlaceholderPoses. Keep it at the capsule centre for the standing pose. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	TObjectPtr<USceneComponent> PlaceholderRoot;

	/** The body: a cylinder as tall as the capsule. No collision and no influence on the navmesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	/** The facing marker: a cone at head height pointing forward (+X). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	TObjectPtr<UStaticMeshComponent> PlaceholderNose;

	// ------------------------------------------------------------------
	// Configuration (Class Defaults of the NPC Blueprint)
	// ------------------------------------------------------------------

	/** Seconds the body takes to sit or lie down on its spot, and to get up again. 0 = instant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|NPC", meta = (ClampMin = "0.0", Units = "s"))
	float RestBlendTime = 0.4f;

	/** Placeholder only: colours picked at random, one per NPC, so NPCs are easy to tell apart. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	TArray<FLinearColor> PlaceholderColors;

	/** Placeholder only: vector parameter that receives the colour. The engine BasicShapeMaterial calls it "Color". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	FName PlaceholderColorParameter = TEXT("Color");

	/**
	 * Placeholder only: transform of PlaceholderRoot, relative to the capsule, for each activity (for example lying =
	 * pitch 90 and lowered to the mattress). An activity without an entry keeps the standing transform of the Blueprint.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|NPC|Placeholder")
	TMap<FGameplayTag, FTransform> PlaceholderPoses;

protected:
	/** Applies an activity change on this machine (placeholder pose, OnActivityChanged). Server and clients. */
	virtual void HandleActivityChanged(FGameplayTag OldActivity);

private:
	UFUNCTION()
	void OnRep_ActivityTag(FGameplayTag OldActivity);

	void SetupPlaceholder();
	void ApplyPlaceholderPose();
	void StartBodyBlend(const FTransform& TargetActorTransform);
	void FinishStandingUp();

	/** Replicated activity. See SetActivityTag. */
	UPROPERTY(ReplicatedUsing = OnRep_ActivityTag)
	FGameplayTag ActivityTag;

	/** True from BeginRest until EndRest has finished. */
	bool bResting = false;

	/** True while EndRest blends the body back to its feet. */
	bool bStandingUp = false;

	/** Body blend (sitting down, standing up). Advanced in Tick while bBlending is true. */
	bool bBlending = false;
	float BlendElapsed = 0.f;
	FTransform BlendStart;
	FTransform BlendTarget;

	/** Called by FinishStandingUp. Set by EndRest. */
	FSimpleDelegate PendingStoodUp;

	/** Pawn collision response of the capsule before BeginRest, restored when the NPC stands up. */
	TEnumAsByte<ECollisionResponse> StandingPawnResponse = ECR_Block;

	/** Standing transform of PlaceholderRoot, as set in the Blueprint. */
	FTransform PlaceholderStandTransform;

	/** False when Mesh has a skeletal mesh: the placeholder is hidden and never posed. */
	bool bPlaceholderActive = false;
};
