// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HeroInputComponent.generated.h"

class UInputComponent;
class UAscenduntInputConfig;
struct FGameplayTag;
class UInputMappingContext;
struct FInputActionValue;
class UEnhancedInputLocalPlayerSubsystem;
class UAscenduntAbilitySystemComponent;
class AAscenduntPlayerController;

USTRUCT()
struct FInputMappingContextAndPriority
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TObjectPtr<UInputMappingContext> InputMapping;

	// Higher priority input mappings will be prioritized over mappings with a lower priority.
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ASCENDUNT_API UHeroInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHeroInputComponent(const FObjectInitializer& ObjectInitializer);

	bool CanInitializePlayerInput(UInputComponent* PlayerInputComponent) const;
	bool InitializePlayerInput(UInputComponent* PlayerInputComponent);
	void UninitializePlayerInput(bool bForceRemoveMappings = false);
	void RemoveIMC(UInputMappingContext* Context);
	void ApplyIMC(UInputMappingContext* Context);

	/** Local presentation state used only to select the user ADS sensitivity multiplier. */
	void SetADSActive(bool bEnabled);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	void Input_AbilityInputTagReleased(FGameplayTag InputTag);
	void Input_AxisAbilityTriggered(
		const FInputActionValue& InputActionValue,
		FGameplayTag PositiveInputTag,
		FGameplayTag NegativeInputTag);

	void Input_Move(const FInputActionValue& InputActionValue);
	void Input_Look(const FInputActionValue& InputActionValue);
	void Input_Crouch(const FInputActionValue& InputActionValue);

protected:
	UPROPERTY(EditAnywhere)
	TArray<FInputMappingContextAndPriority> InputMappings;

	UPROPERTY(Transient)
	TSet<TObjectPtr<const UInputMappingContext>> ActiveHeroIMCs;

private:
	UEnhancedInputLocalPlayerSubsystem* GetEnhancedSubsystem() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedPawn;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAscenduntAbilitySystemComponent> CachedASC;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CachedInputSubsystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<AAscenduntPlayerController> CachedPlayerController;

	bool bADSActive = false;
};
