// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "HeroCameraComponent.generated.h"

class UAscenduntCameraMode;

/*
* Hero Camera Component that Handling Camera for player controlled pawns.
* 
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ASCENDUNT_API UHeroCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHeroCameraComponent(const FObjectInitializer& ObjectInitializer);

	/** Returns the hero camera component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Hero")
	static UHeroCameraComponent* FindHeroCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UHeroCameraComponent>() : nullptr); }

	/** Overrides the camera from an active gameplay ability */
	void SetAbilityCameraMode(TSubclassOf<UAscenduntCameraMode> CameraMode, const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** Clears the camera override if it is set */
	void ClearAbilityCameraMode(const FGameplayAbilitySpecHandle& OwningSpecHandle);

	/** Local presentation override used while the possessed PvE hero is Downed. */
	void SetDownedCameraModeActive(bool bActive);

	/**
	 * 3인칭 보기를 껐다 켠다. 로컬 표현 전용이며 리플리케이트되지 않는다.
	 *
	 * 어빌리티가 세팅하는 AbilityCameraMode 와 달리 어빌리티 수명과 무관하게 래치된다.
	 * 그래서 키를 누르고 있는 동안만이 아니라 다시 누를 때까지 유지된다.
	 *
	 * @param CameraMode 켤 때 사용할 3인칭 모드. 널이면 아무 것도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ascendunt|Camera")
	void ToggleThirdPersonView(TSubclassOf<UAscenduntCameraMode> CameraMode);

	/**
	 * 구현이 .cpp 에 있는 이유:
	 * TSubclassOf 를 nullptr 과 비교하면 operator UClass* -> T::StaticClass() 가 인스턴스화되어
	 * 완전한 타입이 필요해진다. 이 헤더는 UAscenduntCameraMode 를 전방 선언만 하고 있어서
	 * 인라인으로 두면 컴파일되지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Camera")
	bool IsThirdPersonViewActive() const;

	TSubclassOf<UAscenduntCameraMode> DetermineCameraMode() const;

protected:
	/** Third-person mode used while the local possessed hero is Downed. */
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt|PvE|Downed")
	TSubclassOf<UAscenduntCameraMode> DownedCameraModeClass;

	/**
	 * 토글로 켜둔 3인칭 모드. 널이면 꺼진 상태다.
	 * 우선순위는 Downed > 어빌리티 > 이 값 > 폰 데이터 기본값이다.
	 * 어빌리티 카메라(궁극기 연출 등)보다 아래에 둬야 연출이 이 토글에 덮이지 않는다.
	 */
	UPROPERTY()
	TSubclassOf<UAscenduntCameraMode> ThirdPersonViewModeClass;

	/** Camera mode set by an ability. */
	UPROPERTY()
	TSubclassOf<UAscenduntCameraMode> AbilityCameraMode;

	/** Spec handle for the last ability to set a camera mode. */
	FGameplayAbilitySpecHandle AbilityCameraModeOwningSpecHandle;

	bool bDownedCameraModeActive = false;
};
