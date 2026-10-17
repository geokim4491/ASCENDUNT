// Fill out your copyright notice in the Description page of Project Settings.

#include "AscenduntCameraMode_DeathSpectatorThirdPerson.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UAscenduntCameraMode_DeathSpectatorThirdPerson::UAscenduntCameraMode_DeathSpectatorThirdPerson()
{
	FieldOfView = 95.0f;
	BlendTime = 0.2f;
	BlendFunction = EAscenduntCameraModeBlendFunction::EaseOut;
	BlendExponent = 4.0f;

	// Keep the living target composition close to the normal third-person camera.
	LivingPivotSocketName = TEXT("TP_Camera");
	LivingPivotSocketOffset = FVector::ZeroVector;
	LivingViewSpaceCameraOffset = FVector(-260.0f, 55.0f, 55.0f);

	// Follow the physical body position, but never inherit pelvis rotation.
	RagdollPivotBoneName = TEXT("pelvis");
	RagdollPivotBoneOffset = FVector(0.0f, 0.0f, 20.0f);
	RagdollViewSpaceCameraOffset = FVector(-320.0f, 70.0f, 110.0f);
	RagdollPivotInterpSpeed = 8.0f;
}

void UAscenduntCameraMode_DeathSpectatorThirdPerson::OnActivation()
{
	Super::OnActivation();
	bHasSmoothedPivot = false;
}

void UAscenduntCameraMode_DeathSpectatorThirdPerson::OnDeactivation()
{
	bHasSmoothedPivot = false;
	Super::OnDeactivation();
}

void UAscenduntCameraMode_DeathSpectatorThirdPerson::UpdateView(float DeltaTime)
{
	bool bRagdollTarget = false;
	const FVector DesiredPivotLocation = ResolveDesiredPivotLocation(bRagdollTarget);

	if (!bHasSmoothedPivot || bResetInterpolation || !bRagdollTarget || RagdollPivotInterpSpeed <= 0.0f)
	{
		SmoothedPivotLocation = DesiredPivotLocation;
		bHasSmoothedPivot = true;
	}
	else
	{
		SmoothedPivotLocation = FMath::VInterpTo(
			SmoothedPivotLocation,
			DesiredPivotLocation,
			DeltaTime,
			RagdollPivotInterpSpeed);
	}

	bResetInterpolation = false;

	FRotator ViewRotation = ResolveSpectatorViewRotation();
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, ViewPitchMin, ViewPitchMax);
	ViewRotation.Roll = 0.0f;

	const FVector CameraOffset = bRagdollTarget
		? RagdollViewSpaceCameraOffset
		: LivingViewSpaceCameraOffset;

	const FRotationMatrix ViewRotationMatrix(ViewRotation);
	const FVector Forward = ViewRotationMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = ViewRotationMatrix.GetUnitAxis(EAxis::Y);

	View.Location = SmoothedPivotLocation
		+ (Forward * CameraOffset.X)
		+ (Right * CameraOffset.Y)
		+ (FVector::UpVector * CameraOffset.Z);
	View.Rotation = ViewRotation;
	View.ControlRotation = ViewRotation;
	View.FieldOfView = FieldOfView;
}

FVector UAscenduntCameraMode_DeathSpectatorThirdPerson::ResolveDesiredPivotLocation(bool& bOutRagdollTarget) const
{
	bOutRagdollTarget = false;

	const ACharacter* TargetCharacter = Cast<ACharacter>(GetTargetActor());
	const USkeletalMeshComponent* Mesh = TargetCharacter ? TargetCharacter->GetMesh() : nullptr;
	if (!TargetCharacter || !Mesh)
	{
		return UAscenduntCameraMode::GetPivotLocation();
	}

	bOutRagdollTarget = Mesh->IsAnySimulatingPhysics();
	const FName PivotName = bOutRagdollTarget ? RagdollPivotBoneName : LivingPivotSocketName;
	const FVector PivotOffset = bOutRagdollTarget ? RagdollPivotBoneOffset : LivingPivotSocketOffset;

	if (PivotName != NAME_None && Mesh->DoesSocketExist(PivotName))
	{
		const FTransform PivotTransform = Mesh->GetSocketTransform(PivotName, RTS_World);
		return PivotTransform.TransformPosition(PivotOffset);
	}

	if (bOutRagdollTarget)
	{
		// The capsule no longer follows a ragdoll. Mesh bounds are a safer fallback than ActorLocation.
		return Mesh->Bounds.Origin + FVector::UpVector * RagdollPivotBoneOffset.Z;
	}

	return UAscenduntCameraMode::GetPivotLocation();
}

FRotator UAscenduntCameraMode_DeathSpectatorThirdPerson::ResolveSpectatorViewRotation() const
{
	if (const APlayerController* ViewingController = FindLocalViewingController())
	{
		FRotator Result = ViewingController->GetControlRotation();
		Result.Roll = 0.0f;
		return Result;
	}

	FRotator Result = UAscenduntCameraMode::GetPivotRotation();
	Result.Roll = 0.0f;
	return Result;
}

APlayerController* UAscenduntCameraMode_DeathSpectatorThirdPerson::FindLocalViewingController() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* FirstLocalController = nullptr;
	const AActor* TargetActor = GetTargetActor();

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController || !PlayerController->IsLocalController())
		{
			continue;
		}

		if (!FirstLocalController)
		{
			FirstLocalController = PlayerController;
		}

		if (PlayerController->GetViewTarget() == TargetActor)
		{
			return PlayerController;
		}
	}

	return FirstLocalController;
}
