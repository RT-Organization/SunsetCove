// SunsetCoveUISettings.h
// SunsetCove - UI System / HUD. The project-wide UI settings page: Project Settings > Game > Sunset Cove UI.
//
// WHAT THIS CLASS IS
//   The one place where the HUD finds its data tables, so no code holds an asset path. Saved in Config/DefaultGame.ini under
//   [/Script/SunsetCove.SunsetCoveUISettings]. Read it with GetDefault<USunsetCoveUISettings>().
//
// WHAT IS NOT HERE
//   The look of each HUD element (crosshair size, colors, radial radii, fonts) lives on the element's Widget Blueprint as
//   Class Defaults (WBP_Crosshair, WBP_NotificationFeed, WBP_ToolRadialMenu), where a designer sees the result in the
//   UMG designer. Which elements the HUD shows is in DA_UIConfig > HUD Elements (CoreFramework).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SunsetCoveUISettings.generated.h"

class UDataTable;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sunset Cove UI"))
class SUNSETCOVE_API USunsetCoveUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USunsetCoveUISettings();

	/**
	 * The messages of the suggestion feed: a Data Table with the row structure HUDNotificationRow. Game code asks for a message
	 * by row name (USunsetCoveHUDSubsystem::ShowNotificationById). Default: /Game/UI/HUD/Data/DT_HUDNotifications.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "HUD|Notifications", meta = (RequiredAssetDataTags = "RowStructure=/Script/SunsetCove.HUDNotificationRow"))
	TSoftObjectPtr<UDataTable> NotificationTable;
};
