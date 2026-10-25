// Fill out your copyright notice in the Description page of Project Settings.


#include "AscenduntPlayerCameraManager.h"

#include "Async/TaskGraphInterfaces.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "AscenduntCameraComponent.h"
#include "AscUICameraManagerComponent.h"

class FDebugDisplayInfo;

static FName UICameraComponentName(TEXT("UICamera"));

AAscenduntPlayerCameraManager::AAscenduntPlayerCameraManager(const FObjectInitializer& ObjectInitializer)
{
	DefaultFOV = ASCENDUNT_CAMERA_DEFAULT_FOV;
	ViewPitchMin = ASCENDUNT_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = ASCENDUNT_CAMERA_DEFAULT_PITCH_MAX;

	UICamera = CreateDefaultSubobject<UAscUICameraManagerComponent>(UICameraComponentName);
}

UAscUICameraManagerComponent* AAscenduntPlayerCameraManager::GetUICameraComponent() const
{
	return UICamera;
}

void AAscenduntPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	// If the UI Camera is looking at something, let it have priority.
	if (UICamera->NeedsToUpdateViewTarget())
	{
		Super::UpdateViewTarget(OutVT, DeltaTime);
		UICamera->UpdateViewTarget(OutVT, DeltaTime);
		return;
	}

	Super::UpdateViewTarget(OutVT, DeltaTime);
}

void AAscenduntPlayerCameraManager::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetFont(GEngine->GetSmallFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("AscenduntPlayerCameraManager: %s"), *GetNameSafe(this)));

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	const APawn* Pawn = (PCOwner ? PCOwner->GetPawn() : nullptr);

	if (const UAscenduntCameraComponent* CameraComponent = UAscenduntCameraComponent::FindCameraComponent(Pawn))
	{
		CameraComponent->DrawDebug(Canvas);
	}
}
