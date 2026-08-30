#pragma once

#include "CoreMinimal.h"
#include "ASCENDUNT/AbilitySystem/Abilities/Common/GA_DataAbility.h"
#include "ASCENDUNT/Character/Hero/Hakuko/HakukoTypes.h"
#include "GA_HakukoSkills.generated.h"

class UHakukoBioGrenadeData;
class UHakukoStimData;
class UHakukoHealGrenadeData;
class UHakukoUltimateData;
struct FHakukoGrenadeProjectileConfig;

/** Deprecated scroll-selector shell kept so existing BP assets do not lose their native parent. */
UCLASS()
class ASCENDUNT_API UGA_HakukoSelectFox : public UAscenduntGameplayAbility
{
	GENERATED_BODY()
public:
	UGA_HakukoSelectFox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Legacy serialized BP property; selector ability is now a no-op. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Fox", meta=(DeprecatedProperty, DeprecationMessage="Fox selection was removed"))
	EHakukoFoxType FoxTypeToSelect = EHakukoFoxType::Attack;
};

UCLASS()
class ASCENDUNT_API UGA_HakukoBioGrenade : public UGA_DataAbility
{
	GENERATED_BODY()
public:
	UGA_HakukoBioGrenade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual const UAscAbilityData* GetAbilityData() const override;
private:
	void ThrowGrenade();
	FTimerHandle ThrowTimerHandle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilityData", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHakukoBioGrenadeData> BioGrenadeData;
};

/** Existing E BP native parent now behaves as Bio Grenade. */
UCLASS()
class ASCENDUNT_API UGA_HakukoSendFox : public UGA_HakukoBioGrenade
{
	GENERATED_BODY()
public:
	UGA_HakukoSendFox(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS()
class ASCENDUNT_API UGA_HakukoStim : public UGA_DataAbility
{
	GENERATED_BODY()
public:
	UGA_HakukoStim(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual const UAscAbilityData* GetAbilityData() const override;
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilityData", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHakukoStimData> StimData;
};

/** Existing Shift BP native parent now behaves as Stim. */
UCLASS()
class ASCENDUNT_API UGA_HakukoDash : public UGA_HakukoStim
{
	GENERATED_BODY()
public:
	UGA_HakukoDash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	/** Legacy serialized BP properties retained only so existing Dash children load cleanly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Dash", meta=(DeprecatedProperty, DeprecationMessage="Dash was replaced by Stim"))
	float DashStrength = 1200.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hakuko|Dash", meta=(DeprecatedProperty, DeprecationMessage="Dash was replaced by Stim"))
	float DashUpwardVelocity = 0.0f;
};

UCLASS()
class ASCENDUNT_API UGA_HakukoHealGrenade : public UGA_DataAbility
{
	GENERATED_BODY()
public:
	UGA_HakukoHealGrenade(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual const UAscAbilityData* GetAbilityData() const override;
private:
	void FireHealGrenade();
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilityData", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHakukoHealGrenadeData> HealGrenadeData;
};

UCLASS()
class ASCENDUNT_API UGA_HakukoUltimate : public UGA_DataAbility
{
	GENERATED_BODY()
public:
	UGA_HakukoUltimate(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual const UAscAbilityData* GetAbilityData() const override;
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AbilityData", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UHakukoUltimateData> UltimateData;
};
