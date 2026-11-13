// SunsetCoveSeaSubsystem.h
// SunsetCove - Environment / Water. The phone book entry of the sea (same pattern as UDayNightCycleSubsystem).
//
// WHAT THIS IS FOR
//   Other systems ask questions about the sea without holding a reference to a placed actor: the wind ambience (how far is the
//   shore?), future floating props and the swimming player (where is the water surface?), visitors (is this spot wet?).
//
// HOW TO USE IT
//   C++        if (const ASunsetCoveSea* Sea = USunsetCoveSeaSubsystem::GetSea(this)) { const float Z = Sea->GetWaterHeightAt(Location); }
//   Blueprint  "Get Sea" (any actor or component works as the context).
//   Null in a level with no sea (the main menu): always check it.
//
// LIFETIME
//   The sea registers itself in BeginPlay and unregisters in EndPlay. One sea per world; a second one is reported and ignored.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SunsetCoveSeaSubsystem.generated.h"

class ASunsetCoveSea;

UCLASS()
class SUNSETCOVE_API USunsetCoveSeaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The sea of this world, or null. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	ASunsetCoveSea* GetActiveSea() const { return ActiveSea.Get(); }

	/** Shortcut for any object that lives in a world: the sea, or null. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Sea"))
	static ASunsetCoveSea* GetSea(const UObject* WorldContextObject);

	/** Called by ASunsetCoveSea only. */
	void RegisterSea(ASunsetCoveSea* Sea);

	/** Called by ASunsetCoveSea only. Ignored if it is not the registered sea. */
	void UnregisterSea(ASunsetCoveSea* Sea);

private:
	TWeakObjectPtr<ASunsetCoveSea> ActiveSea;
};
