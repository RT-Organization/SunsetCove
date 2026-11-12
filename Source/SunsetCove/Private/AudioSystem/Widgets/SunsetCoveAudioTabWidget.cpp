// SunsetCoveAudioTabWidget.cpp
// SunsetCove - Audio System / Widgets. See Public/AudioSystem/Widgets/SunsetCoveAudioTabWidget.h.

#include "AudioSystem/Widgets/SunsetCoveAudioTabWidget.h"

#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "AudioSystem/Widgets/SunsetCoveVolumeRowWidget.h"
#include "Blueprint/WidgetTree.h"

USunsetCoveAudioTabWidget::USunsetCoveAudioTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USunsetCoveAudioTabWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TakeSnapshot();
}

void USunsetCoveAudioTabWidget::NotifyTabSelected()
{
	Super::NotifyTabSelected();

	// Coming back to the tab with nothing pending: start a fresh reference (volumes may have changed elsewhere).
	if (!HasPendingChanges())
	{
		TakeSnapshot();
	}
}

TArray<USunsetCoveVolumeRowWidget*> USunsetCoveAudioTabWidget::GetVolumeRows() const
{
	TArray<USunsetCoveVolumeRowWidget*> Rows;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&Rows](UWidget* Widget)
		{
			if (USunsetCoveVolumeRowWidget* Row = Cast<USunsetCoveVolumeRowWidget>(Widget))
			{
				Rows.Add(Row);
			}
		});
	}
	return Rows;
}

void USunsetCoveAudioTabWidget::TakeSnapshot()
{
	Snapshot.Reset();
	const UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	for (const USunsetCoveVolumeRowWidget* Row : GetVolumeRows())
	{
		if (Row->GetCategory().IsValid())
		{
			FCategoryState& State = Snapshot.FindOrAdd(Row->GetCategory());
			State.Volume = Row->GetVolume();
			State.bMuted = Audio && Audio->IsCategoryMuted(Row->GetCategory());
		}
	}
}

bool USunsetCoveAudioTabWidget::HasPendingChanges_Implementation() const
{
	const UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	if (!Audio)
	{
		return false;
	}

	for (const TPair<FGameplayTag, FCategoryState>& Pair : Snapshot)
	{
		if (!FMath::IsNearlyEqual(Audio->GetCategoryVolume(Pair.Key), Pair.Value.Volume, KINDA_SMALL_NUMBER)
			|| Audio->IsCategoryMuted(Pair.Key) != Pair.Value.bMuted)
		{
			return true;
		}
	}
	return false;
}

void USunsetCoveAudioTabWidget::ApplyChanges_Implementation()
{
	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		Audio->SaveAudioSettings();
	}
	TakeSnapshot();
}

void USunsetCoveAudioTabWidget::RevertChanges_Implementation()
{
	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		for (const TPair<FGameplayTag, FCategoryState>& Pair : Snapshot)
		{
			Audio->SetCategoryVolume(Pair.Key, Pair.Value.Volume);
			Audio->SetCategoryMuted(Pair.Key, Pair.Value.bMuted);
		}
	}
	TakeSnapshot();
}
