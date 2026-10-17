// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AscenduntProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UAbilitySystemComponent;
class UAscenduntProjectilePoolSubsystem;

USTRUCT()
struct FAscenduntProjectileInitData
{
	GENERATED_BODY()

	UPROPERTY() FVector LaunchDir = FVector::ForwardVector;
	UPROPERTY() float Speed = 1500.f;
	UPROPERTY() float Scale = 1.f;
	UPROPERTY() bool bCanBeNullified = true;
	UPROPERTY() float DamageCoef = 1.f;
	UPROPERTY() float DamageOverride = -1.f;
	UPROPERTY() float ActiveDuration = 10.f;
	UPROPERTY() float Instances = 1.f;
	UPROPERTY() bool bAOEImpact = false;
	UPROPERTY() float AOERadius = 0.f;
	UPROPERTY() float AOEDamageCoefficient = 1.f;
	UPROPERTY() bool bDebugDrawAOERadius = false;
	UPROPERTY() float AOEDebugDrawDuration = 0.f;
};

UCLASS()
class ASCENDUNT_API AAscenduntProjectile : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Ascendunt")
	TObjectPtr<UStaticMeshComponent> MeshComp;

public:
	AAscenduntProjectile();

	// GA가 Acquire 후 세팅 (서버 전용 값)
	void SetDamageContext(
		UAbilitySystemComponent* InSourceASC,
		UObject* InSourceObject,
		TSubclassOf<UGameplayEffect> InDamageEffect,
		TSubclassOf<UGameplayEffect> InHealEffect,
		bool bInCanHeal,
		float InBaseDamage,
		float InBaseHeal,
		bool bInReturnToPoolOnWorldHit
	);

	// Pool link
	void SetPoolSubsystem(UAscenduntProjectilePoolSubsystem* InPool) { PoolSubsystem = InPool; }

	// 풀에서 나올 때(서버가 호출)
	void ActivateFromGA(const FAscenduntProjectileInitData& InInit);

	// 무효화
	UFUNCTION(BlueprintCallable)
	void InvalidateProjectile();

	// 풀 이벤트 훅
	void OnAcquiredFromPool();
	void OnReturnedToPool();

	// 풀 서브시스템이 상태 토글할 때 호출
	void SetActiveState(bool bInActive);

	bool GetIsActive() const { return bActive; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 벽/월드 Blocking으로 투사체가 멈췄을 때
	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	/**
	 * 월드 임팩트 GameplayCue 를 실행한다. 서버에서만 호출한다.
	 *
	 * 히트스캔은 UGA_BaseFire::FireDelivery_Hitscan 이 어빌리티 안에서 큐를 쏘지만,
	 * 투사체는 발사된 뒤 어빌리티 손을 떠나 스스로 날아가므로 그 경로를 타지 않는다.
	 * 그래서 벽 충돌 연출은 투사체 자신이 책임진다.
	 *
	 * 큐 파라미터는 UGA_BaseFire::PlayImpactWorldCue 와 같은 모양으로 채운다.
	 * GCN 이 SourceObject 를 무기로 캐스팅해 WeaponDefinitionData 에서
	 * FireImpactWorld VFX 를 읽어가기 때문에 형태가 어긋나면 안 된다.
	 */
	void PlayWorldImpactCue(const FHitResult& Hit) const;

	// Rep
	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_Init)
	FAscenduntProjectileInitData RepInit;

	UFUNCTION() void OnRep_Active();
	UFUNCTION() void OnRep_Init();

	UFUNCTION()	void EnableOverlapEvents();

protected:
	bool bHitConsumed = false;

	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	UPROPERTY() TObjectPtr<UObject> SourceObject = nullptr;
	UPROPERTY() TSubclassOf<UGameplayEffect> DamageEffectClass;
	UPROPERTY() TSubclassOf<UGameplayEffect> HealEffectClass;
	UPROPERTY() bool bCanHeal = false;

	UPROPERTY()	float BaseDamage = 0.f;

	UPROPERTY() float BaseHeal = 0.f;

	// impact policy (server only)
	UPROPERTY() bool bReturnToPoolOnWorldHit = false;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY() TObjectPtr<UAscenduntProjectilePoolSubsystem> PoolSubsystem = nullptr;

	FTimerHandle LifeTimer;
	FTimerHandle OverlapEnableTimer;

	void ApplyInitData(const FAscenduntProjectileInitData& Data);

	void ConsumeAndMaybeEnd(bool bReturnToPool);

	void ApplyHealToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& Hit) const;
	void ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const FHitResult& Hit) const;
	void ApplyAOEDamageAtLocation(const FVector& ImpactPoint, AActor* DirectTarget) const;

	void ReleaseToPool();

};
