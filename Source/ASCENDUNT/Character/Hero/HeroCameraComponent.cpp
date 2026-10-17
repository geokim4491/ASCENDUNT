// Fill out your copyright notice in the Description page of Project Settings.

#include "HeroCameraComponent.h"
#include "ASCENDUNT/Player/AscenduntPlayerState.h"
#include "ASCENDUNT/Character/Hero/HeroPawnData.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode.h"
#include "ASCENDUNT/Camera/AscenduntCameraMode_ThirdPerson.h"


UHeroCameraComponent::UHeroCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilityCameraMode = nullptr;
	DownedCameraModeClass = UAscenduntCameraMode_ThirdPerson::StaticClass();
}

void UHeroCameraComponent::SetAbilityCameraMode(TSubclassOf<UAscenduntCameraMode> CameraMode, const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (CameraMode)
	{
		AbilityCameraMode = CameraMode;
		AbilityCameraModeOwningSpecHandle = OwningSpecHandle;
	}
}

void UHeroCameraComponent::ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle)
{
	if (AbilityCameraModeOwningSpecHandle == OwningSpecHandle)
	{
		AbilityCameraMode = nullptr;
		AbilityCameraModeOwningSpecHandle = FGameplayAbilitySpecHandle();
	}
}

void UHeroCameraComponent::SetDownedCameraModeActive(bool bActive)
{
	bDownedCameraModeActive = bActive;
}

void UHeroCameraComponent::ToggleThirdPersonView(TSubclassOf<UAscenduntCameraMode> CameraMode)
{
	if (ThirdPersonViewModeClass)
	{
		ThirdPersonViewModeClass = nullptr;
		return;
	}

	// 켜는데 모드가 안 넘어오면 조용히 무시한다. 널을 래치해두면 다음 입력이
	// "끄기" 로 해석되어 토글이 한 번 어긋난 채로 굳는다.
	if (CameraMode)
	{
		ThirdPersonViewModeClass = CameraMode;
	}
}

bool UHeroCameraComponent::IsThirdPersonViewActive() const
{
	return ThirdPersonViewModeClass != nullptr;
}

TSubclassOf<UAscenduntCameraMode> UHeroCameraComponent::DetermineCameraMode() const
{
	// Downed presentation has higher priority than any stale ability camera override.
	if (bDownedCameraModeActive && DownedCameraModeClass)
	{
		return DownedCameraModeClass;
	}

	if (AbilityCameraMode)
	{
		return AbilityCameraMode;
	}

	if (ThirdPersonViewModeClass)
	{
		return ThirdPersonViewModeClass;
	}

	AActor* Actor = GetOwner();
	if (!Actor) return nullptr;

	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn)
	{
		return nullptr;
	}

	if (AAscenduntPlayerState* APS = Cast<AAscenduntPlayerState>(Pawn->GetPlayerState()))
	{
		if (const UHeroPawnData* PawnData = APS->GetPawnData())
		{
			return PawnData->DefaultCameraMode;
		}
	}

	return nullptr;
}
