// ArmsAnimationSet.cpp
// SunsetCove - Animation System / Arms Animation. See Public/AnimationSystem/ArmsAnimation/ArmsAnimationSet.h

#include "AnimationSystem/ArmsAnimation/ArmsAnimationSet.h"

#include "Animation/AnimSequence.h"
#include "Core/Logging/SunsetCoveLog.h"

#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Misc/DataValidation.h"
#endif

bool UArmsAnimationSet::FindClip(FName ItemId, EArmsAnimAction Action, EArmSide Side, FArmsClip& OutClip) const
{
	if (const FArmsItemAnimations* Item = Items.Find(ItemId))
	{
		if (const FArmsActionClips* Clips = Item->Actions.Find(Action))
		{
			const FArmsClip& Clip = Clips->Get(Side);
			if (Clip.Sequence)
			{
				OutClip = Clip;
				return true;
			}
		}
	}

	// A debug placeholder stands in for whatever is missing, so every event is visible before the real animations exist.
	if (bUseDebugPlaceholder && DebugPlaceholderSequence)
	{
		OutClip = FArmsClip();
		OutClip.Sequence = DebugPlaceholderSequence;
		return true;
	}

	return false;
}

#if WITH_EDITOR

namespace
{
	/** AS_Arms_Umbrella_Drop_L -> Item "Umbrella", Action Drop, Side Left. False if the name does not follow the convention. */
	bool ParseArmsAnimationName(const FString& AssetName, FName& OutItem, EArmsAnimAction& OutAction, EArmSide& OutSide)
	{
		static const FString Prefix(TEXT("AS_Arms_"));
		if (!AssetName.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}
		const FString Rest = AssetName.RightChop(Prefix.Len());

		TArray<FString> Parts;
		Rest.ParseIntoArray(Parts, TEXT("_"));
		if (Parts.Num() != 3)
		{
			return false;
		}

		const UEnum* ActionEnum = StaticEnum<EArmsAnimAction>();
		const int64 ActionValue = ActionEnum ? ActionEnum->GetValueByNameString(Parts[1]) : INDEX_NONE;
		if (ActionValue == INDEX_NONE)
		{
			return false;
		}

		if (Parts[2] == TEXT("R"))
		{
			OutSide = EArmSide::Right;
		}
		else if (Parts[2] == TEXT("L"))
		{
			OutSide = EArmSide::Left;
		}
		else
		{
			return false;
		}

		OutItem = FName(*Parts[0]);
		OutAction = static_cast<EArmsAnimAction>(ActionValue);
		return !Parts[0].IsEmpty();
	}
}

void UArmsAnimationSet::PopulateFromNamingConvention()
{
	if (SearchFolder.Path.IsEmpty())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimationSet '%s': set Search Folder (Editor Helpers) first, for example /Game/Animation/FirstPerson/Items."), *GetName());
		return;
	}

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchFolder.Path);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	FAssetRegistryModule::GetRegistry().GetAssets(Filter, Assets);

	Modify();

	int32 Filled = 0;
	TArray<FString> Skipped;
	for (const FAssetData& Asset : Assets)
	{
		const FString Name = Asset.AssetName.ToString();

		FName Item;
		EArmsAnimAction Action;
		EArmSide Side;
		if (!ParseArmsAnimationName(Name, Item, Action, Side))
		{
			// Only the arms animations are expected here: a name that starts with AS_Arms_ but does not parse is worth a line.
			if (Name.StartsWith(TEXT("AS_Arms_")))
			{
				Skipped.Add(Name);
			}
			continue;
		}

		UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
		if (!Sequence)
		{
			continue;
		}

		Items.FindOrAdd(Item).Actions.FindOrAdd(Action).Get(Side).Sequence = Sequence;
		++Filled;
	}

	MarkPackageDirty();
	UE_LOG(LogSunsetCove, Log, TEXT("ArmsAnimationSet '%s': %d clips filled from '%s'. Save the asset."), *GetName(), Filled, *SearchFolder.Path);
	for (const FString& Name : Skipped)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimationSet '%s': '%s' does not follow AS_Arms_<Item>_<Action>_<Hand> (actions: Hold, PlaceAim, ThrowCharge, Grab, Place, Throw, Drop, Equip, Holster, Use, Pickup, Open, Close; hands: R, L). Skipped."),
			*GetName(), *Name);
	}
}

void UArmsAnimationSet::EnsureSkeletonSlots()
{
	USkeleton* Skeleton = TargetSkeleton.LoadSynchronous();
	if (!Skeleton)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimationSet '%s': set Target Skeleton (Editor Helpers) first (the skeleton of the arms, SK_Character)."), *GetName());
		return;
	}

	Skeleton->Modify();

	const TPair<FName, FName> Pairs[] = {
		TPair<FName, FName>(RightSlotName, RightSlotGroupName),
		TPair<FName, FName>(LeftSlotName, LeftSlotGroupName)
	};
	for (const TPair<FName, FName>& Pair : Pairs)
	{
		Skeleton->AddSlotGroupName(Pair.Value);
		Skeleton->RegisterSlotNode(Pair.Key);
		Skeleton->SetSlotGroupName(Pair.Key, Pair.Value);
	}

	Skeleton->MarkPackageDirty();
	UE_LOG(LogSunsetCove, Log, TEXT("ArmsAnimationSet '%s': slot '%s' (group '%s') and slot '%s' (group '%s') are on '%s'. Save the skeleton."),
		*GetName(), *RightSlotName.ToString(), *RightSlotGroupName.ToString(), *LeftSlotName.ToString(), *LeftSlotGroupName.ToString(), *Skeleton->GetName());
}

EDataValidationResult UArmsAnimationSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	const USkeleton* Skeleton = TargetSkeleton.LoadSynchronous();

	for (const TPair<FName, FArmsItemAnimations>& Item : Items)
	{
		if (Item.Key.IsNone())
		{
			Context.AddError(FText::FromString(TEXT("An item has an empty id.")));
			Result = EDataValidationResult::Invalid;
		}

		for (const TPair<EArmsAnimAction, FArmsActionClips>& Action : Item.Value.Actions)
		{
			// The enum name without its "EArmsAnimAction::" prefix: Drop, ThrowCharge...
			FString ActionName = StaticEnum<EArmsAnimAction>()->GetNameStringByValue(static_cast<int64>(Action.Key));
			int32 ColonIndex = INDEX_NONE;
			if (ActionName.FindLastChar(TEXT(':'), ColonIndex))
			{
				ActionName = ActionName.RightChop(ColonIndex + 1);
			}

			for (const EArmSide Side : { EArmSide::Right, EArmSide::Left })
			{
				const FArmsClip& Clip = Action.Value.Get(Side);
				if (!Clip.Sequence)
				{
					continue;
				}

				const TCHAR* SideLetter = (Side == EArmSide::Right) ? TEXT("R") : TEXT("L");

				if (Skeleton && Clip.Sequence->GetSkeleton() && !Skeleton->IsCompatibleForEditor(Clip.Sequence->GetSkeleton()))
				{
					Context.AddError(FText::FromString(FString::Printf(TEXT("%s / %s / %s: '%s' is not made for the skeleton '%s'."),
						*Item.Key.ToString(), *ActionName, SideLetter, *Clip.Sequence->GetName(), *Skeleton->GetName())));
					Result = EDataValidationResult::Invalid;
				}

				const FString Expected = FString::Printf(TEXT("AS_Arms_%s_%s_%s"), *Item.Key.ToString(), *ActionName, SideLetter);
				if (Clip.Sequence->GetName() != Expected)
				{
					Context.AddWarning(FText::FromString(FString::Printf(TEXT("%s / %s / %s: '%s' does not match the naming convention (expected '%s'). Is the clip under the wrong item, action or hand?"),
						*Item.Key.ToString(), *ActionName, SideLetter, *Clip.Sequence->GetName(), *Expected)));
				}
			}
		}
	}

	return Result;
}

#endif // WITH_EDITOR
