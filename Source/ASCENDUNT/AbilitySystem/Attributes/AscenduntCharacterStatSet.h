// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ASCENDUNT/AbilitySystem/Attributes/AscenduntAttributeSet.h"
#include "AscenduntCharacterStatSet.generated.h"

/**
 * 캐릭터 본체 성능 스탯
 * 
 * - 이동/점프 계열
 * - 피격/치유 보정 계열
 * - 쿨다운 지속시간 보정
 * - 발소리 크기 보정
 * 
 * 주의:
 * - 현재 이동속도, 현재 crouch 여부, 현재 sprint 여부 같은 "상태"는 넣지 않음
 * - 이 Set은 "기본 성능값 + 버프/디버프 보정용 입력값"만 담당
 */
UCLASS()
class ASCENDUNT_API UAscenduntCharacterStatSet : public UAscenduntAttributeSet
{
	GENERATED_BODY()
	
public:
	UAscenduntCharacterStatSet();

	// ---------- Attribute Accessors ----------
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, BaseMoveSpeed);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, CrouchMoveSpeedScalar);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, JumpZVelocity);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, AirControl);

	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, DamageDealtScalar);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, DamageTakenScalar);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, HealingReceivedScalar);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, CooldownDurationScalar);
	ATTRIBUTE_ACCESSORS(UAscenduntCharacterStatSet, FootstepSoundScalar);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	// ---------- RepNotifies ----------
	UFUNCTION() void OnRep_BaseMoveSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CrouchMoveSpeedScalar(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_JumpZVelocity(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_AirControl(const FGameplayAttributeData& OldValue);

	UFUNCTION() void OnRep_DamageDealtScalar(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_DamageTakenScalar(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_HealingReceivedScalar(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_CooldownDurationScalar(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_FootstepSoundScalar(const FGameplayAttributeData& OldValue);

private:
	// --- Movement ---

	// 기본 이동속도 (예: 600)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_BaseMoveSpeed, Category = "Character|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData BaseMoveSpeed;

	// 웅크리기 시 기본 이동속도에 곱해지는 배수 (예: 0.55 ~ 0.7)
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CrouchMoveSpeedScalar, Category = "Character|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CrouchMoveSpeedScalar;

	// CharacterMovementComponent::JumpZVelocity 에 대응
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_JumpZVelocity, Category = "Character|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData JumpZVelocity;

	// CharacterMovementComponent::AirControl 에 대응
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AirControl, Category = "Character|Move", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData AirControl;


	// --- Survival / Response ---

	// 주는 피해 최종 배수. 1.0 = 기본, 1.2 = 20% 증가
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageDealtScalar, Category = "Character|Combat", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData DamageDealtScalar;

	// 받는 피해 최종 배수. 1.0 = 기본, 0.8 = 20% 감소, 1.2 = 20% 증가
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageTakenScalar, Category = "Character|Survival", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData DamageTakenScalar;

	// 받는 치유 최종 배수. 1.0 = 기본, 1.2 = 20% 증가
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealingReceivedScalar, Category = "Character|Survival", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData HealingReceivedScalar;


	// --- Ability / Cooldown ---

	// 최종 쿨다운 지속시간 배수. 1.0 = 기본, 0.8 = 20% 감소, 1.2 = 20% 증가
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CooldownDurationScalar, Category = "Character|Ability", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData CooldownDurationScalar;


	// --- Perception / Sound ---

	// 발소리 크기/전달량 배수. 1.0 = 기본, 0.7 = 더 조용함, 1.3 = 더 큼
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FootstepSoundScalar, Category = "Character|Sound", Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData FootstepSoundScalar;
	
	
};
