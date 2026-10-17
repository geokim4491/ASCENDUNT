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

	/**
	 * 1인칭 무기/팔의 ADS 전용 Scale / FOV. Hip-fire 값(FirstPersonScale / FirstPersonFieldOfView)
	 * 과 별개로 둬서, ADS 로 총이 카메라에 바짝 붙을 때만 더 강하게 축소할 수 있게 한다.
	 *
	 * 별도 On/Off 나 알파 변수를 새로 만들지 않고 위 ADSBlendAlpha 를 그대로 재사용한다.
	 * GA_ADS 가 SetADSFieldOfView() 로 이미 켜고 끄는 그 블렌드와 같은 타이밍으로 전환돼야
	 * 씬 FOV 줌과 1인칭 축소가 같이 움직이기 때문이다. 즉 GA_ADS.cpp 는 수정할 필요가 없다.
	 */
	UPROPERTY(EditAnywhere, Category = "Ascendunt|Camera|ADS", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float ADSFirstPersonScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Ascendunt|Camera|ADS", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float ADSFirstPersonFieldOfView = 90.0f;

	/** Runtime-only: do not replicate, because only the owning local player's view should be grayscale. */
	bool bDownedGrayscaleEnabled = false;
	
};
