#pragma once

#include "CoreMinimal.h"
#include "GA_BaseFire.h"
#include "GA_FireControlBase.generated.h"

class UGA_FireShot;
class UAbilityTask_WaitInputRelease;

/** Owns input lifetime; individual shots execute through ShotAbilityClass. */
UCLASS(Abstract)
class ASCENDUNT_API UGA_FireControlBase : public UGA_BaseFire
{
	GENERATED_BODY()

public:
	UGA_FireControlBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire|Control")
	TSubclassOf<UGA_FireShot> ShotAbilityClass;

	virtual bool CanIssueShot() const
	{
		return IsActive() && !bInputReleased;
	}

	virtual float GetShotDamageCoefficient() const
	{
		return 1.f;
	}

	virtual void ConsumeShotPermission() {}

	bool MatchesShotClass(const UClass* Class) const;

	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
	    FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	    bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual bool CheckCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    FGameplayTagContainer* OptionalRelevantTags = nullptr) const override
	{
		return true;
	}

	virtual void ApplyCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    FGameplayAbilityActivationInfo ActivationInfo) const override
	{
	}

	virtual void InputReleased(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	    FGameplayAbilityActivationInfo ActivationInfo) override;
	bool RequestShot();

	virtual void OnControlReleased() PURE_VIRTUAL(UGA_FireControlBase::OnControlReleased, );

	UFUNCTION()
	void HandleControlReleased(float TimeHeld);

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputRelease> ReleaseTask;
	bool bInputReleased = false;
};
