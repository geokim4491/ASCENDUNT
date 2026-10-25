// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "AscenduntCameraMode_ThirdPerson.generated.h"

/**
 * Third-person camera mode.
 *
 * Socket is used as a camera pivot only, so animation-driven vertical motion can be followed.
 * The final camera location is calculated from the view/control rotation, so TurnInPlace mesh
 * rotation lag does not make the camera stay on the old mesh-facing direction.
 */
UCLASS()
class ASCENDUNT_API UAscenduntCameraMode_ThirdPerson : public UAscenduntCameraMode
{
	GENERATED_BODY()
	
public:
	UAscenduntCameraMode_ThirdPerson();

protected:
	virtual FVector GetPivotLocation() const override;

protected:
	// If true, use a mesh socket/bone as the camera pivot location.
	// Put this socket near the character center/head/chest, not at the final camera position.
	UPROPERTY(EditDefaultsOnly, Category = "ThirdPerson|Pivot")
	bool bUsePivotSocket;

	// Socket or bone name used as the camera pivot. SkeletalMeshComponent can resolve bones as sockets.
	// Kept as HeadSocketName to avoid breaking existing Blueprint/default references.
	UPROPERTY(EditDefaultsOnly, Category = "ThirdPerson|Pivot", meta = (EditCondition = "bUsePivotSocket", DisplayName = "Pivot Socket Name"))
	FName HeadSocketName;

	// Local offset from the pivot socket/bone.
	UPROPERTY(EditDefaultsOnly, Category = "ThirdPerson|Pivot", meta = (EditCondition = "bUsePivotSocket", DisplayName = "Pivot Socket Local Offset"))
	FVector HeadSocketOffset;

	// Final camera offset from the pivot, applied in view/control-rotation space.
	// X = forward/backward, Y = right/left, Z = world up/down. Use negative X to move behind the character.
	UPROPERTY(EditDefaultsOnly, Category = "ThirdPerson|Camera")
	FVector ViewSpaceCameraOffset;
};
