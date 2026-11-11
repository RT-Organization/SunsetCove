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
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

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
	 * Box that wraps the skeletal mesh, for the root collision box. The root is the CENTER of the box (it is also the point
	 * physics turns the item around), so the mesh has to be moved until its own center is on the root.
	 * @param OutBoxExtent            Half size of the box (cm), with Padding added on every side.
	 * @param OutMeshRelativeLocation Where to put the mesh component relative to the root so the mesh is centered in the box.
	 * @return false if the mesh component has no skeletal mesh asset (the outputs are not touched).
	 */
	inline bool ComputeFittedBox(const USkeletalMeshComponent* MeshComponent, float Padding, FVector& OutBoxExtent, FVector& OutMeshRelativeLocation)
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

		OutBoxExtent = BoxInRoot.GetExtent() + FVector(Padding);
		OutMeshRelativeLocation = -BoxInRoot.GetCenter();
		return true;
	}
}
