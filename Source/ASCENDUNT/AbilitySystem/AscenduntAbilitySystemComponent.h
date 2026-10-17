// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AscenduntAbilitySystemComponent.generated.h"

class UAscenduntGameplayAbility;
class UTexture2D;
enum class EAscenduntAbilityActivationGroup : uint8;

/*
*  라우터에선 전송 방식 즉, GC를 어디서 실행할지 결정한다. (전송 방식)
* 
*  클라이언트 코드에선 LocalOnly를 선택해서 사용한다.
* 
*  서버 코드에선 ReplicatedToRelevant, TargetOwnerOnly, InstigatorOnly, InstigatorOnly_CueTarget 중에 선택해서 사용한다.
*  서버 코드에서 LocalOnly로 하면, 서버에서 실행되는 이상한 결과가 발생함.
*   
*/
UENUM(BlueprintType)
enum class EAscenduntCueRoute : uint8 
{
	/** 모든 클라에 복제. 특정 타깃 제외하려면, GCN에서 설정. */ 
	ReplicatedToRelevant,   

	/** 플레이하는 컴퓨터에서만 ExecuteGameplayCueLocal */
	LocalOnly,              

	/** 서버->타깃에게만(Client RPC로 Local 실행) */
	TargetOwnerOnly,        

	/* 서버->가해자에게만(Client RPC로 Local 실행) 
	*  가해자 A의 화면에만 히트 마커가 뜨고, 피해자 B의 몸에는 VFX가 안 붙는 경우가 여기에 해당.
	*/
	InstigatorOnly,

	/** 피해자 B의 몸에 VFX를 붙이고, 가해자 A의 화면에만 띄운다. */
	InstigatorOnly_CueTarget,
};

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Gameplay_AbilityInputBlocked);

USTRUCT(BlueprintType)
struct FAscAbilityUIState
{
	GENERATED_BODY()

	/** Icon comes from UAscAbilityData::AbilityIcon. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	TObjectPtr<UTexture2D> AbilityIcon = nullptr;

	/** Charge-based skills show this number over the icon. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	bool bUsesStock = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	int32 CurrentStock = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	int32 MaxStock = 0;

	/** True while a stock is recharging or a normal GAS cooldown is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	bool bShowCooldownOverlay = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float TimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float Duration = 0.0f;

	/** 1 immediately after use, decreasing to 0 as the yellow overlay is removed. */
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Ability UI")
	float OverlayPercent = 0.0f;
};

/**
 * 
 */
UCLASS()
class ASCENDUNT_API UAscenduntAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UAscenduntAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	//~UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	void ProcessAbilityInput(float DeltaTime, bool bGamePaused);
	void ClearAbilityInput();

	/** Re-runs ActivationPolicy::OnSpawn abilities after a preserved AbilitySet receives a new AvatarActor. */
	void TryActivateAbilitiesOnSpawn();

	/** UI helper: returns the replicated/predicted GAS cooldown for an ability identity tag. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|Cooldown")
	bool GetAbilityCooldownByAbilityTag(FGameplayTag AbilityTag, float& TimeRemaining, float& CooldownDuration) const;

	/** UI helper: same query using the InputTag assigned by AbilitySet. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|Cooldown")
	bool GetAbilityCooldownByInputTag(FGameplayTag InputTag, float& TimeRemaining, float& CooldownDuration) const;

	/** Unified HUD query: stock recharge for charge skills, GAS cooldown for normal skills. */
	UFUNCTION(BlueprintPure, Category = "Ascendunt|Ability|UI")
	bool GetAbilityUIStateByInputTag(FGameplayTag InputTag, FAscAbilityUIState& OutState) const;


	bool IsActivationGroupBlocked(EAscenduntAbilityActivationGroup Group) const;

	void AddAbilityToActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability);
	void RemoveAbilityFromActivationGroup(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* Ability);

	void CancelActivationGroupAbilities(EAscenduntAbilityActivationGroup Group, UAscenduntGameplayAbility* IgnoreAbility = nullptr);

	// ======= GameplayCue =========
	// 로컬에서만 1회 Execute
	UFUNCTION(BlueprintCallable, Category = "GameplayCue",
		Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void ExecuteGameplayCueLocal(const FGameplayTag GameplayCueTag,
		const FGameplayCueParameters& GameplayCueParameters);

	// 로컬에서만 Add/Remove(지속형 큐 필요하면)
	UFUNCTION(BlueprintCallable, Category = "GameplayCue",
		Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void AddGameplayCueLocal(const FGameplayTag GameplayCueTag,
		const FGameplayCueParameters& GameplayCueParameters);

	UFUNCTION(BlueprintCallable, Category = "GameplayCue",
		Meta = (AutoCreateRefTerm = "GameplayCueParameters", GameplayTagFilter = "GameplayCue"))
	void RemoveGameplayCueLocal(const FGameplayTag GameplayCueTag,
		const FGameplayCueParameters& GameplayCueParameters);

	static void AscRouteCue(
		/** 누가 원인(가해자/시전자)이냐 */
		UAscenduntAbilitySystemComponent* InstigatorASC,
		/** 누가 결과(피해자/수혜자)냐 + VFX를 어디 기준으로 붙일지 */
		UAscenduntAbilitySystemComponent* CueTargetASC,
		const FGameplayTag CueTag,
		const FGameplayCueParameters& Params,
		EAscenduntCueRoute Route);

	// ======= RPC ========
	UFUNCTION(Client, Unreliable)
	void Client_ExecuteCueLocal(FGameplayTag CueTag, FGameplayCueParameters Params);

	UFUNCTION(Client, Unreliable)
	void Client_ExecuteCueLocalOnActor(AActor* CueTarget, FGameplayTag CueTag, FGameplayCueParameters Params);

protected:
	// Handles to abilities that had their input pressed this frame.
	TArray<FGameplayAbilitySpecHandle> InputPressedSpecHandles;

	// Handles to abilities that had their input released this frame.
	TArray<FGameplayAbilitySpecHandle> InputReleasedSpecHandles;

	// Handles to abilities that have their input held.
	TArray<FGameplayAbilitySpecHandle> InputHeldSpecHandles;

	TArray<TArray<TWeakObjectPtr<UAscenduntGameplayAbility>>> ActivationGroupAbilities;
};
