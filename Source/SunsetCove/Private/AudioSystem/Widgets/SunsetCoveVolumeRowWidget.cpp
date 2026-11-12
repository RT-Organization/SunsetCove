// SunsetCoveVolumeRowWidget.cpp
// SunsetCove - Audio System / Widgets. See Public/AudioSystem/Widgets/SunsetCoveVolumeRowWidget.h.

#include "AudioSystem/Widgets/SunsetCoveVolumeRowWidget.h"

#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "Components/Slider.h"
#include "Core/Logging/SunsetCoveLog.h"

USunsetCoveVolumeRowWidget::USunsetCoveVolumeRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Volumes are 0..1 shown as a percentage, 5 % per Left / Right step.
	MinValue = 0.0f;
	MaxValue = 1.0f;
	StepSize = 0.05f;
	ValueFormat = ESettingValueFormat::Percent;
	PreviewValue = 0.8f;
}

UAudioFrameSubsystem* USunsetCoveVolumeRowWidget::GetAudio() const
{
	return UAudioFrameSubsystem::Get(this);
}

void USunsetCoveVolumeRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	OnValueChanged.AddUniqueDynamic(this, &USunsetCoveVolumeRowWidget::HandleRowValueChanged);

	// BindWidget of the parent: the Blueprint does not compile without ValueSlider.
	ValueSlider->OnMouseCaptureEnd.AddUniqueDynamic(this, &USunsetCoveVolumeRowWidget::HandleSliderReleased);
	ValueSlider->OnControllerCaptureEnd.AddUniqueDynamic(this, &USunsetCoveVolumeRowWidget::HandleSliderReleased);

	if (!Category.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: volume row without a Category. Set it on the placed row (Details > Volume Row)."), *GetPathName());
	}
}

void USunsetCoveVolumeRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UAudioFrameSubsystem* Audio = GetAudio())
	{
		Audio->OnCategoryVolumeChanged.AddUniqueDynamic(this, &USunsetCoveVolumeRowWidget::HandleCategoryVolumeChanged);
		BoundAudio = Audio;
	}
	RefreshFromAudio();
}

void USunsetCoveVolumeRowWidget::NativeDestruct()
{
	if (UAudioFrameSubsystem* Audio = BoundAudio.Get())
	{
		Audio->OnCategoryVolumeChanged.RemoveDynamic(this, &USunsetCoveVolumeRowWidget::HandleCategoryVolumeChanged);
	}
	BoundAudio.Reset();

	Super::NativeDestruct();
}

float USunsetCoveVolumeRowWidget::GetVolume() const
{
	const UAudioFrameSubsystem* Audio = GetAudio();
	return Audio ? Audio->GetCategoryVolume(Category) : 1.0f;
}

void USunsetCoveVolumeRowWidget::RefreshFromAudio()
{
	SetValue(GetVolume(), /*bBroadcast=*/false);
}

void USunsetCoveVolumeRowWidget::HandleRowValueChanged(USliderSettingRowWidget* Row, float NewValue)
{
	if (UAudioFrameSubsystem* Audio = GetAudio())
	{
		TGuardValue<bool> Guard(bWritingVolume, true);
		Audio->SetCategoryVolume(Category, NewValue);
	}
}

void USunsetCoveVolumeRowWidget::HandleSliderReleased()
{
	if (PreviewCue.IsValid())
	{
		PlayUISound(PreviewCue);
	}
}

void USunsetCoveVolumeRowWidget::HandleCategoryVolumeChanged(FGameplayTag ChangedCategory, float Volume)
{
	if (!bWritingVolume && ChangedCategory == Category)
	{
		SetValue(Volume, /*bBroadcast=*/false);
	}
}
