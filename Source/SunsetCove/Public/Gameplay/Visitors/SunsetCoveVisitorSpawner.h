// SunsetCoveVisitorSpawner.h
// SunsetCove - Gameplay area (Visitors). The hidden end of the sandy path: visitors appear here and disappear here.
//
// WHAT THIS CLASS IS
//   The "NPC Spawn" star of the Miro concept map. While the shift is active it sends a visitor up the path every few
//   seconds, until MaxVisitorsInCove are in the cove. Each visitor gets the entrance to walk to and this spawner as its
//   exit, so leaving visitors walk back down the same path and are removed here, out of the player's sight
//   (GDD: "Ingresso e apparizione degli NPC").
//
// SHIFT
//   With bSpawnOnlyDuringShift (default) it follows USunsetCoveWorkdaySubsystem: it starts on OnShiftStarted and stops
//   on OnShiftEnded. Visitors already in the cove leave on their own when the shift ends.
//
// SETUP
//   Use BP_VisitorSpawner (Class Defaults: Visitor Classes = BP_Visitor) and place it where the path leaves the
//   playable area, hidden by trees or rocks, on the navmesh. On the placed instance pick the Entrance actor.
//   GDD, still open: number of entry points, arrival rate, behaviour while the barrier is closed.
//
// HIERARCHY
//   AActor -> ASunsetCoveVisitorSpawner (this class) -> BP_VisitorSpawner (Blueprint)
//
// NETWORK
//   Spawns with authority only. Visitors replicate as normal characters.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SunsetCoveVisitorSpawner.generated.h"

class ASunsetCoveEntrance;
class ASunsetCoveNPCCharacter;
class UArrowComponent;
class USceneComponent;
class USunsetCoveRadiusRingComponent;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveVisitorSpawner : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveVisitorSpawner();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	//~ End AActor Interface

	/** Spawns one visitor now (ignores the caps and the shift). Returns null if nothing could be spawned. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Visitors")
	ASunsetCoveNPCCharacter* SpawnVisitor();

	/** Visitors spawned here that are still in the world. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Visitors")
	int32 GetVisitorCount() const;

	/** Where visitors spawned here leave the cove: this actor, on the navmesh (the actor itself until the navmesh is ready). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Visitors")
	FVector GetExitLocation() const;

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	/** Editor only: the direction new visitors face (up the path). */
	UPROPERTY(VisibleAnywhere, Category = "SunsetCove|Visitors")
	TObjectPtr<UArrowComponent> FacingArrow;

	/** Editor only: shows SpawnRadius, the area new visitors appear in (keep it on the navmesh and out of sight). */
	UPROPERTY(VisibleAnywhere, Category = "SunsetCove|Visitors")
	TObjectPtr<USunsetCoveRadiusRingComponent> SpawnRing;
#endif

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Visitor Blueprints to pick from at random (BP_Visitor...). Each needs a SunsetCove Visitor component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors")
	TArray<TSubclassOf<ASunsetCoveNPCCharacter>> VisitorClasses;

	/** The barrier visitors walk to first. Without one they go straight to a spot. Set it on the placed instance. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors")
	TObjectPtr<ASunsetCoveEntrance> Entrance;

	/** No new visitor while this many visitors from this spawner are in the world. Keep it under the crowd limit (50). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0"))
	int32 MaxVisitorsInCove = 20;

	/** Visitors per shift, in total. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0"))
	int32 MaxVisitorsPerShift = 0;

	/** Seconds before the first visitor of a shift. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float FirstSpawnDelay = 1.f;

	/** Shortest gap between two visitors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.1", Units = "s"))
	float MinSpawnInterval = 3.f;

	/** Longest gap between two visitors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.1", Units = "s"))
	float MaxSpawnInterval = 8.f;

	/** Visitors appear anywhere within this radius, on the navmesh, so they do not stack on one point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnRadius = 150.f;

	/** Spawn only while the workday shift is active (USunsetCoveWorkdaySubsystem). Off: spawn from BeginPlay on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Visitors")
	bool bSpawnOnlyDuringShift = true;

private:
	UFUNCTION()
	void HandleShiftStarted();

	UFUNCTION()
	void HandleShiftEnded();

	UFUNCTION()
	void HandleVisitorDestroyed(AActor* DestroyedActor);

	void StartSpawning();
	void StopSpawning();
	void HandleSpawnTimer();
	void ScheduleNextSpawn(float Delay);
	bool FindSpawnFeetLocation(FVector& OutFeetLocation) const;

	/** Visitors spawned here and still alive. */
	TArray<TWeakObjectPtr<ASunsetCoveNPCCharacter>> Visitors;

	/**
	 * This spawner's location on the navmesh. Found on first use, not at BeginPlay: the navigation system registers the
	 * navmesh on its first tick, after every BeginPlay, so a BeginPlay query always fails.
	 */
	mutable FVector CachedExitLocation = FVector::ZeroVector;
	mutable bool bExitLocationOnNavmesh = false;

	/** The "no navmesh here" warning is written once per play session, not once per spawn attempt. */
	bool bWarnedNoNavmesh = false;

	int32 SpawnedThisShift = 0;
	bool bSpawning = false;
	FTimerHandle SpawnTimer;
};
