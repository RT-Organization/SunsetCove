// SunsetCoveGameFlowSubsystem.cpp
// SunsetCove - Core area / Game Flow. See Public/Core/GameFlow/SunsetCoveGameFlowSubsystem.h.

#include "Core/GameFlow/SunsetCoveGameFlowSubsystem.h"

#include "Core/GameFlow/SunsetCoveGameFlowSettings.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SaveSystem/Library/SaveFunctionLibrary.h"
#include "SaveSystem/Manager/SaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "SunsetCoveGameFlow"

USunsetCoveGameFlowSubsystem* USunsetCoveGameFlowSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USunsetCoveGameFlowSubsystem>() : nullptr;
}

void USunsetCoveGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// The save system is a sibling subsystem: make sure it exists before this one uses it.
	Collection.InitializeDependency<USaveSubsystem>();
	Super::Initialize(Collection);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USunsetCoveGameFlowSubsystem::HandlePostLoadMap);
}

void USunsetCoveGameFlowSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();

	OnWorldSaveStateChanged.Clear();
	OnFlowFailed.Clear();
	OnBusyChanged.Clear();

	Super::Deinitialize();
}

USaveSubsystem* USunsetCoveGameFlowSubsystem::GetSaveSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

FString USunsetCoveGameFlowSubsystem::GetWorldSlotName() const
{
	return GetDefault<USunsetCoveGameFlowSettings>()->WorldSlotName;
}

// ----------------------------------------------------------------------
// World save state
// ----------------------------------------------------------------------

void USunsetCoveGameFlowSubsystem::RefreshWorldSaveState()
{
	USaveSubsystem* Save = GetSaveSubsystem();
	if (!Save)
	{
		return;
	}

	Save->ListSlots(FSaveCallback([WeakThis = TWeakObjectPtr<USunsetCoveGameFlowSubsystem>(this)](ESaveResult /*Result*/, const FSaveSlotInfo& /*Info*/)
	{
		// A failed list still leaves a valid (possibly empty) cache: the answer is "no save".
		if (USunsetCoveGameFlowSubsystem* Self = WeakThis.Get())
		{
			Self->UpdateWorldSaveStateFromCache(Self->GetSaveSubsystem());
		}
	}));
}

void USunsetCoveGameFlowSubsystem::UpdateWorldSaveStateFromCache(USaveSubsystem* Save)
{
	bWorldSaveStateKnown = true;
	bHasWorldSave = Save && Save->DoesSlotExist(GetWorldSlotName());
	OnWorldSaveStateChanged.Broadcast(bHasWorldSave);
}

// ----------------------------------------------------------------------
// New world
// ----------------------------------------------------------------------

void USunsetCoveGameFlowSubsystem::StartNewWorld()
{
	USaveSubsystem* Save = GetSaveSubsystem();
	if (bBusy || !Save)
	{
		return;
	}

	const FString SlotName = GetWorldSlotName();
	if (!USaveSubsystem::IsValidSlotName(SlotName))
	{
		Fail(FText::Format(LOCTEXT("BadSlotName", "Invalid world slot name '{0}' (Project Settings > Game > Sunset Cove Game Flow)."), FText::FromString(SlotName)));
		return;
	}

	SetBusy(true);

	// List first: the answer is fresh even if the menu never refreshed (DoesSlotExist reads the cache).
	Save->ListSlots(FSaveCallback([WeakThis = TWeakObjectPtr<USunsetCoveGameFlowSubsystem>(this), SlotName](ESaveResult /*Result*/, const FSaveSlotInfo& /*Info*/)
	{
		USunsetCoveGameFlowSubsystem* Self = WeakThis.Get();
		USaveSubsystem* SaveSystem = Self ? Self->GetSaveSubsystem() : nullptr;
		if (!SaveSystem)
		{
			return;
		}

		if (!SaveSystem->DoesSlotExist(SlotName))
		{
			Self->CreateFreshWorld();
			return;
		}

		// The player confirmed: the old world is deleted before the new one is written.
		SaveSystem->DeleteSlot(SlotName, FSaveCallback([WeakThis](ESaveResult DeleteResult, const FSaveSlotInfo& /*Info*/)
		{
			USunsetCoveGameFlowSubsystem* Inner = WeakThis.Get();
			if (!Inner)
			{
				return;
			}
			if (DeleteResult != ESaveResult::Success && DeleteResult != ESaveResult::SlotNotFound)
			{
				Inner->Fail(USaveFunctionLibrary::ResultToText(DeleteResult));
				return;
			}

			UE_LOG(LogSunsetCove, Log, TEXT("GameFlow: old world deleted."));
			Inner->bHasWorldSave = false;
			Inner->OnWorldSaveStateChanged.Broadcast(false);
			Inner->CreateFreshWorld();
		}));
	}));
}

void USunsetCoveGameFlowSubsystem::CreateFreshWorld()
{
	USaveSubsystem* Save = GetSaveSubsystem();
	if (!Save)
	{
		Fail(LOCTEXT("NoSaveSystem", "The save system is not available."));
		return;
	}

	const USunsetCoveGameFlowSettings* Settings = GetDefault<USunsetCoveGameFlowSettings>();
	const ESaveResult StartResult = Save->StartNewGame(Settings->WorldSlotName);
	if (StartResult != ESaveResult::Success)
	{
		Fail(USaveFunctionLibrary::ResultToText(StartResult));
		return;
	}

	// First save right away: from now on the world exists on disk, so the next menu shows Continue Game.
	Save->SaveToSlot(Settings->WorldSlotName, Settings->WorldDisplayName,
		FSaveCallback([WeakThis = TWeakObjectPtr<USunsetCoveGameFlowSubsystem>(this)](ESaveResult Result, const FSaveSlotInfo& /*Info*/)
	{
		USunsetCoveGameFlowSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result != ESaveResult::Success)
		{
			Self->Fail(USaveFunctionLibrary::ResultToText(Result));
			return;
		}

		UE_LOG(LogSunsetCove, Log, TEXT("GameFlow: new world created and saved."));
		Self->bWorldSaveStateKnown = true;
		Self->bHasWorldSave = true;
		Self->OnWorldSaveStateChanged.Broadcast(true);

		Self->bSavePendingAfterMapLoad = GetDefault<USunsetCoveGameFlowSettings>()->bSaveWhenNewWorldMapLoaded;
		Self->OpenWorldMap(FString());
	}));
}

// ----------------------------------------------------------------------
// Continue
// ----------------------------------------------------------------------

void USunsetCoveGameFlowSubsystem::ContinueWorld()
{
	USaveSubsystem* Save = GetSaveSubsystem();
	if (bBusy || !Save)
	{
		return;
	}

	SetBusy(true);

	// LoadSlot makes the save the active data; the saveables of the map restore themselves when they register.
	Save->LoadSlot(GetWorldSlotName(), FSaveCallback([WeakThis = TWeakObjectPtr<USunsetCoveGameFlowSubsystem>(this)](ESaveResult Result, const FSaveSlotInfo& Info)
	{
		USunsetCoveGameFlowSubsystem* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		if (Result != ESaveResult::Success)
		{
			if (Result == ESaveResult::SlotNotFound)
			{
				Self->bHasWorldSave = false;
				Self->OnWorldSaveStateChanged.Broadcast(false);
			}
			Self->Fail(USaveFunctionLibrary::ResultToText(Result));
			return;
		}

		UE_LOG(LogSunsetCove, Log, TEXT("GameFlow: world loaded (saved in map '%s')."), *Info.MapName);
		Self->OpenWorldMap(Info.MapName);
	}));
}

void USunsetCoveGameFlowSubsystem::SaveWorld()
{
	USaveSubsystem* Save = GetSaveSubsystem();
	if (!Save)
	{
		return;
	}

	const USunsetCoveGameFlowSettings* Settings = GetDefault<USunsetCoveGameFlowSettings>();
	Save->SaveToSlot(Settings->WorldSlotName, Settings->WorldDisplayName,
		FSaveCallback([WeakThis = TWeakObjectPtr<USunsetCoveGameFlowSubsystem>(this)](ESaveResult Result, const FSaveSlotInfo& /*Info*/)
	{
		if (USunsetCoveGameFlowSubsystem* Self = WeakThis.Get())
		{
			if (Result == ESaveResult::Success)
			{
				Self->bWorldSaveStateKnown = true;
				Self->bHasWorldSave = true;
			}
			else
			{
				UE_LOG(LogSunsetCove, Warning, TEXT("GameFlow: world save failed: %s"), *USaveFunctionLibrary::ResultToText(Result).ToString());
			}
		}
	}));
}

// ----------------------------------------------------------------------
// Maps
// ----------------------------------------------------------------------

void USunsetCoveGameFlowSubsystem::OpenWorldMap(const FString& SavedMapName)
{
	const USunsetCoveGameFlowSettings* Settings = GetDefault<USunsetCoveGameFlowSettings>();
	const FString MenuMapName = Settings->MainMenuMap.IsNull() ? FString() : Settings->MainMenuMap.ToSoftObjectPath().GetAssetName();
	const FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(GetGameInstance(), /*bRemovePrefixString*/ true);

	// A save made in the menu (the first save of a new world) records the menu map: that is not where the world lives.
	const bool bUseSavedMap = !SavedMapName.IsEmpty() && SavedMapName != MenuMapName && SavedMapName != CurrentMapName;
	if (bUseSavedMap)
	{
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*SavedMapName));
		return;
	}

	if (Settings->GameplayMap.IsNull())
	{
		Fail(LOCTEXT("NoGameplayMap", "No gameplay map is set (Project Settings > Game > Sunset Cove Game Flow)."));
		return;
	}
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetGameInstance(), Settings->GameplayMap);
}

void USunsetCoveGameFlowSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	SetBusy(false);

	if (!bSavePendingAfterMapLoad)
	{
		return;
	}
	bSavePendingAfterMapLoad = false;

	// Next frame: the actors of the map have begun play and their saveables have registered.
	LoadedWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SaveWorld();
	}));
}

// ----------------------------------------------------------------------
// State
// ----------------------------------------------------------------------

void USunsetCoveGameFlowSubsystem::SetBusy(bool bNewBusy)
{
	if (bBusy != bNewBusy)
	{
		bBusy = bNewBusy;
		OnBusyChanged.Broadcast(bBusy);
	}
}

void USunsetCoveGameFlowSubsystem::Fail(const FText& Reason)
{
	UE_LOG(LogSunsetCove, Warning, TEXT("GameFlow: %s"), *Reason.ToString());
	bSavePendingAfterMapLoad = false;
	SetBusy(false);
	OnFlowFailed.Broadcast(Reason);
}

#undef LOCTEXT_NAMESPACE
