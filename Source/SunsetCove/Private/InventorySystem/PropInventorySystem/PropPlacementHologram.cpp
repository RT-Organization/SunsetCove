// PropPlacementHologram.cpp
// SunsetCove - Inventory System / Prop Inventory. See Public/InventorySystem/PropInventorySystem/PropPlacementHologram.h.

#include "InventorySystem/PropInventorySystem/PropPlacementHologram.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

APropPlacementHologram::APropPlacementHologram()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	HologramRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HologramRoot"));
	SetRootComponent(HologramRoot);
	HologramRoot->SetMobility(EComponentMobility::Movable);
}

void APropPlacementHologram::InitializeFromProp(const AActor* SourceProp, UMaterialInterface* HologramMaterial, FName ColorParameterName, const FLinearColor& InValidColor, const FLinearColor& InInvalidColor)
{
	ColorParameter = ColorParameterName;
	ValidColor = InValidColor;
	InvalidColor = InInvalidColor;

	if (!SourceProp || !SourceProp->GetRootComponent())
	{
		return;
	}

	if (HologramMaterial)
	{
		HologramMaterialInstance = UMaterialInstanceDynamic::Create(HologramMaterial, this);
	}

	const FTransform SourceRootTransform = SourceProp->GetRootComponent()->GetComponentTransform();

	TInlineComponentArray<UStaticMeshComponent*> SourceMeshes(SourceProp);
	for (const UStaticMeshComponent* Source : SourceMeshes)
	{
		if (!Source || !Source->GetStaticMesh() || !Source->IsVisible())
		{
			continue;
		}

		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Copy->SetStaticMesh(Source->GetStaticMesh());
		Copy->SetMobility(EComponentMobility::Movable);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetGenerateOverlapEvents(false);
		Copy->SetCanEverAffectNavigation(false);
		Copy->SetCastShadow(false);
		Copy->bReceivesDecals = false;
		Copy->SetupAttachment(HologramRoot);
		Copy->RegisterComponent();

		// Same place relative to the prop root as the original mesh, so multi-mesh props keep their shape.
		Copy->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(SourceRootTransform));

		const int32 MaterialCount = Copy->GetNumMaterials();
		for (int32 Index = 0; Index < MaterialCount; ++Index)
		{
			if (HologramMaterialInstance)
			{
				Copy->SetMaterial(Index, HologramMaterialInstance);
			}
			else
			{
				Copy->SetMaterial(Index, Source->GetMaterial(Index));
			}
		}

		MeshCopies.Add(Copy);
	}

	// Skeletal meshes (every prop of the game): same copy, and the copy follows the pose of the original.
	TInlineComponentArray<USkeletalMeshComponent*> SourceSkeletals(SourceProp);
	for (USkeletalMeshComponent* Source : SourceSkeletals)
	{
		USkeletalMesh* SkeletalMesh = Source ? Source->GetSkeletalMeshAsset() : nullptr;
		if (!SkeletalMesh || !Source->IsVisible())
		{
			continue;
		}

		USkeletalMeshComponent* Copy = NewObject<USkeletalMeshComponent>(this, NAME_None, RF_Transient);
		Copy->SetSkeletalMeshAsset(SkeletalMesh);
		Copy->SetMobility(EComponentMobility::Movable);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetGenerateOverlapEvents(false);
		Copy->SetCanEverAffectNavigation(false);
		Copy->SetCastShadow(false);
		Copy->bReceivesDecals = false;
		Copy->SetupAttachment(HologramRoot);
		Copy->RegisterComponent();

		Copy->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(SourceRootTransform));

		// The copy shows the pose the prop has now (for example the folded pose it is carried in).
		Copy->SetLeaderPoseComponent(Source);

		const int32 MaterialCount = Copy->GetNumMaterials();
		for (int32 Index = 0; Index < MaterialCount; ++Index)
		{
			Copy->SetMaterial(Index, HologramMaterialInstance ? Cast<UMaterialInterface>(HologramMaterialInstance) : Source->GetMaterial(Index));
		}

		MeshCopies.Add(Copy);
	}
}

void APropPlacementHologram::SetValid(bool bNewValid)
{
	if (HologramMaterialInstance)
	{
		if (!ColorParameter.IsNone())
		{
			HologramMaterialInstance->SetVectorParameterValue(ColorParameter, bNewValid ? ValidColor : InvalidColor);
		}
		SetActorHiddenInGame(false);
	}
	else
	{
		// No material to tint: valid is visible, invalid is hidden.
		SetActorHiddenInGame(!bNewValid);
	}
}
