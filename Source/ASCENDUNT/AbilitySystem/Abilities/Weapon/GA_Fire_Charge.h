#pragma once
#include "CoreMinimal.h"
#include "GA_FireControlBase.h"
#include "GA_Fire_Charge.generated.h"

UCLASS()
class ASCENDUNT_API UGA_Fire_Charge : public UGA_FireControlBase
{
	GENERATED_BODY()

public:
	UGA_Fire_Charge();

	virtual bool CanIssueShot() const override
	{
		return IsActive() && bInputReleased && !bShotConsumed;
	}

	virtual float GetShotDamageCoefficient() const override;

	virtual void ConsumeShotPermission() override
	{
		bShotConsumed = true;
	}

protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnControlReleased() override;
	void ExpireReleasedControl();
	double ChargeStartTimeSec = 0.;
	float ReleasedHeldTime = 0.f;
	bool bShotConsumed = false;
	FTimerHandle ReleaseExpiryTimer;
};
