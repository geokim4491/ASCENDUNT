// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AscenduntAttributeSet.h"
#include "NativeGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AscenduntWeaponSet.generated.h"

/**
 * 무기/기본공격 스탯을 GAS AttributeSet으로 관리하기 위한 클래스
 * 
 * “현재 탄 수(CurrentAmmo)” 같은 런타임 상태는 무기 액터(AAscenduntWeapon)가 관리하고
 * 탄창 크기/재장전/데미지/거리감쇠/탄퍼짐/반동/이속 패널티/탄속” 같은 수치는 Attribute로 관리
 * 
 */
UCLASS()
class ASCENDUNT_API UAscenduntWeaponSet : public UAscenduntAttributeSet
{
	GENERATED_BODY()
	
public:
	UAscenduntWeaponSet();
	
	// ---------- Attribute Accessors ----------
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, RateOfFire);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, ShotsPerTrigger);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, AmmoCostPerShot);

	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, MagSize);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, ReloadTime);

	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, BaseSpreadDeg);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, SpreadIncPerShot);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, SpreadRecoverPerSec);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, RecoilPitch);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, RecoilYaw);
	
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, HeadshotDamageMultiplier);

	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, MoveSpeedScalarWhileFiring);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, MoveSpeedScalarWhileADS);
	
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, ProjectileSpeed);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, ProjectileScale);

	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, MeleePlayRate);
	ATTRIBUTE_ACCESSORS(UAscenduntWeaponSet, MeleeDamageCoef);

protected:
	// RepNotifies
	UFUNCTION() void OnRep_RateOfFire(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShotsPerTrigger(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AmmoCostPerShot(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_MagSize(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ReloadTime(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_BaseSpreadDeg(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpreadIncPerShot(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_SpreadRecoverPerSec(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RecoilPitch(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_RecoilYaw(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_HeadshotDamageMultiplier(const FGameplayAttributeData& OldValue);


	UFUNCTION() void OnRep_MoveSpeedScalarWhileFiring(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MoveSpeedScalarWhileADS(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_ProjectileSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ProjectileScale(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_MeleePlayRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MeleeDamageCoef(const FGameplayAttributeData& OldValue);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	// --- Core Fire ---

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RateOfFire, Category = "Weapon|Fire", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RateOfFire; // shots per second

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShotsPerTrigger, Category = "Weapon|Fire", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ShotsPerTrigger; // 1, 3(버스트)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AmmoCostPerShot, Category = "Weapon|Ammo", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData AmmoCostPerShot; // 저격모드 3 같은 케이스


	// --- Ammo / Reload ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagSize, Category = "Weapon|Ammo", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MagSize;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadTime, Category = "Weapon|Ammo", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ReloadTime;


	// --- Spread / Recoil ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseSpreadDeg, Category = "Weapon|Spread", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseSpreadDeg;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadIncPerShot, Category = "Weapon|Spread", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData SpreadIncPerShot;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpreadRecoverPerSec, Category = "Weapon|Spread", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData SpreadRecoverPerSec;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecoilPitch, Category = "Weapon|Recoil", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RecoilPitch;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecoilYaw, Category = "Weapon|Recoil", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData RecoilYaw;

	// --- HeadShot ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HeadshotDamageMultiplier, Category = "Weapon|Ammo", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData HeadshotDamageMultiplier;


	// --- Movement Penalty ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedScalarWhileFiring, Category = "Weapon|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedScalarWhileFiring; // 0~1

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeedScalarWhileADS, Category = "Weapon|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedScalarWhileADS; // 0~1


	// --- Projectile ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProjectileSpeed, Category = "Weapon|Projectile", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ProjectileSpeed;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProjectileScale, Category = "Weapon|Projectile", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData ProjectileScale;

	// --- Melee ---
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeleePlayRate, Category = "Weapon|Melee", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MeleePlayRate;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MeleeDamageCoef, Category = "Weapon|Melee", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MeleeDamageCoef;
};
