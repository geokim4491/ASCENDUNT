#pragma once
#include "CoreMinimal.h"
#include "GA_FireControlBase.h"
#include "GA_Fire_Auto.generated.h"

class UAbilityTask_WaitDelay;

/** Input controller. Remote player instances never schedule server-side shots. */
UCLASS()
class ASCENDUNT_API UGA_Fire_Auto : public UGA_FireControlBase
{
	GENERATED_BODY()

public:
	UGA_Fire_Auto();

protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnControlReleased() override;

	UFUNCTION()
	void HandleTickFire();
	void ScheduleNextTick();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> TickTask;
};
