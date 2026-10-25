// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntCameraMode_ThirdPerson.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"

UAscenduntCameraMode_ThirdPerson::UAscenduntCameraMode_ThirdPerson()
{
	FieldOfView = 100.0f;

	BlendTime = 0.8f;
	BlendFunction = EAscenduntCameraModeBlendFunction::EaseInOut;
	BlendExponent = 4.0f;

	// 기본은 캡슐 기준 피벗(Super::GetPivotLocation)이다. 라이라 원본과 동일한 거동으로,
	// ULyraCameraMode_ThirdPerson도 GetPivotLocation을 오버라이드하지 않고 캡슐 위치를 그대로 쓴다.
	//
	// 메시 소켓을 피벗으로 쓰면 점프 착지 리커버리나 걷기 상하 운동 같은 애니메이션이
	// 그대로 카메라 흔들림이 된다. 시야는 애니메이션에서 분리해두고, 흔들림이 필요하면
	// CameraShake로 명시적으로 주는 편이 강도와 타이밍을 제어할 수 있어 낫다.
	//
	// 소켓 피벗이 필요한 연출이 생기면 이 값을 켜고 HeadSocketName을 지정한다.
	bUsePivotSocket = false;
	HeadSocketName = TEXT("TP_Camera");
	HeadSocketOffset = FVector::ZeroVector;

	// Camera location relative to the pivot in view/control-rotation space.
	// X negative = behind the character, Y positive = right shoulder, Z positive = higher.
	ViewSpaceCameraOffset = FVector(-220.0f, 0.0f, 60.0f);
}

FVector UAscenduntCameraMode_ThirdPerson::GetPivotLocation() const
{
	FVector PivotLocation = Super::GetPivotLocation();

	const AActor* TargetActor = GetTargetActor();
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	const USkeletalMeshComponent* Mesh = TargetCharacter ? TargetCharacter->GetMesh() : nullptr;

	// Use socket/bone position only as the pivot. Do not use socket rotation for camera offset.
	// This lets jump/root/mesh-driven vertical animation move the pivot while TurnInPlace still
	// uses the current view/control rotation for the final camera offset.
	if (bUsePivotSocket && Mesh && HeadSocketName != NAME_None && Mesh->DoesSocketExist(HeadSocketName))
	{
		const FTransform SocketTransform = Mesh->GetSocketTransform(HeadSocketName, RTS_World);
		PivotLocation = SocketTransform.TransformPosition(HeadSocketOffset);
	}

	FRotator ViewRotation = GetPivotRotation();
	ViewRotation.Pitch = FMath::ClampAngle(ViewRotation.Pitch, ViewPitchMin, ViewPitchMax);
	ViewRotation.Roll = 0.0f;

	const FVector Forward = ViewRotation.Vector();
	const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const FVector Up = FVector::UpVector;

	return PivotLocation
		+ (Forward * ViewSpaceCameraOffset.X)
		+ (Right * ViewSpaceCameraOffset.Y)
		+ (Up * ViewSpaceCameraOffset.Z);
}
