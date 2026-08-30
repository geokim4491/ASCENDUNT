// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntCameraMode_FirstPerson.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UAscenduntCameraMode_FirstPerson::UAscenduntCameraMode_FirstPerson()
{
	// FPS 기본값 느낌으로
	FieldOfView = 90.0f;
	BlendTime = 0.10f;

	// NAME_None 이면 Super::GetPivotLocation() 으로 떨어진다.
	// Mesh1P 를 카메라 컴포넌트에 부착하는 구조에서는 이쪽이 기본값이어야 한다.
	// 머리 본을 피벗으로 쓰면 3인칭 로코모션의 헤드 보빙이 그대로 카메라 흔들림으로 새어나오고,
	// Super 경로는 캡슐 + BaseEyeHeight 기반이라 크라우치 높이 보정까지 해준다.
	HeadSocketName = NAME_None;
	HeadSocketOffset = FVector::ZeroVector;
}

FVector UAscenduntCameraMode_FirstPerson::GetPivotLocation() const
{
	const AActor* TargetActor = GetTargetActor();
	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return Super::GetPivotLocation();
	}

	const USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
	if (!Mesh)
	{
		return Super::GetPivotLocation();
	}

	// USkeletalMeshComponent는 본 이름도 소켓처럼 transform을 뽑을 수 있음.
	// HeadSocketName이 실제 Socket이면 Socket, 아니면 Bone이라도 동작하는 케이스가 많음.
	if (HeadSocketName != NAME_None && Mesh->DoesSocketExist(HeadSocketName))
	{
		const FTransform HeadXf = Mesh->GetSocketTransform(HeadSocketName, RTS_World);
		return HeadXf.TransformPosition(HeadSocketOffset);
	}

	// Fallback: 그냥 기존(eye height 기반)
	return Super::GetPivotLocation();
}
