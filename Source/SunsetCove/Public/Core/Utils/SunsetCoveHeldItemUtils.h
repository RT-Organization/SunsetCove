// SunsetCoveHeldItemUtils.h
// SunsetCove - Core area. Helpers shared by everything the player can hold in a hand: props (ASunsetCoveProp) and tools
// (ASunsetCoveTool). Both are a collision box with a skeletal mesh whose "Grip" bone must land on the hand socket.
//
// Header only. Nothing here depends on a gameplay system.
//
// THE GRIP MATH IN ONE PARAGRAPH
//   The animator puts a Grip bone on the item and a hand socket (Prop_R, Tool_R...) on the arms, in Blender. Holding means
//   "the Grip bone sits exactly on the hand socket". The item is attached by its ROOT (the box), so the root has to be placed
//   relative to the socket at  RootInSocket = GripInRoot^-1 * FineTune,  where GripInRoot is where the grip bone is relative
//   to the root and FineTune is a small per-item nudge (HoldLocationOffset / HoldRotationOffset). With a zero FineTune the
//   grip lands on the socket exactly.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Components/SunsetCovePivotBoxComponent.h"
#include "Engine/HitResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace SunsetCoveHeldItem
{
	/** Name if the skeleton has a bone or a socket with that name, otherwise NAME_None. */
	inline FName NameIfSocketExists(const USkeletalMeshComponent* Mesh, FName Name)
	{
		return (Mesh && !Name.IsNone() && Mesh->DoesSocketExist(Name)) ? Name : NAME_None;
	}

	/**
	 * Where the grip bone (or socket) is relative to the actor root: location and rotation only, scale is dropped.
	 * Identity when GripName is None. The root of these items is the collision box, so actor space is the space of the root.
	 */
	inline FTransform GetGripTransformInRoot(const USkeletalMeshComponent* Mesh, FName GripName)
	{
		if (!Mesh || GripName.IsNone())
		{
			return FTransform::Identity;
		}

		const FTransform GripInActor = Mesh->GetSocketTransform(GripName, RTS_Actor);
		return FTransform(GripInActor.GetRotation(), GripInActor.GetLocation());
	}

	/** The transform of the item root relative to the hand socket: the inverse of the grip, then the fine tune offset. */
	inline FTransform ComputeRootInHandSocket(const FTransform& GripInRoot, const FVector& HoldLocationOffset, const FRotator& HoldRotationOffset)
	{
		const FTransform FineTune(HoldRotationOffset, HoldLocationOffset);
		return GripInRoot.Inverse() * FineTune;
	}

	/**
	 * A location given in WORLD units (cm) turned into the location to set RELATIVE to a parent whose space is scaled.
	 * Why: the arms come from Blender with scaled bones (the hand socket space can be x100). A child attached with
	 * "SnapToTargetNotIncludingScale" keeps its own WORLD scale, so its relative scale becomes 0.01, but a RELATIVE location is
	 * still multiplied by the parent scale: an offset of 27 cm would become 27 m. Dividing by the parent scale makes the offset
	 * come out in cm in the world, whatever the scale of the hand chain is.
	 */
	inline FVector WorldOffsetToRelative(const FVector& WorldOffset, const FVector& ParentScale)
	{
		auto Safe = [](double Scale) { return FMath::Abs(Scale) > UE_KINDA_SMALL_NUMBER ? Scale : 1.0; };
		return FVector(WorldOffset.X / Safe(ParentScale.X), WorldOffset.Y / Safe(ParentScale.Y), WorldOffset.Z / Safe(ParentScale.Z));
	}

	/**
	 * Box that wraps the skeletal mesh, for the root collision box (USunsetCovePivotBoxComponent), and where to put the mesh in it.
	 *   Origin Bottom (the default of items): the root is the center of the bottom face. The mesh is moved until the bottom of its
	 *                 bounds is ON the root, centered in X / Y. Padding is added on the sides and on top, never below: the base of
	 *                 the box is the base of the mesh, so the item stands on the ground with no gap, placed or fallen.
	 *   Origin Center: the root is the center of the box (the old behavior); the mesh is centered on it, Padding on every side.
	 * Physics turns the item around the center of the box in both cases (the center of mass follows the shape).
	 * @param OutBoxExtent            Half size of the box (cm), padding included.
	 * @param OutMeshRelativeLocation Where to put the mesh component relative to the root.
	 * @return false if the mesh component has no skeletal mesh asset (the outputs are not touched).
	 */
	inline bool ComputeFittedBox(const USkeletalMeshComponent* MeshComponent, float Padding, ESunsetCoveBoxOrigin Origin, FVector& OutBoxExtent, FVector& OutMeshRelativeLocation)
	{
		const USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh)
		{
			return false;
		}

		// Bounds of the mesh in its own space, brought into the space of the root (the mesh may be rotated or scaled in the Blueprint).
		const FBox MeshBox = Mesh->GetBounds().GetBox();
		const FTransform MeshRotationAndScale(MeshComponent->GetRelativeRotation(), FVector::ZeroVector, MeshComponent->GetRelativeScale3D());
		const FBox BoxInRoot = MeshBox.TransformBy(MeshRotationAndScale);
		const FVector MeshCenter = BoxInRoot.GetCenter();
		const FVector MeshExtent = BoxInRoot.GetExtent();

		if (Origin == ESunsetCoveBoxOrigin::Bottom)
		{
			// Half the padding on the half height = the whole padding on top only.
			OutBoxExtent = FVector(MeshExtent.X + Padding, MeshExtent.Y + Padding, MeshExtent.Z + Padding * 0.5f);
			OutMeshRelativeLocation = FVector(-MeshCenter.X, -MeshCenter.Y, -BoxInRoot.Min.Z);
		}
		else
		{
			OutBoxExtent = MeshExtent + FVector(Padding);
			OutMeshRelativeLocation = -MeshCenter;
		}
		return true;
	}

	/**
	 * The footprint of an item put down: its root box without the aiming padding (sides and top; the bottom too for a Center
	 * origin), in root space. The Prop Inventory rests the item on the ground and tests for obstacles with it.
	 */
	inline FBox ComputeFootprint(const USunsetCovePivotBoxComponent* Body, float Padding)
	{
		if (!Body)
		{
			return FBox(ForceInit);
		}

		FBox Box = Body->GetLocalBox();
		const float BottomPadding = Body->Origin == ESunsetCoveBoxOrigin::Bottom ? 0.0f : Padding;
		Box.Min += FVector(Padding, Padding, BottomPadding);
		Box.Max -= FVector(Padding);

		// Never inverted (a box smaller than its padding): at least 2 cm on every axis, growing up from the base.
		Box.Max = Box.Max.ComponentMax(Box.Min + FVector(2.0f));
		return Box;
	}

	/**
	 * An item that leaves the hand must not start inside a wall. Traces from the camera to the CENTER of the item's bounds (not its
	 * origin, which may be its base) and, when something is in between, moves the item so its center is Margin in front of the hit.
	 * Works for any origin.
	 */
	inline void KeepInFrontOfWall(UWorld* World, AActor* Item, const FVector& ViewLocation, ECollisionChannel Channel, const FCollisionQueryParams& Params, float Margin)
	{
		const UPrimitiveComponent* Root = Item ? Cast<UPrimitiveComponent>(Item->GetRootComponent()) : nullptr;
		if (!World || !Root)
		{
			return;
		}

		const FVector Center = Root->Bounds.Origin;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, Center, Channel, Params))
		{
			const FVector OriginToCenter = Center - Item->GetActorLocation();
			Item->SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * Margin - OriginToCenter, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}
