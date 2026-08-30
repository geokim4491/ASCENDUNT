// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

#include "AscenduntCameraComponent.generated.h"

class UCanvas;
class UAscenduntCameraMode;
class UAscenduntCameraModeStack;
class UObject;
struct FFrame;
struct FGameplayTag;
struct FMinimalViewInfo;
template <class TClass> class TSubclassOf;

DECLARE_DELEGATE_RetVal(TSubclassOf<UAscenduntCameraMode>, FAscenduntCameraModeDelegate);

/**
 * UAscenduntCameraComponent
 * 
 * The base camera component class used by this project.
 */
UCLASS()
class ASCENDUNT_API UAscenduntCameraComponent : public UCameraComponent
{
	GENERATED_BODY()
	
public:

	UAscenduntCameraComponent(const FObjectInitializer& ObjectInitializer);

	// Returns the camera component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Camera")
	static UAscenduntCameraComponent* FindCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UAscenduntCameraComponent>() : nullptr); }

	// Returns the target actor that the camera is looking at.
	virtual AActor* GetTargetActor() const { return GetOwner(); }

	// Delegate used to query for the best camera mode.
	FAscenduntCameraModeDelegate DetermineCameraModeDelegate;

	// Add an offset to the field of view.  The offset is only for one frame, it gets cleared once it is applied.
	void AddFieldOfViewOffset(float FovOffset) { FieldOfViewOffset += FovOffset; }

	/** Local-only ADS presentation. The owning ADS ability supplies the weapon's target FOV. */
	void SetADSFieldOfView(bool bEnabled, float InTargetFOV);

	/** Owner-local grayscale presentation used only while that player is PvE Downed. */
	void SetDownedGrayscaleEnabled(bool bEnabled) { bDownedGrayscaleEnabled = bEnabled; }

	virtual void DrawDebug(UCanvas* Canvas) const;

	// Gets the tag associated with the top layer and the blend weight of it
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

protected:

	virtual void OnRegister() override;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	virtual void UpdateCameraModes();

protected:

	// Stack used to blend the camera modes.
	UPROPERTY()
	TObjectPtr<UAscenduntCameraModeStack> CameraModeStack;

	// Offset applied to the field of view.  The offset is only for one frame, it gets cleared once it is applied.
	float FieldOfViewOffset;

	/** ADS is presentation-only here; authoritative gameplay state must live elsewhere. */
	bool bADSFieldOfViewEnabled = false;
	float ADSFieldOfView = 70.0f;
	float ADSBlendAlpha = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Camera|ADS", meta = (ClampMin = "0.1"))
	float ADSBlendSpeed = 14.0f;

	/** Runtime-only: do not replicate, because only the owning local player's view should be grayscale. */
	bool bDownedGrayscaleEnabled = false;
	
};
