// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "AscenduntAttributeSet.h"
#include "NativeGameplayTags.h"
#include "AscenduntHealthSet.generated.h"

struct FGameplayEffectModCallbackData;
class UHeroPawnData;

USTRUCT(BlueprintType)
struct FAscenduntHealthSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)	float BasicHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float MaxBasicHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float Armor = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float MaxArmor = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float Thorn = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float MaxThorn = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float OverHealth = 0.0f;
	UPROPERTY(BlueprintReadOnly)	float MaxOverHealth = 0.0f;

	void SetFromHealthSet(const class UAscenduntHealthSet* HealthSet);

	/** Sum of all currently available health pools, clamped to non-negative values. */
	float GetCurrentTotal() const;

	/** Display capacity used by UI. Temporary health can expand the visible capacity. */
	float GetDisplayCapacityTotal() const;
};

DECLARE_MULTICAST_DELEGATE_SixParams(
	FOnHealthChanged,
	AActor* /*Target*/,
	AActor* /*Instigator*/,
	bool /*bWasDamaging*/,
	const FGameplayEffectSpec* /*EffectSpec*/,
	float /*RawDamage or RawHealing*/,
	const FAscenduntHealthSnapshot& /*HealthSnapshot*/
);

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnOutOfHealth,
	AActor* /*Target*/,
	AActor* /*Instigator*/,
	const FGameplayEffectSpec* /*EffectSpec*/
);

/**
* UAscenduntHealthSet
* 
* Health/Armor/Thorn/OverHealth + Regen + Meta(Damage/Healing)
*/
UCLASS()
class ASCENDUNT_API UAscenduntHealthSet : public UAscenduntAttributeSet
{
	GENERATED_BODY()
	
public:
	UAscenduntHealthSet();

	// 사망 조건: Thorn 존재 여부에 따라 분기
	bool IsDead() const
	{
		const bool bHasThorn = (GetMaxThorn() > 0.f);
		return bHasThorn ? (GetThorn() <= 0.f) : (GetBasicHealth() <= 0.f);
	}

	float GetTotalHealth() const
	{
		return GetBasicHealth() + GetArmor() + GetThorn() + GetOverHealth();
	}

	float GetMaxTotalHealth() const
	{
		return GetMaxBasicHealth() + GetMaxArmor() + GetMaxThorn() + GetMaxOverHealth();
	}

	// ---------- Attribute Accessors ----------
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, BasicHealth);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, MaxBasicHealth);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, Armor);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, MaxArmor);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, Thorn);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, MaxThorn);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, OverHealth);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, MaxOverHealth);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, RegenRate);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, RegenDelay);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, ArmorFlatReductionPerInstance);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, ArmorMaxReductionPct);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, ThornReflectPct);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, Damage);
	ATTRIBUTE_ACCESSORS(UAscenduntHealthSet, Healing);

	mutable FOnHealthChanged OnHealthChanged;

	mutable FOnOutOfHealth OnOutOfHealth;

	void SetSourcePawnData(const UHeroPawnData* InPawnData) { SourcePawnData = InPawnData; }
	const UHeroPawnData* GetSourcePawnData() const { return SourcePawnData; }
	
protected:
	// ---------- Replication ----------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------- GAS Hooks ----------
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;


	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;	

	UFUNCTION() void OnRep_BasicHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxBasicHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxArmor(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Thorn(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxThorn(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_OverHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxOverHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_RegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RegenDelay(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_ArmorFlatReduction(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ArmorMaxReductionPct(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ThornReflectPct(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(Replicated)
	TObjectPtr<const UHeroPawnData> SourcePawnData = nullptr;

	// ======= Current Pools (UI shows: Thorn -> Health -> Armor -> OverHealth) =======
	//
	// Basic Health (White Bar)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BasicHealth, Category = "Ascendun|tHealth", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BasicHealth;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxBasicHealth, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxBasicHealth;

	// Armor (Orange Bar)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Armor;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxArmor, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxArmor;

	// Thorn (Red Bar)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Thorn, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Thorn;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxThorn, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxThorn;

	// OverHealth (Green Bar)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OverHealth, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData OverHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxOverHealth, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MaxOverHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Health", Meta = (AllowPrivateAccess = true))
	FAscenduntHealthSnapshot LastHealthSnapshot;

	// ======= Regen Policy =======
	//
	// (10 + MaxHP*0.05) / s
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RegenRate, Category = "Ascendunt|Regen", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RegenRate;

	// Delay after taking damage before regen starts. Main Dealer/Healer = 6s, Sub Dealer = 4.5s
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RegenDelay, Category = "Ascendunt|Regen", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RegenDelay;



	// ======= Armor/Thorn Policy =======
	//
	// 7
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorFlatReduction, Category = "Ascendunt|Policy", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ArmorFlatReductionPerInstance;

	// 0.5
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorMaxReductionPct, Category = "Ascendunt|Policy", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ArmorMaxReductionPct;

	// 0.3 (상대 데미지의 30% 반사)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ThornReflectPct, Category = "Ascendunt|Policy", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ThornReflectPct;


	// ======= Meta Attributes (NOT replicated usually) =======
	//
	// SetByCaller 또는 GE modifier로 들어오고, PostGameplayEffectExecute에서 소비 후 0
	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Meta", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Damage;

	UPROPERTY(BlueprintReadOnly, Category = "Ascendunt|Meta", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Healing;

private:
	// “분배 결과”를 돌려서 Thorn이 실제로 깎였는지 판단
	struct FDamageSplit
	{
		float UseOver = 0.f;
		float UseArmor = 0.f;
		float UseHealth = 0.f;
		float UseThorn = 0.f;
	};

	void ClampAll();
	void SetHealthSnapshot();

	FDamageSplit ApplyDamage_Distribute(const FGameplayEffectModCallbackData& Data, float RawDamage);
	void ApplyHealing_Distribute(const FGameplayEffectModCallbackData& Data, float RawHeal);

	static float ApplyArmorMitigation_PerInstance(float TotalDamage, float Instances, float FlatReduce, float MaxReducePct);

	// “Thorn이 깎였을 때만” 호출
	void TryApplyThornReflect_WhenThornDamaged(const FGameplayEffectModCallbackData& Data, float RawDamage);

	// 클라에 Broadcast
	void BroadcastHealthChangedFromRep(float NewValue, float OldValue);

	void GCCharacterHit(const FGameplayEffectModCallbackData& Data, const FDamageSplit& Split, const float RawDamage, const float RawHeal);
};
