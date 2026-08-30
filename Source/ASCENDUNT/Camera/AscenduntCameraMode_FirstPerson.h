// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AscenduntCameraMode.h"
#include "AscenduntCameraMode_FirstPerson.generated.h"

/**
 * First-person camera mode.
 * Camera pivot is driven by a head socket (or head bone name) on the character mesh.
 */
UCLASS()
class ASCENDUNT_API UAscenduntCameraMode_FirstPerson : public UAscenduntCameraMode
{
	GENERATED_BODY()
	
public:
	UAscenduntCameraMode_FirstPerson();

protected:
	virtual FVector GetPivotLocation() const override;

protected:
	// Socket (or bone) name to drive the camera location. (SkeletalMeshComponent treats bones as sockets for transforms)
	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson")
	FName HeadSocketName;

	// Offset in the socket/bone local space (X forward, Y right, Z up).
	UPROPERTY(EditDefaultsOnly, Category = "FirstPerson")
	FVector HeadSocketOffset;
	
	
};
